/** @file tracking.h @brief Replaceable tracking strategies on shared native I/Q windows. */
#ifndef NGENE_TRACKING_H
#define NGENE_TRACKING_H
#include "ngene/acquisition.h"
#include "ngene/navigation.h"
#define NG_TRACKING_MAX_TAPS 33u
#define NG_TRACKING_EGNOS_CHANNEL 65u
#define NG_TRACKING_EGNOS_SIGNAL                                                                   \
    4u /**< Presentation identifier, outside the PVT constellation enum. */

/** User settings for one DLL, FLL or PLL coefficient calculation. */
typedef struct {
    double bandwidthHz; /**< Requested one-sided loop noise bandwidth in hertz. */
    double gain;        /**< Positive discriminator/loop gain used by the coefficient model. */
} TNgLoopSettings;

enum { NG_LOOP_DLL, NG_LOOP_FLL, NG_LOOP_PLL, NG_LOOP_COUNT };

/** Frozen per-run numerical and correlator configuration; no automatic precision changes. */
typedef struct {
    TNgLoopSettings loops[NG_TRACKING_EGNOS_SIGNAL]
                         [NG_LOOP_COUNT]; /**< GPS, Galileo, BeiDou and EGNOS. */
    unsigned gpsCoherentMs, gpsTransitionChecks;
    unsigned fllDecisionUpdates, fllDecisionIntervalMs;
    double fllTransitionHz;
    unsigned channelCount;      /**< Total shared receiver slots, 1..64. */
    double egnosSpacingChips;   /**< EGNOS adjacent tap delay in chips. */
    double cn0CutoffDbHz;       /**< Zero disables the optional tracking admission cutoff. */
    double beidouSpacingChips;  /**< BeiDou adjacent tap delay, zero inherits Galileo spacing. */
    double galileoSpacingChips; /**< Galileo adjacent tap delay, central BOC lobe. */
    unsigned gpsChannels;       /**< Slots reserved for GPS; remaining slots are shared. */
    unsigned galileoChannels;   /**< Slots reserved for Galileo; remaining slots are shared. */
    unsigned beidouChannels;    /**< Slots reserved for BeiDou; zero uses shared slots. */
    unsigned autoPriority;      /**< Shared slot preference: 0 first confirmed, 1 GPS, 2 Galileo. */
    unsigned workers;           /**< Fixed tracking worker pool, 1..16. */
    int enabled;                /**< Track confirmed acquisition results when nonzero. */
    int squareCarrier;          /**< Explicit approximate square carrier; precise is default. */
    int xorCorrelation;         /**< Exact sign XOR, independent of carrier mode. */
    unsigned tapCount;          /**< Odd count 3..33, equally spaced around prompt. */
    double spacingChips;        /**< Adjacent tap spacing; E/L are adjacent to prompt. */
    int fastCarrier;         /**< Opt-in for CI8 input: four-lane double carrier recurrence. Still
                          precise (no tables, no phase quantization); differs from the default
                          one-lane recurrence only by double rounding. */
    unsigned mixerBits;      /**< 0: precise recurrence; 8: signed-byte mixer, 16-level carrier. */
    int egnosEnabled;        /**< Search, track and apply EGNOS L1 corrections. */
    int broadcastIonosphere; /**< Apply satellite broadcast ionosphere corrections in PVT. */
    unsigned dopplerSmoothingMs; /**< Output Doppler filter time constant; zero disables. */
} TNgTrackingConfig;

/** Completed coherent integration, in native ADC units per sample. */
typedef struct {
    TNgNavigationSnapshot navigation; /**< Demodulated data; no PVT eligibility implied. */
    unsigned reservation;             /**< 0 automatic, 1 GPS, 2 Galileo. */
    int cn0Valid;           /**< Moment estimate has accumulated enough coherent epochs. */
    double cn0DbHz;         /**< Native-complex M2/M4 estimate; NaN/invalid when unavailable. */
    int mainPeakVerified;   /**< Galileo half-chip probes support the central BOC peak. */
    double codeDelayChips;  /**< Local code delay relative to acquisition prediction. */
    unsigned channelId;     /**< Stable slot identity, independent of assigned PRN. */
    unsigned state;         /**< 0 waiting, 1 acquisition, 2 tracking. */
    unsigned prn;           /**< Zero while unassigned. */
    unsigned constellation; /**< TNgConstellation for the assigned signal. */
    int active;             /**< NCO and loop state valid in current epoch. */
    int pll;              /**< FLL convergence passed; does not establish navigation eligibility. */
    uint64_t epoch;       /**< Continuity generation. */
    uint64_t firstSample; /**< Integration start. */
    uint64_t endSample;   /**< Exclusive integration end. */
    uint64_t integrations;               /**< Completed integrations since seed. */
    double dopplerHz;                    /**< Physical Doppler excluding IF. */
    double codeRateHz;                   /**< Estimated chips per second. */
    double dllError;                     /**< Classic normalized power discriminator. */
    double carrierError;                 /**< Classic Costas discriminator in cycles. */
    unsigned loopCoherentMs;             /**< Effective loop update interval. */
    unsigned tapCount;                   /**< Valid entries of offset/i/q. */
    double offset[NG_TRACKING_MAX_TAPS]; /**< Replica delay in chips. */
    double i[NG_TRACKING_MAX_TAPS];      /**< Mean coherent I. */
    double q[NG_TRACKING_MAX_TAPS];      /**< Mean coherent Q. */
    uint64_t trackingStartSample;        /**< First input sample accepted for this assignment. */
} TNgTrackingSnapshot;
/** Opaque private tracking state; no shared mutable DSP buffers. */
typedef struct TNgTracking TNgTracking;
/** @brief Restore cached ephemeris and clock for the assigned satellite.
 * @param[in,out] tracking Tracker in the caller's exclusive access.
 * @param[in] broadcast Optional cache copied into the tracker.
 * Fresh demodulation must establish the time anchor before PVT can use the cache.
 */
