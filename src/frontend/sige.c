/** SiGe GN3Sv2/SE4110L native marker firmware, matching Classic ID 1.
 * Classic-compatible outstanding USB reads absorb scheduling bursts; callbacks only publish
 * completion. Decode and ring conversion execute on the common source worker. */
#include "frontend/sige.h"
#ifdef _WIN32
#include <winsock2.h>
#endif
#include "platform/platform.h"
#include <libusb.h>
#include <stdlib.h>
#include <string.h>

enum { TRANSFERS = 128, BYTES = 8192, VID = 0x16c0, PID = 0x072f };

typedef struct {
    struct libusb_transfer *transfer;
    unsigned char *bytes;
    int ready, flight;
} Slot;

typedef struct {
    libusb_context *usb;
    libusb_device_handle *device;
    Slot slots[TRANSFERS];
    TNgSigeDecoder decoder;
    TNgIq8 *output;
    size_t capacity, offset;
    unsigned current;
    uint64_t produced;
    int claimed, started, hasRun;
    char serial[128];
} Sige;

static void LIBUSB_CALL Complete(struct libusb_transfer *transfer)
{
    Slot *slot = transfer->user_data;
    slot->flight = 0;
    slot->ready = 1;
}

static int Submit(Slot *slot)
{
    int rc;
    slot->ready = 0;
    rc = libusb_submit_transfer(slot->transfer);
    if (rc == 0) {
        slot->flight = 1;
    }
    return rc;
}

static int Stop(void *context, TNgError *error)
{
    Sige *s = context;
    unsigned k;
    int active, rc = 0;
    if (s->started) {
        rc = libusb_control_transfer(s->device, 0x40, 1, 0, 0, NULL, 0, 1000);
        s->started = 0;
    }
    for (k = 0; k < TRANSFERS; k++) {
        if (s->slots[k].flight) {
            libusb_cancel_transfer(s->slots[k].transfer);
        }
    }
    do {
        struct timeval timeout = {0, 10000};
        active = 0;
        for (k = 0; k < TRANSFERS; k++) {
            active += s->slots[k].flight;
        }
        if (active) {
            libusb_handle_events_timeout(s->usb, &timeout);
        }
    } while (active);
    return rc < 0 ? NgFail(error, "SiGe stop command failed: %s", libusb_error_name(rc)) : NG_OK;
}

static int Start(void *context, TNgError *error)
{
    Sige *s = context;
    unsigned k;
    int rc;
    if (s->hasRun) {
        return NgFail(error, "SiGe source supports one run");
    }
    s->hasRun = 1;
    for (k = 0; k < TRANSFERS; k++) {
        if ((rc = Submit(&s->slots[k])) != 0) {
            TNgError ignored = {{0}};
            Stop(s, &ignored);
            return NgFail(error, "SiGe USB submit: %s", libusb_error_name(rc));
        }
    }
    /* Match Classic: queue reads before enabling Cypress GPIF. */
    s->started = 1;
    rc = libusb_control_transfer(s->device, 0x40, 1, 1, 0, NULL, 0, 1000);
    if (rc < 0) {
        TNgError ignored = {{0}};
        Stop(s, &ignored);
        return NgFail(error, "SiGe start command: %s", libusb_error_name(rc));
    }
    return NG_OK;
}

