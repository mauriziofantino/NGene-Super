#include "ngene/calibration.h"
#include "ngene/dsp/statistics.h"
#include "frontend/calibration_frontend.h"
#include "platform/platform.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    NG_CAL_PAIRS = 32768,
    NG_CAL_BLOCK = 32768,
    NG_CAL_INGRESS_BLOCKS = 128,
    NG_CAL_REPEATS = 3,
    NG_CAL_HISTOGRAM_LEVELS = 8192,
    NG_CAL_GAIN_GROUPS = 197
};

/** @brief Preferred measured split for one RF-amp/physical-total-gain group. */
typedef struct {
    int valid, earlyDb;
    unsigned lna, vga;
    int rtlGain, rtlIfGain;
    TNgGainQuality quality;
} TNgGainSplit;

/** @brief Bounded worker-owned sweep state; no full configuration copies per point. */
typedef struct {
    TNgGainSplit groups[2][NG_CAL_GAIN_GROUPS];
    unsigned char seen[2][41][64];
} TNgGainSweep;

/** @brief Map legacy requests to physical relative gain states in firmware v2026.01.3.
 * Values are relative to each RFIC's lowest LNA state, not antenna gain or measured NF.
 * MAX2839 compensation is part of this mapping; MAX2831 legacy aliases collapse here.
 */
static void HackrfPhysicalGains(TNgGainPolicy policy, unsigned lna, unsigned vga, int *early,
                                int *late)
{
    int front = (int)lna, back = (int)vga;
    if (policy == NG_GAIN_POLICY_HACKRF_MAX2831) {
        front = lna < 16 ? 0 : lna < 32 ? 17 : 33;
    } else if (policy == NG_GAIN_POLICY_HACKRF_MAX2839) {
        back += 3;
        if (back > 63) {
            if (front < 40) {
                front += 8;
                back -= 8;
            } else {
                back = 63;
            }
        }
        if (front == 0) {
            front = 8;
            back = back < 8 ? 0 : back - 8;
        }
        if (front == 16) {
            int transfer = back > 32 ? 8 : -8;
            front += transfer;
            back -= transfer;
        }
        front -= 8;
    }
    *early = front;
    *late = back;
}

static int Cancelled(const TNgGainCalibrationCallbacks *callbacks)
{
    return callbacks && callbacks->cancelled && callbacks->cancelled(callbacks->context);
}

static double Median3(double firstValue, double middleValue, double lastValue)
{
    if (firstValue > middleValue) {
        double previousValue = firstValue;
        firstValue = middleValue;
        middleValue = previousValue;
    }
    if (middleValue > lastValue) {
        double previousValue = middleValue;
        middleValue = lastValue;
        lastValue = previousValue;
    }
    if (firstValue > middleValue) {
        middleValue = firstValue;
    }
    return middleValue;
}

/** Estimate ADC loading on native quantizer levels.
 *
 * Let \f$x_{n,c}\f$ be a centered sample normalized by the ADC half scale,
 * with \f$c\in\{I,Q\}\f$. The moment estimates and pooled AC RMS are
 * \f[
 * \sigma_c^2 = \frac{1}{N}\sum_n (x_{n,c}-\bar{x}_c)^2,\qquad
 * v_{\mathrm{rms}} = \sqrt{(\sigma_I^2+\sigma_Q^2)/2}.
 * \f]
 * Compare the empirical CDF with a fitted normal CDF at the upper edge of
 * each quantizer bin. The largest discrepancy over both components and the
 * two deviations from Gaussian kurtosis 3 determine the distribution penalty.
 * Measure clipping before DC subtraction, since the rails apply to the raw ADC.
 * A valid measurement can reject a gain without making the analysis call fail.
 */
