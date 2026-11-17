/** SiGe GN3Sv2/SE4110L native marker firmware, matching Classic ID 1.
 * Classic-compatible outstanding USB reads absorb scheduling bursts; callbacks only publish
 * completion. Decode and ring conversion execute on the common source worker. */
#include "frontend/sige.h"
#ifdef _WIN32
#include <winsock2.h>
#endif
#include "platform/platform.h"
#include "sige_firmware.h"
#include <libusb.h>
#include <stdlib.h>
#include <string.h>

enum {
    TRANSFERS = 128,
    BYTES = 8192,
    VID = 0x16c0,
    PID = 0x072f,
    BOOT_VID = 0x1781,
    BOOT_PID = 0x0b39,
    FX2_CPUCS = 0xe600,
    FX2_RAM_REQUEST = 0xa0,
    FX2_MAX_WRITE = 64,
    FIRMWARE_TIMEOUT_MS = 1000,
    REENUMERATION_TIMEOUT_MS = 5000,
    REENUMERATION_POLL_MS = 50
};

typedef struct {
    uint16_t address;
    unsigned char data[255];
    unsigned char length, type;
} FirmwareRecord;

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

static int HexDigit(unsigned char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

static int ReadHexByte(const unsigned char *text, unsigned char *value)
{
    int high = HexDigit(text[0]);
    int low = HexDigit(text[1]);
    if (high < 0 || low < 0) {
        return 0;
    }
    *value = (unsigned char)((high << 4) | low);
    return 1;
}

static int ReadFirmwareRecord(size_t *position, FirmwareRecord *record)
{
    unsigned char bytes[260];
    size_t start, end, count, k;
    unsigned sum = 0;
    while (*position < ngSigeFirmwareHexLength &&
           (ngSigeFirmwareHex[*position] == '\r' || ngSigeFirmwareHex[*position] == '\n')) {
        (*position)++;
    }
    if (*position == ngSigeFirmwareHexLength) {
        return 0;
    }
    start = *position;
    if (ngSigeFirmwareHex[start++] != ':') {
        return -1;
    }
    end = start;
    while (end < ngSigeFirmwareHexLength && ngSigeFirmwareHex[end] != '\r' &&
           ngSigeFirmwareHex[end] != '\n') {
        end++;
    }
    if ((end - start) < 10 || ((end - start) & 1u) != 0) {
        return -1;
    }
    count = (end - start) / 2;
    if (count > sizeof(bytes)) {
        return -1;
    }
    for (k = 0; k < count; k++) {
        if (!ReadHexByte(&ngSigeFirmwareHex[start + 2 * k], &bytes[k])) {
            return -1;
        }
        sum += bytes[k];
    }
    if (count != (size_t)bytes[0] + 5u || (sum & 0xffu) != 0) {
        return -1;
    }
    record->length = bytes[0];
    record->address = (uint16_t)(((uint16_t)bytes[1] << 8) | bytes[2]);
    record->type = bytes[3];
    if (record->length) {
        memcpy(record->data, &bytes[4], record->length);
    }
    *position = end;
    return 1;
}

static int ValidateFirmware(TNgError *error)
{
    size_t position = 0;
    unsigned records = 0;
    int ended = 0, rc;
    FirmwareRecord record;
    while ((rc = ReadFirmwareRecord(&position, &record)) > 0) {
        uint32_t limit;
        if (ended) {
            return NgFail(error, "SiGe firmware contains records after EOF");
        }
        if (record.type == 0) {
            limit = (uint32_t)record.address + record.length;
            if (limit > 0x10000u ||
                ((uint32_t)record.address <= FX2_CPUCS && limit > FX2_CPUCS)) {
                return NgFail(error, "SiGe firmware contains an invalid FX2 address");
            }
            records++;
        } else if (record.type == 1 && record.length == 0 && record.address == 0) {
            ended = 1;
        } else {
            return NgFail(error, "SiGe firmware contains an unsupported Intel HEX record");
        }
    }
    if (rc < 0) {
        return NgFail(error, "SiGe firmware Intel HEX record is invalid");
    }
    if (!ended || records == 0) {
        return NgFail(error, "SiGe firmware Intel HEX is incomplete");
    }
    return NG_OK;
}

static int WriteFx2(libusb_device_handle *device, uint16_t address, unsigned char *data,
                    uint16_t length, TNgError *error)
{
    int rc = libusb_control_transfer(device, 0x40, FX2_RAM_REQUEST, address, 0, data, length,
                                     FIRMWARE_TIMEOUT_MS);
    if (rc < 0) {
        return NgFail(error, "SiGe FX2 firmware transfer at 0x%04x: %s", address,
                      libusb_error_name(rc));
    }
    if (rc != length) {
        return NgFail(error, "SiGe FX2 firmware transfer at 0x%04x was incomplete", address);
    }
    return NG_OK;
}

static int UploadFirmware(libusb_device_handle *device, TNgError *error)
{
    size_t position = 0;
    unsigned char reset = 1;
    int rc;
    FirmwareRecord record;
    if (ValidateFirmware(error) != NG_OK) {
        return NG_ERROR;
    }
    if (WriteFx2(device, FX2_CPUCS, &reset, 1, error) != NG_OK) {
        return NG_ERROR;
    }
    while ((rc = ReadFirmwareRecord(&position, &record)) > 0) {
        unsigned offset;
        if (record.type != 0) {
            continue;
        }
        for (offset = 0; offset < record.length; offset += FX2_MAX_WRITE) {
            unsigned remaining = record.length - offset;
            uint16_t chunk = (uint16_t)(remaining > FX2_MAX_WRITE ? FX2_MAX_WRITE : remaining);
            uint16_t address = (uint16_t)(record.address + offset);
            if (WriteFx2(device, address, &record.data[offset], chunk, error) != NG_OK) {
                return NG_ERROR;
            }
        }
    }
    if (rc < 0) {
        return NgFail(error, "SiGe firmware Intel HEX could not be read");
    }
    reset = 0;
    return WriteFx2(device, FX2_CPUCS, &reset, 1, error);
}

static int HasUsbDevice(libusb_context *usb, uint16_t vendor, uint16_t product, int *present,
                        TNgError *error)
{
    libusb_device **list = NULL;
    ssize_t count, k;
    *present = 0;
    count = libusb_get_device_list(usb, &list);
    if (count < 0) {
        return NgFail(error, "Cannot enumerate SiGe USB: %s", libusb_error_name((int)count));
    }
    for (k = 0; k < count; k++) {
        struct libusb_device_descriptor descriptor;
        if (libusb_get_device_descriptor(list[k], &descriptor) == 0 &&
            descriptor.idVendor == vendor && descriptor.idProduct == product) {
            *present = 1;
            break;
        }
    }
    libusb_free_device_list(list, 1);
    return NG_OK;
}

static int LoadBootFirmware(libusb_context *usb, TNgError *error)
{
    libusb_device **list = NULL;
    libusb_device_handle *device = NULL;
    ssize_t count, k;
    int rc, present;
    count = libusb_get_device_list(usb, &list);
    if (count < 0) {
        return NgFail(error, "Cannot enumerate SiGe FX2 USB: %s", libusb_error_name((int)count));
    }
    for (k = 0; k < count; k++) {
        struct libusb_device_descriptor descriptor;
        if (libusb_get_device_descriptor(list[k], &descriptor) != 0 ||
            descriptor.idVendor != BOOT_VID || descriptor.idProduct != BOOT_PID) {
            continue;
        }
        if (device) {
            libusb_close(device);
            device = NULL;
            libusb_free_device_list(list, 1);
            return NgFail(error, "Multiple SiGe FX2 boot devices are connected");
        }
        rc = libusb_open(list[k], &device);
        if (rc != 0) {
            libusb_free_device_list(list, 1);
            return NgFail(error, "Cannot access SiGe FX2 boot device 1781:0b39: %s",
                          libusb_error_name(rc));
        }
    }
    libusb_free_device_list(list, 1);
    if (!device) {
        return NgFail(error,
                      "SiGe USB device not found; expected FX2 boot device 1781:0b39 or native-IQ "
                      "device 16c0:072f");
    }
    rc = UploadFirmware(device, error);
    libusb_close(device);
    if (rc != NG_OK) {
        return rc;
    }
    for (k = 0; k < REENUMERATION_TIMEOUT_MS / REENUMERATION_POLL_MS; k++) {
        NgSleepMs(REENUMERATION_POLL_MS);
        if (HasUsbDevice(usb, VID, PID, &present, error) != NG_OK) {
            return NG_ERROR;
        }
        if (present) {
            return NG_OK;
        }
    }
    return NgFail(error, "SiGe FX2 firmware uploaded, but native-IQ device 16c0:072f did not "
                         "re-enumerate");
}

static int EnsureRuntimeDevice(libusb_context *usb, TNgError *error)
{
    int present;
    if (HasUsbDevice(usb, VID, PID, &present, error) != NG_OK) {
        return NG_ERROR;
    }
    return present ? NG_OK : LoadBootFirmware(usb, error);
}

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
        if (libusb_get_device_descriptor(list[k], &d) == 0) {
            if (d.idVendor == VID && d.idProduct == PID) {
                fprintf(output, "SiGe native IQ USB %u:%u (16c0:072f)\n",
                        libusb_get_bus_number(list[k]), libusb_get_device_address(list[k]));
            } else if (d.idVendor == BOOT_VID && d.idProduct == BOOT_PID) {
                fprintf(output, "SiGe FX2 boot USB %u:%u (1781:0b39; firmware loads on open)\n",
                        libusb_get_bus_number(list[k]), libusb_get_device_address(list[k]));
            }
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
    if (EnsureRuntimeDevice(s->usb, error) != NG_OK) {
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
        NgFail(error, "SiGe runtime device 16c0:072f is not accessible; check the USB driver "
                      "and permissions");
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
    frontend->info.spectrumInverted = 1;
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
