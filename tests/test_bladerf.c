/** @file test_bladerf.c @brief Native SC16 Q11/SC8 Q7 and timestamp integrity fixture; no RF
 * hardware. */
#include "frontend/bladerf_adapter.h"
#include "platform/platform.h"
#include <libbladeRF.h>
#include <stdlib.h>
#include <string.h>
/** @brief Fail the CTest fixture on an invalid contract. */
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x);                                        \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

/** @brief Fixture native device and monotonic block count. */
struct bladerf {
    unsigned calls;
    uint64_t frequency;
    bladerf_format format;
    int enabled;
};

/** @brief Select normal, overload, malformed metadata and partial-timeout cases. */
static unsigned scenario;
static const char *fixtureBoard = "bladerf2";

/** @brief Identify the RFIC family; the adapter must retain native stage allocation. */
const char *CALL_CONV bladerf_get_board_name(struct bladerf *device)
{
    (void)device;
    return fixtureBoard;
}

/** @brief Verify the selected format reaches the native API without fallback. */
static bladerf_format expected_format;

/** @brief Native request size, independent of the USB-aligned vendor buffer size. */
static unsigned expected_transfer_samples = 16384u;

/** @brief Native requests should fill complete ingress windows when they fit. */
static size_t expected_window_samples = 4;

/** @brief Inject a native configuration rejection. */
static int configuration_result;

/** @brief Inject a native RX-enable rejection. */
static int enable_result;

/** @brief Count ownership transitions and native configuration calls. */
static unsigned opened, closed, configuration_calls;

/** @brief Configurable device-list outcome for enumeration coverage. */
static int device_list_result = 1;

/** @brief Enumerate one fixture. @param list Receives owned list. @return One. */
int CALL_CONV bladerf_get_device_list(struct bladerf_devinfo **list)
{
    if (device_list_result != 1) {
        *list = NULL;
        return device_list_result;
    }
    *list = calloc(1, sizeof(**list));
    CHECK(*list);
    strcpy((*list)->serial, "blade-test");
    return 1;
}

/** @brief Free fixture enumeration. @param list Owned list. */
void CALL_CONV bladerf_free_device_list(struct bladerf_devinfo *list)
{
    free(list);
}

/** @brief Open one fixture. @param device Receives owned state. @param info Copied native identity.
 * @return Zero. */
int CALL_CONV bladerf_open_with_devinfo(struct bladerf **device, struct bladerf_devinfo *info)
{
    (void)info;
    *device = calloc(1, sizeof(**device));
    if (*device != NULL) {
        ++opened;
    }
    return *device ? 0 : -1;
}

/** @brief Close stopped fixture. @param device Owned state. */
void CALL_CONV bladerf_close(struct bladerf *device)
{
    CHECK(device->enabled == 0);
    ++closed;
    free(device);
}

/** @brief Report configured fixture FPGA. @param device Fixture. @return One. */
int CALL_CONV bladerf_is_fpga_configured(struct bladerf *device)
{
    (void)device;
    return 1;
}

/** @brief Return fixed fixture error text. @param code Native error. @return Static error label. */
const char *CALL_CONV bladerf_strerror(int code)
{
    (void)code;
    return "fixture error";
}

/** @brief Set fixture tuning. @param device Fixture. @param channel RX0. @param frequency Desired
 * frequency. @return Zero. */
int CALL_CONV bladerf_set_frequency(struct bladerf *device, bladerf_channel channel,
                                    bladerf_frequency frequency)
{
    (void)channel;
    device->frequency = frequency;
    return 0;
}

/** @brief Read fixture tuning. @param device Fixture. @param channel RX0. @param frequency Output.
 * @return Zero. */
int CALL_CONV bladerf_get_frequency(struct bladerf *device, bladerf_channel channel,
                                    bladerf_frequency *frequency)
{
    (void)channel;
    *frequency = device->frequency;
    return 0;
}

/** @brief Read native rate bounds. @param device Fixture. @param channel RX0. @param range Borrowed
 * fixture bounds. @return Zero. */
int CALL_CONV bladerf_get_sample_rate_range(struct bladerf *device, bladerf_channel channel,
                                            const struct bladerf_range **range)
{
    static const struct bladerf_range limits = {80000, 61440000, 1, 1};
    (void)device;
    (void)channel;
    *range = &limits;
    return 0;
}

/** @brief Read native frequency bounds. @param device Fixture. @param channel RX0. @param range
 * Output. @return Zero. */
