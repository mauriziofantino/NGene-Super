/** @file server.c @brief Local HTTP control and bounded WebSocket telemetry. */
#include "ngene/web.h"
#include "ngene/capture.h"
#include "ngene/control.h"
#include "ngene/build.h"
#include "ngene/dsp/fft.h"
#include "ngene/dsp/sample_ops.h"
#include <math.h>
#include "platform/platform.h"
#include "platform/files.h"
#include <civetweb.h>
#include <cJSON.h>
#include "web_assets.h"
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

/** @brief Maximum request body bytes including bounded configuration JSON. */
#define MAX_BODY 16384

/** @brief JSON integer dispatch with finite, integral and destination-range validation. */
#define UINT_FIELD(name, member)                                                                   \
    if (strcmp(key, name) == 0) {                                                                  \
        if (cJSON_IsNumber(v) == 0 || isfinite(v->valuedouble) == 0 || v->valuedouble < 0 ||       \
            v->valuedouble > NG_ANALYSIS_PAIRS || floor(v->valuedouble) != v->valuedouble) {       \
            return NgFail(error, "Expected bounded integer: %s", key);                             \
        }                                                                                          \
        out->member = (unsigned)v->valuedouble;                                                    \
        continue;                                                                                  \
    }
/** @brief JSON real dispatch rejecting nonnumeric and non-finite inputs. */
#define REAL_FIELD(name, member)                                                                   \
    if (strcmp(key, name) == 0) {                                                                  \
        if (cJSON_IsNumber(v) == 0 || isfinite(v->valuedouble) == 0) {                             \
            return NgFail(error, "Expected finite number: %s", key);                               \
        }                                                                                          \
        out->member = v->valuedouble;                                                              \
        continue;                                                                                  \
    }
/** @brief JSON boolean dispatch accepting only a JSON true or false value. */
#define BOOL_FIELD(name, member)                                                                   \
    if (strcmp(key, name) == 0) {                                                                  \
        if (cJSON_IsBool(v) == 0) {                                                                \
            return NgFail(error, "Expected boolean: %s", key);                                     \
        }                                                                                          \
        out->member = cJSON_IsTrue(v);                                                             \
        continue;                                                                                  \
    }

/** @brief Local HTTP service with independent primary and replay controllers. */
typedef struct {
    /** Owned primary controller, independent of replayControl. */
    TNgControl *control;
    /** Owned file-replay controller with its own engine, clock and rings. */
    TNgControl *replayControl;
    /** Owned CivetWeb listener stopped before controller destruction. */
    struct mg_context *http;
    /** Serializes creation/copy of request-local large status JSON snapshots. */
    TNgMutex jsonMutex;
    /** Serializes start/stop requests with cross-session file deletion checks. */
    TNgMutex commandMutex;
    /** Atomic shutdown request polled by the service owner. */
    TNgAtomicInt quit;
    /** Number of open bounded telemetry connections. */
    unsigned websocketClients;
    /** NUL-terminated 32-byte random token encoded as hexadecimal. */
    char token[65];
    /** Expected numeric loopback host and port. */
    char host[64];
    /** Expected localhost host and port. */
    char localhost[64];
} TNgWebServer;

/**
 * @brief Append a text field to an owned JSON object.
 *
 * @details
 * cJSON copies the string; caller retains source and owns the object. Used outside sample
 * callbacks.
 *
 * @param object Owned JSON object.
 * @param key NUL-terminated member name.
 * @param value NUL-terminated member value.
 */
static void String(cJSON *object, const char *key, const char *value)
{
    cJSON_AddStringToObject(object, key, value);
}

/**
 * @brief Append a finite numeric telemetry field.
 *
 * @details
 * cJSON represents numbers as double; uint64 counters instead use Counter to preserve exact
 * values.
 *
 * @param object Owned JSON object.
 * @param key NUL-terminated member name.
 * @param value Numeric member value.
 */
static void Number(cJSON *object, const char *key, double value)
{
    cJSON_AddNumberToObject(object, key, value);
}

/**
 * @brief Serialize a uint64 counter as a decimal JSON string.
 *
 * @details
 * Avoids loss beyond the JavaScript double integer range. Stack buffer is copied into the JSON
 * object.
 *
 * @param object Owned JSON object.
 * @param key NUL-terminated member name.
 * @param value Unsigned counter value.
 */
static void Counter(cJSON *object, const char *key, uint64_t value)
{
    char text[32];
    snprintf(text, sizeof(text), "%" PRIu64, value);
    String(object, key, text);
}

/**
 * @brief Write bounded HTTP response headers.
 *
 * @details
 * Emits content length, no-cache and same-origin content policy. HTTP thread may block on its
 * socket; receiver is independent.
 *
 * @param connection Borrowed CivetWeb connection.
 * @param status HTTP status code.
 * @param type Content type.
 * @param length Response body length in bytes.
 *
 * @return Return value of mg_printf (bytes written or write error).
 */
static int Headers(struct mg_connection *connection, int status, const char *type, size_t length)
{
    return mg_printf(connection,
                     "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
                     "Cache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n"
                     "Content-Security-Policy: default-src 'self'; script-src 'self'; style-src "
                     "'self'; img-src 'self' data:; connect-src 'self' ws://127.0.0.1:* "
                     "ws://localhost:*; frame-ancestors 'none'; base-uri 'none'\r\n"
                     "Connection: close\r\n\r\n",
                     status, status < 400 ? "OK" : "Error", type, length);
}

/**
 * @brief Serialize, send and destroy a JSON response.
 *
 * @details
 * Takes ownership of object and frees serialized text on all paths. Allocation failure becomes
 * HTTP 503. Never holds receiver data locks during socket writes.
 *
 * @param connection Borrowed HTTP connection.
 * @param code Requested HTTP status.
 * @param object Owned JSON response; consumed by this call, may be NULL.
 *
 * @return HTTP status sent.
 */
static int JsonReply(struct mg_connection *connection, int code, cJSON *object)
{
    char *text = (object) != NULL ? cJSON_PrintUnformatted(object) : NULL;
    const char *body = (text) != NULL ? text : "{\"error\":\"Cannot allocate response\"}";
    if (text == NULL) {
        code = 503;
    }
    Headers(connection, code, "application/json; charset=utf-8", strlen(body));
    mg_write(connection, body, strlen(body));
    cJSON_free(text);
    cJSON_Delete(object);
    return code;
}

/**
 * @brief Send a JSON error response.
 *
 * @details
 * Creates an error object and delegates ownership/serialization to JsonReply.
 *
 * @param connection Borrowed HTTP connection.
 * @param code Error HTTP status code.
 * @param message Error message copied into JSON.
 *
 * @return HTTP status sent.
 */
static int Failure(struct mg_connection *connection, int code, const char *message)
{
    cJSON *object = cJSON_CreateObject();
    String(object, "error", message);
    return JsonReply(connection, code, object);
}

/**
 * @brief Check local Host and same-origin request constraints.
 *
 * @details
 * Requires expected loopback Host; WebSocket also requires Origin. Mutation token verification
 * is performed separately.
 *
 * @param server Server state with allowed host strings.
 * @param connection Borrowed connection/request headers.
 * @param websocket Nonzero for a WebSocket handshake.
 *
 * @return Nonzero when request origin/host are accepted.
 */
