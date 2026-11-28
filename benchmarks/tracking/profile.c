/** @file profile.c @brief Isolated production-kernel timing; never linked into the receiver. */
#include "ngene/dsp/search.h"
#include "ngene/dsp/tracking_replica.h"
#include "ngene/dsp/gps_ca.h"
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#define PROFILE(label, operation)                                                                  \
    do {                                                                                           \
        double start = NgMonotonicSeconds();                                                       \
        for (iteration = 0; iteration < repeats; ++iteration) {                                    \
            operation;                                                                             \
        }                                                                                          \
        printf("%s,%.9f\n", label, (NgMonotonicSeconds() - start) / repeats);                      \
    } while (0)

/** Report seconds per native-rate millisecond for each independently timed production kernel. */
int main(void)
{
    const size_t count = 16368;
    const unsigned repeats = 2000;
    unsigned iteration;
    size_t k;
    TNgComplex *values = calloc(count, sizeof(*values)), *carrier = calloc(count, sizeof(*carrier));
    double *phases = calloc(count, sizeof(*phases)), i = 0, q = 0;
    volatile double budget = 0;
    float *replica = calloc(count, sizeof(*replica));
    int8_t chips[1023];
    TNgSearchFft *fft = NULL;
    TNgError error = {{0}};
    if (!values || !carrier || !phases || !replica || NgGpsCaGenerate(7, chips, &error) != NG_OK ||
        NgSearchFftCreate(4096, &fft, &error) != NG_OK) {
        return 1;
    }
    for (k = 0; k < count; ++k) {
        values[k].i = (float)(k % 127);
        values[k].q = (float)(k % 63);
    }
    puts("kernel,seconds_per_call");
    PROFILE("budget_clock_pair_and_ema", double a = NgMonotonicSeconds();
            double b = NgMonotonicSeconds(); budget += 0.1 * ((b - a) / 0.001 - budget));
    PROFILE("carrier_precise", NgSearchCarrier(carrier, count, 16367600, 1350, 0.17, 0));
    PROFILE("carrier_square", NgSearchCarrier(carrier, count, 16367600, 1350, 0.17, 1));
    NgSearchCarrier(carrier, count, 16367600, 1350, 0.17, 0);
    PROFILE("complex_mix", NgSearchMix(values, carrier, count));
    PROFILE("code_phases", NgTrackingCodePhases(phases, count, 0.37, 1023000.2, 16367600));
    PROFILE("code_replica_one_tap", NgTrackingReplica(replica, phases, count, chips, 1023, 0.4, 0));
    PROFILE("correlation_product_one_tap", NgSearchCorrelate(values, replica, count, 0, &i, &q));
    PROFILE("correlation_xor_one_tap", NgSearchCorrelate(values, replica, count, 1, &i, &q));
    /* Forward/inverse pairs keep the fixture bounded between iterations. */
    for (k = 0; k < count; ++k) {
        values[k].i = (float)(k % 127);
        values[k].q = (float)(k % 63);
    }
    PROFILE("search_fft4096_forward_inverse", NgSearchFftExecute(fft, values, 0);
            NgSearchFftExecute(fft, values, 1));
    fprintf(stderr, "correlation checksum=%.12g\n", i + q);
    NgSearchFftDestroy(fft);
    free(replica);
    free(phases);
    free(carrier);
    free(values);
    return 0;
}
