#include "ngene/receiver.h"
#include "ngene/capture.h"
#include "ngene/ring.h"
#include "platform/platform.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);                        \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

/**
 * @brief Verify parse behaviour.
 *
 * @details
 * Test-only helper exercising the named contract with deterministic inputs. Caller owns
 * fixture/context storage; assertion failures abort the test.
 *
 * @param text Read-only test input; retained by the fixture owner.
 * @param c Caller-owned test fixture or output storage used by this helper.
 *
 * @return Test callback result/status, checked by the calling assertion.
 */
static int Parse(const char *text, TNgConfig *c)
{
    TNgError e = {{0}};
    FILE *f = tmpfile();
    int result;
    CHECK(f != NULL);
    CHECK(fwrite(text, 1, strlen(text), f) == strlen(text));
    rewind(f);
    result = NgConfigRead(f, c, &e);
    fclose(f);
    return result;
}

/**
 * @brief Verify config behaviour.
 *
 * @details
 * Test-only helper exercising the named contract with deterministic inputs. Caller owns
 * fixture/context storage; assertion failures abort the test.
 */
static void TestConfig(void)
{
    TNgConfig c;
    CHECK(Parse("[frontend]\ntype=file\nsample_rate_hz=100000\n[buffer]\nwindow_samples=1000\n",
                &c) == NG_OK);
    CHECK(c.sampleRateHz == 100000 && c.windowSamples == 1000);
    CHECK(Parse("[frontend]\nsample_rate_hz=-1\n", &c) == NG_ERROR);
    CHECK(Parse("[frontend]\nsample_rate_hz=10junk\n", &c) == NG_ERROR);
    CHECK(Parse("[frontend]\nsample_rate_hz=16000000\nsample_rate_hz=8000000\n", &c) == NG_ERROR);
    CHECK(Parse("[frontend]\ntype=soapy\n", &c) == NG_ERROR);
    CHECK(Parse("[receiver]\nrun_seconds=nan\n", &c) == NG_ERROR);
    CHECK(Parse("[hackrf]\nlna_gain_db=17\n", &c) == NG_ERROR);
    CHECK(Parse("[buffer]\nwindow_samples=0\n", &c) == NG_ERROR);
    CHECK(Parse("[recording]\nunknown=true\n", &c) == NG_ERROR);
    CHECK(Parse("[receiver]\nmode=record\n", &c) == NG_ERROR);
    CHECK(Parse("[conditioning]\nmodule=dc_iir\ndc_shift=4\n", &c) == NG_OK && c.dcRemoval == 0);
    CHECK(Parse("[frontend]\ntype=hackrf\nserial=\n[replay]\npath=\n"
                "[recording]\ndirectory=\n",
                &c) == NG_OK);
    CHECK(Parse("[frontend]\nsample_rate_hz=\n", &c) == NG_ERROR);
    CHECK(Parse("[replay]\nloop=\n", &c) == NG_ERROR);
    CHECK(Parse("[frontend]\ntype=bladerf\n", &c) == NG_OK);
    CHECK(c.bladeFormat == NG_FORMAT_SC16_Q11);
    CHECK(Parse("[frontend]\ntype=bladerf\n[bladerf]\nformat=CI8\n", &c) == NG_OK);
    CHECK(c.bladeFormat == NG_FORMAT_CI8);
    CHECK(Parse("[bladerf]\nformat=CU8\n", &c) == NG_ERROR);
    CHECK(Parse("[bladerf]\nformat=SC8_Q7\n", &c) == NG_ERROR);
    CHECK(Parse("[bladerf]\nformat=\n", &c) == NG_ERROR);
    CHECK(Parse("[bladerf]\nformat=CI8\nformat=SC16_Q11\n", &c) == NG_ERROR);
    {
        FILE *profile = tmpfile();
        TNgConfig loaded;
        TNgError error = {{0}};
        NgConfigDefaults(&c);
        c.frontend = NG_FRONTEND_HACKRF;
        c.bladeFormat = NG_FORMAT_CI8;
        CHECK(profile != NULL && NgConfigWrite(profile, &c, &error) == NG_OK);
        rewind(profile);
        CHECK(NgConfigRead(profile, &loaded, &error) == NG_OK);
        CHECK(loaded.frontend == NG_FRONTEND_HACKRF && loaded.replayPath[0] == '\0');
        CHECK(loaded.bladeFormat == NG_FORMAT_CI8);
        CHECK(fclose(profile) == 0);
    }
    CHECK(Parse("[buffer]\nwindow_samples=1048576\ningress_blocks=4096\n", &c) == NG_ERROR);
    /* The same queue geometry fits the sample-storage guard only at two bytes per pair. */
    CHECK(Parse("[frontend]\ntype=bladerf\n[bladerf]\nformat=CI8\n"
                "[buffer]\nwindow_samples=65536\ningress_blocks=1024\n",
                &c) == NG_OK);
    CHECK(Parse("[frontend]\ntype=bladerf\n[bladerf]\nformat=SC16_Q11\n"
                "[buffer]\nwindow_samples=65536\ningress_blocks=1024\n",
                &c) == NG_ERROR);
}