static int AllowedRequest(TNgWebServer *server, const struct mg_connection *connection,
                          int websocket)
{
    const char *host = mg_get_header(connection, "Host");
    const char *origin = mg_get_header(connection, "Origin");
    char expected[96];
    if (host == NULL || (strcmp(host, server->host) != 0 && strcmp(host, server->localhost) != 0)) {
        return 0;
    }
    snprintf(expected, sizeof(expected), "http://%s", host);
    if ((websocket != 0 && origin == NULL) || (origin != NULL && strcmp(origin, expected) != 0)) {
        return 0;
    }
    return 1;
}

/**
 * @brief Serialize diagnostic settings to a new JSON object.
 *
 * @details
 * Includes discrete FFT, overlap/rates, pre/post choices and temporal settings.
 *
 * @param c Read-only diagnostic configuration.
 *
 * @return New caller-owned cJSON object; release with cJSON_Delete or JsonReply.
 */
static cJSON *AnalysisConfigJson(const TNgAnalysisConfig *c)
{
    cJSON *o = cJSON_CreateObject();
    cJSON_AddBoolToObject(o, "spectrum_enabled", c->spectrumEnabled);
    cJSON_AddBoolToObject(o, "histogram_enabled", c->histogramEnabled);
    Number(o, "fft_size", (double)c->fftSize);
    String(o, "window", NgWindowName(c->window));
    Number(o, "overlap_percent", c->overlapPercent);
    Number(o, "averages", c->averages);
    Number(o, "update_hz", c->updateHz);
    Number(o, "histogram_pairs", (double)c->histogramPairs);
    Number(o, "histogram_bins", c->histogramBins);
    Number(o, "histogram_update_hz", c->histogramUpdateHz);
    Number(o, "temporal_frames", c->temporalFrames);
    Number(o, "smoothing_alpha", c->smoothingAlpha);
    String(o, "temporal", NgTemporalName(c->temporal));
    String(o, "spectrum_source", (c->spectrumPre) != 0 ? "pre" : "post");
    String(o, "histogram_source", (c->histogramPre) != 0 ? "pre" : "post");
    String(o, "units", (c->density) != 0 ? "dbfs_hz" : "dbfs_bin");
    return o;
}

/**
 * @brief Serialize one I/Q component statistical summary.
 *
 * @details
 * Includes population standard deviation, RMS, extrema, rail and zero counts.
 *
 * @param s Read-only component statistics.
 *
 * @return New caller-owned cJSON object; release with cJSON_Delete or JsonReply.
 */
static cJSON *ComponentJson(const TNgComponentStats *s)
{
    cJSON *o = cJSON_CreateObject();
    Number(o, "mean", s->mean);
    Number(o, "std", s->std);
    Number(o, "rms", s->rms);
    Number(o, "min", s->min);
    Number(o, "max", s->max);
    Number(o, "clipping_count", s->clipping);
    Number(o, "zero_count", s->zeros);
    return o;
}

/**
 * @brief Serialize an owned PSD/histogram snapshot with integrity metadata.
 *
 * @details
 * Rechecks PSD integrity on copied storage; sends bins and metrics, never raw IQ.
 *
 * @param s Read-only by-value diagnostic snapshot.
 *
 * @return New caller-owned cJSON object; release with cJSON_Delete or JsonReply.
 */
static cJSON *AnalysisJson(const TNgAnalysisSnapshot *s)
{
    cJSON *o = cJSON_CreateObject();
    cJSON *power = cJSON_CreateArray();
    cJSON *hi = cJSON_CreateArray();
    cJSON *hq = cJSON_CreateArray();
    size_t k;
    TNgPsdIntegrity check;
    NgPsdCheck(s->spectrumDbfs, s->spectrumBins, &check);
    Counter(o, "sequence", s->sequence);
    Counter(o, "sequence_id", s->psdSequence);
    Counter(o, "histogram_sequence", s->histogramSequence);
    Counter(o, "histogram_first_sample", s->histogramFirst);
    Counter(o, "histogram_epoch", s->histogramEpoch);
    Number(o, "produced_seconds", s->producedSeconds);
    Number(o, "histogram_age_seconds",
           (s->histogramSequence) != 0 ? NgMonotonicSeconds() - s->histogramSeconds : 0);
    Number(o, "min", s->integrity.min);
    Number(o, "max", s->integrity.max);
    Number(o, "mean", s->integrity.mean);
    Number(o, "nan_count", s->integrity.nanCount);
    Number(o, "inf_count", s->integrity.infCount);
    Number(o, "checksum", s->integrity.checksum);
    Number(o, "telemetry_checksum", check.checksum);
    cJSON_AddBoolToObject(o, "integrity_ok",
                          !s->spectrumBins || (check.checksum == s->integrity.checksum &&
                                               !check.nanCount && !check.infCount));
    Number(o, "enbw_hz", s->enbwHz);
    cJSON_AddItemToObject(o, "stats_i", ComponentJson(&s->statsI));
    cJSON_AddItemToObject(o, "stats_q", ComponentJson(&s->statsQ));
    Number(o, "dc_magnitude", s->dcMagnitude);
    String(o, "sample_backend", NgSampleOpsBackend());
    Counter(o, "revision", s->revision);
    Counter(o, "epoch", s->epoch);
    Counter(o, "first_sample", s->firstSample);
    Counter(o, "skipped_bursts", s->skippedBursts);
    Counter(o, "contention_blocks", s->contentionBlocks);
    Number(o, "observed_pairs", (double)s->observedPairs);
    Number(o, "fft_size", (double)s->fftSize);
    Number(o, "sample_rate_hz", s->sampleRateHz);
    Number(o, "bin_hz", s->binHz);
    Number(o, "peak_hz", s->peakHz);
    Number(o, "peak_dbfs", s->peakDbfs);
    Number(o, "compute_ms", s->computeMs);
    Number(o, "age_seconds", (s->sequence) != 0 ? NgMonotonicSeconds() - s->capturedSeconds : 0);
    Number(o, "histogram_count", s->histogramCount);
    Number(o, "clipped_i", s->clippedI);
    Number(o, "clipped_q", s->clippedQ);
    String(o, "point", (s->config.spectrumPre) != 0 ? "pre_conditioning" : "post_conditioning");
    String(o, "backend", NgFftBackend());
    for (k = 0; k < s->spectrumBins; ++k) {
        cJSON_AddItemToArray(power, cJSON_CreateNumber(s->spectrumDbfs[k]));
    }
    for (k = 0; k < s->histogramBins; ++k) {
        cJSON_AddItemToArray(hi, cJSON_CreateNumber(s->histogramI[k]));
        cJSON_AddItemToArray(hq, cJSON_CreateNumber(s->histogramQ[k]));
    }
    cJSON_AddItemToObject(o, "dbfs", power);
    cJSON_AddItemToObject(o, "histogram_i", hi);
    cJSON_AddItemToObject(o, "histogram_q", hq);
    cJSON_AddItemToObject(o, "config", AnalysisConfigJson(&s->config));
    return o;
}

/**
 * @brief Send current diagnostic settings and profile revision.
 *
 * @details
 * Copies controller config before serialization. Response ownership is consumed by JsonReply.
 *
 * @param control Selected live receiver or file replay controller.
 * @param connection Borrowed HTTP connection.
 *
 * @return HTTP response status.
 */
static int AnalysisConfigReply(TNgControl *control, struct mg_connection *connection)
{
    TNgConfig config;
    uint64_t revision;
    cJSON *o = cJSON_CreateObject();
    NgControlConfig(control, &config, &revision);
    Counter(o, "revision", revision);
    cJSON_AddItemToObject(o, "analysis", AnalysisConfigJson(&config.analysis));
    return JsonReply(connection, 200, o);
}

