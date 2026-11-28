/** @file tracking.c @brief Classic discriminators/second-order loops with private multi-tap DSP.
 * Algorithm basis: NGene Classic tracking/trackingDiscriminatorsLoops.c,
 * Mario Nicola, 2014. Adapted to native complex samples and shared window epochs.
 */
#include "tracking/egnos.h"
#include "dsp/egnos_code.h"
#include "ngene/beidou_observation.h"
#include "ngene/dsp/gps_ca.h"
#include "ngene/dsp/tracking_replica.h"
#include "acquisition/galileo_codes.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define NG_TRACKING_CHUNK 2048u

/** Private NCOs, integration accumulators and reusable block kernels. */
struct TNgTracking {
    void (*egnosPrompt)(void *, double, uint64_t); /**< Receiver-owned prompt sink. */
    void *egnosContext; /**< Lifetime of the separate GEO tracker. */
    int observeRequested, observeIntegration; /**< Optional tap interest, latched per integration. */
    double offsets[NG_TRACKING_MAX_TAPS]; /**< Configured tap delays, never telemetry-compacted. */
    TNgNavigation navigation;   /**< Per-integration data demodulator; never fed from telemetry. */
    TNgBcnv1Stream bcnv1;
    TNgBcnv1Cache bdt;
    unsigned bdtPeriods, beidouStable;
    uint64_t bcnv1Frames;
    double transmitBoundary;    /**< Transmit time of latest code boundary. */
    uint64_t transmitMilliseconds; /**< Exact elapsed code time since decoded anchor. */
    int transmitTimeValid;      /**< A decoded TOW anchored the code counter. */
    TNgTrackingConfig config;     /**< Frozen arithmetic choices. */
    TNgStreamInfo stream;         /**< Actual opened source geometry. */
    TNgTrackingSnapshot snapshot; /**< Latest completed integration. */
    TNgAcquisitionResult seed;    /**< Capture-referenced initialization. */
    double startPhase;            /**< Code phase at the integration start. */
    double m2, m4, cn0Smooth;     /**< Classic moment-method exponential memories. */
    unsigned cn0Samples;          /**< Valid prompt moments accumulated since seed. */
    double lockI, lockQ;          /**< Classic length-100 PLL power means. */
    unsigned lockAge, failureMs;  /**< Signal-time acquisition/relock deadlines. */
    int qualified;               /**< High PLL metric was established after the transient. */
    double sideI[2], sideQ[2];    /**< Half-chip BOC qualification coherent sums. */
    double peakPower[3];          /**< 100-ms accumulated left/prompt/right powers. */
    unsigned peakMs;              /**< Signal-time age of the qualification sums. */
    unsigned system;              /**< Constellation. */
    unsigned period;              /**< Code length in chips. */
    int started;                  /**< At least one window accepted after seed. */
    uint64_t expected;            /**< Next contiguous sample. */
    uint64_t start;               /**< Current integration origin. */
    size_t accumulated;           /**< Samples accumulated so far. */
    size_t target;                /**< Samples in current coherent integration. */
    int discardFirst;             /**< Discard the first partial primary-code period. */
    double codePhase;             /**< Local code phase in chips. */
    double carrierPhase;          /**< Local carrier phase in cycles. */
    double carrierHz;             /**< IF plus estimated Doppler. */
    double smoothedDopplerHz;     /**< Filtered output; carrier NCO stays untouched. */
    double initialCarrier;        /**< Carrier anchor for Classic loop. */
    double initialCode;           /**< Code-rate anchor for Classic loop. */
    double carrierNco, codeNco, oldCarrierError, oldDllError;      /**< Loop memories. */
    double oldI, oldQ;                                             /**< Previous prompt for FLL. */
    double meanFrequency, oldMean;                                 /**< FLL transition smoothing. */
    double sumI[NG_TRACKING_MAX_TAPS], sumQ[NG_TRACKING_MAX_TAPS]; /**< Coherent sums. */
    int8_t beidouData[2 * NG_BEIDOU_B1C_LENGTH];
    double beidouDataI;
    int8_t chips[2 * NG_BEIDOU_B1C_LENGTH];                                            /**< Primary code. */
    TNgComplex values[NG_TRACKING_CHUNK], carrier[NG_TRACKING_CHUNK]; /**< Block scratch. */
    double prefix[2 * (NG_TRACKING_CHUNK + 1)]; /**< Running (I, Q) sums of mixed samples. */
};

