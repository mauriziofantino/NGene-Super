/** @file test_bladerf.c @brief Native API fixture for SC16 Q11 and timestamp integrity; no RF
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
};

/** @brief Inject invalid native sample counts. */
static int malformed;

/** @brief Enumerate one fixture. @param list Receives owned list. @return One. */
int CALL_CONV bladerf_get_device_list(struct bladerf_devinfo **list)
{
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
    return *device ? 0 : -1;
}

/** @brief Close stopped fixture. @param device Owned state. */
void CALL_CONV bladerf_close(struct bladerf *device)
{
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
 * @param format Must retain SC16 Q11 metadata.
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
    (void)device;
    CHECK(layout == BLADERF_RX_X1 && format == BLADERF_FORMAT_SC16_Q11_META &&
          buffers > transfers && size % 1024 == 0 && timeout == 100);
    return 0;
}

/** @brief Accept RX enable/disable. @param device Fixture. @param channel RX0. @param enable
 * Desired state. @return Zero. */
int CALL_CONV bladerf_enable_module(struct bladerf *device, bladerf_channel channel, bool enable)
{
    (void)device;
    (void)channel;
    (void)enable;
    return 0;
}

/** @brief Produce two valid blocks with a five-pair hardware-clock gap, then return timeouts.
 * @param device Fixture.
 * @param samples Writable production-owned SC16 Q11 buffer.
 * @param count Requested pair capacity.
 * @param metadata Receives hardware tick, contiguous length and overrun flag.
 * @param timeout Bounded wait.
 * @return Zero for blocks; native timeout when idle.
 */
int CALL_CONV bladerf_sync_rx(struct bladerf *device, void *samples, unsigned count,
                              struct bladerf_metadata *metadata, unsigned timeout)
{
    unsigned index;
    TNgIqView view = {0};
    (void)timeout;
    if (device->calls >= 2) {
        NgSleepMs(1);
        return BLADERF_ERR_TIMEOUT;
    }
    metadata->timestamp = device->calls == 0 ? 1000u : 1037u;
    metadata->status = device->calls == 0 ? 0u : (uint32_t)BLADERF_META_STATUS_OVERRUN;
    metadata->actual_count = malformed ? count + 1u : device->calls == 0 ? 32u : 8u;
    view.samples = samples;
    view.format = NG_FORMAT_SC16_Q11;
    for (index = 0; index < 32; ++index) {
        NgIqStore(samples, view.format, index, 0, 1235);
        NgIqStore(samples, view.format, index, 1, -1901);
    }
    ++device->calls;
    return 0;
}

/** @brief Run native precision, timestamp, gain and separated-loss contracts. @return Zero after
 * assertions. */
int main(void)
{
    TNgConfig config;
    TNgFrontend source = {0};
    TNgFrontendStats stats;
    TNgFrontendCaps caps;
    TNgError error = {{0}};
    TNgIqView view;
    unsigned scenario, index;
    NgConfigDefaults(&config);
    config.frontend = NG_FRONTEND_BLADERF;
    config.windowSamples = 4;
    config.ingressBlocks = 64;
    strcpy(config.serial, "blade-test");
    CHECK(NgBladerfCapabilities(&config, &caps, &error) == NG_OK && caps.gainMinDb == -12 &&
          caps.gainMaxDb == 60);
    config.bladeGainDb = 61;
    CHECK(NgBladerfOpen(&config, &source, &error) == NG_ERROR);
    config.bladeGainDb = 30;
    for (scenario = 0; scenario < 3; ++scenario) {
        malformed = scenario == 2;
        config.ingressBlocks = scenario == 1 ? 2 : 64;
        CHECK(NgBladerfOpen(&config, &source, &error) == NG_OK &&
              source.info.format == NG_FORMAT_SC16_Q11);
        CHECK(source.ops->start(source.context, &error) == NG_OK);
        if (scenario == 0) {
            for (index = 0; index < 10; ++index) {
                CHECK(source.ops->acquire(source.context, &view, 1000, &error) == NG_OK);
                CHECK(NgIqComponent(&view, 0, 0) == 1235 && NgIqComponent(&view, 0, 1) == -1901);
                CHECK((view.meta.flags & NG_BLOCK_HARDWARE_TIME) != 0);
                CHECK(view.meta.hardwareTimestamp == 1000u + view.meta.firstSample);
                if (index == 8) {
                    CHECK(view.meta.firstSample == 37 && view.meta.missingKnown &&
                          view.meta.missingBefore == 5 && (view.meta.flags & NG_BLOCK_GAP));
                }
                source.ops->release(source.context);
            }
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
        if (scenario == 0) {
            CHECK(stats.receivedSamples == 40 && stats.deviceGapEvents == 1 &&
                  stats.deviceMissingSamples == 5 && stats.droppedSamples == 0);
        }
        if (scenario == 1) {
            CHECK(stats.droppedSamples == 24 && stats.transportErrors == 0 &&
                  stats.ingress.highWaterBlocks == 2);
        }
        if (scenario == 2) {
            CHECK(stats.receivedSamples == 0 && stats.transportErrors == 1 &&
                  stats.droppedSamples == 0);
        }
        CHECK(stats.hardwareLossState == NG_DIAGNOSTIC_UNSUPPORTED);
        NgFrontendDestroy(&source);
    }
    puts("PASS native bladeRF adapter fixture, all Q11 bits, metadata gaps, gain bounds and "
         "overflow-stop");
    return 0;
}