void NgTrackingRestoreBroadcast(TNgTracking *tracking, const TNgBroadcast *broadcast);
/** @brief Initialize the default receiver tracking profile.
 * @param[out] config Caller-owned profile. The default uses the precise carrier
 * and three adjacent taps. GPS uses 1-ms loop updates unless reconfigured.
 */
void NgTrackingDefaults(TNgTrackingConfig *config);
/** @brief Validate the per-signal loop settings, tap geometry and slot allocations.
 * @param[in] config Tracking profile to validate.
 * @param[out] error Optional diagnostic on failure.
 * @return NG_OK on success or NG_ERROR for an invalid profile.
 */
int NgTrackingValidate(const TNgTrackingConfig *config, TNgError *error);
/** @brief Construct one unassigned GPS, Galileo or BeiDou tracker.
 * @param[in] config Valid tracking profile, copied by value.
 * @param[in] stream Native sample format, rate and IF, copied by value.
 * @param[in] constellation GPS, Galileo or BeiDou signal identifier.
 * @param[out] out Receives the caller-owned tracker, or NULL on failure.
 * @param[out] error Optional diagnostic on failure.
 * @return NG_OK on success or NG_ERROR on invalid inputs or allocation failure.
 * @note Serialize operations on each instance. No input I/Q buffer is retained.
 */
int NgTrackingCreate(const TNgTrackingConfig *config, const TNgStreamInfo *stream,
                     unsigned constellation, TNgTracking **out, TNgError *error);
/** @brief Switch an unassigned slot to another supported signal.
 * @param[in,out] tracking Caller-owned unassigned tracker.
 * @param[in] constellation GPS, Galileo or BeiDou signal identifier.
 * @param[out] error Optional diagnostic on failure.
 * @return NG_OK on success or NG_ERROR on invalid inputs.
 */
int NgTrackingSetSignal(TNgTracking *tracking, unsigned constellation, TNgError *error);
/** @brief Initialize the loop state from a confirmed acquisition.
 * @param[in,out] tracking Caller-owned tracker with the corresponding signal selected.
 * @param[in] result Confirmed PRN, Doppler and code phase at capture origin.
 * @param[out] error Optional diagnostic on failure.
 * @return NG_OK on success or NG_ERROR if the assignment is invalid.
 * Subsequent processing propagates code phase from the capture origin and
 * invalidates the assignment if the stream epoch or sample sequence is discontinuous.
 */
int NgTrackingSeed(TNgTracking *tracking, const TNgAcquisitionResult *result, TNgError *error);
/** @brief Consume one native I/Q window and publish completed integrations.
 * @param[in,out] tracking Tracker owned by the calling channel.
 * @param[in] window Immutable sample view, borrowed only until the call returns.
 * @param[out] error Optional diagnostic on failure.
 * @return NG_OK on success or NG_ERROR for an invalid view.
 * A discontinuity invalidates the assignment. Processing does not allocate or perform I/O.
 */
int NgTrackingProcess(TNgTracking *tracking, const TNgIqView *window, TNgError *error);
/** @brief Copy the latest published tracking state.
 * @param[in] tracking Tracker in the caller's exclusive access.
 * @param[out] snapshot Caller-owned destination.
 */
void NgTrackingRead(const TNgTracking *tracking, TNgTrackingSnapshot *snapshot);
/** @brief Set interest in taps beyond the early, prompt and late correlators.
 * @param[in,out] tracking Tracker in the caller's exclusive access.
 * @param[in] observed Nonzero to publish the configured diagnostic taps.
 * The request is latched at the start of the next integration. The receiver
 * always computes the correlators required by the loops.
 */
void NgTrackingObserve(TNgTracking *tracking, int observed);
/** @brief Release the tracker and its private resources.
 * @param[in] tracking Caller-owned tracker, or NULL. No calls may be in progress.
 */
void NgTrackingDestroy(TNgTracking *tracking);
#endif