/** Set independent precise defaults. */
void NgTrackingDefaults(TNgTrackingConfig *config)
{
    memset(config, 0, sizeof(*config));
    config->channelCount = 16;
    config->galileoSpacingChips = 0.2;
    config->gpsChannels = 0;
    config->galileoChannels = 0;
    config->autoPriority = 0;
    config->workers = 4;
    config->enabled = 1;
    config->tapCount = 3;
    config->spacingChips = 0.4;
    config->broadcastIonosphere = 1;
    config->dopplerSmoothingMs = 200;
}

/** Validate all configurable arithmetic boundaries. */
int NgTrackingValidate(const TNgTrackingConfig *config, TNgError *error)
{
    if (!config || !config->channelCount || config->channelCount > 64 ||
        config->gpsChannels + config->galileoChannels + config->beidouChannels > config->channelCount ||
        !isfinite(config->galileoSpacingChips) || config->galileoSpacingChips <= 0 ||
        config->galileoSpacingChips >= 1.0 / 3.0 || config->gpsChannels > 32 ||
        !isfinite(config->beidouSpacingChips) || config->beidouSpacingChips < 0 ||
        config->beidouSpacingChips >= 1.0 / 3.0 ||
        config->galileoChannels > 32 || config->beidouChannels > NG_SATELLITE_MAX_PRN ||
        config->autoPriority > NG_CONSTELLATION_COUNT ||
        !config->workers || config->workers > 16 ||
        (config->enabled != 0 && config->enabled != 1) ||
        (config->squareCarrier != 0 && config->squareCarrier != 1) ||
        (config->xorCorrelation != 0 && config->xorCorrelation != 1) ||
        (config->fastCarrier != 0 && config->fastCarrier != 1) ||
        (config->mixerBits != 0 && config->mixerBits != 4 && config->mixerBits != 8 &&
         config->mixerBits != 16) || config->dopplerSmoothingMs > 10000 || config->tapCount < 3 ||
        config->tapCount > NG_TRACKING_MAX_TAPS || !(config->tapCount & 1u) ||
        !isfinite(config->spacingChips) || config->spacingChips <= 0 || config->spacingChips > 1)
        return NgFail(error, "Invalid tracking modes or multi-correlator geometry");
    return NG_OK;
}

/** Construct private, bounded native-rate tracking. */
int NgTrackingCreate(const TNgTrackingConfig *config, const TNgStreamInfo *stream,
                     unsigned constellation, TNgTracking **out, TNgError *error)
{
    TNgTracking *tracking;
    if (!out)
        return NgFail(error, "Missing tracking destination");
    *out = NULL;
    if (NgTrackingValidate(config, error) != NG_OK)
        return NG_ERROR;
    if (!stream || stream->sampleRateHz < 2048000 || stream->sampleRateHz > 20000000 ||
        (constellation < 1 || constellation > NG_CONSTELLATION_COUNT))
        return NgFail(error, "Unsupported tracking stream or constellation");
    if (config->mixerBits != 0 && stream->format != NG_FORMAT_CI8)
        return NgFail(error, "Byte arithmetic requires a CI8 common stream");
    tracking = calloc(1, sizeof(*tracking));
    if (!tracking)
        return NgFail(error, "Cannot allocate tracking workspace");
    tracking->config = *config;
    tracking->observeRequested = 1;
    tracking->stream = *stream;
    tracking->system = constellation;
    tracking->period = constellation == 1 ? 1023u : constellation == 2 ? 4092u : NG_BEIDOU_B1C_LENGTH;
    *out = tracking;
    return NG_OK;
}

/** Reuse a slot's private workspace for a supported primary code without allocating. */
int NgTrackingSetSignal(TNgTracking *tracking, unsigned constellation, TNgError *error)
{
    if (!tracking || (constellation < 1 || constellation > NG_CONSTELLATION_COUNT))
        return NgFail(error, "Invalid tracking signal assignment");
    tracking->system = constellation;
    tracking->period = constellation == 1 ? 1023u : constellation == 2 ? 4092u : NG_BEIDOU_B1C_LENGTH;
    return NG_OK;
}