/**
 * @brief Parse a typed bounded diagnostic JSON request.
 *
 * @details
 * Starts from supported defaults and requires the expected field types; new settings are
 * validated before controller publication.
 *
 * @param body Read-only parsed JSON request.
 * @param out Receives draft settings on success.
 * @param error Optional validation error destination.
 *
 * @return NG_OK on success; NG_ERROR for type/range/enum failures.
 */
static int ReadAnalysisConfig(cJSON *body, TNgAnalysisConfig *out, TNgError *error)
{
    cJSON *v;
    NgAnalysisDefaults(out);
    for (v = body->child; v != NULL; v = v->next) {
        const char *key = v->string;
        if (key == NULL) {
            return NgFail(error, "Missing analysis key");
        }
        if (strcmp(key, "revision") == 0) {
            continue;
        }
        UINT_FIELD("fft_size", fftSize)
        UINT_FIELD("overlap_percent", overlapPercent)
        UINT_FIELD("averages", averages)
        UINT_FIELD("histogram_pairs", histogramPairs)
        UINT_FIELD("histogram_bins", histogramBins)
        UINT_FIELD("temporal_frames", temporalFrames)
        REAL_FIELD("update_hz", updateHz)
        REAL_FIELD("histogram_update_hz", histogramUpdateHz)
        REAL_FIELD("smoothing_alpha", smoothingAlpha)
        BOOL_FIELD("spectrum_enabled", spectrumEnabled)
        BOOL_FIELD("histogram_enabled", histogramEnabled)
        if (cJSON_IsString(v) == 0) {
            return NgFail(error, "Expected string: %s", key);
        }
        if (strcmp(key, "window") == 0) {
            int w;
            for (w = NG_WINDOW_RECT; w <= NG_WINDOW_BLACKMAN_HARRIS; ++w) {
                if (strcmp(v->valuestring, NgWindowName((TNgWindow)w)) == 0) {
                    break;
                }
            }
            out->window = (TNgWindow)w;
        } else if (strcmp(key, "temporal") == 0) {
            int mode;
            for (mode = NG_TEMPORAL_NONE; mode <= NG_TEMPORAL_EMA; ++mode) {
                if (strcmp(v->valuestring, NgTemporalName((TNgTemporal)mode)) == 0) {
                    break;
                }
            }
            out->temporal = (TNgTemporal)mode;
        } else if (strcmp(key, "spectrum_source") == 0 || strcmp(key, "histogram_source") == 0) {
            int pre = strcmp(v->valuestring, "pre") == 0;
            if (pre == 0 && strcmp(v->valuestring, "post") != 0) {
                return NgFail(error, "Source must be pre or post");
            }
            if (strcmp(key, "spectrum_source") == 0) {
                out->spectrumPre = pre;
            } else {
                out->histogramPre = pre;
            }
        } else if (strcmp(key, "units") == 0) {
            out->density = strcmp(v->valuestring, "dbfs_hz") == 0;
            if (out->density == 0 && strcmp(v->valuestring, "dbfs_bin") != 0) {
                return NgFail(error, "Unknown PSD units");
            }
        } else {
            return NgFail(error, "Unknown analysis parameter: %s", key);
        }
    }
    return NgAnalysisValidate(out, error);
}

/**
 * @brief Copy lifecycle/diagnostic status and serialize it without holding engine locks.
 *
 * @details
 * Deep copy precedes JSON construction; counters are decimal strings.
 *
 * @param control Selected live receiver or file replay controller.
 *
 * @return New caller-owned cJSON object; release with cJSON_Delete or JsonReply.
 */
static cJSON *StatusJson(TNgControl *control)
{
    static const char *states[] = {"stopped", "starting", "running", "stopping", "error"};
    static const char *recordStates[] = {"off", "starting", "recording", "stopping"};
    TNgControlStatus status;
    cJSON *object = cJSON_CreateObject();
    TNgAnalysisSnapshot telemetry;
    NgControlSnapshot(control, &status);
    /* Deliberate deep copy before telemetry. Snapshot arrays are values, not borrowed pointers. */
    telemetry = status.analysis;
    cJSON_AddItemToObject(object, "analysis", AnalysisJson(&telemetry));
    Number(object, "hardware_frequency_hz", (double)status.hardwareFrequencyHz);
    Number(object, "signal_frequency_hz", (double)status.signalFrequencyHz);
    Number(object, "digital_if_hz", (double)status.digitalIfHz);
    Number(object, "actual_sample_rate_hz", status.actualSampleRateHz);
    String(object, "sample_rate_basis", "configured_nominal");
    String(object, "state", states[status.state]);
    String(object, "recording_state", recordStates[status.recordingState]);
    String(object, "message", status.message);
    String(object, "recording_name", status.recordingName);
    String(object, "output_directory", status.outputDirectory);
    Counter(object, "revision", status.revision);
    Counter(object, "active_revision", status.activeRevision);
    Counter(object, "run_id", status.runId);
    Counter(object, "command_id", status.commandId);
    Counter(object, "completed_command_id", status.completedCommandId);
    cJSON_AddBoolToObject(object, "command_ok", status.commandOk);
    Number(object, "wall_seconds", status.receiver.wallSeconds);
    Number(object, "actual_bandwidth_hz", status.actualBandwidthHz);
    Counter(object, "received_samples", status.receiver.frontend.receivedSamples);
    Counter(object, "processed_samples", status.receiver.processedSamples);
    Counter(object, "windows", status.receiver.windows);
    Counter(object, "gap_events", status.receiver.gapEvents);
    Counter(object, "known_dropped_samples", status.receiver.frontend.droppedSamples);
    Counter(object, "input_queued_samples", status.receiver.frontend.queuedSamples);
    Counter(object, "input_discarded_on_stop", status.receiver.frontend.discardedOnStop);
    Counter(object, "ring_high_water", status.receiver.ringHighWater);
    Counter(object, "written_samples", status.totalWrittenSamples);
    Counter(object, "session_written_samples", status.recording.samples);
    Counter(object, "recording_queued_samples", status.recording.queuedSamples);
    Counter(object, "recording_segments", status.recording.segments);
    return object;
}

/**
 * @brief Serialize a profile into a bounded text buffer.
 *
 * @details
 * Uses a temporary C stream owned by this helper, checks reads/writes and closes it. This work
 * stays on HTTP threads.
 *
 * @param config Read-only configuration.
 * @param text Writable destination for NUL-terminated INI.
 * @param capacity Destination capacity in bytes.
 *
 * @return NG_OK if serialization fits; NG_ERROR otherwise.
 */
static int ConfigText(const TNgConfig *config, char *text, size_t capacity)
{
    FILE *file = tmpfile();
    TNgError error = {{0}};
    size_t length;
    if (file == NULL) {
        return NG_ERROR;
    }
    if (NgConfigWrite(file, config, &error) != NG_OK || fflush(file) != 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NG_ERROR;
    }
    length = fread(text, 1, capacity - 1, file);
    text[length] = '\0';
    if (ferror(file) != 0 || feof(file) == 0) {
        fclose(file);
        return NG_ERROR;
    }
    fclose(file);
    return NG_OK;
}

/**
 * @brief Send the current complete INI profile and revision.
 *
 * @details
 * Snapshot precedes serialization. The HTTP thread owns temporary JSON/text buffers.
 *
 * @param control Selected live receiver or file replay controller.
 * @param connection Borrowed HTTP connection.
 *
 * @return HTTP status sent.
 */