int NgGainAnalyze(const TNgIqView *view, TNgGainQuality *quality, TNgError *error)
{
    double mean[2] = {0}, second[2] = {0}, fourth[2] = {0};
    double limit, variance[2], kurtosis[2], dc, rms, cdfError = 0;
    uint32_t histogram[2][NG_CAL_HISTOGRAM_LEVELS] = {{0}};
    unsigned levels;
    int offset[2];
    int32_t *centered;
    size_t sampleIndex;
    size_t componentIndex;
    size_t clipped = 0;
    if (!view || !view->samples || view->meta.count < 1024 || !quality) {
        return NgFail(error, "Calibration requires at least 1024 I/Q pairs");
    }
    if (view->format != NG_FORMAT_CI8 && view->format != NG_FORMAT_CU8 &&
        view->format != NG_FORMAT_SC16_Q11) {
        return NgFail(error, "Unsupported native ADC format for gain analysis");
    }
    memset(quality, 0, sizeof(*quality));
    if (view->meta.count > 262144 ||
        (centered = malloc(2 * view->meta.count * sizeof(*centered))) == NULL) {
        return NgFail(error, "Cannot allocate bounded calibration DC workspace");
    }
    if (NgIqCenterBlock(view, centered, offset, error) != NG_OK) {
        free(centered);
        return NG_ERROR;
    }
    levels = view->format == NG_FORMAT_SC16_Q11 ? 4096u : 256u;
    limit = (double)levels * 0.5;
    for (sampleIndex = 0; sampleIndex < view->meta.count; ++sampleIndex) {
        unsigned iqComponent;
        for (iqComponent = 0; iqComponent < 2; ++iqComponent) {
            int value = centered[2 * sampleIndex + iqComponent];
            int raw = value + offset[iqComponent];
            int index = value + (int)levels;
            double normalizedSample = (double)value / limit;
            mean[iqComponent] += normalizedSample;
            if (fabs((double)raw / limit) >= 0.97) {
                ++clipped;
            }
            if (index < 0) {
                index = 0;
            }
            if ((unsigned)index >= 2 * levels) {
                index = (int)(2 * levels) - 1;
            }
            ++histogram[iqComponent][(size_t)index];
        }
    }
    for (componentIndex = 0; componentIndex < 2; ++componentIndex) {
        mean[componentIndex] /= (double)view->meta.count;
    }
    for (sampleIndex = 0; sampleIndex < view->meta.count; ++sampleIndex) {
        unsigned iqComponent;
        for (iqComponent = 0; iqComponent < 2; ++iqComponent) {
            double centeredDeviation =
                centered[2 * sampleIndex + iqComponent] / limit - mean[iqComponent];
            double squaredDeviation = centeredDeviation * centeredDeviation;
            second[iqComponent] += squaredDeviation;
            fourth[iqComponent] += squaredDeviation * squaredDeviation;
        }
    }
    for (componentIndex = 0; componentIndex < 2; ++componentIndex) {
        variance[componentIndex] = second[componentIndex] / (double)view->meta.count;
        kurtosis[componentIndex] = variance[componentIndex] > 1e-12
                                       ? fourth[componentIndex] / (double)view->meta.count /
                                             (variance[componentIndex] * variance[componentIndex])
                                       : 0;
    }
    rms = sqrt((variance[0] + variance[1]) * 0.5);
    dc = hypot(mean[0] + offset[0] / limit, mean[1] + offset[1] / limit);
    for (componentIndex = 0; componentIndex < 2; ++componentIndex) {
        uint64_t cumulative = 0;
        unsigned bin;
        double invStd;
        if (variance[componentIndex] < 1e-12) {
            cdfError = 1.0;
            continue;
        }
        invStd = 1.0 / sqrt(2.0 * variance[componentIndex]);
        for (bin = 0; bin < 2 * levels; ++bin) {
            double edge, expected, observed, difference;
            cumulative += histogram[componentIndex][bin];
            /* A discrete ADC level occupies a bin centred on the integer. */
            edge = ((double)bin + 0.5 - (double)levels) / limit;
            expected = 0.5 * (1.0 + erf((edge - mean[componentIndex]) * invStd));
            observed = (double)cumulative / (double)view->meta.count;
            difference = fabs(observed - expected);
            if (difference > cdfError) {
                cdfError = difference;
            }
        }
    }
    quality->rmsFraction = rms;
    quality->clippedFraction = (double)clipped / (2.0 * (double)view->meta.count);
    /* Compare the native ADC-level CDF, independently of the display bins. */
    quality->gaussianError =
        (fabs(kurtosis[0] - 3.0) + fabs(kurtosis[1] - 3.0)) * 0.5 + 8.0 * cdfError;
    quality->dcFraction = dc;
    quality->iqImbalance = fabs(sqrt(variance[0]) - sqrt(variance[1])) /
                           (sqrt(variance[0]) + sqrt(variance[1]) + 1e-12);
    if (rms < 0.06) {
        quality->rejectionReasons |= NG_GAIN_WEAK;
    }
    if (rms > 0.32 || quality->clippedFraction >= 0.001) {
        quality->rejectionReasons |= NG_GAIN_OVERLOAD;
    }
    if (quality->gaussianError >= 0.9) {
        quality->rejectionReasons |= NG_GAIN_DISTRIBUTION;
    }
    if (quality->iqImbalance >= 0.25) {
        quality->rejectionReasons |= NG_GAIN_IMBALANCE;
    }
    /* A normal receiver DC offset is not evidence of overload. Its statistical
     * significance increases with capture length even when ADC headroom does not
     * change. Actual rail occupancy and the centered distribution guard overload. */
    quality->acceptable = quality->rejectionReasons == 0;
    quality->score = fabs(log((rms + 1e-12) / 0.12)) + 2.0 * quality->gaussianError +
                     8.0 * quality->iqImbalance + 1.5 * dc / (rms + 1e-12) +
                     100.0 * quality->clippedFraction;
    free(centered);
    return NG_OK;
}

