/** @file sbas_stream.h @brief SBAS L1 streaming FEC decoder from 1-ms C/A prompts. */
#ifndef NG_SBAS_STREAM_H
#define NG_SBAS_STREAM_H
#include "ngene/sbas.h"

#define NG_SBAS_TRACEBACK 32u

/** One combination of symbol phase and convolutional pairing. */
typedef struct {
    double metric[64];
    uint8_t previous[NG_SBAS_TRACEBACK][64];
    uint8_t bits[250];
    unsigned path, fill, bit, count, paired;
    double pending;
} TNgSbasLane;

/** Recover from either 2-ms symbol phase and either convolutional symbol order. */
typedef struct {
    TNgSbasLane lanes[4];
    double prompt;
    unsigned havePrompt, parity, pairPhase[2];
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
