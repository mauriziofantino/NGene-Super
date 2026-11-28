/** @file sbas_stream.h @brief SBAS L1 streaming FEC decoder from 1-ms C/A prompts. */
#ifndef NG_SBAS_STREAM_H
#define NG_SBAS_STREAM_H
#include "ngene/sbas.h"

#define NG_SBAS_TRACEBACK 32u

/** One combination of symbol phase and convolutional pairing. */
typedef struct {
    double metric[64];                       /**< Normalized path metric for each trellis state. */
    uint8_t previous[NG_SBAS_TRACEBACK][64]; /**< Circular survivor predecessor history. */
    uint8_t bits[250]; /**< Circular recovered bits used for preamble and CRC checks. */
    unsigned path;     /**< Next survivor row to write. */
    unsigned fill;     /**< Number of buffered survivor rows, capped at traceback depth. */
    unsigned bit;      /**< Next recovered bit slot to write. */
    unsigned count;    /**< Recovered bits available, capped at one frame. */
    unsigned paired;   /**< The first symbol of a trellis pair is available. */
    double pending;    /**< Signed sum of the two prompts forming the first symbol. */
} TNgSbasLane;

/** Recover from either 2-ms symbol phase and either convolutional symbol order. */
typedef struct {
    TNgSbasLane lanes[4];  /**< Two symbol phases with two pair alignments each. */
    double prompt;         /**< Previous one-ms prompt. */
    unsigned havePrompt;   /**< The previous prompt is valid. */
    unsigned parity;       /**< Current one-ms phase within a two-ms symbol. */
    unsigned pairPhase[2]; /**< Current convolutional pair parity per symbol phase. */
} TNgSbasStream;

/** @brief Submit one consecutive 1-ms in-phase prompt to the SBAS decoder.
 * @param[in,out] stream Zero-initialized Viterbi and symbol phase state.
 * @param[in,out] bank Correction bank for this GEO assignment.
 * @param[in] prompt Signed coherent I in native ADC units per sample.
 * @param[in] now End of the prompt in receiver signal seconds.
 * The receiver thread owns both objects and resets them on discontinuity.
 */
void NgSbasPrompt(TNgSbasStream *stream, TNgSbasCorrections *bank, double prompt, double now);
#endif
