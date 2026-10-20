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
} TNgGainQuality;

typedef struct {
    TNgConfig selected;
    TNgFrontendCaps capabilities;
    TNgGainQuality quality;
    unsigned tested;
    int accepted;
} TNgGainCalibration;

/** Analyze native ADC levels, independent of the receiver's display histogram bins.
 * Reject non-Gaussian, clipped, DC-biased or weak input distributions.
 */
int NgGainAnalyze(const TNgIqView *view, TNgGainQuality *quality, TNgError *error);
/** Test every gain setting exposed by the selected live SDR while stopped.
 * Three independent 32768-pair windows are compared per setting; this may take
 * several minutes for a frontend with many combinations (HackRF: 384).
 */
int NgGainCalibrate(const TNgConfig *config, TNgGainCalibration *result, TNgError *error);
#endif
