/** @file config.h @brief Strict, bounded INI configuration. */
#ifndef NGENE_CONFIG_H
#define NGENE_CONFIG_H
#include "ngene/types.h"
#include "ngene/analysis.h"
#include <stdio.h>

/** @brief Implemented independent stream source kinds. */
typedef enum {
    NG_FRONTEND_FILE,
    NG_FRONTEND_HACKRF,
    NG_FRONTEND_RTLSDR,
    NG_FRONTEND_BLADERF
} TNgFrontendKind;

/** @brief Mandatory stream-buffer overflow policy. */
typedef enum { NG_OVERFLOW_STOP, NG_OVERFLOW_DROP_NEWEST } TNgOverflowPolicy;

/** @brief Receiver profile; startup settings and independent diagnostic settings. */
typedef struct {
    /** Selected transport implementation or frontend statistics, according to the object. */
    TNgFrontendKind frontend;
    /** Optional NUL-terminated HackRF device serial; empty selects the default device. */
    char serial[128];
    /** Complex pairs per second; never the interleaved component byte rate. */
    uint32_t sampleRateHz;
    /** Hardware SDR tuning; legacy key retained. */
    uint64_t centerFrequencyHz;
    /** 0 follows hardware, for old profiles. */
    uint64_t signalFrequencyHz;
    /** Enable the optional continuous DC estimator/remover. */
    int dcRemoval;
    /** Continuous DC estimator coefficient = 2^-shift. */
    unsigned dcShift;
    /** Requested analogue baseband filter bandwidth; zero selects automatically. */
    uint32_t bandwidthHz;
    /** HackRF LNA gain in decibels: 0 through 40 in steps of 8. */
    unsigned lnaGainDb;
    /** HackRF VGA gain in decibels: 0 through 62 in steps of 2. */
    unsigned vgaGainDb;
    /** Enable the HackRF RF amplifier when nonzero. */
    int rfAmp;
    /** Enable HackRF antenna power when nonzero; explicit user configuration. */
    int antennaPower;
    /** RTL-SDR manual tuner gain, in tenth-dB; validated against the connected tuner. */
    int rtlGainTenthsDb;
    /** Use the RTL-SDR tuner automatic gain mode when nonzero. */
    int rtlAutoGain;
    /** RTL-SDR oscillator correction in ppm. */
    int rtlPpm;
    /** Explicitly requested RTL-SDR bias-tee state. */
    int rtlBiasTee;
    /** bladeRF total RX gain in dB, validated using the device gain range. */
    int bladeGainDb;
    /** Explicitly requested bladeRF RX bias-tee state. */
    int bladeBiasTee;
    /** IQ representation for replay without a sidecar; metadata overrides this selection. */
    TNgSampleFormat replayFormat;
    /** Complex pairs in each shared engine window. */
    size_t windowSamples;
    /** Capacity of the bounded device-callback ingress queue, in windows. */
    size_t ingressBlocks;
    /** Capacity of the shared engine ring, in windows. */
    size_t ringBlocks;
    /** Action on an exhausted mandatory stream buffer. */
    TNgOverflowPolicy overflowPolicy;
    /** Maximum run duration in seconds; zero runs until stop or EOF. */
    double runSeconds;
    /** Maximum time without frontend progress before a stream error. */
    unsigned idleTimeoutMs;
    /** Automatically begin recording when the engine starts. */
    int recordingEnabled;
    /** Capture-only execution without processing receiver channels. */
    int recordOnly;
    /** 0 post (default), 1 raw frontend samples. */
    int recordingPreConditioning;
    /** Optional diagnostic settings or an owned diagnostic snapshot. */
    TNgAnalysisConfig analysis;
    /** Capture filename prefix; CLI may include a path when no directory is set. */
    char recordingPrefix[512];
    /** Optional receiver-side directory; empty preserves legacy CLI prefix paths. */
    char recordingDirectory[320];
    /** Bounded disk-writer queue capacity, in windows. */
    size_t recordingQueueBlocks;
    /** Complex pairs per raw recording segment; zero disables rotation. */
    uint64_t segmentSamples;
    /** Receiver-side immutable CI8 input file. */
    char replayPath[512];
    /** Initial complex-pair offset. */
    uint64_t replayStartSample;
    /** 0 reads the selected file range without a limit. */
    uint64_t replaySampleLimit;
    /** Pace by configured complex sample rate. */
    int replayRealtime;
    /** Repeat the selected range with monotonic sample indices. */
    int replayLoop;
} TNgConfig;

/**
 * @brief Initialize a complete receiver configuration with supported defaults.
 *
 * @details
 * Overwrites the structure, including diagnostics, frontend, buffers and recording settings.
 * It does not open hardware or create files.
 *
 * @param config Writable configuration structure.
 */
void NgConfigDefaults(TNgConfig *config);
/**
 * @brief Read and validate a complete INI profile from a stream.
 *
 * @details
 * Starts from defaults. Rejects unknown or duplicate keys and malformed/bounded values. The
 * caller retains the FILE handle; no hardware is opened.
 *
 * @param input Readable stream positioned at the beginning of the INI profile.
 * @param config Receives parsed settings; do not use them if parsing fails.
 * @param error Optional parse/validation error destination.
 *
 * @return NG_OK for a valid profile; NG_ERROR for parse, I/O or validation failures.
 */
int NgConfigRead(FILE *input, TNgConfig *config, TNgError *error);
/**
 * @brief Serialize a receiver configuration as an INI profile.
 *
 * @details
 * Writes the supported keys to the supplied stream without taking ownership. The caller is
 * responsible for flush, close and atomic file replacement.
 *
 * @param output Writable output stream.
 * @param config Settings to serialize.
 * @param error Optional write error destination.
 *
 * @return NG_OK when serialization succeeds; NG_ERROR on stream errors.
 */
int NgConfigWrite(FILE *output, const TNgConfig *config, TNgError *error);
/**
 * @brief Open, read and close an INI profile file.
 *
 * @details
 * Uses NgConfigRead and closes its local handle on every path. Does not configure hardware.
 *
 * @param path Path of an existing readable INI file.
 * @param config Receives parsed settings on success.
 * @param error Optional error destination.
 *
 * @return NG_OK on success; NG_ERROR for file, parse or validation errors.
 */
int NgConfigLoad(const char *path, TNgConfig *config, TNgError *error);
/**
 * @brief Check receiver, hardware and diagnostic parameter bounds.
 *
 * @details
 * Checks interdependent settings such as buffer dimensions, gain steps, sample format and
 * signal centre in the sampled band. Does not mutate settings or query a device.
 *
 * @param config Configuration to check.
 * @param error Optional validation error destination.
 *
 * @return NG_OK if consistent; NG_ERROR otherwise.
 */
int NgConfigValidate(const TNgConfig *config, TNgError *error);
#endif
