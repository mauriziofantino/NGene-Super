/* Exercise the shared PSD worker with deterministic CI8 Welch bursts. */
#include "ngene/analysis.h"
#include "platform/platform.h"
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static TNgIq8 samples[5 * NG_FFT_MAX_SIZE / 2];
static TNgAnalysisSnapshot result;

static int Measure(size_t n, TNgSearchFftMode mode)
{
    TNgAnalysisConfig config;
    TNgAnalysis *analysis;
    TNgError error = {{0}};
    TNgIqView view = {samples, {0}, NG_FORMAT_CI8};
    NgAnalysisDefaults(&config);
    config.fftSize = n;
    config.fftMode = mode;
    config.histogramEnabled = 0;
    config.window = NG_WINDOW_HANN;
    config.averages = 4;
    config.overlapPercent = 50;
    config.temporal = NG_TEMPORAL_NONE;
    config.updateHz = 30;
    view.meta.count = 5 * n / 2;
    if (NgAnalysisCreate(65536, &config, &analysis, &error) != NG_OK) {
        fprintf(stderr, "%s\n", error.message);
        return 0;
    }
    for (unsigned frame = 0; frame < 9; ++frame) {
        const double timeout = NgMonotonicSeconds() + 2;
        do {
            if (NgAnalysisSubmitAt(analysis, &view, 0) == NG_OK)
                NgSleepMs(2);
            NgAnalysisRead(analysis, &result);
        } while (result.psdSequence <= frame && NgMonotonicSeconds() < timeout);
        if (result.psdSequence != frame + 1 || result.firstSample != view.meta.firstSample ||
            result.observedPairs != view.meta.count || result.fftSize != n ||
            fabs(result.peakHz - 4096) > result.binHz || result.integrity.nanCount ||
            result.integrity.infCount) {
            fprintf(stderr, "Unexpected PSD publication at N=%zu frame=%u\n", n, frame);
            NgAnalysisDestroy(analysis);
            return 0;
        }
        if (frame) {
            printf("%zu,%u,%" PRIu64 ",%" PRIu32 ",%.17g,%.17g,%.6f,%.6f\n",
                   n, frame, result.firstSample, result.integrity.checksum, result.peakHz,
                   result.peakDbfs, result.computeMs,
                   1000 * (result.producedSeconds - result.capturedSeconds));
        }
        view.meta.firstSample += view.meta.count;
        NgSleepMs(36);
    }
    NgAnalysisDestroy(analysis);
    return 1;
}

int main(int argc, char **argv)
{
    if (argc > 2 || (argc == 2 && strcmp(argv[1], "--q14") != 0))
        return EXIT_FAILURE;
    const TNgSearchFftMode mode = argc == 2 ? NG_SEARCH_FFT_INTEGER_SCALED :
                                            NG_SEARCH_FFT_INTEGER_PRECISE;
    for (size_t k = 0; k < sizeof(samples) / sizeof(samples[0]); ++k) {
        const double phase = 6.283185307179586 * (double)(k % 16) / 16;
        samples[k].i = (int8_t)lround(64 * cos(phase));
        samples[k].q = (int8_t)lround(64 * sin(phase));
    }
    puts("N,frame,first_sample,checksum,peak_hz,peak_dbfs,compute_ms,latency_ms");
    for (size_t n = NG_SPECTRUM_MIN_FFT_SIZE; n <= NG_FFT_MAX_SIZE; n *= 2)
        if (!Measure(n, mode))
            return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