static int Acquire(void *context, TNgIqView *view, unsigned timeoutMs, TNgError *error)
{
    Sige *s = context;
    size_t pairs = 0;
    double deadline = NgMonotonicSeconds() + timeoutMs * 0.001;
    if (!s->started) {
        return NgFail(error, "SiGe is not running");
    }
    while (!pairs) {
        Slot *slot = &s->slots[s->current];
        size_t bytes, capacity;
        while (!slot->ready) {
            struct timeval wait = {0, 1000};
            int rc;
            if (NgMonotonicSeconds() >= deadline) {
                return NG_TIMEOUT;
            }
            rc = libusb_handle_events_timeout(s->usb, &wait);
            if (rc && rc != LIBUSB_ERROR_INTERRUPTED) {
                return NgFail(error, "SiGe USB events: %s", libusb_error_name(rc));
            }
        }
        if (slot->transfer->status != LIBUSB_TRANSFER_COMPLETED) {
            return NgFail(error,
                          "SiGe USB transfer failed/disconnected (%d); missing duration unknown",
                          (int)slot->transfer->status);
        }
        if (slot->transfer->actual_length < 0 || slot->transfer->actual_length > BYTES) {
            return NgFail(error, "SiGe invalid USB length");
        }
        bytes = (size_t)slot->transfer->actual_length - s->offset;
        capacity = 2 * s->capacity - (s->decoder.pending ? 1u : 0u);
        if (bytes > capacity) {
            bytes = capacity;
        }
        if (NgSigeDecode(&s->decoder, slot->bytes + s->offset, bytes, s->output, s->capacity,
                         &pairs) != NG_OK) {
            return NgFail(
                error,
                "SiGe native I/Q markers lost; reception stopped without fabricating samples");
        }
        s->offset += bytes;
        if (s->offset == (size_t)slot->transfer->actual_length) {
            int rc = Submit(slot);
            s->offset = 0;
            s->current = (s->current + 1u) % TRANSFERS;
            if (rc) {
                return NgFail(error, "SiGe USB resubmit: %s", libusb_error_name(rc));
            }
        }
        if (NgMonotonicSeconds() >= deadline && !pairs) {
            return NG_TIMEOUT;
        }
    }
    if (UINT64_MAX - s->produced < pairs) {
        return NgFail(error, "SiGe sample index overflow");
    }
    memset(view, 0, sizeof(*view));
    view->samples = s->output;
    view->format = NG_FORMAT_CI8;
    view->meta.firstSample = s->produced;
    view->meta.count = pairs;
    if (s->decoder.gap) {
        view->meta.flags = NG_BLOCK_GAP;
        s->decoder.gap = 0;
    }
    s->produced += pairs;
    return NG_OK;
}

static void Release(void *context)
{
    (void)context;
}

static void Stats(void *context, TNgFrontendStats *stats)
{
    Sige *s = context;
    memset(stats, 0, sizeof(*stats));
    stats->receivedSamples = s->produced;
    stats->hardwareLossState = NG_DIAGNOSTIC_UNSUPPORTED;
}

static void Destroy(void *context)
{
    Sige *s = context;
    unsigned k;
    TNgError ignored = {{0}};
    Stop(s, &ignored);
    for (k = 0; k < TRANSFERS; k++) {
        if (s->slots[k].transfer) {
            libusb_free_transfer(s->slots[k].transfer);
        }
        free(s->slots[k].bytes);
    }
    if (s->claimed) {
        libusb_release_interface(s->device, 2);
    }
    if (s->device) {
        libusb_close(s->device);
    }
    if (s->usb) {
        libusb_exit(s->usb);
    }
    free(s->output);
    free(s);
}

static const TNgFrontendOps ops = {Start, Acquire, Release, Stop, Stats, Destroy};

int NgSigeList(FILE *output, TNgError *error)
{
    libusb_context *usb = NULL;
    libusb_device **list = NULL;
    ssize_t n, k;
    if (libusb_init(&usb) != 0) {
        return NgFail(error, "Cannot initialize SiGe USB");
    }
    n = libusb_get_device_list(usb, &list);
    if (n < 0) {
        libusb_exit(usb);
        return NgFail(error, "Cannot enumerate SiGe USB");
    }
    for (k = 0; k < n; k++) {
        struct libusb_device_descriptor d;
        if (libusb_get_device_descriptor(list[k], &d) == 0 && d.idVendor == VID &&
            d.idProduct == PID) {
            fprintf(output, "SiGe native IQ USB %u:%u (16c0:072f)\n",
                    libusb_get_bus_number(list[k]), libusb_get_device_address(list[k]));
        }
    }
    libusb_free_device_list(list, 1);
    libusb_exit(usb);
    return NG_OK;
}