int CALL_CONV bladerf_get_frequency_range(struct bladerf *device, bladerf_channel channel,
                                          const struct bladerf_range **range)
{
    static const struct bladerf_range limits = {47000000, 6000000000, 1, 1};
    (void)device;
    (void)channel;
    *range = &limits;
    return 0;
}

/** @brief Read frequency-dependent gain bounds. @param device Fixture. @param channel RX0. @param
 * range Output. @return Zero. */
int CALL_CONV bladerf_get_gain_range(struct bladerf *device, bladerf_channel channel,
                                     const struct bladerf_range **range)
{
    static const struct bladerf_range limits = {-12, 60, 1, 1};
    (void)device;
    (void)channel;
    *range = &limits;
    return 0;
}

/** @brief Accept requested rate. @param device Fixture. @param channel RX0. @param rate Requested.
 * @param actual Output. @return Zero. */
int CALL_CONV bladerf_set_sample_rate(struct bladerf *device, bladerf_channel channel,
                                      bladerf_sample_rate rate, bladerf_sample_rate *actual)
{
    (void)device;
    (void)channel;
    *actual = rate;
    return 0;
}

/** @brief Accept bandwidth. @param device Fixture. @param channel RX0. @param bandwidth Requested.
 * @param actual Output. @return Zero. */
int CALL_CONV bladerf_set_bandwidth(struct bladerf *device, bladerf_channel channel,
                                    bladerf_bandwidth bandwidth, bladerf_bandwidth *actual)
{
    (void)device;
    (void)channel;
    *actual = bandwidth;
    return 0;
}

/** @brief Accept manual gain mode. @param device Fixture. @param channel RX0. @param mode Native
 * mode. @return Zero. */
int CALL_CONV bladerf_set_gain_mode(struct bladerf *device, bladerf_channel channel,
                                    bladerf_gain_mode mode)
{
    (void)device;
    (void)channel;
    (void)mode;
    return 0;
}

/** @brief Accept checked gain. @param device Fixture. @param channel RX0. @param gain Native dB.
 * @return Zero. */
int CALL_CONV bladerf_set_gain(struct bladerf *device, bladerf_channel channel, bladerf_gain gain)
{
    (void)device;
    (void)channel;
    (void)gain;
    return 0;
}

/** @brief Accept explicit bias tee. @param device Fixture. @param channel RX0. @param on Enable.
 * @return Zero. */
int CALL_CONV bladerf_set_bias_tee(struct bladerf *device, bladerf_channel channel, bool on)
{
    (void)device;
    (void)channel;
    (void)on;
    return 0;
}

/** @brief Validate RX metadata stream configuration.
 * @param device Fixture.
 * @param layout Must be RX X1.
 * @param format Must select the requested native metadata representation.
 * @param buffers Vendor buffer count.
 * @param size Vendor pair count.
 * @param transfers Concurrent transfers.
 * @param timeout Native bounded timeout.
 * @return Zero after validation.
 */
int CALL_CONV bladerf_sync_config(struct bladerf *device, bladerf_channel_layout layout,
                                  bladerf_format format, unsigned buffers, unsigned size,
                                  unsigned transfers, unsigned timeout)
{
    ++configuration_calls;
    CHECK(layout == BLADERF_RX_X1 && format == expected_format && buffers > transfers &&
          size == 16384u && timeout == 100);
    device->format = format;
    return configuration_result;
}

/** @brief Accept RX enable/disable. @param device Fixture. @param channel RX0. @param enable
 * Desired state. @return Zero. */
int CALL_CONV bladerf_enable_module(struct bladerf *device, bladerf_channel channel, bool enable)
{
    (void)channel;
    if (enable && enable_result != 0) {
        return enable_result;
    }
    CHECK((enable != 0) != (device->enabled != 0));
    device->enabled = enable;
    return 0;
}

/** @brief Produce two valid blocks with a five-pair hardware-clock gap, then return timeouts.
 * @param device Fixture.
 * @param samples Writable production-owned buffer in the selected native format.
 * @param count Requested pair capacity.
 * @param metadata Receives hardware tick, contiguous length and overrun flag.
 * @param timeout Bounded wait.
 * @return Zero for blocks; native timeout when idle.
 */
