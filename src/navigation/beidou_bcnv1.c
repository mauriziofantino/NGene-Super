/** @file beidou_bcnv1.c @brief Soft B-CNAV1 frame decoding and synchronization. */
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
    int edge[NG_BCNV1_MAX_ROWS][4][NG_BCNV1_GF]; /**< Previous check messages. */
    uint8_t gf[NG_BCNV1_GF][NG_BCNV1_GF]; /**< GF(64) product permutations. */
} TNgBcnv1Ldpc;

typedef struct {
    unsigned soh, errors; /**< Header SOH index and BCH Hamming distance. */
    int inverted; /**< Received polarity relative to the ICD. */
} TNgBcnv1Header;

static const TNgBcnv1Code sf2Code = {sf2Columns, sf2Coefficients, 100};
static const TNgBcnv1Code sf3Code = {sf3Columns, sf3Coefficients, 44};

/** Generate the BCH recurrence in transmission order from the systematic bits. */
static uint64_t Bch(unsigned value, unsigned bits, unsigned count, unsigned poly)
{
    uint64_t word = value;
    const unsigned top = 1u << (bits - 1);
    for (unsigned n = bits; n < count; ++n) {
        unsigned feedback = 0;
        for (unsigned k = 0; k < bits; ++k)
            if (poly & (top >> k))
                feedback ^= (unsigned)(word >> k) & 1u;
        word = (word << 1) | feedback;
    }
    return word;
}

/** Exact Hamming distance on up to 51 received code symbols. */
static unsigned Distance(uint64_t x, uint64_t y)
{
    unsigned distance = 0;
    for (uint64_t bits = x ^ y; bits; bits &= bits - 1)
        ++distance;
    return distance;
}

/** Check the expected PRN and find the nearest unambiguous SOH in either polarity. */
static int Header(const int8_t *symbols, uint64_t prnWord, TNgBcnv1Header *header)
{
    uint64_t prn = 0, hour = 0;
    for (unsigned n = 0; n < 21; ++n)
        prn = (prn << 1) | (unsigned)(symbols[n] < 0);
    const unsigned d = Distance(prn, prnWord);
    if (d > 3 && d < 18)
        return 0;
    const int inverted = d >= 18;
    for (unsigned n = 21; n < NG_BCNV1_HEADER_SYMBOLS; ++n)
        hour = (hour << 1) | (unsigned)((symbols[n] < 0) != inverted);
    unsigned best = 12, soh = 256;
    for (unsigned value = 0; value < 256; ++value) {
        const unsigned distance = Distance(hour, Bch(value, 8, 51, 0x9f));
        if (distance < best) {
            best = distance;
            soh = value;
        } else if (distance == best) {
            soh = 256;
        }
    }
    if (soh >= 200)
        return 0;
    *header = (TNgBcnv1Header){soh, best + (inverted ? 21 - d : d), inverted};
    return 1;
}

/** Multiply polynomial-basis GF(64) symbols modulo 1+x+x^6. */
static unsigned Product(unsigned a, unsigned b)
{
    unsigned out = 0;
    while (b) {
        if (b & 1u)
            out ^= a;
        a = (a << 1) ^ (a & 32u ? 0x43u : 0u);
        b >>= 1;
    }
    return out;
}

/** Check all parity equations before a codeword can reach the CRC gate. */
static int Syndrome(const TNgBcnv1Code *code, const uint8_t *word)
{
    for (unsigned row = 0; row < code->rows; ++row) {
        unsigned sum = 0;
        for (unsigned e = 0; e < 4; ++e)
            sum ^= Product(code->coeff[row][e], word[code->column[row][e]]);
        if (sum)
            return 0;
    }
    return 1;
}

/** Compute the min-plus XOR convolution of two full symbol-metric vectors. */
static void Convolve(int *out, const int *a, const int *b)
{
    for (unsigned x = 0; x < NG_BCNV1_GF; ++x) {
        int best = INT_MAX;
        for (unsigned y = 0; y < NG_BCNV1_GF; ++y) {
            const int cost = a[y] + b[x ^ y];
            if (cost < best)
                best = cost;
        }
        out[x] = best;
    }
}

