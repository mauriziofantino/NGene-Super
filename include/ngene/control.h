/** @file control.h @brief Thread-safe lifecycle control, independent of HTTP and GUI. */
#ifndef NGENE_CONTROL_H
#define NGENE_CONTROL_H
#include "ngene/receiver.h"

/** @brief Opaque owned TNgControl handle; release with the matching destroy operation. */
typedef struct TNgControl TNgControl;

/** @brief Receiver service lifecycle states. */
typedef enum { NG_STOPPED, NG_STARTING, NG_RUNNING, NG_STOPPING, NG_FAILED } TNgRunState;

/** @brief Independent disk-capture lifecycle states. */
typedef enum { NG_REC_OFF, NG_REC_STARTING, NG_REC_ACTIVE, NG_REC_STOPPING } TNgRecordState;

/** @brief Asynchronous lifecycle commands accepted by a controller. */
typedef enum { NG_CMD_START = 1, NG_CMD_STOP, NG_CMD_RECORD_START, NG_CMD_RECORD_STOP } TNgCommand;

/** @brief Owned controller snapshot; no live sample pointers or file handles. */
typedef struct {
    /** Current receiver/channel lifecycle state. */
    TNgRunState state;
    /** Disk capture lifecycle, independent of receiver run state. */
    TNgRecordState recordingState;
    /** Configuration revision associated with this immutable snapshot. */
    uint64_t revision;
    /** Revision frozen when the current receiver run started. */
    uint64_t activeRevision;
    /** Latest accepted lifecycle command identifier. */
    uint64_t commandId;
    /** Latest completed lifecycle command identifier. */
    uint64_t completedCommandId;
    /** Identifier increased for each newly accepted receiver run. */
    uint64_t runId;
    /** Nonzero if the completed command succeeded. */
    int commandOk;
    /** Copied common-engine counters. */
    TNgReceiverStats receiver;
    /** Copied disk-writer statistics; not a borrowed recorder handle. */
    TNgRecordingStats recording;
    /** Optional diagnostic settings or an owned diagnostic snapshot. */
    TNgAnalysisSnapshot analysis;
    /** Copied GPS search outcomes; not tracking locks or navigation observables. */
    TNgAcquisitionSnapshot acquisition;
    /** Cumulative written pairs including completed capture sessions. */
    uint64_t totalWrittenSamples;
    /** Baseband filter bandwidth applied by the source, in hertz. */
    uint32_t actualBandwidthHz;
    /** Complex sample rate reported by the opened frontend, in pairs per second. */
    uint32_t actualSampleRateHz;
    /** Native sample representation applied by the active frontend. */
    TNgSampleFormat sampleFormat;
    TNgSampleFormat nativeFormat;
    unsigned adcBits;
    /** Applied SDR tuning frequency in hertz. */
    uint64_t hardwareFrequencyHz;
    /** Requested signal centre in hertz; independent of SDR tuning. */
    uint64_t signalFrequencyHz;
    /** Signal frequency minus hardware frequency in hertz; samples are not shifted. */
    int64_t digitalIfHz;
    /** Bounded NUL-terminated diagnostic text; owned by this object. */
    char message[256];
    /** Active capture basename; empty when no recording session exists. */
    char recordingName[160];
    /** Absolute receiver-side capture directory for this session. */
    char outputDirectory[320];
} TNgControlStatus;

/**
 * @brief Validate a web-controllable receiver profile and capture label.
 *
 * @details
 * Adds safe recording-prefix restrictions to the core INI checks. No file or device operation
 * occurs.
 *
 * @param config Configuration to check.
 * @param error Optional error destination.
 *
 * @return NG_OK if valid; NG_ERROR otherwise.
 */
int NgControlValidate(const TNgConfig *config, TNgError *error);

/**
 * @brief Create a lifecycle controller and its manager thread.
 *
 * @details
 * Validates config, prepares/normalizes profile and capture directories, and starts the
 * manager. It does not start the receiver. The caller owns the controller and must destroy it
 * after the web/service layer stops.
 *
 * @param config Initial validated receiver settings.
 * @param outputDirectory Profile directory; parents must exist. Capture directory may be
 * separately configured.
 * @param out Receives owned controller on success.
 * @param error Optional error destination.
 *
 * @return NG_OK on success; NG_ERROR for validation, directory, allocation or synchronization
 * errors.
 */
int NgControlCreate(const TNgConfig *config, const char *outputDirectory, TNgControl **out,
                    TNgError *error);
/**
 * @brief Apply optional diagnostic parameters without restarting reception.
 *
 * @details
 * Uses revision checking and rejects start/stop transitions. Copies settings into
 * controller/worker and invalidates older analysis results. Persist the full profile
 * separately while stopped.
 *
 * @param control Live controller.
 * @param config New diagnostic settings.
 * @param expectedRevision Expected current profile revision.
 * @param error Optional error destination.
 *
 * @return NG_OK on success; NG_ERROR for invalid settings, revision or transition.
 */
