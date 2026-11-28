/** @file throughput.c @brief Isolated tracking load probe; no RF or receiver instrumentation. */
#include "ngene/tracking.h"
#include "platform/platform.h"
#include "core/channel_dispatch.h"
#include <stdio.h>
#include <stdlib.h>

/** Dispatch one exclusively owned benchmark channel. */
static int Process(void *context, const TNgChannelId *id, TNgChannelState state,
                   const TNgIqView *window, TNgChannelState *next, TNgError *error)
{
    (void)id;
    *next = state;
    return NgTrackingProcess(context, window, error);
}

/** Benchmark uses the same bounded scheduler and barrier as production. */
static const TNgChannelOps ops = {
    Process, 0, NULL,
    0}; /** Measure private channels against nominal signal duration; input generation is excluded. */

int main(int argc, char **argv)
{
    TNgTrackingConfig config;
    TNgStreamInfo stream = {0};
    TNgError error = {{0}};
    TNgTracking *channels[32] = {0};
    TNgChannel jobs[32] = {0};
    TNgChannelDispatch *dispatch = NULL;
    unsigned workers = argc > 5 ? (unsigned)strtoul(argv[5], NULL, 10) : 1;
    TNgIq8 *samples;
    TNgIqView view = {0};
    unsigned channel, window;
    unsigned count = argc > 3 ? (unsigned)strtoul(argv[3], NULL, 10) : 8;
    unsigned windows = argc > 4 ? (unsigned)strtoul(argv[4], NULL, 10) : 100;
    double begin, elapsed, checksum = 0;
    size_t n;
    stream.sampleRateHz = argc > 1 ? (uint32_t)strtoul(argv[1], NULL, 10) : 16000000;
    NgTrackingDefaults(&config);
    config.tapCount = argc > 2 ? (unsigned)strtoul(argv[2], NULL, 10) : 9;
    config.squareCarrier = argc > 6 ? atoi(argv[6]) : 0;
    config.xorCorrelation = argc > 7 ? atoi(argv[7]) : 0;
    if (!count || count > 32 || !windows || stream.sampleRateHz < 2048000 ||
        NgTrackingValidate(&config, &error) != NG_OK)
        return 1;
    view.meta.count = stream.sampleRateHz / 1000;
    samples = calloc(view.meta.count, sizeof(*samples));
    if (!samples)
        return 1;
    for (n = 0; n < view.meta.count; ++n) {
        samples[n].i = (int8_t)((n * 31u) % 127u);
        samples[n].q = (int8_t)(-((int)(n * 17u % 127u)));
    }
    view.samples = samples;
    for (channel = 0; channel < count; ++channel) {
        TNgAcquisitionResult seed = {0};
        seed.confirmed = 1;
        seed.prn = channel + 1;
        seed.dopplerHz = 350;
        seed.codePhaseChips = 173.25;
        if (NgTrackingCreate(&config, &stream, channel % 2 + 1, &channels[channel], &error) !=
                NG_OK ||
            NgTrackingSeed(channels[channel], &seed, &error) != NG_OK)
            return 1;
        if (argc > 8)
            NgTrackingObserve(channels[channel],
                              atoi(argv[8]) == 2 ? channel == 0 : atoi(argv[8]) != 0);
    }
    for (channel = 0; channel < count; ++channel) {
        jobs[channel].ops = &ops;
        jobs[channel].context = channels[channel];
        jobs[channel].state = NG_CHANNEL_TRACKING;
    }
    if (NgChannelDispatchCreate(jobs, count, workers, &dispatch, &error) != NG_OK)
        return 1;
    begin = NgMonotonicSeconds();
    for (window = 0; window < windows; ++window) {
        view.meta.firstSample = (uint64_t)window * view.meta.count;
        if (NgChannelDispatchProcess(dispatch, &view, &error) != NG_OK) {
            fprintf(stderr, "%s\n", error.message);
            return 1;
        }
    }
    elapsed = NgMonotonicSeconds() - begin;
    for (channel = 0; channel < count; ++channel) {
        TNgTrackingSnapshot snapshot;
        unsigned tap;
        NgTrackingRead(channels[channel], &snapshot);
        for (tap = 0; tap < config.tapCount; ++tap)
            checksum += snapshot.i[tap] + snapshot.q[tap];
        NgTrackingDestroy(channels[channel]);
    }
    NgChannelDispatchDestroy(dispatch);
    free(samples);
    printf(
        "Fs=%u taps=%u channels=%u workers=%u square=%d xor=%d signal=%.3fs elapsed=%.6fs real_time_ratio=%.3f checksum=%.12g\n",
        stream.sampleRateHz, config.tapCount, count, workers, config.squareCarrier,
        config.xorCorrelation, (double)windows * view.meta.count / stream.sampleRateHz, elapsed,
        elapsed / ((double)windows * view.meta.count / stream.sampleRateHz), checksum);
    return 0;
}