static int ConfigReply(TNgControl *control, struct mg_connection *connection)
{
    TNgConfig config;
    uint64_t revision;
    char ini[8192];
    cJSON *object;
    NgControlConfig(control, &config, &revision);
    if (ConfigText(&config, ini, sizeof(ini)) != NG_OK) {
        return Failure(connection, 500, "Cannot serialize configuration");
    }
    object = cJSON_CreateObject();
    String(object, "ini", ini);
    Counter(object, "revision", revision);
    String(object, "mutability", "stopped");
    return JsonReply(connection, 200, object);
}

/**
 * @brief Parse an exact decimal revision string from JSON.
 *
 * @details
 * Requires the revision member be a JSON string of digits and fit uint64; no floating-point
 * counter conversion.
 *
 * @param object Read-only request object.
 * @param revision Receives revision on success.
 *
 * @return Nonzero if parsed, zero otherwise.
 */
static int ParseRevision(cJSON *object, uint64_t *revision)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(object, "revision");
    char *end;
    unsigned long long value;
    const char *p;
    if (!cJSON_IsString(item) || item->valuestring[0] == '\0') {
        return 0;
    }
    for (p = item->valuestring; *p; ++p) {
        if (*p < '0' || *p > '9') {
            return 0;
        }
    }
    errno = 0;
    value = strtoull(item->valuestring, &end, 10);
    if (errno != 0 || *end != 0 || value > UINT64_MAX) {
        return 0;
    }
    *revision = (uint64_t)value;
    return 1;
}

/**
 * @brief Check an immediate downloadable capture filename.
 *
 * @details
 * Permits safe filename characters and known raw/index/metadata suffixes; rejects traversal
 * and leading dots.
 *
 * @param name NUL-terminated immediate filename.
 *
 * @return Nonzero when accepted.
 */
static int FileNameValid(const char *name)
{
    size_t k;
    size_t n = strlen(name);
    if (n == 0 || n > 240 || strstr(name, "..") != NULL) {
        return 0;
    }
    for (k = 0; k < n; ++k) {
        unsigned char ch = (unsigned char)name[k];
        if ((((((isalnum((unsigned char)ch) != 0)) || (ch == '_')) || (ch == '-')) ||
             (ch == '.')) == 0) {
            return 0;
        }
    }
    return (n > 4 && strcmp(name + n - 4, ".cs8") == 0) ||
           (n > 9 && strcmp(name + n - 9, ".meta.ini") == 0) ||
           (n > 10 && strcmp(name + n - 10, ".index.csv") == 0);
}

/**
 * @brief Decide whether a listed capture belongs to an active writer.
 *
 * @details
 * Uses the unique session prefix; while recording starts, treats all files conservatively as
 * active. Only an advisory snapshot; deletion rechecks in controller.
 *
 * @param status Copied controller status.
 * @param name NUL-terminated capture filename.
 *
 * @return Nonzero if active, zero if closed.
 */
static int FileActive(const TNgControlStatus *status, const char *name)
{
    size_t n = strlen(status->recordingName);
    if (status->recordingState == NG_REC_STARTING) {
        return 1;
    }
    return status->recordingState != NG_REC_OFF && n > 0 &&
           strncmp(name, status->recordingName, n) == 0 && (name[n] == '_' || name[n] == '.');
}

/** @brief Request-local bounded capture-file listing and copied lifecycle status. */
typedef struct {
    /** Request-owned JSON file array; no live file handle is retained. */
    cJSON *files;
    /** Owned copied lifecycle snapshot protected by the controller mutex. */
    TNgControlStatus status;
    /** Number of file entries added to the bounded response. */
    size_t count;
    /** Nonzero when the directory listing exceeded response capacity. */
    int truncated;
} TNgFileList;

/**
 * @brief Append a capture-directory entry to the bounded JSON file list.
 *
 * @details
 * Visitor ignores unsupported names and caps listing at 200 files, marking truncation. Copies
 * filename and marks closed/active according to status snapshot.
 *
 * @param context Owned file-list accumulator.
 * @param name Borrowed immediate filename.
 * @param bytes File length in bytes.
 */
static void AddFile(void *context, const char *name, uint64_t bytes)
{
    TNgFileList *list = context;
    cJSON *entry;
    if (FileNameValid(name) == 0) {
        return;
    }
    if (list->count >= 200) {
        list->truncated = 1;
        return;
    }
    ++list->count;
    entry = cJSON_CreateObject();
    String(entry, "name", name);
    Counter(entry, "bytes", bytes);
    cJSON_AddBoolToObject(entry, "closed", !FileActive(&list->status, name));
    cJSON_AddItemToArray(list->files, entry);
}

/** @brief Check an absolute file against both active capture/replay sessions.
 * @details Uses copied status/profile snapshots; no sample-thread work is introduced.
 * @param server Live server containing both independent controllers.
 * @param full Resolved absolute candidate path.
 * @param includeReaders Nonzero also protects inputs of running replay sessions.
 * @return Nonzero if a receiver currently owns the file for writing or selected reading.
 */
static int FileInUse(TNgWebServer *server, const char *full, int includeReaders)
{
    TNgControl *controllers[2] = {server->control, server->replayControl};
    size_t index;
    for (index = 0; index < 2; ++index) {
        TNgControlStatus status;
        TNgConfig config = {0};
        uint64_t revision;
        char directory[512];
        char candidate[512];
        const char *name = full, *cursor;
        NgControlSnapshot(controllers[index], &status);
        for (cursor = full; *cursor != '\0'; ++cursor) {
            if (*cursor == '/' || *cursor == '\\') {
                name = cursor + 1;
            }
        }
        if (name != full && (size_t)(name - full) < sizeof(directory)) {
            size_t length = (size_t)(name - full - 1);
            if (length == 0 || (length == 2 && full[1] == ':')) {
                ++length;
            }
            (void)snprintf(directory, sizeof(directory), "%.*s", (int)length, full);
            if (NgPathEqual(directory, status.outputDirectory) != 0 &&
                FileActive(&status, name) != 0) {
                return 1;
            }
        }
        if (includeReaders != 0 && status.state != NG_STOPPED && status.state != NG_FAILED) {
            NgControlConfig(controllers[index], &config, &revision);
            if (config.frontend == NG_FRONTEND_FILE &&
                NgFileAbsolute(config.replayPath, candidate, sizeof(candidate)) == 0 &&
                NgPathEqual(full, candidate) != 0) {
                return 1;
            }
        }
    }
    return 0;
}

/** @brief Serialize lifecycle requests with the cross-session deletion guard.
 * @details Only HTTP request threads acquire this mutex; sample producers never wait on it.
 * @param server Live server owning the guard.
 * @param control Selected receiver controller.
 * @param command Requested lifecycle action.
 * @param revision Expected profile revision.
 * @param id Receives accepted command identifier.
 * @param error Optional command diagnostic.
 * @return NG_OK if accepted; controller error otherwise.
 */
static int GuardedCommand(TNgWebServer *server, TNgControl *control, TNgCommand command,
                          uint64_t revision, uint64_t *id, TNgError *error)
{
    int result;
    NgMutexLock(&server->commandMutex);
    result = NgControlRequest(control, command, revision, id, error);
    NgMutexUnlock(&server->commandMutex);
    return result;
}

/** @brief Bounded request-owned file-picker accumulator; no borrowed names are retained. */
typedef struct {
    /** Borrowed server, retained for the whole synchronous request. */
    TNgWebServer *server;
    /** Owned JSON array of immediate entries. */
    cJSON *entries;
    /** Resolved current directory; empty for the OS root list. */
    char directory[512];
    /** Optional filename filter applied before the response limit. */
    char nameFilter[240];
    /** Entries emitted into this bounded response. */
    size_t count;
    /** Nonzero when additional entries were omitted. */
    int truncated;
} TNgPickerList;

