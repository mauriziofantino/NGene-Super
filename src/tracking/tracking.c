/** @file tracking.c @brief Classic discriminators/second-order loops with private multi-tap DSP.
 * Algorithm basis: NGene Classic tracking/trackingDiscriminatorsLoops.c,
 * Mario Nicola, 2014. Adapted to native complex samples and shared window epochs.
 */
#include "ngene/tracking.h"
#include "ngene/dsp/gps_ca.h"
#include "acquisition/galileo_codes.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define NG_TRACKING_CHUNK 2048u

/** Private NCOs, integration accumulators and reusable block kernels. */
struct TNgTracking {
    TNgTrackingConfig config;     /**< Frozen arithmetic choices. */
    TNgStreamInfo stream;         /**< Actual opened source geometry. */
    TNgTrackingSnapshot snapshot; /**< Latest completed integration. */
    TNgAcquisitionResult seed;    /**< Capture-referenced initialization. */
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
    double initialCarrier;        /**< Carrier anchor for Classic loop. */
    double initialCode;           /**< Code-rate anchor for Classic loop. */
    double carrierNco, codeNco, oldCarrierError, oldDllError;      /**< Loop memories. */
    double oldI, oldQ;                                             /**< Previous prompt for FLL. */
    double meanFrequency, oldMean;                                 /**< FLL transition smoothing. */
    double sumI[NG_TRACKING_MAX_TAPS], sumQ[NG_TRACKING_MAX_TAPS]; /**< Coherent sums. */
    int8_t chips[4092];                                            /**< Primary code. */
    TNgComplex values[NG_TRACKING_CHUNK], carrier[NG_TRACKING_CHUNK]; /**< Block scratch. */
    float replica[NG_TRACKING_CHUNK]; /**< One tap, reused sequentially. */
};

/** Set independent precise defaults. */
void NgTrackingDefaults(TNgTrackingConfig *config)
{
    memset(config, 0, sizeof(*config));
    config->enabled = 1;
    config->tapCount = 3;
    config->spacingChips = 0.5;
}

/** Validate all configurable arithmetic boundaries. */
int NgTrackingValidate(const TNgTrackingConfig *config, TNgError *error)
{
    if (!config || (config->enabled != 0 && config->enabled != 1) ||
        (config->squareCarrier != 0 && config->squareCarrier != 1) ||
        (config->xorCorrelation != 0 && config->xorCorrelation != 1) || config->tapCount < 3 ||
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
        (constellation != 1 && constellation != 2))
        return NgFail(error, "Unsupported tracking stream or constellation");
    tracking = calloc(1, sizeof(*tracking));
    if (!tracking)
        return NgFail(error, "Cannot allocate tracking workspace");
    tracking->config = *config;
    tracking->stream = *stream;
    tracking->system = constellation;
    tracking->period = constellation == 1 ? 1023u : 4092u;
    *out = tracking;
    return NG_OK;
}

