/** @file test_analysis.c @brief DSP worker, power calibration, histograms and runtime changes. */
#include "ngene/analysis.h"
#include "platform/platform.h"
#include <math.h>
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
static TNgIq8 data[8192];
static TNgAnalysisSnapshot snapshot;

/**
 * @brief Verify wait result behaviour.
 *
 * @details
 * Test-only helper exercising the named contract with deterministic inputs. Caller owns
 * fixture/context storage; assertion failures abort the test.
 *
 * @param a Caller-owned test fixture or output storage used by this helper.
 * @param revision Test parameter controlling this contract scenario.
 */
static void WaitResult(TNgAnalysis *a, uint64_t revision)
{
    unsigned i;
    for (i = 0; i < 500; ++i) {
        NgAnalysisRead(a, &snapshot);
        if (snapshot.sequence != 0 && snapshot.revision == revision &&
            (snapshot.config.spectrumEnabled == 0 || snapshot.psdSequence != 0) &&
            (snapshot.config.histogramEnabled == 0 || snapshot.histogramSequence != 0)) {
            return;
        }
        NgSleepMs(2);
    }
    fprintf(stderr,
            "Waiting for revision %llu: revision %llu, sequence %llu, PSD %llu, "
            "histogram %llu, enabled %d/%d\n",
            (unsigned long long)revision, (unsigned long long)snapshot.revision,
            (unsigned long long)snapshot.sequence, (unsigned long long)snapshot.psdSequence,
            (unsigned long long)snapshot.histogramSequence, snapshot.config.spectrumEnabled,
            snapshot.config.histogramEnabled);
    CHECK(0);
}

/**
 * @brief Verify submit behaviour.
 *
 * @details
 * Test-only helper exercising the named contract with deterministic inputs. Caller owns
 * fixture/context storage; assertion failures abort the test.
 *
 * @param a Caller-owned test fixture or output storage used by this helper.
 * @param v Read-only test input; retained by the fixture owner.
 * @param pre Test parameter controlling this contract scenario.
 */
static void Submit(TNgAnalysis *a, const TNgIqView *v, int pre)
{
    unsigned k;
    for (k = 0; k < 1000; ++k) {
        if (NgAnalysisSubmitAt(a, v, pre) == NG_OK) {
            return;
        }
        NgSleepMs(1);
    }
    CHECK(0);
}

/** @brief Revocation and lease expiry must discard partial PSD observations before resuming. */
static void DormantCapture(void)
{
    unsigned expire;
    for (expire = 0; expire < 2; ++expire) {
        TNgAnalysisConfig c;
        TNgAnalysis *a;
        TNgError error = {{0}};
        TNgIqView v = {data, {0}, NG_FORMAT_CI8};
        NgAnalysisDefaults(&c);
        c.histogramEnabled = 0;
        c.fftSize = 1024;
        c.averages = 2;
        c.window = NG_WINDOW_RECT;
        c.updateHz = 30;
        CHECK(NgAnalysisCreate(65536, &c, &a, &error) == NG_OK);
        NgAnalysisRead(a, &snapshot);
        CHECK(snapshot.sequence == 0 && snapshot.revision == 1 &&
              snapshot.config.spectrumEnabled == 1 && snapshot.config.histogramEnabled == 0);
        if (expire != 0) {
            NgAnalysisInterest(a, 1, NgMonotonicSeconds() + 0.02);
        }
        v.meta.count = 512;
        Submit(a, &v, 0);
        if (expire != 0) {
            NgSleepMs(30);
        } else {
            NgAnalysisInterest(a, 1, 0);
            v.meta.firstSample = 512;
            v.meta.count = 4096;
            Submit(a, &v, 0);
            NgSleepMs(10);
            NgAnalysisRead(a, &snapshot);
            CHECK(snapshot.sequence == 0);
        }
        NgAnalysisInterest(a, 1, NgMonotonicSeconds() + 2.0);
        v.meta.firstSample = 512;
        v.meta.count = 512;
        Submit(a, &v, 0);
        v.meta.firstSample = 1024;
        v.meta.count = 1024;
        Submit(a, &v, 0);
        WaitResult(a, 1);
        CHECK(snapshot.firstSample == 512 && snapshot.observedPairs == 1536);
        /* Both configured producers disabled: no capture or publication, even with interest. */
        c.spectrumEnabled = 0;
        CHECK(NgAnalysisConfigure(a, &c, &error) == NG_OK);
        NgAnalysisInterest(a, 3, NgMonotonicSeconds() + 2.0);
        v.meta.firstSample = 2048;
        v.meta.count = 4096;
        Submit(a, &v, 0);
        NgSleepMs(10);
        NgAnalysisRead(a, &snapshot);
        CHECK(snapshot.sequence == 0 && snapshot.spectrumBins == 0 && snapshot.histogramBins == 0);
        NgAnalysisDestroy(a);
    }
}