static int Measure(const TNgConfig *candidate, TNgGainQuality measurements[NG_CAL_REPEATS],
                   const TNgGainCalibrationCallbacks *callbacks, TNgFrontend *retained,
                   TNgError *error)
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
    local.windowSamples = NG_CAL_BLOCK;
    /* 128 x 32768 pairs provide ~262 ms at 16 Msps; the former 32 x 8192
     * probe queue provided only 16 ms of scheduling headroom. */
    local.ingressBlocks = NG_CAL_INGRESS_BLOCKS;
    local.ringBlocks = 2;
    local.overflowPolicy = NG_OVERFLOW_STOP;
    if (Cancelled(callbacks)) {
        return NG_END;
    }
    if (retained && retained->ops) {
        frontend = *retained;
    } else if (NgFrontendOpenNative(&local, &frontend, error) != NG_OK) {
        return NG_ERROR;
    }
    if (retained && NgFrontendCalibrationNext(&local, &frontend, error) != NG_OK) {
        goto done;
    }
    if (Cancelled(callbacks)) {
        ok = NG_END;
        goto done;
    }
    bytes = NgSampleBytes(frontend.info.format);
    if (!bytes || (samples = malloc(NG_CAL_REPEATS * NG_CAL_PAIRS * bytes)) == NULL) {
        NgFail(error, "Cannot allocate calibration sample buffer");
        goto done;
    }
    if (frontend.ops->start(frontend.context, error) != NG_OK) {
        goto done;
    }
    started = 1;
    /* The RTL tuner controls its own gain in automatic mode and needs longer
     * to settle than a fixed analog gain before we judge its distribution. */
    collected.samples = (const TNgIq8 *)samples;
    collected.format = frontend.info.format;
    collected.meta.count = NG_CAL_PAIRS;
    for (repeat = 0; repeat < (unsigned)NG_CAL_REPEATS; ++repeat) {
        count = 0;
        discard = frontend.info.sampleRateHz / (candidate->frontend == NG_FRONTEND_RTLSDR &&
                                                        candidate->rtlAutoGain && repeat == 0
                                                    ? 10u
                                                    : 50u);
        /* RF/IF gains do not change between the three non-overlapping windows.
         * RTL settling is needed once after a gain change, not before each window. */
        if (candidate->frontend == NG_FRONTEND_RTLSDR && repeat != 0) {
            discard = 0;
        }
        if (retained && repeat == 0) {
            discard += (size_t)NG_CAL_RTL_BUFFERS * NG_CAL_RTL_BUFFER_PAIRS;
        }
        deadline = NgMonotonicSeconds() + 4.0;
        while (count < NG_CAL_PAIRS && NgMonotonicSeconds() < deadline) {
            if (Cancelled(callbacks)) {
                ok = NG_END;
                goto done;
            }
            int rc = frontend.ops->acquire(frontend.context, &view, 50, error);
            size_t offset = 0, copiedPairs;
            if (rc == NG_TIMEOUT) {
                continue;
            }
            if (rc != NG_OK) {
                goto done;
            }
            if (discard) {
                offset = view.meta.count < discard ? view.meta.count : discard;
                discard -= offset;
            }
            copiedPairs = view.meta.count - offset;
            if (copiedPairs > NG_CAL_PAIRS - count) {
                copiedPairs = NG_CAL_PAIRS - count;
            }
            if (copiedPairs) {
                memcpy(samples + (repeat * NG_CAL_PAIRS + count) * bytes,
                       (const unsigned char *)view.samples + offset * bytes, copiedPairs * bytes);
            }
            count += copiedPairs;
            frontend.ops->release(frontend.context);
        }
        if (count < NG_CAL_PAIRS) {
            NgFail(error, "SDR did not deliver enough I/Q samples for calibration");
            goto done;
        }
    }
    /* Drain and copy all windows before doing statistical work. Otherwise USB
     * continues filling ingress during the native-level CDF/moment analysis. */
    if (retained) {
        if (NgFrontendCalibrationPause(&frontend, error) != NG_OK) {
            goto done;
        }
    } else if (frontend.ops->stop(frontend.context, error) != NG_OK) {
        goto done;
    }
    started = 0;
    for (repeat = 0; repeat < (unsigned)NG_CAL_REPEATS; ++repeat) {
        if (Cancelled(callbacks)) {
            ok = NG_END;
            goto done;
        }
        collected.samples = (const TNgIq8 *)(samples + repeat * NG_CAL_PAIRS * bytes);
        if (NgGainAnalyze(&collected, &measurements[repeat], error) != NG_OK) {
            goto done;
        }
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
    if (retained && ok == NG_OK) {
        *retained = frontend;
    } else {
        NgFrontendDestroy(&frontend);
        if (retained) {
            memset(retained, 0, sizeof(*retained));
        }
    }
    free(samples);
    return ok;
}

