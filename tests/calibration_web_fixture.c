/* Test executable only: deterministic measurements behind the real HTTP worker.
 * Does not open hardware. Never linked into the delivered ngene application. */
#include "ngene/calibration.h"
#include "platform/platform.h"
#include <string.h>

int NgGainCalibrateControlled(const TNgConfig *config, TNgGainCalibration *result,
                              const TNgGainCalibrationCallbacks *callbacks, TNgError *error)
{
    unsigned mode = config->frontend == NG_FRONTEND_HACKRF ? config->vgaGainDb
                    : config->rtlIfGainDb == 3             ? 6u
                                                           : 0u;
    unsigned k, count = mode == 6 ? 1000u : 60u;
    memset(result, 0, sizeof(*result));
    result->selected = *config;
    result->capabilities.gainPolicy = NG_GAIN_POLICY_HACKRF_MAX2837;
    strcpy(result->capabilities.deviceModel, "HackRF fixture / MAX2837");
    if (config->frontend == NG_FRONTEND_RTLSDR) {
        result->capabilities.gainPolicy = NG_GAIN_POLICY_RTL_E4000;
        strcpy(result->capabilities.deviceModel, "RTL-SDR fixture / E4000");
        result->capabilities.tunerGainCount = 2;
        result->capabilities.tunerGains[0] = 340;
        result->capabilities.tunerGains[1] = 420;
        result->selected.rtlAutoGain = 0;
        result->selected.rtlGainTenthsDb = 420;
        result->selected.rtlIfGainDb = 50;
    }
    if (mode == 4) {
        return NgFail(error, "Simulated SDR transport failure");
    }
    for (k = 0; k < count; ++k) {
        if (callbacks->cancelled(callbacks->context)) {
            return NG_END;
        }
        result->tested = 3 * k;
        callbacks->progress(callbacks->context, config, result);
        NgSleepMs(10);
    }
    result->accepted = mode != 2;
    result->quality.acceptable = result->accepted;
    result->quality.rmsFraction = result->accepted ? 0.12 : 0.02;
    result->quality.dcFraction = 0.02;
    result->rejectedWeak = result->accepted ? 0 : count;
    result->quality.rejectionReasons = result->accepted ? 0u : NG_GAIN_WEAK;
    result->selected.lnaGainDb = 24;
    result->selected.vgaGainDb = 28;
    return NG_OK;
}
