/** @file test_rtlsdr.c @brief Deterministic native API fixture; no RF hardware validation. */
#include "frontend/rtlsdr_adapter.h"
#include "frontend/calibration_frontend.h"
#include "platform/platform.h"
#include <rtl-sdr.h>
#include <stdlib.h>
#include <string.h>
/** @brief Stop immediately on a failed native-adapter contract. */
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x);                                        \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

/** @brief Fixture-owned device configuration and native cancellation state. */
struct rtlsdr_dev {
    uint32_t rate, frequency;
    TNgAtomicInt canceled;
};

/** @brief Inject an odd byte count; never exposed outside this fixture. */
static int malformed;
static int ifCalls, ifGains[6], failIfStage, deviceOpens, deviceCloses;
static int asyncCalls;
static enum rtlsdr_tuner fixtureTuner = RTLSDR_TUNER_R820T;

/** @brief Return the native tuner ID used to select actual supported gain operations. */
enum rtlsdr_tuner rtlsdr_get_tuner_type(rtlsdr_dev_t *device)
{
    (void)device;
    return fixtureTuner;
}

/** @brief Return the one fixture device. @return One connected fixture. */
uint32_t rtlsdr_get_device_count(void)
{
    return 1;
}

/** @brief Copy fixture USB identity.
 * @param index Device index.
 * @param manufacturer Writable native manufacturer buffer.
 * @param product Writable product buffer.
 * @param serial Writable serial buffer.
 * @return Zero for index zero, otherwise failure.
 */
int rtlsdr_get_device_usb_strings(uint32_t index, char *manufacturer, char *product, char *serial)
{
    if (index != 0) {
        return -1;
    }
    strcpy(manufacturer, "Fixture");
    strcpy(product, "RTL-SDR");
    strcpy(serial, "rtl-test");
    return 0;
}

/** @brief Open fixture state. @param device Receives owned fixture. @param index Must be zero.
 * @return Zero on allocation. */
int rtlsdr_open(rtlsdr_dev_t **device, uint32_t index)
{
    if (index != 0) {
        return -1;
    }
    *device = calloc(1, sizeof(**device));
    ++deviceOpens;
    return *device == NULL ? -1 : 0;
}

/** @brief Free stopped fixture. @param device Owned handle. @return Zero. */
int rtlsdr_close(rtlsdr_dev_t *device)
{
    free(device);
    ++deviceCloses;
    return 0;
}

/** @brief Set fixture sample rate. @param device Fixture. @param rate Desired rate. @return Zero.
 */
int rtlsdr_set_sample_rate(rtlsdr_dev_t *device, uint32_t rate)
{
    device->rate = rate;
    return 0;
}

/** @brief Read fixture rate. @param device Fixture. @return Applied rate. */
uint32_t rtlsdr_get_sample_rate(rtlsdr_dev_t *device)
{
    return device->rate;
}

/** @brief Set fixture RF tuning. @param device Fixture. @param frequency Desired RF. @return Zero.
 */
int rtlsdr_set_center_freq(rtlsdr_dev_t *device, uint32_t frequency)
{
    device->frequency = frequency;
    return 0;
}

/** @brief Read fixture RF tuning. @param device Fixture. @return Applied RF. */
uint32_t rtlsdr_get_center_freq(rtlsdr_dev_t *device)
{
    return device->frequency;
}

/** @brief Copy three discrete tuner gains. @param device Unused fixture. @param gains Optional
 * three-item output. @return Three. */
int rtlsdr_get_tuner_gains(rtlsdr_dev_t *device, int *gains)
{
    (void)device;
    if (gains) {
        gains[0] = 0;
        gains[1] = 77;
        gains[2] = 144;
    }
    return 3;
}

/** @brief Accept a validated gain mode. @param device Fixture. @param mode Native manual flag.
 * @return Zero. */
int rtlsdr_set_tuner_gain_mode(rtlsdr_dev_t *device, int mode)
{
    (void)device;
    (void)mode;
    return 0;
}

/** @brief Accept selected fixture gain. @param device Fixture. @param gain Discrete gain. @return
 * Zero. */
int rtlsdr_set_tuner_gain(rtlsdr_dev_t *device, int gain)
{
    (void)device;
    (void)gain;
    return 0;
}

/** @brief Check actual E4000 stage calls, including native tenth-dB units. */
int rtlsdr_set_tuner_if_gain(rtlsdr_dev_t *device, int stage, int gain)
{
    (void)device;
    CHECK(fixtureTuner == RTLSDR_TUNER_E4000 && stage >= 1 && stage <= 6 && gain % 10 == 0);
    ifGains[stage - 1] = gain / 10;
    ++ifCalls;
    return stage == failIfStage ? -1 : 0;
}

/** @brief Accept PPM correction. @param device Fixture. @param ppm Correction. @return Zero. */
int rtlsdr_set_freq_correction(rtlsdr_dev_t *device, int ppm)
{
    (void)device;
    (void)ppm;
    return 0;
}

