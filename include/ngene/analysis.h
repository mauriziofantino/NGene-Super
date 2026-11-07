/** @file analysis.h @brief Optional bounded diagnostics, independent of GUI/HTTP. */
#ifndef NGENE_ANALYSIS_H
#define NGENE_ANALYSIS_H
#include "ngene/types.h"
/** @brief Maximum telemetry PSD bins; larger FFTs are reduced for display. */
#define NG_ANALYSIS_BINS 1024
/** @brief All signed-int8 levels, including both saturation endpoints. */
#define NG_HISTOGRAM_BINS 256
/** @brief Maximum copied complex-pair diagnostic observation capacity. */
#define NG_ANALYSIS_PAIRS 262144

/** @brief Supported analysis-window families. */
typedef enum {
    NG_WINDOW_RECT,
    NG_WINDOW_HANN,
    NG_WINDOW_HAMMING,
    NG_WINDOW_BLACKMAN,
    NG_WINDOW_BLACKMAN_HARRIS
} TNgWindow;

/** @brief Linear-power temporal averaging modes. */
typedef enum { NG_TEMPORAL_NONE, NG_TEMPORAL_MEAN, NG_TEMPORAL_EMA } TNgTemporal;

/** @brief Independent optional spectrum and histogram configuration. */
typedef struct {
    /** Enable the optional PSD producer; no subscriber means no FFT work. */
    int spectrumEnabled;
    /** Enable the optional I/Q histogram producer independently of the PSD. */
    int histogramEnabled;
    /** Discrete complex FFT length; analysis accepts 256 through 16384 powers of two. */
    size_t fftSize;
    /** Window applied before each integer FFT. */
    TNgWindow window;
    /** Welch segment overlap in percent, from 0 through 99. */
    unsigned overlapPercent;
    /** Number of overlapping Welch segments accumulated per PSD frame. */
    unsigned averages;
    /** Maximum PSD capture cadence in frames per second. */
    double updateHz;
    /** Independent histogram capture cadence in frames per second. */
    double histogramUpdateHz;
    /** Number of contiguous complex pairs per histogram/statistics observation. */
    size_t histogramPairs;
    /** Uniform signed-int8 bins; supports 256 individual ADC levels. */
    unsigned histogramBins;
    /** Nonzero selects raw frontend samples; zero selects conditioned samples. */
    int spectrumPre;
    /** Nonzero selects raw frontend samples independently of spectrumPre. */
    int histogramPre;
    /** Nonzero reports dBFS/Hz; zero reports dBFS/bin. */
    int density;
    /** Linear-power temporal averaging mode; never average logarithmic bins. */
    TNgTemporal temporal;
    /** History length used by the temporal mean mode. */
    unsigned temporalFrames;
    /** Weight of newest linear-power frame, (0,1]. */
    double smoothingAlpha;
} TNgAnalysisConfig;

/** @brief Population statistics of one signed ADC component. */
typedef struct {
    /** Arithmetic mean over this observation in the component ADC units. */
    double mean;
    /** Population standard deviation in component ADC units. */
    double std;
    /** Root-mean-square amplitude in component ADC units. */
    double rms;
    /** Minimum observed component value. */
    int min;
    /** Maximum observed component value. */
    int max;
    /** Original input stream endpoint count, before mean subtraction; not physical ADC clipping. */
    uint32_t clipping;
    /** Number of exactly zero component values. */
    uint32_t zeros;
} TNgComponentStats;

/** @brief Numeric and checksum diagnostics for one immutable PSD frame. */
typedef struct {
    /** Minimum PSD bin in the configured logarithmic units. */
    double min;
    /** Maximum PSD bin in the configured logarithmic units. */
    double max;
    /** Arithmetic mean of the published logarithmic PSD bins; used for integrity checks. */
    double mean;
    /** Number of non-finite NaN PSD bins before publication. */
    uint32_t nanCount;
    /** Number of positive or negative infinite PSD bins before publication. */
    uint32_t infCount;
    /** FNV-1a checksum of the published PSD representation. */
    uint32_t checksum;
} TNgPsdIntegrity;