/** Reset every integration and loop memory when a new acquisition is accepted. */
int NgTrackingSeed(TNgTracking *tracking, const TNgAcquisitionResult *result, TNgError *error)
{
    unsigned k;
    if (!tracking || !result || !result->confirmed || result->prn < 1 || result->prn > 32 ||
        !isfinite(result->dopplerHz) || !isfinite(result->codePhaseChips))
        return NgFail(error, "Tracking requires a finite confirmed acquisition seed");
    memset(&tracking->snapshot, 0, sizeof(tracking->snapshot));
    tracking->seed = *result;
    tracking->started = 0;
    tracking->accumulated = 0;
    tracking->discardFirst = 1;
    tracking->carrierNco = tracking->codeNco = 0;
    tracking->oldCarrierError = tracking->oldDllError = 0;
    tracking->oldI = tracking->oldQ = 0;
    tracking->carrierPhase = 0;
    tracking->initialCarrier = (double)tracking->stream.digitalIfHz + result->dopplerHz;
    tracking->carrierHz = tracking->initialCarrier;
    tracking->meanFrequency = tracking->initialCarrier;
    tracking->oldMean = 0;
    tracking->initialCode = 1023000.0 * (1.0 + result->dopplerHz / 1575420000.0);
    tracking->snapshot.codeRateHz = tracking->initialCode;
    tracking->snapshot.active = 1;
    tracking->snapshot.prn = result->prn;
    tracking->snapshot.constellation = tracking->system;
    tracking->snapshot.epoch = result->epoch;
    tracking->snapshot.tapCount = tracking->config.tapCount;
    for (k = 0; k < tracking->config.tapCount; ++k)
        tracking->snapshot.offset[k] =
            ((double)k - tracking->config.tapCount / 2u) * tracking->config.spacingChips;
    memset(tracking->sumI, 0, sizeof(tracking->sumI));
    memset(tracking->sumQ, 0, sizeof(tracking->sumQ));
    if (tracking->system == 1)
        return NgGpsCaGenerate(result->prn, tracking->chips, error);
    for (k = 0; k < 4092; ++k)
        tracking->chips[k] =
            (ngGalileoCodes[result->prn - 1][k / 8] & (1u << (7u - k % 8u))) ? -1 : 1;
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

/** Commit one coherent integration and update Classic FLL/Costas PLL and power DLL. */
static void Complete(TNgTracking *t)
{
    unsigned k, p = t->config.tapCount / 2;
    double e = t->sumI[p - 1] * t->sumI[p - 1] + t->sumQ[p - 1] * t->sumQ[p - 1];
    double l = t->sumI[p + 1] * t->sumI[p + 1] + t->sumQ[p + 1] * t->sumQ[p + 1];
    double i = t->sumI[p], q = t->sumQ[p];
    double delta = (double)t->period / 1023000.0;
    double error = 0;
    double dll = e + l > 0 ? (t->system == 1 ? 0.5 : 1.0 / 6.0) * (e - l) / (e + l) : 0;
    if (t->snapshot.pll) {
        if (i != 0)
            error = -atan(q / i) / 6.2831853071795864769;
    } else if (t->snapshot.integrations) {
        double dot = t->oldI * i + t->oldQ * q;
        double cross = t->oldI * q - i * t->oldQ;
        if (dot != 0)
            error = -atan(cross / dot) / 6.2831853071795864769;
    }
    /* Complex exp(-j phase) reverses the Classic IF Q convention: negate the
       carrier correction when applying the retained discriminator recurrence. */
    t->carrierNco = Loop(t->carrierNco, error, t->oldCarrierError, 15,
                         t->snapshot.pll ? delta * 1000.0 : 1.0, delta);
    t->codeNco = Loop(t->codeNco, dll, t->oldDllError, 2, 1, delta);
    t->carrierHz = t->initialCarrier - t->carrierNco;
    /* Replica delay offsets use the opposite sign to Classic's buffer offsets. */
    t->snapshot.codeRateHz = t->initialCode + t->codeNco;
    t->oldCarrierError = error;
    t->oldDllError = dll;
    t->oldI = i;
    t->oldQ = q;
    t->snapshot.integrations++;
    t->meanFrequency += (t->carrierHz - t->meanFrequency) / 20.0;
    if (!t->snapshot.pll && (t->snapshot.integrations * (t->system == 1 ? 1u : 4u)) % 100u == 0) {
        if (fabs(t->meanFrequency - t->oldMean) < 1.0) {
            t->snapshot.pll = 1;
            t->initialCarrier = t->carrierHz;
            t->carrierNco = t->oldCarrierError = 0;
        }
        t->oldMean = t->meanFrequency;
    }
    t->snapshot.firstSample = t->start;
    t->snapshot.endSample = t->expected;
    t->snapshot.dopplerHz = t->carrierHz - (double)t->stream.digitalIfHz;
    t->snapshot.dllError = dll;
    t->snapshot.carrierError = error;
    for (k = 0; k < t->config.tapCount; ++k) {
        t->snapshot.i[k] = t->sumI[k] / (double)t->accumulated;
        t->snapshot.q[k] = t->sumQ[k] / (double)t->accumulated;
        t->sumI[k] = t->sumQ[k] = 0;
    }
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
        t->started = 1;
    }
    while (position < window->meta.count) {
        size_t count, n;
        unsigned tap;
        if (!t->accumulated) {
            double remaining = (double)t->period - t->codePhase;
            t->target = (size_t)ceil(remaining * t->stream.sampleRateHz / t->snapshot.codeRateHz);
            if (!t->target)
                t->target = 1;
            t->start = t->expected;
        }
        count = t->target - t->accumulated;
        if (count > NG_TRACKING_CHUNK)
            count = NG_TRACKING_CHUNK;
        if (count > window->meta.count - position)
            count = window->meta.count - position;
        NgSearchCarrier(t->carrier, count, t->stream.sampleRateHz, t->carrierHz, t->carrierPhase,
                        t->config.squareCarrier);
        for (n = 0; n < count; ++n) {
            t->values[n].i = (float)NgIqComponent(window, position + n, 0);
            t->values[n].q = (float)(NgIqComponent(window, position + n, 1) *
                                     (t->stream.spectrumInverted ? -1 : 1));
        }
        NgSearchMix(t->values, t->carrier, count);
        for (tap = 0; tap < t->config.tapCount; ++tap) {
            double i, q;
            for (n = 0; n < count; ++n) {
                double phase = t->codePhase +
                               (double)n * t->snapshot.codeRateHz / t->stream.sampleRateHz -
                               t->snapshot.offset[tap];
                double wrapped = fmod(fmod(phase, t->period) + t->period, t->period);
                int sign = t->chips[(size_t)wrapped];
                if (t->system == 2 && wrapped - floor(wrapped) >= 0.5)
                    sign = -sign;
                t->replica[n] = (float)sign;
            }
            NgSearchCorrelate(t->values, t->replica, count, t->config.xorCorrelation, &i, &q);
            t->sumI[tap] += i;
            t->sumQ[tap] += q;
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
                t->accumulated = 0;
                memset(t->sumI, 0, sizeof(t->sumI));
                memset(t->sumQ, 0, sizeof(t->sumQ));
            } else
                Complete(t);
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

/** Release private channel workspace. */
void NgTrackingDestroy(TNgTracking *tracking)
{
    free(tracking);
}
