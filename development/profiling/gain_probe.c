/* Optional stopped-device audit. Never linked into the operational receiver. */
#include "../../src/frontend/calibration.c"

int main(void)
{
    static const unsigned settings[][2] = {
        {0,62},{8,54},{24,38},{32,30},{40,22},
        {40,26},{40,30},{40,34},{40,38},{40,42},{40,50},{40,62}
    };
    TNgConfig config;
    TNgFrontendCaps caps;
    TNgError error = {{0}};
    size_t k;
    NgConfigDefaults(&config);
    config.frontend = NG_FRONTEND_HACKRF;
    config.sampleRateHz = 16000000;
    config.centerFrequencyHz = UINT64_C(1575420000);
    config.signalFrequencyHz = config.centerFrequencyHz;
    config.rfAmp = 1;
    if (NgFrontendCapabilities(&config, &caps, &error) != NG_OK) {
        fprintf(stderr, "%s\n", error.message);return 1;
    }
    if (caps.gainPolicy < NG_GAIN_POLICY_HACKRF_MAX2837 || caps.gainPolicy > NG_GAIN_POLICY_HACKRF_MAX2831) {
        fprintf(stderr, "Unsupported HackRF gain policy\n");return 1;
    }
    snprintf(config.serial, sizeof(config.serial), "%s", caps.deviceSerial);
    printf("model=%s\n", caps.deviceModel);
    puts("lna,vga,early,late,total,accepted,rms,gaussian,imbalance,dc,score,reasons");
    for (k=0;k<sizeof(settings)/sizeof(settings[0]);++k) {
        TNgGainCalibration result = {0};
        int early, late;
        config.lnaGainDb=settings[k][0];config.vgaGainDb=settings[k][1];
        result.capabilities=caps;
        if (Try(&config,&result,NULL,NULL,NULL,&error) != NG_OK) {
            fprintf(stderr,"%s\n",error.message);return 1;
        }
        HackrfPhysicalGains(caps.gainPolicy,config.lnaGainDb,config.vgaGainDb,&early,&late);
        printf("%u,%u,%d,%d,%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%u\n",
               config.lnaGainDb,config.vgaGainDb,early,late,early+late,result.accepted,
               result.quality.rmsFraction,result.quality.gaussianError,result.quality.iqImbalance,
               result.quality.dcFraction,result.quality.score,result.quality.rejectionReasons);
        fflush(stdout);
    }
    return 0;
}