/** @brief Match filename text with case-insensitive ASCII comparison.
 * @param name Borrowed filename.
 * @param filter Borrowed optional search text; empty matches every filename.
 * @return Nonzero when the filter occurs in name.
 */
static int NameContains(const char *name, const char *filter)
{
    size_t length = strlen(filter);
    for (;;) {
        size_t index = 0;
        while (index < length && name[index] != '\0' &&
               tolower((unsigned char)name[index]) == tolower((unsigned char)filter[index])) {
            ++index;
        }
        if (index == length) {
            return 1;
        }
        if (*name == '\0') {
            return 0;
        }
        ++name;
    }
}

/** @brief Append one directory or supported CI8 file to the picker.
 * @param context Request-owned listing.
 * @param name Borrowed immediate entry name or absolute root.
 * @param bytes File length in bytes.
 * @param directory Nonzero for a directory/root.
 */
static void AddPickerEntry(void *context, const char *name, uint64_t bytes, int directory)
{
    TNgPickerList *list = context;
    cJSON *entry;
    char full[512];
    char absolute[512];
    size_t length = strlen(name);
    int usable = 1;
    if (directory == 0 && (length < 4 || strcmp(name + length - 4, ".cs8") != 0 ||
                           NameContains(name, list->nameFilter) == 0)) {
        return;
    }
    if (list->count == 500) {
        list->truncated = 1;
        return;
    }
    if (directory == 0) {
        usable = bytes > 0 && bytes % sizeof(TNgIq8) == 0 &&
                 snprintf(full, sizeof(full), "%s/%s", list->directory, name) < (int)sizeof(full) &&
                 NgFileAbsolute(full, absolute, sizeof(absolute)) == 0 &&
                 FileInUse(list->server, absolute, 0) == 0;
    }
    entry = cJSON_CreateObject();
    String(entry, "name", name);
    Counter(entry, "bytes", bytes);
    cJSON_AddBoolToObject(entry, "directory", directory);
    cJSON_AddBoolToObject(entry, "selectable", usable);
    cJSON_AddItemToArray(list->entries, entry);
    ++list->count;
}

/** @brief Return a read-only directory listing on the receiver computer.
 * @param server Borrowed local server.
 * @param connection Borrowed HTTP connection.
 * @param body Parsed request with an optional directory path.
 * @return HTTP response status.
 */
static int PickerBrowse(TNgWebServer *server, struct mg_connection *connection, cJSON *body)
{
    TNgPickerList list = {0};
    TNgControlStatus status;
    cJSON *requested = cJSON_GetObjectItemCaseSensitive(body, "path");
    cJSON *filter = cJSON_GetObjectItemCaseSensitive(body, "filter");
    cJSON *reply;
    char parent[512] = "";
    const char *path;
    list.server = server;
    NgControlSnapshot(server->control, &status);
    if (requested != NULL && cJSON_IsString(requested) == 0) {
        return Failure(connection, 400, "Expected a receiver-side directory path");
    }
    if (filter != NULL) {
        if (cJSON_IsString(filter) == 0 || strlen(filter->valuestring) >= sizeof(list.nameFilter)) {
            return Failure(connection, 400, "Invalid filename filter");
        }
        (void)snprintf(list.nameFilter, sizeof(list.nameFilter), "%s", filter->valuestring);
    }
    path = requested != NULL ? requested->valuestring : status.outputDirectory;
    if (strlen(path) >= sizeof(list.directory) || strchr(path, '\n') != NULL ||
        strchr(path, '\r') != NULL) {
        return Failure(connection, 400, "Invalid or too long directory path");
    }
    list.entries = cJSON_CreateArray();
    if (*path == '\0') {
        NgDirectoryRoots(AddPickerEntry, &list);
    } else {
        char *separator;
        if (NgDirectoryResolve(path, list.directory, sizeof(list.directory)) != 0 ||
            NgDirectoryBrowse(list.directory, AddPickerEntry, &list) != 0) {
            cJSON_Delete(list.entries);
            return Failure(connection, 400, "Cannot browse this receiver-side directory");
        }
        (void)snprintf(parent, sizeof(parent), "%s", list.directory);
        for (separator = parent; *separator != '\0'; ++separator) {
            if (*separator == '\\') {
                *separator = '/';
            }
        }
        separator = strrchr(parent, '/');
        if (separator == NULL) {
            parent[0] = '\0';
        } else if (separator == parent + 2 && parent[1] == ':') {
            if (separator[1] == '\0') {
                parent[0] = '\0';
            } else {
                separator[1] = '\0';
            }
        } else if (separator == parent) {
            parent[1] = '\0';
            if (strcmp(list.directory, "/") == 0) {
                parent[0] = '\0';
            }
        } else {
            *separator = '\0';
        }
    }
    reply = cJSON_CreateObject();
    String(reply, "directory", list.directory);
    String(reply, "parent", parent);
    cJSON_AddItemToObject(reply, "entries", list.entries);
    cJSON_AddBoolToObject(reply, "truncated", list.truncated);
    return JsonReply(connection, 200, reply);
}

/** @brief Inspect a selection and return a replay draft without modifying a controller.
 * @param server Borrowed server containing the producer sessions.
 * @param control Selected stopped replay/primary controller.
 * @param connection Borrowed HTTP connection.
 * @param body Parsed request containing a receiver-side file path.
 * @return HTTP response status.
 */
static int PickerInspect(TNgWebServer *server, TNgControl *control,
                         struct mg_connection *connection, cJSON *body)
{
    cJSON *selected = cJSON_GetObjectItemCaseSensitive(body, "path");
    cJSON *reply;
    TNgConfig config = {0};
    TNgCaptureMetadata metadata = {0};
    TNgControlStatus status;
    TNgError error = {{0}};
    uint64_t revision;
    char full[512];
    char ini[12000];
    if (cJSON_IsString(selected) == 0 ||
        NgFileAbsolute(selected->valuestring, full, sizeof(full)) != 0) {
        return Failure(connection, 400, "Select an existing regular file on the receiver computer");
    }
    NgControlSnapshot(control, &status);
    if ((status.state != NG_STOPPED && status.state != NG_FAILED) ||
        FileInUse(server, full, 0) != 0) {
        return Failure(connection, 409,
                       "Stop the selected receiver or the file's writer before selection");
    }
    NgControlConfig(control, &config, &revision);
    if (NgReplayConfigure(full, &config, &metadata, &error) != NG_OK) {
        return Failure(connection, 400, error.message);
    }
    if (ConfigText(&config, ini, sizeof(ini)) != NG_OK) {
        return Failure(connection, 500, "Cannot prepare replay profile");
    }
    reply = cJSON_CreateObject();
    String(reply, "ini", ini);
    String(reply, "path", full);
    Counter(reply, "revision", revision);
    Counter(reply, "samples", metadata.samples);
    cJSON_AddBoolToObject(reply, "metadata_available", metadata.available);
    String(reply, "metadata_path", metadata.metadataPath);
    cJSON_AddBoolToObject(reply, "complete", metadata.complete);
    cJSON_AddBoolToObject(reply, "has_gaps", metadata.hasGaps);
    String(reply, "recording_point",
           metadata.preConditioning != 0 ? "pre_conditioning" : "post_conditioning");
    String(reply, "recorded_conditioning", metadata.conditioningName);
    String(reply, "sample_rate_basis", "configured_nominal");
    return JsonReply(connection, 200, reply);
}