int CALL_CONV bladerf_sync_rx(struct bladerf *device, void *samples, unsigned count,
                              struct bladerf_metadata *metadata, unsigned timeout)
{
    unsigned index;
    (void)timeout;
    CHECK(metadata->flags == UINT32_C(0x80000000));
    CHECK(count == expected_transfer_samples && count >= 32u && count <= 16384u);
    CHECK(expected_window_samples > 16384u || count % expected_window_samples == 0);
    if (device->calls >= 2) {
        NgSleepMs(1);
        return BLADERF_ERR_TIMEOUT;
    }
    CHECK(device->enabled != 0);
    if (scenario >= 2 && scenario <= 5) {
        NgSleepMs(5);
    }
    if (scenario == 9) {
        ++device->calls;
        return BLADERF_ERR_IO;
    }
    metadata->timestamp = device->calls == 0 ? 1000u : scenario == 8 ? 1032u : 1037u;
    if (scenario == 4 && device->calls != 0) {
        metadata->timestamp = 1030u;
    }
    if (scenario == 10 && device->calls != 0) {
        metadata->timestamp = 1031u;
    }
    if (scenario == 5) {
        metadata->timestamp = UINT64_MAX - 3u;
    }
    metadata->status =
        device->calls == 0 && scenario != 7 && scenario != 10 ? (uint32_t)BLADERF_META_STATUS_OVERRUN : 0u;
    metadata->actual_count = scenario == 2        ? count + 1u
                             : scenario == 3      ? 0u
                             : device->calls == 0 ? 32u
                                                  : 8u;
    for (index = 0; index < 32; ++index) {
        /* Write the vendor memory layout independently of NGene's IQ helpers. */
        if (device->format == BLADERF_FORMAT_SC8_Q7_META) {
            static const int8_t levels[4] = {-128, -127, 0, 127};
            int8_t *bytes = samples;
            bytes[index * 2u] = levels[index % 4u];
            bytes[index * 2u + 1u] = levels[3u - index % 4u];
        } else {
            static const int16_t levels[4] = {-2048, -1901, 1235, 2047};
            int16_t *words = samples;
            words[index * 2u] = levels[index % 4u];
            words[index * 2u + 1u] = levels[3u - index % 4u];
        }
    }
    ++device->calls;
    return scenario == 6 && device->calls == 1 ? BLADERF_ERR_TIMEOUT : 0;
}

/** @brief Exercise the shared ring and native continuity in one selected representation.
 * @param config Native format and fixture device settings.
 * @param testCase Stream failure or continuity case to exercise.
 */
