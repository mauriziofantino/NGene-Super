#include "ngene/frontend.h"
#include "ngene/capture.h"
#include "ngene/receiver.h"
#include "ngene/conditioning.h"
#include "frontend/sige.h"
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x);                                        \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

static void Conversion(void)
{
    unsigned char raw[131076], out[131076];
    TNgIqView in = {0}, view = {0};
    size_t n, k;
    unsigned f, t;
    const TNgSampleFormat formats[] = {NG_FORMAT_CI8, NG_FORMAT_CU8, NG_FORMAT_SC16_Q11,
                                       NG_FORMAT_CI16};
    in.samples = (const TNgIq8 *)(const void *)raw;
    for (f = 0; f < 4; f++) {
        for (t = 0; t < 2; t++) {
            for (n = 1; n <= 32769; n = n < 65 ? n + 1 : n + 997) {
                int limit = formats[f] == NG_FORMAT_CI16       ? 32768
                            : formats[f] == NG_FORMAT_SC16_Q11 ? 2048
                                                               : 128;
                TNgSampleFormat target = t ? NG_FORMAT_CI16 : NG_FORMAT_CI8;
                in.format = formats[f];
                in.meta.count = n;
                for (k = 0; k < 2 * n; k++) {
                    NgIqStore(raw, in.format, k / 2, (unsigned)(k % 2),
                              (int)(k * 271u % (unsigned)(2 * limit)) - limit);
                }
                CHECK(NgIqConvert(out, target, &in) == NG_OK);
                view.samples = (const TNgIq8 *)(const void *)out;
                view.format = target;
                for (k = 0; k < 2 * n; k++) {
                    int x = NgIqComponent(&in, k / 2, (unsigned)(k % 2)), expected = x;
                    if (target == NG_FORMAT_CI16 && in.format != NG_FORMAT_CI16) {
                        expected *= in.format == NG_FORMAT_SC16_Q11 ? 16 : 256;
                    }
                    if (target == NG_FORMAT_CI8 && NgSampleBytes(in.format) == 4) {
                        int divisor = in.format == NG_FORMAT_SC16_Q11 ? 16 : 256,
                            half = divisor / 2;
                        expected = x >= 0 ? (x + half) / divisor : -((-x + half) / divisor);
                        if (expected > 127) {
                            expected = 127;
                        }
                        if (expected < -128) {
                            expected = -128;
                        }
                    }
                    CHECK(NgIqComponent(&view, k / 2, (unsigned)(k % 2)) == expected);
                }
            }
        }
    }
    in.format = NG_FORMAT_SC16_Q11;
    in.meta.count = 17;
    memset(raw, 0, sizeof(raw));
    NgIqStore(raw, NG_FORMAT_SC16_Q11, 0, 0, 2048);
    CHECK(NgIqConvert(out, NG_FORMAT_CI16, &in) == NG_ERROR);
    CHECK(NgIqConvert(out, NG_FORMAT_CI8, &in) == NG_ERROR);
    /* Slot alignment must survive odd-sized windows and head/tail wrap. */
    {
        TNgRing *ring;
        TNgError error = {{0}};
        in.format = NG_FORMAT_CU8;
        in.meta.count = 17;
        CHECK(NgRingCreateFormat(3, 17, NG_FORMAT_CI16, &ring, &error) == NG_OK);
        for (k = 0; k < 30; k++) {
            in.meta.firstSample = k * 17;
            CHECK(NgRingPushConverted(ring, &in) == NG_OK);
            CHECK(NgRingAcquire(ring, &view, 0) == NG_OK);
            CHECK(((uintptr_t)view.samples & 63u) == 0);
            CHECK(view.meta.firstSample == in.meta.firstSample);
            NgRingRelease(ring);
        }
        NgRingDestroy(ring);
    }
}

