/** @file sample_ops.h @brief Compatibility umbrella for integer sample mathematics. */
#ifndef NGENE_SAMPLE_OPS_H
#define NGENE_SAMPLE_OPS_H
#include "ngene/dsp/statistics.h"
#include "ngene/dsp/power.h"

/**
 * @brief Report the compiled sample primitive backend.
 *
 * @details
 * The returned static string may be used in diagnostics; no allocation or runtime ISA probing
 * occurs.
 *
 * @return SSE2-intrinsics, NEON-intrinsics or C-reference.
 */
const char *NgSampleOpsBackend(void);
#endif