static void Consider(const TNgConfig *candidate, const TNgGainQuality *quality,
                     TNgGainCalibration *result, TNgGainSweep *sweep)
{
    if (sweep) {
        int early, late, amp, total, selected = 0, selectedEarly = -1;
        int e4000 = result->capabilities.gainPolicy == NG_GAIN_POLICY_RTL_E4000;
        TNgGainSplit *split;
        if (e4000) {
            early = candidate->rtlGainTenthsDb;
            late = candidate->rtlIfGainDb * 10;
            amp = 0;
            total = (early + late) / 5; /* E4000 totals are on a 0.5 dB grid. */
        } else {
            HackrfPhysicalGains(result->capabilities.gainPolicy, candidate->lnaGainDb,
                                candidate->vgaGainDb, &early, &late);
            amp = candidate->rfAmp;
            total = early + late;
        }
        split = &sweep->groups[amp][total];
        if (!split->valid || quality->acceptable > split->quality.acceptable ||
            (quality->acceptable == split->quality.acceptable &&
             ((quality->acceptable && early > split->earlyDb) ||
              ((!quality->acceptable || early == split->earlyDb) &&
               quality->score < split->quality.score)))) {
            split->valid = 1;
            split->earlyDb = early;
            split->lna = candidate->lnaGainDb;
            split->vga = candidate->vgaGainDb;
            split->rtlGain = candidate->rtlGainTenthsDb;
            split->rtlIfGain = candidate->rtlIfGainDb;
            split->quality = *quality;
        }
        /* Recompute after replacing a representative: its score can increase.
         * A pairwise comparator mixing split priority and score is not transitive. */
        for (amp = 0; amp < 2; ++amp) {
            for (total = 0; total < NG_CAL_GAIN_GROUPS; ++total) {
                split = &sweep->groups[amp][total];
                if (split->valid &&
                    (!selected || split->quality.acceptable > result->accepted ||
                     (split->quality.acceptable == result->accepted &&
                      ((!e4000 && split->quality.acceptable && split->earlyDb > selectedEarly) ||
                       ((e4000 || !split->quality.acceptable || split->earlyDb == selectedEarly) &&
                        split->quality.score < result->quality.score))))) {
                    result->selected = *candidate;
                    if (e4000) {
                        result->selected.rtlGainTenthsDb = split->rtlGain;
                        result->selected.rtlIfGainDb = split->rtlIfGain;
                    } else {
                        result->selected.rfAmp = amp;
                        result->selected.lnaGainDb = split->lna;
                        result->selected.vgaGainDb = split->vga;
                    }
                    result->quality = split->quality;
                    result->accepted = split->quality.acceptable;
                    selectedEarly = split->earlyDb;
                    selected = 1;
                }
            }
        }
    } else if (result->tested == 0 || (quality->acceptable && !result->accepted) ||
               (quality->acceptable == result->accepted &&
                quality->score < result->quality.score)) {
        result->selected = *candidate;
        result->quality = *quality;
        result->accepted = quality->acceptable;
    }
    result->tested += (unsigned)NG_CAL_REPEATS;
    if (quality->rejectionReasons & NG_GAIN_WEAK) {
        ++result->rejectedWeak;
    }
    if (quality->rejectionReasons & NG_GAIN_OVERLOAD) {
        ++result->rejectedOverload;
    }
    if (quality->rejectionReasons & NG_GAIN_DISTRIBUTION) {
        ++result->rejectedDistribution;
    }
    if (quality->rejectionReasons & NG_GAIN_IMBALANCE) {
        ++result->rejectedImbalance;
    }
}