/** Reset every integration and loop memory when a new acquisition is accepted. */
int NgTrackingSeed(TNgTracking *tracking, const TNgAcquisitionResult *result, TNgError *error)
{
    unsigned k;
    if (!tracking || !result || !result->confirmed || !NgSatellitePrnValid((TNgConstellation)tracking->system, result->prn) ||
        !isfinite(result->dopplerHz) || !isfinite(result->codePhaseChips))
        return NgFail(error, "Tracking requires a finite confirmed acquisition seed");
    memset(&tracking->snapshot, 0, sizeof(tracking->snapshot));
    tracking->m2 = tracking->m4 = tracking->cn0Smooth = 0;
    tracking->cn0Samples = 0;
    tracking->lockI = tracking->lockQ = 0;
    tracking->lockAge = tracking->failureMs = 0;
    tracking->qualified = 0;
    tracking->peakMs = 0;
    memset(tracking->peakPower, 0, sizeof(tracking->peakPower));
    memset(tracking->sideI, 0, sizeof(tracking->sideI));
    memset(tracking->sideQ, 0, sizeof(tracking->sideQ));
    tracking->egnosPrompt = NULL;
    tracking->seed = *result;
    NgNavigationReset(&tracking->navigation);
    memset(&tracking->bcnv1, 0, sizeof(tracking->bcnv1));
    memset(&tracking->bdt, 0, sizeof(tracking->bdt));
    tracking->bdtPeriods = tracking->beidouStable = 0;
    tracking->beidouDataI = 0;
    tracking->bcnv1Frames = 0;
    tracking->transmitTimeValid = 0;
    tracking->started = 0;
    tracking->accumulated = 0;
    tracking->discardFirst = 1;
    tracking->carrierNco = tracking->codeNco = 0;
    tracking->oldCarrierError = tracking->oldDllError = 0;
    tracking->oldI = tracking->oldQ = 0;
    tracking->carrierPhase = 0;
    tracking->initialCarrier = (double)tracking->stream.digitalIfHz + result->dopplerHz;
    tracking->carrierHz = tracking->initialCarrier;
    tracking->smoothedDopplerHz = result->dopplerHz;
    tracking->meanFrequency = tracking->initialCarrier;
    tracking->oldMean = 0;
    tracking->initialCode = 1023000.0 * (1.0 + result->dopplerHz / 1575420000.0);
    tracking->snapshot.codeRateHz = tracking->initialCode;
    tracking->snapshot.active = 1;
    tracking->snapshot.prn = result->prn;
    tracking->snapshot.constellation = tracking->system;
    tracking->snapshot.epoch = result->epoch;
    tracking->snapshot.tapCount = tracking->config.tapCount;
    double spacing = tracking->system == NG_CONSTELLATION_GPS ? tracking->config.spacingChips :
                     tracking->config.galileoSpacingChips;
    if (tracking->system == NG_CONSTELLATION_BEIDOU && tracking->config.beidouSpacingChips > 0)
        spacing = tracking->config.beidouSpacingChips;
    for (k = 0; k < tracking->config.tapCount; ++k)
        tracking->snapshot.offset[k] = tracking->offsets[k] =
            ((double)k - tracking->config.tapCount / 2u) * spacing;
    memset(tracking->sumI, 0, sizeof(tracking->sumI));
    memset(tracking->sumQ, 0, sizeof(tracking->sumQ));
    if (tracking->system == 1)
        return NgGpsCaGenerate(result->prn, tracking->chips, error);
    if (tracking->system == NG_CONSTELLATION_BEIDOU) {
        int8_t primary[NG_BEIDOU_B1C_LENGTH];
        if (NgBeidouB1cGenerate(result->prn, NG_BEIDOU_B1C_DATA, primary, error) != NG_OK)
            return NG_ERROR;
        for (k = 0; k < NG_BEIDOU_B1C_LENGTH; ++k) {
            tracking->beidouData[2 * k] = primary[k];
            tracking->beidouData[2 * k + 1] = (int8_t)-primary[k];
        }
        if (NgBeidouB1cGenerate(result->prn, NG_BEIDOU_B1C_PILOT, primary, error) != NG_OK)
            return NG_ERROR;
        for (k = 0; k < NG_BEIDOU_B1C_LENGTH; ++k) {
            tracking->chips[2 * k] = primary[k];
            tracking->chips[2 * k + 1] = (int8_t)-primary[k];
        }
        return NG_OK;
    }
    for (k = 0; k < 4092; ++k) {
        int8_t sign = (ngGalileoCodes[result->prn - 1][k / 8] & (1u << (7u - k % 8u))) ? -1 : 1;
        tracking->chips[2 * k] = sign;
        tracking->chips[2 * k + 1] = (int8_t)-sign;
    }
    return NG_OK;
}

