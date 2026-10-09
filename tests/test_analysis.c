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
    TNgIqView v = {data, {0}};
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
    WaitResult(a, 2);
    CHECK(snapshot.spectrumBins == 0 && snapshot.histogramI[0] == 1024 &&
          snapshot.histogramQ[15] == 1024);
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
        data[i].i = 4;
        data[i].q = 0;
    }
    Submit(a, &v, 0);
    WaitResult(a, 1);
    CHECK(snapshot.statsI.mean == 21 && snapshot.statsI.std == 1 && snapshot.statsI.min == 20 &&
          snapshot.statsI.max == 22);
    CHECK(snapshot.statsQ.mean == 12 && snapshot.statsQ.std == 0 && snapshot.statsQ.rms == 12);
    CHECK(fabs(snapshot.statsI.rms - sqrt(442)) < 1e-9);
    CHECK(fabs(snapshot.dcMagnitude - hypot(21, 12)) < 1e-9);
    CHECK(snapshot.histogramI[148] == 2048 && snapshot.histogramI[150] == 2048);
    CHECK(fabs(snapshot.peakDbfs - 20 * log10(4.0 / 128)) < 0.01 && snapshot.peakHz == 0);
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
            data[i].i = 4;
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
                data[i].i = 4;
                data[i].q = 0;
            }
            v.meta.count = 256;
            v.meta.firstSample = 0;
            Submit(a, &v, 0);
            WaitResult(a, 1);
            firstSequence = snapshot.psdSequence;
            NgSleepMs(40);
            for (i = 0; i < 8192; ++i) {
                data[i].i = 8;
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
    puts("PASS: tone frequency/level, histogram totals/rails, gap reset and runtime "
         "reconfiguration");
    return 0;
}
