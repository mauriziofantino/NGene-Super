/** @file test_sdr.c @brief Native-format ingress ownership, loss accounting and pipeline
 * regressions. */
#include "frontend/ingress.h"
#include "ngene/receiver.h"
#include "ngene/capture.h"
#include "ngene/analysis.h"
#include "ngene/calibration.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>

/** @brief Abort this CTest fixture on a violated contract. */
#define CHECK(expression)                                                                          \
    do {                                                                                           \
        if (!(expression)) {                                                                       \
            fprintf(stderr, "FAIL %d: %s\n", __LINE__, #expression);                               \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

/** @brief Populate extreme native levels without narrowing SC16 values.
 * @param storage Writable array with count native pairs.
 * @param format Native representation.
 * @param first First monotonic sample index.
 * @param count Number of pairs.
 */
static void Fill(void *storage, TNgSampleFormat format, uint64_t first, size_t count)
{
    size_t index;
    int limit = format == NG_FORMAT_SC16_Q11 ? 2048 : 128;
    for (index = 0; index < count; ++index) {
        int value = (int)((first + index) % (unsigned)(2 * limit)) - limit;
        NgIqStore(storage, format, index, 0, value);
        NgIqStore(storage, format, index, 1, -1 - value);
    }
}

/** @brief Verify overflow/drop-stop semantics, leased-block protection and exact counters.
 * @param format Native representation tested with the same production ring implementation.
 */
static void TestIngress(TNgSampleFormat format)
{
    TNgConfig config;
    TNgIngress ingress;
    TNgError error = {{0}};
    TNgFrontendStats stats;
    unsigned char bytes[44];
    TNgIqView input = {0}, output;
    NgConfigDefaults(&config);
    config.windowSamples = 4;
    config.ingressBlocks = 2;
    config.sampleRateHz = 4000;
    config.overflowPolicy = NG_OVERFLOW_DROP_NEWEST;
    input.samples = (const TNgIq8 *)(const void *)bytes;
    input.format = format;
    input.meta.count = 11;
    Fill(bytes, format, 0, 11);
    CHECK(NgIngressInit(&ingress, &config, format, &error) == NG_OK);
    CHECK(NgIngressSubmit(&ingress, &input) == NG_OK);
    NgIngressSnapshot(&ingress, 0, &stats);
    CHECK(stats.receivedSamples == 11 && stats.droppedSamples == 3);
    CHECK(stats.ingress.acceptedSamples == 8 && stats.ingress.pendingSamples == 8);
    CHECK(stats.ingress.highWaterBlocks == 2 && stats.ingress.usedBlocks == 2);
    CHECK(stats.ingress.overflowEvents == 1 && stats.ingress.occupancyPercent == 100);
    CHECK(stats.ingress.availableMs == 0 && stats.transportErrors == 0);
    CHECK(stats.hardwareLossState == NG_DIAGNOSTIC_UNSUPPORTED);
    CHECK(NgRingAcquire(ingress.ring, &output, 0) == NG_OK);
    CHECK(NgIqComponent(&output, 0, 0) == (format == NG_FORMAT_SC16_Q11 ? -2048 : -128));
    memset(bytes, 0, sizeof(bytes));
    input.meta.firstSample = 11;
    input.meta.count = 2;
    CHECK(NgIngressSubmit(&ingress, &input) == NG_OK);
    CHECK(NgIqComponent(&output, 0, 0) == (format == NG_FORMAT_SC16_Q11 ? -2048 : -128));
    NgRingRelease(ingress.ring);
    NgIngressSnapshot(&ingress, 0, &stats);
    CHECK(stats.ingress.consumedSamples == 4 && stats.ingress.pendingSamples == 4);
    CHECK(fabs(stats.ingress.availableMs - 1.0) < 1e-12);
    Fill(bytes, format, 13, 3);
    input.meta.firstSample = 13;
    input.meta.count = 3;
    CHECK(NgIngressSubmit(&ingress, &input) == NG_OK);
    CHECK(NgRingAcquire(ingress.ring, &output, 0) == NG_OK && output.meta.firstSample == 4);
    NgRingRelease(ingress.ring);
    CHECK(NgRingAcquire(ingress.ring, &output, 0) == NG_OK && output.meta.firstSample == 13);
    CHECK((output.meta.flags & NG_BLOCK_GAP) != 0 && output.meta.missingKnown &&
          output.meta.missingBefore == 5);
    NgRingRelease(ingress.ring);
    NgRingClose(ingress.ring);
    NgIngressSnapshot(&ingress, 1, &stats);
    CHECK(stats.receivedSamples == 16 && stats.ingress.acceptedSamples == 11 &&
          stats.droppedSamples == 5);
    CHECK(stats.ingress.acceptedSamples == stats.ingress.consumedSamples &&
          stats.queuedSamples == 0);
    CHECK(NgRingHighWater(ingress.ring) == 2);
    NgIngressDestroy(&ingress);
    config.overflowPolicy = NG_OVERFLOW_STOP;
    CHECK(NgIngressInit(&ingress, &config, format, &error) == NG_OK);
    NgIngressSnapshot(&ingress, 0, &stats);
    CHECK(stats.ingress.acceptedSamples == 0 && stats.ingress.highWaterBlocks == 0);
    input.meta.firstSample = 0;
    input.meta.count = 11;
    CHECK(NgIngressSubmit(&ingress, &input) == NG_FULL);
    CHECK(NgAtomicGet(&ingress.failure) == 2);
    NgIngressSnapshot(&ingress, 1, &stats);
    CHECK(stats.droppedSamples == 3 && stats.ingress.acceptedSamples == 8 &&
          stats.transportErrors == 0);
    CHECK(stats.discardedOnStop == 8);
    NgIngressDestroy(&ingress);
}

/** @brief Preserve an unknown vendor-gap duration even if its first block overflows.
 * @details Known software losses must never turn an unknown device gap into a falsely
 * exact loss estimate. This uses the same production ingress and counter architecture.
 */
static void TestUnknownDeviceGap(void)
{
    TNgConfig config;
    TNgIngress ingress;
    TNgError error = {{0}};
    TNgIq8 samples[4] = {{0}};
    TNgIqView transfer = {samples, {0}, NG_FORMAT_CI8}, output;
    NgConfigDefaults(&config);
    config.windowSamples = 4;
    config.ingressBlocks = 2;
    config.overflowPolicy = NG_OVERFLOW_DROP_NEWEST;
    CHECK(NgIngressInit(&ingress, &config, NG_FORMAT_CI8, &error) == NG_OK);
    transfer.meta.count = 4;
    CHECK(NgIngressSubmit(&ingress, &transfer) == NG_OK);
    transfer.meta.firstSample = 4;
    CHECK(NgIngressSubmit(&ingress, &transfer) == NG_OK);
    transfer.meta.firstSample = 8;
    transfer.meta.flags = NG_BLOCK_GAP;
    CHECK(NgIngressSubmit(&ingress, &transfer) ==
          NG_OK); /* Unknown gap's first frame is dropped. */
    CHECK(NgRingAcquire(ingress.ring, &output, 0) == NG_OK);
    NgRingRelease(ingress.ring);
    transfer.meta.firstSample = 12;
    transfer.meta.flags = 0;
    CHECK(NgIngressSubmit(&ingress, &transfer) == NG_OK);
    CHECK(NgRingAcquire(ingress.ring, &output, 0) == NG_OK);
    NgRingRelease(ingress.ring);
    CHECK(NgRingAcquire(ingress.ring, &output, 0) == NG_OK);
    CHECK(output.meta.firstSample == 12 && (output.meta.flags & NG_BLOCK_GAP) != 0 &&
          output.meta.missingKnown == 0);
    NgRingRelease(ingress.ring);
    NgIngressDestroy(&ingress);
}

/** @brief Concurrent SPSC fixture, using production native-format storage. */
typedef struct {
    /** Existing ring, shared by one producer and one consumer. */
    TNgRing *ring;
    /** Fixed representation for this run. */
    TNgSampleFormat format;
    /** Reader-observed monotonic pair count. */
    uint64_t consumed;
} TNgStress;

/** @brief Produce deterministic varying-size blocks, retrying full test queues without dropping.
 * @param context Live stress fixture retained until join.
 */
static void Produce(void *context)
{
    TNgStress *test = context;
    unsigned char storage[256];
    uint64_t first = 0;
    size_t block;
    for (block = 0; block < 20000; ++block) {
        TNgIqView input = {0};
        input.samples = (const TNgIq8 *)(const void *)storage;
        input.format = test->format;
        input.meta.firstSample = first;
        input.meta.count = block % 64 + 1;
        Fill(storage, test->format, first, input.meta.count);
        while (NgRingPush(test->ring, &input) == NG_FULL) {
            NgSleepMs(1);
        }
        first += input.meta.count;
    }
    NgRingClose(test->ring);
}

/** @brief Verify byte integrity and strict sequence across 20,000 concurrent producer blocks.
 * @param format Native representation.
 */
static void TestConcurrent(TNgSampleFormat format)
{
    TNgStress test = {0};
    TNgThread thread;
    TNgError error = {{0}};
    TNgIqView view;
    TNgRingStats stats;
    int status;
    size_t blocks = 0;
    int limit = format == NG_FORMAT_SC16_Q11 ? 2048 : 128;
    test.format = format;
    CHECK(NgRingCreateFormat(64, 64, format, &test.ring, &error) == NG_OK);
    CHECK(NgThreadStart(&thread, Produce, &test) == 0);
    for (;;) {
        size_t index;
        status = NgRingAcquire(test.ring, &view, 1000);
        if (status == NG_END) {
            break;
        }
        CHECK(status == NG_OK && view.meta.firstSample == test.consumed);
        for (index = 0; index < view.meta.count; ++index) {
            int expected = (int)((view.meta.firstSample + index) % (unsigned)(2 * limit)) - limit;
            CHECK(NgIqComponent(&view, index, 0) == expected &&
                  NgIqComponent(&view, index, 1) == -1 - expected);
        }
        NgRingSnapshot(test.ring, 16000000, &stats);
        CHECK(stats.acceptedSamples - stats.consumedSamples == stats.pendingSamples &&
              stats.usedBlocks <= 64);
        test.consumed += view.meta.count;
        ++blocks;
        NgRingRelease(test.ring);
    }
    NgThreadJoin(thread);
    NgRingSnapshot(test.ring, 16000000, &stats);
    CHECK(blocks == 20000 && stats.pendingSamples == 0 && stats.acceptedSamples == test.consumed);
    NgRingDestroy(test.ring);
}

/** @brief Exercise native recording, per-file metadata and exact replay through the existing
 * engine.
 * @param format Native IQ representation; SC16 extreme values must survive byte-for-byte.
 */
static void TestPipeline(TNgSampleFormat format)
{
    TNgConfig config, replay;
    TNgFrontend source = {0};
    TNgConditioner bypass = NgConditionerBypass();
    TNgReceiverStats stats;
    TNgError error = {{0}};
    TNgCaptureMetadata metadata;
    unsigned char expected[44], actual[44];
    char path[640], sidecar[672], inputPath[80];
    FILE *file;
    TNgIqView view;
    const char *extension = format == NG_FORMAT_SC16_Q11 ? "sc16q11"
                            : format == NG_FORMAT_CU8    ? "cu8"
                                                         : "cs8";
    snprintf(inputPath, sizeof(inputPath), "ngene_native_input_%s.%s", NgSampleFormatName(format),
             extension);
    Fill(expected, format, 0, 11);
    file = NgFopenExclusive(inputPath);
    CHECK(file != NULL);
    CHECK(fwrite(expected, NgSampleBytes(format), 11, file) == 11 && fclose(file) == 0);
    NgConfigDefaults(&config);
    config.frontend = NG_FRONTEND_FILE;
    config.replayFormat = format;
    snprintf(config.replayPath, sizeof(config.replayPath), "%s", inputPath);
    snprintf(config.recordingPrefix, sizeof(config.recordingPrefix), "ngene_native_%s",
             NgSampleFormatName(format));
    config.replayRealtime = 0;
    config.runSeconds = 0;
    config.windowSamples = 4;
    config.recordingEnabled = 1;
    config.recordingPreConditioning = 1;
    CHECK(NgFrontendOpenNative(&config, &source, &error) == NG_OK);
    CHECK(NgReceiverRun(&config, &source, &bypass, NULL, 0, NULL, NULL, &stats, &error) == NG_OK);
    NgFrontendDestroy(&source);
    CHECK(stats.processedSamples == 11 && stats.recording.samples == 11 && stats.gapEvents == 0);
    snprintf(path, sizeof(path), "%s_000000.%s", config.recordingPrefix, extension);
    file = fopen(path, "rb");
    CHECK(file != NULL);
    CHECK(fread(actual, NgSampleBytes(format), 11, file) == 11 && fclose(file) == 0);
    CHECK(memcmp(expected, actual, 11 * NgSampleBytes(format)) == 0);
    NgConfigDefaults(&replay);
    CHECK(NgReplayConfigure(path, &replay, &metadata, &error) == NG_OK);
    CHECK(metadata.available && metadata.complete && metadata.samples == 11 &&
          replay.replayFormat == format);
    replay.replayRealtime = 0;
    CHECK(NgFrontendOpenNative(&replay, &source, &error) == NG_OK);
    CHECK(source.ops->start(source.context, &error) == NG_OK);
    CHECK(source.ops->acquire(source.context, &view, 0, &error) == NG_OK && view.meta.count == 11);
    CHECK(memcmp(view.samples, expected, 11 * NgSampleBytes(format)) == 0);
    source.ops->release(source.context);
    CHECK(source.ops->stop(source.context, &error) == NG_OK);
    NgFrontendDestroy(&source);
    if (format == NG_FORMAT_SC16_Q11) {
        const char *unnamed = "ngene_explicit_native_format.bin";
        file = NgFopenExclusive(unnamed);
        CHECK(file != NULL);
        CHECK(fwrite(expected, 4u, 11, file) == 11 && fclose(file) == 0);
        NgConfigDefaults(&replay);
        replay.replayFormat = NG_FORMAT_SC16_Q11;
        CHECK(NgReplayConfigure(unnamed, &replay, &metadata, &error) == NG_OK);
        CHECK(!metadata.available && metadata.samples == 11 &&
              replay.replayFormat == NG_FORMAT_SC16_Q11);
        CHECK(remove(unnamed) == 0);
    }
    snprintf(sidecar, sizeof(sidecar), "%s.meta.ini", path);
    CHECK(remove(sidecar) == 0);
    CHECK(remove(path) == 0 && remove(inputPath) == 0);
    snprintf(path, sizeof(path), "%s.meta.ini", config.recordingPrefix);
    CHECK(remove(path) == 0);
    snprintf(path, sizeof(path), "%s.index.csv", config.recordingPrefix);
    CHECK(remove(path) == 0);
}

/** @brief Verify full-resolution ADC moments, signed CU8 centering and calibrated integer PSD.
 * @param format Native representation.
 */
static void TestDiagnostics(TNgSampleFormat format)
{
    unsigned char storage[4096];
    TNgIqView view = {0};
    TNgAnalysisConfig config;
    TNgAnalysis *analysis;
    TNgAnalysisSnapshot snapshot;
    TNgError error = {{0}};
    size_t index;
    int value = format == NG_FORMAT_SC16_Q11 ? 1235 : 32;
    NgAnalysisDefaults(&config);
    config.fftSize = 256;
    config.averages = 1;
    config.histogramPairs = 1024;
    config.histogramBins = 256;
    config.window = NG_WINDOW_RECT;
    CHECK(NgAnalysisCreate(16000000, &config, &analysis, &error) == NG_OK);
    NgAnalysisInterest(analysis, 3, NgMonotonicSeconds() + 2);
    view.samples = (const TNgIq8 *)(const void *)storage;
    view.format = format;
    view.meta.count = 1024;
    for (index = 0; index < 1024; ++index) {
        NgIqStore(storage, format, index, 0, value);
        NgIqStore(storage, format, index, 1, 0);
    }
    CHECK(NgAnalysisSubmitAt(analysis, &view, 0) == NG_OK);
    for (index = 0; index < 500; ++index) {
        NgAnalysisRead(analysis, &snapshot);
        if (snapshot.histogramSequence != 0 && snapshot.psdSequence != 0) {
            break;
        }
        NgSleepMs(1);
    }
    CHECK(snapshot.histogramSequence != 0 && snapshot.psdSequence != 0);
    CHECK(snapshot.statsI.mean == 0 && snapshot.statsI.rms == 0 &&
          snapshot.statsQ.zeros == 1024);
    CHECK(snapshot.removedDcI == value && snapshot.removedDcQ == 0);
    CHECK(NgIqComponent(&view, 0, 0) == value); /* Native input is never rewritten. */
    CHECK(snapshot.histogramMin == (format == NG_FORMAT_SC16_Q11 ? -2048 : -128));
    CHECK(snapshot.peakDbfs < -150); /* A constant input is DC, not an RF tone. */
    NgAnalysisDestroy(analysis);
}

/** @brief Verify native DC estimation stays continuous across IQ block boundaries.
 * @param format Native representation; SC16 Q11 keeps the full 12-bit ADC domain.
 */
/** @brief Measure native ring copy/lease/release cost without RF, FFT, disk or channels.
 * @details This is a microbenchmark, not a full GNSS real-time acceptance result.
 */
static void BenchmarkRing(void)
{
    TNgSampleFormat format;
    for (format = NG_FORMAT_CI8; format <= NG_FORMAT_SC16_Q11;
         format = (TNgSampleFormat)(format + 1)) {
        TNgRing *ring;
        TNgError error = {{0}};
        TNgIqView input = {0}, output;
        unsigned char *storage = malloc(16000u * NgSampleBytes(format));
        size_t iteration;
        double begin, elapsed;
        CHECK(storage != NULL);
        Fill(storage, format, 0, 16000);
        CHECK(NgRingCreateFormat(128, 16000, format, &ring, &error) == NG_OK);
        input.samples = (const TNgIq8 *)(const void *)storage;
        input.format = format;
        input.meta.count = 16000;
        begin = NgMonotonicSeconds();
        for (iteration = 0; iteration < 20000; ++iteration) {
            input.meta.firstSample = (uint64_t)iteration * 16000u;
            CHECK(NgRingPush(ring, &input) == NG_OK && NgRingAcquire(ring, &output, 0) == NG_OK);
            NgRingRelease(ring);
        }
        elapsed = NgMonotonicSeconds() - begin;
        printf("%s: 16000 pairs/block, 20000 iterations, %.3f us/block, %.1f M complex pairs/s\n",
               NgSampleFormatName(format), elapsed * 1000000.0 / 20000.0, 320.0 / elapsed);
        NgRingDestroy(ring);
        free(storage);
    }
}

/** @brief Run native format, integrity and pipeline regressions on this host.
 * @return Zero after all assertions; a failed contract terminates with nonzero status.
 */
static void TestGainQuality(void)
{
    unsigned char samples[8192 * 4];
    TNgIqView view = {0};
    TNgGainQuality quality;
    TNgError error = {{0}};
    uint32_t state = 1729;
    size_t k;
    TNgSampleFormat format;
    view.samples = (const TNgIq8 *)(const void *)samples;
    view.meta.count = 8192;
    for (format = NG_FORMAT_CI8; format <= NG_FORMAT_SC16_Q11;
         format = (TNgSampleFormat)(format + 1)) {
        int scale = format == NG_FORMAT_SC16_Q11 ? 16 : 1;
        double centeredScore;
        view.format = format;
        for (k = 0; k < 8192; ++k) {
            int sumI = 0, sumQ = 0;
            unsigned j;
            for (j = 0; j < 12; ++j) {
                state = state * 1664525u + 1013904223u;
                sumI += (int)(state >> 28);
                state = state * 1664525u + 1013904223u;
                sumQ += (int)(state >> 28);
            }
            NgIqStore(samples, format, k, 0, (sumI - 90) * 2 * scale);
            NgIqStore(samples, format, k, 1, (sumQ - 90) * 2 * scale);
        }
        CHECK(NgGainAnalyze(&view, &quality, &error) == NG_OK && quality.acceptable);
        centeredScore = quality.score;
        /* A small native DC offset is removable without clipping. Measure and
         * penalize it, but do not reject it by significance of its mean. */
        for (k = 0; k < 8192; ++k) {
            int inPhase = NgIqComponent(&view, k, 0);
            int quadrature = NgIqComponent(&view, k, 1);
            NgIqStore(samples, format, k, 0, inPhase + 3 * scale);
            NgIqStore(samples, format, k, 1, quadrature + 3 * scale);
        }
        CHECK(NgGainAnalyze(&view, &quality, &error) == NG_OK &&
              quality.score > centeredScore && quality.acceptable);
        for (k = 0; k < 8192; ++k) {
            int peak = format == NG_FORMAT_SC16_Q11 ? 2047 : 127;
            NgIqStore(samples, format, k, 0, k % 2 ? peak : -peak);
            NgIqStore(samples, format, k, 1, k % 2 ? -peak : peak);
        }
        CHECK(NgGainAnalyze(&view, &quality, &error) == NG_OK && !quality.acceptable &&
              quality.clippedFraction > 0.9);
    }
}

int main(int argc, char **argv)
{
    TNgSampleFormat format;
    if (argc == 2 && strcmp(argv[1], "--benchmark") == 0) {
        BenchmarkRing();
        return 0;
    }
    TestUnknownDeviceGap();
    TestGainQuality();
    for (format = NG_FORMAT_CI8; format <= NG_FORMAT_SC16_Q11;
         format = (TNgSampleFormat)(format + 1)) {
        TestIngress(format);
        TestConcurrent(format);
        TestPipeline(format);
        TestDiagnostics(format);
    }
    puts("PASS native CI8/CU8/SC16 Q11 ingress, 60,000 concurrent blocks, exact recording/replay "
         "and integer diagnostics");
    return 0;
}
