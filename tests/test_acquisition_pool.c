/** @file test_acquisition_pool.c @brief Parallel native search parity, ownership and admission. */
#include "ngene/acquisition.h"
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "line %d: %s\n", __LINE__, #x);                                        \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

/** Run the same frozen RF once serially and once at the available native concurrency. */
static TNgAcquisitionSnapshot Run(unsigned limit)
{
    TNgAcquisitionConfig config;
    TNgAcquisition *a = NULL;
    TNgStreamInfo stream = {0};
    TNgChannel channel;
    TNgChannelState next;
    TNgAcquisitionSnapshot snapshot;
    TNgError error = {{0}};
    TNgIqView view = {0};
    size_t count = 2048u * 56u, k;
    uint32_t random = 567;
    unsigned tries, maximum = 0;
    int suspended = 0;
    TNgIq8 *samples = calloc(count, sizeof(*samples));
    CHECK(samples);
    for (k = 0; k < count; k++) {
        random = random * 1664525u + 1013904223u;
        samples[k].i = (int8_t)(random >> 24);
        random = random * 1664525u + 1013904223u;
        samples[k].q = (int8_t)(random >> 24);
    }
    NgAcquisitionDefaults(&config);
    config.firstPrn = 1;
    config.lastPrn = 8;
    stream.sampleRateHz = 2048000;
    CHECK(NgAcquisitionCreate(&config, &stream, &a, &channel, &error) == NG_OK);
    NgAcquisitionRead(a, &snapshot);
    CHECK(snapshot.parallelCapacity >= 4);
    NgAcquisitionSetParallelism(a, 0, 8, 0);
    NgAcquisitionSetBudget(a,
                           1); /* Keep stage boundaries observable without changing arithmetic. */
    view.samples = samples;
    view.meta.count = count;
    view.meta.epoch = 9;
    view.meta.firstSample = 500000;
    CHECK(channel.ops->process(channel.context, &channel.id, channel.state, &view, &next, &error) ==
          NG_OK);
    NgSleepMs(10);
    NgAcquisitionRead(a, &snapshot);
    CHECK(!snapshot.activeSearches && !snapshot.searches);
    NgAcquisitionSetParallelism(a, limit, 8, limit);
    for (tries = 0; tries < 10000; tries++) {
        NgAcquisitionRead(a, &snapshot);
        if (snapshot.activeSearches > maximum)
            maximum = snapshot.activeSearches;
        CHECK(snapshot.poolActive <= snapshot.parallelCapacity);
        if (limit > 1 && !suspended && snapshot.activeSearches >= 4) {
            NgAcquisitionSetParallelism(a, 0, 8, 0);
            for (unsigned wait = 0; wait < 1000; wait++) {
                NgAcquisitionRead(a, &snapshot);
                if (!snapshot.poolExecuting)
                    break;
                NgSleepMs(1);
            }
            CHECK(!snapshot.poolExecuting && snapshot.poolActive >= 4);
            uint64_t completed = snapshot.searches;
            NgSleepMs(30);
            NgAcquisitionRead(a, &snapshot);
            CHECK(!snapshot.poolExecuting && snapshot.searches == completed);
            /* Existing jobs must obey the reduced execution cap as well as new jobs. */
            NgAcquisitionSetParallelism(a, limit, 8, 1);
            suspended = 1;
        }
        if (suspended)
            CHECK(snapshot.poolExecuting <= 1);
        if (snapshot.searches == 8 && !snapshot.poolActive)
            break;
        NgSleepMs(1);
    }
    CHECK(snapshot.searches == 8 && tries < 10000);
    if (limit > 1)
        CHECK(suspended);
    if (limit == 1)
        CHECK(maximum == 1);
    else
        CHECK(maximum >= 4);
    printf("native acquisition limit=%u observed=%u capacity=%u searches=%llu\n", limit, maximum,
           snapshot.parallelCapacity, (unsigned long long)snapshot.searches);
    NgAcquisitionDestroy(a);
    free(samples);
    return snapshot;
}

/** Whether 50 ms of busy work registers at least 25 ms on the thread CPU clock. */
static int ThreadClockMeasures(void)
{
    double cpu = NgThreadCpuSeconds(), wall = NgMonotonicSeconds();
    volatile double sink = 0;
    while (NgMonotonicSeconds() - wall < 0.05)
        sink += 1.0;
    return NgThreadCpuSeconds() - cpu >= 0.025;
}