/**
 * @brief Verify make view behaviour.
 *
 * @details
 * Test-only helper exercising the named contract with deterministic inputs. Caller owns
 * fixture/context storage; assertion failures abort the test.
 *
 * @param samples Caller-owned test fixture or output storage used by this helper.
 * @param first Test parameter controlling this contract scenario.
 * @param count Test parameter controlling this contract scenario.
 *
 * @return Test callback result/status, checked by the calling assertion.
 */
static TNgIqView MakeView(TNgIq8 *samples, uint64_t first, size_t count)
{
    TNgIqView view;
    size_t i;
    memset(&view, 0, sizeof(view));
    view.samples = samples;
    view.meta.firstSample = first;
    view.meta.count = count;
    for (i = 0; i < count; ++i) {
        samples[i].i = (int8_t)((first + i) % 100);
        samples[i].q = (int8_t)(-samples[i].i);
    }
    return view;
}

/**
 * @brief Verify ring behaviour.
 *
 * @details
 * Test-only helper exercising the named contract with deterministic inputs. Caller owns
 * fixture/context storage; assertion failures abort the test.
 */
static void TestRing(void)
{
    TNgError e = {{0}};
    TNgRing *r;
    TNgIq8 a[4];
    TNgIqView v;
    TNgIqView held;
    CHECK(NgRingCreate(2, 4, &r, &e) == NG_OK);
    CHECK(NgRingAcquire(r, &held, 0) == NG_TIMEOUT);
    v = MakeView(a, 0, 4);
    CHECK(NgRingPush(r, &v) == NG_OK);
    v = MakeView(a, 4, 3);
    CHECK(NgRingPush(r, &v) == NG_OK);
    CHECK(NgRingAcquire(r, &held, 0) == NG_OK);
    CHECK(held.meta.firstSample == 0 && held.samples[3].i == 3);
    v = MakeView(a, 7, 4);
    CHECK(NgRingPush(r, &v) == NG_FULL);
    CHECK(held.samples[0].i == 0);
    CHECK(NgRingAcquire(r, &v, 0) == NG_ERROR);
    NgRingRelease(r);
    CHECK(NgRingPush(r, &v) == NG_OK);
    CHECK(NgRingAcquire(r, &held, 0) == NG_OK);
    CHECK(held.meta.firstSample == 4 && held.meta.count == 3);
    NgRingRelease(r);
    NgRingClose(r);
    CHECK(NgRingPush(r, &v) == NG_ERROR);
    CHECK(NgRingAcquire(r, &held, 0) == NG_OK);
    CHECK(held.meta.firstSample == 7 && held.samples[0].i == 7);
    NgRingRelease(r);
    CHECK(NgRingAcquire(r, &held, 0) == NG_END);
    CHECK(NgRingHighWater(r) == 2);
    NgRingDestroy(r);
}

