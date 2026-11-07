/** @file recording.c
 * @brief NGene recording implementation.
 */
#include "ngene/recording.h"
#include "ngene/capture.h"
#include "ngene/ring.h"
#include "platform/platform.h"
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/** @brief Bounded input queue and writer-owned capture files with synchronized statistics. */
struct TNgRecorder {
    /** Owned bounded ring; samples are copied before returning to the producer. */
    TNgRing *queue;
    /** Protects copied writer counters; transport never performs disk I/O. */
    TNgMutex statsMutex;
    /** Owned worker handle; join before destroying any accessed state. */
    TNgThread thread;
    /** Atomic writer failure flag checked by the receiver/manager. */
    TNgAtomicInt failed;
    /** Writer diagnostic published before the failure flag. */
    TNgError writerError;
    /** Copied validated configuration; owned by this instance. */
    TNgConfig config;
    TNgSampleFormat nativeFormat;
    unsigned adcBits;
    /** Writer-owned raw CI8 segment handle. */
    FILE *raw;
    /** Writer-owned CSV stream containing sample indices and gap events. */
    FILE *index;
    /** Writer-owned metadata stream; finalized before the session closes. */
    FILE *metadata;
    /** Writer-owned sidecar for the current raw segment. */
    FILE *segmentMetadata;
    /** Owned stream description used by every segment header. */
    TNgCaptureMetadata capture;
    /** Discontinuities observed in the current segment. */
    uint64_t segmentGaps;
    /** Current raw segment number, starting at zero. */
    uint64_t segment;
    /** Complex pairs already written to the current raw segment. */
    uint64_t segmentOffset;
    /** Copied counters protected by statsMutex. */
    TNgRecordingStats stats;
};

static unsigned StoredPrecision(const TNgRecorder *r)
{
    unsigned width = (unsigned)NgSampleBytes(r->capture.format) * 4u;
    return r->adcBits < width ? r->adcBits : width;
}

/**
 * @brief Open the next raw CI8 segment exclusively.
 *
 * @details
 * Uses the recorder prefix and sequential segment number; existing files are not overwritten.
 * Writer thread owns segment creation during capture.
 *
 * @param r Recorder state; may update raw handle and segment counters.
 *
 * @return NG_OK on success; NG_ERROR on path/open failure.
 */
static int SegmentOpen(TNgRecorder *r)
{
    char path[640];
    if (snprintf(path, sizeof(path), "%s_%06" PRIu64 ".%s", r->config.recordingPrefix, r->segment,
                 r->capture.format == NG_FORMAT_CI16       ? "ci16"
                 : r->capture.format == NG_FORMAT_SC16_Q11 ? "sc16q11"
                 : r->capture.format == NG_FORMAT_CU8      ? "cu8"
                                                           : "cs8") >= (int)sizeof(path)) {
        return NgFail(&r->writerError, "Recording path is too long");
    }
    r->raw = NgFopenExclusive(path);
    if (r->raw == NULL) {
        return NgFail(&r->writerError, "Cannot create %s: %s", path, strerror(errno));
    }
    {
        char sidecar[672];
        const char *name = path;
        const char *cursor;
        for (cursor = path; *cursor != '\0'; ++cursor) {
            if (*cursor == '/' || *cursor == '\\') {
                name = cursor + 1;
            }
        }
        (void)snprintf(sidecar, sizeof(sidecar), "%s.meta.ini", path);
        r->segmentMetadata = NgFopenExclusive(sidecar);
        if (r->segmentMetadata == NULL) {
            return NgFail(&r->writerError, "Cannot create segment sidecar: %s", strerror(errno));
        }
        if (fprintf(
                r->segmentMetadata,
                "[capture]\nversion=2\nformat=%s\nlayout=I,Q interleaved\n"
                "component_bits=%u\ncomponent_signed=%s\nbyte_order=little_endian\n"
                "significant_bits=%u\nfractional_bits=%u\nsource_native_format=%s\nsource_adc_bits=%u\ndata_file=%s\n"
                "sample_rate_hz=%u\nsample_rate_basis=configured_nominal\n"
                "center_frequency_hz=%" PRIu64 "\nsignal_frequency_hz=%" PRIu64
                "\ndigital_if_hz=%" PRId64
                "\nspectrum_inverted=%d\nbandwidth_hz=%u\nconditioning=%s\n"
                "recording_point=%s\nhardware_timestamp=%s\n"
                "host_start_unix_seconds=%" PRId64 "\n",
                NgSampleFormatName(r->capture.format),
                r->capture.format == NG_FORMAT_SC16_Q11 || r->capture.format == NG_FORMAT_CI16 ? 16u
                                                                                               : 8u,
                r->capture.format == NG_FORMAT_CU8 ? "false" : "true", StoredPrecision(r),
                r->capture.format == NG_FORMAT_CI16       ? 15u
                : r->capture.format == NG_FORMAT_SC16_Q11 ? 11u
                                                          : 0u,
                NgSampleFormatName(r->nativeFormat), r->adcBits, name, r->capture.sampleRateHz,
                r->capture.centerFrequencyHz, r->capture.signalFrequencyHz,
                (int64_t)r->capture.signalFrequencyHz - (int64_t)r->capture.centerFrequencyHz,
                r->capture.spectrumInverted, r->capture.bandwidthHz, r->capture.conditioningName,
                r->capture.preConditioning != 0 ? "pre_conditioning" : "post_conditioning",
                r->capture.format == NG_FORMAT_SC16_Q11 && r->config.frontend == NG_FRONTEND_BLADERF
                    ? "true"
                    : "false",
                (int64_t)time(NULL)) < 0 ||
            fflush(r->segmentMetadata) != 0) {
            return NgFail(&r->writerError, "Cannot write segment metadata");
        }
    }
    NgMutexLock(&r->statsMutex);
    ++r->stats.segments;
    NgMutexUnlock(&r->statsMutex);
    r->segmentOffset = 0;
    r->segmentGaps = 0;
    return NG_OK;
}