int NgControlAnalysisConfigure(TNgControl *control, const TNgAnalysisConfig *config,
                               uint64_t expectedRevision, TNgError *error);
/**
 * @brief Stop and join receiver/manager activity and free the controller.
 *
 * @details
 * The web layer and other control clients must have stopped calling the controller. Finishes
 * any recording and joins diagnostics before releasing resources. NULL is accepted.
 *
 * @param control Controller to destroy; invalid after return.
 */
void NgControlDestroy(TNgControl *control);
/**
 * @brief Queue one asynchronous lifecycle command.
 *
 * @details
 * Thread-safe with status readers. Uses a revision guard and rejects conflicting transitions
 * or pending commands. Completion is reported later through command IDs in snapshots; stop
 * completion includes writer flush.
 *
 * @param control Live controller.
 * @param command Start/stop receiver or start/stop recording command.
 * @param revision Expected current profile revision.
 * @param commandId Receives the accepted command identifier.
 * @param error Optional conflict/error destination.
 *
 * @return NG_OK when queued; NG_ERROR for stale revision, unsupported transition or an already
 * pending command.
 */
int NgControlRequest(TNgControl *control, TNgCommand command, uint64_t revision,
                     uint64_t *commandId, TNgError *error);
/**
 * @brief Renew a two-second consumer lease on optional diagnostics.
 *
 * @details
 * Thread-safe; subscriptions from independent clients combine. Does not stop receiver, channel
 * dispatch or recording when plots become dormant.
 *
 * @param control Live controller.
 * @param mask Mask bits: 1 PSD, 2 histogram.
 */
void NgControlDiagnosticInterest(TNgControl *control, unsigned mask);
/**
 * @brief Replace selected diagnostic deadlines after aggregating interested clients.
 * @details Thread-safe; callers aggregate independent subscriptions before replacing a lane.
 * Zero revokes selected lanes immediately without changing receiver dispatch or recording.
 * @param control Live controller.
 * @param mask Selected lanes: 1 PSD, 2 histogram.
 * @param expiresSeconds Absolute monotonic lease deadline; zero revokes selected lanes.
 */
void NgControlDiagnosticInterestUntil(TNgControl *control, unsigned mask, double expiresSeconds);
/**
 * @brief Copy coherent lifecycle status and owned diagnostic data.
 *
 * @details
 * No borrowed sample storage escapes. The caller may serialize the snapshot after return.
 * Protect controller lifetime; JSON/network I/O occurs outside its locks.
 *
 * @param control Live controller.
 * @param status Receives a by-value status snapshot.
 */
void NgControlSnapshot(TNgControl *control, TNgControlStatus *status);
/**
 * @brief Copy the current receiver configuration and revision.
 *
 * @details
 * Thread-safe read; changes are applied through configure APIs, not through this returned
 * copy.
 *
 * @param control Live controller.
 * @param config Receives owned configuration copy.
 * @param revision Receives current profile revision.
 */
void NgControlConfig(TNgControl *control, TNgConfig *config, uint64_t *revision);
/**
 * @brief Validate and atomically persist a new stopped receiver profile.
 *
 * @details
 * Requires receiver stopped/error with no conflicting command and matching revision. Prepares
 * capture directory, writes a temporary INI and atomically replaces the profile before
 * publishing config. May perform filesystem I/O.
 *
 * @param control Live controller.
 * @param config Complete settings to copy and persist.
 * @param expectedRevision Expected current profile revision.
 * @param error Optional validation/file/conflict error destination.
 *
 * @return NG_OK on success; NG_ERROR otherwise.
 */
int NgControlConfigure(TNgControl *control, const TNgConfig *config, uint64_t expectedRevision,
                       TNgError *error);
/**
 * @brief Delete one confirmed closed capture file in the selected receiver directory.
 *
 * @details
 * Requires matching profile revision. Rejects traversal, unsupported suffixes,
 * symlinks/nonregular files and active capture names. Serializes directory/state checks with
 * configuration and recorder publication; sample dispatch does not acquire this mutex. Deletes
 * one file only, leaving related files untouched.
 *
 * @param control Live controller.
 * @param name Immediate filename with .cs8, .index.csv or .meta.ini suffix; no directories.
 * @param expectedRevision Revision returned by the capture-file list.
 * @param error Optional error destination.
 *
 * @return NG_OK if removed; NG_FULL for stale revision/active recording; NG_END if absent;
 * NG_ERROR for invalid name or filesystem failure.
 */
int NgControlDeleteFile(TNgControl *control, const char *name, uint64_t expectedRevision,
                        TNgError *error);
#endif