/**
 * @brief Verify producer behaviour.
 *
 * @details
 * Test-only helper exercising the named contract with deterministic inputs. Caller owns
 * fixture/context storage; assertion failures abort the test.
 *
 * @param context Borrowed test fixture passed through the callback interface.
 */
static void Producer(void *context)
{
    TNgRing *r = context;
    uint64_t index;
    TNgIq8 a[7];
    for (index = 0; index < 35000; index += 7) {
        TNgIqView view = MakeView(a, index, 7);
        int result;
        while ((result = NgRingPush(r, &view)) == NG_FULL) {
            NgSleepMs(1);
        }
        CHECK(result == NG_OK);
    }
    NgRingClose(r);
}

/**
 * @brief Verify concurrent ring behaviour.
 *
 * @details
 * Test-only helper exercising the named contract with deterministic inputs. Caller owns
 * fixture/context storage; assertion failures abort the test.
 */
static void TestConcurrentRing(void)
{
    TNgRing *r;
    TNgThread thread;
    TNgError e = {{0}};
    uint64_t expected = 0;
    CHECK(NgRingCreate(3, 7, &r, &e) == NG_OK);
    CHECK(NgThreadStart(&thread, Producer, r) == 0);
    for (;;) {
        TNgIqView view;
        size_t k;
        int status = NgRingAcquire(r, &view, 1000);
        if (status == NG_END) {
            break;
        }
        CHECK(status == NG_OK);
        CHECK(view.meta.firstSample == expected);
        for (k = 0; k < view.meta.count; ++k) {
            CHECK(view.samples[k].i == (int8_t)((expected + k) % 100));
            CHECK(view.samples[k].q == -view.samples[k].i);
        }
        expected += view.meta.count;
        NgRingRelease(r);
    }
    NgThreadJoin(thread);
    CHECK(expected == 35000);
    NgRingDestroy(r);
}

typedef struct {
    size_t call;
    TNgIq8 samples[4];
    int stopped;
} TNgMockSource;

/**
 * @brief Verify mock start behaviour.
 *
 * @details
 * Test-only helper exercising the named contract with deterministic inputs. Caller owns
 * fixture/context storage; assertion failures abort the test.
 *
 * @param context Borrowed test fixture passed through the callback interface.
 * @param error Optional diagnostic output used for test failures.
 *
 * @return Test callback result/status, checked by the calling assertion.
 */
static int MockStart(void *context, TNgError *error)
{
    (void)error;
    ((TNgMockSource *)context)->call = 0;
    return NG_OK;
}

/**
 * @brief Verify mock acquire behaviour.
 *
 * @details
 * Test-only helper exercising the named contract with deterministic inputs. Caller owns
 * fixture/context storage; assertion failures abort the test.
 *
 * @param context Borrowed test fixture passed through the callback interface.
 * @param view Caller-owned test fixture or output storage used by this helper.
 * @param timeoutMs Test parameter controlling this contract scenario.
 * @param error Optional diagnostic output used for test failures.
 *
 * @return Test callback result/status, checked by the calling assertion.
 */
static int MockAcquire(void *context, TNgIqView *view, unsigned timeoutMs, TNgError *error)
{
    static const uint64_t positions[] = {0, 3, 7, 20, 24};
    static const size_t counts[] = {3, 4, 3, 4, 3};
    TNgMockSource *m = context;
    (void)timeoutMs;
    (void)error;
    if (m->call == 5) {
        return NG_END;
    }
    *view = MakeView(m->samples, positions[m->call], counts[m->call]);
    ++m->call;
    return NG_OK;
}

/**
 * @brief Verify mock release behaviour.
 *
 * @details
 * Test-only helper exercising the named contract with deterministic inputs. Caller owns
 * fixture/context storage; assertion failures abort the test.
 *
 * @param context Borrowed test fixture passed through the callback interface.
 */
static void MockRelease(void *context)
{
    (void)context;
}

