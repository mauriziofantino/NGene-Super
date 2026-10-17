/** @file channel_dispatch.c @brief Fixed worker pool with one barrier per shared window. */
#include "core/channel_dispatch.h"
#include "platform/platform.h"
#include <limits.h>
#include <stdlib.h>

/** @brief Preallocated result written by one worker and read after the window barrier. */
typedef struct {
    /** Callback result, including engine validation of the requested transition. */
    int result;
    /** Proposed state; published only after all callbacks finish. */
    TNgChannelState next;
    /** Private error destination avoids concurrent writes to the caller's error. */
    TNgError error;
} TNgChannelResult;

/** @brief Run-owned scheduler; one receiver thread submits windows synchronously. */
struct TNgChannelDispatch {
    /** Borrowed registration array, immutable except for state committed by the receiver. */
    TNgChannel *channels;
    /** Fixed number of channels sharing every dispatched window. */
    size_t channelCount;
    /** Owned thread handles, released after all worker joins. */
    TNgThread *workers;
    /** Number of successfully started workers, including partially initialized pools. */
    size_t workerCount;
    /** Owned per-channel result scratch, reused after each completion barrier. */
    TNgChannelResult *results;
    /** Protects work publication, the job cursor and completion count. */
    TNgMutex mutex;
    /** Wakes idle workers for a new window or shutdown. */
    TNgCondition work;
    /** Wakes the receiver when all callbacks for its current window have finished. */
    TNgCondition complete;
    /** Borrowed immutable view; published while mutex is held. */
    TNgIqView window;
    /** Publication generation; workers wait while their last generation remains current. */
    uint64_t generation;
    /** Next registration index, dynamically claimed by idle workers under mutex. */
    size_t nextJob;
    /** Finished callbacks for the current generation, protected by mutex. */
    size_t completed;
    /** Shutdown predicate, protected by mutex and set only outside active dispatch. */
    int stopping;
};

/** @brief Claim channel jobs until the current window completes, sleeping between windows.
 * @param context Run-owned dispatcher retained until every worker is joined.
 */
static void Worker(void *context)
{
    TNgChannelDispatch *dispatch = context;
    uint64_t generation = 0;
    NgMutexLock(&dispatch->mutex);
    for (;;) {
        while (dispatch->stopping == 0 && generation == dispatch->generation) {
            NgConditionWait(&dispatch->work, &dispatch->mutex, UINT_MAX);
        }
        if (dispatch->stopping != 0) {
            break;
        }
        generation = dispatch->generation;
        while (dispatch->nextJob < dispatch->channelCount) {
            size_t index = dispatch->nextJob++;
            TNgChannel *channel = &dispatch->channels[index];
            TNgChannelResult *result = &dispatch->results[index];
            TNgIqView window = dispatch->window;
            NgMutexUnlock(&dispatch->mutex);
            result->next = channel->state;
            result->error.message[0] = '\0';
            result->result = channel->ops->process(channel->context, &channel->id, channel->state,
                                                   &window, &result->next, &result->error);
            if (result->result == NG_OK &&
                NgChannelTransitionValid(channel->state, result->next) == 0) {
                result->result =
                    NgFail(&result->error, "Invalid state transition requested by channel %u",
                           channel->id.instanceId);
            }
            NgMutexLock(&dispatch->mutex);
            ++dispatch->completed;
            if (dispatch->completed == dispatch->channelCount) {
                NgConditionSignal(&dispatch->complete);
            }
        }
    }
    NgMutexUnlock(&dispatch->mutex);
}