/** A signal-time CPU budget delays searches until signal is processed, never changes them. */
static TNgAcquisitionSnapshot Budgeted(void)
{
    TNgAcquisitionConfig config;
    TNgAcquisition *a = NULL;
    TNgStreamInfo stream = {0};
    TNgChannel channel;
    TNgChannelState next;
    TNgAcquisitionSnapshot snapshot;
    TNgError error = {{0}};
    TNgIqView view = {0};
    size_t count = 2048u * 56u, k;
    uint32_t random = 567;
    uint64_t signal = 0, stalled;
    unsigned rounds;
    TNgIq8 *samples = calloc(count, sizeof(*samples));
    CHECK(samples);
    for (k = 0; k < count; k++) {
        random = random * 1664525u + 1013904223u;
        samples[k].i = (int8_t)(random >> 24);
        random = random * 1664525u + 1013904223u;
        samples[k].q = (int8_t)(random >> 24);
    }
    NgAcquisitionDefaults(&config);
    config.lastPrn = 8;
    config.cpuBudget = 0.01; /* 10 ms of search CPU per second of signal, 2.5 ms cap */
    stream.sampleRateHz = 2048000;
    CHECK(NgAcquisitionCreate(&config, &stream, &a, &channel, &error) == NG_OK);
    CHECK(NgAcquisitionAdvanceSignal(a, 3, signal, 2048000.0) == 1);
    NgAcquisitionSetParallelism(a, 4, 8, 1);
    view.samples = samples;
    view.meta.count = count;
    view.meta.epoch = 9;
    view.meta.firstSample = 500000;
    CHECK(channel.ops->process(channel.context, &channel.id, channel.state, &view, &next, &error) ==
          NG_OK);
    /* Without new signal the initial credit runs out long before eight searches. The stall
     * is only observable where the thread CPU clock measures busy time (not under Rosetta 2). */
    NgSleepMs(300);
    NgAcquisitionRead(a, &snapshot);
    stalled = snapshot.searches;
    if (ThreadClockMeasures()) {
        CHECK(stalled < 8);
        NgSleepMs(100);
        NgAcquisitionRead(a, &snapshot);
        CHECK(snapshot.searches == stalled);
    } else {
        puts("thread CPU clock under-reports busy time here: budget stall check skipped");
    }
    /* Each quarter second of processed signal releases the capped credit again. */
    for (rounds = 0; rounds < 20000 && (snapshot.searches < 8 || snapshot.poolActive); ++rounds) {
        signal += 512000;
        CHECK(NgAcquisitionAdvanceSignal(a, 3, signal, 2048000.0) == 1);
        NgSleepMs(1);
        NgAcquisitionRead(a, &snapshot);
    }
    CHECK(snapshot.searches == 8);
    printf("budgeted acquisition stalled at %llu searches, finished after %u signal steps\n",
           (unsigned long long)stalled, rounds);
    NgAcquisitionDestroy(a);
    free(samples);
    return snapshot;
}

/** Every per-satellite decision must remain identical under concurrent PRN scheduling. */
int main(void)
{
    TNgAcquisitionSnapshot serial = Run(1), parallel = Run(NG_ACQUISITION_MAX_PARALLEL);
    TNgAcquisitionSnapshot budgeted = Budgeted();
    unsigned k;
    for (k = 0; k < 8; k++) {
        const TNgAcquisitionResult *a = &serial.results[k], *b = &parallel.results[k];
        CHECK(a->prn == b->prn && a->epoch == b->epoch && a->firstSample == b->firstSample);
        CHECK(a->confirmed == b->confirmed && a->successes == b->successes);
        CHECK(a->dopplerHz == b->dopplerHz && a->codePhaseChips == b->codePhaseChips &&
              a->peakRatio == b->peakRatio);
        b = &budgeted.results[k];
        CHECK(a->confirmed == b->confirmed && a->successes == b->successes &&
              a->dopplerHz == b->dopplerHz && a->codePhaseChips == b->codePhaseChips &&
              a->peakRatio == b->peakRatio);
    }
    puts("native parallel pool preserves exact single-satellite results and pauses admission");
    return 0;
}
