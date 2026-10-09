/** @file test_hackrf.c @brief Native adapter contracts using the real vendor ABI.
 * This does not replace on-device USB/RF validation. */
#include "ngene/frontend.h"
#include "fixtures.h"
#include <hackrf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x);                                        \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
#include "frontend/hackrf_adapter.h"
static int initialPairs = 6;
static int oddTransfer = 0;
static int failRate = 0;
static int alive = 0;
static int initialized = 0;
static hackrf_sample_block_cb_fn receiveCallback;
static void *receiveContext;
static int fakeDevice;

/**
 * @brief Increment the simulated vendor initialization balance.
 * @details Test-only implementation of the external C ABI; never linked into the
 * receiver executable. Assertions terminate on a configuration mismatch.
 * @return HACKRF_SUCCESS except the explicitly injected failure.
 */
int ADDCALL hackrf_init(void)
{
    ++initialized;
    return HACKRF_SUCCESS;
}

/**
 * @brief Decrement the simulated vendor initialization balance.
 * @details Test-only implementation of the external C ABI; never linked into the
 * receiver executable. Assertions terminate on a configuration mismatch.
 * @return HACKRF_SUCCESS except the explicitly injected failure.
 */
int ADDCALL hackrf_exit(void)
{
    --initialized;
    return HACKRF_SUCCESS;
}

/**
 * @brief Return the immutable test driver version label.
 * @details Test-only implementation of the external C ABI; never linked into the
 * receiver executable. Assertions terminate on a configuration mismatch.
 * @return Immutable static string; do not free.
 */
const char *ADDCALL hackrf_library_version(void)
{
    return "test-driver";
}

/**
 * @brief Return the immutable simulated driver error label.
 * @details Test-only implementation of the external C ABI; never linked into the
 * receiver executable. Assertions terminate on a configuration mismatch.
 * @param error Vendor error code (label is fixed in this fixture).
 * @return Immutable static string; do not free.
 */
const char *ADDCALL hackrf_error_name(enum hackrf_error error)
{
    (void)error;
    return "simulated-error";
}

/**
 * @brief Allocate a list containing one deterministic test device.
 * @details Test-only implementation of the external C ABI; never linked into the
 * receiver executable. Assertions terminate on a configuration mismatch.
 * @return Owned list; free with hackrf_device_list_free.
 */
hackrf_device_list_t *ADDCALL hackrf_device_list(void)
{
    static char *serials[] = {"test-device"};
    hackrf_device_list_t *list = calloc(1, sizeof(*list));
    CHECK(list != NULL);
    list->devicecount = 1;
    list->serial_numbers = serials;
    return list;
}

/**
 * @brief Free the list allocated by the simulated enumeration.
 * @details Test-only implementation of the external C ABI; never linked into the
 * receiver executable. Assertions terminate on a configuration mismatch.
 * @param list Simulated enumeration list.
 */
void ADDCALL hackrf_device_list_free(hackrf_device_list_t *list)
{
    free(list);
}

/**
 * @brief Assert device index zero and return the fixture handle.
 * @details Test-only implementation of the external C ABI; never linked into the
 * receiver executable. Assertions terminate on a configuration mismatch.
 * @param list Simulated enumeration list.
 * @param index Device index, expected to be zero.
 * @param device Receives the nonowned static fixture handle.
 * @return HACKRF_SUCCESS except the explicitly injected failure.
 */
int ADDCALL hackrf_device_list_open(hackrf_device_list_t *list, int index, hackrf_device **device)
{
    (void)list;
    CHECK(index == 0);
    *device = (hackrf_device *)&fakeDevice;
    return HACKRF_SUCCESS;
}

/**
 * @brief Assert the test serial and return the fixture handle.
 * @details Test-only implementation of the external C ABI; never linked into the
 * receiver executable. Assertions terminate on a configuration mismatch.
 * @param serial Expected NUL-terminated test serial.
 * @param device Receives the nonowned static fixture handle.
 * @return HACKRF_SUCCESS except the explicitly injected failure.
 */
int ADDCALL hackrf_open_by_serial(const char *serial, hackrf_device **device)
{
    CHECK(strcmp(serial, "test-device") == 0);
    *device = (hackrf_device *)&fakeDevice;
    return HACKRF_SUCCESS;
}

/**
 * @brief Accept closure of the fixture handle without freeing static storage.
 * @details Test-only implementation of the external C ABI; never linked into the
 * receiver executable. Assertions terminate on a configuration mismatch.
 * @param device Static fixture device handle; no heap ownership is transferred.
 * @return HACKRF_SUCCESS except the explicitly injected failure.
 */