/**
 * @brief Route local REST/static-file requests to the receiver controller.
 *
 * @details
 * Checks origin/token, body bounds and duplicate fields. File mutations are revision guarded
 * and do not enter RF callbacks. Sends responses and disposes request JSON before return.
 *
 * @param connection Borrowed HTTP connection.
 * @param context Live web server callback context.
 *
 * @return HTTP status handled, following CivetWeb callback convention.
 */
static int Handle(struct mg_connection *connection, void *context)
{
    TNgWebServer *server = context;
    const struct mg_request_info *request = mg_get_request_info(connection);
    const char *path = request->local_uri;
    TNgControl *control = server->control;
    char scopedPath[1024];
    int replay = 0;
    if (strncmp(path, "/api/v1/replay/", 15) == 0) {
        if (snprintf(scopedPath, sizeof(scopedPath), "/api/v1/%s", path + 15) >=
            (int)sizeof(scopedPath)) {
            return Failure(connection, 400, "Replay endpoint is too long");
        }
        path = scopedPath;
        control = server->replayControl;
        replay = 1;
    }
    cJSON *body = NULL;
    cJSON *reply;
    int code = 200;
    if (AllowedRequest(server, connection, 0) == 0) {
        return Failure(connection, 403, "Only same-origin local requests are accepted");
    }
    if (strcmp(request->request_method, "GET") == 0) {
        if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
            Headers(connection, 200, "text/html; charset=utf-8", ngIndexHtmlLength);
            mg_write(connection, ngIndexHtml, ngIndexHtmlLength);
            return 200;
        }
        if (strcmp(path, "/app.js") == 0) {
            Headers(connection, 200, "text/javascript; charset=utf-8", ngAppJsLength);
            mg_write(connection, ngAppJs, ngAppJsLength);
            return 200;
        }
        if (strcmp(path, "/style.css") == 0) {
            Headers(connection, 200, "text/css; charset=utf-8", ngStyleCssLength);
            mg_write(connection, ngStyleCss, ngStyleCssLength);
            return 200;
        }
        if (strcmp(path, "/api/v1/bootstrap") == 0) {
            reply = cJSON_CreateObject();
            String(reply, "token", server->token);
            String(reply, "version", NGENE_VERSION);
            String(reply, "os", NGENE_OS);
            String(reply, "architecture", NGENE_ARCH);
            cJSON_AddBoolToObject(reply, "hackrf", NgHackrfAvailable());
            cJSON_AddBoolToObject(reply, "periodogram", 1);
            String(reply, "fft_backend", NgFftBackend());
            return JsonReply(connection, 200, reply);
        }
        if (strcmp(path, "/api/v1/status") == 0) {
            return JsonReply(connection, 200, StatusJson(control));
        }
        if (strcmp(path, "/api/v1/analysis/config") == 0) {
            return AnalysisConfigReply(control, connection);
        }
        if (strcmp(path, "/api/v1/config") == 0) {
            return ConfigReply(control, connection);
        }
        if (strcmp(path, "/api/v1/files") == 0) {
            TNgFileList list;
            memset(&list, 0, sizeof(list));
            list.files = cJSON_CreateArray();
            NgControlSnapshot(control, &list.status);
            if (NgDirectoryFiles(list.status.outputDirectory, AddFile, &list) != 0) {
                cJSON_Delete(list.files);
                return Failure(connection, 500, "Cannot read capture directory");
            }
            reply = cJSON_CreateObject();
            cJSON_AddItemToObject(reply, "files", list.files);
            Counter(reply, "revision", list.status.revision);
            String(reply, "directory", list.status.outputDirectory);
            cJSON_AddBoolToObject(reply, "truncated", list.truncated);
            return JsonReply(connection, 200, reply);
        }
        if (strncmp(path, "/api/v1/files/", 14) == 0) {
            TNgControlStatus status;
            char full[1024];
            char extra[512];
            uint64_t bytes;
            const char *name = path + 14;
            if (FileNameValid(name) == 0) {
                return Failure(connection, 400, "Invalid recording filename");
            }
            NgControlSnapshot(control, &status);
            if (FileActive(&status, name) != 0) {
                return Failure(connection, 409, "Stop recording before downloading its files");
            }
            snprintf(full, sizeof(full), "%s/%s", status.outputDirectory, name);
            if (NgRegularFile(full, &bytes) != 0) {
                return Failure(connection, 404, "Recording file not found");
            }
            snprintf(extra, sizeof(extra),
                     "Content-Disposition: attachment; filename=\"%s\"\r\nCache-Control: "
                     "no-store\r\nX-Content-Type-Options: nosniff\r\n",
                     name);
            mg_send_mime_file2(connection, full, "application/octet-stream", extra);
            return 200;
        }
        return Failure(connection, 404, "Not found");
    }
    if (strcmp(request->request_method, "POST") != 0) {
        return Failure(connection, 405, "Use GET or POST");
    }
    {
        const char *token = mg_get_header(connection, "X-NGene-Token");
        const char *type = mg_get_header(connection, "Content-Type");
        char data[MAX_BODY + 1];
        size_t received = 0;
        const char *parseEnd = NULL;
        if (token == NULL || strcmp(token, server->token) != 0) {
            return Failure(connection, 403, "Missing or invalid session token");
        }
        if (NgAtomicGet(&server->quit) != 0) {
            return Failure(connection, 409, "Service is shutting down");
        }
        if (type == NULL || strncmp(type, "application/json", 16) != 0) {
            return Failure(connection, 415, "Expected application/json");
        }
        if (request->content_length <= 0 || request->content_length > MAX_BODY) {
            return Failure(connection, 413, "JSON body must be 1..16384 bytes with Content-Length");
        }
        while (received < (size_t)request->content_length) {
            int n =
                mg_read(connection, data + received, (size_t)request->content_length - received);
            if (n <= 0) {
                return Failure(connection, 400, "Incomplete request body");
            }
            received += (size_t)n;
        }
        data[received] = '\0';
        if (strlen(data) != received || strstr(data, "\\u0000") != NULL) {
            return Failure(connection, 400, "NUL byte in request body");
        }
        NgMutexLock(&server->jsonMutex);
        body = cJSON_ParseWithLengthOpts(data, received + 1, &parseEnd, 1);
        NgMutexUnlock(&server->jsonMutex);
        if (cJSON_IsObject(body) == 0) {
            cJSON_Delete(body);
            return Failure(connection, 400, "Malformed JSON object");
        }
        /* Duplicate JSON fields could otherwise select different values in clients and server. */
        {
            cJSON *a;
            cJSON *b;
            for (a = body->child; a != NULL; a = a->next) {
                for (b = a->next; b != NULL; b = b->next) {
                    if (strcmp(a->string, b->string) == 0) {
                        cJSON_Delete(body);
                        return Failure(connection, 400, "Duplicate JSON field");
                    }
                }
            }
        }
    }
    if (strcmp(path, "/api/v1/replay-files/browse") == 0) {
        code = PickerBrowse(server, connection, body);
    } else if (strcmp(path, "/api/v1/replay-files/inspect") == 0) {
        code = PickerInspect(server, control, connection, body);
    } else if (strcmp(path, "/api/v1/files/delete") == 0) {
        cJSON *name = cJSON_GetObjectItemCaseSensitive(body, "name");
        uint64_t revision;
        TNgError error = {{0}};
        if (cJSON_IsString(name) == 0 || ParseRevision(body, &revision) == 0) {
            code = Failure(connection, 400, "Expected name and revision strings");
        } else {
            TNgControlStatus status;
            char candidate[1024];
            char absolute[512];
            int result;
            NgMutexLock(&server->commandMutex);
            NgControlSnapshot(control, &status);
            (void)snprintf(candidate, sizeof(candidate), "%s/%s", status.outputDirectory,
                           name->valuestring);
            if (FileNameValid(name->valuestring) != 0 &&
                NgFileAbsolute(candidate, absolute, sizeof(absolute)) == 0 &&
                FileInUse(server, absolute, 1) != 0) {
                NgMutexUnlock(&server->commandMutex);
                code = Failure(connection, 409, "File is in use by a capture or replay session");
                cJSON_Delete(body);
                return code;
            }
            result = NgControlDeleteFile(control, name->valuestring, revision, &error);
            NgMutexUnlock(&server->commandMutex);
            if (result != NG_OK) {
                code = Failure(connection,
                               result == NG_FULL  ? 409
                               : result == NG_END ? 404
                                                  : 400,
                               error.message);
            } else {
                reply = cJSON_CreateObject();
                cJSON_AddBoolToObject(reply, "deleted", 1);
                String(reply, "name", name->valuestring);
                code = JsonReply(connection, 200, reply);
            }
        }
    } else if (strcmp(path, "/api/v1/analysis/config") == 0) {
        TNgAnalysisConfig config;
        uint64_t revision;
        TNgError error = {{0}};
        if (ParseRevision(body, &revision) == 0 ||
            ReadAnalysisConfig(body, &config, &error) != NG_OK) {
            code = Failure(connection, 400,
                           (error.message[0]) != 0 ? error.message : "Expected revision");
        } else if (NgControlAnalysisConfigure(control, &config, revision, &error) != NG_OK) {
            code = Failure(connection, 409, error.message);
        } else {
            code = AnalysisConfigReply(control, connection);
        }
    } else if (strcmp(path, "/api/v1/config") == 0 ||
               strcmp(path, "/api/v1/config/validate") == 0) {
        TNgConfig config = {0};
        TNgError error = {{0}};
        uint64_t revision;
        FILE *file;
        cJSON *ini = cJSON_GetObjectItemCaseSensitive(body, "ini");
        if (cJSON_IsString(ini) == 0 || ParseRevision(body, &revision) == 0) {
            code = Failure(connection, 400, "Expected ini and revision strings");
        } else if ((file = tmpfile()) == NULL) {
            code = Failure(connection, 500, "Cannot allocate configuration stream");
        } else {
            int result = NG_ERROR;
            if (fputs(ini->valuestring, file) == EOF || fflush(file) != 0 ||
                fseek(file, 0, SEEK_SET) != 0) {
                /* Keep failure explicit: an I/O error must never apply an unparsed config. */
                (void)NgFail(&error, "Cannot prepare configuration stream");
            } else {
                result = NgConfigRead(file, &config, &error);
            }
            fclose(file);
            if (result == NG_OK && replay != 0 && config.frontend != NG_FRONTEND_FILE) {
                result = NgFail(&error, "Replay session accepts recorded files only");
            }
            if (result == NG_OK) {
                result = NgControlValidate(&config, &error);
            }
            if (result != NG_OK) {
                code = Failure(connection, 400, error.message);
            } else if (strcmp(path, "/api/v1/config/validate") == 0) {
                reply = cJSON_CreateObject();
                cJSON_AddBoolToObject(reply, "valid", 1);
                code = JsonReply(connection, 200, reply);
            } else if (NgControlConfigure(control, &config, revision, &error) != NG_OK) {
                code = Failure(connection, 409, error.message);
            } else {
                code = ConfigReply(control, connection);
            }
        }
    } else if (strcmp(path, "/api/v1/shutdown") == 0) {
        reply = cJSON_CreateObject();
        cJSON_AddBoolToObject(reply, "accepted", 1);
        code = JsonReply(connection, 202, reply);
        NgAtomicSet(&server->quit, 1);
    } else {
        TNgCommand command = 0;
        uint64_t revision;
        uint64_t id;
        TNgError error = {{0}};
        if (strcmp(path, "/api/v1/receiver/start") == 0) {
            command = NG_CMD_START;
        }
        if (strcmp(path, "/api/v1/receiver/stop") == 0) {
            command = NG_CMD_STOP;
        }
        if (strcmp(path, "/api/v1/recording/start") == 0) {
            command = NG_CMD_RECORD_START;
        }
        if (strcmp(path, "/api/v1/recording/stop") == 0) {
            command = NG_CMD_RECORD_STOP;
        }
        if (command == 0) {
            code = Failure(connection, 404, "Unknown command");
        } else if (ParseRevision(body, &revision) == 0) {
            code = Failure(connection, 400, "Expected revision string");
        } else if (GuardedCommand(server, control, command, revision, &id, &error) != NG_OK) {
            code = Failure(connection, 409, error.message);
        } else {
            reply = cJSON_CreateObject();
            cJSON_AddBoolToObject(reply, "accepted", 1);
            Counter(reply, "command_id", id);
            code = JsonReply(connection, 202, reply);
        }
    }
    cJSON_Delete(body);
    return code;
}

