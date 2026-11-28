/** @file acquisition.h @brief Replaceable GPS search strategies on bounded common-ring captures. */
#ifndef NGENE_ACQUISITION_H
#define NGENE_ACQUISITION_H
#include "ngene/channel.h"
#include "ngene/dsp/search.h"

/** Acquisition-only options; no tracking or navigation decisions are implied. */
typedef struct {
    double falseAlarmProbability; /**< Per-cell Pfa, default 1e-9; shared noise policy. */
    int enabled;                  /**< Zero disables channel registration. */
    unsigned firstPrn;            /**< First PRN of the selected constellation. */
    unsigned lastPrn;             /**< Last PRN of the selected constellation, inclusive. */
    int squareCarrier;            /**< Squared local sin/cos instead of sinusoidal carrier. */
    int xorCorrelation;           /**< Exact SIMD code sign XOR instead of multiplication. */
    TNgSearchFftMode fftMode;     /**< Coarse PCPS FFT arithmetic; refinement remains native. */
    double cpuBudget; /**< Receiver-run search CPU (thread CPU clock), in cores per signal
                           second; 0 disables.
                           Scheduling only: it delays searches, never changes their decisions.
                           A linked GPS/Galileo pool uses the GPS value. Default 1. */
    unsigned fftSize; /**< Galileo coarse points per 4-ms period; GPS ignores it. 0 automatic:
                           16384 (8192 below 4.096 Msps) with a BOC(1,1) replica. 4096 is the
                           Classic sizing: the subcarrier is removed at the native rate with a
                           fixed phase before compression, and the grid replica is the primary
                           code only. Refinement and confirmation stay native rate. Opt-in. */
} TNgAcquisitionConfig;

/** One search outcome, referenced to the first sample of its capture. */
typedef struct {
    double completedSeconds; /**< Monotonic completion time, scheduling only, never GNSS time. */
    unsigned prn;            /**< PRN in the runner constellation. */
    int confirmed;           /**< Nonzero only after 3 successes out of 4 independent attempts. */
    double dopplerHz;        /**< Physical Doppler, excluding digital IF. */
    double
        codePhaseChips; /**< Local replica phase at firstSample, modulo the primary code length. */
    double peakRatio;   /**< Coarse power divided by its statistical threshold. */
    unsigned successes; /**< Confirmation successes, 0..4. */
    uint64_t epoch;     /**< Common-ring continuity generation. */
    uint64_t firstSample; /**< Capture origin, not a GNSS time. */
} TNgAcquisitionResult;

/** Bounded display of a completed coarse search. Code cells retain the maximum of
 * their original FFT bins; values are power / coarse detection threshold.
 * Display reduction never feeds acquisition decisions. */
#define NG_ACQUISITION_CODE_BINS 1024u
#define NG_ACQUISITION_DOPPLER_BINS 16u
#define NG_ACQUISITION_MAX_DOPPLER_BINS 116u
#define NG_ACQUISITION_MAX_PARALLEL 32u

/** Display data copied from one completed acquisition; consumers own their copy. */
typedef struct {
    int valid;                   /**< Nonzero when the remaining fields describe a result. */
    TNgAcquisitionResult result; /**< Origin and parameters of the acquisition shown. */
    unsigned dopplerBins;        /**< Actual grid rows: GPS 16, Galileo 116. */
    unsigned codePeriodChips;    /**< Primary-code period: GPS 1023, Galileo 4092. */
    unsigned fftSize;            /**< Number of FFT points on the original code grid. */
    double threshold;            /**< Detection threshold on the original power scale. */
    double peakDopplerHz;        /**< Doppler at the coarse grid maximum, in hertz. */
    double peakCodePhaseChips;   /**< Code phase at the coarse grid maximum, in chips. */
    float power[NG_ACQUISITION_MAX_DOPPLER_BINS]
               [NG_ACQUISITION_CODE_BINS]; /**< Power / threshold. */
} TNgAcquisitionSearchSpace;

