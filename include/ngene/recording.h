/** @file recording.h @brief Bounded asynchronous writer for raw CI8 and metadata. */
#ifndef NGENE_RECORDING_H
#define NGENE_RECORDING_H
#include "ngene/config.h"
#include "ngene/frontend.h"
/** @brief Opaque owned TNgRecorder handle; release with the matching destroy operation. */
typedef struct TNgRecorder TNgRecorder;

/** @brief Copied disk-writer counters for one capture session. */
typedef struct {
    /** Number of complete windows processed or written, according to the object. */
    uint64_t windows;
    /** Borrowed complex-pair array; lifetime follows the enclosing view contract. */
    uint64_t samples;
    /** Number of raw CI8 segment files opened by the writer. */
    uint64_t segments;
    /** Complex pairs currently awaiting consumption or disk writing. */
    uint64_t queuedSamples;
} TNgRecordingStats;

/**
 * @brief Create exclusive capture files and start the asynchronous writer.
 *
 * @details
 * Creates raw CI8, sample index and metadata files without replacing existing files. Copies
 * stream metadata as needed; the recorder owns a bounded queue. Finish is mandatory to flush
 * and release resources.
 *
 * @param config Validated recording settings including receiver-side directory and prefix.
 * @param info Stream metadata describing captured samples.
 * @param conditioningName Conditioner label written to metadata.
 * @param out Receives caller-owned recorder on success.
 * @param error Optional file/allocation/thread error destination.
 *
 * @return NG_OK on success; NG_ERROR otherwise.
 */
int NgRecorderOpen(const TNgConfig *config, const TNgStreamInfo *info, const char *conditioningName,
                   TNgRecorder **out, TNgError *error);
/**
 * @brief Copy a whole window into the bounded recorder queue.
 *
 * @details
 * One sample producer may submit while the writer consumes. Does not perform filesystem I/O.
 * Full or failed recorder queues must stop capture; silent recording loss is not permitted.
 *
 * @param recorder Live recorder; protect lifetime against concurrent finish.
 * @param window Borrowed read-only CI8 window copied during this call.
 *
 * @return NG_OK if copied; NG_FULL if the queue is full; NG_ERROR if writer failed or input is
 * invalid.
 */
int NgRecorderSubmit(TNgRecorder *recorder, const TNgIqView *window);
/**
 * @brief Copy recorder progress while protecting its counters.
 *
 * @details
 * The owner must prevent concurrent recorder destruction. Counts describe written blocks;
 * queue occupancy is sampled separately and is not an atomic multi-counter transaction.
 *
 * @param recorder Live recorder whose lifetime is protected by caller.
 * @param stats Receives progress counters in windows, complex pairs and segments.
 */
void NgRecorderSnapshot(TNgRecorder *recorder, TNgRecordingStats *stats);
/**
 * @brief Drain accepted windows, join the writer and release the recorder.
 *
 * @details
 * Stop submitting first. May block on filesystem I/O. Closes all files and appends final
 * completion/loss metadata; frees the recorder even if finalization fails. A NULL recorder is
 * accepted.
 *
 * @param recorder Recorder to finish; invalid after return.
 * @param successfulRun Nonzero if the receiver run completed without upstream failure.
 * @param sourceStats Optional final frontend loss/capture counters.
 * @param stats Optional destination for final written counters.
 * @param error Optional error destination.
 *
 * @return NG_OK if finalization succeeds; NG_ERROR for writer/flush/close failures.
 */
int NgRecorderFinish(TNgRecorder *recorder, int successfulRun, const TNgFrontendStats *sourceStats,
                     TNgRecordingStats *stats, TNgError *error);
#endif
