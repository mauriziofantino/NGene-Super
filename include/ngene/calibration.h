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

/** Analyze native complex samples; a non-Gaussian, clipped or weak input is rejected. */
int NgGainAnalyze(const TNgIqView *view, TNgGainQuality *quality, TNgError *error);
/** Briefly sample the selected SDR while the receiver remains stopped. */
int NgGainCalibrate(const TNgConfig *config, TNgGainCalibration *result, TNgError *error);
#endif