/** @brief Close one raw segment and finalize its adjacent, portable sidecar.
 * @details Only the disk writer or the joined recorder owner may call this function.
 * A segment becomes complete only after the raw close succeeds; failures remain explicit.
 * @param r Owned recorder state.
 * @param successful Nonzero if upstream and writer operations succeeded.
 * @return NG_OK if both files close successfully; NG_ERROR otherwise.
 */
static int SegmentClose(TNgRecorder *r, int successful)
{
    int failed = 0;
    if (r->raw != NULL) {
        failed = fclose(r->raw) != 0;
        r->raw = NULL;
    }
    if (r->segmentMetadata != NULL) {
        if (fprintf(r->segmentMetadata,
                    "\n[result]\ncomplete=%s\nwritten_samples=%" PRIu64 "\ngap_events=%" PRIu64
                    "\n",
                    successful != 0 && failed == 0 ? "true" : "false", r->segmentOffset,
                    r->segmentGaps) < 0) {
            failed = 1;
        }
        if (fclose(r->segmentMetadata) != 0) {
            failed = 1;
        }
        r->segmentMetadata = NULL;
    }
    return failed != 0 ? NgFail(&r->writerError, "Cannot finalize recording segment") : NG_OK;
}

/**
 * @brief Write one queued complex window and its sample-index record.
 *
 * @details
 * Splits across configured segments without changing sample order, updates counters under
 * stats lock and records gap/epoch metadata. Filesystem work belongs only to the writer
 * thread.
 *
 * @param r Writer-owned recorder state.
 * @param view Borrowed queue view, valid until writer releases it.
 *
 * @return NG_OK on success; NG_ERROR for write/segment/close errors.
 */
static int WriteWindow(TNgRecorder *r, const TNgIqView *view)
{
    size_t offset = 0;
    while (offset < view->meta.count) {
        size_t count = view->meta.count - offset;
        if (r->config.segmentSamples != 0 && r->segmentOffset == r->config.segmentSamples) {
            if (SegmentClose(r, 1) != NG_OK) {
                return NG_ERROR;
            }
            ++r->segment;
            if (SegmentOpen(r) != NG_OK) {
                return NG_ERROR;
            }
        }
        if (r->config.segmentSamples != 0 && r->config.segmentSamples - r->segmentOffset < count) {
            count = (size_t)(r->config.segmentSamples - r->segmentOffset);
        }
        if (fwrite((const unsigned char *)(const void *)view->samples +
                       offset * NgSampleBytes(view->format),
                   NgSampleBytes(view->format), count, r->raw) != count) {
            return NgFail(&r->writerError,
                          "I/Q write failed (disk full or I/O error); capture is incomplete");
        }
        if (fprintf(r->index,
                    "%" PRIu64 ",%" PRIu64 ",%zu,%" PRIu64 ",%" PRIu64 ",%u,%d,%" PRIu64
                    ",%d,%" PRIu64 "\n",
                    view->meta.epoch, view->meta.firstSample + offset, count, r->segment,
                    r->segmentOffset,
                    offset == 0 ? view->meta.flags : (view->meta.flags & ~(unsigned)NG_BLOCK_GAP),
                    offset == 0 ? view->meta.missingKnown : 1,
                    offset == 0 ? view->meta.missingBefore : UINT64_C(0),
                    (view->meta.flags & NG_BLOCK_HARDWARE_TIME) != 0,
                    (view->meta.flags & NG_BLOCK_HARDWARE_TIME) != 0
                        ? view->meta.hardwareTimestamp + offset
                        : 0) < 0) {
            return NgFail(&r->writerError, "Recording index write failed");
        }
        r->segmentOffset += count;
        if (offset == 0 && (view->meta.flags & NG_BLOCK_GAP) != 0) {
            ++r->segmentGaps;
        }
        NgMutexLock(&r->statsMutex);
        r->stats.samples += count;
        NgMutexUnlock(&r->statsMutex);
        offset += count;
    }
    NgMutexLock(&r->statsMutex);
    ++r->stats.windows;
    NgMutexUnlock(&r->statsMutex);
    return NG_OK;
}

