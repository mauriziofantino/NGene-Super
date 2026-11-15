/** @file analysis.c @brief Selected contiguous bursts, integer FFT/power/histogram worker. */
#include "ngene/analysis.h"
#include "ngene/dsp/fft.h"
#include "ngene/dsp/sample_ops.h"
#include "platform/platform.h"
#include <math.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/** @brief One copied diagnostic input slot; lifecycle serializes producer and worker ownership. */
typedef struct {
    /** Owned copied native IQ observation; immutable while the worker owns the slot. */
    union {
        /** Unchanged signed CI8 fast path. */
        TNgIq8 ci8[NG_ANALYSIS_PAIRS];
        /** Native IQ copy, large enough for SC16 Q11. */
        unsigned char native[NG_ANALYSIS_PAIRS * 4];
    } data; /**< Owned copied IQ payload, immutable while leased to the worker. */

    /** Native representation of this copied observation. */
    TNgSampleFormat format;
    /** Diagnostic producer selector: zero PSD, one histogram. */
    int kind;
    /** Slot ownership state: free, filling, ready, or worker-owned. */
    int state;
    /** Number of populated complex pairs or response entries. */
    size_t count;
    /** Contiguous complex pairs needed before the observation becomes ready. */
    size_t required;
    /** Continuity generation; invalidates coherent history after a reset. */
    uint64_t epoch;
    /** First copied complex-pair sample index. */
    uint64_t first;
    /** Configuration revision associated with copied input or current settings. */
    uint64_t revision;
    /** Local discontinuity generation used to reject incoherent temporal history. */
    uint64_t continuity;
    /** Cancellation generation copied when this observation begins. */
    int generation;
    /** Copied validated configuration; owned by this instance. */
    TNgAnalysisConfig config;
    /** Monotonic timestamp at which this observation was copied. */
    double captured;
} TNgBurst;

/** @brief Bounded diagnostic tap, worker and immutable published snapshot. */
struct TNgAnalysis {
    /** Protects shared state; never perform transport or disk I/O while holding it. */
    TNgMutex mutex;
    /** Notification for a complete copied observation. */
    TNgCondition ready;
    /** Owned worker handle; join before destroying any accessed state. */
    TNgThread thread;
    /** Atomic request for the worker/engine to stop. */
    TNgAtomicInt stop;
    /** Atomic count of optional submissions skipped instead of waiting. */
    TNgAtomicInt contention;
    /** Copied validated configuration; owned by this instance. */
    TNgAnalysisConfig config;
    /** Configuration revision associated with copied input or current settings. */
    uint64_t revision;
    /** Next diagnostic publication identifier, protected by mutex. */
    uint64_t sequence;
    /** Bounded observations skipped when all eligible slots were occupied. */
    uint64_t skipped;
    /** Complex sample rate in pairs per second. */
    uint32_t sampleRate;
    /** Once set, prevents sample-rate changes racing with calculation. */
    int offered;
    /** Independent next capture deadlines for PSD and histogram, in monotonic seconds. */
    double nextCapture[2];
    /** Subscriber-interest expiry for each optional producer, in monotonic seconds. */
    double interestUntil[2];
    /** Atomic cancellation generations checked between optional calculation stages. */
    TNgAtomicInt generation[2];
    /** Slot index currently being filled by each producer; -1 means none. */
    int filling[2];
    /** Four owned input slots shared under mutex, two per diagnostic kind. */
    TNgBurst buffers[4];
    /** Revision of the current temporal PSD history. */
    uint64_t temporalRevision;
    /** Input epoch of the current temporal PSD history. */
    uint64_t temporalEpoch;
    /** Local continuity generation of temporal PSD history. */
    uint64_t temporalContinuity;
    /** Local discontinuity generation used to reject incoherent temporal history. */
    uint64_t continuity[2];
    /** Number of populated linear-power temporal observations. */
    unsigned temporalCount;
    /** Next circular temporal-history insertion index. */
    unsigned temporalIndex;
    /** Worker-owned linear-power history; never shared with HTTP clients. */
    double history[NG_SPECTRUM_MAX_TEMPORAL_FRAMES][NG_FFT_MAX_SIZE];
    /** Worker-owned temporal power result before logarithmic conversion. */
    double smoothed[NG_FFT_MAX_SIZE];
    /** Owned published result, copied under mutex before telemetry. */
    TNgAnalysisSnapshot snapshot;
    /** Worker-owned startup estimates, separate for the legacy pre/post input taps. */
    int dcKnown[2], dcOffset[2][2];
    /** Format in which each cached offset is expressed; a format change re-estimates it. */
    TNgSampleFormat dcFormat[2];
    /** Number of pairs used for each initial estimate, shared by PSD and histogram. */
    uint32_t dcEstimatePairs[2];
    /** Owned immutable FFT plan; destroyed after worker join. */
    TNgFftPlan *plan;
    /** FFT length associated with cached window coefficients. */
    size_t cachedSize;
    /** Window family associated with cached coefficients. */
    TNgWindow cachedWindow;
    /** Coherent amplitude gain of the cached window. */
    double cachedGain;
    /** Squared window-coefficient sum used for PSD calibration. */
    double cachedEnergy;
    /** Worker-private frame; copied to snapshot only after complete calculation. */
    TNgAnalysisSnapshot workResult;
    /** Owned input stream or workspace; no frontend pointer is retained. */
    int16_t input[2 * NG_FFT_MAX_SIZE];
    /** Widened ADC units after SIMD offset subtraction; native bursts stay immutable. */
    int32_t centered[2 * NG_ANALYSIS_PAIRS];
    /** Owned output workspace, reused only by this worker. */
    int16_t output[2 * NG_FFT_MAX_SIZE];
    /** Worker-owned fixed-point window coefficients. */
    int32_t window[NG_FFT_MAX_SIZE];
    /** Worker-owned uint64 FFT power accumulation buffer. */
    uint64_t power[NG_FFT_MAX_SIZE];
};

