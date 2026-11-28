/** @file config.c
 * @brief NGene config implementation.
 */
#include "ngene/config.h"
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/** @brief Strict boolean INI dispatch; returns immediately after a matched key. */
#define BOOL_KEY(name, field)                                                                      \
    if (strcmp(key, name) == 0) {                                                                  \
        return ParseBool(value, &c->field);                                                        \
    }
/** @brief Finite real INI dispatch; returns immediately after a matched key. */
#define REAL_KEY(name, field)                                                                      \
    if (strcmp(key, name) == 0) {                                                                  \
        return ParseReal(value, &c->field);                                                        \
    }
/** @brief Strict unsigned 64-bit INI dispatch; rejects malformed or overflowing values. */
#define U64_KEY(name, field)                                                                       \
    if (strcmp(key, name) == 0) {                                                                  \
        return ParseU64(value, &c->field);                                                         \
    }
/** @brief Unsigned integer INI dispatch; checks the destination range before narrowing. */
#define UINT_KEY(name, field)                                                                      \
    if (strcmp(key, name) == 0) {                                                                  \
        if (ParseU64(value, &n) != NG_OK || n > UINT32_MAX) {                                      \
            return NG_ERROR;                                                                       \
        }                                                                                          \
        c->field = (uint32_t)n;                                                                    \
        return NG_OK;                                                                              \
    }
/** @brief Sample-count INI dispatch; checks size_t range before narrowing. */
#define SIZE_KEY(name, field)                                                                      \
    if (strcmp(key, name) == 0) {                                                                  \
        if (ParseU64(value, &n) != NG_OK || n > SIZE_MAX) {                                        \
            return NG_ERROR;                                                                       \
        }                                                                                          \
        c->field = (size_t)n;                                                                      \
        return NG_OK;                                                                              \
    }

/**
 * @brief Initialize a complete receiver configuration with supported defaults.
 *
 * @details
 * Overwrites the structure, including diagnostics, frontend, buffers and recording settings.
 * It does not open hardware or create files.
 *
 * Parameter config: Writable configuration structure.
 */
void NgConfigDefaults(TNgConfig *config)
{
    memset(config, 0, sizeof(*config));
    config->frontend = NG_FRONTEND_FILE;
    NgAnalysisDefaults(&config->analysis);
    NgAcquisitionDefaults(&config->acquisition);
    NgTrackingDefaults(&config->tracking);
    /* Acquisition/tracking are intrinsic receiver stages; record-only skips channel work. */
    config->acquisition.enabled = 1;
    NgAcquisitionDefaults(&config->galileoAcquisition);
    config->galileoAcquisition.enabled = 1;
    NgAcquisitionDefaults(&config->beidouAcquisition);
    config->beidouAcquisition.lastPrn = NG_SATELLITE_MAX_PRN;
    config->rtlAutoGain = 1;
    config->rtlIfGainDb = -1;
    config->bladeGainDb = 30;
    config->bladeFormat = NG_FORMAT_SC16_Q11;
    config->replayFormat = NG_FORMAT_CI8;
    config->dcShift = NG_DEFAULT_DC_SHIFT;
    config->sampleRateHz = NG_DEFAULT_SAMPLE_RATE_HZ;
    config->centerFrequencyHz = NG_GNSS_L1_FREQUENCY_HZ;
    config->lnaGainDb = NG_DEFAULT_LNA_GAIN_DB;
    config->vgaGainDb = NG_DEFAULT_VGA_GAIN_DB;
    config->windowSamples = NG_DEFAULT_WINDOW_SAMPLES;
    config->ingressBlocks = NG_DEFAULT_INGRESS_BLOCKS;
    config->ringBlocks = NG_DEFAULT_RING_BLOCKS;
    config->overflowPolicy = NG_OVERFLOW_STOP;
    config->runSeconds = 2;
    config->idleTimeoutMs = NG_DEFAULT_IDLE_TIMEOUT_MS;
    config->recordingQueueBlocks = NG_DEFAULT_RECORDING_BLOCKS;
    (void)snprintf(config->recordingPrefix, sizeof(config->recordingPrefix), "ngene_capture");
    config->replayRealtime = 1;
}

/**
 * @brief Trim surrounding whitespace in place.
 *
 * @details
 * Returns a pointer inside the caller buffer and inserts a trailing NUL. No allocation; the
 * original buffer retains ownership.
 *
 * @param text Writable NUL-terminated text buffer.
 *
 * @return Pointer to the trimmed substring.
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

/**
 * @brief Parse an unsigned decimal INI value with strict syntax.
 *
 * @details
 * Accepts digits only, rejects empty strings, signs, overflow and trailing characters. Writes
 * the output only on success.
 *
 * @param text NUL-terminated decimal text.
 * @param value Receives parsed uint64 value on success.
 *
 * @return NG_OK on success; NG_ERROR for malformed or overflowing input.
 */
static int ParseU64(const char *text, uint64_t *value)
{
    char *end;
    unsigned long long result;
    const char *p;
    if (*text == '\0') {
        return NG_ERROR;
    }
    for (p = text; *p != '\0'; ++p) {
        if (*p < '0' || *p > '9') {
            return NG_ERROR;
        }
    }
    errno = 0;
    result = strtoull(text, &end, 10);
    if (errno != 0 || *end != '\0' || result > UINT64_MAX) {
        return NG_ERROR;
    }
    *value = (uint64_t)result;
    return NG_OK;
}

/**
 * @brief Parse a finite real INI value.
 *
 * @details
 * Checks conversion, errno, trailing characters and finiteness before publishing the parsed
 * value.
 *
 * @param text NUL-terminated number text.
 * @param value Receives finite double on success.
 *
 * @return NG_OK on success; NG_ERROR otherwise.
 */
static int ParseReal(const char *text, double *value)
{
    char *end;
    double result;
    errno = 0;
    result = strtod(text, &end);
    if (text == end || errno != 0 || *end != '\0' || !isfinite(result)) {
        return NG_ERROR;
    }
    *value = result;
    return NG_OK;
}

/**
 * @brief Parse the explicit INI boolean words true and false.
 *
 * @details
 * Only those lowercase spellings are accepted. No numerical or ambiguous aliases.
 *
 * @param text NUL-terminated boolean text.
 * @param value Receives 1 for true or 0 for false.
 *
 * @return NG_OK for true/false; NG_ERROR otherwise.
 */
