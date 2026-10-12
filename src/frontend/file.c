/** @file file.c
 * @brief Recorded CI8 playback with independent file, buffer and monotonic pacing clock.
 */
#include "frontend/file.h"
#include "platform/platform.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <sys/types.h>
#endif

/** @brief Independent immutable CI8 reader, leased window and monotonic replay clock. */
typedef struct {
    /** Copied validated configuration; owned by this instance. */
    TNgConfig config;
    /** Owned read-only FILE handle; no other replay instance shares it. */
    FILE *input;
    /** Owned CI8 read window; reused only after the reader releases its lease. */
    TNgIq8 *buffer;
    /** Fixed selected file range in complex pairs, captured at open. */
    uint64_t rangeSamples;
    /** Complex-pair position within the selected range. */
    uint64_t cursor;
    /** Total delivered complex pairs, including completed loop iterations. */
    uint64_t produced;
    /** Continuity generation; invalidates coherent history after a reset. */
    uint64_t epoch;
    /** Monotonic pacing-clock origin; unrelated to RF or GNSS time. */
    double startTime;
    /** Nonzero after transport start and before a successful stop. */
    int started;
    /** Nonzero while the sole reader owns a borrowed view. */
    int leased;
    /** Pending gap flag for the first view after a loop rewind. */
    int rewound;
} TNgFileSource;

/** @brief Seek to a complex-pair offset using the target's 64-bit file API.
 * @details Range is checked before conversion; no shared file handle is used.
 * @param source Owned file-source context.
 * @return NG_OK on success; NG_ERROR if the file cannot be positioned.
 */
static int RewindRange(TNgFileSource *source)
{
    int64_t offset =
        (int64_t)(source->config.replayStartSample * NgSampleBytes(source->config.replayFormat));
#ifdef _WIN32
    return _fseeki64(source->input, offset, SEEK_SET) == 0 ? NG_OK : NG_ERROR;
#else
    return fseeko(source->input, (off_t)offset, SEEK_SET) == 0 ? NG_OK : NG_ERROR;
#endif
}

/** @brief Start an opened replay and reset its clock/counters.
 * @details Called by this engine's reader before any leases; another engine is unaffected.
 * @param context Owned file source.
 * @param error Optional seek diagnostic.
 * @return NG_OK if reset; NG_ERROR on seek failure or invalid lifecycle.
 */
static int Start(void *context, TNgError *error)
{
    TNgFileSource *source = context;
    if (source->started != 0 || source->leased != 0 || RewindRange(source) != NG_OK) {
        return NgFail(error, "Cannot reset replay source");
    }
    source->cursor = 0;
    source->produced = 0;
    source->epoch = 0;
    source->rewound = 0;
    source->startTime = NgMonotonicSeconds();
    source->started = 1;
    return NG_OK;
}

/** @brief Borrow a bounded CI8 file window after optional real-time pacing.
 * @details One reader/lease per instance. A loop rewind reports an explicit discontinuity;
 * it never pretends repeated RF data is a new uninterrupted acquisition. File truncation is
 * an error. Lifetime ends at Release; no pointer is shared with another source.
 * @param context Owned started file source.
 * @param view Receives immutable samples on NG_OK only.
 * @param timeoutMs Maximum pacing wait in milliseconds; zero polls.
 * @param error Optional read/lifecycle diagnostic.
 * @return NG_OK, NG_TIMEOUT, NG_END at the selected range end, or NG_ERROR.
 */
static int Acquire(void *context, TNgIqView *view, unsigned timeoutMs, TNgError *error)
{
    TNgFileSource *source = context;
    size_t count = source->config.windowSamples;
    uint64_t remaining;
    if (source->started == 0 || source->leased != 0 || view == NULL) {
        return NgFail(error, "Invalid replay lifecycle");
    }
    if (source->cursor == source->rangeSamples) {
        if (source->config.replayLoop == 0) {
            return NG_END;
        }
        if (source->epoch == UINT64_MAX || RewindRange(source) != NG_OK) {
            return NgFail(error, "Cannot rewind replay range");
        }
        source->cursor = 0;
        ++source->epoch;
        source->rewound = 1;
    }
    remaining = source->rangeSamples - source->cursor;
    if (remaining < count) {
        count = (size_t)remaining;
    }
    if (source->produced > UINT64_MAX - count) {
        return NgFail(error, "Replay sample index exhausted");
    }
    if (source->config.replayRealtime != 0) {
        double due =
            source->startTime + (double)(source->produced + count) / source->config.sampleRateHz;
        double wait = due - NgMonotonicSeconds();
        if (wait > (double)timeoutMs / NG_MILLISECONDS_PER_SECOND) {
            NgSleepMs(timeoutMs);
            return NG_TIMEOUT;
        }
        if (wait > 0.0) {
            NgSleepMs((unsigned)ceil(wait * NG_MILLISECONDS_PER_SECOND));
        }
    }
    if (fread(source->buffer, NgSampleBytes(source->config.replayFormat), count, source->input) !=
        count) {
        return NgFail(error, "Replay read failed or the input file was truncated");
    }
    memset(view, 0, sizeof(*view));
    view->samples = source->buffer;
    view->format = source->config.replayFormat;
    view->meta.count = count;
    view->meta.firstSample = source->produced;
    view->meta.epoch = source->epoch;
    if (count < source->config.windowSamples) {
        view->meta.flags |= NG_BLOCK_PARTIAL;
    }
    if (source->rewound != 0) {
        view->meta.flags |= NG_BLOCK_GAP;
        source->rewound = 0;
    }
    source->cursor += count;
    source->produced += count;
    source->leased = 1;
    return NG_OK;
}

