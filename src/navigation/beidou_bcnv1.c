/** @file beidou_bcnv1.c @brief Soft B-CNAV1 frame decoding and synchronization.
 *
 * @details Identify a candidate frame by testing the BCH-protected SF1
 * header against the expected PRN and symbol alignment. Decode SF2 and
 * SF3 with their sparse GF(64) parity-check matrices after reversing
 * the wire interleaving. Each row connects four symbols, so a syndrome
 * sweep applies four field products and XOR additions per row.
 *
 * The iterative decoder uses soft symbol costs and a bounded sweep
 * count. Parity satisfaction is an internal decoder result. Publish
 * navigation data only after the frame fields and CRC-24Q pass their
 * acceptance checks. Maintain independent alignment and freshness in
 * each tracking channel.
 */
#include "ngene/beidou_bcnv1.h"
#include "ngene/channel.h"
#include <limits.h>
#include <stdlib.h>

#include "beidou_bcnv1_tables.h"

#define NG_BCNV1_GF 64u
#define NG_BCNV1_MAX_ROWS 100u
#define NG_BCNV1_MAX_SYMBOLS 200u
#define NG_BCNV1_LDPC_SWEEPS 16u

typedef struct {
    const uint8_t (*column)[4], (*coeff)[4]; /**< Sparse H matrix, row degree four. */
    unsigned rows; /**< The systematic code has twice as many columns as rows. */
} TNgBcnv1Code;

typedef struct {
    int belief[NG_BCNV1_MAX_SYMBOLS][NG_BCNV1_GF]; /**< Current symbol metrics. */
    int edge[NG_BCNV1_MAX_ROWS][4][NG_BCNV1_GF];   /**< Previous check messages. */
    uint8_t gf[NG_BCNV1_GF][NG_BCNV1_GF];          /**< GF(64) product permutations. */
} TNgBcnv1Ldpc;

typedef struct {
    unsigned soh, errors; /**< Header SOH index and BCH Hamming distance. */
    int inverted;         /**< Received polarity relative to the ICD. */
} TNgBcnv1Header;

static const TNgBcnv1Code sf2Code = {sf2Columns, sf2Coefficients, 100};
static const TNgBcnv1Code sf3Code = {sf3Columns, sf3Coefficients, 44};

/** Generate the BCH recurrence in transmission order from the systematic bits. */
static uint64_t Bch(unsigned value, unsigned bits, unsigned count, unsigned generatorPolynomial)
{
    uint64_t word = value;
    const unsigned top = 1u << (bits - 1);
    for (unsigned symbolIndex = bits; symbolIndex < count; ++symbolIndex) {
        unsigned feedback = 0;
        for (unsigned tapIndex = 0; tapIndex < bits; ++tapIndex) {
            if (generatorPolynomial & (top >> tapIndex)) {
                feedback ^= (unsigned)(word >> tapIndex) & 1u;
            }
        }
        word = (word << 1) | feedback;
    }
    return word;
}

/** Exact Hamming distance on up to 51 received code symbols. */
static unsigned Distance(uint64_t receivedWord, uint64_t referenceWord)
{
    unsigned distance = 0;
    for (uint64_t bits = receivedWord ^ referenceWord; bits; bits &= bits - 1) {
        ++distance;
    }
    return distance;
}

/** Check the expected PRN and find the nearest unambiguous SOH in either polarity. */
static int Header(const int8_t *symbols, uint64_t prnWord, TNgBcnv1Header *header)
{
    uint64_t receivedPrnWord = 0, receivedSohWord = 0;
    for (unsigned symbolIndex = 0; symbolIndex < 21; ++symbolIndex) {
        receivedPrnWord = (receivedPrnWord << 1) | (unsigned)(symbols[symbolIndex] < 0);
    }
    const unsigned prnDistance = Distance(receivedPrnWord, prnWord);
    if (prnDistance > 3 && prnDistance < 18) {
        return 0;
    }
    const int inverted = prnDistance >= 18;
    for (unsigned symbolIndex = 21; symbolIndex < NG_BCNV1_HEADER_SYMBOLS; ++symbolIndex) {
        receivedSohWord =
            (receivedSohWord << 1) | (unsigned)((symbols[symbolIndex] < 0) != inverted);
    }
    unsigned bestSohDistance = 12, soh = 256;
    for (unsigned value = 0; value < 256; ++value) {
        const unsigned distance = Distance(receivedSohWord, Bch(value, 8, 51, 0x9f));
        if (distance < bestSohDistance) {
            bestSohDistance = distance;
            soh = value;
        } else if (distance == bestSohDistance) {
            soh = 256;
        }
    }
    if (soh >= 200) {
        return 0;
    }
    *header = (TNgBcnv1Header){soh, bestSohDistance + (inverted ? 21 - prnDistance : prnDistance),
                               inverted};
    return 1;
}