static void CheckStream(TNgConfig *config, unsigned testCase)
{
    TNgFrontend source = {0};
    TNgFrontendStats stats;
    TNgError error = {{0}};
    TNgIqView view;
    unsigned index;
    int valid;
    scenario = testCase;
    expected_transfer_samples = 16384u;
    expected_window_samples = config->windowSamples;
    config->ingressBlocks = scenario == 1 ? 2 : 64;
    CHECK(NgBladerfOpen(config, &source, &error) == NG_OK &&
          source.info.format == config->bladeFormat);
    CHECK(source.ops->start(source.context, &error) == NG_OK);
    CHECK(source.ops->start(source.context, &error) == NG_ERROR);
    valid = scenario == 0 || (scenario >= 6 && scenario <= 8) || scenario == 10;
    if (valid) {
        for (index = 0; index < 10; ++index) {
            size_t sample;
            size_t sampleOffset = scenario == 10 && index >= 8 ? 1u : 0u;
            static const int ci8[4] = {-128, -127, 0, 127};
            static const int sc16[4] = {-2048, -1901, 1235, 2047};
            const int *levels = config->bladeFormat == NG_FORMAT_CI8 ? ci8 : sc16;
            CHECK(source.ops->acquire(source.context, &view, 1000, &error) == NG_OK);
            CHECK(view.format == config->bladeFormat &&
                  view.meta.count == (scenario == 10 && index == 9 ? 3u : 4u));
            for (sample = 0; sample < view.meta.count; ++sample) {
                CHECK(NgIqComponent(&view, sample, 0) == levels[(sample + sampleOffset) % 4u]);
                CHECK(NgIqComponent(&view, sample, 1) ==
                      levels[3u - (sample + sampleOffset) % 4u]);
            }
            CHECK((view.meta.flags & NG_BLOCK_HARDWARE_TIME) != 0);
            CHECK(view.meta.hardwareTimestamp == 1000u + view.meta.firstSample);
            CHECK(view.meta.firstSample == (index < 8       ? index * 4u
                                            : (scenario == 8 || scenario == 10) ? index * 4u
                                                            : index * 4u + 5u));
            if (index == 8) {
                CHECK((view.meta.flags & NG_BLOCK_GAP) != 0);
                CHECK(view.meta.missingKnown == (scenario != 8 && scenario != 10));
                CHECK(view.meta.missingBefore == ((scenario == 8 || scenario == 10) ? 0u : 5u));
            } else {
                CHECK((view.meta.flags & NG_BLOCK_GAP) == 0);
            }
            source.ops->release(source.context);
        }
    } else {
        for (index = 0; index < 1000; ++index) {
            source.ops->stats(source.context, &stats);
            if (stats.droppedSamples || stats.transportErrors) {
                break;
            }
            NgSleepMs(1);
        }
        CHECK(source.ops->acquire(source.context, &view, 0, &error) == NG_ERROR);
        if (scenario == 2) {
            CHECK(strstr(error.message, "actual_count=") != NULL &&
                  strstr(error.message, "exceeds requested") != NULL);
        } else if (scenario == 3) {
            CHECK(strstr(error.message, "zero actual_count") != NULL);
        } else if (scenario == 4) {
            CHECK(strstr(error.message, "timestamp moved backwards") != NULL);
        } else if (scenario == 5) {
            CHECK(strstr(error.message, "timestamp overflow") != NULL);
        } else if (scenario == 9) {
            CHECK(strstr(error.message, "bladerf_sync_rx failed") != NULL &&
                  strstr(error.message, "fixture error") != NULL);
        }
    }
    CHECK(source.ops->stop(source.context, &error) == NG_OK);
    CHECK(source.ops->stop(source.context, &error) == NG_OK);
    source.ops->stats(source.context, &stats);
    if (valid) {
        CHECK(stats.receivedSamples == (scenario == 10 ? 39u : 40u) && stats.deviceGapEvents == 1 &&
              stats.deviceMissingSamples == ((scenario == 8 || scenario == 10) ? 0u : 5u) &&
              stats.droppedSamples == 0 && stats.transportErrors == 0);
        CHECK(stats.ingress.capacityBlocks == config->ingressBlocks &&
              stats.ingress.blockSamples == config->windowSamples);
        CHECK(source.ops->acquire(source.context, &view, 0, &error) == NG_END);
    } else if (scenario == 1) {
        CHECK(stats.droppedSamples == 24 && stats.transportErrors == 0 &&
              stats.ingress.highWaterBlocks == 2);
    } else {
        CHECK(stats.receivedSamples == (scenario == 4 ? 32u : 0u) && stats.transportErrors == 1 &&
              stats.droppedSamples == 0);
    }
    CHECK(stats.hardwareLossState == NG_DIAGNOSTIC_UNSUPPORTED);
    NgFrontendDestroy(&source);
    CHECK(opened == closed);
}

/** @brief Check request packing without changing the vendor USB buffer geometry.
 * @param config Fixture settings; restores the existing ingress-window geometry.
 * @param windowSamples Requested ingress slot capacity.
 * @param transferSamples Expected native sync request in complex pairs.
 */
static void CheckRequestGeometry(TNgConfig *config, size_t windowSamples, unsigned transferSamples)
{
    TNgFrontend source = {0};
    TNgIqView view;
    TNgError error = {{0}};
    size_t savedWindowSamples = config->windowSamples;
    scenario = 0;
    config->windowSamples = windowSamples;
    expected_transfer_samples = transferSamples;
    expected_window_samples = windowSamples;
    CHECK(NgBladerfOpen(config, &source, &error) == NG_OK);
    CHECK(source.ops->start(source.context, &error) == NG_OK);
    CHECK(source.ops->acquire(source.context, &view, 1000, &error) == NG_OK);
    CHECK(view.meta.count == 32 && view.meta.firstSample == 0);
    source.ops->release(source.context);
    CHECK(source.ops->acquire(source.context, &view, 1000, &error) == NG_OK);
    CHECK(view.meta.count == 8 && view.meta.firstSample == 37 && view.meta.missingBefore == 5 &&
          view.meta.missingKnown != 0);
    source.ops->release(source.context);
    CHECK(source.ops->stop(source.context, &error) == NG_OK);
    NgFrontendDestroy(&source);
    CHECK(opened == closed);
    config->windowSamples = savedWindowSamples;
    expected_transfer_samples = 16384u;
    expected_window_samples = savedWindowSamples;
}

/** @brief Run native precision, timestamp, gain and separated-loss contracts. @return Zero after
 * assertions. */