/** Layered normalized min-sum using the four nonzero entries of a parity row. */
static void UpdateRow(TNgBcnv1Ldpc *work, const TNgBcnv1Code *code, unsigned row)
{
    int input[4][NG_BCNV1_GF], pairs[2][NG_BCNV1_GF], output[NG_BCNV1_GF];
    for (unsigned e = 0; e < 4; ++e) {
        const unsigned column = code->column[row][e], coeff = code->coeff[row][e];
        for (unsigned s = 0; s < NG_BCNV1_GF; ++s)
            input[e][work->gf[coeff][s]] = work->belief[column][s] - work->edge[row][e][s];
    }
    Convolve(pairs[0], input[0], input[1]);
    Convolve(pairs[1], input[2], input[3]);
    for (unsigned e = 0; e < 4; ++e) {
        const unsigned column = code->column[row][e], coeff = code->coeff[row][e];
        int minimum = INT_MAX;
        Convolve(output, pairs[1u - e / 2u], input[e ^ 1u]);
        for (unsigned s = 0; s < NG_BCNV1_GF; ++s)
            if (output[s] < minimum)
                minimum = output[s];
        for (unsigned s = 0; s < NG_BCNV1_GF; ++s) {
            const unsigned field = work->gf[coeff][s];
            int message = (output[field] - minimum) * 3 / 4;
            if (message > 4096)
                message = 4096;
            work->edge[row][e][s] = message;
            work->belief[column][s] = input[e][field] + message;
        }
    }
}

/** Decode with a hard-decision fast path, then bounded soft iterations if needed. */
static int Ldpc(const TNgBcnv1Code *code, const int16_t *llr, uint8_t *bytes,
                unsigned *changed)
{
    uint8_t hard[NG_BCNV1_MAX_SYMBOLS], word[NG_BCNV1_MAX_SYMBOLS];
    const unsigned symbols = 2 * code->rows;
    int haveEvidence = 0;
    for (unsigned n = 0; n < symbols; ++n) {
        unsigned bits = 0;
        for (unsigned b = 0; b < 6; ++b) {
            const int sample = llr[6 * n + b];
            haveEvidence |= sample != 0;
            bits = (bits << 1) | (unsigned)(sample < 0);
        }
        word[n] = hard[n] = (uint8_t)bits;
    }
    if (!haveEvidence)
        return NG_END;
    if (!Syndrome(code, word)) {
        TNgBcnv1Ldpc *work = calloc(1, sizeof(*work));
        if (!work)
            return NG_ERROR;
        for (unsigned a = 0; a < NG_BCNV1_GF; ++a)
            for (unsigned b = 0; b < NG_BCNV1_GF; ++b)
                work->gf[a][b] = (uint8_t)Product(a, b);
        for (unsigned n = 0; n < symbols; ++n)
            for (unsigned s = 0; s < NG_BCNV1_GF; ++s)
                for (unsigned b = 0; b < 6; ++b)
                    if ((s ^ hard[n]) & (32u >> b))
                        work->belief[n][s] += abs(llr[6 * n + b]);
        int valid = 0;
        for (unsigned sweep = 0; sweep < NG_BCNV1_LDPC_SWEEPS && !valid; ++sweep) {
            for (unsigned row = 0; row < code->rows; ++row)
                UpdateRow(work, code, row);
            for (unsigned n = 0; n < symbols; ++n) {
                unsigned best = 0;
                for (unsigned s = 1; s < NG_BCNV1_GF; ++s)
                    if (work->belief[n][s] < work->belief[n][best])
                        best = s;
                word[n] = (uint8_t)best;
            }
            valid = Syndrome(code, word);
        }
        free(work);
        if (!valid)
            return NG_END;
    }
    *changed = 0;
    for (unsigned n = 0; n < symbols; ++n)
        *changed += Distance(hard[n], word[n]);
    for (unsigned n = 0; n < code->rows; n += 4) {
        const unsigned packed = ((unsigned)word[n] << 18) | ((unsigned)word[n + 1] << 12) |
                                ((unsigned)word[n + 2] << 6) | word[n + 3];
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
    for (size_t n = 0; n < count; ++n) {
        remainder ^= (uint32_t)bytes[n] << 16;
        for (unsigned b = 0; b < 8; ++b) {
            remainder <<= 1;
            if (remainder & UINT32_C(0x1000000))
                remainder ^= UINT32_C(0x1864cfb);
        }
    }
    return remainder == 0;
}

/** Extract a short unsigned field from packed MSB-first data. */
static unsigned Field(const uint8_t *bytes, unsigned start, unsigned width)
{
    unsigned value = 0;
    for (unsigned b = start; b < start + width; ++b)
        value = (value << 1) | (unsigned)((bytes[b / 8] >> (7u - b % 8)) & 1);
    return value;
}

/** Reverse the joint interleaver and publish only a fully checked candidate. */
static int Payload(const int8_t *symbols, unsigned prn, TNgBcnv1Header header,
                   TNgBcnv1Frame *frame)
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
        if (row < 33 && row % 3 == 2)
            n3 += 48;
        else
            n2 += 48;
    }
    int result = Ldpc(&sf2Code, sf2, candidate.sf2, &candidate.correctedBits[1]);
    if (result != NG_OK)
        return result;
    if (!Crc(candidate.sf2, sizeof(candidate.sf2)))
        return NG_END;
    result = Ldpc(&sf3Code, sf3, candidate.sf3, &candidate.correctedBits[2]);
    if (result != NG_OK)
        return result;
    if (!Crc(candidate.sf3, sizeof(candidate.sf3)))
        return NG_END;

    candidate.week = Field(candidate.sf2, 0, 13);
    candidate.hour = Field(candidate.sf2, 13, 8);
    if (candidate.hour >= 168)
        return NG_END;
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

