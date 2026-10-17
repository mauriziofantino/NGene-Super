/** @file sample_backend.c @brief Report the compiler-selected sample primitive backend. */
#include "ngene/dsp/sample_ops.h"
#include "dsp/sample_backend.h"

/**
 * @brief Report the compiled sample primitive backend.
 *
 * @details
 * The returned static string may be used in diagnostics; no allocation or runtime ISA probing
 * occurs.
 *
 * Result: SSE2-intrinsics, NEON-intrinsics or C-reference.
 */
const char *NgSampleOpsBackend(void)
{
#if NG_SSE2
    return "SSE2-intrinsics";
#elif NG_NEON
    return "NEON-intrinsics";
#else
    return "C-reference";
#endif
}