/**
 * @brief Verify mock stop behaviour.
 *
 * @details
 * Test-only helper exercising the named contract with deterministic inputs. Caller owns
 * fixture/context storage; assertion failures abort the test.
 *
 * @param context Borrowed test fixture passed through the callback interface.
 * @param error Optional diagnostic output used for test failures.
 *
 * @return Test callback result/status, checked by the calling assertion.
 */
static int MockStop(void *context, TNgError *error)
{
    (void)error;
    ((TNgMockSource *)context)->stopped = 1;
    return NG_OK;
}

/**
 * @brief Verify mock stats behaviour.
 *
 * @details
 * Test-only helper exercising the named contract with deterministic inputs. Caller owns
 * fixture/context storage; assertion failures abort the test.
 *
 * @param context Borrowed test fixture passed through the callback interface.
 * @param s Caller-owned test fixture or output storage used by this helper.
 */
static void MockStats(void *context, TNgFrontendStats *s)
{
    (void)context;
    memset(s, 0, sizeof(*s));
    s->receivedSamples = 17;
}

/**
 * @brief Verify mock destroy behaviour.
 *
 * @details
 * Test-only helper exercising the named contract with deterministic inputs. Caller owns
 * fixture/context storage; assertion failures abort the test.
 *
 * @param context Borrowed test fixture passed through the callback interface.
 */
static void MockDestroy(void *context)
{
    (void)context;
}

static const TNgFrontendOps mockOps = {MockStart, MockAcquire, MockRelease,
                                       MockStop,  MockStats,   MockDestroy};

/**
 * @brief Verify stop after three reads behaviour.
 *
 * @details
 * Test-only helper exercising the named contract with deterministic inputs. Caller owns
 * fixture/context storage; assertion failures abort the test.
 *
 * @param context Borrowed test fixture passed through the callback interface.
 *
 * @return Test callback result/status, checked by the calling assertion.
 */
static int StopAfterThreeReads(void *context)
{
    return ((TNgMockSource *)context)->call >= 3;
}

typedef struct {
    const TNgIq8 *firstPointer;
    unsigned calls[2];
} TNgObservers;

/**
 * @brief Verify observe behaviour.
 *
 * @details
 * Test-only helper exercising the named contract with deterministic inputs. Caller owns
 * fixture/context storage; assertion failures abort the test.
 *
 * @param context Borrowed test fixture passed through the callback interface.
 * @param id Read-only test input; retained by the fixture owner.
 * @param state Test parameter controlling this contract scenario.
 * @param view Read-only test input; retained by the fixture owner.
 * @param next Caller-owned test fixture or output storage used by this helper.
 * @param error Optional diagnostic output used for test failures.
 *
 * @return Test callback result/status, checked by the calling assertion.
 */
static int Observe(void *context, const TNgChannelId *id, TNgChannelState state,
                   const TNgIqView *view, TNgChannelState *next, TNgError *error)
{
    static const uint64_t first[] = {0, 4, 8, 20, 24};
    static const size_t counts[] = {4, 4, 2, 4, 3};
    TNgObservers *o = context;
    unsigned channel = id->instanceId;
    unsigned frame = o->calls[channel]++;
    size_t k;
    (void)error;
    CHECK(frame < 5);
    CHECK(view->meta.firstSample == first[frame]);
    CHECK(view->meta.count == counts[frame]);
    /* Tracking may run before acquisition. Whichever channel enters first records
     * the lease; the second must see the identical pointer for this window. */
    if (o->calls[1u - channel] == frame) {
        o->firstPointer = view->samples;
    } else {
        CHECK(o->calls[1u - channel] == frame + 1);
        CHECK(view->samples == o->firstPointer);
    }
    CHECK(((view->meta.flags & NG_BLOCK_PARTIAL) != 0) == (frame == 2 || frame == 4));
    CHECK(((view->meta.flags & NG_BLOCK_GAP) != 0) == (frame == 3));
    if (frame == 3) {
        CHECK(view->meta.missingKnown && view->meta.missingBefore == 10);
    }
    for (k = 0; k < view->meta.count; ++k) {
        CHECK(view->samples[k].i == (int8_t)((first[frame] + k) % 100));
    }
    *next = channel == 0 ? (TNgChannelState)(((int)state + 1) % 4) : state;
    return NG_OK;
}