/** Multiply polynomial-basis GF(64) symbols modulo 1+x+x^6. */
static unsigned Product(unsigned multiplicand, unsigned multiplier)
{
    unsigned product = 0;
    while (multiplier) {
        if (multiplier & 1u) {
            product ^= multiplicand;
        }
        multiplicand = (multiplicand << 1) ^ (multiplicand & 32u ? 0x43u : 0u);
        multiplier >>= 1;
    }
    return product;
}

/** Check all parity equations before a codeword can reach the CRC gate. */
static int Syndrome(const TNgBcnv1Code *code, const uint8_t *word)
{
    for (unsigned row = 0; row < code->rows; ++row) {
        unsigned sum = 0;
        for (unsigned edgeIndex = 0; edgeIndex < 4; ++edgeIndex) {
            sum ^= Product(code->coeff[row][edgeIndex], word[code->column[row][edgeIndex]]);
        }
        if (sum) {
            return 0;
        }
    }
    return 1;
}

/** Compute the min-plus XOR convolution of two full symbol-metric vectors. */
static void Convolve(int *outputMetrics, const int *leftMetrics, const int *rightMetrics)
{
    for (unsigned outputSymbol = 0; outputSymbol < NG_BCNV1_GF; ++outputSymbol) {
        int minimumCost = INT_MAX;
        for (unsigned leftSymbol = 0; leftSymbol < NG_BCNV1_GF; ++leftSymbol) {
            const int cost = leftMetrics[leftSymbol] + rightMetrics[outputSymbol ^ leftSymbol];
            if (cost < minimumCost) {
                minimumCost = cost;
            }
        }
        outputMetrics[outputSymbol] = minimumCost;
    }
}

/** Layered normalized min-sum using the four nonzero entries of a parity row. */
static void UpdateRow(TNgBcnv1Ldpc *work, const TNgBcnv1Code *code, unsigned row)
{
    int input[4][NG_BCNV1_GF], pairs[2][NG_BCNV1_GF], output[NG_BCNV1_GF];
    for (unsigned edgeIndex = 0; edgeIndex < 4; ++edgeIndex) {
        const unsigned column = code->column[row][edgeIndex], coeff = code->coeff[row][edgeIndex];
        for (unsigned fieldSymbol = 0; fieldSymbol < NG_BCNV1_GF; ++fieldSymbol) {
            input[edgeIndex][work->gf[coeff][fieldSymbol]] =
                work->belief[column][fieldSymbol] - work->edge[row][edgeIndex][fieldSymbol];
        }
    }
    Convolve(pairs[0], input[0], input[1]);
    Convolve(pairs[1], input[2], input[3]);
    for (unsigned edgeIndex = 0; edgeIndex < 4; ++edgeIndex) {
        const unsigned column = code->column[row][edgeIndex], coeff = code->coeff[row][edgeIndex];
        int minimum = INT_MAX;
        Convolve(output, pairs[1u - edgeIndex / 2u], input[edgeIndex ^ 1u]);
        for (unsigned fieldSymbol = 0; fieldSymbol < NG_BCNV1_GF; ++fieldSymbol) {
            if (output[fieldSymbol] < minimum) {
                minimum = output[fieldSymbol];
            }
        }
        for (unsigned fieldSymbol = 0; fieldSymbol < NG_BCNV1_GF; ++fieldSymbol) {
            const unsigned field = work->gf[coeff][fieldSymbol];
            int message = (output[field] - minimum) * 3 / 4;
            if (message > 4096) {
                message = 4096;
            }
            work->edge[row][edgeIndex][fieldSymbol] = message;
            work->belief[column][fieldSymbol] = input[edgeIndex][field] + message;
        }
    }
}

