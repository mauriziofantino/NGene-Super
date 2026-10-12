/** @file dc.c @brief Continuous Q32 DC estimator; no block-wise mean reset. */
#include "ngene/conditioning.h"
#include <stdlib.h>

/** @brief Continuous Q32 estimator and private conditioned output window. */
typedef struct {
    /** Continuous Q32 component estimates, index zero I and index one Q. */
    int64_t mean[2];
    /** Continuity generation; invalidates coherent history after a reset. */
    uint64_t epoch;
    /** Expected next complex-pair index for continuity detection. */
    uint64_t next;
    /** Estimator coefficient exponent; alpha equals two to the negative shift. */
    unsigned shift;
    /** Nonzero once the continuous estimator has a valid epoch. */
    int initialized;
    /** Preallocated sample or slot capacity, according to the owning container. */
    size_t capacity;
    /** Owned preallocated complex-pair storage; released by the instance destructor. */
    TNgIq8 *samples;
    /** Native input/output representation; no sample narrowing. */
    TNgSampleFormat format;
} TNgDcState;

/**
 * @brief Compute a defined arithmetic-floor right shift.
 *
 * @details
 * Handles negative Q32 estimator values without relying on implementation-defined signed
 * shifts. Inputs fit the estimator int64 range; shift is bounded.
 *
 * @param x Signed fixed-point value.
 * @param shift Right-shift amount, up to 32 in this module.
 *
 * @return floor(x / 2^shift).
 */
static int64_t FloorShift(int64_t x, unsigned shift)
{
    return x >= 0 ? (int64_t)((uint64_t)x >> shift) : -1 - (int64_t)((uint64_t)(-1 - x) >> shift);
}

/**
 * @brief Correct one signed sample and update the continuous DC estimate.
 *
 * @details
 * Subtracts the rounded previous Q32 mean, saturates output to [-128,127], then updates the
 * estimator with alpha = 2^-shift. State belongs to one component.
 *
 * @param x Input signed ADC component.
 * @param mean Read/write Q32 mean for this component.
 * @param shift Estimator exponent in 4..24.
 *
 * @return Corrected signed CI8 component.
 */
static int8_t RemoveDc(int8_t x, int64_t *mean, unsigned shift)
{
    int64_t y = (int64_t)x - FloorShift(*mean + INT64_C(2147483648), 32);
    *mean += FloorShift((int64_t)x * INT64_C(4294967296) - *mean, shift);
    /* CI8 output contract: saturate, never wrap multibit samples. */
    return (int8_t)(y > 127 ? 127 : y < -128 ? -128 : y);
}

/**
 * @brief Condition one view using continuous independent I/Q DC state.
 *
 * @details
 * Resets only on initial input, epoch/gap or index discontinuity; capacity checked before
 * writes. Output metadata matches input and output storage is borrowed until the next call.
 * Single receiver-thread use.
 *
 * @param context Owned DC state.
 * @param input Borrowed nonempty CI8 input view.
 * @param output Receives a borrowed view of the conditioner output.
 * @param error Optional error destination.
 *
 * @return NG_OK on success; NG_ERROR if input exceeds allocated capacity.
 */
static int Process(void *context, const TNgIqView *input, TNgIqView *output, TNgError *error)
{
    TNgDcState *s = context;
    size_t k;
    if (input->meta.count > s->capacity ||
        (input->format == 0 ? NG_FORMAT_CI8 : input->format) != s->format) {
        return NgFail(error, "DC conditioner capacity exceeded");
    }
    if (s->initialized == 0 || s->epoch != input->meta.epoch ||
        s->next != input->meta.firstSample || ((input->meta.flags) & (NG_BLOCK_GAP)) != 0) {
        s->mean[0] = s->mean[1] = 0;
        s->initialized = 1;
    }
    for (k = 0; k < input->meta.count; ++k) {
        if (s->format == NG_FORMAT_CI8) {
            s->samples[k].i = RemoveDc(input->samples[k].i, &s->mean[0], s->shift);
            s->samples[k].q = RemoveDc(input->samples[k].q, &s->mean[1], s->shift);
        } else {
            unsigned component;
            int limit = s->format == NG_FORMAT_SC16_Q11 ? 2048 : 128;
            for (component = 0; component < 2; ++component) {
                int x = NgIqComponent(input, k, component);
                int64_t y = (int64_t)x - FloorShift(s->mean[component] + INT64_C(2147483648), 32);
                s->mean[component] +=
                    FloorShift((int64_t)x * INT64_C(4294967296) - s->mean[component], s->shift);
                NgIqStore(s->samples, s->format, k, component,
                          (int)(y >= limit   ? limit - 1
                                : y < -limit ? -limit
                                             : y));
            }
        }
    }
    s->epoch = input->meta.epoch;
    s->next = input->meta.firstSample + input->meta.count;
    *output = *input;
    output->samples = s->samples;
    return NG_OK;
}

