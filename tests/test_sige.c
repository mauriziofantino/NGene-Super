/** Native USB ABI fixture; never linked into the receiver or used as RF evidence. */
#ifdef _WIN32
#include <winsock2.h>
#endif
#include <libusb.h>
#include "frontend/sige.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x);                                        \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
static int object, initialized, claimed, allocations, enable, failStart, corrupt, disconnect;
static struct libusb_transfer *queue[4096], *allocated[128];
static unsigned readIndex, writeIndex, cancelled[128];
static uint64_t byteIndex;
#if defined(_WIN32) && LIBUSB_API_VERSION >= 0x0100010C
libusb_device *LIBUSB_CALL libusb_get_device(libusb_device_handle *d)
{
    (void)d;
    return (libusb_device *)&object;
}

int LIBUSB_CALL libusb_get_max_packet_size(libusb_device *d, unsigned char ep)
{
    (void)d;
    CHECK(ep == 0x86);
    return 512;
}

int LIBUSB_CALL libusb_endpoint_supports_raw_io(libusb_device_handle *d, unsigned char ep)
{
    (void)d;
    CHECK(ep == 0x86);
    return 1;
}

int LIBUSB_CALL libusb_get_max_raw_io_transfer_size(libusb_device_handle *d, unsigned char ep)
{
    (void)d;
    CHECK(ep == 0x86);
    return 65536;
}

int LIBUSB_CALL libusb_endpoint_set_raw_io(libusb_device_handle *d, unsigned char ep, int value)
{
    (void)d;
    CHECK(ep == 0x86 && value == 1);
    return 0;
}
#endif
static unsigned Index(struct libusb_transfer *t)
{
    unsigned k;
    for (k = 0; k < 128; k++) {
        if (allocated[k] == t) {
            return k;
        }
    }
    CHECK(0);
    return 0;
}

int LIBUSB_CALL libusb_init(libusb_context **ctx)
{
    *ctx = (libusb_context *)&object;
    initialized++;
    return 0;
}

void LIBUSB_CALL libusb_exit(libusb_context *ctx)
{
    (void)ctx;
    initialized--;
}

ssize_t LIBUSB_CALL libusb_get_device_list(libusb_context *ctx, libusb_device ***list)
{
    (void)ctx;
    *list = calloc(2, sizeof(**list));
    CHECK(*list);
    (*list)[0] = (libusb_device *)&object;
    return 1;
}

void LIBUSB_CALL libusb_free_device_list(libusb_device **list, int unref)
{
    (void)unref;
    free(list);
}

int LIBUSB_CALL libusb_get_device_descriptor(libusb_device *dev, struct libusb_device_descriptor *d)
{
    (void)dev;
    memset(d, 0, sizeof(*d));
    d->idVendor = 0x16c0;
    d->idProduct = 0x072f;
    return 0;
}

uint8_t LIBUSB_CALL libusb_get_bus_number(libusb_device *dev)
{
    (void)dev;
    return 1;
}

uint8_t LIBUSB_CALL libusb_get_device_address(libusb_device *dev)
{
    (void)dev;
    return 2;
}

int LIBUSB_CALL libusb_open(libusb_device *dev, libusb_device_handle **handle)
{
    (void)dev;
    *handle = (libusb_device_handle *)&object;
    return 0;
}

void LIBUSB_CALL libusb_close(libusb_device_handle *handle)
{
    (void)handle;
    CHECK(!claimed);
}

int LIBUSB_CALL libusb_get_string_descriptor_ascii(libusb_device_handle *d, uint8_t index,
                                                   unsigned char *text, int length)
{
    (void)d;
    (void)index;
    if (length) {
        text[0] = 0;
    }
    return 0;
}

int LIBUSB_CALL libusb_get_configuration(libusb_device_handle *d, int *config)
{
    (void)d;
    *config = 0;
    return 0;
}

int LIBUSB_CALL libusb_set_configuration(libusb_device_handle *d, int config)
{
    (void)d;
    CHECK(config == 1);
    return 0;
}

int LIBUSB_CALL libusb_set_auto_detach_kernel_driver(libusb_device_handle *d, int value)
{
    (void)d;
    CHECK(value == 1);
    return LIBUSB_ERROR_NOT_SUPPORTED;
}

int LIBUSB_CALL libusb_claim_interface(libusb_device_handle *d, int interfaceNumber)
{
    (void)d;
    CHECK(interfaceNumber == 2);
    claimed++;
    return 0;
}

int LIBUSB_CALL libusb_release_interface(libusb_device_handle *d, int interfaceNumber)
{
    (void)d;
    CHECK(interfaceNumber == 2);
    claimed--;
    return 0;
}

int LIBUSB_CALL libusb_set_interface_alt_setting(libusb_device_handle *d, int interfaceNumber,
                                                 int alt)
{
    (void)d;
    CHECK(interfaceNumber == 2 && alt == 0);
    return 0;
}