/** @brief Owned diagnostic results; safe after later producer publications. */
typedef struct {
    /** Monotonic publication identifier for diagnostic snapshots. */
    uint64_t sequence;
    /** Configuration revision associated with this immutable snapshot. */
    uint64_t revision;
    /** Continuity generation; changes when the stream time reference is reset. */
    uint64_t epoch;
    /** First complex-pair index observed in the current continuity epoch. */
    uint64_t firstSample;
    /** Diagnostic captures skipped because bounded worker storage was busy. */
    uint64_t skippedBursts;
    /** Optional diagnostic submissions skipped to avoid waiting on a mutex. */
    uint64_t contentionBlocks;
    /** Monotonic identifier changed only when a new PSD is published. */
    uint64_t psdSequence;
    /** Monotonic identifier changed only for a new histogram observation. */
    uint64_t histogramSequence;
    /** First complex-pair index of the independent histogram observation. */
    uint64_t histogramFirst;
    /** Continuity epoch of the independent histogram observation. */
    uint64_t histogramEpoch;
    /** Actual number of complex pairs used for the PSD observation. */
    size_t observedPairs;
    /** Number of populated spectrumDbfs elements. */
    size_t spectrumBins;
    /** Uniform signed-int8 bins; supports 256 individual ADC levels. */
    unsigned histogramBins;
    /** Lowest and highest signed ADC levels represented by the histogram bins. */
    int histogramMin;
    /** Highest ADC level represented by the histogram bins. */
    int histogramMax;
    /** Native representation of the observed samples. */
    TNgSampleFormat sampleFormat;
    /** Counts for signed in-phase ADC levels grouped into histogramBins. */
    uint32_t histogramI[NG_HISTOGRAM_BINS];
    /** Counts for signed quadrature ADC levels grouped into histogramBins. */
    uint32_t histogramQ[NG_HISTOGRAM_BINS];
    /** Number of complex pairs in the histogram observation. */
    uint32_t histogramCount;
    /** In-phase endpoint count for this histogram observation. */
    uint32_t clippedI;
    /** Quadrature endpoint count for this histogram observation. */
    uint32_t clippedQ;
    /** In-phase moments calculated from the histogram observation. */
    TNgComponentStats statsI;
    /** Quadrature moments calculated from the histogram observation. */
    TNgComponentStats statsQ;
    /** Magnitude hypot(mean I, mean Q) in ADC units; no bin is hidden. */
    double dcMagnitude;
    /** Fixed rounded startup offsets subtracted before histogram statistics/bins. */
    int removedDcI, removedDcQ;
    /** Pairs in the initial DC estimate; independent of the current histogram size. */
    uint32_t dcEstimatePairs;
    /** Owned FFT-shifted PSD bins in the configured logarithmic units. */
    double spectrumDbfs[NG_ANALYSIS_BINS];
    /** Statistics and checksum of this published PSD frame. */
    TNgPsdIntegrity integrity;
    /** Monotonic process time when the PSD was completed; not GNSS time. */
    double producedSeconds;
    /** Monotonic process time when the histogram was completed. */
    double histogramSeconds;
    /** Equivalent noise bandwidth of the configured window in hertz. */
    double enbwHz;
    /** Physical FFT-bin spacing, sample rate divided by FFT size. */
    double binHz;
    /** Peak PSD frequency relative to hardware baseband, in hertz. */
    double peakHz;
    /** Peak PSD level in the selected logarithmic units. */
    double peakDbfs;
    /** Diagnostic worker processing time in milliseconds. */
    double computeMs;
    /** Monotonic process time when this input observation was captured. */
    double capturedSeconds;
    /** Discrete complex FFT length; analysis accepts 256 through 16384 powers of two. */
    size_t fftSize;
    /** Complex pairs per second; never the interleaved component byte rate. */
    uint32_t sampleRateHz;
    /** Copied configuration for this snapshot; no borrowed strings. */
    TNgAnalysisConfig config;
} TNgAnalysisSnapshot;
/** @brief Opaque owned TNgAnalysis handle; release with the matching destroy operation. */
typedef struct TNgAnalysis TNgAnalysis;
/**
 * @brief Initialize optional spectrum/histogram settings.
 *
 * @details
 * Overwrites the structure with supported defaults; no workers or FFT plans are allocated.
 *
 * @param config Writable diagnostic configuration.
 */
void NgAnalysisDefaults(TNgAnalysisConfig *config);
/**
 * @brief Validate diagnostic dimensions, rates and enum selections.
 *
 * @details
 * Also checks the full Welch burst span against the bounded diagnostic capacity. Does not
 * mutate the profile.
 *
 * @param config Configuration to check.
 * @param error Optional validation error destination.
 *
 * @return NG_OK for supported settings; NG_ERROR otherwise.
 */
int NgAnalysisValidate(const TNgAnalysisConfig *config, TNgError *error);
/**
 * @brief Create the bounded diagnostic worker and its private workspaces.
 *
 * @details
 * One sample producer may submit pre/post views. A worker processes independent PSD and
 * histogram bursts; it never retains frontend/ring pointers. The caller owns the instance
 * until NgAnalysisDestroy. Each input tap estimates DC once from its first complete burst
 * and shares that fixed rounded offset between PSD and histograms. A format change estimates
 * again. Recreate the instance after changing hardware gain or tuning.
 *
 * @param sampleRate Nominal complex sample rate in pairs per second; positive.
 * @param config Validated diagnostic settings.
 * @param out Receives caller-owned worker instance on success.
 * @param error Optional error destination.
 *
 * @return NG_OK on success; NG_ERROR for settings, allocation or thread errors.
 */
int NgAnalysisCreate(uint32_t sampleRate, const TNgAnalysisConfig *config, TNgAnalysis **out,
                     TNgError *error);
/** @brief Set the device-applied nominal sample rate before the first IQ submission.
 * @details Configuration-time operation only; refuses changes after any burst was offered.
 * This is a library-reported nominal rate, not a measured oscillator frequency.
 * @param analysis Live newly created diagnostic worker.
 * @param sampleRateHz Applied complex pairs/s, positive.
 * @return NG_OK on update or NG_ERROR if capture has already started.
 */
