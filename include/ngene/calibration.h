/** @file calibration.h @brief Stopped-receiver SDR gain calibration. */
#ifndef NGENE_CALIBRATION_H
#define NGENE_CALIBRATION_H
#include "ngene/frontend.h"

/** Native ADC loading and distribution measurements for one sample window. */
typedef struct {
    double rmsFraction;        /**< Pooled AC RMS as a fraction of ADC half scale. */
    double clippedFraction;    /**< Fraction of I and Q components near either rail. */
    double gaussianError;      /**< Combined deviation from Gaussian kurtosis and CDF. */
    double dcFraction;         /**< Magnitude of complex DC as a fraction of ADC half scale. */
    double iqImbalance;        /**< Absolute difference of I/Q RMS divided by their sum. */
    double score;              /**< Relative quality penalty used to rank gain candidates. */
    int acceptable;            /**< Nonzero when the candidate passes all rejection checks. */
    unsigned rejectionReasons; /**< Bit set of NG_GAIN_* rejection causes. */
} TNgGainQuality;

enum {
    NG_GAIN_WEAK = 1u,
    NG_GAIN_OVERLOAD = 2u,
    NG_GAIN_DISTRIBUTION = 4u,
    NG_GAIN_IMBALANCE = 8u
};

/** Winning candidate and cumulative diagnostics from a stopped-receiver gain sweep. */
typedef struct {
    TNgConfig selected;            /**< Winning native gain allocation. */
    TNgFrontendCaps capabilities;  /**< Detected native gain capabilities. */
    TNgGainQuality quality;        /**< Aggregated measurements of the winning candidate. */
    unsigned tested;               /**< Number of gain settings evaluated. */
    int accepted;                  /**< Nonzero once a candidate passes the selection criteria. */
    unsigned rejectedWeak;         /**< Candidates with insufficient RMS. */
    unsigned rejectedOverload;     /**< Candidates with excessive RMS or clipping. */
    unsigned rejectedDistribution; /**< Candidates with excessive distribution error. */
    unsigned rejectedImbalance;    /**< Candidates with excessive I/Q imbalance. */
} TNgGainCalibration;

/** Optional cooperative cancellation and copied progress; called on the calibration worker.
 * Progress must not retain either borrowed argument. No callback runs on an SDR callback. */
typedef struct {
    int (*cancelled)(void *context); /**< Return nonzero to request cancellation. */
    /** Receive borrowed snapshots valid only for the duration of the call. */
    void (*progress)(void *context, const TNgConfig *candidate, const TNgGainCalibration *result);
    void *context; /**< Caller-owned callback data that outlives the sweep. */
} TNgGainCalibrationCallbacks;

/** Analyze native ADC levels, independent of the receiver's display histogram bins.
 * Reject non-Gaussian, clipped, imbalanced or weak input distributions.
 * DC is measured and penalized, not tested against the standard error of the mean.
 * This evaluates ADC loading, not GNSS C/N0 or receiver sensitivity.
 * @param view Borrowed native ADC sample window, interleaved by complex pair.
 * @param quality Destination for loading statistics and rejection reasons.
 * @param error Optional diagnostic for input or workspace failures.
 * @return NG_OK for a measured window, even if the gain is rejected, or NG_ERROR.
 */
int NgGainAnalyze(const TNgIqView *view, TNgGainQuality *quality, TNgError *error);
/** Test live SDR gains while stopped using the identified device's allocation policy.
 * HackRF exhausts distinct physical states and prefers earlier gain within each
 * accepted equal-total-gain/RF-amp group. Among accepted HackRF groups, prefer
 * greater physical early-stage gain first, then the loading score. Weak or
 * overloaded observations cannot acquire that priority.
 * RTL-SDR uses manual native allocations; E4000 also sweeps all 54 IF totals
 * with earlier IF stages preferred. At equal RF+IF total prefer RF gain.
 * bladeRF retains its native gain table.
 * Unsupported/unknown devices fail before RX rather than pretending to calibrate.
 * Three non-overlapping 32768-pair windows must pass per setting. RTL keeps
 * USB RX running and gates publication during gain changes/statistics; other
 * drivers stop RX before statistics. Duration depends on the device and search:
 * several minutes for a frontend with many combinations (HackRF: up to 384,
 * E4000: 14 native RF gains x 54 IF totals = 756).
 * @param config Starting frontend configuration. The receiver must be stopped.
 * @param result Destination for the selected allocation and sweep diagnostics.
 * @param error Optional diagnostic for unsupported configuration or a failed sweep.
 * @return NG_OK for an accepted allocation or NG_ERROR on failure.
 */
int NgGainCalibrate(const TNgConfig *config, TNgGainCalibration *result, TNgError *error);
/** Run the same gain sweep with optional cancellation and progress reporting.
 * @param config Starting frontend configuration. The receiver must be stopped.
 * @param result Destination for the selected allocation and sweep diagnostics.
 * @param callbacks Optional callbacks invoked synchronously on the calibration worker.
 * @param error Optional diagnostic for unsupported configuration or a failed sweep.
 * @return NG_OK for an accepted allocation, NG_END after cancellation and cleanup,
 *         or NG_ERROR on failure.
 */
int NgGainCalibrateControlled(const TNgConfig *config, TNgGainCalibration *result,
                              const TNgGainCalibrationCallbacks *callbacks, TNgError *error);
#endif