/**
 * @brief Verify engine behaviour.
 *
 * @details
 * Test-only helper exercising the named contract with deterministic inputs. Caller owns
 * fixture/context storage; assertion failures abort the test.
 */
static void TestEngine(void)
{
    TNgConfig c;
    TNgReceiverStats stats;
    TNgError e = {{0}};
    TNgMockSource mock = {0};
    TNgObservers observed = {0};
    TNgFrontend source = {
        .ops = &mockOps,
        .context = &mock,
        .info = {.sampleRateHz = 100000, .centerFrequencyHz = 1575420000,
                 .format = NG_FORMAT_CI8, .sourceName = "mock", .clockDomain = "test",
                 .signalFrequencyHz = 1575420000}};
    TNgConditioner conditioner = NgConditionerBypass();
    const TNgChannelOps ops = {Observe, 0, 0};
    TNgChannel channels[2] = {{{0, 0, 1, 0}, NG_CHANNEL_ACQUISITION, &ops, &observed},
                              {{1, 0, 2, 0}, NG_CHANNEL_TRACKING, &ops, &observed}};
    NgConfigDefaults(&c);
    c.sampleRateHz = 100000;
    c.windowSamples = 4;
    c.runSeconds = 0;
    CHECK(NgReceiverRun(&c, &source, &conditioner, channels, 2, NULL, NULL, &stats, &e) == NG_OK);
    CHECK(mock.stopped);
    CHECK(stats.processedSamples == 17 && stats.windows == 5 && stats.gapEvents == 1);
    CHECK(observed.calls[0] == 5 && observed.calls[1] == 5);
    CHECK(!NgChannelTransitionValid(NG_CHANNEL_TRACKING, NG_CHANNEL_ACQUISITION));
    CHECK(NgChannelTransitionValid(NG_CHANNEL_HOLDOVER, NG_CHANNEL_TRACKING));
    memset(&observed, 0, sizeof(observed));
    mock.stopped = 0;
    channels[0].state = NG_CHANNEL_ACQUISITION;
    CHECK(NgReceiverRun(&c, &source, &conditioner, channels, 2, StopAfterThreeReads, &mock, &stats,
                        &e) == NG_OK);
    CHECK(mock.stopped && stats.processedSamples == 10 && stats.windows == 3);
}

/**
 * @brief Verify recording behaviour.
 *
 * @details
 * Test-only helper exercising the named contract with deterministic inputs. Caller owns
 * fixture/context storage; assertion failures abort the test.
 */