/** Decode with a hard-decision fast path, then bounded soft iterations if needed. */
static int Ldpc(const TNgBcnv1Code *code, const int16_t *llr, uint8_t *bytes, unsigned *changed)
{
    uint8_t hard[NG_BCNV1_MAX_SYMBOLS], word[NG_BCNV1_MAX_SYMBOLS];
    const unsigned symbols = 2 * code->rows;
    int haveEvidence = 0;
    for (unsigned symbolIndex = 0; symbolIndex < symbols; ++symbolIndex) {
        unsigned bits = 0;
        for (unsigned bitIndex = 0; bitIndex < 6; ++bitIndex) {
            const int sample = llr[6 * symbolIndex + bitIndex];
            haveEvidence |= sample != 0;
            bits = (bits << 1) | (unsigned)(sample < 0);
        }
        word[symbolIndex] = hard[symbolIndex] = (uint8_t)bits;
    }
    if (!haveEvidence) {
        return NG_END;
    }
    if (!Syndrome(code, word)) {
        TNgBcnv1Ldpc *work = calloc(1, sizeof(*work));
        if (!work) {
            return NG_ERROR;
        }
        for (unsigned leftFactor = 0; leftFactor < NG_BCNV1_GF; ++leftFactor) {
            for (unsigned rightFactor = 0; rightFactor < NG_BCNV1_GF; ++rightFactor) {
                work->gf[leftFactor][rightFactor] = (uint8_t)Product(leftFactor, rightFactor);
            }
        }
        for (unsigned symbolIndex = 0; symbolIndex < symbols; ++symbolIndex) {
            for (unsigned candidateSymbol = 0; candidateSymbol < NG_BCNV1_GF; ++candidateSymbol) {
                for (unsigned bitIndex = 0; bitIndex < 6; ++bitIndex) {
                    if ((candidateSymbol ^ hard[symbolIndex]) & (32u >> bitIndex)) {
                        work->belief[symbolIndex][candidateSymbol] +=
                            abs(llr[6 * symbolIndex + bitIndex]);
                    }
                }
            }
        }
        int valid = 0;
        for (unsigned sweep = 0; sweep < NG_BCNV1_LDPC_SWEEPS && !valid; ++sweep) {
            for (unsigned row = 0; row < code->rows; ++row) {
                UpdateRow(work, code, row);
            }
            for (unsigned symbolIndex = 0; symbolIndex < symbols; ++symbolIndex) {
                unsigned bestSymbol = 0;
                for (unsigned candidateSymbol = 1; candidateSymbol < NG_BCNV1_GF;
                     ++candidateSymbol) {
                    if (work->belief[symbolIndex][candidateSymbol] <
                        work->belief[symbolIndex][bestSymbol]) {
                        bestSymbol = candidateSymbol;
                    }
                }
                word[symbolIndex] = (uint8_t)bestSymbol;
            }
            valid = Syndrome(code, word);
        }
        free(work);
        if (!valid) {
            return NG_END;
        }
    }
    *changed = 0;
    for (unsigned symbolIndex = 0; symbolIndex < symbols; ++symbolIndex) {
        *changed += Distance(hard[symbolIndex], word[symbolIndex]);
    }
    for (unsigned symbolIndex = 0; symbolIndex < code->rows; symbolIndex += 4) {
        const unsigned packed = ((unsigned)word[symbolIndex] << 18) |
                                ((unsigned)word[symbolIndex + 1] << 12) |
                                ((unsigned)word[symbolIndex + 2] << 6) | word[symbolIndex + 3];
        *bytes++ = (uint8_t)(packed >> 16);
        *bytes++ = (uint8_t)(packed >> 8);
        *bytes++ = (uint8_t)packed;
    }
    return NG_OK;
}