/**
 * @brief Return the serialized diagnostic window name.
 *
 * @details
 * Static storage; no allocation and no caller ownership.
 *
 * Parameter window: Window enum.
 *
 * Result: Static window name, or invalid for an unknown enum.
 */
const char *NgWindowName(TNgWindow window)
{
    static const char *names[] = {"rectangular", "hann", "hamming", "blackman", "blackman_harris"};
    return (window >= NG_WINDOW_RECT) && (window <= NG_WINDOW_BLACKMAN_HARRIS) ? names[window]
                                                                               : "invalid";
}

/**
 * @brief Return the serialized temporal averaging name.
 *
 * @details
 * Static storage; no allocation and no caller ownership.
 *
 * Parameter mode: Temporal averaging enum.
 *
 * Result: Static mode name, or invalid for an unknown enum.
 */
const char *NgTemporalName(TNgTemporal mode)
{
    static const char *names[] = {"none", "mean", "ema"};
    return (mode >= NG_TEMPORAL_NONE) && (mode <= NG_TEMPORAL_EMA) ? names[mode] : "invalid";
}

/**
 * @brief Initialize optional spectrum/histogram settings.
 *
 * @details
 * Overwrites the structure with supported defaults; no workers or FFT plans are allocated.
 *
 * Parameter config: Writable diagnostic configuration.
 */
void NgAnalysisDefaults(TNgAnalysisConfig *config)
{
    memset(config, 0, sizeof(*config));
    config->spectrumEnabled = 1;
    config->histogramEnabled = 1;
    config->fftSize = NG_DEFAULT_SPECTRUM_FFT_SIZE;
    config->window = NG_WINDOW_HANN;
    config->overlapPercent = NG_DEFAULT_OVERLAP_PERCENT;
    config->averages = NG_DEFAULT_WELCH_SEGMENTS;
    config->updateHz = NG_DEFAULT_DIAGNOSTIC_UPDATE_HZ;
    config->histogramUpdateHz = NG_DEFAULT_DIAGNOSTIC_UPDATE_HZ;
    config->temporalFrames = NG_DEFAULT_TEMPORAL_FRAMES;
    config->smoothingAlpha = NG_DEFAULT_SMOOTHING_ALPHA;
    config->histogramPairs = NG_DEFAULT_HISTOGRAM_PAIRS;
    config->histogramBins = NG_DEFAULT_HISTOGRAM_BINS;
}

/**
 * @brief Validate diagnostic dimensions, rates and enum selections.
 *
 * @details
 * Also checks the full Welch burst span against the bounded diagnostic capacity. Does not
 * mutate the profile.
 *
 * Parameter config: Configuration to check.
 * Parameter error: Optional validation error destination.
 *
 * Result: NG_OK for supported settings; NG_ERROR otherwise.
 */
int NgAnalysisValidate(const TNgAnalysisConfig *config, TNgError *error)
{
    if (config == NULL || config->fftSize < NG_SPECTRUM_MIN_FFT_SIZE ||
        config->fftSize > NG_FFT_MAX_SIZE || ((config->fftSize) & (config->fftSize - 1)) != 0 ||
        config->window < NG_WINDOW_RECT || config->window > NG_WINDOW_BLACKMAN_HARRIS ||
        config->overlapPercent > NG_SPECTRUM_MAX_OVERLAP_PERCENT || config->averages < 1 ||
        config->averages > NG_SPECTRUM_MAX_AVERAGES || isfinite(config->updateHz) == 0 ||
        config->updateHz < NG_DIAGNOSTIC_MIN_UPDATE_HZ ||
        config->updateHz > NG_DIAGNOSTIC_MAX_UPDATE_HZ ||
        isfinite(config->histogramUpdateHz) == 0 ||
        config->histogramUpdateHz < NG_DIAGNOSTIC_MIN_UPDATE_HZ ||
        config->histogramUpdateHz > NG_DIAGNOSTIC_MAX_UPDATE_HZ ||
        config->temporal < NG_TEMPORAL_NONE || config->temporal > NG_TEMPORAL_EMA ||
        config->temporalFrames < 1 || config->temporalFrames > NG_SPECTRUM_MAX_TEMPORAL_FRAMES ||
        isfinite(config->smoothingAlpha) == 0 || config->smoothingAlpha <= 0 ||
        config->smoothingAlpha > 1 || (config->spectrumPre != 0 && config->spectrumPre != 1) ||
        (config->histogramPre != 0 && config->histogramPre != 1) ||
        (config->density != 0 && config->density != 1) ||
        config->histogramPairs < NG_HISTOGRAM_MIN_PAIRS ||
        config->histogramPairs > NG_ANALYSIS_PAIRS ||
        config->histogramBins < NG_HISTOGRAM_MIN_BINS ||
        config->histogramBins > NG_HISTOGRAM_BINS ||
        ((config->histogramBins) & (config->histogramBins - 1)) != 0 ||
        (config->spectrumEnabled != 0 && config->spectrumEnabled != 1) ||
        (config->histogramEnabled != 0 && config->histogramEnabled != 1)) {
        return NgFail(error,
                      "Analysis: FFT power of two 256..16384, overlap 0..99%%, averages 1..64, "
                      "rates 0.1..30 Hz, temporal frames 1..16, alpha (0,1], histogram "
                      "1024..262144 pairs and 16/32/64/128/256 bins");
    }
    if (config->fftSize +
            (config->averages - 1) * (config->fftSize * (100 - config->overlapPercent) / 100) >
        NG_ANALYSIS_PAIRS) {
        return NgFail(error,
                      "Welch burst exceeds 262144 pairs: reduce segments or increase overlap");
    }
    return NG_OK;
}

