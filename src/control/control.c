/** @file control.c @brief Serialize lifecycle work away from the sample path. */
#include "ngene/control.h"
#include "platform/platform.h"
#include "platform/files.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

/** @brief One receiver engine, command manager and optional recorder/diagnostic worker. */
struct TNgControl {
    /** Protects shared state; never perform transport or disk I/O while holding it. */
    TNgMutex mutex;
    /** Protects recorder lifetime/publication separately from service status. */
    TNgMutex recorderMutex;
    /** Owned command-manager thread; joins the receiver worker. */
    TNgThread manager;
    /** Owned receiver-run thread handle. */
    TNgThread worker;
    /** Atomic service shutdown request. */
    TNgAtomicInt terminate;
    /** Atomic request for the worker/engine to stop. */
    TNgAtomicInt stop;
    /** Atomic publication that the receiver worker has completed. */
    TNgAtomicInt done;
    /** Manager-owned flag recording whether worker requires a join. */
    int workerLive;
    /** Worker result published before done. */
    int workerResult;
    /** Sample-side record failure communicated to the command manager. */
    int fatalRecordError;
    /** Nonzero while an asynchronous lifecycle command is outstanding. */
    int busy;
    /** Pending lifecycle command protected by mutex. */
    TNgCommand pending;
    /** Copied validated configuration; owned by this instance. */
    TNgConfig config;
    /** Startup configuration frozen for the active run. */
    TNgConfig runConfig;
    /** Owned copied lifecycle snapshot protected by the controller mutex. */
    TNgControlStatus status;
    /** Owned current disk writer; lifetime protected by recorderMutex. */
    TNgRecorder *recorder;
    /** Owned optional diagnostic worker; destroyed after the receiver stops. */
    TNgAnalysis *analysis;
    /** Run-owned module adapter; publication/lifetime protected by mutex. */
    TNgAcquisitionReceiver *acquisition;
    /** Last aggregate CAF consumer deadline, also applied to a newly started run. */
    double cafInterestUntil;
    /** Bounded measured surfaces retained after replay EOF or manual stop. */
    TNgAcquisitionCaf *cafArchive;
    unsigned cafArchiveLatestPrn;
    /** Cumulative pairs from completed recording sessions. */
    uint64_t completedWritten;
    /** Copied rate/RF metadata; identity strings point into controller-owned storage. */
    TNgStreamInfo info;
    /** Absolute receiver-side profile location; independent of capture directory. */
    char profileDirectory[320];
    /** Owned copy of frontend source name. */
    char source[64];
    /** Owned copy of frontend clock-domain name. */
    char clock[96];
    /** Owned copy of frontend device identity. */
    char device[128];
    /** Worker result counters, published before done. */
    TNgReceiverStats finalStats;
    /** Worker diagnostic, published before done. */
    TNgError workerError;
};

/**
 * @brief Check a capture prefix without directory components.
 *
 * @details
 * Allows 1..64 ASCII letters, digits, underscores and hyphens; the path is configured
 * separately.
 *
 * @param label NUL-terminated proposed filename prefix.
 *
 * @return Nonzero for an accepted label, zero otherwise.
 */
