/** @file tracking.h @brief Replaceable tracking strategies on shared native I/Q windows. */
#ifndef NGENE_TRACKING_H
#define NGENE_TRACKING_H
#include "ngene/acquisition.h"
#include "ngene/navigation.h"
#define NG_TRACKING_MAX_TAPS 33u

/** Frozen per-run numerical and correlator configuration; no automatic precision changes. */
typedef struct {
    unsigned channelCount;      /**< Total shared receiver slots, 1..64. */
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
    int fastCarrier; /**< Opt-in for CI8 input: four-lane double carrier recurrence. Still
                          precise (no tables, no phase quantization); differs from the default
                          one-lane recurrence only by double rounding. */
    unsigned mixerBits; /**< 0: precise recurrence; 8: signed-byte mixer, 16-level carrier. */
    int egnosEnabled; /**< Search, track and apply EGNOS L1 corrections. */
    int broadcastIonosphere; /**< Apply satellite broadcast ionosphere corrections in PVT. */
    unsigned dopplerSmoothingMs; /**< Output Doppler filter time constant; zero disables. */
} TNgTrackingConfig;

/** Completed coherent integration, in native ADC units per sample. */
typedef struct {
    TNgNavigationSnapshot navigation; /**< Demodulated data; no PVT eligibility implied. */
    unsigned reservation;     /**< 0 automatic, 1 GPS, 2 Galileo. */
    int cn0Valid;             /**< Moment estimate has accumulated enough coherent epochs. */
    double cn0DbHz;           /**< Native-complex M2/M4 estimate; NaN/invalid when unavailable. */
    int mainPeakVerified;     /**< Galileo half-chip probes support the central BOC peak. */
    double codeDelayChips;    /**< Local code delay relative to acquisition prediction. */
    unsigned channelId;       /**< Stable slot identity, independent of assigned PRN. */
    unsigned state;           /**< 0 waiting, 1 acquisition, 2 tracking. */
    unsigned prn;             /**< Zero while unassigned. */
    unsigned constellation;   /**< TNgConstellation for the assigned signal. */
    int active;               /**< NCO and loop state valid in current epoch. */
    int pll;              /**< FLL convergence passed; does not establish navigation eligibility. */
    uint64_t epoch;       /**< Continuity generation. */
    uint64_t firstSample; /**< Integration start. */
    uint64_t endSample;   /**< Exclusive integration end. */
    uint64_t integrations;               /**< Completed integrations since seed. */
    double dopplerHz;                    /**< Physical Doppler excluding IF. */
    double codeRateHz;                   /**< Estimated chips per second. */
    double dllError;                     /**< Classic normalized power discriminator. */
    double carrierError;                 /**< Classic Costas discriminator in cycles. */
    unsigned tapCount;                   /**< Valid entries of offset/i/q. */
    double offset[NG_TRACKING_MAX_TAPS]; /**< Replica delay in chips. */
    double i[NG_TRACKING_MAX_TAPS];      /**< Mean coherent I. */
    double q[NG_TRACKING_MAX_TAPS];      /**< Mean coherent Q. */
    uint64_t trackingStartSample;      /**< First input sample accepted for this assignment. */
} TNgTrackingSnapshot;
/** Opaque private tracking state; no shared mutable DSP buffers. */
typedef struct TNgTracking TNgTracking;
/** Restore only per-satellite broadcast cache; new demodulation must establish fresh TOW. */
void NgTrackingRestoreBroadcast(TNgTracking *tracking, const TNgBroadcast *broadcast);
/** Initialize precise carrier/products and three taps at -0.5,0,+0.5 chips. */
void NgTrackingDefaults(TNgTrackingConfig *config);
/** Reject unsupported modes, count and spacing; NG_OK or NG_ERROR. */
int NgTrackingValidate(const TNgTrackingConfig *config, TNgError *error);
/** Allocate independent channel resources before reception. constellation is 1 or 2.
 * No sample memory is retained; caller serializes process/seed/read/destroy. */
int NgTrackingCreate(const TNgTrackingConfig *config, const TNgStreamInfo *stream,
                     unsigned constellation, TNgTracking **out, TNgError *error);
/** Select signal-specific geometry on an unassigned slot; no allocation. */
int NgTrackingSetSignal(TNgTracking *tracking, unsigned constellation, TNgError *error);
/** Seed from a confirmed result. Propagate code phase from capture origin to next window.
 * Stale epochs and discontinuities are rejected by Process, requiring a new acquisition. */
int NgTrackingSeed(TNgTracking *tracking, const TNgAcquisitionResult *result, TNgError *error);
/** Consume an immutable common-ring window synchronously with no allocation or I/O. */
int NgTrackingProcess(TNgTracking *tracking, const TNgIqView *window, TNgError *error);
/** Copy the latest complete integration; single owner only. */
void NgTrackingRead(const TNgTracking *tracking, TNgTrackingSnapshot *snapshot);
/** Request observational taps beyond E/P/L; owner calls between dispatched windows.
 * Each integration latches the request at its start; E/P/L and loop decisions always run. */
void NgTrackingObserve(TNgTracking *tracking, int observed);
/** Release private state; accepts NULL. */
void NgTrackingDestroy(TNgTracking *tracking);
#endif