/**
 * @brief Compute the contiguous sample span needed for one diagnostic lane.
 *
 * @details
 * PSD span is NFFT plus Welch hops; histogram length is independently configured.
 * Configuration has already passed validation.
 *
 * @param c Validated diagnostic settings.
 * @param kind 0 for PSD, 1 for histogram.
 *
 * @return Required number of complex pairs.
 */
static size_t Required(const TNgAnalysisConfig *c, int kind)
{
    return (kind) != 0
               ? c->histogramPairs
               : c->fftSize + (c->averages - 1) * (c->fftSize * (100 - c->overlapPercent) / 100);
}

/**
 * @brief Calculate PSD range, mean, finite-value counts and a lightweight checksum.
 *
 * @details
 * FNV-1a hashes little-endian signed milli-dB values. Producer quantization must match
 * telemetry/browser checks. Nonfinite values are counted, not repaired. This is an integrity
 * diagnostic, not a cryptographic signature.
 *
 * Parameter bins: Read-only PSD bins in dBFS units.
 * Parameter n: Number of bins.
 * Parameter out: Receives min/max/mean, NaN/Inf counts and checksum.
 */
void NgPsdCheck(const double *bins, size_t n, TNgPsdIntegrity *out)
{
    size_t k;
    size_t finite = 0;
    memset(out, 0, sizeof(*out));
    out->checksum = UINT32_C(2166136261);
    for (k = 0; k < n; ++k) {
        double x = bins[k];
        uint32_t word;
        unsigned j;
        if (isnan(x) != 0) {
            ++out->nanCount;
            continue;
        }
        if (isfinite(x) == 0) {
            ++out->infCount;
            continue;
        }
        if (finite == 0 || x < out->min) {
            out->min = x;
        }
        if (finite == 0 || x > out->max) {
            out->max = x;
        }
        out->mean += x;
        ++finite;
        /* Producer bounds diagnostic values before hashing; avoid invalid float casts. */
        if (x < -1000 || x > 1000) {
            ++out->infCount;
            continue;
        }
        word = (uint32_t)(int32_t)floor(x * 1000 + 0.5);
        for (j = 0; j < 4; ++j) {
            out->checksum = (out->checksum ^ (word & 255)) * UINT32_C(16777619);
            word >>= 8;
        }
    }
    if (finite != 0) {
        out->mean /= (double)finite;
    }
}

/**
 * @brief Compute the arithmetic-floor Q15 window scaling.
 *
 * @details
 * Provides defined negative rounding without relying on signed right shifts. Product range
 * fits int32.
 *
 * @param x Signed sample/window product.
 *
 * @return floor(x / 32768).
 */
static int32_t Shift15(int32_t x)
{
    return x >= 0 ? x / 32768 : -1 - ((-1 - x) / 32768);
}

/** @brief Return whether a leased observation still has an interested consumer. */
static int BurstCurrent(TNgAnalysis *a, const TNgBurst *b)
{
    return NgAtomicGet(&a->stop) == 0 && NgAtomicGet(&a->generation[b->kind]) == b->generation;
}

/** @brief Cancel queued/partial work for one lane while holding its publication mutex. */
static void CancelLane(TNgAnalysis *a, unsigned kind)
{
    int k;
    int generation = NgAtomicGet(&a->generation[kind]);
    NgAtomicSet(&a->generation[kind], generation == INT_MAX ? 0 : generation + 1);
    a->filling[kind] = -1;
    a->nextCapture[kind] = 0;
    ++a->continuity[kind];
    for (k = (int)kind * 2; k < (int)kind * 2 + 2; ++k) {
        if (a->buffers[k].state != 3) {
            a->buffers[k].state = 0;
        }
    }
}

/**
 * @brief Calculate one worker-owned diagnostic burst.
 *
 * @details
 * Uses private FFT/window/power workspaces for PSD or exact statistics/histogram for the
 * independent histogram lane. Linear temporal averaging precedes dB conversion. Caller owns
 * Burst until calculation ends and discards results of stale revisions.
 *
 * @param a Diagnostic instance with worker-only DSP workspace.
 * @param b Immutable complete burst leased to the worker.
 * @param s Receives this calculation result for later locked publication.
 * @return Nonzero for a complete current observation; zero for cancellation or invalid input.
 */