int ADDCALL hackrf_close(hackrf_device *device)
{
    (void)device;
    return HACKRF_SUCCESS;
}

/**
 * @brief Check 16 MS/s and optionally inject a configuration failure.
 * @details Test-only implementation of the external C ABI; never linked into the
 * receiver executable. Assertions terminate on a configuration mismatch.
 * @param device Static fixture device handle; no heap ownership is transferred.
 * @param rate Requested complex sample rate in pairs per second.
 * @return HACKRF_SUCCESS except the explicitly injected failure.
 */
int ADDCALL hackrf_set_sample_rate(hackrf_device *device, const double rate)
{
    (void)device;
    CHECK(rate == 16000000);
    return (failRate) != 0 ? HACKRF_ERROR_INVALID_PARAM : HACKRF_SUCCESS;
}

/**
 * @brief Assert GPS L1 hardware tuning at 1575420000 Hz.
 * @details Test-only implementation of the external C ABI; never linked into the
 * receiver executable. Assertions terminate on a configuration mismatch.
 * @param device Static fixture device handle; no heap ownership is transferred.
 * @param hz Requested frequency or bandwidth in Hz.
 * @return HACKRF_SUCCESS except the explicitly injected failure.
 */
int ADDCALL hackrf_set_freq(hackrf_device *device, const uint64_t hz)
{
    (void)device;
    CHECK(hz == 1575420000);
    return HACKRF_SUCCESS;
}

/**
 * @brief Assert the expected 15 MHz baseband filter.
 * @details Test-only implementation of the external C ABI; never linked into the
 * receiver executable. Assertions terminate on a configuration mismatch.
 * @param device Static fixture device handle; no heap ownership is transferred.
 * @param hz Requested frequency or bandwidth in Hz.
 * @return HACKRF_SUCCESS except the explicitly injected failure.
 */
int ADDCALL hackrf_set_baseband_filter_bandwidth(hackrf_device *device, const uint32_t hz)
{
    (void)device;
    CHECK(hz == 15000000);
    return HACKRF_SUCCESS;
}

/**
 * @brief Return the deterministic 15 MHz bandwidth fixture.
 * @details Test-only implementation of the external C ABI; never linked into the
 * receiver executable. Assertions terminate on a configuration mismatch.
 * @param hz Requested frequency or bandwidth in Hz.
 * @return Bandwidth fixture in Hz.
 */
uint32_t ADDCALL hackrf_compute_baseband_filter_bw_round_down_lt(const uint32_t hz)
{
    (void)hz;
    return 15000000;
}

/**
 * @brief Return the requested bandwidth for this test driver.
 * @details Test-only implementation of the external C ABI; never linked into the
 * receiver executable. Assertions terminate on a configuration mismatch.
 * @param hz Requested frequency or bandwidth in Hz.
 * @return Bandwidth fixture in Hz.
 */
uint32_t ADDCALL hackrf_compute_baseband_filter_bw(const uint32_t hz)
{
    return hz;
}

/**
 * @brief Assert 16 dB LNA gain.
 * @details Test-only implementation of the external C ABI; never linked into the
 * receiver executable. Assertions terminate on a configuration mismatch.
 * @param device Static fixture device handle; no heap ownership is transferred.
 * @param value Expected configured gain or enable value.
 * @return HACKRF_SUCCESS except the explicitly injected failure.
 */
int ADDCALL hackrf_set_lna_gain(hackrf_device *device, uint32_t value)
{
    (void)device;
    CHECK(value == 16);
    return HACKRF_SUCCESS;
}

/**
 * @brief Assert 20 dB VGA gain.
 * @details Test-only implementation of the external C ABI; never linked into the
 * receiver executable. Assertions terminate on a configuration mismatch.
 * @param device Static fixture device handle; no heap ownership is transferred.
 * @param value Expected configured gain or enable value.
 * @return HACKRF_SUCCESS except the explicitly injected failure.
 */
int ADDCALL hackrf_set_vga_gain(hackrf_device *device, uint32_t value)
{
    (void)device;
    CHECK(value == 20);
    return HACKRF_SUCCESS;
}

/**
 * @brief Assert the RF amplifier is disabled.
 * @details Test-only implementation of the external C ABI; never linked into the
 * receiver executable. Assertions terminate on a configuration mismatch.
 * @param device Static fixture device handle; no heap ownership is transferred.
 * @param value Expected configured gain or enable value.
 * @return HACKRF_SUCCESS except the explicitly injected failure.
 */