static int Try(const TNgConfig *candidate, TNgGainCalibration *result, TNgGainSweep *sweep,
               const TNgGainCalibrationCallbacks *callbacks, TNgFrontend *retained, TNgError *error)
{
    TNgGainQuality measurements[NG_CAL_REPEATS], quality;
    unsigned accepted = 0, measurementIndex;
    int rc;
    if (Cancelled(callbacks)) {
        return NG_END;
    }
    if (callbacks && callbacks->progress) {
        callbacks->progress(callbacks->context, candidate, result);
    }
    rc = Measure(candidate, measurements, callbacks, retained, error);
    if (rc != NG_OK) {
        return rc;
    }
    quality.rejectionReasons = 0;
    for (measurementIndex = 0; measurementIndex < (unsigned)NG_CAL_REPEATS; ++measurementIndex) {
        accepted += (unsigned)(measurements[measurementIndex].acceptable != 0);
        quality.rejectionReasons |= measurements[measurementIndex].rejectionReasons;
    }
    quality.rmsFraction = Median3(measurements[0].rmsFraction, measurements[1].rmsFraction,
                                  measurements[2].rmsFraction);
    quality.clippedFraction =
        Median3(measurements[0].clippedFraction, measurements[1].clippedFraction,
                measurements[2].clippedFraction);
    quality.gaussianError = Median3(measurements[0].gaussianError, measurements[1].gaussianError,
                                    measurements[2].gaussianError);
    quality.dcFraction =
        Median3(measurements[0].dcFraction, measurements[1].dcFraction, measurements[2].dcFraction);
    quality.iqImbalance = Median3(measurements[0].iqImbalance, measurements[1].iqImbalance,
                                  measurements[2].iqImbalance);
    quality.score = Median3(measurements[0].score, measurements[1].score, measurements[2].score);
    /* A clipped window must never be hidden by the median. */
    quality.acceptable = accepted == NG_CAL_REPEATS;
    Consider(candidate, &quality, result, sweep);
    if (callbacks && callbacks->progress) {
        callbacks->progress(callbacks->context, candidate, result);
    }
    return NG_OK;
}