static int Calculate(TNgAnalysis *a, const TNgBurst *b, TNgAnalysisSnapshot *s)
{
    const TNgAnalysisConfig *c = &b->config;
    size_t n = c->fftSize;
    size_t k;
    size_t j;
    unsigned f;
    double gain = 0;
    double energy = 0;
    double start = NgMonotonicSeconds();
    TNgError error = {{0}};
    TNgIqView native = {0};
    int removedDc[2];
    int source = b->kind ? c->histogramPre : c->spectrumPre;
    int fftAttenuated = 0;
    int adcLimit = b->format == NG_FORMAT_CI16       ? 32768
                   : b->format == NG_FORMAT_SC16_Q11 ? 2048
                                                     : 128;
    int fftScale = b->format == NG_FORMAT_CI16 ? 1 : b->format == NG_FORMAT_SC16_Q11 ? 8 : 128;
    if (BurstCurrent(a, b) == 0) {
        return 0;
    }
    native.samples = (const TNgIq8 *)(const void *)b->data.native;
    native.format = b->format;
    native.meta.count = b->count;
    if (!a->dcKnown[source] || a->dcFormat[source] != b->format) {
        if (NgIqCenterBlock(&native, a->centered, removedDc, &error) != NG_OK ||
            BurstCurrent(a, b) == 0)
            return 0;
        a->dcOffset[source][0] = removedDc[0];
        a->dcOffset[source][1] = removedDc[1];
        a->dcFormat[source] = b->format;
        a->dcEstimatePairs[source] = (uint32_t)b->count;
        a->dcKnown[source] = 1;
    } else {
        removedDc[0] = a->dcOffset[source][0];
        removedDc[1] = a->dcOffset[source][1];
        if (NgIqSubtractDc(&native, a->centered, removedDc, &error) != NG_OK ||
            BurstCurrent(a, b) == 0)
            return 0;
    }
    memset(s, 0, sizeof(*s));
    s->histogramMin = -adcLimit;
    s->histogramMax = adcLimit - 1;
    s->sampleFormat = b->format;
    s->revision = b->revision;
    s->epoch = b->epoch;
    s->firstSample = b->first;
    s->observedPairs = b->count;
    s->sampleRateHz = a->sampleRate;
    s->fftSize = n;
    s->config = *c;
    s->capturedSeconds = b->captured;
    if (b->kind == 1) {
        int64_t sums[2] = {0, 0};
        uint64_t squares[2] = {0, 0};
        unsigned rawClipping[2] = {0, 0};
        int histogramLimit = adcLimit;
        TNgComponentStats *stats[2] = {&s->statsI, &s->statsQ};
        s->histogramBins = c->histogramBins;
        s->histogramCount = (uint32_t)c->histogramPairs;
        s->histogramFirst = b->first;
        s->histogramEpoch = b->epoch;
        s->statsI.min = s->statsQ.min = 2 * adcLimit;
        s->statsI.max = s->statsQ.max = -2 * adcLimit;
        for (k = 0; k < c->histogramPairs; ++k) {
            if (k % 1024 == 0 && BurstCurrent(a, b) == 0) {
                return 0;
            }
            for (j = 0; j < 2; ++j) {
                int v = a->centered[2 * k + j];
                int raw = v + removedDc[j];
                sums[j] += v;
                squares[j] += (uint64_t)((int64_t)v * v);
                if (v < stats[j]->min) {
                    stats[j]->min = v;
                }
                if (v > stats[j]->max) {
                    stats[j]->max = v;
                }
                stats[j]->zeros += v == 0;
                rawClipping[j] += (unsigned)(raw == -adcLimit || raw == adcLimit - 1);
                if (v < -adcLimit || v >= adcLimit)
                    histogramLimit = 2 * adcLimit;
            }
        }
        /* Widen the displayed axis only when subtracting the mean exceeds the
         * original ADC range. Never clip centered samples into an end bin. */
        s->histogramMin = -histogramLimit;
        s->histogramMax = histogramLimit - 1;
        for (k = 0; k < c->histogramPairs; ++k) {
            if (k % 1024 == 0 && BurstCurrent(a, b) == 0)
                return 0;
            ++s->histogramI[(unsigned)(a->centered[2 * k] + histogramLimit) * c->histogramBins /
                            (unsigned)(2 * histogramLimit)];
            ++s->histogramQ[(unsigned)(a->centered[2 * k + 1] + histogramLimit) * c->histogramBins /
                            (unsigned)(2 * histogramLimit)];
        }
        for (j = 0; j < 2; ++j) {
            double power = (double)squares[j] / (double)c->histogramPairs;
            stats[j]->mean = (double)sums[j] / (double)c->histogramPairs;
            stats[j]->rms = sqrt(power);
            stats[j]->std = sqrt(fmax(0, power - stats[j]->mean * stats[j]->mean));
            stats[j]->clipping = rawClipping[j];
        }
        s->dcMagnitude = hypot(s->statsI.mean, s->statsQ.mean);
        s->removedDcI = removedDc[0];
        s->removedDcQ = removedDc[1];
        s->dcEstimatePairs = a->dcEstimatePairs[source];
        s->clippedI = s->statsI.clipping;
        s->clippedQ = s->statsQ.clipping;
        s->histogramSeconds = NgMonotonicSeconds();
    }
    if (b->kind == 0) {
        /* Offset subtraction can extend beyond the ADC range. Keep every FFT
         * component inside its Q14 contract, then restore power normalization. */
        for (k = 0; k < 2 * b->count; ++k) {
            if (k % 2048 == 0 && BurstCurrent(a, b) == 0)
                return 0;
            if (a->centered[k] < -adcLimit || a->centered[k] > adcLimit) {
                fftAttenuated = 1;
                break;
            }
        }
        /* Window/twiddle initialization is outside the FFT sample arithmetic.
         * Recomputed per burst here only if its shape/size changes (worker cache). */
        static const double pi2 = 6.2831853071795864769;
        if (a->cachedSize != n || a->cachedWindow != c->window) {
            /* Cancellation can leave coefficients partially overwritten. */
            a->cachedSize = 0;
            for (k = 0; k < n; ++k) {
                double x = pi2 * (double)k / (double)n;
                double w = 1;
                if (k % 1024 == 0 && BurstCurrent(a, b) == 0) {
                    return 0;
                }
                if (c->window == NG_WINDOW_HANN) {
                    w = 0.5 - 0.5 * cos(x);
                } else if (c->window == NG_WINDOW_HAMMING) {
                    w = 0.54 - 0.46 * cos(x);
                } else if (c->window == NG_WINDOW_BLACKMAN) {
                    w = 0.42 - 0.5 * cos(x) + 0.08 * cos(2 * x);
                } else if (c->window == NG_WINDOW_BLACKMAN_HARRIS) {
                    w = 0.35875 - 0.48829 * cos(x) + 0.14128 * cos(2 * x) - 0.01168 * cos(3 * x);
                }
                a->window[k] = (int32_t)lround(w * 32768);
                gain += a->window[k];
                energy += (double)a->window[k] * a->window[k] / (32768.0 * 32768.0);
            }
            a->cachedGain = gain / (32768 * (double)n);
            a->cachedEnergy = energy;
            a->cachedSize = n;
            a->cachedWindow = c->window;
        }
        gain = a->cachedGain;
        memset(a->power, 0, n * sizeof(a->power[0]));
        for (f = 0; f < c->averages; ++f) {
            size_t offset = f * (n * (100 - c->overlapPercent) / 100);
            if (BurstCurrent(a, b) == 0) {
                return 0;
            }
            for (k = 0; k < n; ++k) {
                int iValue, qValue;
                iValue = a->centered[2 * (offset + k)];
                qValue = a->centered[2 * (offset + k) + 1];
                {
                    int32_t product = (int32_t)iValue * fftScale * a->window[k];
                    if (b->format == NG_FORMAT_CI16)
                        product = product >= 0 ? product / 2 : -1 - ((-1 - product) / 2);
                    if (fftAttenuated)
                        product = product >= 0 ? product / 2 : -1 - ((-1 - product) / 2);
                    a->input[2 * k] = (int16_t)Shift15(product);
                }
                {
                    int32_t product = (int32_t)qValue * fftScale * a->window[k];
                    if (b->format == NG_FORMAT_CI16)
                        product = product >= 0 ? product / 2 : -1 - ((-1 - product) / 2);
                    if (fftAttenuated)
                        product = product >= 0 ? product / 2 : -1 - ((-1 - product) / 2);
                    a->input[2 * k + 1] = (int16_t)Shift15(product);
                }
            }
            if (BurstCurrent(a, b) == 0) {
                return 0;
            }
            if (NgFftExecute(a->plan, n, a->input, a->output, &error) != NG_OK) {
                return 0; /* Validated fixed workspace contract. */
            }
            NgPowerAccumulate(a->output, a->power, n);
        }
        s->enbwHz = (double)a->sampleRate * a->cachedEnergy / ((double)n * (double)n * gain * gain);
        if (a->temporalRevision != b->revision || a->temporalEpoch != b->epoch ||
            a->temporalContinuity != b->continuity) {
            a->temporalCount = a->temporalIndex = 0;
            a->temporalRevision = b->revision;
            a->temporalEpoch = b->epoch;
            a->temporalContinuity = b->continuity;
        }
        for (k = 0; k < n; ++k) {
            double value = (double)a->power[k] * (fftAttenuated ? 4.0 : 1.0) /
                           ((double)c->averages * 16384 * 16384 * gain * gain);
            if (c->density != 0) {
                value /= s->enbwHz;
            }
            if (c->temporal == NG_TEMPORAL_MEAN) {
                a->history[a->temporalIndex][k] = value;
                value = 0;
                for (j = 0; j < c->temporalFrames && j <= a->temporalCount; ++j) {
                    value += a->history[j][k];
                }
                value /=
                    a->temporalCount < c->temporalFrames ? a->temporalCount + 1 : c->temporalFrames;
            } else if (c->temporal == NG_TEMPORAL_EMA && a->temporalCount != 0) {
                value = c->smoothingAlpha * value + (1 - c->smoothingAlpha) * a->smoothed[k];
            }
            a->smoothed[k] = value;
        }
        if (a->temporalCount < c->temporalFrames) {
            ++a->temporalCount;
        }
        a->temporalIndex = (a->temporalIndex + 1) % c->temporalFrames;
        s->spectrumBins = n < NG_ANALYSIS_BINS ? n : NG_ANALYSIS_BINS;
        s->binHz = (double)a->sampleRate / (double)n;
        s->peakDbfs = -200;
        for (k = 0; k < s->spectrumBins; ++k) {
            double peak = 0;
            size_t peakIndex = 0;
            size_t group = n / s->spectrumBins;
            for (j = 0; j < group; ++j) {
                size_t shifted = k * group + j;
                size_t index = (shifted + n / 2) % n;
                if (a->smoothed[index] >= peak) {
                    peak = a->smoothed[index];
                    peakIndex = shifted;
                }
            }
            s->spectrumDbfs[k] = peak > 0 ? fmax(-200, 10 * log10(peak)) : -200;
            /* Quantize once, before publication; telemetry must not alter the payload. */
            s->spectrumDbfs[k] = floor(s->spectrumDbfs[k] * 1000 + 0.5) / 1000;
            if (s->spectrumDbfs[k] > s->peakDbfs) {
                s->peakDbfs = s->spectrumDbfs[k];
                s->peakHz = ((double)peakIndex - (double)n / 2) * s->binHz;
            }
        }
    }
    s->producedSeconds = NgMonotonicSeconds();
    NgPsdCheck(s->spectrumDbfs, s->spectrumBins, &s->integrity);
    s->computeMs = (NgMonotonicSeconds() - start) * 1000;
    return BurstCurrent(a, b);
}

