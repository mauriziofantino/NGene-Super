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
        if (snapshot.activeSearches > maximum) {
            maximum = snapshot.activeSearches;
        }
        CHECK(snapshot.poolActive <= snapshot.parallelCapacity);
        if (limit > 1 && !suspended && snapshot.activeSearches >= 4) {
            NgAcquisitionSetParallelism(a, 0, 8, 0);
            for (unsigned wait = 0; wait < 1000; wait++) {
                NgAcquisitionRead(a, &snapshot);
                if (!snapshot.poolExecuting) {
                    break;
                }
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
        if (suspended) {
            CHECK(snapshot.poolExecuting <= 1);
        }
        if (snapshot.searches == 8 && !snapshot.poolActive) {
            break;
        }
        NgSleepMs(1);
    }
    CHECK(snapshot.searches == 8 && tries < 10000);
    if (limit > 1) {
        CHECK(suspended);
    }
    if (limit == 1) {
        CHECK(maximum == 1);
    } else {
        CHECK(maximum >= 4);
    }
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
    while (NgMonotonicSeconds() - wall < 0.05) {
        sink += 1.0;
    }
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

typedef struct {
    TNgConstellation system;
    unsigned *order;
    unsigned *count;
    TNgAtomicInt block, entered, cancelled;
} MockSubscription;

static int MockSearch(void *context, const TNgAcquisitionCapture *capture, unsigned prn,
                      const TNgAcquisitionConfig *config, TNgAcquisitionCancel cancel,
                      void *cancelContext, TNgAcquisitionResult *result, TNgError *error)
{
    MockSubscription *mock = context;
    (void)config;
    (void)error;
    CHECK(capture->count == 2048);
    if (NgAtomicGet(&mock->block)) {
        NgAtomicSet(&mock->entered, 1);
        while (!cancel(cancelContext)) {
            NgSleepMs(1);
        }
        NgAtomicSet(&mock->cancelled, 1);
        return NG_ERROR;
    }
    if (mock->order) {
        mock->order[(*mock->count)++] = 100u * (unsigned)mock->system + prn;
    }
    if (prn == 63) {
        CHECK(capture->displayPrn == 63);
    }
    result->prn = prn;
    result->epoch = capture->origin.epoch;
    result->firstSample = capture->origin.firstSample;
    result->dopplerHz = (double)mock->system;
    result->confirmed = 1;
    return NG_OK;
}

static const TNgAcquisitionStrategy mockStrategy = {MockSearch};

static void FeedMock(TNgChannel *channel, uint64_t firstSample)
{
    TNgIq8 samples[2048] = {{0}};
    TNgIqView view = {0};
    TNgChannelState next;
    TNgError error = {{0}};
    view.samples = samples;
    view.format = NG_FORMAT_CI8;
    view.meta.count = 2048;
    view.meta.epoch = 1;
    view.meta.firstSample = firstSample;
    CHECK(channel->ops->process(channel->context, &channel->id, channel->state, &view, &next,
                                &error) == NG_OK);
}

static void WaitMock(TNgAcquisition *a, uint64_t searches)
{
    TNgAcquisitionSnapshot snapshot;
    for (unsigned i = 0; i < 5000; ++i) {
        NgAcquisitionRead(a, &snapshot);
        if (snapshot.searches == searches && !snapshot.poolActive) {
            return;
        }
        NgSleepMs(1);
    }
    CHECK(0 && "mock acquisition timed out");
}

static void ThreeConstellations(void)
{
    TNgAcquisitionConfig config;
    TNgStreamInfo stream = {0};
    TNgError error = {{0}};
    TNgAcquisition *a[4] = {0};
    TNgChannel channels[4];
    TNgAcquisitionSnapshot snapshot;
    TNgAcquisitionResult results[NG_SATELLITE_MAX_PRN];
    unsigned order[16], count = 0;
    const unsigned expected[] = {131, 231, 331, 132, 232, 332, 333};
    MockSubscription mocks[4] = {{0}};

    stream.sampleRateHz = 2048000;
    NgAcquisitionDefaults(&config);
    config.firstPrn = 31;
    for (unsigned i = 0; i < 4; ++i) {
        TNgConstellation system = i < 3 ? (TNgConstellation)(i + 1) : NG_CONSTELLATION_GPS;
        mocks[i].system = system;
        mocks[i].order = order;
        mocks[i].count = &count;
        config.lastPrn = i == 2 ? 33 : 32;
        CHECK(NgAcquisitionCreateForConstellation(&config, &stream, system, 1, &mockStrategy,
                                                  &mocks[i], &a[i], &channels[i], &error) == NG_OK);
        CHECK(channels[i].id.constellation == (uint32_t)system);
        CHECK(channels[i].id.signal == NG_PRIMARY_SIGNAL);
    }
    CHECK(NgAcquisitionLink(a[0], a[1], &error) == NG_OK);
    CHECK(NgAcquisitionLink(a[1], a[2], &error) == NG_OK);
    CHECK(NgAcquisitionLink(a[2], a[3], &error) == NG_ERROR);
    NgAcquisitionDestroy(a[3]);
    NgAcquisitionRead(a[2], &snapshot);
    CHECK(snapshot.parallelCapacity == 3);

    NgAcquisitionSetParallelism(a[0], 0, 1, 0);
    for (unsigned i = 0; i < 3; ++i) {
        FeedMock(&channels[i], 0);
    }
    NgAcquisitionSetParallelism(a[0], 1, 1, 1);
    WaitMock(a[2], 3);
    CHECK(count == sizeof(expected) / sizeof(*expected));
    for (unsigned i = 0; i < count; ++i) {
        CHECK(order[i] == expected[i]);
    }
    for (unsigned i = 0; i < 3; ++i) {
        NgAcquisitionReadResults(a[i], results);
        CHECK(results[31].prn == 32 && results[31].dopplerHz == (double)mocks[i].system);
        CHECK(results[32].prn == (i == 2 ? 33u : 0u));
    }

    NgAcquisitionDestroy(a[0]);
    FeedMock(&channels[1], 2048);
    FeedMock(&channels[2], 2048);
    WaitMock(a[1], 4);
    WaitMock(a[2], 6);
    NgAcquisitionDestroy(a[1]);
    NgAcquisitionDestroy(a[2]);
}

static void BeidouPrnLimits(void)
{
    TNgAcquisitionConfig config;
    TNgStreamInfo stream = {0};
    TNgError error = {{0}};
    TNgChannel channel;
    TNgAcquisition *a = NULL;
    TNgAcquisitionSnapshot snapshot;
    TNgAcquisitionResult results[NG_SATELLITE_MAX_PRN];
    MockSubscription mock = {0};

    mock.system = NG_CONSTELLATION_BEIDOU;
    stream.sampleRateHz = 2048000;
    NgAcquisitionDefaults(&config);
    config.lastPrn = 63;
    CHECK(NgAcquisitionValidate(&config, &error) == NG_ERROR);
    config.firstPrn = 0;
    CHECK(NgAcquisitionCreateForConstellation(&config, &stream, mock.system, 1, &mockStrategy,
                                              &mock, &a, &channel, &error) == NG_ERROR);
    config.firstPrn = 1;
    config.lastPrn = 64;
    CHECK(NgAcquisitionCreateForConstellation(&config, &stream, mock.system, 1, &mockStrategy,
                                              &mock, &a, &channel, &error) == NG_ERROR);
    config.lastPrn = 63;
    CHECK(NgAcquisitionCreateForConstellation(&config, &stream, mock.system, 1, &mockStrategy,
                                              &mock, &a, &channel, &error) == NG_OK);
    NgAcquisitionSetAdmissionMask(a, UINT64_C(1) << 61, 0);
    NgAcquisitionObserve(a, 63);
    FeedMock(&channel, 0);
    WaitMock(a, 62);
    NgAcquisitionReadResults(a, results);
    NgAcquisitionRead(a, &snapshot);
    CHECK(snapshot.parallelCapacity == 1);
    CHECK(results[0].prn == 1 && results[31].prn == 32 && results[32].prn == 33);
    CHECK(results[61].prn == 0 && results[62].prn == 63 && snapshot.results[31].prn == 32);

    NgAtomicSet(&mock.block, 1);
    FeedMock(&channel, 2048);
    for (unsigned i = 0; i < 5000 && !NgAtomicGet(&mock.entered); ++i) {
        NgSleepMs(1);
    }
    CHECK(NgAtomicGet(&mock.entered));
    NgAcquisitionDestroy(a);
    CHECK(NgAtomicGet(&mock.cancelled));
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
    ThreeConstellations();
    BeidouPrnLimits();
    puts("native parallel pool preserves exact single-satellite results and pauses admission");
    return 0;
}
