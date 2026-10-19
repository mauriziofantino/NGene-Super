/** @file test_engine_scheduler.c @brief Shared-window serial and parallel engine contracts. */
#include "ngene/receiver.h"
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expression)                                                                          \
    do {                                                                                           \
        if (!(expression)) {                                                                       \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expression);                  \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

enum { NG_TEST_CHANNELS = 6, NG_TEST_WINDOWS = 5 };

/** @brief Test modes select callback behavior without changing the receiver. */
typedef enum {
    NG_TEST_SERIAL,
    NG_TEST_PARALLEL,
    NG_TEST_FAILURE,
    NG_TEST_INVALID_TRANSITION
} TNgSchedulerTestMode;

/** @brief Injectable source, synchronized channel state and copied run results. */
typedef struct {
    /** Protects channel counters and the explicit concurrent-callback handshake. */
    TNgMutex mutex;
    /** Notifies callback entry and release; always used with mutex. */
    TNgCondition condition;
    /** Complete startup configuration, owned by this fixture. */
    TNgConfig config;
    /** Frontend descriptor whose context is this fixture. */
    TNgFrontend frontend;
    /** Stateless conditioner used for the source data. */
    TNgConditioner conditioner;
    /** Independently identified channels sharing explicitly synchronized test state. */
    TNgChannel channels[NG_TEST_CHANNELS];
    /** Requested bounded concurrency for the scheduled entry point. */
    TNgReceiverExecution execution;
    /** Receiver-thread hook that checks the per-window completion barrier. */
    TNgReceiverHooks hooks;
    /** Number of registered entries in channels. */
    size_t channelCount;
    /** Callback behavior selected before the receiver starts. */
    TNgSchedulerTestMode mode;
    /** Reused source buffer, immutable during each frontend lease. */
    TNgIq8 samples[4];
    /** Source operations are owned by the receiver thread until it is joined. */
    size_t sourceCalls;
    /** Number of frontend starts. */
    unsigned starts;
    /** Number of frontend stops after all processing has completed. */
    unsigned stops;
    /** Number of acquired source blocks subsequently released. */
    unsigned releases;
    /** Nonzero while the source owns an outstanding borrow. */
    int leased;
    /** Number of finished callbacks, separately indexed for each channel. */
    unsigned calls[NG_TEST_CHANNELS];
    /** Nonzero while a particular channel callback is active. */
    int active[NG_TEST_CHANNELS];
    /** Number of simultaneously active callbacks. */
    unsigned activeCount;
    /** Maximum observed simultaneous callbacks; no elapsed-time comparison is used. */
    unsigned maximumActive;
    /** Number of jobs that have entered each common window. */
    unsigned entered[NG_TEST_WINDOWS];
    /** First common sample pointer observed for each window. */
    const TNgIq8 *common[NG_TEST_WINDOWS];
    /** Number of completed receiver-thread block hooks. */
    unsigned hookCalls;
    /** Nonzero when this entry point exposes the optional block hook. */
    int expectHooks;
    /** Indicates that the deliberately early second-channel failure is ready. */
    int highFailureReady;
    /** External test thread releases the deliberately blocked first channel. */
    int releaseFirst;
    /** Both error-path callbacks have finished touching borrowed samples. */
    unsigned failureFinished;
    /** Final return code, accessed after receiver-thread join. */
    int result;
    /** Final engine statistics, accessed after receiver-thread join. */
    TNgReceiverStats stats;
    /** Final diagnostic, accessed after receiver-thread join. */
    TNgError error;
    size_t reportedLoad;
    unsigned loadNotifications;
} TNgSchedulerFixture;

/** @brief Reset the owned source before one run.
 * @param context Borrowed fixture.
 * @param error Optional unused diagnostic.
 * @return NG_OK.
 */
static int SourceStart(void *context, TNgError *error)
{
    TNgSchedulerFixture *fixture = context;
    (void)error;
    CHECK(fixture->leased == 0);
    fixture->sourceCalls = 0;
    ++fixture->starts;
    return NG_OK;
}

/** @brief Return unequal source blocks with an implicit ten-sample discontinuity.
 * @param context Borrowed fixture.
 * @param view Receives one borrowed CI8 block.
 * @param timeoutMs Unused synchronous-source timeout.
 * @param error Optional unused diagnostic.
 * @return NG_OK for five blocks, then NG_END.
 */
static int SourceAcquire(void *context, TNgIqView *view, unsigned timeoutMs, TNgError *error)
{
    static const uint64_t first[] = {0, 3, 7, 20, 24};
    static const size_t counts[] = {3, 4, 3, 4, 3};
    TNgSchedulerFixture *fixture = context;
    size_t index;
    (void)timeoutMs;
    (void)error;
    CHECK(fixture->leased == 0);
    if (fixture->sourceCalls == NG_TEST_WINDOWS) {
        return NG_END;
    }
    memset(view, 0, sizeof(*view));
    view->samples = fixture->samples;
    view->format = NG_FORMAT_CI8;
    view->meta.firstSample = first[fixture->sourceCalls];
    view->meta.count = counts[fixture->sourceCalls];
    for (index = 0; index < view->meta.count; ++index) {
        fixture->samples[index].i = (int8_t)((view->meta.firstSample + index) % 100u);
        fixture->samples[index].q = (int8_t)-fixture->samples[index].i;
    }
    fixture->leased = 1;
    ++fixture->sourceCalls;
    return NG_OK;
}

/** @brief Release exactly one source lease.
 * @param context Borrowed fixture.
 */
static void SourceRelease(void *context)
{
    TNgSchedulerFixture *fixture = context;
    CHECK(fixture->leased != 0);
    fixture->leased = 0;
    ++fixture->releases;
}

/** @brief Check that cleanup waits until every callback has stopped accessing the window.
 * @param context Borrowed fixture.
 * @param error Optional unused diagnostic.
 * @return NG_OK.
 */
static int SourceStop(void *context, TNgError *error)
{
    TNgSchedulerFixture *fixture = context;
    (void)error;
    NgMutexLock(&fixture->mutex);
    CHECK(fixture->activeCount == 0 && fixture->leased == 0);
    if (fixture->mode == NG_TEST_FAILURE || fixture->mode == NG_TEST_INVALID_TRANSITION) {
        CHECK(fixture->failureFinished == fixture->channelCount);
    }
    ++fixture->stops;
    NgMutexUnlock(&fixture->mutex);
    return NG_OK;
}

/** @brief Fixture operations need no vendor, device or filesystem. */
static const TNgFrontendOps sourceOps = {SourceStart, SourceAcquire, SourceRelease,
                                         SourceStop,  NULL,          NULL};

/** @brief Wait on a test predicate with a deadlock guard, not a performance threshold.
 * @param fixture Fixture whose mutex the caller owns.
 * @param predicate Value updated under the same mutex.
 */
static void WaitFor(TNgSchedulerFixture *fixture, const int *predicate)
{
    double deadline = NgMonotonicSeconds() + 10.0;
    while (*predicate == 0) {
        CHECK(NgMonotonicSeconds() < deadline);
        NgConditionWait(&fixture->condition, &fixture->mutex, 100);
    }
}

/** @brief Verify immutable samples and normalized gap/partial metadata.
 * @param window Borrowed shared ring view.
 * @param frame Zero-based expected engine window.
 */
static void CheckWindow(const TNgIqView *window, unsigned frame)
{
    static const uint64_t first[] = {0, 4, 8, 20, 24};
    static const size_t counts[] = {4, 4, 2, 4, 3};
    size_t index;
    CHECK(frame < NG_TEST_WINDOWS);
    CHECK(window->format == NG_FORMAT_CI8);
    CHECK(window->meta.firstSample == first[frame]);
    CHECK(window->meta.count == counts[frame]);
    CHECK(((window->meta.flags & NG_BLOCK_PARTIAL) != 0) == (frame == 2 || frame == 4));
    CHECK(((window->meta.flags & NG_BLOCK_GAP) != 0) == (frame == 3));
    if (frame == 3) {
        CHECK(window->meta.missingKnown != 0 && window->meta.missingBefore == 10);
    }
    for (index = 0; index < window->meta.count; ++index) {
        CHECK(window->samples[index].i == (int8_t)((first[frame] + index) % 100u));
        CHECK(window->samples[index].q == -window->samples[index].i);
    }
}

/** @brief Process one window while explicitly coordinating concurrent jobs.
 * @param context Borrowed synchronized fixture.
 * @param id Unique registered channel identity.
 * @param state Current engine-owned channel state.
 * @param window Immutable common window.
 * @param next Receives a valid transition, or a deliberate invalid transition in its test.
 * @param error Receives a deliberate diagnostic in error tests.
 * @return NG_OK normally; NG_ERROR in configured failure tests.
 */
static int Process(void *context, const TNgChannelId *id, TNgChannelState state,
                   const TNgIqView *window, TNgChannelState *next, TNgError *error)
{
    TNgSchedulerFixture *fixture = context;
    size_t channel = id->instanceId;
    unsigned frame;
    int result = NG_OK;
    NgMutexLock(&fixture->mutex);
    CHECK(channel < fixture->channelCount && fixture->active[channel] == 0);
    frame = fixture->calls[channel];
    CheckWindow(window, frame);
    CHECK(state == (TNgChannelState)(frame % 4u));
    CHECK(fixture->expectHooks == 0 || fixture->hookCalls == frame);
    fixture->active[channel] = 1;
    ++fixture->activeCount;
    if (fixture->activeCount > fixture->maximumActive) {
        fixture->maximumActive = fixture->activeCount;
    }
    if (fixture->common[frame] == NULL) {
        fixture->common[frame] = window->samples;
    }
    CHECK(fixture->common[frame] == window->samples);
    ++fixture->entered[frame];
    NgConditionSignal(&fixture->condition);
    if (fixture->mode == NG_TEST_SERIAL) {
        size_t earlier;
        for (earlier = 0; earlier < channel; ++earlier) {
            CHECK(fixture->calls[earlier] == frame + 1u);
        }
        CHECK(fixture->activeCount == 1);
    } else if (fixture->mode == NG_TEST_PARALLEL) {
        double deadline = NgMonotonicSeconds() + 10.0;
        while (fixture->entered[frame] < 2) {
            CHECK(NgMonotonicSeconds() < deadline);
            NgConditionWait(&fixture->condition, &fixture->mutex, 100);
        }
    } else if (channel == 0) {
        WaitFor(fixture, &fixture->releaseFirst);
        CHECK(fixture->highFailureReady != 0);
        if (fixture->mode == NG_TEST_FAILURE) {
            result = NgFail(error, "first registered channel failed");
        }
    } else {
        fixture->highFailureReady = 1;
        NgConditionSignal(&fixture->condition);
        result = NgFail(error, "second registered channel failed");
    }
    CheckWindow(window, frame);
    *next = fixture->mode == NG_TEST_INVALID_TRANSITION && channel == 0
                ? NG_CHANNEL_HOLDOVER
                : (TNgChannelState)(((unsigned)state + 1u) % 4u);
    ++fixture->calls[channel];
    fixture->active[channel] = 0;
    --fixture->activeCount;
    if (fixture->mode == NG_TEST_FAILURE || fixture->mode == NG_TEST_INVALID_TRANSITION) {
        ++fixture->failureFinished;
    }
    NgConditionSignal(&fixture->condition);
    NgMutexUnlock(&fixture->mutex);
    return result;
}

/** @brief Shared immutable channel operations; mutable state belongs to the fixture. */
static const TNgChannelOps channelOps = {Process, 0, 0};

/** @brief Verify that the receiver-thread hook runs only after the entire window joins.
 * @param context Borrowed synchronized fixture.
 * @param view Completed common window.
 * @param error Optional unused diagnostic.
 * @return NG_OK.
 */
static int Block(void *context, const TNgIqView *view, TNgError *error)
{
    TNgSchedulerFixture *fixture = context;
    size_t channel;
    (void)error;
    NgMutexLock(&fixture->mutex);
    CheckWindow(view, fixture->hookCalls);
    CHECK(fixture->activeCount == 0);
    for (channel = 0; channel < fixture->channelCount; ++channel) {
        CHECK(fixture->calls[channel] == fixture->hookCalls + 1u);
    }
    ++fixture->hookCalls;
    NgMutexUnlock(&fixture->mutex);
    return NG_OK;
}

/** @brief Initialize a source and bounded test channels without starting threads.
 * @param fixture Receives the initialized owned fixture.
 * @param mode Selected test behavior.
 * @param count Number of independently identified channels.
 * @param workers Requested execution concurrency.
 */
static void Initialize(TNgSchedulerFixture *fixture, TNgSchedulerTestMode mode, size_t count,
                       size_t workers)
{
    size_t channel;
    memset(fixture, 0, sizeof(*fixture));
    CHECK(NgMutexInit(&fixture->mutex) == 0);
    CHECK(NgConditionInit(&fixture->condition) == 0);
    NgConfigDefaults(&fixture->config);
    fixture->config.sampleRateHz = 100000;
    fixture->config.windowSamples = 4;
    fixture->config.runSeconds = 0;
    fixture->frontend.ops = &sourceOps;
    fixture->frontend.context = fixture;
    fixture->frontend.info.sampleRateHz = fixture->config.sampleRateHz;
    fixture->frontend.info.centerFrequencyHz = fixture->config.centerFrequencyHz;
    fixture->frontend.info.signalFrequencyHz = fixture->config.centerFrequencyHz;
    fixture->frontend.info.format = NG_FORMAT_CI8;
    fixture->frontend.info.sourceName = "test-only scheduler fixture";
    fixture->frontend.info.clockDomain = "test-only sample indices";
    fixture->conditioner = NgConditionerBypass();
    fixture->execution.workerCount = workers;
    fixture->hooks.context = fixture;
    fixture->hooks.block = Block;
    fixture->channelCount = count;
    fixture->mode = mode;
    fixture->expectHooks = 1;
    CHECK(count <= NG_TEST_CHANNELS);
    for (channel = 0; channel < count; ++channel) {
        fixture->channels[channel].id.instanceId = (uint32_t)channel;
        fixture->channels[channel].state = NG_CHANNEL_ACQUISITION;
        fixture->channels[channel].ops = &channelOps;
        fixture->channels[channel].context = fixture;
    }
}

/** @brief Destroy fixture synchronization after receiver callbacks and threads finish.
 * @param fixture Initialized stopped fixture.
 */
static void Destroy(TNgSchedulerFixture *fixture)
{
    CHECK(fixture->activeCount == 0 && fixture->leased == 0);
    NgConditionDestroy(&fixture->condition);
    NgMutexDestroy(&fixture->mutex);
}

/** @brief Invoke the scheduled engine from either the caller or an explicit test thread.
 * @param context Borrowed fixture valid until this function returns.
 */
static void RunScheduled(void *context)
{
    TNgSchedulerFixture *fixture = context;
    fixture->result = NgReceiverRunScheduled(&fixture->config, &fixture->frontend,
                                             &fixture->conditioner, fixture->channels,
                                             fixture->channelCount, NULL, NULL, &fixture->stats,
                                             &fixture->execution, &fixture->hooks, &fixture->error);
}

/** @brief Verify serial wrappers, explicit serial execution and the NULL execution default. */
static void TestSerialCompatibility(void)
{
    unsigned entry;
    for (entry = 0; entry < 4; ++entry) {
        TNgSchedulerFixture fixture;
        Initialize(&fixture, NG_TEST_SERIAL, 2, 1);
        if (entry == 0) {
            fixture.expectHooks = 0;
            fixture.result = NgReceiverRun(&fixture.config, &fixture.frontend, &fixture.conditioner,
                                           fixture.channels, fixture.channelCount, NULL, NULL,
                                           &fixture.stats, &fixture.error);
        } else if (entry == 1) {
            fixture.result = NgReceiverRunEx(
                &fixture.config, &fixture.frontend, &fixture.conditioner, fixture.channels,
                fixture.channelCount, NULL, NULL, &fixture.stats, &fixture.hooks, &fixture.error);
        } else if (entry == 2) {
            RunScheduled(&fixture);
        } else {
            fixture.result =
                NgReceiverRunScheduled(&fixture.config, &fixture.frontend, &fixture.conditioner,
                                       fixture.channels, fixture.channelCount, NULL, NULL,
                                       &fixture.stats, NULL, &fixture.hooks, &fixture.error);
        }
        CHECK(fixture.result == NG_OK);
        CHECK(fixture.starts == 1 && fixture.stops == 1 && fixture.releases == 5);
        CHECK(fixture.stats.processedSamples == 17 && fixture.stats.windows == 5);
        CHECK(fixture.maximumActive == 1);
        CHECK(fixture.calls[0] == 5 && fixture.calls[1] == 5);
        CHECK(fixture.channels[0].state == NG_CHANNEL_TRACKING);
        CHECK(fixture.channels[1].state == NG_CHANNEL_TRACKING);
        if (entry != 0) {
            CHECK(fixture.hookCalls == 5);
        }
        Destroy(&fixture);
    }
}

/** @brief Verify shared immutable leases, per-channel ordering and bounded parallel jobs. */
static void TestParallelWindows(void)
{
    size_t requested;
    for (requested = 2; requested <= 8; requested += 6) {
        TNgSchedulerFixture fixture;
        size_t channel;
        Initialize(&fixture, NG_TEST_PARALLEL, NG_TEST_CHANNELS, requested);
        RunScheduled(&fixture);
        CHECK(fixture.result == NG_OK);
        CHECK(fixture.maximumActive >= 2 && fixture.maximumActive <= requested);
        CHECK(fixture.maximumActive <= fixture.channelCount);
        CHECK(fixture.stats.processedSamples == 17 && fixture.stats.windows == 5);
        CHECK(fixture.stats.gapEvents == 1 && fixture.hookCalls == 5);
        CHECK(fixture.starts == 1 && fixture.stops == 1 && fixture.releases == 5);
        for (channel = 0; channel < fixture.channelCount; ++channel) {
            CHECK(fixture.calls[channel] == 5);
            CHECK(fixture.channels[channel].state == NG_CHANNEL_TRACKING);
        }
        Destroy(&fixture);
    }
}

/** @brief Delay the first registered job and verify error ordering and cleanup barriers. */
static void TestFailureBarrier(void)
{
    TNgSchedulerTestMode mode;
    for (mode = NG_TEST_FAILURE; mode <= NG_TEST_INVALID_TRANSITION; ++mode) {
        TNgSchedulerFixture fixture;
        TNgThread receiver;
        Initialize(&fixture, mode, 2, 2);
        CHECK(NgThreadStart(&receiver, RunScheduled, &fixture) == 0);
        NgMutexLock(&fixture.mutex);
        WaitFor(&fixture, &fixture.active[0]);
        WaitFor(&fixture, &fixture.highFailureReady);
        CHECK(fixture.stops == 0);
        fixture.releaseFirst = 1;
        NgConditionSignal(&fixture.condition);
        NgMutexUnlock(&fixture.mutex);
        NgThreadJoin(receiver);
        CHECK(fixture.result == NG_ERROR);
        CHECK(fixture.calls[0] == 1 && fixture.calls[1] == 1);
        CHECK(fixture.failureFinished == 2 && fixture.stops == 1);
        CHECK(fixture.stats.windows == 0 && fixture.hookCalls == 0);
        CHECK(fixture.channels[0].state == NG_CHANNEL_ACQUISITION);
        CHECK(fixture.channels[1].state == NG_CHANNEL_ACQUISITION);
        if (mode == NG_TEST_FAILURE) {
            CHECK(strstr(fixture.error.message, "first registered channel failed") != NULL);
        } else {
            CHECK(strstr(fixture.error.message, "Invalid state transition") != NULL);
        }
        Destroy(&fixture);
    }
}

/** @brief Reject a zero worker request before opening transport activity. */
static void TestInvalidExecution(void)
{
    TNgSchedulerFixture fixture;
    Initialize(&fixture, NG_TEST_SERIAL, 2, 0);
    RunScheduled(&fixture);
    CHECK(fixture.result == NG_ERROR);
    CHECK(fixture.starts == 0 && fixture.stops == 0 && fixture.sourceCalls == 0);
    CHECK(fixture.calls[0] == 0 && fixture.calls[1] == 0);
    Destroy(&fixture);
}

/** @brief Run deterministic scheduler contracts without RF hardware or timing ratios.
 * @return Zero after every contract succeeds; failed assertions terminate with a diagnostic.
 */
static void PriorityLoad(void *context, size_t trackingChannels)
{
    TNgSchedulerFixture *fixture = context;
    CHECK(fixture->activeCount == 0);
    fixture->reportedLoad = trackingChannels;
    ++fixture->loadNotifications;
}

static int PriorityProcess(void *context, const TNgChannelId *id, TNgChannelState state,
                           const TNgIqView *window, TNgChannelState *next, TNgError *error)
{
    TNgSchedulerFixture *fixture = context;
    unsigned frame;
    (void)error;
    NgMutexLock(&fixture->mutex);
    frame = fixture->calls[id->instanceId];
    CheckWindow(window, frame);
    CHECK(fixture->reportedLoad == 2);
    CHECK(fixture->loadNotifications == 3 * (frame + 1));
    if (id->instanceId == 0) {
        CHECK(fixture->calls[1] == frame + 1);
        CHECK(fixture->calls[2] == frame + 1);
    }
    ++fixture->calls[id->instanceId];
    *next = state;
    NgMutexUnlock(&fixture->mutex);
    return NG_OK;
}

static void TestTrackingPriority(void)
{
    static const TNgChannelOps ops = {PriorityProcess, 0, PriorityLoad};
    size_t workers, channel;
    for (workers = 1; workers <= 3; workers += 2) {
        TNgSchedulerFixture fixture;
        Initialize(&fixture, NG_TEST_SERIAL, 3, workers);
        fixture.channels[1].state = NG_CHANNEL_TRACKING;
        fixture.channels[2].state = NG_CHANNEL_HOLDOVER;
        for (channel = 0; channel < 3; ++channel) fixture.channels[channel].ops = &ops;
        RunScheduled(&fixture);
        CHECK(fixture.result == NG_OK && fixture.hookCalls == 5);
        for (channel = 0; channel < 3; ++channel) CHECK(fixture.calls[channel] == 5);
        Destroy(&fixture);
    }
}

int main(void)
{
    TestTrackingPriority();
    TestInvalidExecution();
    TestSerialCompatibility();
    TestParallelWindows();
    TestFailureBarrier();
    puts("PASS: serial engine compatibility; bounded parallel shared-window dispatch; "
         "ordered channel states/gaps; deterministic errors and cleanup barriers");
    return 0;
}