/**
 * @brief Process newest ready diagnostic bursts and publish copied snapshots.
 *
 * @details
 * Waits on the ready predicate under mutex; calculation runs outside that mutex. Ready slots
 * become worker-owned until completion. Discards obsolete revisions and stops when requested.
 *
 * @param context Diagnostic instance valid until this thread is joined.
 */
static void Worker(void *context)
{
    NgThreadSchedulingPriority(-1);
    TNgAnalysis *a = context;
    TNgAnalysisSnapshot *s = &a->workResult;
    while (NgAtomicGet(&a->stop) == 0) {
        int index = -1;
        int k;
        int calculated;
        double now;
        NgMutexLock(&a->mutex);
        now = NgMonotonicSeconds();
        for (k = 0; k < 2; ++k) {
            if (a->interestUntil[k] != 0 && now > a->interestUntil[k]) {
                a->interestUntil[k] = 0;
                CancelLane(a, (unsigned)k);
            }
        }
        for (k = 0; k < 4; ++k) {
            if (a->buffers[k].state == 2 && (a->buffers[k].revision != a->revision ||
                                             now > a->interestUntil[a->buffers[k].kind])) {
                a->buffers[k].state = 0;
            }
            if (a->buffers[k].state == 2 &&
                (index < 0 || a->buffers[k].captured > a->buffers[index].captured)) {
                index = k;
            }
        }
        /* Latest-ready wins; stale diagnostics never form a producer backlog. */
        if (index >= 0) {
            for (k = 0; k < 4; ++k) {
                if (k != index && a->buffers[k].state == 2 &&
                    a->buffers[k].kind == a->buffers[index].kind) {
                    a->buffers[k].state = 0;
                    ++a->skipped;
                }
            }
        }
        if (index >= 0) {
            a->buffers[index].state = 3;
        }
        if (index < 0 && NgAtomicGet(&a->stop) == 0) {
            /* Producers and shutdown signal this condition; dormant plots need no polling. */
            NgConditionWait(&a->ready, &a->mutex, UINT_MAX);
        }
        NgMutexUnlock(&a->mutex);
        if (index < 0) {
            continue;
        }
        calculated = Calculate(a, &a->buffers[index], s);
        NgMutexLock(&a->mutex);
        if (calculated != 0 && BurstCurrent(a, &a->buffers[index]) != 0 &&
            s->revision == a->revision &&
            NgMonotonicSeconds() <= a->interestUntil[a->buffers[index].kind]) {
            s->sequence = ++a->sequence;
            s->skippedBursts = a->skipped;
            TNgAnalysisSnapshot *d = &a->snapshot;
            if (a->buffers[index].kind == 0) {
                d->psdSequence++;
                d->spectrumBins = s->spectrumBins;
                memcpy(d->spectrumDbfs, s->spectrumDbfs, sizeof(d->spectrumDbfs));
                d->integrity = s->integrity;
                d->producedSeconds = s->producedSeconds;
                d->epoch = s->epoch;
                d->firstSample = s->firstSample;
                d->observedPairs = s->observedPairs;
                d->fftSize = s->fftSize;
                d->binHz = s->binHz;
                d->enbwHz = s->enbwHz;
                d->peakHz = s->peakHz;
                d->peakDbfs = s->peakDbfs;
                d->capturedSeconds = s->capturedSeconds;
            } else {
                d->histogramSequence++;
                d->histogramFirst = s->histogramFirst;
                d->histogramEpoch = s->histogramEpoch;
                d->histogramSeconds = s->histogramSeconds;
                d->histogramBins = s->histogramBins;
                d->histogramMin = s->histogramMin;
                d->histogramMax = s->histogramMax;
                d->histogramCount = s->histogramCount;
                memcpy(d->histogramI, s->histogramI, sizeof(d->histogramI));
                memcpy(d->histogramQ, s->histogramQ, sizeof(d->histogramQ));
                d->statsI = s->statsI;
                d->statsQ = s->statsQ;
                d->dcMagnitude = s->dcMagnitude;
                d->removedDcI = s->removedDcI;
                d->removedDcQ = s->removedDcQ;
                d->dcEstimatePairs = s->dcEstimatePairs;
                d->clippedI = s->clippedI;
                d->clippedQ = s->clippedQ;
            }
            d->sequence = s->sequence;
            d->revision = s->revision;
            d->config = s->config;
            d->sampleRateHz = s->sampleRateHz;
            d->sampleFormat = s->sampleFormat;
            d->computeMs = s->computeMs;
            d->skippedBursts = s->skippedBursts;
        }
        a->buffers[index].state = 0;
        NgMutexUnlock(&a->mutex);
    }
}

