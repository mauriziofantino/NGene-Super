/** @file gps_ca.c @brief GPS L1 C/A Gold codes and persistent sampled replicas. */
#include "dsp/egnos_code.h"
#include <math.h>

int NgGpsCaGenerate(unsigned prn, int8_t chips[NG_GPS_CA_LENGTH], TNgError *error)
{
    static const unsigned char taps[32][2] = {
        {2, 6}, {3, 7}, {4, 8}, {5, 9}, {1, 9},  {2, 10}, {1, 8}, {2, 9}, {3, 10}, {2, 3}, {3, 4},
        {5, 6}, {6, 7}, {7, 8}, {8, 9}, {9, 10}, {1, 4},  {2, 5}, {3, 6}, {4, 7},  {5, 8}, {6, 9},
        {1, 3}, {4, 6}, {5, 7}, {6, 8}, {7, 9},  {8, 10}, {1, 6}, {2, 7}, {3, 8},  {4, 9}};
    unsigned g1 = 1023u, g2 = 1023u, n;
    if (chips == NULL || prn < 1u || prn > 32u)
        return NgFail(error, "GPS C/A PRN must be in 1..32");
    for (n = 0; n < NG_GPS_CA_LENGTH; ++n) {
        unsigned bit =
            ((g1 >> 9) ^ (g2 >> (taps[prn - 1u][0] - 1u)) ^ (g2 >> (taps[prn - 1u][1] - 1u))) & 1u;
        unsigned feedback1 = ((g1 >> 2) ^ (g1 >> 9)) & 1u;
        unsigned feedback2 =
            ((g2 >> 1) ^ (g2 >> 2) ^ (g2 >> 5) ^ (g2 >> 7) ^ (g2 >> 8) ^ (g2 >> 9)) & 1u;
        /* Legacy NGene stores XOR 1 as +1, XOR 0 as -1. */
        chips[n] = bit != 0u ? 1 : -1;
        g1 = ((g1 << 1) | feedback1) & 1023u;
        g2 = ((g2 << 1) | feedback2) & 1023u;
    }
    return NG_OK;
}

int NgGpsCaNcoInit(TNgGpsCaNco *nco, double sampleRateHz, double codeRateHz, double phaseChips,
                   TNgError *error)
{
    double phase, increment;
    if (nco == NULL || !isfinite(sampleRateHz) || sampleRateHz <= 0.0 || !isfinite(codeRateHz) ||
        codeRateHz <= 0.0 || !isfinite(phaseChips) || !isfinite(codeRateHz / sampleRateHz))
        return NgFail(error, "Invalid GPS code NCO parameters");
    phase = fmod(phaseChips, (double)NG_GPS_CA_LENGTH);
    if (phase < 0.0)
        phase += (double)NG_GPS_CA_LENGTH;
    if (phase >= (double)NG_GPS_CA_LENGTH)
        phase = 0.0;
    increment = fmod(codeRateHz / sampleRateHz, (double)NG_GPS_CA_LENGTH);
    nco->phase = (uint64_t)(phase * 4294967296.0);
    nco->step = (uint64_t)(increment * 4294967296.0 + 0.5);
    nco->denominator = 0;
    nco->remainder = 0;
    nco->wholeStep = 0;
    nco->fractionalStep = 0;
    /* Integer nominal rates use an exact rational accumulator. This matters at
     * rates such as 3 MHz, where the sampled code hits exact chip boundaries. */
    if (sampleRateHz <= 4294967295.0 && codeRateHz <= 4294967295.0 &&
        sampleRateHz == floor(sampleRateHz) && codeRateHz == floor(codeRateHz)) {
        uint64_t numerator = (uint64_t)llround(phase * sampleRateHz);
        uint64_t denominator = (uint64_t)sampleRateHz;
        numerator %= (uint64_t)NG_GPS_CA_LENGTH * denominator;
        nco->denominator = (uint32_t)denominator;
        nco->phase = (numerator / denominator) << 32;
        nco->remainder = numerator % denominator;
        nco->wholeStep = (uint32_t)((uint64_t)codeRateHz / denominator) % NG_GPS_CA_LENGTH;
        nco->fractionalStep = (uint32_t)((uint64_t)codeRateHz % denominator);
    }
    return NG_OK;
}

int NgGpsCaNext(const int8_t chips[NG_GPS_CA_LENGTH], TNgGpsCaNco *nco)
{
    const uint64_t period = (uint64_t)NG_GPS_CA_LENGTH << 32;
    int chip = chips[(size_t)(nco->phase >> 32)];
    if (nco->denominator != 0u) {
        nco->phase += (uint64_t)nco->wholeStep << 32;
        nco->remainder += nco->fractionalStep;
        if (nco->remainder >= nco->denominator) {
            nco->remainder -= nco->denominator;
            nco->phase += UINT64_C(1) << 32;
        }
    } else
        nco->phase += nco->step;
    if (nco->phase >= period)
        nco->phase -= period;
    return chip;
}

void NgGpsCaSample(const int8_t chips[NG_GPS_CA_LENGTH], TNgGpsCaNco *nco, int8_t *output,
                   size_t count)
{
    size_t n;
    for (n = 0; n < count; ++n)
        output[n] = (int8_t)NgGpsCaNext(chips, nco);
}

/** Use the G2 delays of IS-GPS-200N Table 6-I, also used by NGene Classic. */
void NgEgnosCode(unsigned slot, int8_t chips[NG_GPS_CA_LENGTH])
{
    static const unsigned delays[] = {175, 595};
    unsigned g1 = 1023, g2 = 1023;
    uint8_t direct[NG_GPS_CA_LENGTH], delayed[NG_GPS_CA_LENGTH];
    for (unsigned k = 0; k < NG_GPS_CA_LENGTH; ++k) {
        direct[k] = (uint8_t)(g1 >> 9);
        delayed[k] = (uint8_t)(g2 >> 9);
        const unsigned next1 = (g1 >> 2) ^ (g1 >> 9);
        const unsigned next2 =
            (g2 >> 1) ^ (g2 >> 2) ^ (g2 >> 5) ^ (g2 >> 7) ^ (g2 >> 8) ^ (g2 >> 9);
        g1 = ((g1 << 1) | (next1 & 1u)) & 1023u;
        g2 = ((g2 << 1) | (next2 & 1u)) & 1023u;
    }
    for (unsigned k = 0; k < NG_GPS_CA_LENGTH; ++k) {
        chips[k] = (direct[k] ^ delayed[(k + NG_GPS_CA_LENGTH - delays[slot]) % NG_GPS_CA_LENGTH])
                       ? -1
                       : 1;
    }
}