static void TestRecording(void)
{
    TNgConfig c;
    TNgRecorder *r;
    TNgError e = {{0}};
    TNgRecordingStats stats;
    TNgIq8 data[5];
    TNgIq8 expected[13];
    TNgIq8 actual[13];
    TNgStreamInfo info = {.sampleRateHz = 100000, .centerFrequencyHz = 1575420000,
                         .format = NG_FORMAT_CI8, .sourceName = "test", .clockDomain = "relative",
                         .signalFrequencyHz = 1575420000};
    char path[640];
    char metadata[4096];
    FILE *f;
    size_t done = 0;
    size_t count;
    size_t index;
    TNgFrontendStats sourceStats = {0};
    sourceStats.receivedSamples = 13;
    NgConfigDefaults(&c);
    c.windowSamples = 5;
    c.sampleRateHz = 100000;
    c.segmentSamples = 7;
    c.recordingEnabled = 1;
    c.recordingQueueBlocks = 16;
    snprintf(c.recordingPrefix, sizeof(c.recordingPrefix), "ngene_test_%" PRIu64,
             (uint64_t)(NgMonotonicSeconds() * 1000000));
    CHECK(NgRecorderOpen(&c, &info, "bypass", &r, &e) == NG_OK);
    for (index = 0; index < 3; ++index) {
        TNgIqView view = MakeView(data, 100 + done, index == 2 ? 3 : 5);
        memcpy(expected + done, data, view.meta.count * 2);
        done += view.meta.count;
        CHECK(NgRecorderSubmit(r, &view) == NG_OK);
    }
    CHECK(NgRecorderFinish(r, 1, &sourceStats, &stats, &e) == NG_OK);
    CHECK(stats.samples == 13 && stats.segments == 2);
    done = 0;
    for (index = 0; index < 2; ++index) {
        snprintf(path, sizeof(path), "%s_%06zu.cs8", c.recordingPrefix, index);
        f = fopen(path, "rb");
        CHECK(f != NULL);
        count = fread(actual + done, sizeof(TNgIq8), 13 - done, f);
        CHECK(count == (index == 0 ? (size_t)7 : (size_t)6));
        done += count;
        CHECK(!ferror(f));
        fclose(f);
        {
            TNgCaptureMetadata found;
            TNgConfig replay;
            char sidecar[672];
            NgConfigDefaults(&replay);
            replay.dcRemoval = 1;
            CHECK(NgReplayConfigure(path, &replay, &found, &e) == NG_OK);
            CHECK(found.available && found.version == 2 && found.complete);
            CHECK(found.samples == (index == 0 ? 7u : 6u));
            CHECK(replay.frontend == NG_FRONTEND_FILE && replay.sampleRateHz == info.sampleRateHz);
            CHECK(replay.centerFrequencyHz == info.centerFrequencyHz && replay.dcRemoval == 0);
            CHECK(replay.windowSamples == 100 && replay.runSeconds == 0);
            CHECK(replay.recordingEnabled == 0 && replay.replayStartSample == 0);
            snprintf(sidecar, sizeof(sidecar), "%s.meta.ini", path);
            CHECK(remove(sidecar) == 0);
            /* Existing session metadata remains readable after removing the new sidecar. */
            CHECK(NgReplayInspect(path, &found, &e) == NG_OK && found.version == 1);
        }
        CHECK(remove(path) == 0);
    }
    CHECK(memcmp(actual, expected, sizeof(actual)) == 0);
    snprintf(path, sizeof(path), "%s.meta.ini", c.recordingPrefix);
    f = fopen(path, "rb");
    CHECK(f != NULL);
    count = fread(metadata, 1, sizeof(metadata) - 1, f);
    metadata[count] = 0;
    fclose(f);
    CHECK(strstr(metadata, "complete=true") != NULL &&
          strstr(metadata, "written_samples=13") != NULL);
    CHECK(NgRecorderOpen(&c, &info, "bypass", &r, &e) == NG_ERROR); /* No overwrite. */
    CHECK(remove(path) == 0);
    snprintf(path, sizeof(path), "%s.index.csv", c.recordingPrefix);
    f = fopen(path, "rb");
    CHECK(f != NULL);
    count = 0;
    while (fgets(metadata, sizeof(metadata), f) != NULL) {
        ++count;
    }
    fclose(f);
    CHECK(count == 5);
    CHECK(remove(path) == 0); /* header + four segment pieces */
    strcpy(c.recordingPrefix, "nonexistent_ngene_directory/capture");
    CHECK(NgRecorderOpen(&c, &info, "bypass", &r, &e) == NG_ERROR);
}

/**
 * @brief Verify halve behaviour.
 *
 * @details
 * Test-only helper exercising the named contract with deterministic inputs. Caller owns
 * fixture/context storage; assertion failures abort the test.
 *
 * @param context Borrowed test fixture passed through the callback interface.
 * @param input Read-only test input; retained by the fixture owner.
 * @param output Caller-owned test fixture or output storage used by this helper.
 * @param error Optional diagnostic output used for test failures.
 *
 * @return Test callback result/status, checked by the calling assertion.
 */