int ADDCALL hackrf_set_amp_enable(hackrf_device *device, const uint8_t value)
{
    (void)device;
    CHECK(value == 0);
    return HACKRF_SUCCESS;
}

/**
 * @brief Assert antenna power is disabled.
 * @details Test-only implementation of the external C ABI; never linked into the
 * receiver executable. Assertions terminate on a configuration mismatch.
 * @param device Static fixture device handle; no heap ownership is transferred.
 * @param value Expected configured gain or enable value.
 * @return HACKRF_SUCCESS except the explicitly injected failure.
 */
int ADDCALL hackrf_set_antenna_enable(hackrf_device *device, const uint8_t value)
{
    (void)device;
    CHECK(value == 0);
    return HACKRF_SUCCESS;
}

/**
 * @brief Verify emit behaviour.
 *
 * @details
 * Test-only helper exercising the named contract with deterministic inputs. Caller owns
 * fixture/context storage; assertion failures abort the test.
 *
 * @param pairs Test parameter controlling this contract scenario.
 */
static void Emit(int pairs)
{
    uint8_t buffer[128];
    hackrf_transfer transfer;
    int k;
    CHECK(pairs * 2 < (int)sizeof(buffer));
    memset(&transfer, 0, sizeof(transfer));
    for (k = 0; k < pairs; ++k) {
        buffer[k * 2] = (uint8_t)k;
        buffer[k * 2 + 1] = (uint8_t)(-k);
    }
    transfer.buffer = buffer;
    transfer.buffer_length = (int)sizeof(buffer);
    transfer.valid_length = pairs * 2 + oddTransfer;
    transfer.rx_ctx = receiveContext;
    if (receiveCallback(&transfer) != 0) {
        alive = 0;
    }
    memset(buffer, 0xff, sizeof(buffer)); /* Prove callback data was copied. */
}

/**
 * @brief Store the borrowed RX callback and emit the initial fixture transfer.
 * @details Test-only implementation of the external C ABI; never linked into the
 * receiver executable. Assertions terminate on a configuration mismatch.
 * @param device Static fixture device handle; no heap ownership is transferred.
 * @param callback Borrowed RX callback retained until stop.
 * @param context Borrowed callback context retained until stop.
 * @return HACKRF_SUCCESS except the explicitly injected failure.
 */
int ADDCALL hackrf_start_rx(hackrf_device *device, hackrf_sample_block_cb_fn callback,
                            void *context)
{
    (void)device;
    receiveCallback = callback;
    receiveContext = context;
    alive = 1;
    Emit(initialPairs);
    return HACKRF_SUCCESS;
}

/**
 * @brief Report whether the simulated RX stream is active.
 * @details Test-only implementation of the external C ABI; never linked into the
 * receiver executable. Assertions terminate on a configuration mismatch.
 * @param device Static fixture device handle; no heap ownership is transferred.
 * @return HACKRF_TRUE while active, otherwise HACKRF_ERROR_STREAMING_STOPPED.
 */
int ADDCALL hackrf_is_streaming(hackrf_device *device)
{
    (void)device;
    return (alive) != 0 ? HACKRF_TRUE : HACKRF_ERROR_STREAMING_STOPPED;
}

/**
 * @brief Mark the simulated RX stream stopped.
 * @details Test-only implementation of the external C ABI; never linked into the
 * receiver executable. Assertions terminate on a configuration mismatch.
 * @param device Static fixture device handle; no heap ownership is transferred.
 * @return HACKRF_SUCCESS except the explicitly injected failure.
 */
int ADDCALL hackrf_stop_rx(hackrf_device *device)
{
    (void)device;
    alive = 0;
    return HACKRF_SUCCESS;
}

/**
 * @brief Verify destroy source behaviour.
 *
 * @details
 * Test-only helper exercising the named contract with deterministic inputs. Caller owns
 * fixture/context storage; assertion failures abort the test.
 *
 * @param f Caller-owned test fixture or output storage used by this helper.
 * @param e Optional diagnostic output used for test failures.
 */
static void DestroySource(TNgFrontend *f, TNgError *e)
{
    CHECK(f->ops->stop(f->context, e) == NG_OK);
    f->ops->destroy(f->context);
    CHECK(initialized == 0);
}

/**
 * @brief Run the C contract test executable.
 *
 * @details
 * Assertions abort with a nonzero process status on contract failures. This executable is
 * development tooling, not receiver runtime.
 *
 * @return Zero if every contract assertion passes.
 */
