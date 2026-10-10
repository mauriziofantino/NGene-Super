/** @file metadata.c @brief Bounded, versioned sidecar reader for recorded CI8 streams. */
#include "ngene/capture.h"
#include "platform/files.h"
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

/** @brief Trim an owned line without allocating or changing its embedded characters.
 * @param text Writable NUL-terminated line.
 * @return Pointer into the same line, valid for its lifetime.
 */
static char *Trim(char *text)
{
    char *end;
    while (isspace((unsigned char)*text) != 0) {
        ++text;
    }
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1]) != 0) {
        --end;
    }
    *end = '\0';
    return text;
}

/** @brief Parse one strictly unsigned decimal integer with an explicit upper bound.
 * @param text NUL-terminated decimal value.
 * @param maximum Maximum accepted value.
 * @param value Receives the parsed integer on success.
 * @return Nonzero on success; zero for sign, overflow or trailing characters.
 */
static int Number(const char *text, uint64_t maximum, uint64_t *value)
{
    char *end;
    uintmax_t parsed;
    if (*text < '0' || *text > '9') {
        return 0;
    }
    errno = 0;
    parsed = strtoumax(text, &end, 10);
    if (errno != 0 || *end != '\0' || parsed > maximum) {
        return 0;
    }
    *value = (uint64_t)parsed;
    return 1;
}

/** @brief Return the basename of a portable receiver-side path.
 * @param path Borrowed NUL-terminated path with either separator style.
 * @return Borrowed suffix, valid for the lifetime of path.
 */
static const char *BaseName(const char *path)
{
    const char *base = path;
    const char *cursor;
    for (cursor = path; *cursor != '\0'; ++cursor) {
        if (*cursor == '/' || *cursor == '\\') {
            base = cursor + 1;
        }
    }
    return base;
}

/** @brief Parse one bounded integer key and continue the metadata-reading loop.
 * @details Local expansion uses ReadMetadata's key/value/number/error variables.
 * @param name Full INI key to match.
 * @param maximum Inclusive upper bound accepted by Number.
 * @param destination Assignment statements executed only after successful parsing.
 */
#define NG_META_INTEGER_KEY(name, maximum, destination)                                            \
    if (strcmp(key, name) == 0) {                                                                  \
        if (Number(value, maximum, &number) == 0) {                                                \
            return NgFail(error, "Invalid capture metadata: %s", key);                             \
        }                                                                                          \
        destination;                                                                               \
        continue;                                                                                  \
    }

/** @brief Read a versioned sidecar into an owned description.
 * @details Required capture fields are checked independently of optional descriptive fields.
 * Rejects duplicate keys, unsupported formats and contradictory per-file associations.
 * @param file Borrowed stream, retained by caller.
 * @param rawPath Selected raw file for association checks.
 * @param out Description containing the measured raw sample count.
 * @param error Optional parsing diagnostic.
 * @return NG_OK on a consistent schema; NG_ERROR otherwise.
 */
