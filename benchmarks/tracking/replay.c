/** @file replay.c @brief Isolated CI8 tracking trace from a saved acquisition seed. */
#include "ngene/pvt.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <errno.h>

/** Report the raw observation at an exact receiver sample using the operational PVT API. */
static int ReportObservation(const TNgTrackingSnapshot *snapshot, uint64_t sample, double rate,
                             double receiverTow)
{
    TNgPvtObservation observation;
    if (NgPvtObserve(snapshot, snapshot->epoch, sample, rate, receiverTow, &observation) != NG_OK) {
        fprintf(stderr, "No usable observation at sample %llu (PLL=%d, TOW=%d, frames=%llu)\n",
                (unsigned long long)sample, snapshot->pll, snapshot->navigation.towValid,
                (unsigned long long)snapshot->navigation.validSubframes);
        return 7;
    }
    puts("sample,integration_first_sample,integration_end_sample,code_delay_chips,code_rate_hz,"
         "navigation_boundary_sample,frames,receiver_tow_s,native_transmit_tow_s,"
         "common_transmit_tow_s,pseudorange_m,range_rate_mps");
    printf("%llu,%llu,%llu,%.12f,%.12f,%llu,%llu,%.12f,%.12f,%.12f,%.9f,%.9f\n",
           (unsigned long long)sample, (unsigned long long)snapshot->firstSample,
           (unsigned long long)snapshot->endSample, snapshot->codeDelayChips, snapshot->codeRateHz,
           (unsigned long long)snapshot->navigation.boundarySample,
           (unsigned long long)snapshot->navigation.validSubframes, receiverTow,
           observation.nativeTransmitTow, observation.commonTransmitTow, observation.pseudorange,
           observation.rangeRate);
    return 0;
}

/** Replay a saved seed through tracking, optionally stopping at one PVT sample. */
int main(int argc, char **argv)
{
    TNgTrackingConfig config;
    TNgStreamInfo stream = {0};
    TNgTracking *tracking = NULL;
    TNgAcquisitionResult seed = {0};
    TNgTrackingSnapshot s;
    TNgError error = {{0}};
    TNgIq8 *samples;
    TNgIqView view = {0};
    FILE *input;
    uint64_t first = 0, last = 0;
    size_t count, block;
    const int observe = argc == 11;
    uint64_t observationSample = 0;
    double receiverTow = 0;
    int result = 0;
    if (argc != 8 && argc != 9 && !observe) {
        fprintf(
            stderr,
            "replay file rate system prn doppler phase start_sample [seed_sample [observation_sample receiver_tow_s]]\n");
        return 2;
    }
    stream.sampleRateHz = (uint32_t)strtoul(argv[2], NULL, 10);
    block = stream.sampleRateHz / 1000u;
    NgTrackingDefaults(&config);
    seed.confirmed = 1;
    seed.prn = (unsigned)strtoul(argv[4], NULL, 10);
    seed.dopplerHz = strtod(argv[5], NULL);
    seed.codePhaseChips = strtod(argv[6], NULL);
    first = strtoull(argv[7], NULL, 10);
    seed.firstSample = argc >= 9 ? strtoull(argv[8], NULL, 10) : first;
    if (seed.firstSample > first || !block) {
        return 2;
    }
    if (observe) {
        char *end;
        errno = 0;
        observationSample = strtoull(argv[9], &end, 10);
        if (errno == ERANGE || argv[9][0] < '0' || argv[9][0] > '9' || *end ||
            observationSample <= first) {
            return 2;
        }
        receiverTow = strtod(argv[10], &end);
        if (end == argv[10] || *end || !isfinite(receiverTow)) {
            return 2;
        }
    }
    if (NgTrackingCreate(&config, &stream, (unsigned)strtoul(argv[3], NULL, 10), &tracking,
                         &error) != NG_OK) {
        return 3;
    }
    if (NgTrackingSeed(tracking, &seed, &error) != NG_OK) {
        return 4;
    }
    NgTrackingObserve(tracking, 0);
    samples = calloc(block, sizeof(*samples));
    input = fopen(argv[1], "rb");
    if (!samples || !input || first > LONG_MAX / 2 || fseek(input, (long)(2 * first), SEEK_SET)) {
        return 5;
    }
    if (!observe) {
        puts(
            "sample,integrations,doppler,code_rate,dll,carrier_error,prompt,bit_sync,subframes,tow,active,pll,navigation_boundary_sample,end_sample,transmit_time_seconds");
    }
    while (!observe || first < observationSample) {
        size_t readCount = block;
        if (observe && observationSample - first < block) {
            readCount = (size_t)(observationSample - first);
        }
        count = fread(samples, sizeof(*samples), readCount, input);
        if (!count) {
            break;
        }
        view.samples = samples;
        view.meta.firstSample = first;
        view.meta.count = count;
        if (NgTrackingProcess(tracking, &view, &error) != NG_OK) {
            fprintf(stderr, "%s\n", error.message);
            return 6;
        }
        first += count;
        NgTrackingRead(tracking, &s);
        if (!observe && first - last >= stream.sampleRateHz / 10u) {
            last = first;
            printf("%llu,%llu,%.9f,%.9f,%.9f,%.9f,%.9f,%d,%llu,%d,%d,%d,%llu,%llu,%.12f\n",
                   (unsigned long long)first, (unsigned long long)s.integrations, s.dopplerHz,
                   s.codeRateHz, s.dllError, s.carrierError, hypot(s.i[1], s.q[1]),
                   s.navigation.bitSynchronized, (unsigned long long)s.navigation.validSubframes,
                   s.navigation.towValid, s.active, s.pll,
                   (unsigned long long)s.navigation.boundarySample, (unsigned long long)s.endSample,
                   s.navigation.transmitTimeSeconds);
        }
    }
    if (observe) {
        if (first == observationSample) {
            result = ReportObservation(&s, first, stream.sampleRateHz, receiverTow);
        } else {
            fprintf(stderr, "Capture ended before the requested observation sample\n");
            result = 5;
        }
    }
    fclose(input);
    free(samples);
    NgTrackingDestroy(tracking);
    return result;
}