/** @brief Histograms use a fixed initial offset across revisions, gaps and later bursts. */
static void FixedDcHistograms(void)
{
    unsigned char raw[4096 * 4];
    unsigned char retained[sizeof(raw)];
    TNgSampleFormat format;
    int pre;
    for (format = NG_FORMAT_CI8; format <= NG_FORMAT_CI16; format = (TNgSampleFormat)(format + 1))
        for (pre = 0; pre <= 1; ++pre) {
            TNgAnalysisConfig config;
            TNgAnalysis *analysis;
            TNgError error = {{0}};
            TNgIqView view = {0};
            size_t k;
            int limit = format == NG_FORMAT_CI16       ? 32768
                        : format == NG_FORMAT_SC16_Q11 ? 2048
                                                       : 128;
            int step = limit / 128;
            int offset = limit / 4;
            NgAnalysisDefaults(&config);
            config.spectrumEnabled = 0;
            config.histogramPairs = 4096;
            config.histogramBins = 256;
            config.histogramPre = pre;
            view.samples = (const TNgIq8 *)(const void *)raw;
            view.format = format;
            view.meta.count = 4096;
            for (k = 0; k < view.meta.count; ++k) {
                NgIqStore(raw, format, k, 0, offset + (k % 2 ? step : -step));
                NgIqStore(raw, format, k, 1, -offset);
            }
            memcpy(retained, raw, sizeof(raw));
            CHECK(NgAnalysisCreate(65536, &config, &analysis, &error) == NG_OK);
            Submit(analysis, &view, pre);
            WaitResult(analysis, 1);
            CHECK(snapshot.removedDcI == offset && snapshot.removedDcQ == -offset);
            CHECK(snapshot.dcEstimatePairs == 4096);
            CHECK(snapshot.statsI.mean == 0 && snapshot.statsQ.mean == 0);
            CHECK(snapshot.histogramI[127] == 2048 && snapshot.histogramI[129] == 2048);
            CHECK(snapshot.histogramQ[128] == 4096);
            CHECK(!memcmp(retained, raw, sizeof(raw)));

            /* A changed observed mean must remain visible. A display setting change
         * and a sample gap do not represent a new hardware DC calibration. */
            config.fftSize = 256;
            CHECK(NgAnalysisConfigure(analysis, &config, &error) == NG_OK);
            for (k = 0; k < view.meta.count; ++k) {
                NgIqStore(raw, format, k, 0, offset + 3 * step);
                NgIqStore(raw, format, k, 1, -offset - 2 * step);
            }
            view.meta.firstSample = 9000;
            view.meta.flags = NG_BLOCK_GAP;
            Submit(analysis, &view, pre);
            WaitResult(analysis, 2);
            CHECK(snapshot.removedDcI == offset && snapshot.removedDcQ == -offset);
            CHECK(snapshot.statsI.mean == 3 * step && snapshot.statsQ.mean == -2 * step);
            CHECK(snapshot.histogramI[131] == 4096 && snapshot.histogramQ[126] == 4096);
            config.spectrumEnabled = 1;
            config.window = NG_WINDOW_RECT;
            config.averages = 1;
            config.spectrumPre = pre;
            CHECK(NgAnalysisConfigure(analysis, &config, &error) == NG_OK);
            view.meta.firstSample += view.meta.count;
            view.meta.flags = 0;
            Submit(analysis, &view, pre);
            WaitResult(analysis, 3);
            CHECK(snapshot.removedDcI == offset && snapshot.removedDcQ == -offset);
            CHECK(snapshot.peakHz == 0);
            CHECK(fabs(snapshot.peakDbfs - 10 * log10(13.0 / (128.0 * 128.0))) < 0.02);
            NgAnalysisDestroy(analysis);

            /* A new session estimates again. Widened histograms preserve opposite
         * rail excursions and show the initial integer-rounding residual. */
            for (k = 0; k < view.meta.count; ++k) {
                NgIqStore(raw, format, k, 0, k % 4 ? limit - 1 : -limit);
                NgIqStore(raw, format, k, 1, k % 4 ? -limit : limit - 1);
            }
            CHECK(NgAnalysisCreate(65536, &config, &analysis, &error) == NG_OK);
            Submit(analysis, &view, pre);
            WaitResult(analysis, 1);
            CHECK(snapshot.removedDcI == limit / 2 - 1 && snapshot.removedDcQ == -limit / 2);
            CHECK(snapshot.statsI.mean == 0.25 && snapshot.statsQ.mean == -0.25);
            CHECK(snapshot.histogramMin == -2 * limit && snapshot.histogramMax == 2 * limit - 1);
            CHECK(snapshot.histogramI[32] == 1024 && snapshot.histogramI[160] == 3072);
            CHECK(snapshot.histogramQ[223] == 1024 && snapshot.histogramQ[96] == 3072);
            CHECK(snapshot.clippedI == 4096 && snapshot.clippedQ == 4096);
            CHECK(fabs(snapshot.peakDbfs - 10 * log10(2 * pow(0.5 - 0.25 / limit, 2))) < 0.03);
            NgAnalysisDestroy(analysis);
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
    TNgAnalysisConfig c;
    TNgAnalysis *a;
    TNgError e = {{0}};
    size_t i;
    uint64_t sum;
    TNgIqView v = {data, {0}, NG_FORMAT_CI8};
    NgAnalysisDefaults(&c);
    c.fftSize = 1024;
    c.window = NG_WINDOW_RECT;
    c.averages = 4;
    c.overlapPercent = 50;
    c.histogramPairs = 4096;
    c.histogramBins = 256;
    CHECK(NgAnalysisCreate(65536, &c, &a, &e) == NG_OK);
    for (i = 0; i < 8192; ++i) {
        double phase = 6.283185307179586 * (double)i / 16;
        data[i].i = (int8_t)lround(64 * cos(phase));
        data[i].q = (int8_t)lround(64 * sin(phase));
    }
    v.meta.count = 2048;
    Submit(a, &v, 0);
    /* Gap must discard the partial TNgBurst; never FFT over disconnected data. */
    v.meta.firstSample = 5000;
    v.meta.count = 4096;
    Submit(a, &v, 0);
    WaitResult(a, 1);
    CHECK(snapshot.firstSample == 5000 && snapshot.skippedBursts == 2);
    CHECK(fabs(snapshot.peakHz - 4096) < 0.01);
    CHECK(fabs(snapshot.peakDbfs + 6.02) < 0.15);
    sum = 0;
    for (i = 0; i < 256; ++i) {
        sum += snapshot.histogramI[i];
    }
    CHECK(sum == 4096);
    sum = 0;
    for (i = 0; i < 256; ++i) {
        sum += snapshot.histogramQ[i];
    }
    CHECK(sum == 4096);
    /* The int32 adapter must restore the same tone frequency and level. */
    c.fftSize = 2048;
    c.averages = 2;
    c.fftMode = NG_SEARCH_FFT_INTEGER_PRECISE;
    CHECK(NgAnalysisConfigure(a, &c, &e) == NG_OK);
    v.meta.firstSample = 10000;
    Submit(a, &v, 0);
    WaitResult(a, 2);
    CHECK(snapshot.firstSample == 10000 && snapshot.binHz == 32);
    CHECK(fabs(snapshot.peakHz - 4096) < 0.01);
    CHECK(fabs(snapshot.peakDbfs + 6.02) < 0.15);
    c.spectrumEnabled = 0;
    c.histogramBins = 16;
    c.histogramPairs = 1024;
    CHECK(NgAnalysisConfigure(a, &c, &e) == NG_OK);
    for (i = 0; i < 1024; ++i) {
        data[i].i = -128;
        data[i].q = 127;
    }
    v.meta.firstSample = 12000;
    v.meta.count = 1024;
    Submit(a, &v, 0);
    WaitResult(a, 3);
    CHECK(snapshot.spectrumBins == 0 && snapshot.histogramI[0] == 1024 &&
          snapshot.histogramQ[15] == 1024);
    CHECK(snapshot.removedDcI == 0 && snapshot.removedDcQ == 0);
    CHECK(snapshot.clippedI == 1024 && snapshot.clippedQ == 1024);
    c.histogramBins = 63;
    CHECK(NgAnalysisConfigure(a, &c, &e) == NG_ERROR);
    NgAnalysisDestroy(a);
    /* Independent pre/post paths and statistics must not depend on NFFT. */
    NgAnalysisDefaults(&c);
    c.averages = 1;
    c.fftSize = 256;
    c.window = NG_WINDOW_RECT;
    c.histogramPairs = 4096;
    c.histogramBins = 256;
    c.spectrumPre = 0;
    c.histogramPre = 1;
    CHECK(NgAnalysisCreate(65536, &c, &a, &e) == NG_OK);
    for (i = 0; i < 8192; ++i) {
        data[i].i = (int8_t)(20 + (int)(i % 2) * 2);
        data[i].q = 12;
    }
    v.meta.firstSample = 0;
    v.meta.count = 4096;
    Submit(a, &v, 1);
    for (i = 0; i < 8192; ++i) {
        data[i].i = (int8_t)(i % 2 ? 4 : -4);
        data[i].q = 0;
    }
    Submit(a, &v, 0);
    WaitResult(a, 1);
    CHECK(snapshot.statsI.mean == 0 && snapshot.statsI.std == 1 && snapshot.statsI.min == -1 &&
          snapshot.statsI.max == 1);
    CHECK(snapshot.statsQ.mean == 0 && snapshot.statsQ.std == 0 && snapshot.statsQ.rms == 0);
    CHECK(snapshot.statsI.rms == 1 && snapshot.dcMagnitude == 0);
    CHECK(snapshot.removedDcI == 21 && snapshot.removedDcQ == 12);
    CHECK(snapshot.histogramI[127] == 2048 && snapshot.histogramI[129] == 2048);
    CHECK(fabs(snapshot.peakDbfs - 20 * log10(4.0 / 128)) < 0.01 && snapshot.peakHz == -32768);
    {
        TNgAnalysisSnapshot retained = snapshot;
        TNgPsdIntegrity integrity;
        c.fftSize = 4096;
        c.density = 1;
        c.overlapPercent = 37;
        c.averages = 1;
        CHECK(NgAnalysisConfigure(a, &c, &e) == NG_OK);
        for (i = 0; i < 8192; ++i) {
            data[i].i = (int8_t)(20 + (int)(i % 2) * 2);
            data[i].q = 12;
        }
        Submit(a, &v, 1);
        for (i = 0; i < 8192; ++i) {
            data[i].i = (int8_t)(i % 2 ? 4 : -4);
            data[i].q = 0;
        }
        Submit(a, &v, 0);
        WaitResult(a, 2);
        CHECK(snapshot.histogramCount == retained.histogramCount);
        CHECK(!memcmp(snapshot.histogramI, retained.histogramI, sizeof(snapshot.histogramI)));
        CHECK(snapshot.statsI.mean == retained.statsI.mean);
        CHECK(fabs(snapshot.enbwHz - 16) < 1e-9);
        CHECK(fabs(snapshot.peakDbfs - (retained.peakDbfs - 10 * log10(16))) < 0.01);
        NgPsdCheck(retained.spectrumDbfs, retained.spectrumBins, &integrity);
        CHECK(integrity.checksum == retained.integrity.checksum && !integrity.nanCount &&
              !integrity.infCount);
        retained.spectrumDbfs[2] = NAN;
        retained.spectrumDbfs[3] = INFINITY;
        NgPsdCheck(retained.spectrumDbfs, retained.spectrumBins, &integrity);
        CHECK(integrity.nanCount == 1 && integrity.infCount == 1);
    }
    NgAnalysisDestroy(a);
    /* Temporal smoothing operates on linear power, never directly on dB. */
    {
        int mode;
        for (mode = NG_TEMPORAL_MEAN; mode <= NG_TEMPORAL_EMA; ++mode) {
            uint64_t firstSequence;
            NgAnalysisDefaults(&c);
            c.histogramEnabled = 0;
            c.fftSize = 256;
            c.averages = 1;
            c.window = NG_WINDOW_RECT;
            c.temporal = (TNgTemporal)mode;
            c.temporalFrames = 2;
            c.smoothingAlpha = 0.25;
            c.updateHz = 30;
            CHECK(NgAnalysisCreate(65536, &c, &a, &e) == NG_OK);
            for (i = 0; i < 8192; ++i) {
                data[i].i = (int8_t)(i % 2 ? 4 : -4);
                data[i].q = 0;
            }
            v.meta.count = 256;
            v.meta.firstSample = 0;
            Submit(a, &v, 0);
            WaitResult(a, 1);
            firstSequence = snapshot.psdSequence;
            NgSleepMs(40);
            for (i = 0; i < 8192; ++i) {
                data[i].i = (int8_t)(i % 2 ? 8 : -8);
            }
            v.meta.firstSample = 256;
            Submit(a, &v, 0);
            for (i = 0; i < 1000; ++i) {
                NgAnalysisRead(a, &snapshot);
                if (snapshot.psdSequence > firstSequence) {
                    break;
                }
                NgSleepMs(1);
            }
            CHECK(snapshot.psdSequence > firstSequence);
            CHECK(fabs(snapshot.peakDbfs -
                       10 * log10((mode == NG_TEMPORAL_MEAN ? 40.0 : 28.0) / (128 * 128))) < 0.002);
            if (mode == NG_TEMPORAL_MEAN) {
                NgAnalysisDestroy(a);
            }
        }
    }
    /* Foreground overload pauses optional work without withdrawing its consumer lease. */
    NgAnalysisPause(a, 1);
    NgSleepMs(10);
    NgAnalysisRead(a, &snapshot);
    {
        uint64_t previous = snapshot.sequence;
        Submit(a, &v, 0);
        NgSleepMs(20);
        NgAnalysisRead(a, &snapshot);
        CHECK(snapshot.sequence == previous);
        NgAnalysisPause(a, 0);
        Submit(a, &v, 0);
        for (i = 0; i < 1000; i++) {
            NgAnalysisRead(a, &snapshot);
            if (snapshot.sequence > previous)
                break;
            NgSleepMs(1);
        }
        CHECK(snapshot.sequence > previous);
    }
    /* No consumer interest: production stops, while submissions remain nonblocking. */
    NgAnalysisInterest(a, 3, 0);
    {
        uint64_t previous = snapshot.sequence;
        NgSleepMs(40);
        Submit(a, &v, 0);
        NgSleepMs(10);
        NgAnalysisRead(a, &snapshot);
        CHECK(snapshot.sequence == previous);
    }
    NgAnalysisDestroy(a);
    DormantCapture();
    FixedDcHistograms();
    puts("PASS: tone frequency/level, histogram totals/rails, gap reset and runtime "
         "reconfiguration, dormant capture cancellation");
    return 0;
}