/**
 * @brief Drain recorder queue blocks into raw and index files.
 *
 * @details
 * Consumes one lease at a time, writes outside producer context and latches errors atomically.
 * Exits when the queue is closed and drained or writing fails.
 *
 * @param context Recorder instance retained by finish until this thread joins.
 */
static void WriterThread(void *context)
{
    TNgRecorder *r = context;
    for (;;) {
        TNgIqView view;
        int status = NgRingAcquire(r->queue, &view, 100);
        if (status == NG_END) {
            break;
        }
        if (status == NG_TIMEOUT) {
            continue;
        }
        if (status != NG_OK) {
            NgFail(&r->writerError, "Recorder queue failed");
            NgAtomicSet(&r->failed, 1);
            break;
        }
        status = WriteWindow(r, &view);
        NgRingRelease(r->queue);
        if (status != NG_OK) {
            NgAtomicSet(&r->failed, 1);
            break;
        }
    }
}

/**
 * @brief Create exclusive capture files and start the asynchronous writer.
 *
 * @details
 * Creates native IQ, sample index and metadata files without replacing existing files. Copies
 * stream metadata as needed; the recorder owns a bounded queue. Finish is mandatory to flush
 * and release resources.
 *
 * Parameter config: Validated recording settings including receiver-side directory and prefix.
 * Parameter info: Stream metadata describing captured samples.
 * Parameter conditioningName: Conditioner label written to metadata.
 * Parameter out: Receives caller-owned recorder on success.
 * Parameter error: Optional file/allocation/thread error destination.
 *
 * Result: NG_OK on success; NG_ERROR otherwise.
 */
