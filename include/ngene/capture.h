/** @file capture.h @brief Portable CI8 capture metadata and replay-profile preparation. */
#ifndef NGENE_CAPTURE_H
#define NGENE_CAPTURE_H
#include "ngene/config.h"

/** @brief Owned description of one recorded file; counts always denote complex I/Q pairs. */
typedef struct {
    /** Nonzero when a validated sidecar was found; zero requires explicit stream settings. */
    int available;
    /** Metadata schema: 2 for a per-file sidecar, 1 for a historical session sidecar. */
    unsigned version;
    /** Native IQ representation stored in the raw file. */
    TNgSampleFormat format;
    /** Sampling frequency in complex pairs per second; recorded nominal clock rate. */
    uint32_t sampleRateHz;
    /** SDR hardware tuning frequency in Hz. */
    uint64_t centerFrequencyHz;
    /** Signal centre in Hz, resolved rather than the profile's zero sentinel. */
    uint64_t signalFrequencyHz;
    /** Analogue bandwidth at capture in Hz; zero means unavailable. */
    uint32_t bandwidthHz;
    /** Nonzero when the capture used the DC conditioner. */
    int dcRemoval;
    /** Conditioner label at capture; replay always starts with bypass. */
    char conditioningName[64];
    /** Continuous DC estimator exponent at capture. */
    unsigned dcShift;
    /** Nonzero for samples saved before conditioning. */
    int preConditioning;
    /** Nonzero when the sidecar records successful file/session completion. */
    int complete;
    /** Number of complete complex pairs in the selected raw file. */
    uint64_t samples;
    /** Nonzero when the per-file index contains at least one stream discontinuity. */
    int hasGaps;
    /** NUL-terminated sidecar path; empty when unavailable. */
    char metadataPath[640];
} TNgCaptureMetadata;

/**
 * @brief Inspect a closed CI8 file and discover its adjacent capture metadata.
 * @details Tries FILE.cs8.meta.ini first, then the historical PREFIX.meta.ini for NGene
 * numbered segments. Validates schema, signed interleaving, frequency consistency and
 * per-file sample count. Never reads the sample stream or modifies files. An absent sidecar
 * is allowed; an existing malformed sidecar is an error. Caller excludes active writers.
 * @param path Existing regular CI8 file on the receiver computer.
 * @param metadata Receives owned results only on success.
 * @param error Optional file/metadata diagnostic.
 * @return NG_OK for a valid file; NG_ERROR on malformed metadata or file errors.
 */
int NgReplayInspect(const char *path, TNgCaptureMetadata *metadata, TNgError *error);

/**
 * @brief Prepare a stopped replay profile from a selected file and its metadata.
 * @details Preserves diagnostic/buffer policy settings. Resets the replay range, disables
 * automatic recording and uses bypass conditioning to avoid processing saved samples twice.
 * With metadata, loads Fs, hardware/signal tuning and bandwidth and chooses a 1 ms window.
 * Without metadata, retains explicitly configured stream settings. No receiver is started.
 * @param path Selected receiver-side file.
 * @param config In/out profile; unchanged on failure.
 * @param metadata Receives the inspection result on success; may be NULL.
 * @param error Optional diagnostic.
 * @return NG_OK on success; NG_ERROR for file, metadata or resulting-profile errors.
 */
int NgReplayConfigure(const char *path, TNgConfig *config, TNgCaptureMetadata *metadata,
                      TNgError *error);
#endif