int NgGainCalibrateControlled(const TNgConfig *config, TNgGainCalibration *result,
                              const TNgGainCalibrationCallbacks *callbacks, TNgError *error)
{
    TNgConfig candidate, base;
    if (!config || !result ||
        (config->frontend == NG_FRONTEND_FILE || config->frontend == NG_FRONTEND_SIGE) ||
        !NgFrontendAvailable(config->frontend)) {
        return NgFail(error, "Select an available live SDR for gain calibration");
    }
    memset(result, 0, sizeof(*result));
    result->selected = *config;
    if (Cancelled(callbacks)) {
        return NG_END;
    }
    if (NgFrontendCapabilities(config, &result->capabilities, error) != NG_OK) {
        return NG_ERROR;
    }
    if (Cancelled(callbacks)) {
        return NG_END;
    }
    if (result->capabilities.gainPolicy == NG_GAIN_POLICY_UNAVAILABLE) {
        return NgFail(error, "Gain calibration unavailable for %s",
                      result->capabilities.deviceModel);
    }
    base = *config;
    /* Pin all observations to the same probed device, including an initially empty serial. */
    if (result->capabilities.deviceSerial[0]) {
        snprintf(base.serial, sizeof(base.serial), "%s", result->capabilities.deviceSerial);
    }
    if (config->frontend == NG_FRONTEND_HACKRF) {
        TNgGainSweep sweep = {0};
        unsigned lna, vga;
        int amp;
        if (result->capabilities.gainPolicy < NG_GAIN_POLICY_HACKRF_MAX2837 ||
            result->capabilities.gainPolicy > NG_GAIN_POLICY_HACKRF_MAX2831) {
            return NgFail(error, "Invalid HackRF gain policy");
        }
        /* Exhaust every distinct physical setting; an intermittent overload must
         * not prune unmeasured settings in this or another gain split. */
        for (amp = 0; amp <= 1; ++amp) {
            for (lna = 0; lna <= 40u; lna += 8u) {
                for (vga = 0; vga <= 62u; vga += 2u) {
                    int early, late;
                    HackrfPhysicalGains(result->capabilities.gainPolicy, lna, vga, &early, &late);
                    if (sweep.seen[amp][early][late]) {
                        continue;
                    }
                    sweep.seen[amp][early][late] = 1;
                    candidate = base;
                    candidate.lnaGainDb = lna;
                    candidate.vgaGainDb = vga;
                    candidate.rfAmp = amp;
                    int rc = Try(&candidate, result, &sweep, callbacks, NULL, error);
                    if (rc != NG_OK) {
                        return rc;
                    }
                }
            }
        }
    } else if (config->frontend == NG_FRONTEND_RTLSDR) {
        TNgGainSweep sweep = {0};
        int e4000 = result->capabilities.gainPolicy == NG_GAIN_POLICY_RTL_E4000;
        TNgFrontend retained = {0};
        size_t count = result->capabilities.tunerGainCount;
        size_t gainIndex;
        if (result->capabilities.gainPolicy < NG_GAIN_POLICY_RTL_R82XX ||
            result->capabilities.gainPolicy > NG_GAIN_POLICY_RTL_FC001X) {
            return NgFail(error, "Invalid RTL-SDR gain policy");
        }
        if (!count || count > 64) {
            return NgFail(error, "Invalid native RTL gain list");
        }
        if (e4000) {
            for (gainIndex = 0; gainIndex < count; ++gainIndex) {
                int gain = result->capabilities.tunerGains[gainIndex];
                if (gain < -10 || gain > 420 || gain % 5 != 0) {
                    return NgFail(error, "Unexpected native E4000 gain list");
                }
            }
        }
        for (gainIndex = 0; gainIndex < count; ++gainIndex) {
            size_t earlier;
            int ifGain;
            for (earlier = 0; earlier < gainIndex; ++earlier) {
                if (result->capabilities.tunerGains[earlier] ==
                    result->capabilities.tunerGains[gainIndex]) {
                    break;
                }
            }
            if (earlier < gainIndex) {
                continue;
            }
            candidate = base;
            candidate.rtlAutoGain = 0;
            candidate.rtlGainTenthsDb = result->capabilities.tunerGains[gainIndex];
            /* The E4000 native tuner list controls LNA/mixer only. Exhaust all
             * realizable IF totals too, with earlier IF stages filled first.
             * Other RTL tuners do not implement the public IF-gain operation. */
            for (ifGain = e4000 ? 3 : 0; ifGain <= (e4000 ? 56 : 0); ++ifGain) {
                int rc;
                if (e4000) {
                    candidate.rtlIfGainDb = ifGain;
                }
                rc = Try(&candidate, result, e4000 ? &sweep : NULL, callbacks, &retained, error);
                if (rc != NG_OK) {
                    NgFrontendDestroy(&retained);
                    return rc;
                }
            }
        }
        NgFrontendDestroy(&retained);
    } else if (config->frontend == NG_FRONTEND_BLADERF) {
        int min = result->capabilities.gainMinDb, max = result->capabilities.gainMaxDb;
        int step = result->capabilities.gainStepDb;
        int64_t gain;
        if (result->capabilities.gainPolicy != NG_GAIN_POLICY_BLADERF1 &&
            result->capabilities.gainPolicy != NG_GAIN_POLICY_BLADERF2) {
            return NgFail(error, "Invalid bladeRF gain policy");
        }
        if (step <= 0 || max < min) {
            return NgFail(error, "bladeRF reports invalid gain limits");
        }
        for (gain = min; gain <= max; gain += step) {
            candidate = base;
            candidate.bladeGainDb = (int)gain;
            int rc = Try(&candidate, result, NULL, callbacks, NULL, error);
            if (rc != NG_OK) {
                return rc;
            }
        }
    }
    if (!result->tested) {
        return NG_ERROR;
    }
    return NG_OK;
}

int NgGainCalibrate(const TNgConfig *config, TNgGainCalibration *result, TNgError *error)
{
    return NgGainCalibrateControlled(config, result, NULL, error);
}