/** @brief Initialize all synchronization and workspace before starting any frontend work. */
int NgChannelDispatchCreate(TNgChannel *channels, size_t channelCount, size_t workerCount,
                            TNgChannelDispatch **out, TNgError *error)
{
    TNgChannelDispatch *dispatch;
    size_t k;
    if (out != NULL) {
        *out = NULL;
    }
    if (out == NULL || workerCount == 0 || (channelCount != 0 && channels == NULL)) {
        return NgFail(error, "Invalid channel scheduler configuration");
    }
    if (workerCount > channelCount) {
        workerCount = channelCount;
    }
    if (workerCount <= 1) {
        return NG_OK;
    }
    if (channelCount > SIZE_MAX / sizeof(TNgChannelResult) ||
        workerCount > SIZE_MAX / sizeof(TNgThread)) {
        return NgFail(error, "Channel scheduler allocation exceeds address space");
    }
    dispatch = calloc(1, sizeof(*dispatch));
    if (dispatch == NULL) {
        return NgFail(error, "Cannot allocate channel scheduler");
    }
    dispatch->workers = calloc(workerCount, sizeof(*dispatch->workers));
    dispatch->results = calloc(channelCount, sizeof(*dispatch->results));
    if (dispatch->workers == NULL || dispatch->results == NULL) {
        goto memoryError;
    }
    if (NgMutexInit(&dispatch->mutex) != 0) {
        goto memoryError;
    }
    if (NgConditionInit(&dispatch->work) != 0) {
        NgMutexDestroy(&dispatch->mutex);
        goto memoryError;
    }
    if (NgConditionInit(&dispatch->complete) != 0) {
        NgConditionDestroy(&dispatch->work);
        NgMutexDestroy(&dispatch->mutex);
        goto memoryError;
    }
    dispatch->channels = channels;
    dispatch->channelCount = channelCount;
    for (k = 0; k < workerCount; ++k) {
        if (NgThreadStart(&dispatch->workers[k], Worker, dispatch) != 0) {
            NgChannelDispatchDestroy(dispatch);
            return NgFail(error, "Cannot start channel scheduler worker");
        }
        ++dispatch->workerCount;
    }
    *out = dispatch;
    return NG_OK;
memoryError:
    free(dispatch->results);
    free(dispatch->workers);
    free(dispatch);
    return NgFail(error, "Cannot allocate/init channel scheduler");
}

/** @brief Publish one immutable view and synchronize all workers before returning its lease. */
int NgChannelDispatchProcess(TNgChannelDispatch *dispatch, const TNgIqView *window, TNgError *error)
{
    size_t k;
    int result = NG_OK;
    NgMutexLock(&dispatch->mutex);
    dispatch->window = *window;
    dispatch->nextJob = 0;
    dispatch->completed = 0;
    ++dispatch->generation;
    NgConditionSignal(&dispatch->work);
    while (dispatch->completed != dispatch->channelCount) {
        NgConditionWait(&dispatch->complete, &dispatch->mutex, UINT_MAX);
    }
    NgMutexUnlock(&dispatch->mutex);
    for (k = 0; k < dispatch->channelCount; ++k) {
        TNgChannelResult *channelResult = &dispatch->results[k];
        if (channelResult->result == NG_OK) {
            dispatch->channels[k].state = channelResult->next;
        } else if (result == NG_OK) {
            result = NG_ERROR;
            if (channelResult->error.message[0] == '\0') {
                (void)NgFail(&channelResult->error, "Channel %u processing failed",
                             dispatch->channels[k].id.instanceId);
            }
            if (error != NULL) {
                *error = channelResult->error;
            }
        }
    }
    return result;
}

/** @brief Join even a partially created pool before freeing its synchronization and scratch. */
void NgChannelDispatchDestroy(TNgChannelDispatch *dispatch)
{
    size_t k;
    if (dispatch == NULL) {
        return;
    }
    NgMutexLock(&dispatch->mutex);
    dispatch->stopping = 1;
    NgConditionSignal(&dispatch->work);
    NgMutexUnlock(&dispatch->mutex);
    for (k = 0; k < dispatch->workerCount; ++k) {
        NgThreadJoin(dispatch->workers[k]);
    }
    NgConditionDestroy(&dispatch->complete);
    NgConditionDestroy(&dispatch->work);
    NgMutexDestroy(&dispatch->mutex);
    free(dispatch->results);
    free(dispatch->workers);
    free(dispatch);
}