/** @brief Accept bandwidth. @param device Fixture. @param bandwidth Hz. @return Zero. */
int rtlsdr_set_tuner_bandwidth(rtlsdr_dev_t *device, uint32_t bandwidth)
{
    (void)device;
    (void)bandwidth;
    return 0;
}

/** @brief Accept explicitly configured bias tee. @param device Fixture. @param on Enable. @return
 * Zero. */
int rtlsdr_set_bias_tee(rtlsdr_dev_t *device, int on)
{
    (void)device;
    (void)on;
    return 0;
}

/** @brief Reset native cancellation. @param device Fixture. @return Zero. */
int rtlsdr_reset_buffer(rtlsdr_dev_t *device)
{
    NgAtomicSet(&device->canceled, 0);
    return 0;
}

/** @brief Publish cancellation to the test native dispatcher. @param device Fixture. @return Zero.
 */
int rtlsdr_cancel_async(rtlsdr_dev_t *device)
{
    NgAtomicSet(&device->canceled, 1);
    return 0;
}

/** @brief Deliver vendor-owned bytes, then overwrite them to test copy lifetime.
 * @param device Fixture device.
 * @param callback Production NGene adapter callback.
 * @param context Production callback context.
 * @param buffers Unused vendor-buffer count.
 * @param length Unused vendor-buffer length.
 * @return Zero after cancellation.
 */
int rtlsdr_read_async(rtlsdr_dev_t *device, rtlsdr_read_async_cb_t callback, void *context,
                      uint32_t buffers, uint32_t length)
{
    unsigned char bytes[22];
    unsigned index;
    ++asyncCalls;
    (void)length;
    NgSleepMs(5);
    NgAtomicSet(&device->canceled, 0); /* Deliberately exercise early cancel retry. */
    for (index = 0; index < 11; ++index) {
        bytes[index * 2] = (unsigned char)index;
        bytes[index * 2 + 1] = 255;
    }
    callback(bytes, malformed ? 21u : 22u, context);
    if (buffers == NG_CAL_RTL_BUFFERS) {
        CHECK(length == 2u * NG_CAL_RTL_BUFFER_PAIRS);
        while (!NgAtomicGet(&device->canceled)) {
            NgSleepMs(2);
            callback(bytes, 22u, context);
        }
    }
    memset(bytes, 0, sizeof(bytes));
    while (!NgAtomicGet(&device->canceled)) {
        NgSleepMs(1);
    }
    return 0;
}

/** @brief Run callback ownership, gain, overflow and start/stop race contracts. @return Zero after
 * all assertions. */