/** Check packed bytes with the ICD zero-initialized CRC-24Q remainder. */
static int Crc(const uint8_t *bytes, size_t count)
{
    uint32_t remainder = 0;
    for (size_t byteIndex = 0; byteIndex < count; ++byteIndex) {
        remainder ^= (uint32_t)bytes[byteIndex] << 16;
        for (unsigned bitIndex = 0; bitIndex < 8; ++bitIndex) {
            remainder <<= 1;
            if (remainder & UINT32_C(0x1000000)) {
                remainder ^= UINT32_C(0x1864cfb);
            }
        }
    }
    return remainder == 0;
}

/** Extract a short unsigned field from packed MSB-first data. */
static unsigned Field(const uint8_t *bytes, unsigned start, unsigned width)
{
    unsigned value = 0;
    for (unsigned bitIndex = start; bitIndex < start + width; ++bitIndex) {
        value = (value << 1) | (unsigned)((bytes[bitIndex / 8] >> (7u - bitIndex % 8)) & 1);
    }
    return value;
}

/** Reverse the joint interleaver and publish only a fully checked candidate. */
static int Payload(const int8_t *symbols, unsigned prn, TNgBcnv1Header header, TNgBcnv1Frame *frame)
{
    int16_t sf2[1200], sf3[528];
    unsigned n2 = 0, n3 = 0;
    TNgBcnv1Frame candidate = {0};
    for (unsigned row = 0; row < 36; ++row) {
        int16_t *to = row < 33 && row % 3 == 2 ? sf3 + n3 : sf2 + n2;
        for (unsigned col = 0; col < 48; ++col) {
            const int sample = symbols[NG_BCNV1_HEADER_SYMBOLS + 36 * col + row];
            to[col] = (int16_t)(header.inverted ? -sample : sample);
        }
        if (row < 33 && row % 3 == 2) {
            n3 += 48;
        } else {
            n2 += 48;
        }
    }
    int result = Ldpc(&sf2Code, sf2, candidate.sf2, &candidate.correctedBits[1]);
    if (result != NG_OK) {
        return result;
    }
    if (!Crc(candidate.sf2, sizeof(candidate.sf2))) {
        return NG_END;
    }
    result = Ldpc(&sf3Code, sf3, candidate.sf3, &candidate.correctedBits[2]);
    if (result != NG_OK) {
        return result;
    }
    if (!Crc(candidate.sf3, sizeof(candidate.sf3))) {
        return NG_END;
    }

    candidate.week = Field(candidate.sf2, 0, 13);
    candidate.hour = Field(candidate.sf2, 13, 8);
    if (candidate.hour >= 168) {
        return NG_END;
    }
    candidate.iodc = Field(candidate.sf2, 21, 10);
    candidate.iode = Field(candidate.sf2, 31, 8);
    candidate.pageId = Field(candidate.sf3, 0, 6);
    candidate.health = Field(candidate.sf3, 6, 2);
    candidate.dif = Field(candidate.sf3, 8, 1);
    candidate.sif = Field(candidate.sf3, 9, 1);
    candidate.aif = Field(candidate.sf3, 10, 1);
    candidate.sismai = Field(candidate.sf3, 11, 4);
    candidate.prn = prn;
    candidate.soh = header.soh;
    candidate.inverted = header.inverted;
    candidate.correctedBits[0] = header.errors;
    *frame = candidate;
    return NG_OK;
}

/** Validate the BCH header before decoding the interleaved soft payload. */
int NgBcnv1Decode(const int8_t symbols[NG_BCNV1_FRAME_SYMBOLS], unsigned prn, TNgBcnv1Frame *frame)
{
    TNgBcnv1Header header;
    if (!symbols || !frame || !NgSatellitePrnValid(NG_CONSTELLATION_BEIDOU, prn)) {
        return NG_ERROR;
    }
    if (!Header(symbols, Bch(prn, 6, 21, 0x17), &header)) {
        return NG_END;
    }
    return Payload(symbols, prn, header, frame);
}

