/** @file beidou_bcnv1.h @brief BeiDou B1C data-symbol decoder. */
#ifndef NGENE_BEIDOU_BCNV1_H
#define NGENE_BEIDOU_BCNV1_H
#include "ngene/types.h"

#define NG_BCNV1_FRAME_SYMBOLS 1800u /**< Transmitted symbols per complete frame. */
#define NG_BCNV1_HEADER_SYMBOLS 72u  /**< Transmitted symbols of subframe 1. */
/** Contiguous symbols needed to check consecutive subframe 1 headers. */
#define NG_BCNV1_SYNC_SYMBOLS (NG_BCNV1_FRAME_SYMBOLS + NG_BCNV1_HEADER_SYMBOLS)
#define NG_BCNV1_SF2_BYTES 75u /**< Decoded subframe 2 bytes, including CRC. */
#define NG_BCNV1_SF3_BYTES 33u /**< Decoded subframe 3 bytes, including CRC. */

/** One validated B-CNAV1 frame with decoded timing and original packed payloads. */
typedef struct {
    unsigned prn;                    /**< BeiDou satellite identifier from subframe 1. */
    unsigned soh;                    /**< Time within the hour, in units of 18 seconds. */
    unsigned week;                   /**< BeiDou Time week number. */
    unsigned hour;                   /**< Hour within the BeiDou Time week. */
    unsigned iodc;                   /**< Issue of data for the clock. */
    unsigned iode;                   /**< Issue of data for the ephemeris. */
    unsigned pageId;                 /**< Subframe 3 page identifier. */
    unsigned health;                 /**< Broadcast satellite health flag. */
    unsigned dif;                    /**< Data integrity flag. */
    unsigned sif;                    /**< Signal integrity flag. */
    unsigned aif;                    /**< Accuracy integrity flag. */
    unsigned sismai;                 /**< Signal-in-space monitoring accuracy index. */
    int inverted;                    /**< Soft symbol polarity found in the header. */
    unsigned correctedBits[3];       /**< Hard decisions changed in SF1, SF2 and SF3. */
    uint8_t sf2[NG_BCNV1_SF2_BYTES]; /**< CRC-checked subframe 2, packed MSB first. */
    uint8_t sf3[NG_BCNV1_SF3_BYTES]; /**< CRC-checked subframe 3, packed MSB first. */
} TNgBcnv1Frame;

/** Decode an aligned 1800-symbol frame. Positive soft symbols mean bit zero.
 * The result is written only after both LDPC syndromes and CRC-24Q checks pass.
 * @param symbols Borrowed contiguous frame in transmission order.
 * @param prn Expected BeiDou satellite PRN.
 * @param frame Destination for a frame that passes validation.
 * @return NG_OK on success, NG_END for invalid signal data, or NG_ERROR for bad
 *         arguments or decoder allocation failure.
 */
int NgBcnv1Decode(const int8_t symbols[NG_BCNV1_FRAME_SYMBOLS], unsigned prn, TNgBcnv1Frame *frame);

/** Find a frame in a contiguous soft-symbol span, requiring a second header
 * with matching polarity/PRN and consecutive SOH. The second payload is not decoded.
 * @param symbols Borrowed soft symbols in transmission order.
 * @param count Number of available symbols.
 * @param prn Expected BeiDou satellite PRN.
 * @param offset Destination for the first symbol index of the validated frame.
 * @param frame Destination for the first validated frame.
 * @return NG_OK if a frame is found, NG_END if none passes, or NG_ERROR on failure.
 */
int NgBcnv1Find(const int8_t *symbols, size_t count, unsigned prn, size_t *offset,
                TNgBcnv1Frame *frame);

/** Per-channel framing state. Zero-initialize on assignment or loss of symbol timing.
 * The caller owns the state and serializes calls. */
typedef struct {
    int8_t symbols[2 * NG_BCNV1_SYNC_SYMBOLS];    /**< Mirrored circular window. */
    size_t writeIndex;                            /**< Next slot in the first half of the ring. */
    size_t count;                                 /**< Number of contiguous symbols retained. */
    uint64_t firstSamples[NG_BCNV1_SYNC_SYMBOLS]; /**< Absolute start of each symbol. */
    uint64_t epoch;                               /**< Continuity generation of the input. */
    uint64_t nextSample;                          /**< Expected first sample of the next symbol. */
    unsigned prn;                                 /**< Identity of the buffered symbols. */
    unsigned elapsedSymbols; /**< Periods from the returned SF1 start through this input. */
    unsigned nextFrame;      /**< Symbols remaining before the next aligned decode. */
    unsigned lastSoh;        /**< Time within the hour from the last validated header. */
} TNgBcnv1Stream;

/** Feed one complete 10-ms data prompt as a soft symbol with its input sample range.
 * A new PRN, epoch or discontinuous range discards partial framing. On NG_OK, returns
 * the CRC-validated frame and its first sample. On NG_END, neither output is modified.
 * @param stream Channel-local framing state owned by the caller.
 * @param prn BeiDou satellite PRN associated with the prompt.
 * @param epoch Continuity generation for the input sample range.
 * @param firstSample Inclusive absolute index of the first sample in the prompt.
 * @param endSample Exclusive absolute index of the prompt end.
 * @param symbol Signed soft decision, with a positive value indicating bit zero.
 * @param frame Destination for the newly decoded frame.
 * @param frameStartSample Destination for its absolute start sample.
 * @return NG_OK for a decoded frame, NG_END while waiting for a valid frame, or
 *         NG_ERROR for invalid arguments or decoder resource failure.
 */
int NgBcnv1Push(TNgBcnv1Stream *stream, unsigned prn, uint64_t epoch, uint64_t firstSample,
                uint64_t endSample, int8_t symbol, TNgBcnv1Frame *frame,
                uint64_t *frameStartSample);
#endif