int main(void)
{
    TNgConfig config;
    TNgFrontend source = {0};
    TNgError error = {{0}};
    TNgFrontendStats stats;
    TNgIqView view;
    TNgFrontendCaps caps;
    unsigned scenario, index;
    NgConfigDefaults(&config);
    config.frontend = NG_FRONTEND_RTLSDR;
    config.sampleRateHz = 2400000;
    config.windowSamples = 4;
    config.ingressBlocks = 8;
    strcpy(config.serial, "rtl-test");
    CHECK(NgRtlsdrCapabilities(&config, &caps, &error) == NG_OK && caps.tunerGainCount == 3 &&
          caps.tunerGains[1] == 77);
    CHECK(caps.gainPolicy == NG_GAIN_POLICY_RTL_R82XX && !strcmp(caps.deviceSerial, "rtl-test"));
    fixtureTuner = RTLSDR_TUNER_E4000;
    CHECK(NgRtlsdrCapabilities(&config, &caps, &error) == NG_OK &&
          caps.gainPolicy == NG_GAIN_POLICY_RTL_E4000);
    fixtureTuner = RTLSDR_TUNER_FC0012;
    CHECK(NgRtlsdrCapabilities(&config, &caps, &error) == NG_OK &&
          caps.gainPolicy == NG_GAIN_POLICY_RTL_FC001X);
    fixtureTuner = RTLSDR_TUNER_FC2580;
    CHECK(NgRtlsdrCapabilities(&config, &caps, &error) == NG_OK &&
          caps.gainPolicy == NG_GAIN_POLICY_UNAVAILABLE);
    fixtureTuner = RTLSDR_TUNER_UNKNOWN;
    CHECK(NgRtlsdrCapabilities(&config, &caps, &error) == NG_OK &&
          caps.gainPolicy == NG_GAIN_POLICY_UNAVAILABLE);
    fixtureTuner = RTLSDR_TUNER_R820T;
    config.rtlAutoGain = 0;
    config.rtlGainTenthsDb = 1;
    CHECK(NgRtlsdrOpen(&config, &source, &error) == NG_ERROR);
    config.rtlGainTenthsDb = 77;
    CHECK(NgRtlsdrOpen(&config, &source, &error) == NG_OK && !ifCalls);
    NgFrontendDestroy(&source);
    fixtureTuner = RTLSDR_TUNER_E4000;
    CHECK(NgRtlsdrOpen(&config, &source, &error) == NG_OK && !ifCalls);
    NgFrontendDestroy(&source);
    for (config.rtlIfGainDb = 3; config.rtlIfGainDb <= 56; ++config.rtlIfGainDb) {
        int sum = 0;
        ifCalls = 0;
        CHECK(NgRtlsdrOpen(&config, &source, &error) == NG_OK && ifCalls == 6);
        for (index = 0; index < 6; ++index)
            sum += ifGains[index];
        CHECK(sum == config.rtlIfGainDb);
        CHECK(ifGains[0] == (sum >= 12 ? 6 : -3));
        CHECK(ifGains[1] >= 0 && ifGains[1] <= 9 && ifGains[1] % 3 == 0);
        CHECK(ifGains[2] >= 0 && ifGains[2] <= 9 && ifGains[2] % 3 == 0);
        CHECK(ifGains[3] >= 0 && ifGains[3] <= 2);
        CHECK(ifGains[4] >= 3 && ifGains[4] <= 15 && ifGains[4] % 3 == 0);
        CHECK(ifGains[5] >= 3 && ifGains[5] <= 15 && ifGains[5] % 3 == 0);
        NgFrontendDestroy(&source);
    }
    config.rtlIfGainDb = 48;
    failIfStage = 3;
    CHECK(NgRtlsdrOpen(&config, &source, &error) == NG_ERROR && !source.context);
    CHECK(strstr(error.message, "stage 3") != NULL);
    failIfStage = 0;
    config.rtlAutoGain = 1;
    ifCalls = 0;
    CHECK(NgRtlsdrOpen(&config, &source, &error) == NG_OK && !ifCalls);
    NgFrontendDestroy(&source);
    config.rtlAutoGain = 0;
    fixtureTuner = RTLSDR_TUNER_R820T;
    CHECK(NgRtlsdrOpen(&config, &source, &error) == NG_OK && !ifCalls);
    NgFrontendDestroy(&source);
    /* Repeated calibration observations reuse USB, replace the closed ingress
     * and never expose queued samples from the previous gain. */
    {
        int before = deviceOpens, closed = deviceCloses, workers = asyncCalls;
        malformed = 0;
        CHECK(NgRtlsdrOpen(&config, &source, &error) == NG_OK);
        for (index = 0; index < 3; ++index) {
            CHECK(NgRtlsdrCalibrationNext(&config, &source, &error) == NG_OK);
            CHECK(source.ops->start(source.context, &error) == NG_OK);
            CHECK(source.ops->acquire(source.context, &view, 1000, &error) == NG_OK);
            CHECK(view.meta.firstSample >= index * 11u && NgIqComponent(&view, 0, 0) == -128);
            source.ops->release(source.context);
            CHECK(NgRtlsdrCalibrationPause(&source, &error) == NG_OK);
        }
        CHECK(deviceOpens == before + 1 && deviceCloses == closed);
        NgFrontendDestroy(&source);
        CHECK(deviceCloses == closed + 1 && asyncCalls == workers + 1);
    }
    for (scenario = 0; scenario < 4; ++scenario) {
        malformed = scenario == 2;
        config.ingressBlocks = scenario == 1 ? 2 : 8;
        CHECK(NgRtlsdrOpen(&config, &source, &error) == NG_OK &&
              source.info.format == NG_FORMAT_CU8);
        CHECK(source.ops->start(source.context, &error) == NG_OK);
        if (scenario == 3) {
            NgSleepMs(1);
        } else if (scenario == 0) {
            CHECK(source.ops->acquire(source.context, &view, 1000, &error) == NG_OK);
            CHECK(NgIqComponent(&view, 0, 0) == -128 && NgIqComponent(&view, 0, 1) == 127);
            NgSleepMs(10);
            CHECK(NgIqComponent(&view, 3, 0) == -125);
            source.ops->release(source.context);
        } else if (scenario == 2) {
            /* Callback error arrives while the reader is waiting on an empty ring. */
            CHECK(source.ops->acquire(source.context, &view, 1000, &error) == NG_ERROR);
        } else {
            for (index = 0; index < 100; ++index) {
                source.ops->stats(source.context, &stats);
                if (stats.droppedSamples || stats.transportErrors) {
                    break;
                }
                NgSleepMs(1);
            }
            CHECK(source.ops->acquire(source.context, &view, 0, &error) == NG_ERROR);
        }
        CHECK(source.ops->stop(source.context, &error) == NG_OK);
        source.ops->stats(source.context, &stats);
        if (scenario == 1) {
            CHECK(stats.droppedSamples == 3 && stats.transportErrors == 0 &&
                  stats.ingress.highWaterBlocks == 2);
        }
        if (scenario == 2) {
            CHECK(stats.droppedSamples == 0 && stats.transportErrors == 1);
        }
        CHECK(stats.hardwareLossState == NG_DIAGNOSTIC_UNSUPPORTED);
        NgFrontendDestroy(&source);
    }
    puts(
        "PASS native RTL-SDR adapter fixture, E4000 IF stages/units/failure, CU8 lifetime, gain steps, overflow-stop and "
        "cancellation race");
    return 0;
}