/**
 * @brief Create the bounded diagnostic worker and its private workspaces.
 *
 * @details
 * One sample producer may submit pre/post views. A worker processes independent PSD and
 * histogram bursts; it never retains frontend/ring pointers. The caller owns the instance
 * until NgAnalysisDestroy.
 *
 * Parameter sampleRate: Nominal complex sample rate in pairs per second; positive.
 * Parameter config: Validated diagnostic settings.
 * Parameter out: Receives caller-owned worker instance on success.
 * Parameter error: Optional error destination.
 *
 * Result: NG_OK on success; NG_ERROR for settings, allocation or thread errors.
 */
int NgAnalysisCreate(uint32_t sampleRate, const TNgAnalysisConfig *config, TNgAnalysis **out,
                     TNgError *error)
{
    TNgAnalysis *a;
    if (out == NULL || sampleRate == 0 || NgAnalysisValidate(config, error) != NG_OK) {
        return NG_ERROR;
    }
    *out = NULL;
    a = calloc(1, sizeof(*a));
    if (a == NULL) {
        return NgFail(error, "Cannot allocate analysis workspaces");
    }
    if (NgMutexInit(&a->mutex) != 0) {
        free(a);
        return NgFail(error, "Cannot initialize analysis lock");
    }
    if (NgConditionInit(&a->ready) != 0) {
        NgMutexDestroy(&a->mutex);
        free(a);
        return NgFail(error, "Cannot initialize analysis wakeup");
    }
    if (NgFftCreate(16384, &a->plan, error) != NG_OK) {
        NgConditionDestroy(&a->ready);
        NgMutexDestroy(&a->mutex);
        free(a);
        return NG_ERROR;
    }
    a->interestUntil[0] = a->interestUntil[1] = 1e100; /* Library consumers default active. */
    a->sampleRate = sampleRate;
    a->config = *config;
    a->revision = 1;
    a->snapshot.revision = a->revision;
    a->snapshot.config = *config;
    a->filling[0] = a->filling[1] = -1;
    if (NgThreadStart(&a->thread, Worker, a) != 0) {
        NgFftDestroy(a->plan);
        NgConditionDestroy(&a->ready);
        NgMutexDestroy(&a->mutex);
        free(a);
        return NgFail(error, "Cannot start analysis thread");
    }
    *out = a;
    return NG_OK;
}

