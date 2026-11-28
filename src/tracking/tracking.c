/** @file tracking.c @brief Classic discriminators/second-order loops with private multi-tap DSP.
 * Algorithm basis: NGene Classic tracking/trackingDiscriminatorsLoops.c,
 * Mario Nicola, 2014. Adapted to native complex samples and shared window epochs.
 *
 * @details The tracker propagates carrier and code NCOs on the native
 * sample timeline and forms complex early, prompt and late sums over
 * the current primary-code period. The normalized power discriminator is
 * \f[
 * \epsilon_{\mathrm{DLL}}=K_d
 * \frac{|E|^2-|L|^2}{|E|^2+|L|^2},
 * \f]
 * with zero output for zero total power. GPS, Galileo and EGNOS use the
 * legacy second-order recurrence. For bandwidth \f$B\f$, effective gain
 * \f$g\f$ and update interval \f$T\f$, this implementation evaluates
 * \f[
 * \omega_n=B/0.53,\qquad \tau_1=g/\omega_n^2,\qquad
 * \tau_2=\sqrt{2}/\omega_n,\qquad
 * u_k=u_{k-1}+\frac{\tau_2}{\tau_1}(e_k-e_{k-1})
 *                 +\frac{T}{\tau_1}e_k .
 * \f]
 * The PLL uses gain scaled by the update interval in milliseconds.
 * BeiDou B1C retains its separate 10-ms loop law.
 *
 * A confirmed acquisition supplies the code phase and Doppler at its
 * capture origin. FLL pull-in precedes the PLL decision. Once GPS bit
 * timing is stable, its optional longer PLL/DLL update combines aligned
 * prompt groups within one LNAV bit. The native 1-ms prompt and bit timing
 * continue to feed navigation. Loss of continuity invalidates the assignment.
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
    void *egnosContext;                            /**< Lifetime of the separate GEO tracker. */
    int observeRequested,
        observeIntegration;               /**< Optional tap interest, latched per integration. */
    double offsets[NG_TRACKING_MAX_TAPS]; /**< Configured tap delays, never telemetry-compacted. */
    TNgNavigation navigation; /**< Per-integration data demodulator; never fed from telemetry. */
    TNgBcnv1Stream bcnv1;
    TNgBcnv1Cache bdt;
    unsigned bdtPeriods, beidouStable;
    uint64_t bcnv1Frames;
    double transmitBoundary;       /**< Transmit time of latest code boundary. */
    uint64_t transmitMilliseconds; /**< Exact elapsed code time since decoded anchor. */
    int transmitTimeValid;         /**< A decoded TOW anchored the code counter. */
    TNgTrackingConfig config;      /**< Frozen arithmetic choices. */
    TNgStreamInfo stream;          /**< Actual opened source geometry. */
    TNgTrackingSnapshot snapshot;  /**< Latest completed integration. */
    TNgAcquisitionResult seed;     /**< Capture-referenced initialization. */
    double startPhase;             /**< Code phase at the integration start. */
    double m2, m4, cn0Smooth;      /**< Classic moment-method exponential memories. */
    unsigned cn0Samples;           /**< Valid prompt moments accumulated since seed. */
    double lockI, lockQ;           /**< Classic length-100 PLL power means. */
    unsigned lockAge, failureMs;   /**< Signal-time acquisition/relock deadlines. */
    int qualified;                 /**< High PLL metric was established after the transient. */
    double sideI[2], sideQ[2];     /**< Half-chip BOC qualification coherent sums. */
    double peakPower[3];           /**< 100-ms accumulated left/prompt/right powers. */
    unsigned peakMs;               /**< Signal-time age of the qualification sums. */
    unsigned system;               /**< Constellation. */
    unsigned period;               /**< Code length in chips. */
    int started;                   /**< At least one window accepted after seed. */
    uint64_t expected;             /**< Next contiguous sample. */
    uint64_t start;                /**< Current integration origin. */
    size_t accumulated;            /**< Samples accumulated so far. */
    size_t target;                 /**< Samples in current coherent integration. */
    int discardFirst;              /**< Discard the first partial primary-code period. */
    double codePhase;              /**< Local code phase in chips. */
    double carrierPhase;           /**< Local carrier phase in cycles. */
    double carrierHz;              /**< IF plus estimated Doppler. */
    double smoothedDopplerHz;      /**< Filtered output; carrier NCO stays untouched. */
    double initialCarrier;         /**< Carrier anchor for Classic loop. */
    double initialCode;            /**< Code-rate anchor for Classic loop. */
    double coherentI[3], coherentQ[3];
    unsigned coherentPrompts;
    double loopStep[NG_LOOP_COUNT], loopIntegral[NG_LOOP_COUNT];
    double carrierNco, codeNco, oldCarrierError, oldDllError;      /**< Loop memories. */
    double oldI, oldQ;                                             /**< Previous prompt for FLL. */
    double meanFrequency, oldMean;                                 /**< FLL transition smoothing. */
    double sumI[NG_TRACKING_MAX_TAPS], sumQ[NG_TRACKING_MAX_TAPS]; /**< Coherent sums. */
    int8_t beidouData[2 * NG_BEIDOU_B1C_LENGTH];
    double beidouDataI;
    int8_t chips[2 * NG_BEIDOU_B1C_LENGTH];                           /**< Primary code. */
    TNgComplex values[NG_TRACKING_CHUNK], carrier[NG_TRACKING_CHUNK]; /**< Block scratch. */
    double prefix[2 * (NG_TRACKING_CHUNK + 1)]; /**< Running (I, Q) sums of mixed samples. */
};

