/** @file acquisition.h @brief Replaceable GPS search strategies on bounded common-ring captures. */
#ifndef NGENE_ACQUISITION_H
#define NGENE_ACQUISITION_H
#include "ngene/channel.h"
#include "ngene/dsp/search.h"

/** Acquisition-only options; no tracking or navigation decisions are implied. */
typedef struct {
    double falseAlarmProbability; /**< Per-cell Pfa, default 1e-9; shared noise policy. */
    int enabled;                  /**< Zero disables channel registration. */
    unsigned firstPrn;            /**< First GPS C/A PRN, 1..32. */
    unsigned lastPrn;             /**< Last PRN, firstPrn..32. */
    int squareCarrier;            /**< Squared local sin/cos instead of sinusoidal carrier. */
    int xorCorrelation;           /**< Exact SIMD code sign XOR instead of multiplication. */
    TNgSearchFftMode fftMode;      /**< Coarse PCPS FFT arithmetic; refinement remains native. */
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
    unsigned prn;            /**< GPS L1 C/A PRN. */
    int confirmed;           /**< Nonzero only after 3 successes out of 4 independent attempts. */
    double dopplerHz;        /**< Physical Doppler, excluding digital IF. */
    double codePhaseChips;   /**< Local replica phase at firstSample, modulo 1023. */
    double peakRatio;        /**< Coarse power divided by its statistical threshold. */
    unsigned successes;      /**< Confirmation successes, 0..4. */
    uint64_t epoch;          /**< Common-ring continuity generation. */
    uint64_t firstSample;    /**< Capture origin, not a GNSS time. */
} TNgAcquisitionResult;

/** Bounded display of a completed coarse search. Code cells retain the maximum of
 * their original FFT bins; values are power / coarse detection threshold.
 * Display reduction never feeds acquisition decisions. */
#define NG_ACQUISITION_CODE_BINS 1024u
#define NG_ACQUISITION_DOPPLER_BINS 16u
#define NG_ACQUISITION_MAX_DOPPLER_BINS 116u
#define NG_ACQUISITION_MAX_PARALLEL 32u

typedef struct {
    int valid;
    TNgAcquisitionResult result;
    unsigned dopplerBins;     /**< Actual grid rows: GPS 16, Galileo 116. */
    unsigned codePeriodChips; /**< Primary-code period: GPS 1023, Galileo 4092. */
    unsigned fftSize;
    double threshold;
    double peakDopplerHz;
    double peakCodePhaseChips;
    float power[NG_ACQUISITION_MAX_DOPPLER_BINS][NG_ACQUISITION_CODE_BINS];
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
    TNgAcquisitionResult results[32]; /**< Latest result per GPS PRN. */
    char message[256]; /**< Bounded algorithm error/status; no I/O on the sample path. */
} TNgAcquisitionSnapshot;

/** Strategy input. All memory is immutable and owned by the runner until search returns. */
typedef struct {
    const TNgComplex *samples; /**< Native-rate ADC samples, never sign-quantized. */
    size_t count;              /**< At least 56 ms of contiguous pairs. */
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
/** Merge strategy subscriptions into one pool before reception; NULL operands are accepted. */
int NgAcquisitionLink(TNgAcquisition *gps, TNgAcquisition *galileo, TNgError *error);
/** Receiver-owned signal clock: credit the search pool with cpuBudget seconds of CPU per
 * second of processed signal (capped). Returns the budget's stage concurrency, or zero when
 * the budget is disabled and the caller keeps its own adaptive limits. */
unsigned NgAcquisitionAdvanceSignal(TNgAcquisition *acquisition, uint64_t epoch,
                                    uint64_t endSample, double sampleRate);
/** Receiver-owned adaptive global concurrency and per-constellation free-slot quota. */
void NgAcquisitionSetParallelism(TNgAcquisition *acquisition, unsigned totalLimit, unsigned quota,
                                 unsigned executionLimit);
/** Validate PRNs and boolean options without allocating or opening hardware. */
int NgAcquisitionValidate(const TNgAcquisitionConfig *config, TNgError *error);
/** Create the default Classic-derived PCPS strategy. Allocates before reception.
 * stream must contain the actual opened source metadata; supports CI8 and CI16 common streams.
 * channel receives a borrowed registration, valid until runner destruction. */
int NgAcquisitionCreate(const TNgAcquisitionConfig *config, const TNgStreamInfo *stream,
                        TNgAcquisition **out, TNgChannel *channel, TNgError *error);
/** Register Galileo E1-B PRNs 1..32 with an independent 224-ms capture worker.
 * Same native precision/ownership/cancellation and completed-map contract as GPS.
 * Both registrations may share an engine; callers may override instance identities. */
int NgAcquisitionCreateGalileo(const TNgAcquisitionConfig *config, const TNgStreamInfo *stream,
                               TNgAcquisition **out, TNgChannel *channel, TNgError *error);
/** Create a runner using caller-owned strategy/context. Same ring and cancellation contract. */
int NgAcquisitionCreateWithStrategy(const TNgAcquisitionConfig *config, const TNgStreamInfo *stream,
                                    const TNgAcquisitionStrategy *strategy, void *context,
                                    TNgAcquisition **out, TNgChannel *channel, TNgError *error);
/** Copy worker telemetry; safe concurrently with sample callbacks and search. */
void NgAcquisitionRead(TNgAcquisition *acquisition, TNgAcquisitionSnapshot *snapshot);
/** Copy the last completed map for PRN 1..32; zero output if unavailable. */
void NgAcquisitionReadSearchSpace(TNgAcquisition *acquisition, unsigned prn,
                                  TNgAcquisitionSearchSpace *space);
/** Cancel/join worker and free owned memory after the engine has stopped using channel.
 * Accepts NULL. Results remain acquisition observations; no fake TRACKING transition. */
void NgAcquisitionDestroy(TNgAcquisition *acquisition);
/** Set scheduling exclusions and admission on the owning receiver thread.
 * mask bit PRN-1 excludes a satellite already assigned to tracking. Pausing admits
 * no new capture/search; an in-flight single-satellite decision may finish unchanged. */
void NgAcquisitionSetAdmission(TNgAcquisition *acquisition, uint32_t mask, int paused);
#endif