static int ParseBool(const char *text, int *value)
{
    if (strcmp(text, "true") == 0) {
        *value = 1;
        return NG_OK;
    }
    if (strcmp(text, "false") == 0) {
        *value = 0;
        return NG_OK;
    }
    return NG_ERROR;
}

static int ParseSearchFft(const char *text, TNgSearchFftMode *mode)
{
    if (strcmp(text, "float") == 0) {
        *mode = NG_SEARCH_FFT_FLOAT;
    } else if (strcmp(text, "integer_precise") == 0) {
        *mode = NG_SEARCH_FFT_INTEGER_PRECISE;
    } else if (strcmp(text, "integer_scaled") == 0) {
        *mode = NG_SEARCH_FFT_INTEGER_SCALED;
    } else {
        return NG_ERROR;
    }
    return NG_OK;
}

static const char *SearchFftName(TNgSearchFftMode mode)
{
    return mode == NG_SEARCH_FFT_INTEGER_PRECISE  ? "integer_precise"
           : mode == NG_SEARCH_FFT_INTEGER_SCALED ? "integer_scaled"
                                                  : "float";
}

/**
 * @brief Copy a bounded single-line configuration string.
 *
 * @details
 * Rejects text too long for the destination or containing CR/LF. Copies the NUL terminator on
 * success.
 *
 * @param out Writable destination buffer.
 * @param capacity Capacity including terminator.
 * @param text Read-only NUL-terminated source.
 *
 * @return NG_OK on success; NG_ERROR otherwise.
 */
static int CopyText(char *out, size_t capacity, const char *text)
{
    if (strlen(text) >= capacity || strchr(text, '\n') != NULL || strchr(text, '\r') != NULL) {
        return NG_ERROR;
    }
    memcpy(out, text, strlen(text) + 1);
    return NG_OK;
}

/**
 * @brief Apply one qualified INI key to a draft configuration.
 *
 * @details
 * Maps section.key to the correct field and bounded parser. Unknown keys fail; final inter-
 * field validation is separate.
 *
 * @param c Draft configuration to update.
 * @param key Qualified key such as frontend.sampleRateHz.
 * @param value Value text without surrounding whitespace.
 *
 * @return NG_OK for an accepted value; NG_ERROR for unknown keys or invalid values.
 */
static const char *const loopSignals[] = {"gps", "galileo", "beidou", "egnos"};
static const char *const loopNames[] = {"dll", "fll", "pll"};