/** Classic second-order recurrence; coefficients retain zeta=1/sqrt(2), wn=B/0.53. */
static double Loop(double previous, double error, double oldError, double bandwidth, double gain,
                   double delta)
{
    double wn = bandwidth / 0.53;
    double t1 = gain / (wn * wn);
    double t2 = sqrt(2.0) / wn;
    return previous + t2 / t1 * (error - oldError) + delta / t1 * error;
}

/** Classic M2/M4 moment estimator, adapted to a full precision complex carrier.
 * Unlike the packed IF path, no fixed carrier quantization correction is added.
 * This observation never changes acquisition thresholds or certifies navigation lock. */
static void EstimateCn0(TNgTracking *t, double i, double q, double duration)
{
    double power = i * i + q * q;
    t->snapshot.cn0Valid = 0;
    if (!t->cn0Samples) {
        t->m2 = power;
        t->m4 = power * power;
    } else {
        t->m2 += (power - t->m2) / 100.0;
        t->m4 += (power * power - t->m4) / 100.0;
    }
    ++t->cn0Samples;
    double squareSignal = 2 * t->m2 * t->m2 - t->m4;
    if (t->cn0Samples >= 100 && squareSignal > 0 && duration > 0 && !t->config.squareCarrier) {
        double signal = sqrt(squareSignal), noise = t->m2 - signal;
        if (noise > 0 && signal > 0) {
            double estimate = 10 * log10(signal / noise / duration);
            if (isfinite(estimate)) {
                t->cn0Smooth =
                    t->cn0Smooth == 0 ? estimate : t->cn0Smooth + (estimate - t->cn0Smooth) / 100.0;
                t->snapshot.cn0DbHz = t->cn0Smooth;
                t->snapshot.cn0Valid = 1;
            }
        }
    }
}

/** Observe half-chip BOC probes over 100 ms. Classic's side-peak correction
 * helper is not called by its active tracking path. These observations therefore
 * qualify the acquisition handoff without changing code phase or search decisions. */
static void QualifyGalileoPeak(TNgTracking *t, double i, double q)
{
    unsigned side;
    for (side = 0; side < 2; ++side)
        t->peakPower[side == 0 ? 0 : 2] +=
            t->sideI[side] * t->sideI[side] + t->sideQ[side] * t->sideQ[side];
    t->peakPower[1] += i * i + q * q;
    t->peakMs += 4;
    memset(t->sideI, 0, sizeof(t->sideI));
    memset(t->sideQ, 0, sizeof(t->sideQ));
    if (t->peakMs < 100)
        return;
    double left = t->peakPower[0], prompt = t->peakPower[1], right = t->peakPower[2];
    t->snapshot.mainPeakVerified = prompt > left && prompt > right;
    memset(t->peakPower, 0, sizeof(t->peakPower));
    t->peakMs = 0;
}

/** Convert a complete B1C data prompt to a symbol and update BDT observations. */
static void BeidouPrompt(TNgTracking *t, double prompt)
{
    TNgBcnv1Frame frame;
    uint64_t firstSample;
    double scale = t->m2 > 0 ? 32.0 / sqrt(t->m2) : 0;
    int8_t symbol = (int8_t)lround(fmax(-127, fmin(127, scale * prompt)));
    if (!t->snapshot.pll)
        return;
    if (t->bdt.prn)
        ++t->bdtPeriods;
    if (NgBcnv1Push(&t->bcnv1, t->seed.prn, t->seed.epoch, t->start,
                    t->expected, symbol, &frame, &firstSample) == NG_OK &&
        NgBcnv1Accept(&t->bdt, &frame, t->seed.epoch, firstSample) == NG_OK) {
        t->bdtPeriods = t->bcnv1.elapsedSymbols;
        ++t->bcnv1Frames;
    }
    t->snapshot.navigation.towValid = 0;
    if (t->bdt.prn) {
        double chips = (double)t->bdtPeriods * NG_BEIDOU_B1C_LENGTH + t->codePhase;
        NgBcnv1Navigation(&t->bdt, t->seed.epoch, t->expected, chips,
                          NG_BEIDOU_B1C_DATA, &t->snapshot.navigation);
    }
    t->snapshot.navigation.validSubframes = t->bcnv1Frames;
}

