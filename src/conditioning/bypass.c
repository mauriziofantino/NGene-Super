/** @file bypass.c
 * @brief NGene conditioning implementation.
 */
#include "ngene/conditioning.h"

/**
 * @brief Forward samples and metadata without copying.
 *
 * @details
 * Stateless rate-preserving callback. The output aliases input and inherits its borrowed
 * lifetime; no DSP or allocation.
 *
 * @param context Unused opaque context.
 * @param input Borrowed input view.
 * @param output Receives an alias of input.
 * @param error Unused optional error destination.
 *
 * @return NG_OK.
 */
static int Bypass(void *context, const TNgIqView *input, TNgIqView *output, TNgError *error)
{
    (void)context;
    (void)error;
    *output = *input;
    return NG_OK;
}

/**
 * @brief Return a stateless conditioner which preserves the input view.
 *
 * @details
 * No allocation or destroy is necessary. The process callback aliases the input samples and
 * metadata; their original lifetime still applies.
 *
 * Result: A rate-preserving CI8 conditioner value.
 */
TNgConditioner NgConditionerBypass(void)
{
    TNgConditioner result = {NULL, "bypass", Bypass};
    return result;
}
