/** @file noise_policy.h @brief Shared frontend-independent acquisition noise policy. */
#ifndef NG_ACQUISITION_NOISE_POLICY_H
#define NG_ACQUISITION_NOISE_POLICY_H
#include <math.h>

/** Integer-shape gamma survival inverse, same statistical equation as Classic FalseAlarm.
 * Returns threshold / mean single-period noise power for a SUM of periods powers. */
static double AcquisitionThreshold(unsigned periods, double pfa)
{
    double low = 0;
    double high = fmax(256, -2 * log(pfa) + 2 * periods);
    unsigned iteration;
    for (iteration = 0; iteration < 70; ++iteration) {
        double x = (low + high) * 0.5;
        double term = 1;
        double sum = 1;
        unsigned k;
        for (k = 1; k < periods; ++k) {
            term *= x / (double)k;
            sum += term;
        }
        if (exp(-x) * sum > pfa) {
            low = x;
        } else {
            high = x;
        }
    }
    return (low + high) * 0.5;
}

/** Absolute fake Doppler for coarse noise measurement, outside either search grid. */
static double AcquisitionCoarseNoiseDoppler(void)
{
    return 22000.0;
}

/** Wrong carrier offsets have whole cycles in both 1-ms and 4-ms integrations.
 * Candidate code phase/rate are retained; only the carrier hypothesis changes. */
static double AcquisitionNoiseOffset(unsigned reference)
{
    return 20000.0 + (double)reference * 1000.0;
}
#endif
