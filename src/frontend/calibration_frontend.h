/** @file calibration_frontend.h @brief Private continuous RTL calibration transport. */
#ifndef NGENE_CALIBRATION_FRONTEND_H
#define NGENE_CALIBRATION_FRONTEND_H
#include "ngene/frontend.h"
enum { NG_CAL_RTL_BUFFERS = 2, NG_CAL_RTL_BUFFER_PAIRS = 32768 };
/** Gate publication, drain old samples and apply gains on the same RTL stream.
 * Only calibration uses this; rate, tuning and identity remain fixed. */
int NgFrontendCalibrationNext(const TNgConfig *config, TNgFrontend *frontend, TNgError *error);
/** Suspend publication while the calibration worker analyzes copied windows. */
int NgFrontendCalibrationPause(TNgFrontend *frontend, TNgError *error);
#endif
