#include "ngene/calibration.h"
#include "ngene/dsp/statistics.h"
#include "platform/platform.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

enum { CAL_PAIRS = 32768, CAL_BLOCK = 32768, CAL_INGRESS_BLOCKS = 128,
       CAL_REPEATS = 3, CAL_HISTOGRAM_LEVELS = 8192 };

static int Cancelled(const TNgGainCalibrationCallbacks *callbacks)
{
    return callbacks && callbacks->cancelled && callbacks->cancelled(callbacks->context);
}

static double Median3(double a, double b, double c)
{
    if (a > b) { double tmp = a; a = b; b = tmp; }
    if (b > c) { double tmp = b; b = c; c = tmp; }
    if (a > b) b = a;
    return b;
}

int NgGainAnalyze(const TNgIqView *view, TNgGainQuality *q, TNgError *error)
{
    double mean[2] = {0}, second[2] = {0}, fourth[2] = {0};
    double limit, variance[2], kurtosis[2], dc, rms, cdfError = 0;
    uint32_t histogram[2][CAL_HISTOGRAM_LEVELS] = {{0}};
    unsigned levels;
    int offset[2];
    int32_t *centered;
    size_t k, clipped = 0;
    if (!view || !view->samples || view->meta.count < 1024 || !q)
        return NgFail(error, "Calibration requires at least 1024 I/Q pairs");
    if (view->format != NG_FORMAT_CI8 && view->format != NG_FORMAT_CU8 &&
        view->format != NG_FORMAT_SC16_Q11)
        return NgFail(error, "Unsupported native ADC format for gain analysis");
    memset(q, 0, sizeof(*q));
    if (view->meta.count > 262144 ||
        (centered = malloc(2 * view->meta.count * sizeof(*centered))) == NULL)
        return NgFail(error, "Cannot allocate bounded calibration DC workspace");
    if (NgIqCenterBlock(view, centered, offset, error) != NG_OK) {
        free(centered);
        return NG_ERROR;
    }
    levels = view->format == NG_FORMAT_SC16_Q11 ? 4096u : 256u;
    limit = (double)levels * 0.5;
    for (k = 0; k < view->meta.count; ++k) {
        unsigned c;
        for (c = 0; c < 2; ++c) {
            int value = centered[2 * k + c];
            int raw = value + offset[c];
            int index = value + (int)levels;
            double x = (double)value / limit;
            mean[c] += x;
            if (fabs((double)raw / limit) >= 0.97) ++clipped;
            if (index < 0) index = 0;
            if ((unsigned)index >= 2 * levels) index = (int)(2 * levels) - 1;
            ++histogram[c][(size_t)index];
        }
    }
    for (k = 0; k < 2; ++k) mean[k] /= (double)view->meta.count;
    for (k = 0; k < view->meta.count; ++k) {
        unsigned c;
        for (c = 0; c < 2; ++c) {
            double d = centered[2 * k + c] / limit - mean[c];
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
    dc = hypot(mean[0] + offset[0] / limit, mean[1] + offset[1] / limit);
    for (k = 0; k < 2; ++k) {
        uint64_t cumulative = 0;
        unsigned bin;
        double invStd;
        if (variance[k] < 1e-12) { cdfError = 1.0; continue; }
        invStd = 1.0 / sqrt(2.0 * variance[k]);
        for (bin = 0; bin < 2 * levels; ++bin) {
            double edge, expected, observed, difference;
            cumulative += histogram[k][bin];
            /* A discrete ADC level occupies a bin centred on the integer. */
            edge = ((double)bin + 0.5 - (double)levels) / limit;
            expected = 0.5 * (1.0 + erf((edge - mean[k]) * invStd));
            observed = (double)cumulative / (double)view->meta.count;
            difference = fabs(observed - expected);
            if (difference > cdfError) cdfError = difference;
        }
    }
    q->rmsFraction = rms;
    q->clippedFraction = (double)clipped / (2.0 * (double)view->meta.count);
    /* Compare the native ADC-level CDF, independently of the display bins. */
    q->gaussianError = (fabs(kurtosis[0] - 3.0) + fabs(kurtosis[1] - 3.0)) * 0.5 +
                       8.0 * cdfError;
    q->dcFraction = dc;
    q->iqImbalance = fabs(sqrt(variance[0]) - sqrt(variance[1])) /
                     (sqrt(variance[0]) + sqrt(variance[1]) + 1e-12);
    if (rms < 0.06) q->rejectionReasons |= NG_GAIN_WEAK;
    if (rms > 0.32 || q->clippedFraction >= 0.001) q->rejectionReasons |= NG_GAIN_OVERLOAD;
    if (q->gaussianError >= 0.9) q->rejectionReasons |= NG_GAIN_DISTRIBUTION;
    if (q->iqImbalance >= 0.25) q->rejectionReasons |= NG_GAIN_IMBALANCE;
    /* A normal receiver DC offset is not evidence of overload. Its statistical
     * significance increases with capture length even when ADC headroom does not
     * change. Actual rail occupancy and the centered distribution guard overload. */
    q->acceptable = q->rejectionReasons == 0;
    q->score = fabs(log((rms + 1e-12) / 0.12)) + 2.0 * q->gaussianError +
               8.0 * q->iqImbalance +
               1.5 * dc / (rms + 1e-12) +
               100.0 * q->clippedFraction;
    free(centered);
    return NG_OK;
}

static int Measure(const TNgConfig *candidate, TNgGainQuality q[CAL_REPEATS],
                   const TNgGainCalibrationCallbacks *callbacks, TNgError *error)
{
    TNgConfig local = *candidate;
    TNgFrontend frontend = {0};
    TNgIqView view = {0};
    TNgIqView collected = {0};
    unsigned char *samples = NULL;
    size_t bytes, count, discard;
    double deadline;
    int started = 0, ok = NG_ERROR;
    unsigned repeat;
    local.windowSamples = CAL_BLOCK;
    /* 128 x 32768 pairs provide ~262 ms at 16 Msps; the former 32 x 8192
     * probe queue provided only 16 ms of scheduling headroom. */
    local.ingressBlocks = CAL_INGRESS_BLOCKS;
    local.ringBlocks = 2;
    local.overflowPolicy = NG_OVERFLOW_STOP;
    if (Cancelled(callbacks)) return NG_END;
    if (NgFrontendOpenNative(&local, &frontend, error) != NG_OK) return NG_ERROR;
    bytes = NgSampleBytes(frontend.info.format);
    if (!bytes || (samples = malloc(CAL_REPEATS * CAL_PAIRS * bytes)) == NULL) {
        NgFail(error, "Cannot allocate calibration sample buffer");
        goto done;
    }
    if (frontend.ops->start(frontend.context, error) != NG_OK) goto done;
    started = 1;
    /* The RTL tuner controls its own gain in automatic mode and needs longer
     * to settle than a fixed analog gain before we judge its distribution. */
    collected.samples = (const TNgIq8 *)samples;
    collected.format = frontend.info.format;
    collected.meta.count = CAL_PAIRS;
    for (repeat = 0; repeat < (unsigned)CAL_REPEATS; ++repeat) {
        count = 0;
        discard = frontend.info.sampleRateHz /
                  (candidate->frontend == NG_FRONTEND_RTLSDR && candidate->rtlAutoGain &&
                   repeat == 0 ? 10u : 50u);
        deadline = NgMonotonicSeconds() + 4.0;
        while (count < CAL_PAIRS && NgMonotonicSeconds() < deadline) {
            if (Cancelled(callbacks)) { ok = NG_END; goto done; }
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
            if (n) memcpy(samples + (repeat * CAL_PAIRS + count) * bytes,
                          (const unsigned char *)view.samples + offset * bytes, n * bytes);
            count += n;
            frontend.ops->release(frontend.context);
        }
        if (count < CAL_PAIRS) {
            NgFail(error, "SDR did not deliver enough I/Q samples for calibration");
            goto done;
        }
    }
    /* Drain and copy all windows before doing statistical work. Otherwise USB
     * continues filling ingress during the native-level CDF/moment analysis. */
    if (frontend.ops->stop(frontend.context, error) != NG_OK) goto done;
    started = 0;
    for (repeat = 0; repeat < (unsigned)CAL_REPEATS; ++repeat) {
        if (Cancelled(callbacks)) { ok = NG_END; goto done; }
        collected.samples = (const TNgIq8 *)(samples + repeat * CAL_PAIRS * bytes);
        if (NgGainAnalyze(&collected, &q[repeat], error) != NG_OK) goto done;
    }
    ok = NG_OK;
done:
    if (started) {
        TNgError stopError = {{0}};
        if (frontend.ops->stop(frontend.context, &stopError) != NG_OK) {
            *error = stopError;
            ok = NG_ERROR;
        }
    }
    NgFrontendDestroy(&frontend);
    free(samples);
    return ok;
}

static void Consider(const TNgConfig *candidate, const TNgGainQuality *quality,
                     TNgGainCalibration *result)
{
    if (result->tested == 0 || (quality->acceptable && !result->accepted) ||
        (quality->acceptable == result->accepted && quality->score < result->quality.score)) {
        result->selected = *candidate;
        result->quality = *quality;
        result->accepted = quality->acceptable;
    }
    result->tested += (unsigned)CAL_REPEATS;
    if (quality->rejectionReasons & NG_GAIN_WEAK) ++result->rejectedWeak;
    if (quality->rejectionReasons & NG_GAIN_OVERLOAD) ++result->rejectedOverload;
    if (quality->rejectionReasons & NG_GAIN_DISTRIBUTION) ++result->rejectedDistribution;
    if (quality->rejectionReasons & NG_GAIN_IMBALANCE) ++result->rejectedImbalance;
}

static int Try(const TNgConfig *candidate, TNgGainCalibration *result, int *overloaded,
               const TNgGainCalibrationCallbacks *callbacks, TNgError *error)
{
    TNgGainQuality measurements[CAL_REPEATS], quality;
    unsigned accepted = 0, i;
    int rc;
    if (Cancelled(callbacks)) return NG_END;
    if (callbacks && callbacks->progress) callbacks->progress(callbacks->context, candidate, result);
    rc = Measure(candidate, measurements, callbacks, error);
    if (rc != NG_OK) return rc;
    quality.rejectionReasons = 0;
    for (i = 0; i < (unsigned)CAL_REPEATS; ++i)
    {
        accepted += (unsigned)(measurements[i].acceptable != 0);
        quality.rejectionReasons |= measurements[i].rejectionReasons;
    }
    quality.rmsFraction = Median3(measurements[0].rmsFraction,
                                  measurements[1].rmsFraction,
                                  measurements[2].rmsFraction);
    quality.clippedFraction = Median3(measurements[0].clippedFraction,
                                      measurements[1].clippedFraction,
                                      measurements[2].clippedFraction);
    quality.gaussianError = Median3(measurements[0].gaussianError,
                                    measurements[1].gaussianError,
                                    measurements[2].gaussianError);
    quality.dcFraction = Median3(measurements[0].dcFraction,
                                 measurements[1].dcFraction,
                                 measurements[2].dcFraction);
    quality.iqImbalance = Median3(measurements[0].iqImbalance,
                                  measurements[1].iqImbalance,
                                  measurements[2].iqImbalance);
    quality.score = Median3(measurements[0].score,
                            measurements[1].score,
                            measurements[2].score);
    if (overloaded) {
        *overloaded = 0;
        for (i = 0; i < (unsigned)CAL_REPEATS; ++i)
            if (measurements[i].clippedFraction >= 0.001 || measurements[i].rmsFraction > 0.32)
                *overloaded = 1;
    }
    /* A clipped window must never be hidden by the median. */
    quality.acceptable = accepted == CAL_REPEATS;
    Consider(candidate, &quality, result);
    if (callbacks && callbacks->progress) callbacks->progress(callbacks->context, candidate, result);
    return NG_OK;
}

int NgGainCalibrateControlled(const TNgConfig *config, TNgGainCalibration *result,
                              const TNgGainCalibrationCallbacks *callbacks, TNgError *error)
{
    TNgConfig candidate;
    if (!config || !result || (config->frontend == NG_FRONTEND_FILE || config->frontend == NG_FRONTEND_SIGE) ||
        !NgFrontendAvailable(config->frontend))
        return NgFail(error, "Select an available live SDR for gain calibration");
    memset(result, 0, sizeof(*result));
    result->selected = *config;
    if (NgFrontendCapabilities(config, &result->capabilities, error) != NG_OK) return NG_ERROR;
    if (config->frontend == NG_FRONTEND_HACKRF) {
        unsigned lna, vga;
        int amp;
        /* Sweep from low gain and stop each VGA branch at overload. */
        for (amp = 0; amp <= 1; ++amp) {
            for (lna = 0; lna <= 40u; lna += 8u) {
                for (vga = 0; vga <= 62u; vga += 2u) {
                    int overloaded;
                    candidate = *config;
                    candidate.lnaGainDb = lna;
                    candidate.vgaGainDb = vga;
                    candidate.rfAmp = amp;
                    int rc = Try(&candidate, result, &overloaded, callbacks, error);
                    if (rc != NG_OK) return rc;
                    if (overloaded) break;
                }
            }
        }
    } else if (config->frontend == NG_FRONTEND_RTLSDR) {
        size_t count = result->capabilities.tunerGainCount;
        size_t i;
        candidate = *config;
        candidate.rtlAutoGain = 1;
        int rc = Try(&candidate, result, NULL, callbacks, error);
        if (rc != NG_OK) return rc;
        for (i = 0; i < count; ++i) {
            candidate = *config;
            candidate.rtlAutoGain = 0;
            candidate.rtlGainTenthsDb = result->capabilities.tunerGains[i];
            rc = Try(&candidate, result, NULL, callbacks, error);
            if (rc != NG_OK) return rc;
        }
    } else if (config->frontend == NG_FRONTEND_BLADERF) {
        int min = result->capabilities.gainMinDb, max = result->capabilities.gainMaxDb;
        int step = result->capabilities.gainStepDb;
        int64_t gain;
        if (step <= 0 || max < min) return NgFail(error, "bladeRF reports invalid gain limits");
        for (gain = min; gain <= max; gain += step) {
            candidate = *config;
            candidate.bladeGainDb = (int)gain;
            int rc = Try(&candidate, result, NULL, callbacks, error);
            if (rc != NG_OK) return rc;
        }
    }
    if (!result->tested) return NG_ERROR;
    return NG_OK;
}

int NgGainCalibrate(const TNgConfig *config, TNgGainCalibration *result, TNgError *error)
{
    return NgGainCalibrateControlled(config, result, NULL, error);
}