int NgSigeOpen(const TNgConfig *config, TNgFrontend *frontend, TNgError *error)
{
    Sige *s = calloc(1, sizeof(*s));
    libusb_device **list = NULL;
    ssize_t n, k;
    unsigned j;
    int rc, configuration;
    if (!s) {
        return NgFail(error, "Cannot allocate SiGe source");
    }
    if (libusb_init(&s->usb) != 0) {
        goto failed;
    }
    n = libusb_get_device_list(s->usb, &list);
    if (n < 0) {
        goto failed;
    }
    for (k = 0; k < n; k++) {
        struct libusb_device_descriptor d;
        libusb_device_handle *device = NULL;
        unsigned char serial[128] = {0};
        if (libusb_get_device_descriptor(list[k], &d) != 0 || d.idVendor != VID ||
            d.idProduct != PID) {
            continue;
        }
        if (libusb_open(list[k], &device) != 0) {
            continue;
        }
        if (d.iSerialNumber) {
            libusb_get_string_descriptor_ascii(device, d.iSerialNumber, serial, 127);
        }
        if (config->serial[0] && strcmp(config->serial, (char *)serial) != 0) {
            libusb_close(device);
            continue;
        }
        if (s->device) {
            libusb_close(device);
            libusb_free_device_list(list, 1);
            list = NULL;
            NgFail(error, "Multiple SiGe devices: select a serial");
            goto failed;
        }
        s->device = device;
        memcpy(s->serial, serial, sizeof(s->serial));
    }
    libusb_free_device_list(list, 1);
    list = NULL;
    if (!s->device) {
        NgFail(error,
               "SiGe 16c0:072f not accessible; native-IQ firmware and USB driver are required");
        goto failed;
    }
    if (libusb_get_configuration(s->device, &configuration) != 0) {
        goto failed;
    }
    if (configuration != 1 && libusb_set_configuration(s->device, 1) != 0) {
        goto failed;
    }
    rc = libusb_set_auto_detach_kernel_driver(s->device, 1);
    if (rc != 0 && rc != LIBUSB_ERROR_NOT_SUPPORTED) {
        goto failed;
    }
    if (libusb_claim_interface(s->device, 2) != 0) {
        goto failed;
    }
    s->claimed = 1;
    if (libusb_set_interface_alt_setting(s->device, 2, 0) != 0) {
        goto failed;
    }
#if defined(_WIN32) && LIBUSB_API_VERSION >= 0x0100010C
    /* FX2 FIFO streaming needs reads queued directly to WinUSB. Without RAW_IO
     * this device completes the first read and leaves the remaining queue idle. */
    rc = libusb_endpoint_supports_raw_io(s->device, 0x86);
    if (rc == 1) {
        int maximum = libusb_get_max_raw_io_transfer_size(s->device, 0x86);
        int packet = libusb_get_max_packet_size(libusb_get_device(s->device), 0x86);
        if (maximum < BYTES || packet <= 0 || BYTES % packet != 0) {
            NgFail(error, "SiGe WinUSB RAW_IO transfer geometry is unsupported");
            goto failed;
        }
        rc = libusb_endpoint_set_raw_io(s->device, 0x86, 1);
    }
    if (rc < 0) {
        NgFail(error, "SiGe RAW_IO setup: %s", libusb_error_name(rc));
        goto failed;
    }
#endif
    s->capacity = config->windowSamples;
    s->output = malloc(s->capacity * sizeof(*s->output));
    if (!s->output) {
        goto failed;
    }
    for (j = 0; j < TRANSFERS; j++) {
        Slot *slot = &s->slots[j];
        slot->bytes = malloc(BYTES);
        slot->transfer = libusb_alloc_transfer(0);
        if (!slot->bytes || !slot->transfer) {
            goto failed;
        }
        libusb_fill_bulk_transfer(slot->transfer, s->device, 0x86, slot->bytes, BYTES, Complete,
                                  slot, 0);
    }
    memset(frontend, 0, sizeof(*frontend));
    frontend->ops = &ops;
    frontend->context = s;
    frontend->info.sampleRateHz = NG_SIGE_RATE_HZ;
    frontend->info.centerFrequencyHz = NG_SIGE_CENTER_HZ;
    frontend->info.signalFrequencyHz = NG_SIGE_SIGNAL_HZ;
    frontend->info.digitalIfHz = NG_SIGE_IF_HZ;
    frontend->info.format = NG_FORMAT_CI8;
    frontend->info.adcBits = 1;
    frontend->info.sourceName = "sige";
    frontend->info.deviceIdentifier = s->serial;
    frontend->info.clockDomain = "sige-device-samples";
    return NG_OK;
failed:
    if (list) {
        libusb_free_device_list(list, 1);
    }
    if (error && !error->message[0]) {
        NgFail(error, "SiGe USB initialization failed");
    }
    Destroy(s);
    return NG_ERROR;
}