/** B3 loop gains for 10-ms prompts. The Costas phase and frequency errors
 * remove the unknown sign of the pilot's secondary-code symbol. */
static void BeidouLoops(TNgTracking *t, double i, double q, double dll,
                        double early, double late, double *reportedError)
{
    const double tau = 6.2831853071795864769;
    const double duration = 0.01;
    const double phase = 0.5 * atan2(2 * i * q, i * i - q * q) / tau;
    const double dot = t->oldI * i + t->oldQ * q;
    const double cross = t->oldI * q - t->oldQ * i;
    const double residual = 0.5 * atan2(2 * dot * cross, dot * dot - cross * cross) /
                            (tau * duration);
    const double power = i * i + q * q;
    const int centered = power > 1.3 * fmax(early, late);
    if (!t->snapshot.pll && t->snapshot.integrations) {
        t->carrierHz += 0.55 * residual;
        if (centered && fabs(residual) < 3)
            ++t->beidouStable;
        else
            t->beidouStable = 0;
        if (t->beidouStable >= 4)
            t->snapshot.pll = 1;
    }
    if (t->snapshot.pll) {
        t->carrierHz += 0.12 * phase / duration;
        t->carrierPhase = fmod(t->carrierPhase + 0.48 * phase + 1.0, 1.0);
    }
    const double doppler = t->carrierHz - (double)t->stream.digitalIfHz;
    /* Apply the code phase correction over the next period so that the decoder
     * sees exactly one coherent prompt for every primary-code boundary. */
    t->snapshot.codeRateHz = 1023000 * (1 + doppler / 1575420000.0) + 0.075 * dll / duration;
    *reportedError = phase;
}