/** Recalculate the legacy filters at assignment for the signal's coherent period. */
static void CalculateLoopCoefficients(TNgTracking *tracking, unsigned milliseconds)
{
    const double delta = (double)milliseconds / 1000.0;
    tracking->snapshot.loopCoherentMs = milliseconds;
    const TNgLoopSettings *settings = tracking->config.loops[tracking->system - 1];
    for (unsigned loop = 0; loop < NG_LOOP_COUNT; ++loop) {
        const double bw = settings[loop].bandwidthHz;
        const double gain = settings[loop].gain;
        if (tracking->system == NG_CONSTELLATION_BEIDOU) {
            /* Keep the B1C 10-ms first-order frequency/code and PI carrier laws.
             * Scale their existing gains relative to the configured nominal bandwidth. */
            const double ratio = bw / (loop == NG_LOOP_DLL ? 2.0 : 15.0);
            tracking->loopStep[loop] = (loop == NG_LOOP_DLL   ? 0.075
                                        : loop == NG_LOOP_FLL ? 0.55
                                                              : 0.48) *
                                       ratio / gain;
            tracking->loopIntegral[loop] = 0.12 * ratio * ratio / gain;
        } else {
            const double wn = bw / 0.53;
            const double t1 = (gain * (loop == NG_LOOP_PLL ? delta * 1000.0 : 1.0)) / (wn * wn);
            const double t2 = sqrt(2.0) / wn;
            tracking->loopStep[loop] = t2 / t1;
            tracking->loopIntegral[loop] = delta / t1;
        }
    }
}

/** Set independent precise defaults. */
void NgTrackingDefaults(TNgTrackingConfig *config)
{
    memset(config, 0, sizeof(*config));
    for (unsigned signal = 0; signal < NG_TRACKING_EGNOS_SIGNAL; ++signal) {
        for (unsigned loop = 0; loop < NG_LOOP_COUNT; ++loop) {
            config->loops[signal][loop] = (TNgLoopSettings){loop == NG_LOOP_DLL ? 2.0 : 15.0, 1.0};
        }
    }
    config->fllDecisionUpdates = 20;
    config->fllDecisionIntervalMs = 100;
    config->fllTransitionHz = 1.0;
    config->channelCount = 16;
    config->gpsCoherentMs = 1;
    config->gpsTransitionChecks = 5;
    config->galileoSpacingChips = 0.2;
    config->gpsChannels = 0;
    config->galileoChannels = 0;
    config->autoPriority = 0;
    config->workers = 4;
    config->enabled = 1;
    config->tapCount = 3;
    config->spacingChips = 0.4;
    config->egnosSpacingChips = 0.4;
    config->broadcastIonosphere = 1;
    config->dopplerSmoothingMs = 200;
}

/** Validate all configurable arithmetic boundaries. */
int NgTrackingValidate(const TNgTrackingConfig *config, TNgError *error)
{
    if (!config || !config->channelCount || config->channelCount > 64 ||
        config->gpsChannels + config->galileoChannels + config->beidouChannels >
            config->channelCount ||
        !isfinite(config->galileoSpacingChips) || config->galileoSpacingChips <= 0 ||
        config->galileoSpacingChips >= 1.0 / 3.0 || config->gpsChannels > 32 ||
        !(config->egnosSpacingChips > 0 && config->egnosSpacingChips <= 1) ||
        !(config->cn0CutoffDbHz >= 0 && config->cn0CutoffDbHz <= 60) ||
        !isfinite(config->beidouSpacingChips) || config->beidouSpacingChips < 0 ||
        config->beidouSpacingChips >= 1.0 / 3.0 || config->galileoChannels > 32 ||
        config->beidouChannels > NG_SATELLITE_MAX_PRN ||
        config->autoPriority > NG_CONSTELLATION_COUNT || !config->gpsCoherentMs ||
        20u % config->gpsCoherentMs != 0 || config->gpsTransitionChecks < 1 ||
        config->gpsTransitionChecks > 100 || config->fllDecisionUpdates < 1 ||
        config->fllDecisionUpdates > 1000 || config->fllDecisionIntervalMs < 4 ||
        config->fllDecisionIntervalMs > 2000 ||
        !(config->fllTransitionHz >= 0.01 && config->fllTransitionHz <= 100) || !config->workers ||
        config->workers > 16 || (config->enabled != 0 && config->enabled != 1) ||
        (config->squareCarrier != 0 && config->squareCarrier != 1) ||
        (config->xorCorrelation != 0 && config->xorCorrelation != 1) ||
        (config->fastCarrier != 0 && config->fastCarrier != 1) ||
        (config->mixerBits != 0 && config->mixerBits != 4 && config->mixerBits != 8 &&
         config->mixerBits != 16) ||
        config->dopplerSmoothingMs > 10000 || config->tapCount < 3 ||
        config->tapCount > NG_TRACKING_MAX_TAPS || !(config->tapCount & 1u) ||
        !isfinite(config->spacingChips) || config->spacingChips <= 0 || config->spacingChips > 1) {
        return NgFail(error, "Invalid tracking modes or multi-correlator geometry");
    }
    for (unsigned signal = 0; signal < NG_TRACKING_EGNOS_SIGNAL; ++signal) {
        for (unsigned loop = 0; loop < NG_LOOP_COUNT; ++loop) {
            const TNgLoopSettings settings = config->loops[signal][loop];
            if (!(settings.bandwidthHz >= 0.1 && settings.bandwidthHz <= 50) ||
                !(settings.gain >= 0.1 && settings.gain <= 20)) {
                return NgFail(error, "Invalid tracking filter bandwidth or gain");
            }
        }
    }
    return NG_OK;
}