/** @brief Per-connection rate state; no PSD or IQ buffers are retained. */
typedef struct {
    /** Monotonic time of the most recent response on this connection. */
    double lastReply;
} TNgWebSession;

/**
 * @brief Authorize a WebSocket and allocate its bounded session state.
 *
 * @details
 * Checks same origin and session token; enforces client limit under JSON mutex. The close
 * callback owns releasing a successful session.
 *
 * @param connection Borrowed connection with handshake headers.
 * @param context Live web server callback context.
 *
 * @return Zero to accept, nonzero to reject the handshake.
 */
static int WsConnect(const struct mg_connection *connection, void *context)
{
    TNgWebServer *server = context;
    char token[80];
    const char *query = mg_get_request_info(connection)->query_string;
    TNgWebSession *session;
    if (AllowedRequest(server, connection, 1) == 0 || query == NULL ||
        mg_get_var(query, strlen(query), "token", token, sizeof(token)) < 0 ||
        strcmp(token, server->token) != 0) {
        return 1;
    }
    session = calloc(1, sizeof(*session));
    if (session == NULL) {
        return 1;
    }
    NgMutexLock(&server->jsonMutex);
    if (server->websocketClients >= 3) {
        NgMutexUnlock(&server->jsonMutex);
        free(session);
        return 1;
    }
    ++server->websocketClients;
    NgMutexUnlock(&server->jsonMutex);
    mg_set_user_connection_data((struct mg_connection *)connection, session);
    return 0;
}

/**
 * @brief Release WebSocket session state and client count.
 *
 * @details
 * Runs on CivetWeb close; never accesses a frontend sample buffer. Server remains valid until
 * web threads stop.
 *
 * @param connection Closing connection with owned session state.
 * @param context Live web server callback context.
 */
static void WsClose(const struct mg_connection *connection, void *context)
{
    TNgWebServer *server = context;
    TNgWebSession *session = mg_get_user_connection_data(connection);
    if (session != NULL) {
        NgMutexLock(&server->jsonMutex);
        --server->websocketClients;
        NgMutexUnlock(&server->jsonMutex);
        free(session);
    }
}