int main(void)
{
    TNgConfig c;
    TNgFrontend f;
    TNgError e = {{0}};
    TNgIqView v;
    TNgFrontendStats stats;
    NgConfigDefaults(&c);
    c.frontend = NG_FRONTEND_HACKRF;
    c.windowSamples = 4;
    c.ingressBlocks = 2;
    CHECK(NgHackrfOpen(&c, &f, &e) == NG_OK);
    CHECK(f.info.bandwidthHz == 15000000);
    CHECK(f.ops->start(f.context, &e) == NG_OK);
    CHECK(f.ops->acquire(f.context, &v, 0, &e) == NG_OK);
    CHECK(v.meta.count == 4 && v.samples[3].i == 3 && v.samples[3].q == -3);
    {
        TNgConfig replayConfig = c; /* Independent file-source configuration. */
        TNgFrontend replay;         /* File descriptor owns a separate window and cursor. */
        TNgIqView replayView;       /* Borrowed only until this source releases it. */
        replayConfig.frontend = NG_FRONTEND_FILE;
        replayConfig.replayRealtime = 0;
        strcpy(replayConfig.replayPath, "ngene_hackrf_parallel_replay.cs8");
        CHECK(NgTestWriteTone(replayConfig.replayPath, 32, c.sampleRateHz) == NG_OK);
        CHECK(NgFrontendOpen(&replayConfig, &replay, &e) == NG_OK);
        CHECK(replay.ops->start(replay.context, &e) == NG_OK);
        CHECK(replay.ops->acquire(replay.context, &replayView, 0, &e) == NG_OK);
        CHECK(replayView.samples != v.samples && replayView.samples[0].i == 32);
        CHECK(v.samples[3].i == 3); /* File reads cannot alter the active native ingress lease. */
        replay.ops->release(replay.context);
        CHECK(replay.ops->stop(replay.context, &e) == NG_OK);
        NgFrontendDestroy(&replay);
        CHECK(alive != 0 && initialized != 0);
        CHECK(remove(replayConfig.replayPath) == 0);
    }
    f.ops->release(f.context);
    CHECK(f.ops->acquire(f.context, &v, 0, &e) == NG_OK);
    CHECK(v.meta.firstSample == 4 && v.meta.count == 2);
    f.ops->release(f.context);
    CHECK(f.ops->acquire(f.context, &v, 0, &e) == NG_TIMEOUT);
    DestroySource(&f, &e);
    initialPairs = 10;
    c.overflowPolicy = NG_OVERFLOW_DROP_NEWEST;
    CHECK(NgHackrfOpen(&c, &f, &e) == NG_OK);
    CHECK(f.ops->start(f.context, &e) == NG_OK);
    CHECK(f.ops->acquire(f.context, &v, 0, &e) == NG_OK);
    f.ops->release(f.context);
    CHECK(f.ops->acquire(f.context, &v, 0, &e) == NG_OK);
    f.ops->release(f.context);
    Emit(3);
    CHECK(f.ops->acquire(f.context, &v, 0, &e) == NG_OK);
    CHECK(v.meta.firstSample == 10);
    f.ops->release(f.context);
    CHECK(f.ops->stop(f.context, &e) == NG_OK);
    f.ops->stats(f.context, &stats);
    CHECK(stats.receivedSamples == 13 && stats.droppedSamples == 2);
    f.ops->destroy(f.context);
    CHECK(initialized == 0);
    c.overflowPolicy = NG_OVERFLOW_STOP;
    CHECK(NgHackrfOpen(&c, &f, &e) == NG_OK);
    CHECK(f.ops->start(f.context, &e) == NG_OK);
    CHECK(f.ops->acquire(f.context, &v, 0, &e) == NG_ERROR);
    DestroySource(&f, &e);
    initialPairs = 2;
    oddTransfer = 1;
    CHECK(NgHackrfOpen(&c, &f, &e) == NG_OK);
    CHECK(f.ops->start(f.context, &e) == NG_OK);
    CHECK(f.ops->acquire(f.context, &v, 0, &e) == NG_ERROR);
    DestroySource(&f, &e);
    oddTransfer = 0;
    failRate = 1;
    CHECK(NgHackrfOpen(&c, &f, &e) == NG_ERROR);
    CHECK(initialized == 0);
    puts("PASS: native HackRF API config, callback lifetime, overflow/drop indices, malformed "
         "transfer and cleanup (simulated driver)");
    return 0;
}