/** Construct private, bounded native-rate tracking. */
int NgTrackingCreate(const TNgTrackingConfig *config, const TNgStreamInfo *stream,
                     unsigned constellation, TNgTracking **out, TNgError *error)
{
    TNgTracking *tracking;
    if (!out) {
        return NgFail(error, "Missing tracking destination");
    }
    *out = NULL;
    if (NgTrackingValidate(config, error) != NG_OK) {
        return NG_ERROR;
    }
    if (!stream || stream->sampleRateHz < 2048000 || stream->sampleRateHz > 20000000 ||
        (constellation < 1 || constellation > NG_CONSTELLATION_COUNT)) {
        return NgFail(error, "Unsupported tracking stream or constellation");
    }
    if (config->mixerBits != 0 && stream->format != NG_FORMAT_CI8) {
        return NgFail(error, "Byte arithmetic requires a CI8 common stream");
    }
    tracking = calloc(1, sizeof(*tracking));
    if (!tracking) {
        return NgFail(error, "Cannot allocate tracking workspace");
    }
    tracking->config = *config;
    tracking->observeRequested = 1;
    tracking->stream = *stream;
    tracking->system = constellation;
    tracking->period = constellation == 1   ? 1023u
                       : constellation == 2 ? 4092u
                                            : NG_BEIDOU_B1C_LENGTH;
    *out = tracking;
    return NG_OK;
}

/** Reuse a slot's private workspace for a supported primary code without allocating. */
int NgTrackingSetSignal(TNgTracking *tracking, unsigned constellation, TNgError *error)
{
    if (!tracking || (constellation < 1 || constellation > NG_CONSTELLATION_COUNT)) {
        return NgFail(error, "Invalid tracking signal assignment");
    }
    tracking->system = constellation;
    tracking->period = constellation == 1   ? 1023u
                       : constellation == 2 ? 4092u
                                            : NG_BEIDOU_B1C_LENGTH;
    return NG_OK;
}