static int Apply(TNgConfig *c, const char *key, const char *value)
{
    uint64_t n;
    UINT_KEY("tracking.gps_channels", tracking.gpsChannels)
    UINT_KEY("tracking.galileo_channels", tracking.galileoChannels)
    UINT_KEY("tracking.workers", tracking.workers)
    UINT_KEY("tracking.doppler_smoothing_ms", tracking.dopplerSmoothingMs)
    if (strcmp(key, "receiver.egnos") == 0 || strcmp(key, "tracking.egnos") == 0) {
        return ParseBool(value, &c->tracking.egnosEnabled);
    }
    if (strcmp(key, "navigation.broadcast_ionosphere") == 0 ||
        strcmp(key, "tracking.broadcast_ionosphere") == 0) {
        return ParseBool(value, &c->tracking.broadcastIonosphere);
    }
    if (strcmp(key, "pvt.carrier_smoothing_length") == 0) {
        return ParseU64(value, &n) == NG_OK && n <= 1024 ? NG_OK : NG_ERROR;
    }
    if (strcmp(key, "tracking.enabled") == 0 || strcmp(key, "acquisition.enabled") == 0 ||
        strcmp(key, "galileo_acquisition.enabled") == 0) {
        int legacy;
        return ParseBool(value, &legacy); /* Accepted on import; stages remain active. */
    }
    UINT_KEY("receiver.channels", tracking.channelCount)
    UINT_KEY("receiver.gps_reserved", tracking.gpsChannels)
    UINT_KEY("receiver.galileo_reserved", tracking.galileoChannels)
    UINT_KEY("receiver.beidou_reserved", tracking.beidouChannels)
    UINT_KEY("receiver.auto_priority", tracking.autoPriority)
    REAL_KEY("tracking.galileo_spacing_chips", tracking.galileoSpacingChips)
    REAL_KEY("tracking.beidou_spacing_chips", tracking.beidouSpacingChips)
    REAL_KEY("tracking.egnos_spacing_chips", tracking.egnosSpacingChips)
    REAL_KEY("tracking.cn0_cutoff_db_hz", tracking.cn0CutoffDbHz)
    UINT_KEY("tracking.gps_coherent_ms", tracking.gpsCoherentMs)
    UINT_KEY("tracking.gps_transition_checks", tracking.gpsTransitionChecks)
    UINT_KEY("tracking.fll_decision_updates", tracking.fllDecisionUpdates)
    UINT_KEY("tracking.fll_decision_interval_ms", tracking.fllDecisionIntervalMs)
    REAL_KEY("tracking.fll_transition_hz", tracking.fllTransitionHz)
    if (strncmp(key, "tracking.", 9) == 0) {
        for (unsigned signal = 0; signal < NG_TRACKING_EGNOS_SIGNAL; ++signal) {
            for (unsigned loop = 0; loop < NG_LOOP_COUNT; ++loop) {
                char option[48];
                TNgLoopSettings *settings = &c->tracking.loops[signal][loop];
                snprintf(option, sizeof(option), "%s_%s_bandwidth_hz", loopSignals[signal],
                         loopNames[loop]);
                if (strcmp(key + 9, option) == 0) {
                    return ParseReal(value, &settings->bandwidthHz);
                }
                snprintf(option, sizeof(option), "%s_%s_gain", loopSignals[signal],
                         loopNames[loop]);
                if (strcmp(key + 9, option) == 0) {
                    return ParseReal(value, &settings->gain);
                }
            }
        }
    }
    if (strcmp(key, "tracking.galileo_spacing_slots") == 0) {
        double slots;
        if (ParseReal(value, &slots) != NG_OK) {
            return NG_ERROR;
        }
        c->tracking.galileoSpacingChips = slots * 0.5;
        return NG_OK;
    }
    BOOL_KEY("tracking.square_carrier", tracking.squareCarrier)
    BOOL_KEY("tracking.xor_correlation", tracking.xorCorrelation)
    BOOL_KEY("tracking.fast_carrier", tracking.fastCarrier)
    UINT_KEY("tracking.mixer_bits", tracking.mixerBits)
    BOOL_KEY("dsp.tracking_square_carrier", tracking.squareCarrier)
    BOOL_KEY("dsp.tracking_xor_correlation", tracking.xorCorrelation)
    BOOL_KEY("dsp.tracking_fast_carrier", tracking.fastCarrier)
    UINT_KEY("dsp.tracking_mixer_bits", tracking.mixerBits)
    UINT_KEY("tracking.tap_count", tracking.tapCount)
    REAL_KEY("tracking.spacing_chips", tracking.spacingChips)
    REAL_KEY("acquisition.pfa", acquisition.falseAlarmProbability)

    UINT_KEY("acquisition.first_prn", acquisition.firstPrn)
    UINT_KEY("acquisition.last_prn", acquisition.lastPrn)
    BOOL_KEY("acquisition.square_carrier", acquisition.squareCarrier)
    BOOL_KEY("acquisition.xor_correlation", acquisition.xorCorrelation)
    BOOL_KEY("dsp.gps_square_carrier", acquisition.squareCarrier)
    BOOL_KEY("dsp.gps_xor_correlation", acquisition.xorCorrelation)
    if (strcmp(key, "acquisition.cpu_budget") == 0) {
        /* One shared search pool serves both signals: one budget for both. */
        if (ParseReal(value, &c->acquisition.cpuBudget) != NG_OK) {
            return NG_ERROR;
        }
        c->galileoAcquisition.cpuBudget = c->acquisition.cpuBudget;
        return NG_OK;
    }
    if (strcmp(key, "beidou_acquisition.enabled") == 0) {
        int legacy;
        return ParseBool(value, &legacy);
    }
    UINT_KEY("beidou_acquisition.first_prn", beidouAcquisition.firstPrn)
    UINT_KEY("beidou_acquisition.last_prn", beidouAcquisition.lastPrn)
    REAL_KEY("galileo_acquisition.pfa", galileoAcquisition.falseAlarmProbability)

    UINT_KEY("galileo_acquisition.first_prn", galileoAcquisition.firstPrn)
    UINT_KEY("galileo_acquisition.last_prn", galileoAcquisition.lastPrn)
    UINT_KEY("galileo_acquisition.fft_size", galileoAcquisition.fftSize)
    BOOL_KEY("galileo_acquisition.square_carrier", galileoAcquisition.squareCarrier)
    BOOL_KEY("galileo_acquisition.xor_correlation", galileoAcquisition.xorCorrelation)
    BOOL_KEY("dsp.galileo_square_carrier", galileoAcquisition.squareCarrier)
    BOOL_KEY("dsp.galileo_xor_correlation", galileoAcquisition.xorCorrelation)
    if (strcmp(key, "dsp.gps_fft") == 0) {
        return ParseSearchFft(value, &c->acquisition.fftMode);
    }
    if (strcmp(key, "dsp.galileo_fft") == 0) {
        return ParseSearchFft(value, &c->galileoAcquisition.fftMode);
    }
    if (strcmp(key, "dsp.spectrum_fft") == 0) {
        return ParseSearchFft(value, &c->analysis.fftMode);
    }
    if (strcmp(key, "frontend.type") == 0) {
        if (strcmp(value, "hackrf") == 0) {
            c->frontend = NG_FRONTEND_HACKRF;
            return NG_OK;
        }
        if (strcmp(value, "file") == 0) {
            c->frontend = NG_FRONTEND_FILE;
            return NG_OK;
        }
        if (strcmp(value, "rtlsdr") == 0) {
            c->frontend = NG_FRONTEND_RTLSDR;
            return NG_OK;
        }
        if (strcmp(value, "bladerf") == 0) {
            c->frontend = NG_FRONTEND_BLADERF;
            return NG_OK;
        } else if (strcmp(value, "sige") == 0) {
            c->frontend = NG_FRONTEND_SIGE;
            return NG_OK;
        }
        return NG_ERROR;
    }
    if (strcmp(key, "frontend.serial") == 0) {
        return CopyText(c->serial, sizeof(c->serial), value);
    }
    UINT_KEY("frontend.sample_rate_hz", sampleRateHz)
    U64_KEY("frontend.center_frequency_hz", centerFrequencyHz)
    U64_KEY("frontend.signal_frequency_hz", signalFrequencyHz)
    if (strcmp(key, "conditioning.dc_shift") == 0) {
        return NG_OK;
    }
    if (strcmp(key, "frontend.stream_format") == 0) {
        if (strcmp(value, "auto") == 0) {
            c->streamFormat = 0;
        } else if (strcmp(value, "CI8") == 0) {
            c->streamFormat = NG_FORMAT_CI8;
        } else if (strcmp(value, "CI16") == 0) {
            c->streamFormat = NG_FORMAT_CI16;
        } else {
            return NG_ERROR;
        }
        return NG_OK;
    }
    UINT_KEY("hackrf.bandwidth_hz", bandwidthHz)
    UINT_KEY("hackrf.lna_gain_db", lnaGainDb)
    UINT_KEY("hackrf.vga_gain_db", vgaGainDb)
    BOOL_KEY("hackrf.rf_amp", rfAmp)
    BOOL_KEY("hackrf.antenna_power", antennaPower)
    BOOL_KEY("rtlsdr.auto_gain", rtlAutoGain)
    BOOL_KEY("rtlsdr.bias_tee", rtlBiasTee)
    BOOL_KEY("bladerf.bias_tee", bladeBiasTee)
    UINT_KEY("frontend.bandwidth_hz", bandwidthHz)
    if (strcmp(key, "rtlsdr.gain_tenths_db") == 0 || strcmp(key, "rtlsdr.ppm") == 0 ||
        strcmp(key, "bladerf.gain_db") == 0 || strcmp(key, "rtlsdr.if_gain_db") == 0) {
        double valueReal;
        if (ParseReal(value, &valueReal) != NG_OK || valueReal < -1000 || valueReal > 1000 ||
            valueReal != floor(valueReal)) {
            return NG_ERROR;
        }
        if (strcmp(key, "rtlsdr.ppm") == 0) {
            c->rtlPpm = (int)valueReal;
        } else if (strcmp(key, "bladerf.gain_db") == 0) {
            c->bladeGainDb = (int)valueReal;
        } else if (strcmp(key, "rtlsdr.if_gain_db") == 0) {
            c->rtlIfGainDb = (int)valueReal;
        } else {
            c->rtlGainTenthsDb = (int)valueReal;
        }
        return NG_OK;
    }
    if (strcmp(key, "bladerf.format") == 0) {
        if (strcmp(value, "CI8") == 0) {
            c->bladeFormat = NG_FORMAT_CI8;
        } else if (strcmp(value, "SC16_Q11") == 0) {
            c->bladeFormat = NG_FORMAT_SC16_Q11;
        } else {
            return NG_ERROR;
        }
        return NG_OK;
    }
    if (strcmp(key, "replay.format") == 0) {
        if (strcmp(value, "CI8") == 0) {
            c->replayFormat = NG_FORMAT_CI8;
        } else if (strcmp(value, "CU8") == 0) {
            c->replayFormat = NG_FORMAT_CU8;
        } else if (strcmp(value, "CI16") == 0) {
            c->replayFormat = NG_FORMAT_CI16;
        } else if (strcmp(value, "SC16_Q11") == 0) {
            c->replayFormat = NG_FORMAT_SC16_Q11;
        } else {
            return NG_ERROR;
        }
        return NG_OK;
    }
    if (strcmp(key, "conditioning.module") == 0) {
        return NG_OK; /* Retired; ignored on import. */
    }
    SIZE_KEY("buffer.window_samples", windowSamples)
    SIZE_KEY("buffer.ingress_blocks", ingressBlocks)
    SIZE_KEY("buffer.ring_blocks", ringBlocks)
    if (strcmp(key, "buffer.overflow_policy") == 0) {
        if (strcmp(value, "stop") == 0) {
            c->overflowPolicy = NG_OVERFLOW_STOP;
            return NG_OK;
        }
        if (strcmp(value, "drop_newest") == 0) {
            c->overflowPolicy = NG_OVERFLOW_DROP_NEWEST;
            return NG_OK;
        }
        return NG_ERROR;
    }
    REAL_KEY("receiver.run_seconds", runSeconds)
    UINT_KEY("receiver.idle_timeout_ms", idleTimeoutMs)
    if (strcmp(key, "receiver.mode") == 0) {
        if (strcmp(value, "capture") == 0) {
            c->recordOnly = 0;
            return NG_OK;
        }
        if (strcmp(value, "record") == 0) {
            c->recordOnly = 1;
            return NG_OK;
        }
        return NG_ERROR;
    }
    if (strcmp(key, "recording.point") == 0) {
        if (strcmp(value, "pre") == 0) {
            c->recordingPreConditioning = 1;
            return NG_OK;
        }
        if (strcmp(value, "post") == 0) {
            c->recordingPreConditioning = 0;
            return NG_OK;
        }
        return NG_ERROR;
    }
    BOOL_KEY("analysis.spectrum_enabled", analysis.spectrumEnabled)
    BOOL_KEY("analysis.histogram_enabled", analysis.histogramEnabled)
    SIZE_KEY("analysis.fft_size", analysis.fftSize)
    UINT_KEY("analysis.overlap_percent", analysis.overlapPercent)
    UINT_KEY("analysis.averages", analysis.averages)
    REAL_KEY("analysis.update_hz", analysis.updateHz)
    REAL_KEY("analysis.histogram_update_hz", analysis.histogramUpdateHz)
    UINT_KEY("analysis.temporal_frames", analysis.temporalFrames)
    REAL_KEY("analysis.smoothing_alpha", analysis.smoothingAlpha)
    if (strcmp(key, "analysis.spectrum_source") == 0 ||
        strcmp(key, "analysis.histogram_source") == 0) {
        int *field = strcmp(key, "analysis.spectrum_source") == 0 ? &c->analysis.spectrumPre
                                                                  : &c->analysis.histogramPre;
        if (strcmp(value, "pre") == 0) {
            *field = 1;
            return NG_OK;
        }
        if (strcmp(value, "post") == 0) {
            *field = 0;
            return NG_OK;
        }
        return NG_ERROR;
    }
    if (strcmp(key, "analysis.units") == 0) {
        if (strcmp(value, "dbfs_bin") == 0) {
            c->analysis.density = 0;
            return NG_OK;
        }
        if (strcmp(value, "dbfs_hz") == 0) {
            c->analysis.density = 1;
            return NG_OK;
        }
        return NG_ERROR;
    }
    if (strcmp(key, "analysis.temporal") == 0) {
        int mode;
        for (mode = NG_TEMPORAL_NONE; mode <= NG_TEMPORAL_EMA; ++mode) {
            if (strcmp(value, NgTemporalName((TNgTemporal)mode)) == 0) {
                c->analysis.temporal = (TNgTemporal)mode;
                return NG_OK;
            }
        }
        return NG_ERROR;
    }
    SIZE_KEY("analysis.histogram_pairs", analysis.histogramPairs)
    UINT_KEY("analysis.histogram_bins", analysis.histogramBins)
    if (strcmp(key, "analysis.window") == 0) {
        int w;
        for (w = NG_WINDOW_RECT; w <= NG_WINDOW_BLACKMAN_HARRIS; ++w) {
            if (strcmp(value, NgWindowName((TNgWindow)w)) == 0) {
                c->analysis.window = (TNgWindow)w;
                return NG_OK;
            }
        }
        return NG_ERROR;
    }
    BOOL_KEY("recording.enabled", recordingEnabled)
    if (strcmp(key, "recording.prefix") == 0) {
        return CopyText(c->recordingPrefix, sizeof(c->recordingPrefix), value);
    }
    if (strcmp(key, "recording.directory") == 0) {
        return CopyText(c->recordingDirectory, sizeof(c->recordingDirectory), value);
    }
    SIZE_KEY("recording.queue_blocks", recordingQueueBlocks)
    U64_KEY("recording.segment_samples", segmentSamples)
    if (strcmp(key, "replay.path") == 0) {
        return CopyText(c->replayPath, sizeof(c->replayPath), value);
    }
    U64_KEY("replay.start_sample", replayStartSample)
    U64_KEY("replay.sample_limit", replaySampleLimit)
    BOOL_KEY("replay.realtime", replayRealtime)
    BOOL_KEY("replay.loop", replayLoop)
    BOOL_KEY("replay.spectrum_inverted", replaySpectrumInverted)
    return NG_ERROR;
}

