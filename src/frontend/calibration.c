#include "ngene/calibration.h"
#include "platform/platform.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

enum { CAL_PAIRS = 32768, CAL_BLOCK = 8192, CAL_MAX = 32 };

int NgGainAnalyze(const TNgIqView *view, TNgGainQuality *q, TNgError *error)
{
    double mean[2] = {0}, second[2] = {0}, fourth[2] = {0};
    double limit, variance[2], kurtosis[2], dc, rms;
    size_t k, clipped = 0;
    if (!view || !view->samples || view->meta.count < 1024 || !q)
        return NgFail(error, "Calibration requires at least 1024 I/Q pairs");
    memset(q, 0, sizeof(*q));
    limit = view->format == NG_FORMAT_SC16_Q11 ? 2048.0 : 128.0;
    for (k = 0; k < view->meta.count; ++k) {
        unsigned c;
        for (c = 0; c < 2; ++c) {
            double x = NgIqComponent(view, k, c) / limit;
            mean[c] += x;
            if (fabs(x) >= 0.97) ++clipped;
        }
    }
    for (k = 0; k < 2; ++k) mean[k] /= (double)view->meta.count;
    for (k = 0; k < view->meta.count; ++k) {
        unsigned c;
        for (c = 0; c < 2; ++c) {
            double d = NgIqComponent(view, k, c) / limit - mean[c];
            double d2 = d * d;
            second[c] += d2;
            fourth[c] += d2 * d2;
        }
    }
    for (k = 0; k < 2; ++k) {
        variance[k] = second[k] / (double)view->meta.count;
        kurtosis[k] = variance[k] > 1e-12 ? fourth[k] / (double)view->meta.count /
                       (variance[k] * variance[k]) : 0;
    }
    rms = sqrt((variance[0] + variance[1]) * 0.5);
    dc = hypot(mean[0], mean[1]);
    q->rmsFraction = rms;
    q->clippedFraction = (double)clipped / (2.0 * (double)view->meta.count);
    q->gaussianError = (fabs(kurtosis[0] - 3.0) + fabs(kurtosis[1] - 3.0)) * 0.5;
    q->dcFraction = dc;
    q->iqImbalance = fabs(sqrt(variance[0]) - sqrt(variance[1])) /
                     (sqrt(variance[0]) + sqrt(variance[1]) + 1e-12);
    q->acceptable = rms >= 0.06 && rms <= 0.32 && q->clippedFraction < 0.001 &&
                    q->gaussianError < 0.9 && q->iqImbalance < 0.25 && dc < 0.14;
    q->score = fabs(log((rms + 1e-12) / 0.18)) + 2.0 * q->gaussianError +
               8.0 * q->iqImbalance + 5.0 * dc + 100.0 * q->clippedFraction;
    return NG_OK;
}

static int Measure(const TNgConfig *candidate, TNgGainQuality *q, TNgError *error)
{
    TNgConfig local = *candidate;
    TNgFrontend frontend = {0};
    TNgIqView view = {0};
    TNgIqView collected = {0};
    unsigned char *samples = NULL;
    size_t bytes, count = 0, discard;
    double deadline;
    int started = 0, ok = NG_ERROR;
    local.windowSamples = CAL_BLOCK;
    local.ingressBlocks = 32;
    local.ringBlocks = 32;
    if (NgFrontendOpen(&local, &frontend, error) != NG_OK) return NG_ERROR;
    bytes = NgSampleBytes(frontend.info.format);
    if (!bytes || (samples = malloc(CAL_PAIRS * bytes)) == NULL) {
        NgFail(error, "Cannot allocate calibration sample buffer");
        goto done;
    }
    if (frontend.ops->start(frontend.context, error) != NG_OK) goto done;
    started = 1;
    discard = candidate->sampleRateHz / 50u; /* 20 ms for gain settling. */
    deadline = NgMonotonicSeconds() + 4.0;
    while (count < CAL_PAIRS && NgMonotonicSeconds() < deadline) {
        int rc = frontend.ops->acquire(frontend.context, &view, 200, error);
        size_t offset = 0, n;
        if (rc == NG_TIMEOUT) continue;
        if (rc != NG_OK) goto done;
        if (discard) {
            offset = view.meta.count < discard ? view.meta.count : discard;
            discard -= offset;
        }
        n = view.meta.count - offset;
        if (n > CAL_PAIRS - count) n = CAL_PAIRS - count;
        if (n) memcpy(samples + count * bytes,
                      (const unsigned char *)view.samples + offset * bytes, n * bytes);
        count += n;
        frontend.ops->release(frontend.context);
    }
    if (count < CAL_PAIRS) {
        NgFail(error, "SDR did not deliver enough I/Q samples for calibration");
        goto done;
    }
    collected.samples = (const TNgIq8 *)samples;
    collected.format = frontend.info.format;
    collected.meta.count = count;
    ok = NgGainAnalyze(&collected, q, error);
done:
    if (started) {
        TNgError stopError = {{0}};
        if (frontend.ops->stop(frontend.context, &stopError) != NG_OK && ok == NG_OK) {
            *error = stopError;
            ok = NG_ERROR;
        }
    }
    NgFrontendDestroy(&frontend);
    free(samples);
    return ok;
}