static int SafeLabel(const char *label)
{
    size_t k;
    size_t length = strlen(label);
    if (length == 0 || length > 64) {
        return 0;
    }
    for (k = 0; k < length; ++k) {
        char ch = label[k];
        if (((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
             ch == '-' || ch == '_') == 0) {
            return 0;
        }
    }
    return 1;
}

/**
 * @brief Delete one confirmed closed capture file in the selected receiver directory.
 *
 * @details
 * Requires matching profile expectedRevision. Rejects traversal, unsupported suffixes,
 * symlinks/nonregular files and active capture names. Serializes directory/state checks with
 * configuration and recorder publication; sample dispatch does not acquire this mutex. Deletes
 * one file only, leaving related files untouched.
 *
 * Parameter control: Live controller.
 * Parameter name: Immediate filename with .cs8, .index.csv or .meta.ini suffix; no directories.
 * Parameter expectedRevision: Revision returned by the capture-file list.
 * Parameter error: Optional error destination.
 *
 * Result: NG_OK if removed; NG_FULL for stale expectedRevision/active recording; NG_END if absent;
 * NG_ERROR for invalid name or filesystem failure.
 */
int NgControlDeleteFile(TNgControl *control, const char *name, uint64_t expectedRevision,
                        TNgError *error)
{
    size_t k;
    size_t n;
    size_t active;
    int result;
    if (name == NULL || (n = strlen(name)) == 0 || n >= 240 || name[0] == '.' ||
        strstr(name, "..") != NULL) {
        return NgFail(error, "Invalid capture filename");
    }
    for (k = 0; k < n; ++k) {
        if (((name[k] >= 'a' && name[k] <= 'z') || (name[k] >= 'A' && name[k] <= 'Z') ||
             (name[k] >= '0' && name[k] <= '9') || name[k] == '_' || name[k] == '-' ||
             name[k] == '.') == 0) {
            return NgFail(error, "Invalid capture filename");
        }
    }
    if (((n > 4 && (strcmp(name + n - 4, ".cs8") == 0 || strcmp(name + n - 4, ".cu8") == 0)) ||
         (n > 8 && strcmp(name + n - 8, ".sc16q11") == 0) ||
         (n > 9 && strcmp(name + n - 9, ".meta.ini") == 0) ||
         (n > 10 && strcmp(name + n - 10, ".index.csv") == 0)) == 0) {
        return NgFail(error, "Only capture data, index and metadata files can be deleted");
    }
    NgMutexLock(&control->mutex);
    active = strlen(control->status.recordingName);
    if (expectedRevision != control->status.revision ||
        control->status.recordingState == NG_REC_STARTING ||
        (control->status.recordingState != NG_REC_OFF && active > 0 && n > active &&
         strncmp(name, control->status.recordingName, active) == 0 &&
         (name[active] == '.' || name[active] == '_'))) {
        NgMutexUnlock(&control->mutex);
        NgFail(error,
               "Refresh files after a profile change; files still being written cannot be deleted");
        return NG_FULL;
    }
    /* Serialized with configuration and recorder publication. Sample hooks never need this
     * mutex for data dispatch; progress may skip while filesystem removal runs. */
    result = NgDirectoryRemoveFile(control->status.outputDirectory, name);
    NgMutexUnlock(&control->mutex);
    if (result == 1) {
        NgFail(error, "Capture file not found");
        return NG_END;
    }
    return result == 0
               ? NG_OK
               : NgFail(error,
                        "Cannot delete file: not regular, access denied or filesystem error");
}

/**
 * @brief Publish completion of the pending lifecycle command.
 *
 * @details
 * Caller holds controller mutex. Copies the message, records command ID/success and clears
 * busy; does not send telemetry or perform I/O.
 *
 * @param c Locked controller.
 * @param ok Nonzero for successful completion.
 * @param message NUL-terminated completion message copied into status.
 */
static void CommandResult(TNgControl *c, int ok, const char *message)
{
    c->status.completedCommandId = c->status.commandId;
    c->status.commandOk = ok;
    snprintf(c->status.message, sizeof(c->status.message), "%s", message);
    c->busy = 0;
}

/**
 * @brief Create a unique capture session and publish its recorder between windows.
 *
 * @details
 * Only manager or receiver startup may call. Opens/allocates outside block callbacks,
 * generates a random suffix and publishes under controller then recorder lock.
 *
 * @param c Controller with stable run configuration and stream metadata.
 * @param error Optional error destination.
 *
 * @return NG_OK on success; NG_ERROR otherwise.
 */
static int OpenRecording(TNgControl *c, TNgError *error)
{
    TNgConfig config = c->runConfig;
    TNgRecorder *recorder = NULL;
    unsigned char random[8];
    char suffix[17];
    char name[160];
    size_t k;
    if (NgRandomBytes(random, sizeof(random)) != 0) {
        return NgFail(error, "Cannot generate recording identifier");
    }
    for (k = 0; k < sizeof(random); ++k) {
        snprintf(suffix + 2 * k, 3, "%02x", random[k]);
    }
    snprintf(name, sizeof(name), "%.64s_%" PRId64 "_%s", config.recordingPrefix,
             (int64_t)time(NULL), suffix);
    snprintf(config.recordingPrefix, sizeof(config.recordingPrefix), "%s", name);
    config.recordingEnabled = 1;
    if (NgRecorderOpen(&config, &c->info, (config.dcRemoval) != 0 ? "dc_iir" : "bypass", &recorder,
                       error) != NG_OK) {
        return NG_ERROR;
    }
    NgMutexLock(&c->mutex);
    NgMutexLock(&c->recorderMutex);
    c->recorder = recorder;
    NgMutexUnlock(&c->recorderMutex);
    memset(&c->status.recording, 0, sizeof(c->status.recording));
    snprintf(c->status.recordingName, sizeof(c->status.recordingName), "%s", name);
    c->status.recordingState = NG_REC_ACTIVE;
    NgMutexUnlock(&c->mutex);
    return NG_OK;
}

/**
 * @brief Detach a recorder between windows and finish its pending writes.
 *
 * @details
 * Called by manager after coordinated lifetime access. Detaches under controller/recorder
 * locks, drains outside locks, then updates counters. No-op if no recorder.
 *
 * @param c Controller.
 * @param successful Nonzero if run/session ended successfully.
 * @param source Optional final frontend counters.
 * @param error Optional error destination.
 *
 * @return NG_OK on successful close/no recorder; NG_ERROR on finalization failure.
 */
static int CloseRecording(TNgControl *c, int successful, const TNgFrontendStats *source,
                          TNgError *error)
{
    TNgRecorder *recorder;
    TNgRecordingStats stats = {0};
    int result;
    NgMutexLock(&c->mutex);
    NgMutexLock(&c->recorderMutex);
    recorder = c->recorder;
    /* This lock is also held by the block callback. Detaching is therefore
     * between whole windows, never halfway through an I/Q block. */
    c->recorder = NULL;
    if (recorder != NULL) {
        NgRecorderSnapshot(recorder, &c->status.recording);
        c->status.recordingState = NG_REC_STOPPING;
    }
    NgMutexUnlock(&c->recorderMutex);
    NgMutexUnlock(&c->mutex);
    if (recorder == NULL) {
        return NG_OK;
    }
    result = NgRecorderFinish(recorder, successful, source, &stats, error);
    NgMutexLock(&c->mutex);
    c->status.recording = stats;
    c->completedWritten += stats.samples;
    c->status.totalWrittenSamples = c->completedWritten;
    c->status.recordingState = NG_REC_OFF;
    NgMutexUnlock(&c->mutex);
    return result;
}

/**
 * @brief Read the controller stop flag for the engine predicate.
 *
 * @details
 * No blocking lock or I/O. Context remains valid for the receiver thread.
 *
 * @param context Controller predicate context.
 *
 * @return Nonzero when the engine should stop.
 */
static int StopRequested(void *context)
{
    return NgAtomicGet(&((TNgControl *)context)->stop);
}

/**
 * @brief Publish successful receiver startup and applied stream metadata.
 *
 * @details
 * Engine startup hook; takes controller mutex briefly. Completes Start only if the lifecycle
 * is still Starting.
 *
 * @param context Controller.
 * @param info Borrowed applied stream metadata copied into status.
 */
static void Started(void *context, const TNgStreamInfo *info)
{
    TNgControl *c = context;
    NgMutexLock(&c->mutex);
    c->status.actualBandwidthHz = info->bandwidthHz;
    c->status.actualSampleRateHz = info->sampleRateHz;
    c->status.sampleFormat = info->format;
    c->status.hardwareFrequencyHz = info->centerFrequencyHz;
    c->status.signalFrequencyHz = info->signalFrequencyHz;
    c->status.digitalIfHz = info->digitalIfHz;
    if (c->status.state == NG_STARTING) {
        c->status.state = NG_RUNNING;
        CommandResult(c, 1, "Receiver running");
    }
    NgMutexUnlock(&c->mutex);
}

/**
 * @brief Offer a conditioned window to diagnostics and the active recorder.
 *
 * @details
 * Synchronous engine hook. Analysis uses try-lock; recorder lifetime is protected by its
 * separate mutex. Copies only, no file or network I/O. A recorder error stops capture.
 *
 * @param context Controller with live recorder/analysis ownership.
 * @param view Borrowed post-conditioning view; never retained.
 * @param error Optional recorder error destination.
 *
 * @return NG_OK normally; NG_ERROR when recorder submit fails.
 */
static int Block(void *context, const TNgIqView *view, TNgError *error)
{
    TNgControl *c = context;
    int result = NG_OK;
    /* Lifetime: analysis is published before thread start, destroyed only after join. */
    NgAnalysisSubmitAt(c->analysis, view, 0);
    NgMutexLock(&c->recorderMutex);
    if (c->recorder != NULL && c->runConfig.recordingPreConditioning == 0 &&
        NgRecorderSubmit(c->recorder, view) != NG_OK) {
        result = NgFail(error, "Recorder queue full or writer failed; receiver stopped");
    }
    NgMutexUnlock(&c->recorderMutex);
    return result;
}

/**
 * @brief Offer the original frontend window to selected pre-conditioning taps.
 *
 * @details
 * Runs before conditioning. Analysis uses try-lock; only a pre-selected recorder receives a
 * copy. No file or network I/O.
 *
 * @param context Controller with live recorder/analysis ownership.
 * @param view Borrowed pre-conditioning view; never retained.
 * @param error Optional recorder error destination.
 *
 * @return NG_OK normally; NG_ERROR when recorder submit fails.
 */
static int RawBlock(void *context, const TNgIqView *view, TNgError *error)
{
    TNgControl *c = context;
    int result = NG_OK;
    NgAnalysisSubmitAt(c->analysis, view, 1);
    NgMutexLock(&c->recorderMutex);
    if (c->recorder != NULL && c->runConfig.recordingPreConditioning != 0 &&
        NgRecorderSubmit(c->recorder, view) != NG_OK) {
        result = NgFail(error, "Pre-conditioning recorder queue full or writer failed");
    }
    NgMutexUnlock(&c->recorderMutex);
    return result;
}

/**
 * @brief Publish optional receiver counters without waiting for GUI locks.
 *
 * @details
 * Try-lock skips this status update if control is busy. Sample processing and recording
 * continue regardless.
 *
 * @param context Controller.
 * @param stats Borrowed engine counters copied if the lock is available.
 */
static void Progress(void *context, const TNgReceiverStats *stats)
{
    TNgControl *c = context;
    if (NgMutexTryLock(&c->mutex) != 0) {
        c->status.receiver = *stats;
        NgMutexUnlock(&c->mutex);
    }
}

/**
 * @brief Open frontend/conditioner and run the shared-stream engine.
 *
 * @details
 * Owns run-local hardware and conditioner instances, installs controller hooks, cleans up
 * after engine return and publishes final result through the done flag.
 *
 * @param context Controller retained until worker join.
 */
static void ReceiverWorker(void *context)
{
    TNgControl *c = context;
    TNgFrontend source;
    TNgConditioner conditioner = NgConditionerBypass();
    TNgAcquisitionReceiver *acquisition = NULL;
    TNgChannel *channels = NULL;
    size_t channelCount = 0;
    TNgReceiverExecution execution = {1};
    TNgReceiverHooks hooks = {c, 1, Started, Block, Progress, RawBlock};
    memset(&source, 0, sizeof(source));
    memset(&c->finalStats, 0, sizeof(c->finalStats));
    memset(&c->workerError, 0, sizeof(c->workerError));
    c->workerResult = NgFrontendOpen(&c->runConfig, &source, &c->workerError);
    if (c->workerResult == NG_OK) {
        if (c->runConfig.dcRemoval != 0 &&
            NgConditionerDcCreateFormat(c->runConfig.windowSamples, c->runConfig.dcShift,
                                        source.info.format, &conditioner,
                                        &c->workerError) != NG_OK) {
            NgFrontendDestroy(&source);
            c->workerResult = NG_ERROR;
            NgAtomicSet(&c->done, 1);
            return;
        }
        (void)NgAnalysisSetSampleRate(c->analysis, source.info.sampleRateHz);
        /* Strings outlive source destruction while recorder headers are written. */
        snprintf(c->source, sizeof(c->source), "%s", source.info.sourceName);
        snprintf(c->clock, sizeof(c->clock), "%s", source.info.clockDomain);
        snprintf(c->device, sizeof(c->device), "%s",
                 (source.info.deviceIdentifier) != NULL ? source.info.deviceIdentifier
                                                        : "unavailable");
        c->info = source.info;
        c->info.sourceName = c->source;
        c->info.clockDomain = c->clock;
        c->info.deviceIdentifier = c->device;
        if (c->runConfig.recordingEnabled != 0 && NgAtomicGet(&c->stop) == 0) {
            c->workerResult = OpenRecording(c, &c->workerError);
        }
        if (c->workerResult == NG_OK && NgAtomicGet(&c->stop) == 0) {
            if (!c->runConfig.recordOnly) {
                c->workerResult = NgAcquisitionReceiverCreate(&c->runConfig.acquisition, &source.info,
                                                              &acquisition, &c->workerError);
            }
            channels = NgAcquisitionReceiverChannels(acquisition, &channelCount);
            execution = NgAcquisitionReceiverExecution(acquisition);
            NgMutexLock(&c->mutex);
            c->acquisition = acquisition;
            NgAcquisitionReceiverCafInterestUntil(acquisition, c->cafInterestUntil);
            NgMutexUnlock(&c->mutex);
        }
        if (c->workerResult == NG_OK && NgAtomicGet(&c->stop) == 0) {
            c->workerResult =
                NgReceiverRunScheduled(&c->runConfig, &source, &conditioner, channels, channelCount,
                                      StopRequested, c, &c->finalStats, &execution, &hooks,
                                      &c->workerError);
        }
        NgMutexLock(&c->mutex);
        NgAcquisitionReceiverRead(acquisition, &c->status.acquisition);
        if (acquisition != NULL && c->cafArchive != NULL) {
            TNgAcquisitionCaf newest;
            unsigned prn;
            NgAcquisitionReceiverReadCaf(acquisition, 0, &newest);
            c->cafArchiveLatestPrn = newest.available ? newest.prn : 0;
            for (prn = 1; prn <= 32u; ++prn)
                NgAcquisitionReceiverReadCaf(acquisition, prn, &c->cafArchive[prn - 1u]);
        }
        c->acquisition = NULL;
        NgMutexUnlock(&c->mutex);
        NgAcquisitionReceiverDestroy(acquisition);
        NgFrontendDestroy(&source);
    }
    NgConditionerDcDestroy(&conditioner);
    NgAtomicSet(&c->done, 1);
}

/**
 * @brief Join a completed receiver run and finish its dependent resources.
 *
 * @details
 * Manager-only. Finishes recording, joins/destroys diagnostics, publishes final counters/state
 * and acknowledges pending Stop only after cleanup.
 *
 * @param c Controller with a completed live worker.
 */
static void FinishRun(TNgControl *c)
{
    TNgError error = {{0}};
    int ok;
    TNgAnalysis *analysis;
    NgThreadJoin(c->worker);
    NgMutexLock(&c->mutex);
    analysis = c->analysis;
    if (analysis != NULL) {
        NgAnalysisRead(analysis, &c->status.analysis);
    }
    c->analysis = NULL;
    NgMutexUnlock(&c->mutex);
    NgAnalysisDestroy(analysis);
    c->workerLive = 0;
    ok = c->workerResult == NG_OK && !c->fatalRecordError;
    if (CloseRecording(c, ok, &c->finalStats.frontend, &error) != NG_OK) {
        ok = 0;
    }
    NgMutexLock(&c->mutex);
    c->status.receiver = c->finalStats;
    c->status.state = (ok) != 0 ? NG_STOPPED : NG_FAILED;
    c->status.recordingState = NG_REC_OFF;
    if (ok == 0 && c->workerError.message[0] != 0) {
        snprintf(c->status.message, sizeof(c->status.message), "%s", c->workerError.message);
    } else if (ok == 0 && error.message[0] != 0) {
        snprintf(c->status.message, sizeof(c->status.message), "%s", error.message);
    } else if (ok != 0) {
        snprintf(c->status.message, sizeof(c->status.message),
                 "Receiver stopped; accepted recording data flushed");
    }
    c->status.completedCommandId = c->status.commandId;
    c->status.commandOk = ok;
    if (ok != 0 && c->pending == NG_CMD_RECORD_START) {
        c->status.commandOk = 0;
        snprintf(c->status.message, sizeof(c->status.message),
                 "Receiver ended before recording could start");
    }
    c->busy = 0;
    /* A pending command cannot target a run that has already reached EOF. */
    c->pending = 0;
    NgMutexUnlock(&c->mutex);
}

/**
 * @brief Serialize lifecycle transitions outside HTTP and sample callbacks.
 *
 * @details
 * Dispatches one pending command, joins completed runs, opens/closes recordings, samples
 * recorder counters and polls termination. Controller owns the thread.
 *
 * @param context Controller valid until manager join.
 */
static void Manager(void *context)
{
    TNgControl *c = context;
    for (;;) {
        TNgCommand command;
        TNgError error = {{0}};
        int result = NG_OK;
        if (c->workerLive != 0 && NgAtomicGet(&c->done) != 0) {
            FinishRun(c);
        }
        if (NgAtomicGet(&c->terminate) != 0) {
            NgAtomicSet(&c->stop, 1);
            if (c->workerLive == 0) {
                break;
            }
            NgSleepMs(10);
            continue;
        }
        NgMutexLock(&c->mutex);
        command = c->pending;
        c->pending = 0;
        NgMutexUnlock(&c->mutex);
        if (command == NG_CMD_START) {
            NgAtomicSet(&c->done, 0);
            c->fatalRecordError = 0;
            c->completedWritten = 0;
            TNgAnalysis *analysis = NULL;
            if (NgAnalysisCreate(c->runConfig.sampleRateHz, &c->runConfig.analysis, &analysis,
                                 &error) != NG_OK) {
                NgMutexLock(&c->mutex);
                c->status.state = NG_FAILED;
                CommandResult(c, 0, error.message);
                NgMutexUnlock(&c->mutex);
                continue;
            }
            NgMutexLock(&c->mutex);
            NgAnalysisInterest(analysis, 3, 0); /* Dormant until a diagnostic client subscribes. */
            c->analysis = analysis;
            NgMutexUnlock(&c->mutex);
            if (NgThreadStart(&c->worker, ReceiverWorker, c) != 0) {
                NgMutexLock(&c->mutex);
                c->analysis = NULL;
                c->status.state = NG_FAILED;
                CommandResult(c, 0, "Cannot create receiver thread");
                NgMutexUnlock(&c->mutex);
                NgAnalysisDestroy(analysis);
            } else {
                c->workerLive = 1;
            }
        } else if (command == NG_CMD_STOP) {
            NgAtomicSet(&c->stop, 1); /* Completion is acknowledged only after join/flush. */
        } else if (command == NG_CMD_RECORD_START) {
            result = OpenRecording(c, &error);
            NgMutexLock(&c->mutex);
            if (result != NG_OK) {
                c->status.recordingState = NG_REC_OFF;
            }
            CommandResult(c, result == NG_OK,
                          result == NG_OK ? "Recording started" : error.message);
            NgMutexUnlock(&c->mutex);
        } else if (command == NG_CMD_RECORD_STOP) {
            result = CloseRecording(c, 1, NULL, &error);
            NgMutexLock(&c->mutex);
            CommandResult(c, result == NG_OK,
                          result == NG_OK ? "Recording closed; receiver still running"
                                          : error.message);
            if (result != NG_OK) {
                c->fatalRecordError = 1;
                c->status.state = NG_STOPPING;
                NgAtomicSet(&c->stop, 1);
            }
            NgMutexUnlock(&c->mutex);
        }
        {
            TNgRecordingStats recording = {0};
            int active;
            NgMutexLock(&c->recorderMutex);
            active = c->recorder != NULL;
            if (active != 0) {
                NgRecorderSnapshot(c->recorder, &recording);
            }
            NgMutexUnlock(&c->recorderMutex);
            if (active != 0) {
                NgMutexLock(&c->mutex);
                c->status.recording = recording;
                c->status.totalWrittenSamples = c->completedWritten + recording.samples;
                NgMutexUnlock(&c->mutex);
            }
        }
        NgSleepMs(10);
    }
}

/**
 * @brief Validate a web-controllable receiver profile and capture label.
 *
 * @details
 * Adds safe recording-prefix restrictions to the core INI checks. No file or device operation
 * occurs.
 *
 * Parameter config: Configuration to check.
 * Parameter error: Optional error destination.
 *
 * Result: NG_OK if valid; NG_ERROR otherwise.
 */
int NgControlValidate(const TNgConfig *config, TNgError *error)
{
    if (NgConfigValidate(config, error) != NG_OK) {
        return NG_ERROR;
    }
    if (SafeLabel(config->recordingPrefix) == 0) {
        return NgFail(error, "Web recording prefix: 1..64 letters, digits, _ or -. Select the "
                             "recording.directory separately");
    }
    return NG_OK;
}

/**
 * @brief Create a lifecycle controller and its manager thread.
 *
 * @details
 * Validates config, prepares/normalizes profile and capture directories, and starts the
 * manager. It does not start the receiver. The caller owns the controller and must destroy it
 * after the web/service layer stops.
 *
 * Parameter config: Initial validated receiver settings.
 * Parameter outputDirectory: Profile directory; parents must exist. Capture directory may be
 * separately configured. Parameter out: Receives owned controller on success. Parameter error:
 * Optional error destination.
 *
 * Result: NG_OK on success; NG_ERROR for validation, directory, allocation or synchronization
 * errors.
 */
int NgControlCreate(const TNgConfig *config, const char *outputDirectory, TNgControl **out,
                    TNgError *error)
{
    TNgControl *c;
    if (out == NULL || NgControlValidate(config, error) != NG_OK) {
        return NG_ERROR;
    }
    *out = NULL;
    c = calloc(1, sizeof(*c));
    if (c == NULL) {
        return NgFail(error, "Cannot allocate controller");
    }
    if (NgDirectoryPrepare(outputDirectory, c->status.outputDirectory,
                           sizeof(c->status.outputDirectory)) != 0 ||
        strlen(c->status.outputDirectory) > 240) {
        free(c);
        return NgFail(error, "Cannot create/resolve output directory (maximum 240 bytes)");
    }
    snprintf(c->profileDirectory, sizeof(c->profileDirectory), "%s", c->status.outputDirectory);
    if (config->recordingDirectory[0] != 0 &&
        (NgDirectoryPrepare(config->recordingDirectory, c->status.outputDirectory,
                            sizeof(c->status.outputDirectory)) != 0 ||
         strlen(c->status.outputDirectory) > 240)) {
        free(c);
        return NgFail(error,
                      "Cannot resolve recording directory (maximum 240 bytes; parent must exist)");
    }
    if (NgMutexInit(&c->mutex) != 0) {
        free(c);
        return NgFail(error, "Cannot initialize controller");
    }
    if (NgMutexInit(&c->recorderMutex) != 0) {
        NgMutexDestroy(&c->mutex);
        free(c);
        return NgFail(error, "Cannot initialize recorder lock");
    }
    c->config = *config;
    snprintf(c->config.recordingDirectory, sizeof(c->config.recordingDirectory), "%s",
             c->status.outputDirectory);
    c->status.revision = 1;
    c->status.analysis.config = config->analysis;
    c->status.commandOk = 1;
    snprintf(c->status.message, sizeof(c->status.message), "Ready to configure and start");
    if (NgThreadStart(&c->manager, Manager, c) != 0) {
        NgMutexDestroy(&c->recorderMutex);
        NgMutexDestroy(&c->mutex);
        free(c);
        return NgFail(error, "Cannot create controller thread");
    }
    *out = c;
    return NG_OK;
}

/**
 * @brief Stop and join receiver/manager activity and free the controller.
 *
 * @details
 * The web layer and other control clients must have stopped calling the controller. Finishes
 * any recording and joins diagnostics before releasing resources. NULL is accepted.
 *
 * Parameter control: Controller to destroy; invalid after return.
 */
void NgControlDestroy(TNgControl *control)
{
    if (control == NULL) {
        return;
    }
    NgAtomicSet(&control->terminate, 1);
    NgAtomicSet(&control->stop, 1);
    NgThreadJoin(control->manager);
    NgMutexDestroy(&control->recorderMutex);
    NgMutexDestroy(&control->mutex);
    free(control->cafArchive);
    free(control);
}

/**
 * @brief Queue one asynchronous lifecycle command.
 *
 * @details
 * Thread-safe with status readers. Uses a revision guard and rejects conflicting transitions
 * or pending commands. Completion is reported later through command IDs in snapshots; stop
 * completion includes writer flush.
 *
 * Parameter control: Live controller.
 * Parameter command: Start/stop receiver or start/stop recording command.
 * Parameter revision: Expected current profile revision.
 * Parameter commandId: Receives the accepted command identifier.
 * Parameter error: Optional conflict/error destination.
 *
 * Result: NG_OK when queued; NG_ERROR for stale revision, unsupported transition or an already
 * pending command.
 */
int NgControlRequest(TNgControl *control, TNgCommand command, uint64_t revision,
                     uint64_t *commandId, TNgError *error)
{
    int valid = 0;
    NgMutexLock(&control->mutex);
    if (command == NG_CMD_STOP && control->status.state == NG_STARTING &&
        revision == control->status.revision) {
        NgAtomicSet(&control->stop, 1);
        *commandId = ++control->status.commandId;
        if (control->pending == NG_CMD_START) {
            control->pending = 0;
            control->status.state = NG_STOPPED;
            control->status.recordingState = NG_REC_OFF;
            CommandResult(control, 1, "Start cancelled before opening frontend");
        } else {
            control->status.state = NG_STOPPING;
            control->busy = 1;
            control->pending = NG_CMD_STOP;
        }
        NgMutexUnlock(&control->mutex);
        return NG_OK;
    }
    if (control->busy == 0 && control->pending == 0 && revision == control->status.revision) {
        if (command == NG_CMD_START &&
            (control->status.state == NG_STOPPED || control->status.state == NG_FAILED)) {
            if (control->config.frontend == NG_FRONTEND_FILE &&
                control->config.replayPath[0] == '\0') {
                NgMutexUnlock(&control->mutex);
                return NgFail(error, "Choose a replay file before starting the receiver");
            }
            if (control->config.acquisition.enabled != 0 && control->cafArchive == NULL) {
                control->cafArchive = calloc(32u, sizeof(*control->cafArchive));
                if (control->cafArchive == NULL) {
                    NgMutexUnlock(&control->mutex);
                    return NgFail(error, "Cannot allocate GPS CAF history");
                }
            }
            if (control->cafArchive != NULL)
                memset(control->cafArchive, 0, 32u * sizeof(*control->cafArchive));
            control->cafArchiveLatestPrn = 0;
            NgAtomicSet(&control->stop, 0);
            control->runConfig = control->config;
            control->status.activeRevision = revision;
            ++control->status.runId;
            control->status.state = NG_STARTING;
            control->status.recordingState =
                (control->config.recordingEnabled) != 0 ? NG_REC_STARTING : NG_REC_OFF;
            memset(&control->status.analysis, 0, sizeof(control->status.analysis));
            memset(&control->status.acquisition, 0, sizeof(control->status.acquisition));
            memset(&control->status.receiver, 0, sizeof(control->status.receiver));
            memset(&control->status.recording, 0, sizeof(control->status.recording));
            control->status.actualBandwidthHz = 0;
            control->status.totalWrittenSamples = 0;
            control->status.recordingName[0] = '\0';
            valid = 1;
        } else if (command == NG_CMD_STOP && control->status.state == NG_RUNNING &&
                   control->status.recordingState != NG_REC_STOPPING) {
            control->status.state = NG_STOPPING;
            valid = 1;
        } else if (command == NG_CMD_RECORD_START && control->status.state == NG_RUNNING &&
                   control->status.recordingState == NG_REC_OFF) {
            control->status.recordingState = NG_REC_STARTING;
            valid = 1;
        } else if (command == NG_CMD_RECORD_STOP && control->status.state == NG_RUNNING &&
                   control->status.recordingState == NG_REC_ACTIVE) {
            control->status.recordingState = NG_REC_STOPPING;
            valid = 1;
        }
    }
    if (valid != 0) {
        control->pending = command;
        control->busy = 1;
        *commandId = ++control->status.commandId;
        snprintf(control->status.message, sizeof(control->status.message),
                 "Command accepted; transition pending");
    }
    NgMutexUnlock(&control->mutex);
    return (valid) != 0
               ? NG_OK
               : NgFail(error, "Command conflicts with current state, pending operation or "
                               "configuration revision");
}

/**
 * @brief Copy coherent lifecycle status and owned diagnostic data.
 *
 * @details
 * No borrowed sample storage escapes. The caller may serialize the snapshot after return.
 * Protect controller lifetime; JSON/network I/O occurs outside its locks.
 *
 * Parameter control: Live controller.
 * Parameter status: Receives a by-value status snapshot.
 */
void NgControlSnapshot(TNgControl *control, TNgControlStatus *status)
{
    NgMutexLock(&control->mutex);
    if (control->analysis != NULL) {
        NgAnalysisRead(control->analysis, &control->status.analysis);
    }
    if (control->acquisition != NULL) {
        NgAcquisitionReceiverRead(control->acquisition, &control->status.acquisition);
    }
    *status = control->status;
    NgMutexUnlock(&control->mutex);
}

void NgControlAcquisitionCafSnapshot(TNgControl *control, unsigned prn,
                                     TNgAcquisitionCaf *caf)
{
    if (caf == NULL) return;
    memset(caf, 0, sizeof(*caf));
    if (control == NULL || prn > 32u) return;
    NgMutexLock(&control->mutex);
    if (control->acquisition != NULL) {
        NgAcquisitionReceiverReadCaf(control->acquisition, prn, caf);
    } else if (control->cafArchive != NULL) {
        if (prn == 0) prn = control->cafArchiveLatestPrn;
        if (prn != 0) *caf = control->cafArchive[prn - 1u];
    }
    NgMutexUnlock(&control->mutex);
}

/**
 * @brief Copy the current receiver configuration and revision.
 *
 * @details
 * Thread-safe read; changes are applied through configure APIs, not through this returned
 * copy.
 *
 * Parameter control: Live controller.
 * Parameter config: Receives owned configuration copy.
 * Parameter revision: Receives current profile revision.
 */
void NgControlConfig(TNgControl *control, TNgConfig *config, uint64_t *revision)
{
    NgMutexLock(&control->mutex);
    *config = control->config;
    *revision = control->status.revision;
    NgMutexUnlock(&control->mutex);
}

/**
 * @brief Validate and atomically persist a new stopped receiver profile.
 *
 * @details
 * Requires receiver stopped/error with no conflicting command and matching expectedRevision.
 * Prepares capture directory, writes a temporary INI and atomically replaces the profile before
 * publishing config. May perform filesystem I/O.
 *
 * Parameter control: Live controller.
 * Parameter config: Complete settings to copy and persist.
 * Parameter expectedRevision: Expected current profile expectedRevision.
 * Parameter error: Optional validation/file/conflict error destination.
 *
 * Result: NG_OK on success; NG_ERROR otherwise.
 */
int NgControlConfigure(TNgControl *control, const TNgConfig *config, uint64_t expectedRevision,
                       TNgError *error)
{
    char temporary[512];
    char destination[512];
    FILE *file;
    int result;
    TNgConfig normalized = *config;
    if (NgControlValidate(config, error) != NG_OK) {
        return NG_ERROR;
    }
    NgMutexLock(&control->mutex);
    if (control->busy != 0 ||
        (control->status.state != NG_STOPPED && control->status.state != NG_FAILED) ||
        expectedRevision != control->status.revision) {
        NgMutexUnlock(&control->mutex);
        return NgFail(error,
                      "Stop receiver before applying configuration; reload if revision changed");
    }
    if (NgDirectoryPrepare(
            config->recordingDirectory[0] ? config->recordingDirectory : control->profileDirectory,
            normalized.recordingDirectory, sizeof(normalized.recordingDirectory)) != 0 ||
        strlen(normalized.recordingDirectory) > 240) {
        NgMutexUnlock(&control->mutex);
        return NgFail(error,
                      "Cannot resolve recording directory (maximum 240 bytes; parent must exist)");
    }
    snprintf(temporary, sizeof(temporary), "%s/receiver.ini.tmp", control->profileDirectory);
    snprintf(destination, sizeof(destination), "%s/receiver.ini", control->profileDirectory);
    file = NgFopenExclusive(temporary);
    if (file == NULL) {
        NgMutexUnlock(&control->mutex);
        return NgFail(
            error,
            "Cannot create temporary profile; check output directory or stale receiver.ini.tmp");
    }
    result = NgConfigWrite(file, &normalized, error);
    if (fclose(file) != 0) {
        result = NgFail(error, "Profile flush failed");
    }
    if (result == NG_OK && NgFileReplace(temporary, destination) != 0) {
        result = NgFail(error, "Cannot replace saved profile");
    }
    if (result == NG_OK) {
        control->config = normalized;
        memset(&control->status.analysis, 0, sizeof(control->status.analysis));
        control->status.analysis.config = normalized.analysis;
        snprintf(control->status.outputDirectory, sizeof(control->status.outputDirectory), "%s",
                 normalized.recordingDirectory);
        ++control->status.revision;
        snprintf(control->status.message, sizeof(control->status.message),
                 "Configuration saved; applies on next Start");
    } else {
        (void)remove(temporary);
    }
    NgMutexUnlock(&control->mutex);
    return result;
}

/**
 * @brief Apply optional diagnostic parameters without restarting reception.
 *
 * @details
 * Uses expectedRevision checking and rejects start/stop transitions. Copies settings into
 * controller/worker and invalidates older analysis results. Persist the full profile
 * separately while stopped.
 *
 * Parameter control: Live controller.
 * Parameter config: New diagnostic settings.
 * Parameter expectedRevision: Expected current profile expectedRevision.
 * Parameter error: Optional error destination.
 *
 * Result: NG_OK on success; NG_ERROR for invalid settings, expectedRevision or transition.
 */
int NgControlAnalysisConfigure(TNgControl *control, const TNgAnalysisConfig *config,
                               uint64_t expectedRevision, TNgError *error)
{
    if (NgAnalysisValidate(config, error) != NG_OK) {
        return NG_ERROR;
    }
    NgMutexLock(&control->mutex);
    if (control->busy != 0 || expectedRevision != control->status.revision ||
        control->status.state == NG_STARTING || control->status.state == NG_STOPPING) {
        NgMutexUnlock(&control->mutex);
        return NgFail(error, "Analysis change conflicts with transition or profile revision");
    }
    control->config.analysis = *config;
    ++control->status.revision;
    if (control->analysis != NULL) {
        NgAnalysisConfigure(control->analysis, config, error);
    } else {
        memset(&control->status.analysis, 0, sizeof(control->status.analysis));
        control->status.analysis.config = *config;
    }
    snprintf(control->status.message, sizeof(control->status.message),
             "Analysis applied to session; save full profile while stopped to persist");
    NgMutexUnlock(&control->mutex);
    return NG_OK;
}

/**
 * @brief Renew a two-second consumer lease on optional diagnostics.
 *
 * @details
 * Thread-safe; subscriptions from independent clients combine. Does not stop receiver, channel
 * dispatch or recording when plots become dormant.
 *
 * Parameter control: Live controller.
 * Parameter mask: Mask bits: 1 PSD, 2 histogram, 4 acquisition CAF.
 */
void NgControlDiagnosticInterest(TNgControl *control, unsigned mask)
{
    NgControlDiagnosticInterestUntil(control, mask, NgMonotonicSeconds() + 2.0);
}

/** @brief Replace aggregated diagnostic deadlines while protecting worker lifetime. */
void NgControlDiagnosticInterestUntil(TNgControl *control, unsigned mask, double expiresSeconds)
{
    NgMutexLock(&control->mutex);
    if (control->analysis != NULL) {
        NgAnalysisInterest(control->analysis, mask & 3u, expiresSeconds);
    }
    if ((mask & 4u) != 0) {
        control->cafInterestUntil = expiresSeconds;
        NgAcquisitionReceiverCafInterestUntil(control->acquisition, expiresSeconds);
    }
    NgMutexUnlock(&control->mutex);
}