/** Commit one coherent integration and update the channel loops. */
static void Complete(TNgTracking *t)
{
    unsigned k, p = t->config.tapCount / 2;
    double e = t->sumI[p - 1] * t->sumI[p - 1] + t->sumQ[p - 1] * t->sumQ[p - 1];
    double l = t->sumI[p + 1] * t->sumI[p + 1] + t->sumQ[p + 1] * t->sumQ[p + 1];
    const int beidou = t->system == NG_CONSTELLATION_BEIDOU;
    double i = beidou ? t->sumQ[p] : t->sumI[p];
    double q = beidou ? -t->sumI[p] : t->sumQ[p];
    double delta = (double)t->period / 1023000.0;
    double error = 0;
    double dll = e + l > 0 ? (t->system == 1 ? 0.5 : 1.0 / 6.0) * (e - l) / (e + l) : 0;
    if (beidou) {
        BeidouLoops(t, i, q, dll * 6, e, l, &error);
    } else {
        if (t->snapshot.pll) {
            if (i != 0)
                error = -atan(q / i) / 6.2831853071795864769;
        } else if (t->snapshot.integrations) {
            double dot = t->oldI * i + t->oldQ * q;
            double cross = t->oldI * q - i * t->oldQ;
            if (dot != 0)
                error = -atan(cross / dot) / 6.2831853071795864769;
        }
        /* Complex exp(-j phase) reverses the Classic IF Q convention. */
        t->carrierNco = Loop(t->carrierNco, error, t->oldCarrierError, 15,
                             t->snapshot.pll ? delta * 1000.0 : 1.0, delta);
        t->codeNco = Loop(t->codeNco, dll, t->oldDllError, 2, 1, delta);
        t->carrierHz = t->initialCarrier - t->carrierNco;
        t->snapshot.codeRateHz = t->initialCode + t->codeNco;
        t->oldCarrierError = error;
        t->oldDllError = dll;
    }
    t->oldI = i;
    t->oldQ = q;
    t->snapshot.integrations++;
    t->meanFrequency += (t->carrierHz - t->meanFrequency) / 20.0;
    if (!beidou && !t->snapshot.pll && (t->snapshot.integrations * (t->period / 1023u)) % 100u == 0) {
        if (fabs(t->meanFrequency - t->oldMean) < 1.0) {
            t->snapshot.pll = 1;
            t->initialCarrier = t->carrierHz;
            t->carrierNco = t->oldCarrierError = 0;
        }
        t->oldMean = t->meanFrequency;
    }
    EstimateCn0(t, i / (double)t->accumulated, q / (double)t->accumulated,
                (double)t->accumulated / t->stream.sampleRateHz);
    if (t->system == 2)
        QualifyGalileoPeak(t, i, q);
    else
        t->snapshot.mainPeakVerified = 1;
    /* Classic PLL power qualification and bounded relock. FLL convergence alone
       must never occupy a receiver slot indefinitely on noise or a lost signal.
       Full false-lock correction and NCO holdover remain separate supervision. */
    t->lockAge += t->period / 1023u;
    if (t->snapshot.pll) {
        double ni = i / (double)t->accumulated, nq = q / (double)t->accumulated;
        t->lockI = 0.99 * t->lockI + 0.01 * ni * ni;
        t->lockQ = 0.99 * t->lockQ + 0.01 * nq * nq;
        if (t->lockAge % 100u == 0) {
            double total = t->lockI + t->lockQ;
            double ratio = total > 0 ? (t->lockI - t->lockQ) / total : 0;
            if (!t->qualified && ratio > 0.8) t->qualified = 1;
            if (t->qualified) {
                if (ratio < 0.4) t->failureMs += 100;
                else if (ratio > 0.8) t->failureMs = 0;
                if (t->failureMs > 1000) t->snapshot.active = 0;
            }
        }
    }
    if ((!t->snapshot.pll && t->lockAge >= 4000) || (!t->qualified && t->lockAge >= 8000))
        t->snapshot.active = 0;
    if (!t->snapshot.active) {
        t->snapshot.navigation.towValid = 0;
        return;
    }
    if (t->egnosPrompt) {
        if (t->snapshot.pll) {
            t->egnosPrompt(t->egnosContext, i / (double)t->accumulated, t->expected);
        }
    } else if (t->system == 1) {
        NgNavigationGpsPrompt(&t->navigation, i, q, t->snapshot.pll, t->expected);
        t->snapshot.navigation = t->navigation.snapshot;
    } else if (t->system == NG_CONSTELLATION_GALILEO) {
        NgNavigationGalileoPrompt(&t->navigation, i, t->snapshot.pll, t->expected);
        t->snapshot.navigation = t->navigation.snapshot;
    }
    if (t->system == NG_CONSTELLATION_BEIDOU) {
        BeidouPrompt(t, t->beidouDataI / (double)t->accumulated);
    } else if (t->navigation.snapshot.towValid) {
        if (t->navigation.snapshot.boundarySample == t->expected) {
            t->transmitBoundary = t->navigation.snapshot.towSeconds;
            t->transmitMilliseconds = 0;
            t->transmitTimeValid = 1;
        } else if (t->transmitTimeValid) {
            t->transmitMilliseconds += t->period / 1023u;
        }
        t->snapshot.navigation.transmitTimeSeconds =
            fmod(t->transmitBoundary + (double)t->transmitMilliseconds / 1000.0 +
                 t->codePhase / 1023000.0, 604800.0);
        if (!t->transmitTimeValid)
            t->snapshot.navigation.towValid = 0;
    } else {
        t->transmitTimeValid = 0;
    }
    double elapsed = (double)(t->start - t->seed.firstSample) / t->stream.sampleRateHz;
    double reference = fmod(t->seed.codePhaseChips +
                                elapsed * 1023000.0 * (1.0 + t->seed.dopplerHz / 1575420000.0),
                            t->period);
    double delay = reference - t->startPhase;
    if (delay > t->period / 2.0)
        delay -= t->period;
    if (delay < -(double)t->period / 2.0)
        delay += t->period;
    t->snapshot.codeDelayChips = delay;
    t->snapshot.firstSample = t->start;
    t->snapshot.endSample = t->expected;
    {
        double rawDoppler = t->carrierHz - (double)t->stream.digitalIfHz;
        double dt = (double)t->accumulated / t->stream.sampleRateHz;
        double tau = (double)t->config.dopplerSmoothingMs / 1000.0;
        t->smoothedDopplerHz = tau > 0 ? t->smoothedDopplerHz +
            dt / (tau + dt) * (rawDoppler - t->smoothedDopplerHz) : rawDoppler;
        t->snapshot.dopplerHz = t->smoothedDopplerHz;
    }
    t->snapshot.dllError = dll;
    t->snapshot.carrierError = error;
    t->snapshot.tapCount = t->observeIntegration ? t->config.tapCount : 3u;
    for (k = 0; k < t->snapshot.tapCount; ++k) {
        unsigned source = t->observeIntegration ? k : p + k - 1;
        t->snapshot.i[k] = t->sumI[source] / (double)t->accumulated;
        t->snapshot.q[k] = t->sumQ[source] / (double)t->accumulated;
        t->snapshot.offset[k] = t->offsets[source];
    }
    for (k = 0; k < t->config.tapCount; ++k) {
        t->sumI[k] = t->sumQ[k] = 0;
    }
    t->beidouDataI = 0;
    t->accumulated = 0;
}