/** Confirm frame phase using two successive headers before accepting a payload. */
int NgBcnv1Find(const int8_t *symbols, size_t count, unsigned prn, size_t *offset,
                TNgBcnv1Frame *frame)
{
    const size_t needed = NG_BCNV1_SYNC_SYMBOLS;
    if (!symbols || !offset || !frame || !NgSatellitePrnValid(NG_CONSTELLATION_BEIDOU, prn)) {
        return NG_ERROR;
    }
    if (count < needed) {
        return NG_END;
    }
    const uint64_t prnWord = Bch(prn, 6, 21, 0x17);
    for (size_t frameOffset = 0; frameOffset <= count - needed; ++frameOffset) {
        TNgBcnv1Header firstHeader, nextHeader;
        if (Header(symbols + frameOffset, prnWord, &firstHeader) &&
            Header(symbols + frameOffset + NG_BCNV1_FRAME_SYMBOLS, prnWord, &nextHeader) &&
            firstHeader.inverted == nextHeader.inverted &&
            (firstHeader.soh + 1) % 200 == nextHeader.soh) {
            int result = Payload(symbols + frameOffset, prn, firstHeader, frame);
            if (result == NG_OK) {
                *offset = frameOffset;
            }
            if (result != NG_END) {
                return result;
            }
        }
    }
    return NG_END;
}

/** Evaluate each complete framing window once and preserve its SF1 sample anchor. */
int NgBcnv1Push(TNgBcnv1Stream *stream, unsigned prn, uint64_t epoch, uint64_t firstSample,
                uint64_t endSample, int8_t symbol, TNgBcnv1Frame *frame, uint64_t *frameStartSample)
{
    if (!stream || !frame || !frameStartSample || endSample <= firstSample ||
        !NgSatellitePrnValid(NG_CONSTELLATION_BEIDOU, prn)) {
        return NG_ERROR;
    }
    if (prn != stream->prn || epoch != stream->epoch || firstSample != stream->nextSample) {
        stream->count = stream->writeIndex = 0;
        stream->nextFrame = 0;
        stream->prn = prn;
        stream->epoch = epoch;
    }
    const size_t index = stream->writeIndex;
    stream->symbols[index] = stream->symbols[index + NG_BCNV1_SYNC_SYMBOLS] = symbol;
    stream->firstSamples[index] = firstSample;
    stream->writeIndex = (index + 1) % NG_BCNV1_SYNC_SYMBOLS;
    stream->nextSample = endSample;
    if (stream->count < NG_BCNV1_SYNC_SYMBOLS) {
        ++stream->count;
    }
    if (stream->count < NG_BCNV1_SYNC_SYMBOLS) {
        return NG_END;
    }
    if (stream->nextFrame) {
        if (--stream->nextFrame) {
            return NG_END;
        }
        size_t start = stream->writeIndex + NG_BCNV1_HEADER_SYMBOLS;
        TNgBcnv1Frame aligned;
        int status = NgBcnv1Decode(stream->symbols + start, prn, &aligned);
        if (status == NG_ERROR) {
            return status;
        }
        if (status == NG_OK && aligned.soh == (stream->lastSoh + 1) % 200) {
            stream->lastSoh = aligned.soh;
            stream->nextFrame = NG_BCNV1_FRAME_SYMBOLS;
            *frame = aligned;
            stream->elapsedSymbols = NG_BCNV1_FRAME_SYMBOLS;
            *frameStartSample = stream->firstSamples[start % NG_BCNV1_SYNC_SYMBOLS];
            return NG_OK;
        }
    }
    size_t offset;
    const int result =
        NgBcnv1Find(stream->symbols + stream->writeIndex, stream->count, prn, &offset, frame);
    if (result == NG_OK) {
        *frameStartSample =
            stream->firstSamples[(stream->writeIndex + offset) % NG_BCNV1_SYNC_SYMBOLS];
        stream->elapsedSymbols = NG_BCNV1_SYNC_SYMBOLS;
        stream->lastSoh = frame->soh;
        stream->nextFrame = NG_BCNV1_FRAME_SYMBOLS - NG_BCNV1_HEADER_SYMBOLS;
    }
    return result;
}