/**
 * @brief Read and validate a complete INI profile from a stream.
 *
 * @details
 * Starts from defaults. Rejects unknown or duplicate keys and malformed/bounded values. The
 * caller retains the FILE handle; no hardware is opened.
 *
 * Parameter input: Readable stream positioned at the beginning of the INI profile.
 * Parameter config: Receives parsed settings; do not use them if parsing fails.
 * Parameter error: Optional parse/validation error destination.
 *
 * Result: NG_OK for a valid profile; NG_ERROR for parse, I/O or validation failures.
 */
int NgConfigRead(FILE *input, TNgConfig *config, TNgError *error)
{
    char line[1024];
    char section[64] = "";
    char keys[128][128];
    size_t number = 0;
    size_t keyCount = 0;
    if (input == NULL || config == NULL) {
        return NgFail(error, "Invalid config input");
    }
    NgConfigDefaults(config);
    while (fgets(line, sizeof(line), input) != NULL) {
        char *text;
        char *equals;
        char *value;
        size_t i;
        char key[128];
        ++number;
        if (strchr(line, '\n') == NULL && !feof(input)) {
            return NgFail(error, "Config line %zu is too long", number);
        }
        text = Trim(line);
        if (*text == '\0' || *text == '#' || *text == ';') {
            continue;
        }
        if (*text == '[') {
            size_t length = strlen(text);
            if (length < 3 || text[length - 1] != ']') {
                return NgFail(error, "Invalid section at line %zu", number);
            }
            text[length - 1] = '\0';
            if (CopyText(section, sizeof(section), text + 1) != NG_OK ||
                (strcmp(section, "frontend") != 0 && strcmp(section, "hackrf") != 0 &&
                 strcmp(section, "conditioning") != 0 && strcmp(section, "buffer") != 0 &&
                 strcmp(section, "receiver") != 0 && strcmp(section, "recording") != 0 &&
                 strcmp(section, "replay") != 0 && strcmp(section, "analysis") != 0 &&
                 strcmp(section, "rtlsdr") != 0 && strcmp(section, "bladerf") != 0 &&
                 strcmp(section, "tracking") != 0 && strcmp(section, "dsp") != 0 &&
                 strcmp(section, "navigation") != 0 && strcmp(section, "pvt") != 0 &&
                 strcmp(section, "acquisition") != 0 &&
                 strcmp(section, "galileo_acquisition") != 0 &&
                 strcmp(section, "beidou_acquisition") != 0)) {
                return NgFail(error, "Unknown section at line %zu", number);
            }
            continue;
        }
        equals = strchr(text, '=');
        if (equals == NULL || *section == '\0') {
            return NgFail(error, "Expected section and key=value at line %zu", number);
        }
        *equals = '\0';
        text = Trim(text);
        value = Trim(equals + 1);
        if (*text == '\0' || strlen(section) + strlen(text) + 2 > sizeof(key)) {
            return NgFail(error, "Empty/long key or value at line %zu", number);
        }
        memcpy(key, section, strlen(section));
        key[strlen(section)] = '.';
        memcpy(key + strlen(section) + 1, text, strlen(text) + 1);
        /* Empty optional strings also recover profiles written by earlier releases. */
        if (*value == '\0' && strcmp(key, "frontend.serial") != 0 &&
            strcmp(key, "recording.directory") != 0 && strcmp(key, "replay.path") != 0) {
            return NgFail(error, "Missing value at line %zu: %s", number, key);
        }
        for (i = 0; i < keyCount; ++i) {
            if (strcmp(key, keys[i]) == 0) {
                return NgFail(error, "Duplicate config key %s", key);
            }
        }
        if (keyCount == 128) {
            return NgFail(error, "Too many config keys");
        }
        (void)CopyText(keys[keyCount++], sizeof(keys[0]), key);
        if (Apply(config, key, value) != NG_OK) {
            return NgFail(error, "Unknown key or invalid value at line %zu: %s", number, key);
        }
    }
    if (ferror(input) != 0) {
        return NgFail(error, "Cannot read config");
    }
    return NgConfigValidate(config, error);
}