/** Stream across arbitrary window splits and invalidate rather than integrate across gaps. */
int NgTrackingProcess(TNgTracking *t, const TNgIqView *window, TNgError *error)
{
    size_t position = 0;
    if (!t || !window)
        return NgFail(error, "Missing tracking input");
    if (!t->snapshot.active || !window->meta.count)
        return NG_OK;
    if (window->meta.epoch != t->seed.epoch || (window->meta.flags & NG_BLOCK_GAP) ||
        (t->started && window->meta.firstSample != t->expected) ||
        window->meta.firstSample < t->seed.firstSample) {
        t->snapshot.active = 0;
        return NG_OK;
    }
    if (!t->started) {
        double elapsed =
            (double)(window->meta.firstSample - t->seed.firstSample) / t->stream.sampleRateHz;
        t->codePhase = fmod(t->seed.codePhaseChips + elapsed * t->initialCode, t->period);
        t->expected = window->meta.firstSample;
        t->snapshot.trackingStartSample = window->meta.firstSample;
        t->started = 1;
    }
    while (position < window->meta.count) {
        size_t count, n;
        unsigned tap;
        if (!t->accumulated) {
            t->observeIntegration = t->observeRequested;
            double remaining = (double)t->period - t->codePhase;
            t->target = (size_t)ceil(remaining * t->stream.sampleRateHz / t->snapshot.codeRateHz);
            if (!t->target)
                t->target = 1;
            t->start = t->expected;
            t->startPhase = t->codePhase;
        }
        count = t->target - t->accumulated;
        if (count > NG_TRACKING_CHUNK)
            count = NG_TRACKING_CHUNK;
        if (count > window->meta.count - position)
            count = window->meta.count - position;
        if (window->format == 0 || window->format == NG_FORMAT_CI8) {
            if (t->config.mixerBits != 0)
                NgSearchMixPrefixCi8Arithmetic(t->prefix, window->samples + position, count,
                    t->stream.sampleRateHz, t->carrierHz, t->carrierPhase,
                    t->stream.spectrumInverted, t->config.mixerBits);
            else
            (t->config.fastCarrier ? NgSearchMixPrefixCi8Lanes : NgSearchMixPrefixCi8)(
                t->prefix, window->samples + position, count, t->stream.sampleRateHz,
                t->carrierHz, t->carrierPhase, t->config.squareCarrier,
                t->stream.spectrumInverted);
        } else {
            NgSearchCarrier(t->carrier, count, t->stream.sampleRateHz, t->carrierHz,
                            t->carrierPhase, t->config.squareCarrier);
            for (n = 0; n < count; ++n) {
                t->values[n].i = (float)NgIqComponent(window, position + n, 0);
                t->values[n].q = (float)(NgIqComponent(window, position + n, 1) *
                                         (t->stream.spectrumInverted ? -1 : 1));
            }
            NgSearchMix(t->values, t->carrier, count);
            NgSearchPrefix(t->prefix, t->values, count);
        }
        /* Each tap and Galileo half-chip probe sums whole chip runs of the mixed samples.
         * Runs follow the exact NgTrackingReplica sample-to-chip assignment. */
        for (tap = 0; tap < t->config.tapCount; ++tap) {
            double i, q;
            unsigned prompt = t->config.tapCount / 2;
            if (!t->observeIntegration && (tap + 1 < prompt || tap > prompt + 1))
                continue;
            NgTrackingCorrelateRuns(t->prefix, count, t->codePhase, t->snapshot.codeRateHz,
                                    t->stream.sampleRateHz, t->chips, t->period, t->offsets[tap],
                                    t->system != 1, &i, &q);
            t->sumI[tap] += i;
            t->sumQ[tap] += q;
        }
        if (t->system == NG_CONSTELLATION_BEIDOU) {
            double i, q;
            NgTrackingCorrelateRuns(t->prefix, count, t->codePhase, t->snapshot.codeRateHz,
                                    t->stream.sampleRateHz, t->beidouData, t->period, 0,
                                    1, &i, &q);
            t->beidouDataI += i;
        }
        if (t->system == 2) {
            unsigned side;
            for (side = 0; side < 2; ++side) {
                double i, q;
                NgTrackingCorrelateRuns(t->prefix, count, t->codePhase, t->snapshot.codeRateHz,
                                        t->stream.sampleRateHz, t->chips, t->period,
                                        side == 0 ? -0.5 : 0.5, 1, &i, &q);
                t->sideI[side] += i;
                t->sideQ[side] += q;
            }
        }
        t->codePhase =
            fmod(t->codePhase + (double)count * t->snapshot.codeRateHz / t->stream.sampleRateHz,
                 t->period);
        t->carrierPhase =
            fmod(t->carrierPhase + (double)count * t->carrierHz / t->stream.sampleRateHz, 1.0);
        t->accumulated += count;
        t->expected += count;
        position += count;
        if (t->accumulated == t->target) {
            if (t->discardFirst) {
                t->discardFirst = 0;
                t->beidouDataI = 0;
                t->accumulated = 0;
                memset(t->sumI, 0, sizeof(t->sumI));
                memset(t->sumQ, 0, sizeof(t->sumQ));
                memset(t->sideI, 0, sizeof(t->sideI));
                memset(t->sideQ, 0, sizeof(t->sideQ));
            } else
                Complete(t);
            if (!t->snapshot.active)
                return NG_OK;
        }
        if (!isfinite(t->carrierHz) || !isfinite(t->snapshot.codeRateHz) ||
            t->snapshot.codeRateHz < 1000000 || t->snapshot.codeRateHz > 1050000)
            return NgFail(error, "Tracking loop left the supported L1 code-rate range");
    }
    return NG_OK;
}