int NgRecorderOpen(const TNgConfig *config, const TNgStreamInfo *info, const char *conditioningName,
                   TNgRecorder **out, TNgError *error)
{
    TNgRecorder *r;
    char path[640];
    if (out == NULL || info == NULL || NgConfigValidate(config, error) != NG_OK) {
        return NG_ERROR;
    }
    *out = NULL;
    r = calloc(1, sizeof(*r));
    if (r == NULL) {
        return NgFail(error, "Cannot allocate recorder");
    }
    if (NgMutexInit(&r->statsMutex) != 0) {
        free(r);
        return NgFail(error, "Cannot initialize recorder counters");
    }
    r->config = *config;
    r->nativeFormat = info->nativeFormat ? info->nativeFormat : info->format;
    r->adcBits = info->adcBits ? info->adcBits
                               : (info->format == NG_FORMAT_SC16_Q11 ? 12u
                                  : info->format == NG_FORMAT_CI16   ? 16u
                                                                     : 8u);
    r->capture.format = info->format;
    r->capture.sampleRateHz = info->sampleRateHz;
    r->capture.spectrumInverted = info->spectrumInverted;
    r->capture.centerFrequencyHz = info->centerFrequencyHz;
    r->capture.signalFrequencyHz =
        info->signalFrequencyHz != 0 ? info->signalFrequencyHz : info->centerFrequencyHz;
    r->capture.bandwidthHz = info->bandwidthHz;
    r->capture.preConditioning = config->recordingPreConditioning;
    r->capture.dcShift = config->dcShift;
    if (conditioningName == NULL ||
        strlen(conditioningName) >= sizeof(r->capture.conditioningName)) {
        NgMutexDestroy(&r->statsMutex);
        free(r);
        return NgFail(error, "Invalid recording conditioner label");
    }
    (void)snprintf(r->capture.conditioningName, sizeof(r->capture.conditioningName), "%s",
                   conditioningName);
    if (config->recordingDirectory[0] != 0 &&
        snprintf(r->config.recordingPrefix, sizeof(r->config.recordingPrefix), "%s/%s",
                 config->recordingDirectory,
                 config->recordingPrefix) >= (int)sizeof(r->config.recordingPrefix)) {
        NgMutexDestroy(&r->statsMutex);
        free(r);
        return NgFail(error, "Recording path is too long");
    }
    if (NgRingCreateFormat(config->recordingQueueBlocks, config->windowSamples, info->format,
                           &r->queue, error) != NG_OK) {
        NgMutexDestroy(&r->statsMutex);
        free(r);
        return NG_ERROR;
    }
    (void)snprintf(path, sizeof(path), "%s.meta.ini", r->config.recordingPrefix);
    r->metadata = NgFopenExclusive(path);
    if (r->metadata == NULL) {
        NgFail(error, "Cannot create metadata %s: %s", path, strerror(errno));
        goto failed;
    }
    (void)snprintf(path, sizeof(path), "%s.index.csv", r->config.recordingPrefix);
    r->index = NgFopenExclusive(path);
    if (r->index == NULL) {
        NgFail(error, "Cannot create index %s: %s", path, strerror(errno));
        goto failed;
    }
    if (SegmentOpen(r) != NG_OK) {
        if (error != NULL) {
            *error = r->writerError;
        }
        goto failed;
    }
    if (fprintf(
            r->metadata,
            "[capture]\nversion=1\nformat=%s\nlayout=I,Q "
            "interleaved\ncomponent_bits=%u\ncomponent_signed=%s\n"
            "byte_order=little_endian\nsignificant_bits=%u\nfractional_bits=%u\nsource_native_format=%s\nsource_adc_bits=%u\n"
            "sample_rate_hz=%u\ncenter_frequency_hz=%" PRIu64 "\nsignal_frequency_hz=%" PRIu64
            "\ndigital_if_hz=%" PRId64 "\nspectrum_inverted=%d\nbandwidth_hz=%u\n"
            "source=%s\nclock_domain=%s\nhardware_timestamp=%s\n"
            "host_start_unix_seconds=%" PRId64 "\nconditioning=%s\nrecording_point=%s\n"
            "device_identifier=%s\nserial_requested=%s\nlna_gain_db=%u\nvga_gain_db=%u\nrf_amp="
            "%d\nantenna_power=%d\n"
            "segment_samples=%" PRIu64 "\n",
            NgSampleFormatName(info->format),
            info->format == NG_FORMAT_SC16_Q11 || info->format == NG_FORMAT_CI16 ? 16u : 8u,
            info->format == NG_FORMAT_CU8 ? "false" : "true", StoredPrecision(r),
            info->format == NG_FORMAT_CI16       ? 15u
            : info->format == NG_FORMAT_SC16_Q11 ? 11u
                                                 : 0u,
            NgSampleFormatName(r->nativeFormat), r->adcBits, info->sampleRateHz,
            info->centerFrequencyHz, info->signalFrequencyHz, info->digitalIfHz,
            info->spectrumInverted, info->bandwidthHz, info->sourceName, info->clockDomain,
            config->frontend == NG_FRONTEND_BLADERF ? "true" : "false", (int64_t)time(NULL),
            conditioningName,
            (config->recordingPreConditioning) != 0 ? "pre_conditioning" : "post_conditioning",
            info->deviceIdentifier == NULL ? "unavailable" : info->deviceIdentifier, config->serial,
            config->lnaGainDb, config->vgaGainDb, config->rfAmp, config->antennaPower,
            config->segmentSamples) < 0 ||
        fprintf(r->index, "epoch,first_sample,count,segment,segment_offset_samples,flags,missing_"
                          "known,missing_before,hardware_time_valid,hardware_timestamp\n") < 0 ||
        fflush(r->metadata) != 0 || fflush(r->index) != 0) {
        NgFail(error, "Cannot write capture headers");
        goto failed;
    }
    if (NgThreadStart(&r->thread, WriterThread, r) != 0) {
        NgFail(error, "Cannot start recorder thread");
        goto failed;
    }
    *out = r;
    return NG_OK;
failed:
    (void)SegmentClose(r, 0);
    if (r->index != NULL) {
        (void)fclose(r->index);
    }
    if (r->metadata != NULL) {
        (void)fclose(r->metadata);
    }
    NgRingDestroy(r->queue);
    NgMutexDestroy(&r->statsMutex);
    free(r);
    return NG_ERROR;
}