/**
 * @brief Check receiver, hardware and diagnostic parameter bounds.
 *
 * @details
 * Checks interdependent settings such as buffer dimensions, gain steps, sample format and
 * signal centre in the sampled band. Does not mutate settings or query a device.
 *
 * Parameter config: Configuration to check.
 * Parameter error: Optional validation error destination.
 *
 * Result: NG_OK if consistent; NG_ERROR otherwise.
 */
int NgConfigValidate(const TNgConfig *config, TNgError *error)
{
    uint64_t bytes;
    if (config == NULL) {
        return NgFail(error, "Missing config");
    }
    if (config->replaySpectrumInverted != 0 && config->replaySpectrumInverted != 1) {
        return NgFail(error, "Invalid replay spectrum convention");
    }
    if (NgAnalysisValidate(&config->analysis, error) != NG_OK) {
        return NG_ERROR;
    }
    if (NgAcquisitionValidateBeidou(&config->beidouAcquisition, error) != NG_OK) {
        return NG_ERROR;
    }
    if (NgAcquisitionValidate(&config->galileoAcquisition, error) != NG_OK) {
        return NG_ERROR;
    }
    if (NgTrackingValidate(&config->tracking, error) != NG_OK) {
        return NG_ERROR;
    }
    if (NgAcquisitionValidate(&config->acquisition, error) != NG_OK) {
        return NG_ERROR;
    }
    if (config->dcRemoval != 0 ||
        (config->streamFormat != 0 && config->streamFormat != NG_FORMAT_CI8 &&
         config->streamFormat != NG_FORMAT_CI16)) {
        return NgFail(error, "DC removal is retired or common stream format is invalid");
    }
    if ((config->dcRemoval != 0 && config->dcRemoval != 1) || config->dcShift < 4 ||
        config->dcShift > 24) {
        return NgFail(error, "DC estimator shift must be 4..24");
    }
    if (config->frontend == NG_FRONTEND_SIGE &&
        (config->sampleRateHz != NG_SIGE_RATE_HZ ||
         config->centerFrequencyHz != NG_SIGE_CENTER_HZ ||
         config->signalFrequencyHz != NG_SIGE_SIGNAL_HZ || config->bandwidthHz != 0)) {
        return NgFail(error, "SiGe native IQ uses immutable rate, RF and IF parameters");
    }
    if (config->signalFrequencyHz > NG_HACKRF_MAX_FREQUENCY_HZ ||
        (config->signalFrequencyHz != 0 &&
         fabs((double)config->signalFrequencyHz - (double)config->centerFrequencyHz) >=
             (double)config->sampleRateHz / 2)) {
        return NgFail(error,
                      "Signal centre must be inside sampled hardware band (|digital IF| < Fs/2)");
    }
    if (config->recordingPreConditioning != 0 && config->recordingPreConditioning != 1) {
        return NgFail(error, "Invalid recording point");
    }
    if (config->frontend < NG_FRONTEND_FILE || config->frontend > NG_FRONTEND_SIGE) {
        return NgFail(error, "Unsupported frontend");
    }
    if (config->sampleRateHz == 0 ||
        config->sampleRateHz >
            (config->frontend == NG_FRONTEND_HACKRF ? NG_FRONTEND_MAX_SAMPLE_RATE_HZ : 61440000u) ||
        config->centerFrequencyHz > NG_HACKRF_MAX_FREQUENCY_HZ) {
        return NgFail(error, "Invalid rate or tuning for selected native frontend");
    }
    if (config->frontend == NG_FRONTEND_HACKRF &&
        config->sampleRateHz < NG_HACKRF_MIN_SAMPLE_RATE_HZ) {
        return NgFail(error, "HackRF profile requires at least 2 Msps");
    }
    if (config->lnaGainDb > 40 || config->lnaGainDb % 8 != 0 || config->vgaGainDb > 62 ||
        config->vgaGainDb % 2 != 0) {
        return NgFail(error, "HackRF LNA: 0..40 step 8 dB; VGA: 0..62 step 2 dB");
    }
    if ((config->bladeFormat != NG_FORMAT_CI8 && config->bladeFormat != NG_FORMAT_SC16_Q11) ||
        NgSampleBytes(config->replayFormat) == 0 || config->rtlAutoGain < 0 ||
        config->rtlAutoGain > 1 || config->rtlBiasTee < 0 || config->rtlBiasTee > 1 ||
        config->bladeBiasTee < 0 || config->bladeBiasTee > 1 || config->rtlPpm < -1000 ||
        config->rtlPpm > 1000 || config->rtlGainTenthsDb < -1000 ||
        config->rtlGainTenthsDb > 1000 || config->bladeGainDb < -100 || config->bladeGainDb > 100 ||
        (config->rtlIfGainDb != -1 && (config->rtlIfGainDb < 3 || config->rtlIfGainDb > 56))) {
        return NgFail(error, "Invalid native frontend controls or replay format");
    }
    if (config->frontend == NG_FRONTEND_RTLSDR &&
        (config->sampleRateHz <= 225000u || config->sampleRateHz > 3200000u ||
         (config->sampleRateHz > 300000u && config->sampleRateHz <= 900000u))) {
        return NgFail(error, "RTL-SDR sample rate must be (225000,300000] or (900000,3200000] Hz");
    }
    if (config->windowSamples == 0 || config->windowSamples > 1048576 ||
        config->ingressBlocks < 2 || config->ingressBlocks > 4096 || config->ringBlocks < 2 ||
        config->ringBlocks > 4096 || config->recordingQueueBlocks < 2 ||
        config->recordingQueueBlocks > 4096) {
        return NgFail(error, "Window must be 1..1048576 pairs and queues 2..4096 blocks");
    }
    {
        TNgSampleFormat native = config->frontend == NG_FRONTEND_BLADERF  ? config->bladeFormat
                                 : config->frontend == NG_FRONTEND_RTLSDR ? NG_FORMAT_CU8
                                 : config->frontend == NG_FRONTEND_FILE   ? config->replayFormat
                                                                          : NG_FORMAT_CI8;
        size_t nativeBytes = NgSampleBytes(native),
               commonBytes = config->streamFormat ? NgSampleBytes(config->streamFormat)
                                                  : (nativeBytes == 4u ? 4u : 2u);
        bytes = ((uint64_t)config->windowSamples * nativeBytes + 63u) * config->ingressBlocks;
        bytes += ((uint64_t)config->windowSamples * commonBytes + 63u) *
                 (config->ingressBlocks + config->ringBlocks + config->recordingQueueBlocks + 2u);
        if (config->frontend == NG_FRONTEND_SIGE) {
            bytes += 128u * 8192u;
        }
    }
    if (bytes > UINT64_C(536870912)) {
        return NgFail(error, "Buffer plan exceeds the documented 512 MiB guard");
    }
    if (isfinite(config->runSeconds) == 0 || config->runSeconds < 0 ||
        config->runSeconds > 31536000 || config->idleTimeoutMs < 100 ||
        config->idleTimeoutMs > 3600000) {
        return NgFail(error, "Invalid duration/idle timeout");
    }
    if (config->overflowPolicy != NG_OVERFLOW_STOP &&
        config->overflowPolicy != NG_OVERFLOW_DROP_NEWEST) {
        return NgFail(error, "Invalid overflow policy");
    }
    if (config->recordOnly != 0 && config->recordingEnabled == 0) {
        return NgFail(error, "record mode requires recording.enabled=true");
    }
    if (config->recordingEnabled && config->recordingPrefix[0] == '\0') {
        return NgFail(error, "Recording prefix is empty");
    }
    if (strchr(config->recordingDirectory, '\n') || strchr(config->recordingDirectory, '\r') ||
        strchr(config->recordingPrefix, '\n') || strchr(config->recordingPrefix, '\r')) {
        return NgFail(error, "Recording path cannot contain line breaks");
    }
    if (config->recordingDirectory[0] &&
        (strchr(config->recordingPrefix, '/') || strchr(config->recordingPrefix, '\\') ||
         strchr(config->recordingPrefix, ':'))) {
        return NgFail(error, "Use a base filename when recording.directory is set");
    }
    if (strchr(config->replayPath, '\n') != NULL || strchr(config->replayPath, '\r') != NULL ||
        strchr(config->serial, '\n') != NULL || strchr(config->serial, '\r') != NULL) {
        return NgFail(error, "Replay path and serial cannot contain line breaks");
    }
    if (config->replayStartSample > (uint64_t)INT64_MAX / NgSampleBytes(config->replayFormat)) {
        return NgFail(error, "Replay offset exceeds the supported file range");
    }
    return NG_OK;
}