/** Copied bounded worker telemetry. Results belong to the most recent tested capture per PRN. */
typedef struct {
    double falseAlarmProbability; /**< Per-cell Pfa, default 1e-9; shared noise policy. */
    int enabled;                  /**< Registration enabled for this run. */
    int searching;                /**< Worker is processing an owned capture. */
    unsigned currentPrn;          /**< PRN being searched, zero while waiting. */
    unsigned currentPrns[NG_ACQUISITION_MAX_PARALLEL]; /**< Actual concurrent PRNs by workspace. */
    unsigned activeSearches;   /**< In-flight decisions for this constellation. */
    unsigned parallelLimit;    /**< Current shared-pool admission limit. */
    unsigned parallelCapacity; /**< Preallocated shared CPU workspace capacity. */
    unsigned poolActive;       /**< Shared background jobs, including deferred maps. */
    unsigned poolExecuting;    /**< In-flight DSP stages holding a CPU permit. */
    unsigned executionLimit;   /**< Shared stage cap, independent of slot admission. */
    uint64_t searches;         /**< Completed satellite searches. */
    uint64_t skippedSamples;   /**< Samples not captured while search owns the buffer. */
    uint64_t gaps;             /**< Discontinuities that invalidated the capture generation. */
    TNgAcquisitionResult results[NG_LEGACY_MAX_PRN]; /**< Latest results for PRNs 1..32. */
    char message[256]; /**< Bounded algorithm error/status; no I/O on the sample path. */
} TNgAcquisitionSnapshot;

/** Strategy input. All memory is immutable and owned by the runner until search returns. */
typedef struct {
    const TNgComplex *samples; /**< Native-rate ADC samples, never sign-quantized. */
    size_t count;              /**< Contiguous pairs for the configured capture duration. */
    TNgStreamInfo stream;      /**< Actual rate and digital IF, never inferred from device name. */
    TNgBlockMeta origin;       /**< First capture sample and continuity generation. */
    unsigned displayPrn;       /**< Frozen observer request; zero skips display reduction. */
} TNgAcquisitionCapture;

/** Cooperative cancellation predicate; nonzero invalidates any pending result. */
typedef int (*TNgAcquisitionCancel)(void *context);

/** Alternative algorithms implement this interface, independent of controller and ring storage.
 * search runs exclusively on the acquisition worker. Use public ngene/dsp APIs for FFT/code/
 * carrier/correlation. Never retain capture samples after return; check cancel in long loops.
 * Context belongs to the caller and must outlive NgAcquisitionDestroy. */
typedef struct {
    int (*search)(void *context, const TNgAcquisitionCapture *capture, unsigned prn,
                  const TNgAcquisitionConfig *config, TNgAcquisitionCancel cancel,
                  void *cancelContext, TNgAcquisitionResult *result, TNgError *error);
} TNgAcquisitionStrategy;
/** Opaque owned runner with one bounded capture and one worker. */
typedef struct TNgAcquisition TNgAcquisition;
/** Set GPS PRN1..32, sinusoidal carrier and SIMD products; enabled by default. */
void NgAcquisitionDefaults(TNgAcquisitionConfig *config);
/** Select one display PRN with a short lease; zero removes interest. Never changes decisions. */
void NgAcquisitionObserve(TNgAcquisition *acquisition, unsigned prn);
/** Receiver scheduling only: yield between stages when tracking consumes its budget. */
void NgAcquisitionSetBudget(TNgAcquisition *acquisition, unsigned yieldMilliseconds);
/** Merge idle groups before reception. Accepts NULL operands.
 * The pool supports up to three subscriptions and keeps the first group's budget. */
int NgAcquisitionLink(TNgAcquisition *firstRunner, TNgAcquisition *secondRunner, TNgError *error);
/** Receiver-owned signal clock: credit the search pool with cpuBudget seconds of CPU per
 * second of processed signal (capped). Returns the budget's stage concurrency, or zero when
 * the budget is disabled and the caller keeps its own adaptive limits. */
unsigned NgAcquisitionAdvanceSignal(TNgAcquisition *acquisition, uint64_t epoch, uint64_t endSample,
                                    double sampleRate);
/** @brief Set search concurrency and quotas on the receiver thread.
 * @param[in,out] acquisition Runner sharing the pool, or NULL.
 * @param[in] totalLimit Maximum outstanding searches across the pool.
 * @param[in] quota Per-constellation admission quota.
 * @param[in] executionLimit Maximum stages executing concurrently in the pool.
 */
void NgAcquisitionSetParallelism(TNgAcquisition *acquisition, unsigned totalLimit, unsigned quota,
                                 unsigned executionLimit);
/** @brief Validate the BeiDou B1C search profile.
 * @param[in] config Candidate acquisition settings, including PRNs 1 through 63.
 * @param[out] error Diagnostic when validation fails.
 * @return NG_OK on success or NG_ERROR for an invalid profile.
 */
int NgAcquisitionValidateBeidou(const TNgAcquisitionConfig *config, TNgError *error);
/** @brief Validate the GPS L1 C/A search profile without creating a runner.
 * @param[in] config Candidate acquisition settings, including PRNs 1 through 32.
 * @param[out] error Diagnostic when validation fails.
 * @return NG_OK on success or NG_ERROR for an invalid profile.
 */