int NgAnalysisSetSampleRate(TNgAnalysis *analysis, uint32_t sampleRateHz);
/**
 * @brief Stop, join and release the diagnostic worker.
 *
 * @details
 * The sample producer must stop submitting first. May wait for the current calculation to
 * complete. A NULL worker is accepted.
 *
 * @param analysis Diagnostic instance to destroy.
 */
void NgAnalysisDestroy(TNgAnalysis *analysis);
/**
 * @brief Publish new diagnostic settings and invalidate older results.
 *
 * @details
 * Thread-safe with the producer and worker. A new revision discards partial/obsolete bursts
 * and resets temporal accumulation when the worker observes it. Config is copied; the caller
 * need not retain it. Fixed DC estimates are retained across diagnostic setting changes.
 *
 * @param analysis Live diagnostic worker.
 * @param config New validated settings.
 * @param error Optional error destination.
 *
 * @return NG_OK on success; NG_ERROR for invalid settings.
 */
int NgAnalysisConfigure(TNgAnalysis *analysis, const TNgAnalysisConfig *config, TNgError *error);
/**
 * @brief Offer pre- or post-conditioning samples without waiting for diagnostics.
 *
 * @details
 * Single producer only. A try-lock protects bounded copies into private bursts; a busy
 * observer skips this block. No FFT or I/O runs here. Indices detect any later discontinuity.
 * Never retain the sample pointer.
 *
 * @param analysis Live diagnostic worker; NULL disables the tap.
 * @param view Borrowed nonempty native integer IQ view valid for the duration of this call.
 * @param preConditioning 1 for pre-conditioning, 0 for post-conditioning.
 *
 * @return NG_OK for an offered/inactive/null/empty tap; NG_FULL when the try-lock is
 * contended. Valid nonempty views must provide sample storage.
 */
int NgAnalysisSubmitAt(TNgAnalysis *analysis, const TNgIqView *view, int preConditioning);
/**
 * @brief Offer post-conditioning samples using the compatibility convenience wrapper.
 *
 * @details
 * Calls NgAnalysisSubmitAt with post-conditioning and ignores its status. Use the status-
 * returning entry point when contention needs to be observed.
 *
 * @param analysis Live diagnostic worker, or NULL for a disabled tap.
 * @param view Borrowed native integer IQ input view.
 */
void NgAnalysisSubmit(TNgAnalysis *analysis, const TNgIqView *view);
/**
 * @brief Renew independent diagnostic consumer-interest deadlines.
 *
 * @details
 * Updates selected lane deadlines under the observer mutex. Mask bit 1 selects PSD and bit 2
 * histogram; expired/zero deadlines discard queued and partial observations and cancel
 * in-flight calculations between bounded stages. Renewing an expired lane starts a fresh
 * observation and temporal history. The controller aggregates independent consumer leases.
 * Does not alter receiver dispatch.
 *
 * @param analysis Live diagnostic worker.
 * @param mask Interest mask: 1 PSD, 2 histogram, 3 both.
 * @param expiresSeconds Absolute monotonic deadline in seconds; zero deactivates selected
 * lanes.
 */
void NgAnalysisInterest(TNgAnalysis *analysis, unsigned mask, double expiresSeconds);
/**
 * @brief Copy a coherent diagnostic snapshot by value.
 *
 * @details
 * Takes the publication mutex briefly. Caller-owned arrays are filled; no borrowed PSD or
 * Burst storage escapes and the result may outlive subsequent publications.
 *
 * @param analysis Live diagnostic worker.
 * @param out Receives complete snapshot including owned PSD and histogram arrays.
 */
void NgAnalysisRead(TNgAnalysis *analysis, TNgAnalysisSnapshot *out);
/**
 * @brief Calculate PSD range, mean, finite-value counts and a lightweight checksum.
 *
 * @details
 * FNV-1a hashes little-endian signed milli-dB values. Producer quantization must match
 * telemetry/browser checks. Nonfinite values are counted, not repaired. This is an integrity
 * diagnostic, not a cryptographic signature.
 *
 * @param bins Read-only PSD bins in dBFS units.
 * @param n Number of bins.
 * @param out Receives min/max/mean, NaN/Inf counts and checksum.
 */
void NgPsdCheck(const double *bins, size_t n, TNgPsdIntegrity *out);
/**
 * @brief Return the serialized diagnostic window name.
 *
 * @details
 * Static storage; no allocation and no caller ownership.
 *
 * @param window Window enum.
 *
 * @return Static window name, or invalid for an unknown enum.
 */
const char *NgWindowName(TNgWindow window);
/**
 * @brief Return the serialized temporal averaging name.
 *
 * @details
 * Static storage; no allocation and no caller ownership.
 *
 * @param mode Temporal averaging enum.
 *
 * @return Static mode name, or invalid for an unknown enum.
 */
const char *NgTemporalName(TNgTemporal mode);
#endif