/**
 * @brief Open, read and close an INI profile file.
 *
 * @details
 * Uses NgConfigRead and closes its local handle on every path. Does not configure hardware.
 *
 * Parameter path: Path of an existing readable INI file.
 * Parameter config: Receives parsed settings on success.
 * Parameter error: Optional error destination.
 *
 * Result: NG_OK on success; NG_ERROR for file, parse or validation errors.
 */
int NgConfigLoad(const char *path, TNgConfig *config, TNgError *error)
{
    FILE *file = fopen(path, "rb");
    int result;
    if (file == NULL) {
        return NgFail(error, "Cannot open config: %s", path);
    }
    result = NgConfigRead(file, config, error);
    (void)fclose(file);
    return result;
}

/**
 * @brief Serialize a receiver configuration as an INI profile.
 *
 * @details
 * Writes the supported keys to the supplied stream without taking ownership. The caller is
 * responsible for flush, close and atomic file replacement.
 *
 * Parameter output: Writable output stream.
 * Parameter config: Settings to serialize.
 * Parameter error: Optional write error destination.
 *
 * Result: NG_OK when serialization succeeds; NG_ERROR on stream errors.
 */
int NgConfigWrite(FILE *output, const TNgConfig *config, TNgError *error)
{
    if (output == NULL || NgConfigValidate(config, error) != NG_OK) {
        return NG_ERROR;
    }
    if (fprintf(output, "[frontend]\ntype=%s\nsample_rate_hz=%u\ncenter_frequency_hz=%" PRIu64 "\n",
                config->frontend == NG_FRONTEND_HACKRF    ? "hackrf"
                : config->frontend == NG_FRONTEND_RTLSDR  ? "rtlsdr"
                : config->frontend == NG_FRONTEND_SIGE    ? "sige"
                : config->frontend == NG_FRONTEND_BLADERF ? "bladerf"
                                                          : "file",
                config->sampleRateHz, config->centerFrequencyHz) < 0) {
        return NgFail(error, "Config write failed");
    }
    if (config->serial[0] != 0) {
        fprintf(output, "serial=%s\n", config->serial);
    }
    fprintf(output, "stream_format=%s\n",
            config->streamFormat ? NgSampleFormatName(config->streamFormat) : "auto");
    fprintf(output, "signal_frequency_hz=%" PRIu64 "\n", config->signalFrequencyHz);
    fprintf(output,
            "\n[hackrf]\nbandwidth_hz=%u\nlna_gain_db=%u\nvga_gain_db=%u\nrf_amp=%s\nantenna_power="
            "%s\n",
            config->bandwidthHz, config->lnaGainDb, config->vgaGainDb,
            (config->rfAmp) != 0 ? "true" : "false",
            (config->antennaPower) != 0 ? "true" : "false");
    fprintf(output,
            "\n[rtlsdr]\nauto_gain=%s\ngain_tenths_db=%d\nif_gain_db=%d\nppm=%d\nbias_tee=%s\n",
            config->rtlAutoGain ? "true" : "false", config->rtlGainTenthsDb, config->rtlIfGainDb,
            config->rtlPpm, config->rtlBiasTee ? "true" : "false");
    fprintf(output, "\n[bladerf]\ngain_db=%d\nbias_tee=%s\nformat=%s\n", config->bladeGainDb,
            config->bladeBiasTee ? "true" : "false", NgSampleFormatName(config->bladeFormat));
    fprintf(output,
            "\n[buffer]\nwindow_samples=%zu\ningress_blocks=%"
            "zu\nring_blocks=%zu\noverflow_policy=%s\n",
            config->windowSamples, config->ingressBlocks, config->ringBlocks,
            config->overflowPolicy == NG_OVERFLOW_STOP ? "stop" : "drop_newest");
    fprintf(output, "\n[receiver]\nmode=%s\nrun_seconds=%.17g\nidle_timeout_ms=%u\n",
            (config->recordOnly) != 0 ? "record" : "capture", config->runSeconds,
            config->idleTimeoutMs);
    fprintf(
        output,
        "channels=%u\ngps_reserved=%u\ngalileo_reserved=%u\nbeidou_reserved=%u\nauto_priority=%u\n",
        config->tracking.channelCount, config->tracking.gpsChannels,
        config->tracking.galileoChannels, config->tracking.beidouChannels,
        config->tracking.autoPriority);
    fprintf(output, "egnos=%s\n", config->tracking.egnosEnabled ? "true" : "false");
    fprintf(output,
            "\n[tracking]\nworkers=%u\ntap_count=%u\nspacing_chips=%.17g\n"
            "galileo_spacing_slots=%.17g\ndoppler_smoothing_ms=%u\n",
            config->tracking.workers, config->tracking.tapCount, config->tracking.spacingChips,
            2.0 * config->tracking.galileoSpacingChips, config->tracking.dopplerSmoothingMs);
    for (unsigned signal = 0; signal < NG_TRACKING_EGNOS_SIGNAL; ++signal) {
        for (unsigned loop = 0; loop < NG_LOOP_COUNT; ++loop) {
            const TNgLoopSettings settings = config->tracking.loops[signal][loop];
            fprintf(output, "%s_%s_bandwidth_hz=%.17g\n%s_%s_gain=%.17g\n", loopSignals[signal],
                    loopNames[loop], settings.bandwidthHz, loopSignals[signal], loopNames[loop],
                    settings.gain);
        }
    }
    fprintf(output, "gps_coherent_ms=%u\ngps_transition_checks=%u\n",
            config->tracking.gpsCoherentMs, config->tracking.gpsTransitionChecks);
    fprintf(output,
            "fll_decision_updates=%u\nfll_decision_interval_ms=%u\nfll_transition_hz=%.17g\n",
            config->tracking.fllDecisionUpdates, config->tracking.fllDecisionIntervalMs,
            config->tracking.fllTransitionHz);
    fprintf(output, "beidou_spacing_chips=%.17g\n", config->tracking.beidouSpacingChips);
    fprintf(output, "egnos_spacing_chips=%.17g\ncn0_cutoff_db_hz=%.17g\n",
            config->tracking.egnosSpacingChips, config->tracking.cn0CutoffDbHz);
    fprintf(output, "\n[navigation]\nbroadcast_ionosphere=%s\n",
            config->tracking.broadcastIonosphere ? "true" : "false");
    fprintf(output, "\n[acquisition]\nfirst_prn=%u\nlast_prn=%u\npfa=%.17g\ncpu_budget=%.17g\n",
            config->acquisition.firstPrn, config->acquisition.lastPrn,
            config->acquisition.falseAlarmProbability, config->acquisition.cpuBudget);
    fprintf(output, "\n[beidou_acquisition]\nfirst_prn=%u\nlast_prn=%u\n",
            config->beidouAcquisition.firstPrn, config->beidouAcquisition.lastPrn);
    fprintf(output, "\n[galileo_acquisition]\nfirst_prn=%u\nlast_prn=%u\npfa=%.17g\nfft_size=%u\n",
            config->galileoAcquisition.firstPrn, config->galileoAcquisition.lastPrn,
            config->galileoAcquisition.falseAlarmProbability, config->galileoAcquisition.fftSize);
    fprintf(output,
            "\n[dsp]\ntracking_fast_carrier=%s\ntracking_square_carrier=%s\n"
            "tracking_xor_correlation=%s\ntracking_mixer_bits=%u\n"
            "gps_square_carrier=%s\ngps_xor_correlation=%s\n"
            "galileo_square_carrier=%s\ngalileo_xor_correlation=%s\n"
            "gps_fft=%s\ngalileo_fft=%s\nspectrum_fft=%s\n",
            config->tracking.fastCarrier ? "true" : "false",
            config->tracking.squareCarrier ? "true" : "false",
            config->tracking.xorCorrelation ? "true" : "false", config->tracking.mixerBits,
            config->acquisition.squareCarrier ? "true" : "false",
            config->acquisition.xorCorrelation ? "true" : "false",
            config->galileoAcquisition.squareCarrier ? "true" : "false",
            config->galileoAcquisition.xorCorrelation ? "true" : "false",
            SearchFftName(config->acquisition.fftMode),
            SearchFftName(config->galileoAcquisition.fftMode),
            SearchFftName(config->analysis.fftMode));
    fprintf(output,
            "\n[recording]\nenabled=%s\nprefix=%s\nqueue_blocks=%zu\nsegment_samples=%" PRIu64 "\n",
            (config->recordingEnabled) != 0 ? "true" : "false", config->recordingPrefix,
            config->recordingQueueBlocks, config->segmentSamples);
    fprintf(output, "point=%s\n", (config->recordingPreConditioning) != 0 ? "pre" : "post");
    if (config->recordingDirectory[0] != 0) {
        fprintf(output, "directory=%s\n", config->recordingDirectory);
    }
    fprintf(output, "\n[replay]\nformat=%s\nspectrum_inverted=%s\n",
            NgSampleFormatName(config->replayFormat),
            config->replaySpectrumInverted ? "true" : "false");
    if (config->replayPath[0] != '\0') {
        fprintf(output, "path=%s\n", config->replayPath);
    }
    fprintf(output, "start_sample=%" PRIu64 "\nsample_limit=%" PRIu64 "\nrealtime=%s\nloop=%s\n",
            config->replayStartSample, config->replaySampleLimit,
            (config->replayRealtime) != 0 ? "true" : "false",
            (config->replayLoop) != 0 ? "true" : "false");
    fprintf(
        output,
        "\n[analysis]\nspectrum_enabled=%s\nhistogram_enabled=%s\nfft_size=%zu\nwindow=%s\noverlap_"
        "percent=%u\naverages=%u\nupdate_hz=%.9g\nhistogram_pairs=%zu\nhistogram_bins=%u\n",
        (config->analysis.spectrumEnabled) != 0 ? "true" : "false",
        (config->analysis.histogramEnabled) != 0 ? "true" : "false", config->analysis.fftSize,
        NgWindowName(config->analysis.window), config->analysis.overlapPercent,
        config->analysis.averages, config->analysis.updateHz, config->analysis.histogramPairs,
        config->analysis.histogramBins);
    fprintf(output,
            "histogram_update_hz=%.9g\nspectrum_source=%s\nhistogram_source=%s\nunits=%s\ntemporal="
            "%s\ntemporal_frames=%u\nsmoothing_alpha=%.9g\n",
            config->analysis.histogramUpdateHz,
            (config->analysis.spectrumPre) != 0 ? "pre" : "post",
            (config->analysis.histogramPre) != 0 ? "pre" : "post",
            (config->analysis.density) != 0 ? "dbfs_hz" : "dbfs_bin",
            NgTemporalName(config->analysis.temporal), config->analysis.temporalFrames,
            config->analysis.smoothingAlpha);
    return (ferror(output)) != 0 ? NgFail(error, "Config write failed") : NG_OK;
}

void NgConfigSige(TNgConfig *config)
{
    config->frontend = NG_FRONTEND_SIGE;
    config->sampleRateHz = NG_SIGE_RATE_HZ;
    config->centerFrequencyHz = NG_SIGE_CENTER_HZ;
    config->signalFrequencyHz = NG_SIGE_SIGNAL_HZ;
    config->bandwidthHz = 0;
    config->lnaGainDb = config->vgaGainDb = 0;
    config->rfAmp = config->antennaPower = 0;
}