static int ReadMetadata(FILE *file, const char *rawPath, TNgCaptureMetadata *out, TNgError *error)
{
    char line[1024];
    char section[32] = "";
    char keys[64][128];
    size_t keyCount = 0;
    size_t lineNumber = 0;
    unsigned required = 0;
    uint64_t declaredSamples = 0;
    int countSeen = 0;
    int fileSeen = 0;
    int resultSeen = 0;
    int digitalSeen = 0;
    int64_t digitalIf = 0;
    out->dcShift = NG_DEFAULT_DC_SHIFT;
    while (fgets(line, sizeof(line), file) != NULL) {
        char *text;
        char *equals;
        char *value;
        char key[128];
        uint64_t number = 0;
        size_t i;
        ++lineNumber;
        if (strchr(line, '\n') == NULL && !feof(file) && strlen(line) == sizeof(line) - 1) {
            return NgFail(error, "Capture metadata line %zu is too long", lineNumber);
        }
        text = Trim(line);
        if (*text == '\0' || *text == '#' || *text == ';') {
            continue;
        }
        if (*text == '[') {
            size_t length = strlen(text);
            if (length < 3 || text[length - 1] != ']' || length - 2 >= sizeof(section)) {
                return NgFail(error, "Invalid capture metadata section");
            }
            text[length - 1] = '\0';
            (void)snprintf(section, sizeof(section), "%s", text + 1);
            if (strcmp(section, "capture") != 0 && strcmp(section, "result") != 0) {
                return NgFail(error, "Unsupported capture metadata section");
            }
            continue;
        }
        equals = strchr(text, '=');
        if (equals == NULL || section[0] == '\0') {
            return NgFail(error, "Invalid capture metadata line %zu", lineNumber);
        }
        *equals = '\0';
        text = Trim(text);
        value = Trim(equals + 1);
        if (*text == '\0' ||
            snprintf(key, sizeof(key), "%s.%s", section, text) >= (int)sizeof(key)) {
            return NgFail(error, "Invalid capture metadata key");
        }
        for (i = 0; i < keyCount; ++i) {
            if (strcmp(keys[i], key) == 0) {
                return NgFail(error, "Duplicate capture metadata key: %s", key);
            }
        }
        if (keyCount == 64) {
            return NgFail(error, "Too many capture metadata keys");
        }
        (void)snprintf(keys[keyCount++], sizeof(keys[0]), "%s", key);
        NG_META_INTEGER_KEY("capture.version", 2, out->version = (unsigned)number; required |= 1u)
        NG_META_INTEGER_KEY("capture.sample_rate_hz", UINT32_MAX,
                            out->sampleRateHz = (uint32_t)number;
                            required |= 2u)
        NG_META_INTEGER_KEY("capture.center_frequency_hz", UINT64_C(6000000000),
                            out->centerFrequencyHz = number;
                            required |= 4u)
        NG_META_INTEGER_KEY("capture.signal_frequency_hz", UINT64_C(6000000000),
                            out->signalFrequencyHz = number)
        NG_META_INTEGER_KEY("capture.bandwidth_hz", UINT32_MAX, out->bandwidthHz = (uint32_t)number)
        NG_META_INTEGER_KEY("capture.dc_shift", 24, out->dcShift = (unsigned)number)
        NG_META_INTEGER_KEY("result.written_samples", UINT64_MAX, declaredSamples = number;
                            countSeen = 1)
        NG_META_INTEGER_KEY("result.gap_events", UINT64_MAX, out->hasGaps = number != 0)
#undef NG_META_INTEGER_KEY
        if (strcmp(key, "capture.format") == 0) {
            if (strcmp(value, "CI8") != 0) {
                return NgFail(error, "Replay supports signed CI8 capture metadata only");
            }
            required |= 8u;
        } else if (strcmp(key, "capture.layout") == 0) {
            if (strcmp(value, "I,Q interleaved") != 0) {
                return NgFail(error, "Replay requires I,Q interleaved samples");
            }
            required |= 16u;
        } else if (strcmp(key, "capture.component_bits") == 0) {
            if (strcmp(value, "8") != 0) {
                return NgFail(error, "Replay requires 8-bit components");
            }
            required |= 32u;
        } else if (strcmp(key, "capture.component_signed") == 0) {
            if (strcmp(value, "true") != 0) {
                return NgFail(error, "Replay requires signed components");
            }
            required |= 64u;
        } else if (strcmp(key, "capture.recording_point") == 0) {
            if (strcmp(value, "pre_conditioning") != 0 && strcmp(value, "post_conditioning") != 0) {
                return NgFail(error, "Invalid capture recording point");
            }
            out->preConditioning = strcmp(value, "pre_conditioning") == 0;
            required |= 128u;
        } else if (strcmp(key, "capture.conditioning") == 0) {
            if (*value == '\0' || strlen(value) >= sizeof(out->conditioningName)) {
                return NgFail(error, "Invalid recorded conditioner label");
            }
            (void)snprintf(out->conditioningName, sizeof(out->conditioningName), "%s", value);
            out->dcRemoval = strcmp(value, "dc_iir") == 0;
            required |= 256u;
        } else if (strcmp(key, "capture.data_file") == 0) {
            if (strcmp(value, BaseName(rawPath)) != 0) {
                return NgFail(error, "Sidecar is associated with a different I/Q file");
            }
            fileSeen = 1;
        } else if (strcmp(key, "capture.digital_if_hz") == 0) {
            char *end;
            intmax_t parsed;
            errno = 0;
            parsed = strtoimax(value, &end, 10);
            if (*value == '\0' || errno != 0 || *end != '\0' || parsed < -INT64_C(6000000000) ||
                parsed > INT64_C(6000000000)) {
                return NgFail(error, "Invalid recorded digital IF");
            }
            digitalIf = (int64_t)parsed;
            digitalSeen = 1;
        } else if (strcmp(key, "result.complete") == 0) {
            if (strcmp(value, "true") != 0 && strcmp(value, "false") != 0) {
                return NgFail(error, "Invalid capture completion flag");
            }
            out->complete = strcmp(value, "true") == 0;
            resultSeen = 1;
        }
    }
    if (ferror(file) != 0 || required != 511u || out->version == 0 || out->sampleRateHz == 0 ||
        out->dcShift < 4 ||
        (out->version == 2 && (fileSeen == 0 || countSeen == 0 || resultSeen == 0)) ||
        (out->version == 2 && declaredSamples != out->samples)) {
        return NgFail(error, "Incomplete metadata or I/Q file length does not match its sidecar");
    }
    if (out->signalFrequencyHz == 0) {
        out->signalFrequencyHz = out->centerFrequencyHz;
    }
    if (digitalSeen != 0 &&
        digitalIf != (int64_t)out->signalFrequencyHz - (int64_t)out->centerFrequencyHz) {
        return NgFail(error, "Recorded digital IF contradicts the RF frequencies");
    }
    out->available = 1;
    return NG_OK;
}