static void Try(const TNgConfig *candidate, TNgGainCalibration *result, TNgError *error)
{
    TNgGainQuality quality;
    double score, previous;
    if (result->tested >= CAL_MAX || Measure(candidate, &quality, error) != NG_OK) return;
    ++result->tested;
    score = quality.score;
    previous = result->quality.score;
    if (candidate->frontend == NG_FRONTEND_HACKRF) {
        /* Similar ADC distributions are resolved toward balanced IF/baseband gain.
         * This is only a tie-breaker: measured distortion and clipping dominate. */
        score += 0.005 * fabs((double)candidate->lnaGainDb - candidate->vgaGainDb);
        score += candidate->rfAmp ? 0.1 : 0.0;
        previous += 0.005 * fabs((double)result->selected.lnaGainDb -
                                 result->selected.vgaGainDb);
        previous += result->selected.rfAmp ? 0.1 : 0.0;
    }
    if (result->tested == 1 || (quality.acceptable && !result->accepted) ||
        (quality.acceptable == result->accepted && score < previous)) {
        result->selected = *candidate;
        result->quality = quality;
        result->accepted = quality.acceptable;
    }
}

int NgGainCalibrate(const TNgConfig *config, TNgGainCalibration *result, TNgError *error)
{
    TNgConfig candidate;
    unsigned i;
    static const int percent[] = {0, 25, 50, 75, 100};
    if (!config || !result || config->frontend == NG_FRONTEND_FILE ||
        !NgFrontendAvailable(config->frontend))
        return NgFail(error, "Select an available live SDR for gain calibration");
    memset(result, 0, sizeof(*result));
    result->selected = *config;
    if (NgFrontendCapabilities(config, &result->capabilities, error) != NG_OK) return NG_ERROR;
    if (config->frontend == NG_FRONTEND_HACKRF) {
        static const unsigned lna[] = {0,8,8,16,24,16,24,32,24,32,40,40,40};
        static const unsigned vga[] = {0,8,24,16,8,32,24,16,40,32,24,40,62};
        unsigned a;
        /* Prefer the documented balanced 16/16 starting region. Test comparable
         * total gains with different stage splits; do not assume they are equivalent. */
        for (a = 0; a < sizeof(lna)/sizeof(lna[0]); ++a) {
            candidate = *config;
            candidate.lnaGainDb = lna[a];
            candidate.vgaGainDb = vga[a];
            candidate.rfAmp = 0;
            Try(&candidate, result, error);
        }
        if (result->tested) {
            int delta;
            for (delta = -8; delta <= 8; delta += 16) {
                int nextLna = (int)result->selected.lnaGainDb + delta;
                int nextVga = (int)result->selected.vgaGainDb + delta;
                if (nextLna >= 0 && nextLna <= 40) {
                    candidate = result->selected;
                    candidate.lnaGainDb = (unsigned)nextLna;
                    Try(&candidate, result, error);
                }
                if (nextVga >= 0 && nextVga <= 62) {
                    candidate = result->selected;
                    candidate.vgaGainDb = (unsigned)nextVga;
                    Try(&candidate, result, error);
                }
            }
        }
        if (!result->accepted) {
            for (i = 0; i < 4; ++i) {
                candidate = *config;
                candidate.lnaGainDb = i * 8;
                candidate.vgaGainDb = i * 8;
                candidate.rfAmp = 1;
                Try(&candidate, result, error);
            }
        }
    } else if (config->frontend == NG_FRONTEND_RTLSDR) {
        size_t count = result->capabilities.tunerGainCount;
        if (!count) return NgFail(error, "RTL-SDR reports no manual tuner gains");
        for (i = 0; i < 5; ++i) {
            candidate = *config;
            candidate.rtlAutoGain = 0;
            candidate.rtlGainTenthsDb = result->capabilities.tunerGains[(count - 1) * percent[i] / 100];
            Try(&candidate, result, error);
        }
        if (result->tested) {
            size_t k;
            for (k = 0; k < count; ++k) if (result->capabilities.tunerGains[k] == result->selected.rtlGainTenthsDb) break;
            for (i = 1; i <= 2; ++i) {
                if (k >= i) { candidate = *config; candidate.rtlAutoGain = 0; candidate.rtlGainTenthsDb = result->capabilities.tunerGains[k-i]; Try(&candidate,result,error); }
                if (k+i < count) { candidate = *config; candidate.rtlAutoGain = 0; candidate.rtlGainTenthsDb = result->capabilities.tunerGains[k+i]; Try(&candidate,result,error); }
            }
        }
    } else if (config->frontend == NG_FRONTEND_BLADERF) {
        int min = result->capabilities.gainMinDb, max = result->capabilities.gainMaxDb;
        int step = result->capabilities.gainStepDb;
        int levels;
        if (step <= 0 || max < min) return NgFail(error, "bladeRF reports invalid gain limits");
        levels = (max - min) / step + 1;
        for (i = 0; i < 5; ++i) {
            candidate = *config;
            candidate.bladeGainDb = min + ((levels - 1) * percent[i] / 100) * step;
            Try(&candidate, result, error);
        }
        if (result->tested) {
            int k = (result->selected.bladeGainDb - min) / step;
            for (i = 1; i <= 2; ++i) {
                if (k >= (int)i) { candidate = *config; candidate.bladeGainDb = min+(k-(int)i)*step; Try(&candidate,result,error); }
                if (k+(int)i < levels) { candidate = *config; candidate.bladeGainDb = min+(k+(int)i)*step; Try(&candidate,result,error); }
            }
        }
    }
    if (!result->tested) return NG_ERROR;
    return NG_OK;
}