/** @brief End this source's sample lease.
 * @details No I/O or allocation; previously borrowed samples must no longer be used.
 * @param context Source with one outstanding lease.
 */
static void Release(void *context)
{
    ((TNgFileSource *)context)->leased = 0;
}

/** @brief Stop playback without changing any other receiver session.
 * @details Called after releasing the current window; no background file thread exists.
 * @param context Owned file source.
 * @param error Unused optional diagnostic.
 * @return NG_OK; the file is closed by Destroy.
 */
static int Stop(void *context, TNgError *error)
{
    (void)error;
    ((TNgFileSource *)context)->started = 0;
    return NG_OK;
}

/** @brief Copy counters from the replay reader.
 * @details Reader-thread operation; file playback has no USB loss counter.
 * @param context Live source.
 * @param stats Receives counters in complex pairs.
 */
static void Stats(void *context, TNgFrontendStats *stats)
{
    TNgFileSource *source = context;
    memset(stats, 0, sizeof(*stats));
    stats->receivedSamples = source->produced;
}

/** @brief Close and free one stopped replay source.
 * @details Owner must ensure no lease or reader remains. File and buffer are exclusively owned.
 * @param context Owned stopped file source.
 */
static void Destroy(void *context)
{
    TNgFileSource *source = context;
    if (source != NULL) {
        if (source->input != NULL) {
            (void)fclose(source->input);
        }
        free(source->buffer);
        free(source);
    }
}

/** @brief Open a recorded signed CI8 file and allocate a private read window.
 * @details Captures file length at open; subsequent growth is not included. An input must
 * contain complete complex pairs. Rate/tuning metadata is explicit in config, not guessed.
 * Parameter config: Validated file settings and stream metadata.
 * Parameter frontend: Receives ownership on success; unchanged on failure.
 * Parameter error: Optional allocation, format, path or range diagnostic.
 * Result: NG_OK on success, otherwise NG_ERROR after releasing temporary resources.
 */
int NgFileOpen(const TNgConfig *config, TNgFrontend *frontend, TNgError *error)
{
    static const TNgFrontendOps operations = {Start, Acquire, Release, Stop, Stats, Destroy};
    TNgFileSource *source;
    int64_t bytes;
    uint64_t samples;
    if (config == NULL || frontend == NULL || config->replayPath[0] == '\0') {
        return NgFail(error, "Choose a receiver-side replay file before starting");
    }
    source = calloc(1, sizeof(*source));
    if (source == NULL) {
        return NgFail(error, "Cannot allocate replay source");
    }
    source->config = *config;
    source->input = fopen(config->replayPath, "rb");
    if (source->input == NULL) {
        Destroy(source);
        return NgFail(error, "Cannot open replay file: %s", config->replayPath);
    }
#ifdef _WIN32
    bytes = _fseeki64(source->input, 0, SEEK_END) == 0 ? _ftelli64(source->input) : -1;
#else
    bytes = fseeko(source->input, 0, SEEK_END) == 0 ? (int64_t)ftello(source->input) : -1;
#endif
    if (bytes <= 0 || (uint64_t)bytes % NgSampleBytes(config->replayFormat) != 0) {
        Destroy(source);
        return NgFail(error, "Replay requires a nonempty file of complete CI8 pairs");
    }
    samples = (uint64_t)bytes / NgSampleBytes(config->replayFormat);
    if (config->replayStartSample >= samples) {
        Destroy(source);
        return NgFail(error, "Replay start offset is outside the file");
    }
    source->rangeSamples = samples - config->replayStartSample;
    if (config->replaySampleLimit != 0 && config->replaySampleLimit < source->rangeSamples) {
        source->rangeSamples = config->replaySampleLimit;
    }
    source->buffer = malloc(config->windowSamples * NgSampleBytes(config->replayFormat));
    if (source->buffer == NULL || RewindRange(source) != NG_OK) {
        Destroy(source);
        return NgFail(error, "Cannot allocate or position the replay window");
    }
    memset(frontend, 0, sizeof(*frontend));
    frontend->ops = &operations;
    frontend->context = source;
    frontend->info.sampleRateHz = config->sampleRateHz;
    frontend->info.centerFrequencyHz = config->centerFrequencyHz;
    frontend->info.signalFrequencyHz =
        config->signalFrequencyHz != 0 ? config->signalFrequencyHz : config->centerFrequencyHz;
    frontend->info.digitalIfHz =
        (int64_t)frontend->info.signalFrequencyHz - (int64_t)frontend->info.centerFrequencyHz;
    frontend->info.format = config->replayFormat;
    frontend->info.sourceName = "file";
    frontend->info.clockDomain = "file-playback";
    frontend->info.deviceIdentifier = source->config.replayPath;
    return NG_OK;
}