struct libusb_transfer *LIBUSB_CALL libusb_alloc_transfer(int packets)
{
    unsigned k;
    struct libusb_transfer *t;
    CHECK(packets == 0);
    t = calloc(1, sizeof(*t));
    CHECK(t);
    for (k = 0; k < 128 && allocated[k]; k++) {
    }
    CHECK(k < 128);
    allocated[k] = t;
    allocations++;
    return t;
}

void LIBUSB_CALL libusb_free_transfer(struct libusb_transfer *t)
{
    allocated[Index(t)] = NULL;
    allocations--;
    free(t);
}

int LIBUSB_CALL libusb_submit_transfer(struct libusb_transfer *t)
{
    CHECK(t->endpoint == 0x86 && t->length == 8192);
    CHECK(writeIndex < 4096);
    queue[writeIndex++] = t;
    cancelled[Index(t)] = 0;
    return 0;
}

int LIBUSB_CALL libusb_cancel_transfer(struct libusb_transfer *t)
{
    cancelled[Index(t)] = 1;
    return 0;
}

int LIBUSB_CALL libusb_control_transfer(libusb_device_handle *d, uint8_t type, uint8_t request,
                                        uint16_t value, uint16_t index, unsigned char *data,
                                        uint16_t length, unsigned int timeout)
{
    (void)d;
    (void)data;
    CHECK(type == 0x40 && request == 1 && index == 0 && length == 0 && timeout == 1000);
    CHECK(value <= 1);
    enable = value;
    if (value && failStart) {
        return LIBUSB_ERROR_IO;
    }
    return 0;
}

int LIBUSB_CALL libusb_handle_events_timeout(libusb_context *ctx, struct timeval *timeout)
{
    struct libusb_transfer *t;
    unsigned k;
    (void)ctx;
    (void)timeout;
    if (readIndex == writeIndex) {
        return 0;
    }
    t = queue[readIndex++];
    if (cancelled[Index(t)]) {
        t->status = LIBUSB_TRANSFER_CANCELLED;
    } else if (disconnect) {
        t->status = LIBUSB_TRANSFER_NO_DEVICE;
    } else {
        t->status = LIBUSB_TRANSFER_COMPLETED;
        t->actual_length = 257;
        for (k = 0; k < 257; k++, byteIndex++) {
            t->buffer[k] = (unsigned char)((byteIndex % 2 ? 2 : 0) | (((byteIndex + 1) / 2) % 2));
        }
        if (corrupt) {
            t->buffer[10] ^= 2;
        }
    }
    t->callback(t);
    return 0;
}

const char *LIBUSB_CALL libusb_error_name(int code)
{
    (void)code;
    return "fixture-error";
}

static void Run(int mode)
{
    TNgConfig config;
    TNgFrontend source = {0};
    TNgIqView view;
    TNgError error = {{0}};
    unsigned k, total = 0;
    readIndex = writeIndex = 0;
    byteIndex = 0;
    failStart = mode == 1;
    corrupt = mode == 2;
    disconnect = mode == 3;
    NgConfigDefaults(&config);
    NgConfigSige(&config);
    config.windowSamples = 13;
    CHECK(NgSigeOpen(&config, &source, &error) == NG_OK);
    CHECK(source.info.sampleRateHz == 8183800 && source.info.digitalIfHz == 38400 &&
          source.info.adcBits == 1);
    if (failStart) {
        CHECK(source.ops->start(source.context, &error) == NG_ERROR);
    } else {
        CHECK(source.ops->start(source.context, &error) == NG_OK && enable);
        if (mode) {
            CHECK(source.ops->acquire(source.context, &view, 100, &error) == NG_ERROR);
        } else {
            for (k = 0; k < 40; k++) {
                size_t j;
                CHECK(source.ops->acquire(source.context, &view, 100, &error) == NG_OK);
                CHECK(view.meta.firstSample == total && view.meta.count <= 13);
                if (!total) {
                    CHECK((view.meta.flags & NG_BLOCK_GAP) && !view.meta.missingKnown);
                }
                for (j = 0; j < view.meta.count; j++) {
                    int expected = (total + j + 1) % 2 ? -1 : 1;
                    CHECK(view.samples[j].i == expected && view.samples[j].q == expected);
                }
                total += (unsigned)view.meta.count;
                source.ops->release(source.context);
            }
        }
        CHECK(source.ops->stop(source.context, &error) == NG_OK && !enable);
    }
    NgFrontendDestroy(&source);
    CHECK(!initialized && !claimed && !allocations && readIndex == writeIndex);
}

int main(void)
{
    unsigned k;
    for (k = 0; k < 4; k++) {
        Run((int)k);
    }
    puts("PASS: SiGe USB ABI, queued reads, odd native-IQ transfers, GPIF controls, "
         "marker/disconnect failure and cancellation cleanup.");
    return 0;
}