int main(void)
{
    TNgConfig config;
    TNgFrontend source = {0};
    TNgFrontendCaps caps;
    TNgError error = {{0}};
    unsigned formatIndex, testCase, attempts;
    FILE *output;
    char line[64];
    NgConfigDefaults(&config);
    CHECK(config.bladeFormat == NG_FORMAT_SC16_Q11);
    config.frontend = NG_FRONTEND_BLADERF;
    config.windowSamples = 4;
    config.ingressBlocks = 64;
    strcpy(config.serial, "blade-test");
    output = tmpfile();
    CHECK(output != NULL);
    device_list_result = BLADERF_ERR_NODEV;
    CHECK(NgBladerfList(output, &error) == NG_OK);
    CHECK(fflush(output) == 0 && fseek(output, 0, SEEK_SET) == 0);
    CHECK(fgets(line, sizeof(line), output) != NULL && strcmp(line, "bladeRF devices=0\n") == 0);
    CHECK(fclose(output) == 0);
    device_list_result = BLADERF_ERR_IO;
    CHECK(NgBladerfList(stdout, &error) == NG_ERROR);
    CHECK(strstr(error.message, "bladeRF enumerate: fixture error") != NULL);
    device_list_result = 1;
    CHECK(NgBladerfCapabilities(&config, &caps, &error) == NG_OK && caps.gainMinDb == -12 &&
          caps.gainMaxDb == 60);
    CHECK(caps.gainPolicy == NG_GAIN_POLICY_BLADERF2 && !strcmp(caps.deviceSerial, "blade-test"));
    fixtureBoard = "bladerf1";
    CHECK(NgBladerfCapabilities(&config, &caps, &error) == NG_OK &&
          caps.gainPolicy == NG_GAIN_POLICY_BLADERF1);
    fixtureBoard = "future-board";
    CHECK(NgBladerfCapabilities(&config, &caps, &error) == NG_OK &&
          caps.gainPolicy == NG_GAIN_POLICY_UNAVAILABLE);
    fixtureBoard = "bladerf2";
    config.bladeGainDb = 61;
    CHECK(NgBladerfOpen(&config, &source, &error) == NG_ERROR);
    CHECK(opened == closed);
    config.bladeGainDb = 30;
    config.bladeFormat = NG_FORMAT_CU8;
    attempts = opened;
    CHECK(NgBladerfOpen(&config, &source, &error) == NG_ERROR && opened == attempts);
    for (formatIndex = 0; formatIndex < 2; ++formatIndex) {
        config.bladeFormat = formatIndex == 0 ? NG_FORMAT_SC16_Q11 : NG_FORMAT_CI8;
        expected_format =
            formatIndex == 0 ? BLADERF_FORMAT_SC16_Q11_META : BLADERF_FORMAT_SC8_Q7_META;
        CHECK(NgBladerfCapabilities(&config, &caps, &error) == NG_OK &&
              caps.format == config.bladeFormat);
        attempts = configuration_calls;
        configuration_result = BLADERF_ERR_UNSUPPORTED;
        CHECK(NgBladerfOpen(&config, &source, &error) == NG_ERROR);
        CHECK(configuration_calls == attempts + 1 && opened == closed);
        if (config.bladeFormat == NG_FORMAT_CI8) {
            CHECK(strstr(error.message, "native SC8 Q7") != NULL);
            CHECK(strstr(error.message, "FPGA") != NULL);
        }
        configuration_result = BLADERF_ERR_IO;
        CHECK(NgBladerfOpen(&config, &source, &error) == NG_ERROR);
        CHECK(configuration_calls == attempts + 2 && opened == closed);
        configuration_result = 0;
        for (testCase = 0; testCase < 11; ++testCase) {
            CheckStream(&config, testCase);
        }
        CheckRequestGeometry(&config, 16000, 16000u);
        CheckRequestGeometry(&config, 1000, 16000u);
        CheckRequestGeometry(&config, 16385, 16384u);
        CHECK(NgBladerfOpen(&config, &source, &error) == NG_OK);
        enable_result = BLADERF_ERR_IO;
        CHECK(source.ops->start(source.context, &error) == NG_ERROR);
        NgFrontendDestroy(&source);
        CHECK(opened == closed);
        enable_result = 0;
        CHECK(NgBladerfOpen(&config, &source, &error) == NG_OK);
        NgFrontendDestroy(&source);
        CHECK(opened == closed);
    }
    puts("PASS native bladeRF SC16 Q11/SC8 Q7 selection, signed extrema, metadata continuity, "
         "partial timeouts, rejection without fallback and lifecycle");
    return 0;
}