/**
 * @brief Handle a bounded diagnostic-interest/status WebSocket request.
 *
 * @details
 * Accepts status or status:mask messages with paced polling, renews optional producer
 * interest, copies status before serialization and replies on this client thread. No raw
 * sample transmission.
 *
 * @param connection Borrowed WebSocket connection.
 * @param bits Frame opcode/flags supplied by CivetWeb.
 * @param data Borrowed payload bytes.
 * @param length Payload length in bytes.
 * @param context Live web server callback context.
 *
 * @return Nonzero to retain the connection; zero to close for invalid/abusive messages.
 */
static int WsData(struct mg_connection *connection, int bits, char *data, size_t length,
                  void *context)
{
    TNgWebServer *server = context;
    cJSON *object;
    char *text;
    if (((bits) & (15)) == 8) {
        return 0;
    }
    if (((bits) & (15)) != 1) {
        return 1;
    }
    if (((bits) & (128)) == 0 || (length != 6 && length != 8) ||
        (memcmp(data, "status", 6) != 0 && memcmp(data, "replay", 6) != 0) ||
        (length == 8 && (data[6] != ':' || data[7] < '0' || data[7] > '3'))) {
        return 0;
    }
    {
        TNgWebSession *session = mg_get_user_connection_data(connection);
        double now = NgMonotonicSeconds();
        if (session == NULL) {
            return 0;
        }
        if (now - session->lastReply < 0.02) {
            return 0; /* Close abusive pacing, never leave an unanswered request. */
        }
        session->lastReply = now;
    }
    TNgControl *control = memcmp(data, "replay", 6) == 0 ? server->replayControl : server->control;
    NgControlDiagnosticInterest(control, length == 8 ? (unsigned)(data[7] - '0') : 3u);
    object = StatusJson(control);
    text = (object) != NULL ? cJSON_PrintUnformatted(object) : NULL;
    if (text != NULL) {
        mg_websocket_write(connection, MG_WEBSOCKET_OPCODE_TEXT, text, strlen(text));
    }
    cJSON_free(text);
    cJSON_Delete(object);
    return !NgAtomicGet(&server->quit);
}

/**
 * @brief Serve the embedded console and controller on loopback until shutdown.
 *
 * @details
 * Binds to local interfaces only. REST mutations require the session token and same-origin
 * checks; WebSocket status copies precede serialization. Owns the controller and server for
 * this blocking call, then stops/joins them. Remote use requires the documented forwarding
 * arrangement.
 *
 * Parameter config: Initial receiver configuration.
 * Parameter port: Local TCP port 1024..65535.
 * Parameter outputDirectory: Receiver profile/output outputDirectory; parents must exist.
 * Parameter stop: Optional stop predicate, also polled by the service.
 * Parameter stopContext: Context passed to stop.
 * Parameter error: Optional service/startup error destination.
 *
 * Result: NG_OK on clean shutdown; NG_ERROR on configuration, bind, thread or startup
 * failures.
 */
int NgWebRun(const TNgConfig *config, unsigned port, const char *outputDirectory,
             TNgStopRequested stop, void *stopContext, TNgError *error)
{
    TNgWebServer server;
    unsigned char random[32];
    size_t k;
    char listening[64];
    const char *options[] = {"listening_ports",
                             listening,
                             "num_threads",
                             "6",
                             "request_timeout_ms",
                             "2000",
                             "websocket_timeout_ms",
                             "5000",
                             "enable_directory_listing",
                             "no",
                             "enable_keep_alive",
                             "no",
                             "max_request_size",
                             "8192",
                             NULL};
    memset(&server, 0, sizeof(server));
    if (port < 1024 || port > 65535) {
        return NgFail(error, "Web port must be 1024..65535");
    }
    if (NgRandomBytes(random, sizeof(random)) != 0 || NgMutexInit(&server.jsonMutex) != 0) {
        return NgFail(error, "Cannot initialize web session");
    }
    if (NgMutexInit(&server.commandMutex) != 0) {
        NgMutexDestroy(&server.jsonMutex);
        return NgFail(error, "Cannot initialize file ownership guard");
    }
    for (k = 0; k < sizeof(random); ++k) {
        snprintf(server.token + 2 * k, 3, "%02x", random[k]);
    }
    if (NgControlCreate(config, outputDirectory, &server.control, error) != NG_OK) {
        NgMutexDestroy(&server.commandMutex);
        NgMutexDestroy(&server.jsonMutex);
        return NG_ERROR;
    }
    {
        TNgConfig replayConfig = *config;
        char replayDirectory[1024];
        char replayProfile[1200];
        FILE *saved;
        replayConfig.frontend = NG_FRONTEND_FILE;
        replayConfig.runSeconds = 0;
        replayConfig.recordingEnabled = 0;
        replayConfig.recordingDirectory[0] = '\0';
        replayConfig.replayPath[0] = '\0';
        if (snprintf(replayDirectory, sizeof(replayDirectory), "%s/replay", outputDirectory) >=
                (int)sizeof(replayDirectory) ||
            snprintf(replayProfile, sizeof(replayProfile), "%s/receiver.ini", replayDirectory) >=
                (int)sizeof(replayProfile)) {
            NgControlDestroy(server.control);
            NgMutexDestroy(&server.commandMutex);
            NgMutexDestroy(&server.jsonMutex);
            return NgFail(error, "Replay profile path is too long");
        }
        saved = fopen(replayProfile, "rb");
        if (saved != NULL) {
            int result = NgConfigRead(saved, &replayConfig, error);
            if (fclose(saved) != 0 || result != NG_OK ||
                replayConfig.frontend != NG_FRONTEND_FILE) {
                NgControlDestroy(server.control);
                NgMutexDestroy(&server.commandMutex);
                NgMutexDestroy(&server.jsonMutex);
                return NgFail(error, "Cannot load the replay profile");
            }
        }
        if (NgControlCreate(&replayConfig, replayDirectory, &server.replayControl, error) !=
            NG_OK) {
            NgControlDestroy(server.control);
            NgMutexDestroy(&server.commandMutex);
            NgMutexDestroy(&server.jsonMutex);
            return NG_ERROR;
        }
    }
    snprintf(server.host, sizeof(server.host), "127.0.0.1:%u", port);
    snprintf(server.localhost, sizeof(server.localhost), "localhost:%u", port);
    snprintf(listening, sizeof(listening), "127.0.0.1:%u", port);
    mg_init_library(0);
    server.http = mg_start(NULL, &server, options);
    if (server.http == NULL) {
        NgControlDestroy(server.replayControl);
        NgControlDestroy(server.control);
        NgMutexDestroy(&server.commandMutex);
        NgMutexDestroy(&server.jsonMutex);
        mg_exit_library();
        return NgFail(error, "Cannot start local HTTP listener; port may be in use");
    }
    mg_set_websocket_handler(server.http, "/api/v1/events", WsConnect, NULL, WsData, WsClose,
                             &server);
    mg_set_request_handler(server.http, "/", Handle, &server);
    printf("NGene %s web console: http://%s\nLocal access only. Start/Stop receiver from the "
           "browser; Ctrl+C closes the service.\n",
           NGENE_VERSION, server.host);
    fflush(stdout);
    while (NgAtomicGet(&server.quit) == 0 && (stop != NULL && stop(stopContext) != 0) == 0) {
        NgSleepMs(50);
    }
    NgAtomicSet(&server.quit, 1);
    mg_stop(server.http);
    NgControlDestroy(server.replayControl);
    NgControlDestroy(server.control);
    mg_exit_library();
    NgMutexDestroy(&server.commandMutex);
    NgMutexDestroy(&server.jsonMutex);
    return NG_OK;
}