/** @brief Inspect a closed CI8 file and discover its adjacent capture metadata.
 * Parameter path: Existing regular CI8 file.
 * Parameter metadata: Receives owned validated results on success.
 * Parameter error: Optional diagnostic.
 * Result: NG_OK for a usable file; NG_ERROR on file or metadata errors.
 */
int NgReplayInspect(const char *path, TNgCaptureMetadata *metadata, TNgError *error)
{
    TNgCaptureMetadata result = {0};
    uint64_t bytes = 0;
    uint64_t metaBytes = 0;
    FILE *file;
    size_t n;
    size_t i;
    int status;
    if (path == NULL || metadata == NULL || strchr(path, '\n') != NULL ||
        strchr(path, '\r') != NULL || strlen(path) >= sizeof(((TNgConfig *)0)->replayPath) ||
        NgRegularFile(path, &bytes) != 0 || bytes == 0 || bytes % sizeof(TNgIq8) != 0) {
        return NgFail(error, "Select a nonempty regular file of complete signed CI8 pairs");
    }
    result.samples = bytes / sizeof(TNgIq8);
    (void)snprintf(result.metadataPath, sizeof(result.metadataPath), "%s.meta.ini", path);
    if (NgRegularFile(result.metadataPath, &metaBytes) != 0) {
        if (NgPathExists(result.metadataPath) != 0) {
            return NgFail(error, "Capture sidecar is not a readable regular file");
        }
        /* Historical NGene recordings have one sidecar shared by six-digit segments. */
        n = strlen(path);
        if (n >= 11 && strcmp(path + n - 4, ".cs8") == 0 && path[n - 11] == '_') {
            for (i = n - 10; i < n - 4 && isdigit((unsigned char)path[i]) != 0; ++i) {
            }
            if (i == n - 4) {
                (void)snprintf(result.metadataPath, sizeof(result.metadataPath), "%.*s.meta.ini",
                               (int)(n - 11), path);
            }
        }
        if (NgRegularFile(result.metadataPath, &metaBytes) != 0) {
            if (NgPathExists(result.metadataPath) != 0) {
                return NgFail(error, "Capture sidecar is not a readable regular file");
            }
            result.metadataPath[0] = '\0';
            *metadata = result;
            return NG_OK;
        }
    }
    if (metaBytes == 0 || metaBytes > UINT64_C(16384)) {
        return NgFail(error, "Capture sidecar must contain 1..16384 bytes");
    }
    file = fopen(result.metadataPath, "rb");
    if (file == NULL) {
        return NgFail(error, "Cannot read capture sidecar");
    }
    status = ReadMetadata(file, path, &result, error);
    if (fclose(file) != 0 && status == NG_OK) {
        status = NgFail(error, "Cannot close capture sidecar");
    }
    if (status == NG_OK) {
        *metadata = result;
    }
    return status;
}

/** @brief Prepare a stopped replay profile from a selected file and its metadata.
 * Parameter path: Selected receiver-side file.
 * Parameter config: In/out profile, unchanged on failure.
 * Parameter metadata: Optional inspection result destination.
 * Parameter error: Optional diagnostic.
 * Result: NG_OK on success; NG_ERROR if selection or validation fails.
 */
int NgReplayConfigure(const char *path, TNgConfig *config, TNgCaptureMetadata *metadata,
                      TNgError *error)
{
    TNgCaptureMetadata found = {0};
    TNgConfig draft;
    if (config == NULL) {
        return NgFail(error, "Replay configuration is required");
    }
    if (NgReplayInspect(path, &found, error) != NG_OK) {
        return NG_ERROR;
    }
    draft = *config;
    draft.frontend = NG_FRONTEND_FILE;
    (void)snprintf(draft.replayPath, sizeof(draft.replayPath), "%s", path);
    draft.replayStartSample = 0;
    draft.replaySampleLimit = 0;
    draft.runSeconds = 0;
    draft.recordingEnabled = 0;
    draft.recordOnly = 0;
    draft.dcRemoval = 0;
    if (found.available != 0) {
        draft.sampleRateHz = found.sampleRateHz;
        draft.centerFrequencyHz = found.centerFrequencyHz;
        draft.signalFrequencyHz = found.signalFrequencyHz;
        draft.bandwidthHz = found.bandwidthHz;
        draft.dcShift = found.dcShift;
        draft.windowSamples = found.sampleRateHz / 1000u;
        if (draft.windowSamples == 0) {
            draft.windowSamples = 1;
        }
    }
    if (NgConfigValidate(&draft, error) != NG_OK) {
        return NG_ERROR;
    }
    *config = draft;
    if (metadata != NULL) {
        *metadata = found;
    }
    return NG_OK;
}
