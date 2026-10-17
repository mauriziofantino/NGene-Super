/** @file channel_dispatch.h @brief Private bounded parallel dispatch of one shared window. */
#ifndef NGENE_CHANNEL_DISPATCH_H
#define NGENE_CHANNEL_DISPATCH_H
#include "ngene/channel.h"

/** @brief Run-owned workers and per-channel result storage; never owns sample memory. */
typedef struct TNgChannelDispatch TNgChannelDispatch;

/** @brief Allocate a fixed worker pool before frontend start.
 * @details Channel descriptors and private/synchronized contexts remain live and unchanged
 * until destroy. NULL output is used for zero or one effective worker. Worker count is capped
 * to channelCount. No per-window allocation or affinity changes are performed.
 * @param channels Caller-owned validated channel registrations.
 * @param channelCount Number of channel registrations.
 * @param workerCount Requested positive worker count.
 * @param out Receives owned pool, or NULL for serial dispatch/failure.
 * @param error Optional diagnostic destination.
 * @return NG_OK on success; NG_ERROR for invalid bounds or resource failure.
 */
int NgChannelDispatchCreate(TNgChannel *channels, size_t channelCount, size_t workerCount,
                            TNgChannelDispatch **out, TNgError *error);

/** @brief Process one window and wait for every channel before returning.
 * @details Only the receiver thread may call. Every channel runs once, possibly concurrently;
 * each channel's windows remain ordered. Successful valid state transitions are committed
 * after the barrier. The first failed channel in registration order determines the error.
 * @param dispatch Live parallel pool.
 * @param window Immutable shared view retained by caller until return.
 * @param error Optional diagnostic destination.
 * @return NG_OK when all callbacks succeed; NG_ERROR after all work ends otherwise.
 */
int NgChannelDispatchProcess(TNgChannelDispatch *dispatch, const TNgIqView *window,
                             TNgError *error);

/** @brief Stop and join every worker before freeing run-owned storage.
 * @details Call only outside dispatch, after its completion barrier. NULL is accepted.
 * @param dispatch Owned pool whose callbacks have completed.
 */
void NgChannelDispatchDestroy(TNgChannelDispatch *dispatch);
#endif