int NgAcquisitionValidate(const TNgAcquisitionConfig *config, TNgError *error);
/** @brief Create a GPS L1 C/A runner with the built-in PCPS strategy.
 * @param[in] config Validated per-run search profile.
 * @param[in] stream Actual common stream format, rate and IF from the opened source.
 * @param[out] out Receiver-owned runner, released with NgAcquisitionDestroy().
 * @param[out] channel Borrowed engine registration, valid while the runner lives.
 * @param[out] error Diagnostic when initialization fails.
 * @return NG_OK on success or NG_ERROR on invalid input or allocation failure.
 */
int NgAcquisitionCreate(const TNgAcquisitionConfig *config, const TNgStreamInfo *stream,
                        TNgAcquisition **out, TNgChannel *channel, TNgError *error);
/** @brief Create the Galileo E1-B primary-code search runner.
 * @copydetails NgAcquisitionCreate
 * The runner captures 224 ms at the native stream rate. It may share the
 * engine with the GPS registration.
 */
int NgAcquisitionCreateGalileo(const TNgAcquisitionConfig *config, const TNgStreamInfo *stream,
                               TNgAcquisition **out, TNgChannel *channel, TNgError *error);
/** @brief Create the BeiDou B1C data-component search runner.
 * @copydetails NgAcquisitionCreate
 * The runner captures 570 ms at the native stream rate.
 */
int NgAcquisitionCreateBeidou(const TNgAcquisitionConfig *config, const TNgStreamInfo *stream,
                              TNgAcquisition **out, TNgChannel *channel, TNgError *error);
/** Create a runner using caller-owned strategy/context. Same ring and cancellation contract. */
int NgAcquisitionCreateWithStrategy(const TNgAcquisitionConfig *config, const TNgStreamInfo *stream,
                                    const TNgAcquisitionStrategy *strategy, void *context,
                                    TNgAcquisition **out, TNgChannel *channel, TNgError *error);
/** Register a caller-owned strategy for the primary signal of a supported constellation.
 * Capture duration is in milliseconds at the native input rate. The runner supplies
 * ownership, cancellation and scheduling; the caller supplies signal DSP. */
int NgAcquisitionCreateForConstellation(const TNgAcquisitionConfig *config,
                                        const TNgStreamInfo *stream, TNgConstellation constellation,
                                        unsigned durationMs, const TNgAcquisitionStrategy *strategy,
                                        void *context, TNgAcquisition **out, TNgChannel *channel,
                                        TNgError *error);
/** @brief Copy one coherent publication of all PRN search results.
 * @param[in] acquisition Live runner, or NULL for an empty result.
 * @param[out] results Full PRN-indexed output array. Slot PRN minus one is
 * zero when no result is available.
 */
void NgAcquisitionReadResults(TNgAcquisition *acquisition,
                              TNgAcquisitionResult results[NG_SATELLITE_MAX_PRN]);
/** @brief Copy the latest worker and acquisition counters.
 * @param[in] acquisition Live runner, or NULL for zeroed telemetry.
 * @param[out] snapshot Caller-owned destination. May be read while the worker runs.
 */
void NgAcquisitionRead(TNgAcquisition *acquisition, TNgAcquisitionSnapshot *snapshot);
/** @brief Copy the latest completed search map for one PRN.
 * @param[in] acquisition Live runner, or NULL for an unavailable map.
 * @param[in] prn Satellite PRN in the runner's constellation.
 * @param[out] space Caller-owned buffer, zeroed if a map is unavailable.
 */
void NgAcquisitionReadSearchSpace(TNgAcquisition *acquisition, unsigned prn,
                                  TNgAcquisitionSearchSpace *space);
/** @brief Cancel outstanding searches, join workers and release the runner.
 * @param[in] acquisition Caller-owned runner, or NULL. Remove its borrowed
 * channel registration from use before destruction.
 */
void NgAcquisitionDestroy(TNgAcquisition *acquisition);
/** @brief Set scheduling exclusions for legacy PRNs 1 through 32.
 * @param[in,out] acquisition Runner owned by the calling receiver.
 * @param[in] mask Bit PRN minus one excludes an already assigned satellite.
 * @param[in] paused Nonzero to stop new admissions while allowing in-flight work to finish.
 */
void NgAcquisitionSetAdmission(TNgAcquisition *acquisition, uint32_t mask, int paused);
/** @brief Set scheduling exclusions for a signal with PRNs above 32.
 * @param[in,out] acquisition Runner owned by the calling receiver.
 * @param[in] mask Bit PRN minus one excludes an already assigned satellite.
 * @param[in] paused Nonzero to stop new admissions while allowing in-flight work to finish.
 */
void NgAcquisitionSetAdmissionMask(TNgAcquisition *acquisition, uint64_t mask, int paused);
#endif