/** Copy a completed integration on the owning thread. */
void NgTrackingRead(const TNgTracking *tracking, TNgTrackingSnapshot *snapshot)
{
    *snapshot = tracking->snapshot;
}

/** Change interest only outside jobs; an in-flight integration retains its geometry. */
void NgTrackingRestoreBroadcast(TNgTracking *tracking, const TNgBroadcast *broadcast)
{
    tracking->navigation.snapshot.broadcast = *broadcast;
    tracking->navigation.snapshot.broadcast.towValid = 0;
    tracking->snapshot.navigation.broadcast = tracking->navigation.snapshot.broadcast;
}

/** Change interest only outside jobs; an in-flight integration retains its geometry. */
void NgTrackingObserve(TNgTracking *tracking, int observed)
{
    tracking->observeRequested = observed != 0;
}

/** Release private channel workspace. */
void NgTrackingDestroy(TNgTracking *tracking)
{
    free(tracking);
}

int NgTrackingSeedEgnos(TNgTracking *tracking, const TNgAcquisitionResult *result,
                        void (*prompt)(void *, double, uint64_t), void *context, TNgError *error)
{
    if (!result || result->prn < 1 || result->prn > 2 || !prompt ||
        NgTrackingSetSignal(tracking, NG_CONSTELLATION_GPS, error) != NG_OK ||
        NgTrackingSeed(tracking, result, error) != NG_OK) {
        return NgFail(error, "Cannot initialize EGNOS C/A tracking");
    }
    NgEgnosCode(result->prn - 1, tracking->chips);
    tracking->egnosPrompt = prompt;
    tracking->egnosContext = context;
    return NG_OK;
}