static int Halve(void *context, const TNgIqView *input, TNgIqView *output, TNgError *error)
{
    TNgIq8 *scratch = context;
    size_t k;
    (void)error;
    for (k = 0; k < input->meta.count; ++k) {
        scratch[k].i = (int8_t)(input->samples[k].i / 2);
        scratch[k].q = (int8_t)(input->samples[k].q / 2);
    }
    *output = *input;
    output->samples = scratch;
    return NG_OK;
}

/**
 * @brief Verify recording taps behaviour.
 *
 * @details
 * Test-only helper exercising the named contract with deterministic inputs. Caller owns
 * fixture/context storage; assertion failures abort the test.
 */
static void TestRecordingTaps(void)
{
    int pre;
    TNgError error = {{0}};
    TNgConfig c;
    TNgMockSource mock = {0};
    TNgReceiverStats stats;
    TNgFrontend source = {
        .ops = &mockOps,
        .context = &mock,
        .info = {.sampleRateHz = 100000, .centerFrequencyHz = 1575420000,
                 .format = NG_FORMAT_CI8, .sourceName = "mock", .clockDomain = "test",
                 .signalFrequencyHz = 1575420000}};
    TNgIq8 scratch[4];
    TNgIq8 samples[17];
    TNgConditioner conditioner = {scratch, "test-half", Halve};
    for (pre = 0; pre <= 1; ++pre) {
        char path[640];
        char meta[2048];
        size_t count;
        size_t k;
        FILE *f;
        NgConfigDefaults(&c);
        c.sampleRateHz = 100000;
        c.windowSamples = 4;
        c.runSeconds = 0;
        c.recordingEnabled = 1;
        c.recordingPreConditioning = pre;
        snprintf(c.recordingPrefix, sizeof(c.recordingPrefix), "tap_%d_%" PRIu64, pre,
                 (uint64_t)(NgMonotonicSeconds() * 1000000));
        CHECK(NgReceiverRun(&c, &source, &conditioner, NULL, 0, NULL, NULL, &stats, &error) ==
              NG_OK);
        snprintf(path, sizeof(path), "%s_000000.cs8", c.recordingPrefix);
        f = fopen(path, "rb");
        CHECK(f != NULL);
        count = fread(samples, sizeof(TNgIq8), 17, f);
        CHECK(count == 17);
        fclose(f);
        CHECK(remove(path) == 0);
        for (k = 0; k < 17; ++k) {
            int value = (int)(k < 10 ? k : k + 10);
            if (pre == 0) {
                value /= 2;
            }
            CHECK(samples[k].i == value && samples[k].q == -value);
        }
        snprintf(path, sizeof(path), "%s_000000.cs8.meta.ini", c.recordingPrefix);
        CHECK(remove(path) == 0);
        snprintf(path, sizeof(path), "%s.meta.ini", c.recordingPrefix);
        f = fopen(path, "rb");
        CHECK(f != NULL);
        count = fread(meta, 1, sizeof(meta) - 1, f);
        meta[count] = 0;
        fclose(f);
        CHECK(strstr(meta, pre ? "recording_point=pre_conditioning"
                               : "recording_point=post_conditioning") != NULL);
        CHECK(remove(path) == 0);
        snprintf(path, sizeof(path), "%s.index.csv", c.recordingPrefix);
        CHECK(remove(path) == 0);
    }
}

/**
 * @brief Run the C contract test executable.
 *
 * @details
 * Assertions abort with a nonzero process status on contract failures. This executable is
 * development tooling, not receiver runtime.
 *
 * @return Zero if every contract assertion passes.
 */
int main(void)
{
    TestConfig();
    TestRing();
    TestConcurrentRing();
    TestEngine();
    TestRecording();
    TestRecordingTaps();
    puts("PASS: config, ring lease/wrap/full/EOF, 5000 concurrent blocks, synchronized "
         "channels/gap, segmented byte-exact recording/no-overwrite");
    return 0;
}