/**
 * @brief Copy a whole window into the bounded recorder queue.
 *
 * @details
 * One sample producer may submit while the writer consumes. Does not perform filesystem I/O.
 * Full or failed recorder queues must stop capture; silent recording loss is not permitted.
 *
 * Parameter recorder: Live recorder; protect lifetime against concurrent finish.
 * Parameter window: Borrowed read-only native IQ window copied during this call.
 *
 * Result: NG_OK if copied; NG_FULL if the queue is full; NG_ERROR if writer failed or input is
 * invalid.
 */
int NgRecorderSubmit(TNgRecorder *recorder, const TNgIqView *window)
{
    if (NgAtomicGet(&recorder->failed) != 0) {
        return NG_ERROR;
    }
    return NgRingPush(recorder->queue, window);
}

/**
 * @brief Drain accepted windows, join the writer and release the recorder.
 *
 * @details
 * Stop submitting first. May block on filesystem I/O. Closes all files and appends final
 * completion/loss metadata; frees the recorder even if finalization fails. A NULL recorder is
 * accepted.
 *
 * Parameter recorder: Recorder to finish; invalid after return.
 * Parameter successfulRun: Nonzero if the receiver run completed without upstream failure.
 * Parameter sourceStats: Optional final frontend loss/capture counters.
 * Parameter stats: Optional destination for final written counters.
 * Parameter error: Optional error destination.
 *
 * Result: NG_OK if finalization succeeds; NG_ERROR for writer/flush/close failures.
 */
int NgRecorderFinish(TNgRecorder *recorder, int successfulRun, const TNgFrontendStats *sourceStats,
                     TNgRecordingStats *stats, TNgError *error)
{
    int failed;
    if (recorder == NULL) {
        return NG_OK;
    }
    NgRingClose(recorder->queue);
    NgThreadJoin(recorder->thread);
    failed = NgAtomicGet(&recorder->failed);
    if (SegmentClose(recorder, successfulRun != 0 && failed == 0) != NG_OK) {
        failed = 1;
    }
    if (fclose(recorder->index) != 0) {
        failed = 1;
    }
    if (fprintf(recorder->metadata,
                "\n[result]\ncomplete=%s\nwritten_samples=%" PRIu64 "\nwritten_windows=%" PRIu64
                "\nsegments=%" PRIu64 "\n",
                ((successfulRun) != 0) && ((failed) == 0) ? "true" : "false",
                recorder->stats.samples, recorder->stats.windows, recorder->stats.segments) < 0) {
        failed = 1;
    }
    if (sourceStats != NULL &&
        fprintf(recorder->metadata,
                "source_received_samples=%" PRIu64 "\nsource_known_dropped_samples=%" PRIu64
                "\nsource_queued_samples_discarded_on_stop=%" PRIu64 "\n",
                sourceStats->receivedSamples, sourceStats->droppedSamples,
                sourceStats->discardedOnStop) < 0) {
        failed = 1;
    }
    if (fclose(recorder->metadata) != 0) {
        failed = 1;
    }
    if (stats != NULL) {
        *stats = recorder->stats;
    }
    if (failed != 0) {
        NgFail(error, "%s",
               (recorder->writerError.message[0]) != 0 ? recorder->writerError.message
                                                       : "Recorder final flush failed");
    }
    NgRingDestroy(recorder->queue);
    NgMutexDestroy(&recorder->statsMutex);
    free(recorder);
    return (failed) != 0 ? NG_ERROR : NG_OK;
}

/**
 * @brief Copy recorder progress while protecting its counters.
 *
 * @details
 * The owner must prevent concurrent recorder destruction. Counts describe written blocks;
 * queue occupancy is sampled separately and is not an atomic multi-counter transaction.
 *
 * Parameter recorder: Live recorder whose lifetime is protected by caller.
 * Parameter stats: Receives progress counters in windows, complex pairs and segments.
 */
void NgRecorderSnapshot(TNgRecorder *recorder, TNgRecordingStats *stats)
{
    NgMutexLock(&recorder->statsMutex);
    *stats = recorder->stats;
    NgMutexUnlock(&recorder->statsMutex);
    stats->queuedSamples = NgRingPendingSamples(recorder->queue);
}