/** Reset every integration and loop memory when a new acquisition is accepted. */
int NgTrackingSeed(TNgTracking *tracking, const TNgAcquisitionResult *result, TNgError *error)
{
    unsigned k;
    if (!tracking || !result || !result->confirmed ||
        !NgSatellitePrnValid((TNgConstellation)tracking->system, result->prn) ||
        !isfinite(result->dopplerHz) || !isfinite(result->codePhaseChips)) {
        return NgFail(error, "Tracking requires a finite confirmed acquisition seed");
    }
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
    tracking->coherentPrompts = 0;
    CalculateLoopCoefficients(tracking, tracking->period / 1023u);
    tracking->snapshot.codeRateHz = tracking->initialCode;
    tracking->snapshot.active = 1;
    tracking->snapshot.prn = result->prn;
    tracking->snapshot.constellation = tracking->system;
    tracking->snapshot.epoch = result->epoch;
    tracking->snapshot.tapCount = tracking->config.tapCount;
    double spacing = tracking->system == NG_CONSTELLATION_GPS
                         ? tracking->config.spacingChips
                         : tracking->config.galileoSpacingChips;
    if (tracking->system == NG_CONSTELLATION_BEIDOU && tracking->config.beidouSpacingChips > 0) {
        spacing = tracking->config.beidouSpacingChips;
    }
    for (k = 0; k < tracking->config.tapCount; ++k) {
        tracking->snapshot.offset[k] = tracking->offsets[k] =
            ((double)k - tracking->config.tapCount / 2u) * spacing;
    }
    memset(tracking->sumI, 0, sizeof(tracking->sumI));
    memset(tracking->sumQ, 0, sizeof(tracking->sumQ));
    if (tracking->system == 1) {
        return NgGpsCaGenerate(result->prn, tracking->chips, error);
    }
    if (tracking->system == NG_CONSTELLATION_BEIDOU) {
        int8_t primary[NG_BEIDOU_B1C_LENGTH];
        if (NgBeidouB1cGenerate(result->prn, NG_BEIDOU_B1C_DATA, primary, error) != NG_OK) {
            return NG_ERROR;
        }
        for (k = 0; k < NG_BEIDOU_B1C_LENGTH; ++k) {
            tracking->beidouData[2 * k] = primary[k];
            tracking->beidouData[2 * k + 1] = (int8_t)-primary[k];
        }
        if (NgBeidouB1cGenerate(result->prn, NG_BEIDOU_B1C_PILOT, primary, error) != NG_OK) {
            return NG_ERROR;
        }
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

static double Loop(const TNgTracking *tracking, unsigned loop, double previous, double error,
                   double oldError)
{
    return previous + tracking->loopStep[loop] * (error - oldError) +
           tracking->loopIntegral[loop] * error;
}

/** Classic M2/M4 moment estimator, adapted to a full precision complex carrier.
 * Unlike the packed IF path, no fixed carrier quantization correction is added.
 * This observation never changes acquisition thresholds or certifies navigation lock. */
static void EstimateCn0(TNgTracking *tracking, double promptInPhase, double promptQuadrature,
                        double duration)
{
    double power = promptInPhase * promptInPhase + promptQuadrature * promptQuadrature;
    tracking->snapshot.cn0Valid = 0;
    if (!tracking->cn0Samples) {
        tracking->m2 = power;
        tracking->m4 = power * power;
    } else {
        tracking->m2 += (power - tracking->m2) / 100.0;
        tracking->m4 += (power * power - tracking->m4) / 100.0;
    }
    ++tracking->cn0Samples;
    double squareSignal = 2 * tracking->m2 * tracking->m2 - tracking->m4;
    if (tracking->cn0Samples >= 100 && squareSignal > 0 && duration > 0 &&
        !tracking->config.squareCarrier) {
        double signal = sqrt(squareSignal), noise = tracking->m2 - signal;
        if (noise > 0 && signal > 0) {
            double estimate = 10 * log10(signal / noise / duration);
            if (isfinite(estimate)) {
                tracking->cn0Smooth =
                    tracking->cn0Smooth == 0
                        ? estimate
                        : tracking->cn0Smooth + (estimate - tracking->cn0Smooth) / 100.0;
                tracking->snapshot.cn0DbHz = tracking->cn0Smooth;
                tracking->snapshot.cn0Valid = 1;
            }
        }
    }
}

/** Observe half-chip BOC probes over 100 ms. Classic's side-peak correction
 * helper is not called by its active tracking path. These observations therefore
 * qualify the acquisition handoff without changing code phase or search decisions. */
static void QualifyGalileoPeak(TNgTracking *tracking, double promptInPhase, double promptQuadrature)
{
    unsigned side;
    for (side = 0; side < 2; ++side) {
        tracking->peakPower[side == 0 ? 0 : 2] += tracking->sideI[side] * tracking->sideI[side] +
                                                  tracking->sideQ[side] * tracking->sideQ[side];
    }
    tracking->peakPower[1] += promptInPhase * promptInPhase + promptQuadrature * promptQuadrature;
    tracking->peakMs += 4;
    memset(tracking->sideI, 0, sizeof(tracking->sideI));
    memset(tracking->sideQ, 0, sizeof(tracking->sideQ));
    if (tracking->peakMs < 100) {
        return;
    }
    double left = tracking->peakPower[0], prompt = tracking->peakPower[1],
           right = tracking->peakPower[2];
    tracking->snapshot.mainPeakVerified = prompt > left && prompt > right;
    memset(tracking->peakPower, 0, sizeof(tracking->peakPower));
    tracking->peakMs = 0;
}

/** Convert a complete B1C data prompt to a symbol and update BDT observations. */
static void BeidouPrompt(TNgTracking *tracking, double prompt)
{
    TNgBcnv1Frame frame;
    uint64_t firstSample;
    double scale = tracking->m2 > 0 ? 32.0 / sqrt(tracking->m2) : 0;
    int8_t symbol = (int8_t)lround(fmax(-127, fmin(127, scale * prompt)));
    if (!tracking->snapshot.pll) {
        return;
    }
    if (tracking->bdt.prn) {
        ++tracking->bdtPeriods;
    }
    if (NgBcnv1Push(&tracking->bcnv1, tracking->seed.prn, tracking->seed.epoch, tracking->start,
                    tracking->expected, symbol, &frame, &firstSample) == NG_OK &&
        NgBcnv1Accept(&tracking->bdt, &frame, tracking->seed.epoch, firstSample) == NG_OK) {
        tracking->bdtPeriods = tracking->bcnv1.elapsedSymbols;
        ++tracking->bcnv1Frames;
    }
    tracking->snapshot.navigation.towValid = 0;
    if (tracking->bdt.prn) {
        double chips = (double)tracking->bdtPeriods * NG_BEIDOU_B1C_LENGTH + tracking->codePhase;
        NgBcnv1Navigation(&tracking->bdt, tracking->seed.epoch, tracking->expected, chips,
                          NG_BEIDOU_B1C_DATA, &tracking->snapshot.navigation);
    }
    tracking->snapshot.navigation.validSubframes = tracking->bcnv1Frames;
}

/** B3 loop gains for 10-ms prompts. The Costas phase and frequency errors
 * remove the unknown sign of the pilot's secondary-code symbol. */
static void BeidouLoops(TNgTracking *tracking, double promptInPhase, double promptQuadrature,
                        double codeError, double earlyPower, double latePower,
                        double *reportedError)
{
    const double tau = 6.2831853071795864769;
    const double duration = 0.01;
    const double phase =
        0.5 *
        atan2(2 * promptInPhase * promptQuadrature,
              promptInPhase * promptInPhase - promptQuadrature * promptQuadrature) /
        tau;
    const double dot = tracking->oldI * promptInPhase + tracking->oldQ * promptQuadrature;
    const double cross = tracking->oldI * promptQuadrature - tracking->oldQ * promptInPhase;
    const double residual =
        0.5 * atan2(2 * dot * cross, dot * dot - cross * cross) / (tau * duration);
    const double power = promptInPhase * promptInPhase + promptQuadrature * promptQuadrature;
    const int centered = power > 1.3 * fmax(earlyPower, latePower);
    if (!tracking->snapshot.pll && tracking->snapshot.integrations) {
        tracking->carrierHz += tracking->loopStep[NG_LOOP_FLL] * residual;
        if (centered && fabs(residual) < 3) {
            ++tracking->beidouStable;
        } else {
            tracking->beidouStable = 0;
        }
        if (tracking->beidouStable >= 4) {
            tracking->snapshot.pll = 1;
        }
    }
    if (tracking->snapshot.pll) {
        tracking->carrierHz += tracking->loopIntegral[NG_LOOP_PLL] * phase / duration;
        tracking->carrierPhase =
            fmod(tracking->carrierPhase + tracking->loopStep[NG_LOOP_PLL] * phase + 1.0, 1.0);
    }
    const double doppler = tracking->carrierHz - (double)tracking->stream.digitalIfHz;
    /* Apply the code phase correction over the next period so that the decoder
     * sees exactly one coherent prompt for every primary-code boundary. */
    tracking->snapshot.codeRateHz = 1023000 * (1 + doppler / 1575420000.0) +
                                    tracking->loopStep[NG_LOOP_DLL] * codeError / duration;
    *reportedError = phase;
}

/** Extend GPS PLL/DLL integration only over aligned, same-bit prompt groups.
 * One-ms navigation prompts, measurements and lock supervision continue independently. */
static int GpsLoopPrompt(TNgTracking *tracking, unsigned promptTap, double *loopInPhase,
                         double *loopQuadrature, double *codeError)
{
    const unsigned wanted = tracking->config.gpsCoherentMs;
    if (wanted == 1 || tracking->system != NG_CONSTELLATION_GPS || tracking->egnosPrompt) {
        return 1;
    }
    const int synchronized =
        tracking->snapshot.pll && tracking->navigation.snapshot.bitSynchronized;
    if (!synchronized) {
        tracking->coherentPrompts = 0;
        if (tracking->snapshot.loopCoherentMs != 1) {
            CalculateLoopCoefficients(tracking, 1);
        }
        return 1;
    }
    if (!tracking->coherentPrompts) {
        if (tracking->navigation.bitMs % wanted != 0) {
            return 1;
        }
        memset(tracking->coherentI, 0, sizeof(tracking->coherentI));
        memset(tracking->coherentQ, 0, sizeof(tracking->coherentQ));
    }
    for (unsigned tapIndex = 0; tapIndex < 3; ++tapIndex) {
        tracking->coherentI[tapIndex] += tracking->sumI[promptTap - 1 + tapIndex];
        tracking->coherentQ[tapIndex] += tracking->sumQ[promptTap - 1 + tapIndex];
    }
    if (++tracking->coherentPrompts < wanted) {
        return 0;
    }
    tracking->coherentPrompts = 0;
    if (tracking->snapshot.loopCoherentMs != wanted) {
        CalculateLoopCoefficients(tracking, wanted);
    }
    *loopInPhase = tracking->coherentI[1];
    *loopQuadrature = tracking->coherentQ[1];
    const double early = tracking->coherentI[0] * tracking->coherentI[0] +
                         tracking->coherentQ[0] * tracking->coherentQ[0];
    const double late = tracking->coherentI[2] * tracking->coherentI[2] +
                        tracking->coherentQ[2] * tracking->coherentQ[2];
    *codeError = early + late > 0 ? 0.5 * (early - late) / (early + late) : 0;
    return 1;
}

/** Commit one coherent integration and update the channel loops. */
static void Complete(TNgTracking *tracking)
{
    unsigned tapIndex;
    unsigned promptTap = tracking->config.tapCount / 2;
    double earlyPower = tracking->sumI[promptTap - 1] * tracking->sumI[promptTap - 1] +
                        tracking->sumQ[promptTap - 1] * tracking->sumQ[promptTap - 1];
    double latePower = tracking->sumI[promptTap + 1] * tracking->sumI[promptTap + 1] +
                       tracking->sumQ[promptTap + 1] * tracking->sumQ[promptTap + 1];
    const int beidou = tracking->system == NG_CONSTELLATION_BEIDOU;
    double promptInPhase = beidou ? tracking->sumQ[promptTap] : tracking->sumI[promptTap];
    double promptQuadrature = beidou ? -tracking->sumI[promptTap] : tracking->sumQ[promptTap];
    double error = 0;
    double dll = earlyPower + latePower > 0
                     ? (tracking->system == 1 ? 0.5 : 1.0 / 6.0) * (earlyPower - latePower) /
                           (earlyPower + latePower)
                     : 0;
    double loopI = promptInPhase, loopQ = promptQuadrature;
    if (beidou) {
        BeidouLoops(tracking, promptInPhase, promptQuadrature, dll * 6, earlyPower, latePower,
                    &error);
    } else if (GpsLoopPrompt(tracking, promptTap, &loopI, &loopQ, &dll)) {
        if (tracking->snapshot.pll) {
            if (loopI != 0) {
                error = -atan(loopQ / loopI) / 6.2831853071795864769;
            }
        } else if (tracking->snapshot.integrations) {
            double dot = tracking->oldI * promptInPhase + tracking->oldQ * promptQuadrature;
            double cross = tracking->oldI * promptQuadrature - promptInPhase * tracking->oldQ;
            if (dot != 0) {
                error = -atan(cross / dot) / 6.2831853071795864769;
            }
        }
        /* Complex exp(-j phase) reverses the Classic IF Q convention. */
        tracking->carrierNco = Loop(tracking, tracking->snapshot.pll ? NG_LOOP_PLL : NG_LOOP_FLL,
                                    tracking->carrierNco, error, tracking->oldCarrierError);
        tracking->codeNco =
            Loop(tracking, NG_LOOP_DLL, tracking->codeNco, dll, tracking->oldDllError);
        tracking->carrierHz = tracking->initialCarrier - tracking->carrierNco;
        tracking->snapshot.codeRateHz = tracking->initialCode + tracking->codeNco;
        tracking->oldCarrierError = error;
        tracking->oldDllError = dll;
    }
    tracking->oldI = promptInPhase;
    tracking->oldQ = promptQuadrature;
    tracking->snapshot.integrations++;
    tracking->meanFrequency +=
        (tracking->carrierHz - tracking->meanFrequency) / tracking->config.fllDecisionUpdates;
    const unsigned periodMs = tracking->period / 1023u;
    const unsigned decisionUpdates =
        (tracking->config.fllDecisionIntervalMs + periodMs - 1) / periodMs;
    if (!beidou && !tracking->snapshot.pll &&
        tracking->snapshot.integrations % decisionUpdates == 0) {
        if (fabs(tracking->meanFrequency - tracking->oldMean) < tracking->config.fllTransitionHz) {
            tracking->snapshot.pll = 1;
            tracking->initialCarrier = tracking->carrierHz;
            tracking->carrierNco = tracking->oldCarrierError = 0;
        }
        tracking->oldMean = tracking->meanFrequency;
    }
    EstimateCn0(tracking, promptInPhase / (double)tracking->accumulated,
                promptQuadrature / (double)tracking->accumulated,
                (double)tracking->accumulated / tracking->stream.sampleRateHz);
    if (tracking->system == 2) {
        QualifyGalileoPeak(tracking, promptInPhase, promptQuadrature);
    } else {
        tracking->snapshot.mainPeakVerified = 1;
    }
    /* Classic PLL power qualification and bounded relock. FLL convergence alone
       must never occupy a receiver slot indefinitely on noise or a lost signal.
       Full false-lock correction and NCO holdover remain separate supervision. */
    tracking->lockAge += tracking->period / 1023u;
    if (tracking->snapshot.pll) {
        double ni = promptInPhase / (double)tracking->accumulated,
               nq = promptQuadrature / (double)tracking->accumulated;
        tracking->lockI = 0.99 * tracking->lockI + 0.01 * ni * ni;
        tracking->lockQ = 0.99 * tracking->lockQ + 0.01 * nq * nq;
        if (tracking->lockAge % 100u == 0) {
            double total = tracking->lockI + tracking->lockQ;
            double ratio = total > 0 ? (tracking->lockI - tracking->lockQ) / total : 0;
            if (!tracking->qualified && ratio > 0.8) {
                tracking->qualified = 1;
            }
            if (tracking->qualified) {
                const int belowCutoff = tracking->config.cn0CutoffDbHz > 0 &&
                                        tracking->snapshot.cn0Valid &&
                                        tracking->snapshot.cn0DbHz < tracking->config.cn0CutoffDbHz;
                if (ratio < 0.4 || belowCutoff) {
                    tracking->failureMs += 100;
                } else if (ratio > 0.8) {
                    tracking->failureMs = 0;
                }
                if (tracking->failureMs > 1000) {
                    tracking->snapshot.active = 0;
                }
            }
        }
    }
    if ((!tracking->snapshot.pll && tracking->lockAge >= 4000) ||
        (!tracking->qualified && tracking->lockAge >= 8000)) {
        tracking->snapshot.active = 0;
    }
    if (!tracking->snapshot.active) {
        tracking->snapshot.navigation.towValid = 0;
        return;
    }
    if (tracking->egnosPrompt) {
        if (tracking->snapshot.pll) {
            tracking->egnosPrompt(tracking->egnosContext,
                                  promptInPhase / (double)tracking->accumulated,
                                  tracking->expected);
        }
    } else if (tracking->system == 1) {
        NgNavigationGpsPromptWithChecks(&tracking->navigation, promptInPhase, promptQuadrature,
                                        tracking->snapshot.pll, tracking->expected,
                                        tracking->config.gpsTransitionChecks);
        tracking->snapshot.navigation = tracking->navigation.snapshot;
    } else if (tracking->system == NG_CONSTELLATION_GALILEO) {
        NgNavigationGalileoPrompt(&tracking->navigation, promptInPhase, tracking->snapshot.pll,
                                  tracking->expected);
        tracking->snapshot.navigation = tracking->navigation.snapshot;
    }
    if (tracking->system == NG_CONSTELLATION_BEIDOU) {
        BeidouPrompt(tracking, tracking->beidouDataI / (double)tracking->accumulated);
    } else if (tracking->navigation.snapshot.towValid) {
        if (tracking->navigation.snapshot.boundarySample == tracking->expected) {
            tracking->transmitBoundary = tracking->navigation.snapshot.towSeconds;
            tracking->transmitMilliseconds = 0;
            tracking->transmitTimeValid = 1;
        } else if (tracking->transmitTimeValid) {
            tracking->transmitMilliseconds += tracking->period / 1023u;
        }
        tracking->snapshot.navigation.transmitTimeSeconds =
            fmod(tracking->transmitBoundary + (double)tracking->transmitMilliseconds / 1000.0 +
                     tracking->codePhase / 1023000.0,
                 604800.0);
        if (!tracking->transmitTimeValid) {
            tracking->snapshot.navigation.towValid = 0;
        }
    } else {
        tracking->transmitTimeValid = 0;
    }
    double elapsed =
        (double)(tracking->start - tracking->seed.firstSample) / tracking->stream.sampleRateHz;
    double reference =
        fmod(tracking->seed.codePhaseChips +
                 elapsed * 1023000.0 * (1.0 + tracking->seed.dopplerHz / 1575420000.0),
             tracking->period);
    double delay = reference - tracking->startPhase;
    if (delay > tracking->period / 2.0) {
        delay -= tracking->period;
    }
    if (delay < -(double)tracking->period / 2.0) {
        delay += tracking->period;
    }
    tracking->snapshot.codeDelayChips = delay;
    tracking->snapshot.firstSample = tracking->start;
    tracking->snapshot.endSample = tracking->expected;
    {
        double rawDoppler = tracking->carrierHz - (double)tracking->stream.digitalIfHz;
        double dt = (double)tracking->accumulated / tracking->stream.sampleRateHz;
        double tau = (double)tracking->config.dopplerSmoothingMs / 1000.0;
        tracking->smoothedDopplerHz =
            tau > 0 ? tracking->smoothedDopplerHz +
                          dt / (tau + dt) * (rawDoppler - tracking->smoothedDopplerHz)
                    : rawDoppler;
        tracking->snapshot.dopplerHz = tracking->smoothedDopplerHz;
    }
    tracking->snapshot.dllError = dll;
    tracking->snapshot.carrierError = error;
    tracking->snapshot.tapCount = tracking->observeIntegration ? tracking->config.tapCount : 3u;
    for (tapIndex = 0; tapIndex < tracking->snapshot.tapCount; ++tapIndex) {
        unsigned source = tracking->observeIntegration ? tapIndex : promptTap + tapIndex - 1;
        tracking->snapshot.i[tapIndex] = tracking->sumI[source] / (double)tracking->accumulated;
        tracking->snapshot.q[tapIndex] = tracking->sumQ[source] / (double)tracking->accumulated;
        tracking->snapshot.offset[tapIndex] = tracking->offsets[source];
    }
    for (tapIndex = 0; tapIndex < tracking->config.tapCount; ++tapIndex) {
        tracking->sumI[tapIndex] = tracking->sumQ[tapIndex] = 0;
    }
    tracking->beidouDataI = 0;
    tracking->accumulated = 0;
}

/** Stream across arbitrary window splits and invalidate rather than integrate across gaps. */
int NgTrackingProcess(TNgTracking *tracking, const TNgIqView *window, TNgError *error)
{
    size_t position = 0;
    if (!tracking || !window) {
        return NgFail(error, "Missing tracking input");
    }
    if (!tracking->snapshot.active || !window->meta.count) {
        return NG_OK;
    }
    if (window->meta.epoch != tracking->seed.epoch || (window->meta.flags & NG_BLOCK_GAP) ||
        (tracking->started && window->meta.firstSample != tracking->expected) ||
        window->meta.firstSample < tracking->seed.firstSample) {
        tracking->snapshot.active = 0;
        return NG_OK;
    }
    if (!tracking->started) {
        double elapsed = (double)(window->meta.firstSample - tracking->seed.firstSample) /
                         tracking->stream.sampleRateHz;
        tracking->codePhase =
            fmod(tracking->seed.codePhaseChips + elapsed * tracking->initialCode, tracking->period);
        tracking->expected = window->meta.firstSample;
        tracking->snapshot.trackingStartSample = window->meta.firstSample;
        tracking->started = 1;
    }
    while (position < window->meta.count) {
        size_t count, n;
        unsigned tap;
        if (!tracking->accumulated) {
            tracking->observeIntegration = tracking->observeRequested;
            double remaining = (double)tracking->period - tracking->codePhase;
            tracking->target = (size_t)ceil(remaining * tracking->stream.sampleRateHz /
                                            tracking->snapshot.codeRateHz);
            if (!tracking->target) {
                tracking->target = 1;
            }
            tracking->start = tracking->expected;
            tracking->startPhase = tracking->codePhase;
        }
        count = tracking->target - tracking->accumulated;
        if (count > NG_TRACKING_CHUNK) {
            count = NG_TRACKING_CHUNK;
        }
        if (count > window->meta.count - position) {
            count = window->meta.count - position;
        }
        if (window->format == 0 || window->format == NG_FORMAT_CI8) {
            if (tracking->config.mixerBits != 0) {
                NgSearchMixPrefixCi8Arithmetic(
                    tracking->prefix, window->samples + position, count,
                    tracking->stream.sampleRateHz, tracking->carrierHz, tracking->carrierPhase,
                    tracking->stream.spectrumInverted, tracking->config.mixerBits);
            } else {
                (tracking->config.fastCarrier ? NgSearchMixPrefixCi8Lanes : NgSearchMixPrefixCi8)(
                    tracking->prefix, window->samples + position, count,
                    tracking->stream.sampleRateHz, tracking->carrierHz, tracking->carrierPhase,
                    tracking->config.squareCarrier, tracking->stream.spectrumInverted);
            }
        } else {
            NgSearchCarrier(tracking->carrier, count, tracking->stream.sampleRateHz,
                            tracking->carrierHz, tracking->carrierPhase,
                            tracking->config.squareCarrier);
            for (n = 0; n < count; ++n) {
                tracking->values[n].i = (float)NgIqComponent(window, position + n, 0);
                tracking->values[n].q = (float)(NgIqComponent(window, position + n, 1) *
                                                (tracking->stream.spectrumInverted ? -1 : 1));
            }
            NgSearchMix(tracking->values, tracking->carrier, count);
            NgSearchPrefix(tracking->prefix, tracking->values, count);
        }
        /* Each tap and Galileo half-chip probe sums whole chip runs of the mixed samples.
         * Runs follow the exact NgTrackingReplica sample-to-chip assignment. */
        for (tap = 0; tap < tracking->config.tapCount; ++tap) {
            double inPhase;
            double quadrature;
            unsigned prompt = tracking->config.tapCount / 2;
            if (!tracking->observeIntegration && (tap + 1 < prompt || tap > prompt + 1)) {
                continue;
            }
            NgTrackingCorrelateRuns(tracking->prefix, count, tracking->codePhase,
                                    tracking->snapshot.codeRateHz, tracking->stream.sampleRateHz,
                                    tracking->chips, tracking->period, tracking->offsets[tap],
                                    tracking->system != 1, &inPhase, &quadrature);
            tracking->sumI[tap] += inPhase;
            tracking->sumQ[tap] += quadrature;
        }
        if (tracking->system == NG_CONSTELLATION_BEIDOU) {
            double inPhase;
            double quadrature;
            NgTrackingCorrelateRuns(tracking->prefix, count, tracking->codePhase,
                                    tracking->snapshot.codeRateHz, tracking->stream.sampleRateHz,
                                    tracking->beidouData, tracking->period, 0, 1, &inPhase,
                                    &quadrature);
            tracking->beidouDataI += inPhase;
        }
        if (tracking->system == 2) {
            unsigned side;
            for (side = 0; side < 2; ++side) {
                double inPhase;
                double quadrature;
                NgTrackingCorrelateRuns(
                    tracking->prefix, count, tracking->codePhase, tracking->snapshot.codeRateHz,
                    tracking->stream.sampleRateHz, tracking->chips, tracking->period,
                    side == 0 ? -0.5 : 0.5, 1, &inPhase, &quadrature);
                tracking->sideI[side] += inPhase;
                tracking->sideQ[side] += quadrature;
            }
        }
        tracking->codePhase =
            fmod(tracking->codePhase +
                     (double)count * tracking->snapshot.codeRateHz / tracking->stream.sampleRateHz,
                 tracking->period);
        tracking->carrierPhase = fmod(tracking->carrierPhase + (double)count * tracking->carrierHz /
                                                                   tracking->stream.sampleRateHz,
                                      1.0);
        tracking->accumulated += count;
        tracking->expected += count;
        position += count;
        if (tracking->accumulated == tracking->target) {
            if (tracking->discardFirst) {
                tracking->discardFirst = 0;
                tracking->beidouDataI = 0;
                tracking->accumulated = 0;
                memset(tracking->sumI, 0, sizeof(tracking->sumI));
                memset(tracking->sumQ, 0, sizeof(tracking->sumQ));
                memset(tracking->sideI, 0, sizeof(tracking->sideI));
                memset(tracking->sideQ, 0, sizeof(tracking->sideQ));
            } else {
                Complete(tracking);
            }
            if (!tracking->snapshot.active) {
                return NG_OK;
            }
        }
        if (!isfinite(tracking->carrierHz) || !isfinite(tracking->snapshot.codeRateHz) ||
            tracking->snapshot.codeRateHz < 1000000 || tracking->snapshot.codeRateHz > 1050000) {
            return NgFail(error, "Tracking loop left the supported L1 code-rate range");
        }
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