int NgBcnv1Decode(const int8_t symbols[NG_BCNV1_FRAME_SYMBOLS], unsigned prn,
                  TNgBcnv1Frame *frame)
{
    TNgBcnv1Header header;
    if (!symbols || !frame || !NgSatellitePrnValid(NG_CONSTELLATION_BEIDOU, prn))
        return NG_ERROR;
    if (!Header(symbols, Bch(prn, 6, 21, 0x17), &header))
        return NG_END;
    return Payload(symbols, prn, header, frame);
}

int NgBcnv1Find(const int8_t *symbols, size_t count, unsigned prn, size_t *offset,
                TNgBcnv1Frame *frame)
{
    const size_t needed = NG_BCNV1_SYNC_SYMBOLS;
    if (!symbols || !offset || !frame || !NgSatellitePrnValid(NG_CONSTELLATION_BEIDOU, prn))
        return NG_ERROR;
    if (count < needed)
        return NG_END;
    const uint64_t prnWord = Bch(prn, 6, 21, 0x17);
    for (size_t n = 0; n <= count - needed; ++n) {
        TNgBcnv1Header first, second;
        if (Header(symbols + n, prnWord, &first) &&
            Header(symbols + n + NG_BCNV1_FRAME_SYMBOLS, prnWord, &second) &&
            first.inverted == second.inverted && (first.soh + 1) % 200 == second.soh) {
            int result = Payload(symbols + n, prn, first, frame);
            if (result == NG_OK)
                *offset = n;
            if (result != NG_END)
                return result;
        }
    }
    return NG_END;
}

/** Evaluate each complete framing window once and preserve its SF1 sample anchor. */
int NgBcnv1Push(TNgBcnv1Stream *stream, unsigned prn, uint64_t epoch, uint64_t firstSample,
                uint64_t endSample, int8_t symbol, TNgBcnv1Frame *frame,
                uint64_t *frameStartSample)
{
    if (!stream || !frame || !frameStartSample || endSample <= firstSample ||
        !NgSatellitePrnValid(NG_CONSTELLATION_BEIDOU, prn))
        return NG_ERROR;
    if (prn != stream->prn || epoch != stream->epoch || firstSample != stream->nextSample) {
        stream->count = stream->writeIndex = 0;
        stream->prn = prn;
        stream->epoch = epoch;
    }
    const size_t index = stream->writeIndex;
    stream->symbols[index] = stream->symbols[index + NG_BCNV1_SYNC_SYMBOLS] = symbol;
    stream->firstSamples[index] = firstSample;
    stream->writeIndex = (index + 1) % NG_BCNV1_SYNC_SYMBOLS;
    stream->nextSample = endSample;
    if (stream->count < NG_BCNV1_SYNC_SYMBOLS)
        ++stream->count;
    if (stream->count < NG_BCNV1_SYNC_SYMBOLS)
        return NG_END;
    size_t offset;
    const int result = NgBcnv1Find(stream->symbols + stream->writeIndex, stream->count,
                                   prn, &offset, frame);
    if (result == NG_OK)
        *frameStartSample = stream->firstSamples[(stream->writeIndex + offset) %
                                                NG_BCNV1_SYNC_SYMBOLS];
    return result;
}
