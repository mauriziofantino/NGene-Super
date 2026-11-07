/** @file calibration.h @brief Stopped-receiver SDR gain calibration. */
#ifndef NGENE_CALIBRATION_H
#define NGENE_CALIBRATION_H
#include "ngene/frontend.h"

typedef struct {
    double rmsFraction;
    double clippedFraction;
    double gaussianError;
    double dcFraction;
    double iqImbalance;
    double score;
    int acceptable;
    unsigned rejectionReasons;
} TNgGainQuality;

enum { NG_GAIN_WEAK = 1u, NG_GAIN_OVERLOAD = 2u,
       NG_GAIN_DISTRIBUTION = 4u, NG_GAIN_IMBALANCE = 8u };

typedef struct {
    TNgConfig selected;
    TNgFrontendCaps capabilities;
    TNgGainQuality quality;
    unsigned tested;
    int accepted;
    unsigned rejectedWeak, rejectedOverload, rejectedDistribution, rejectedImbalance;
} TNgGainCalibration;

/** Optional cooperative cancellation and copied progress; called on the calibration worker.
 * Progress must not retain either borrowed argument. No callback runs on an SDR callback. */
typedef struct {
    int (*cancelled)(void *context);
    void (*progress)(void *context, const TNgConfig *candidate, const TNgGainCalibration *result);
    void *context;
} TNgGainCalibrationCallbacks;

/** Analyze native ADC levels, independent of the receiver's display histogram bins.
 * Reject non-Gaussian, clipped, imbalanced or weak input distributions.
 * DC is measured and penalized, not tested against the standard error of the mean.
 * This evaluates ADC loading, not GNSS C/N0 or receiver sensitivity.
 */
int NgGainAnalyze(const TNgIqView *view, TNgGainQuality *quality, TNgError *error);
/** Test live SDR gains while stopped using the identified device's allocation policy.
 * HackRF exhausts distinct physical states and prefers earlier gain within each
 * accepted equal-total-gain/RF-amp group; loading score chooses between groups.
 * RTL-SDR uses manual native allocations; bladeRF retains its native gain table.
 * Unsupported/unknown devices fail before RX rather than pretending to calibrate.
 * Three independent 32768-pair windows must pass per setting. RX stops before
 * statistical analysis; this may take
 * several minutes for a frontend with many combinations (HackRF: up to 384).
 */
int NgGainCalibrate(const TNgConfig *config, TNgGainCalibration *result, TNgError *error);
/** NG_END means cancelled, after RX stops and frontend resources are released. */
int NgGainCalibrateControlled(const TNgConfig *config, TNgGainCalibration *result,
                              const TNgGainCalibrationCallbacks *callbacks, TNgError *error);
#endif