static void Decoder(void)
{
    unsigned char raw[2050];
    TNgIq8 iq[1025];
    size_t split, k, n, total;
    TNgSigeDecoder state;
    for (k = 0; k < sizeof(raw); k++) {
        raw[k] = (unsigned char)((k % 2 ? 0 : 2) | (k / 2 % 2));
    }
    for (split = 1; split < sizeof(raw); split++) {
        memset(&state, 0, sizeof(state));
        CHECK(NgSigeDecode(&state, raw, split, iq, 1025, &n) == NG_OK);
        total = n;
        CHECK(NgSigeDecode(&state, raw + split, sizeof(raw) - split, iq + total, 1025 - total,
                           &n) == NG_OK);
        total += n;
        CHECK(total == 1025 && !state.pending);
        for (k = 0; k < total; k++) {
            CHECK(iq[k].i == (k % 2 ? -1 : 1) && iq[k].q == (k % 2 ? -1 : 1));
        }
    }
    memset(&state, 0, sizeof(state));
    CHECK(NgSigeDecode(&state, raw + 1, 17, iq, 1025, &n) == NG_OK && state.gap && n == 8);
    memset(&state, 0, sizeof(state));
    raw[9] |= 2;
    CHECK(NgSigeDecode(&state, raw, 32, iq, 1025, &n) == NG_ERROR);
}

typedef struct {
    unsigned n, limit;
    int started, stopped;
    unsigned char samples[34];
} Fake;

static int Start(void *c, TNgError *e)
{
    Fake *s = c;
    (void)e;
    s->started = 1;
    return NG_OK;
}

static int Acquire(void *c, TNgIqView *v, unsigned timeout, TNgError *e)
{
    Fake *s = c;
    unsigned k;
    (void)timeout;
    (void)e;
    if (s->n == s->limit) {
        return NG_END;
    }
    for (k = 0; k < 34; k++) {
        s->samples[k] = (unsigned char)(k + s->n);
    }
    memset(v, 0, sizeof(*v));
    v->samples = (const TNgIq8 *)(const void *)s->samples;
    v->format = NG_FORMAT_CU8;
    v->meta.count = 17;
    v->meta.firstSample = s->n * 17;
    v->meta.hardwareTimestamp = 1000 + s->n * 17;
    v->meta.flags = NG_BLOCK_HARDWARE_TIME;
    if (s->n == 3) {
        v->meta.flags |= NG_BLOCK_GAP;
        v->meta.missingKnown = 0;
    }
    ++s->n;
    return NG_OK;
}

static void Release(void *c)
{
    (void)c;
}

static int Stop(void *c, TNgError *e)
{
    Fake *s = c;
    (void)e;
    s->stopped = 1;
    return NG_OK;
}

static void Stats(void *c, TNgFrontendStats *v)
{
    Fake *s = c;
    memset(v, 0, sizeof(*v));
    v->receivedSamples = s->n * 17;
}

static void Destroy(void *c)
{
    (void)c;
}

static const TNgFrontendOps ops = {Start, Acquire, Release, Stop, Stats, Destroy};

static void WorkerTest(TNgSampleFormat target, int overflow)
{
    Fake fake = {0};
    TNgConfig config;
    TNgFrontend source = {0};
    TNgIqView view;
    TNgError error = {{0}};
    unsigned n = 0;
    int rc;
    NgConfigDefaults(&config);
    config.windowSamples = 17;
    config.ingressBlocks = 2;
    config.streamFormat = target;
    fake.limit = overflow == 1 ? 100 : 8;
    source.ops = &ops;
    source.context = &fake;
    source.info.format = NG_FORMAT_CU8;
    source.info.sampleRateHz = 100000;
    source.info.sourceName = overflow ? "fixture" : "file";
    CHECK(NgFrontendCommon(&config, &source, &error) == NG_OK);
    CHECK(source.info.format == target && source.info.nativeFormat == NG_FORMAT_CU8);
    CHECK(source.ops->start(source.context, &error) == NG_OK);
    if (overflow) {
        /* Mode 2 is a finite transport burst followed by a brief scheduling
         * delay, not a stalled consumer. It must drain without losing data. */
        NgSleepMs(overflow == 1 ? 120 : 5);
    }
    while ((rc = source.ops->acquire(source.context, &view, 100, &error)) == NG_OK ||
           rc == NG_TIMEOUT) {
        if (rc == NG_TIMEOUT) {
            continue;
        }
        CHECK(((uintptr_t)view.samples & 63u) == 0);
        CHECK(view.meta.firstSample == n * 17 && view.meta.hardwareTimestamp == 1000 + n * 17);
        CHECK(NgIqComponent(&view, 0, 0) == ((int)n - 128) * (target == NG_FORMAT_CI16 ? 256 : 1));
        if (n == 3) {
            CHECK((view.meta.flags & NG_BLOCK_GAP) && !view.meta.missingKnown);
        }
        ++n;
        source.ops->release(source.context);
    }
    CHECK(rc == (overflow == 1 ? NG_ERROR : NG_END));
    if (overflow != 1) {
        CHECK(n == 8);
    }
    CHECK(source.ops->stop(source.context, &error) == (overflow == 1 ? NG_ERROR : NG_OK) &&
          fake.stopped);
    NgFrontendDestroy(&source);
}

