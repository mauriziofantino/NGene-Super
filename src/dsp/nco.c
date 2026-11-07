/** @file nco.c @brief Interpolated carrier table and modulo-Q64 phase accumulator. */
#include "ngene/dsp/nco.h"
#include <math.h>

static uint64_t CyclePhase(double cycles)
{
    double phase = fmod(cycles, 1.0), upper, lower;
    int negative = phase < 0.0;
    uint64_t result;
    uint32_t hi, lo;
    if (negative)
        phase = -phase;
    /* Split the conversion to avoid a double-to-uint64 cast near UINT64_MAX. */
    upper = phase * 4294967296.0;
    hi = (uint32_t)upper;
    lower = (upper - (double)hi) * 4294967296.0;
    lo = (uint32_t)lower;
    result = ((uint64_t)hi << 32) | (uint64_t)lo;
    return negative ? UINT64_C(0) - result : result;
}

void NgNcoTableInit(TNgNcoTable *table)
{
    unsigned n;
    const double tau = 6.283185307179586476925286766559;
    for (n = 0; n < NG_NCO_TABLE_SIZE; ++n) {
        double phase = tau * (double)n / (double)NG_NCO_TABLE_SIZE;
        table->cosine[n] = (int32_t)llround(cos(phase) * 1073741824.0);
        table->sine[n] = (int32_t)llround(sin(phase) * 1073741824.0);
    }
    table->cosine[NG_NCO_TABLE_SIZE] = table->cosine[0];
    table->sine[NG_NCO_TABLE_SIZE] = table->sine[0];
}

int NgNcoInit(TNgCarrierNco *nco, double sampleRateHz, double frequencyHz, double phaseCycles,
              TNgError *error)
{
    if (nco == NULL || !isfinite(sampleRateHz) || sampleRateHz <= 0.0 || !isfinite(frequencyHz) ||
        !isfinite(phaseCycles) || !isfinite(frequencyHz / sampleRateHz))
        return NgFail(error, "Invalid carrier NCO parameters");
    nco->phase = CyclePhase(phaseCycles);
    nco->step = CyclePhase(frequencyHz / sampleRateHz);
    return NG_OK;
}

static void Components(const TNgNcoTable *table, TNgCarrierNco *nco, int64_t *cosine, int64_t *sine)
{
    size_t index = (size_t)(nco->phase >> 52);
    int64_t fraction = (int64_t)((nco->phase >> 32) & UINT64_C(1048575));
    *cosine = table->cosine[index] +
              ((int64_t)(table->cosine[index + 1u] - table->cosine[index]) * fraction) / 1048576;
    *sine = table->sine[index] +
            ((int64_t)(table->sine[index + 1u] - table->sine[index]) * fraction) / 1048576;
    nco->phase += nco->step;
}

void NgNcoRotate(const TNgNcoTable *table, TNgCarrierNco *nco, int i, int q, double *outI,
                 double *outQ)
{
    int64_t cosine, sine;
    Components(table, nco, &cosine, &sine);
    *outI = ((double)((int64_t)i * cosine + (int64_t)q * sine)) * (1.0 / 1073741824.0);
    *outQ = ((double)((int64_t)q * cosine - (int64_t)i * sine)) * (1.0 / 1073741824.0);
}

void NgNcoRotateF64(const TNgNcoTable *table, TNgCarrierNco *nco, double i, double q, double *outI,
                    double *outQ)
{
    int64_t cosine, sine;
    Components(table, nco, &cosine, &sine);
    *outI = (i * (double)cosine + q * (double)sine) * (1.0 / 1073741824.0);
    *outQ = (q * (double)cosine - i * (double)sine) * (1.0 / 1073741824.0);
}
