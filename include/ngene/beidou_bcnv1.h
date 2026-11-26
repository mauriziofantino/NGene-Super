/** @file beidou_bcnv1.h @brief BeiDou B1C data-symbol decoder. */
#ifndef NGENE_BEIDOU_BCNV1_H
#define NGENE_BEIDOU_BCNV1_H
#include "ngene/types.h"

#define NG_BCNV1_FRAME_SYMBOLS 1800u
#define NG_BCNV1_HEADER_SYMBOLS 72u
#define NG_BCNV1_SYNC_SYMBOLS (NG_BCNV1_FRAME_SYMBOLS + NG_BCNV1_HEADER_SYMBOLS)
#define NG_BCNV1_SF2_BYTES 75u
#define NG_BCNV1_SF3_BYTES 33u

typedef struct {
    unsigned prn, soh, week, hour, iodc, iode; /**< SOH in units of 18 s, week/hour in BDT. */
    unsigned pageId, health, dif, sif, aif, sismai; /**< SF3 common fields. */
    int inverted; /**< Soft symbol polarity found in the header. */
    unsigned correctedBits[3]; /**< Hard decisions changed in SF1, SF2 and SF3. */
    uint8_t sf2[NG_BCNV1_SF2_BYTES], sf3[NG_BCNV1_SF3_BYTES]; /**< CRC-checked, MSB first. */
} TNgBcnv1Frame;

/** Decode an aligned 1800-symbol frame. Positive soft symbols mean bit zero.
 * The result is written only after both LDPC syndromes and CRC-24Q checks pass.
 * Returns NG_END for invalid signal data or NG_ERROR for bad arguments/allocation. */
int NgBcnv1Decode(const int8_t symbols[NG_BCNV1_FRAME_SYMBOLS], unsigned prn,
                  TNgBcnv1Frame *frame);

/** Find a frame in a contiguous soft-symbol span, requiring a second header
 * with matching polarity/PRN and consecutive SOH. The second payload is not decoded.
 * On NG_OK, writes the symbol offset and the first validated frame. */
int NgBcnv1Find(const int8_t *symbols, size_t count, unsigned prn, size_t *offset,
                TNgBcnv1Frame *frame);

/** Per-channel framing state. Zero-initialize on assignment or loss of symbol timing.
 * The caller owns the state and serializes calls. */
typedef struct {
    int8_t symbols[2 * NG_BCNV1_SYNC_SYMBOLS]; /**< Mirrored circular window. */
    size_t writeIndex, count; /**< Next slot and number of contiguous symbols. */
    uint64_t firstSamples[NG_BCNV1_SYNC_SYMBOLS]; /**< Absolute start of each symbol. */
    uint64_t epoch, nextSample; /**< Continuity and expected next start sample. */
    unsigned prn; /**< Identity of the buffered symbols. */
    unsigned elapsedSymbols; /**< Periods from the returned SF1 start through this input. */
    unsigned nextFrame, lastSoh; /**< Aligned decode countdown following a validated frame. */
} TNgBcnv1Stream;

/** Feed one complete 10-ms data prompt as a soft symbol with its input sample range.
 * A new PRN, epoch or discontinuous range discards partial framing. On NG_OK, returns
 * the CRC-validated frame and its first sample. On NG_END, neither output is modified.
 * NG_ERROR indicates an invalid argument or decoder resource failure. */
int NgBcnv1Push(TNgBcnv1Stream *stream, unsigned prn, uint64_t epoch, uint64_t firstSample,
                uint64_t endSample, int8_t symbol, TNgBcnv1Frame *frame,
                uint64_t *frameStartSample);
#endif