static void Recording(void)
{
    char input[128], recorded[160], path[160];
    unsigned char raw[52], actual[52];
    TNgConfig config, replay;
    TNgFrontend source;
    TNgError error = {{0}};
    TNgReceiverStats stats;
    TNgCaptureMetadata metadata;
    TNgConditioner bypass = NgConditionerBypass();
    FILE *file;
    unsigned k;
    NgConfigDefaults(&config);
    snprintf(config.recordingPrefix, sizeof(config.recordingPrefix), "ngene_common_%llu",
             (unsigned long long)(NgMonotonicSeconds() * 1000000));
    snprintf(input, sizeof(input), "%s.input", config.recordingPrefix);
    for (k = 0; k < 26; k++) {
        NgIqStore(raw, NG_FORMAT_SC16_Q11, k / 2, k % 2, (int)k * 157 - 2048);
    }
    file = fopen(input, "wb");
    CHECK(file && fwrite(raw, 4, 13, file) == 13 && fclose(file) == 0);
    snprintf(config.replayPath, sizeof(config.replayPath), "%s", input);
    config.replayFormat = NG_FORMAT_SC16_Q11;
    config.windowSamples = 7;
    config.recordingEnabled = 1;
    config.replayRealtime = 0;
    CHECK(NgFrontendOpen(&config, &source, &error) == NG_OK &&
          source.info.format == NG_FORMAT_CI16);
    CHECK(NgReceiverRun(&config, &source, &bypass, NULL, 0, NULL, NULL, &stats, &error) == NG_OK);
    NgFrontendDestroy(&source);
    snprintf(recorded, sizeof(recorded), "%s_000000.ci16", config.recordingPrefix);
    file = fopen(recorded, "rb");
    CHECK(file && fread(actual, 4, 13, file) == 13 && fclose(file) == 0);
    {
        TNgIqView view = {0};
        view.samples = (const TNgIq8 *)(const void *)actual;
        view.format = NG_FORMAT_CI16;
        for (k = 0; k < 26; k++) {
            CHECK(NgIqComponent(&view, k / 2, k % 2) == ((int)k * 157 - 2048) * 16);
        }
    }
    NgConfigDefaults(&replay);
    CHECK(NgReplayConfigure(recorded, &replay, &metadata, &error) == NG_OK);
    CHECK(replay.replayFormat == NG_FORMAT_CI16 && metadata.samples == 13 && metadata.complete &&
          metadata.sourceAdcBits == 12);
    CHECK(NgFrontendOpen(&replay, &source, &error) == NG_OK && source.info.adcBits == 12);
    NgFrontendDestroy(&source);
    CHECK(remove(input) == 0 && remove(recorded) == 0);
    snprintf(path, sizeof(path), "%s.meta.ini", recorded);
    CHECK(remove(path) == 0);
    snprintf(path, sizeof(path), "%s.meta.ini", config.recordingPrefix);
    CHECK(remove(path) == 0);
    snprintf(path, sizeof(path), "%s.index.csv", config.recordingPrefix);
    CHECK(remove(path) == 0);
}

int main(void)
{
    TNgConfig config;
    TNgError error = {{0}};
    Conversion();
    Decoder();
    WorkerTest(NG_FORMAT_CI8, 0);
    WorkerTest(NG_FORMAT_CI16, 0);
    WorkerTest(NG_FORMAT_CI8, 1);
    WorkerTest(NG_FORMAT_CI8, 2);
    WorkerTest(NG_FORMAT_CI16, 2);
    Recording();
    NgConfigDefaults(&config);
    NgConfigSige(&config);
    CHECK(NgConfigValidate(&config, &error) == NG_OK);
    config.sampleRateHz++;
    CHECK(NgConfigValidate(&config, &error) == NG_ERROR);
    puts("PASS: all common format conversions/tails, aligned ring wrap, native SiGe marker splits, "
         "worker lifetime/backpressure/overflow and immutable hardware profile.");
    return 0;
}