/**
 * @brief Allocate a continuous fixed-point I/Q DC estimator.
 *
 * @details
 * Creates private output storage. Its process callback preserves count/rate/indices and
 * saturates corrected components to CI8; state persists across blocks and resets on gaps,
 * index discontinuities or a new epoch. Use NgConditionerDcDestroy after the engine stops.
 *
 * Parameter capacity: Maximum complex pairs accepted per process call; positive.
 * Parameter shift: Estimator exponent 4..24: alpha = 2^-shift per sample.
 * Parameter out: Receives the owned conditioner instance on success.
 * Parameter error: Optional error destination.
 *
 * Result: NG_OK on success; NG_ERROR for invalid bounds or allocation failure.
 */
int NgConditionerDcCreateFormat(size_t capacity, unsigned shift, TNgSampleFormat format,
                                TNgConditioner *out, TNgError *error)
{
    TNgDcState *s;
    if (out == NULL || capacity == 0 || NgSampleBytes(format) == 0 ||
        capacity > SIZE_MAX / NgSampleBytes(format) || shift < 4 || shift > 24) {
        return NgFail(error, "DC removal needs capacity > 0 and shift 4..24");
    }
    s = calloc(1, sizeof(*s));
    if (s == NULL) {
        return NgFail(error, "Cannot allocate DC state");
    }
    s->samples = malloc(capacity * NgSampleBytes(format));
    if (s->samples == NULL) {
        free(s);
        return NgFail(error, "Cannot allocate DC output");
    }
    s->format = format == 0 ? NG_FORMAT_CI8 : format;
    s->capacity = capacity;
    s->shift = shift;
    out->context = s;
    out->name = "dc_iir";
    out->process = Process;
    return NG_OK;
}

/**
 * @brief Release storage owned by the continuous DC conditioner.
 *
 * @details
 * Stop processing first. A NULL pointer or a conditioner of another implementation is ignored;
 * a destroyed DC instance is replaced with a bypass value.
 *
 * Parameter conditioner: Conditioner created by NgConditionerDcCreate.
 */
void NgConditionerDcDestroy(TNgConditioner *conditioner)
{
    if (conditioner != NULL && conditioner->process == Process) {
        TNgDcState *s = conditioner->context;
        free(s->samples);
        free(s);
        *conditioner = NgConditionerBypass();
    }
}

/**
 * @brief Allocate a continuous fixed-point I/Q DC estimator.
 *
 * @details
 * Creates private output storage. Its process callback preserves count/rate/indices and
 * saturates corrected components to CI8; state persists across blocks and resets on gaps,
 * index discontinuities or a new epoch. Use NgConditionerDcDestroy after the engine stops.
 *
 * Parameter capacity Maximum complex pairs accepted per process call; positive.
 * Parameter shift Estimator exponent 4..24: alpha = 2^-shift per sample.
 * Parameter out Receives the owned conditioner instance on success.
 * Parameter error Optional error destination.
 *
 * Result: NG_OK on success; NG_ERROR for invalid bounds or allocation failure.
 */
int NgConditionerDcCreate(size_t capacity, unsigned shift, TNgConditioner *out, TNgError *error)
{
    return NgConditionerDcCreateFormat(capacity, shift, NG_FORMAT_CI8, out, error);
}