/**
 * @brief Stop, join and release the diagnostic worker.
 *
 * @details
 * The sample producer must stop submitting first. May wait for the current calculation to
 * complete. A NULL worker is accepted.
 *
 * Parameter analysis: Diagnostic instance to destroy.
 */
void NgAnalysisDestroy(TNgAnalysis *analysis)
{
    if (analysis != NULL) {
        NgAtomicSet(&analysis->stop, 1);
        NgMutexLock(&analysis->mutex);
        NgConditionSignal(&analysis->ready);
        NgMutexUnlock(&analysis->mutex);
        NgThreadJoin(analysis->thread);
        NgFftDestroy(analysis->plan);
        NgConditionDestroy(&analysis->ready);
        NgMutexDestroy(&analysis->mutex);
        free(analysis);
    }
}

/**
 * @brief Publish new diagnostic settings and invalidate older results.
 *
 * @details
 * Thread-safe with the producer and worker. A new revision discards partial/obsolete bursts
 * and resets temporal accumulation when the worker observes it. Config is copied; the caller
 * need not retain it.
 *
 * Parameter analysis: Live diagnostic worker.
 * Parameter config: New validated settings.
 * Parameter error: Optional error destination.
 *
 * Result: NG_OK on success; NG_ERROR for invalid settings.
 */
int NgAnalysisConfigure(TNgAnalysis *analysis, const TNgAnalysisConfig *config, TNgError *error)
{
    if (NgAnalysisValidate(config, error) != NG_OK) {
        return NG_ERROR;
    }
    NgMutexLock(&analysis->mutex);
    analysis->config = *config;
    ++analysis->revision;
    CancelLane(analysis, 0);
    CancelLane(analysis, 1);
    memset(&analysis->snapshot, 0, sizeof(analysis->snapshot));
    analysis->snapshot.revision = analysis->revision;
    analysis->snapshot.config = *config;
    NgMutexUnlock(&analysis->mutex);
    return NG_OK;
}

/**
 * @brief Renew independent diagnostic consumer-interest deadlines.
 *
 * @details
 * Updates selected lane deadlines under the observer mutex. Mask bit 1 selects PSD and bit 2
 * histogram; expired/zero deadlines discard pending observations and cancel active work
 * between bounded calculation stages. Renewing an expired lane starts a fresh observation.
 * Does not alter receiver dispatch.
 *
 * Parameter analysis: Live diagnostic worker.
 * Parameter mask: Interest mask: 1 PSD, 2 histogram, 3 both.
 * Parameter expiresSeconds: Absolute monotonic deadline in seconds; zero deactivates selected
 * lanes.
 */
void NgAnalysisInterest(TNgAnalysis *analysis, unsigned mask, double expiresSeconds)
{
    unsigned kind;
    double now;
    if (analysis == NULL) {
        return;
    }
    NgMutexLock(&analysis->mutex);
    now = NgMonotonicSeconds();
    for (kind = 0; kind < 2; ++kind) {
        if (((mask) & ((1u) << (kind))) != 0) {
            if (expiresSeconds <= now || analysis->interestUntil[kind] < now) {
                CancelLane(analysis, kind);
            }
            analysis->interestUntil[kind] = expiresSeconds;
        }
    }
    NgMutexUnlock(&analysis->mutex);
}

/**
 * @brief Offer pre- or post-conditioning samples without waiting for diagnostics.
 *
 * @details
 * Single producer only. A try-lock protects bounded copies into private bursts; analysis busy
 * observer skips this block. No FFT or I/O runs here. Indices detect any later discontinuity.
 * Never retain the sample pointer.
 *
 * Parameter analysis: Live diagnostic worker; NULL disables the tap.
 * Parameter view: Borrowed nonempty native integer IQ view valid for the duration of this call.
 * Parameter preConditioning: 1 for pre-conditioning, 0 for post-conditioning.
 *
 * Result: NG_OK for an offered/inactive/null/empty tap; NG_FULL when the try-lock is
 * contended. Valid nonempty views must provide sample storage.
 */
