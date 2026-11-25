/** @file beidou_bcnv1.h @brief BeiDou B1C data-symbol decoder. */
#ifndef NGENE_BEIDOU_BCNV1_H
#define NGENE_BEIDOU_BCNV1_H
#include "ngene/types.h"

#define NG_BCNV1_FRAME_SYMBOLS 1800u
#define NG_BCNV1_HEADER_SYMBOLS 72u
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
#endif