int NgAnalysisSubmitAt(TNgAnalysis *analysis, const TNgIqView *view, int preConditioning)
{
    int kind;
    double now;
    if (analysis == NULL || view == NULL || view->meta.count == 0) {
        return NG_OK;
    }
    if (view->samples == NULL || NgSampleBytes(view->format) == 0) {
        return NG_ERROR;
    }
    if (NgMutexTryLock(&analysis->mutex) == 0) {
        int count = NgAtomicGet(&analysis->contention);
        if (count < INT_MAX) {
            NgAtomicSet(&analysis->contention, count + 1);
        }
        return NG_FULL;
    }
    analysis->offered = 1;
    now = NgMonotonicSeconds();
    for (kind = 0; kind < 2; ++kind) {
        TNgBurst *b;
        size_t copy;
        int k;
        int start = kind * 2;
        int enabled =
            (kind) != 0 ? analysis->config.histogramEnabled : analysis->config.spectrumEnabled;
        int source = (kind) != 0 ? analysis->config.histogramPre : analysis->config.spectrumPre;
        if (analysis->interestUntil[kind] != 0 && now > analysis->interestUntil[kind]) {
            analysis->interestUntil[kind] = 0;
            CancelLane(analysis, (unsigned)kind);
        }
        if (enabled == 0 || source != preConditioning || analysis->interestUntil[kind] == 0) {
            continue;
        }
        if (((view->meta.flags) & (NG_BLOCK_GAP)) != 0) {
            ++analysis->continuity[kind];
        }
        if (analysis->filling[kind] >= 0) {
            b = &analysis->buffers[analysis->filling[kind]];
            if (((view->meta.flags) & (NG_BLOCK_GAP)) != 0 || b->epoch != view->meta.epoch ||
                b->first + b->count != view->meta.firstSample || b->format != view->format) {
                b->state = 0;
                analysis->filling[kind] = -1;
                ++analysis->continuity[kind];
                ++analysis->skipped;
                analysis->nextCapture[kind] = 0;
            }
        }
        if (analysis->filling[kind] < 0) {
            if (now < analysis->nextCapture[kind]) {
                continue;
            }
            for (k = start; k < start + 2; ++k) {
                if (analysis->buffers[k].state == 0) {
                    analysis->filling[kind] = k;
                    break;
                }
            }
            analysis->nextCapture[kind] =
                now + 1.0 / ((kind) != 0 ? analysis->config.histogramUpdateHz
                                         : analysis->config.updateHz);
            if (analysis->filling[kind] < 0) {
                ++analysis->skipped;
                continue;
            }
            b = &analysis->buffers[analysis->filling[kind]];
            b->state = 1;
            b->kind = kind;
            b->format = view->format;
            b->count = 0;
            b->required = Required(&analysis->config, kind);
            b->config = analysis->config;
            b->revision = analysis->revision;
            b->first = view->meta.firstSample;
            b->epoch = view->meta.epoch;
            b->captured = now;
            b->continuity = analysis->continuity[kind];
            b->generation = NgAtomicGet(&analysis->generation[kind]);
        }
        b = &analysis->buffers[analysis->filling[kind]];
        copy = b->required - b->count;
        if (copy > view->meta.count) {
            copy = view->meta.count;
        }
        memcpy(b->data.native + b->count * NgSampleBytes(view->format), view->samples,
               copy * NgSampleBytes(view->format));
        b->count += copy;
        if (b->count == b->required) {
            b->state = 2;
            analysis->filling[kind] = -1;
            NgConditionSignal(&analysis->ready);
        }
    }
    NgMutexUnlock(&analysis->mutex);
    return NG_OK;
}

/**
 * @brief Offer post-conditioning samples using the compatibility convenience wrapper.
 *
 * @details
 * Calls NgAnalysisSubmitAt with post-conditioning and ignores its status. Use the status-
 * returning entry point when contention needs to be observed.
 *
 * Parameter analysis: Live diagnostic worker, or NULL for analysis disabled tap.
 * Parameter view: Borrowed native integer IQ input view.
 */
void NgAnalysisSubmit(TNgAnalysis *analysis, const TNgIqView *view)
{
    NgAnalysisSubmitAt(analysis, view, 0);
}

/**
 * @brief Copy analysis coherent diagnostic snapshot by value.
 *
 * @details
 * Takes the publication mutex briefly. Caller-owned arrays are filled; no borrowed PSD or
 * Burst storage escapes and the result may outlive subsequent publications.
 *
 * Parameter analysis: Live diagnostic worker.
 * Parameter out: Receives complete snapshot including owned PSD and histogram arrays.
 */
void NgAnalysisRead(TNgAnalysis *analysis, TNgAnalysisSnapshot *out)
{
    NgMutexLock(&analysis->mutex);
    *out = analysis->snapshot;
    out->skippedBursts = analysis->skipped;
    out->contentionBlocks = (unsigned)NgAtomicGet(&analysis->contention);
    NgMutexUnlock(&analysis->mutex);
}

/** @brief Set the device-applied nominal sample rate before the first IQ submission.
 * @details Configuration-time operation only; refuses changes after any burst was offered.
 * This is a library-reported nominal rate, not a measured oscillator frequency.
 * Parameter analysis Live newly created diagnostic worker.
 * Parameter sampleRateHz Applied complex pairs/s, positive.
 * Result: NG_OK on update or NG_ERROR if capture has already started.
 */
int NgAnalysisSetSampleRate(TNgAnalysis *analysis, uint32_t sampleRateHz)
{
    int result = NG_OK;
    if (analysis == NULL || sampleRateHz == 0) {
        return NG_ERROR;
    }
    NgMutexLock(&analysis->mutex);
    if (analysis->offered != 0) {
        result = NG_ERROR;
    } else {
        analysis->sampleRate = sampleRateHz;
    }
    NgMutexUnlock(&analysis->mutex);
    return result;
}
