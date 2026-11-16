/** @file channel.h @brief Replaceable per-channel algorithms with shared epochs. */
#ifndef NGENE_CHANNEL_H
#define NGENE_CHANNEL_H
#include "ngene/types.h"

/** @brief Supported channel acquisition, tracking and recovery states. */
typedef enum {
    NG_CHANNEL_ACQUISITION,
    NG_CHANNEL_TRACKING,
    NG_CHANNEL_HOLDOVER,
    NG_CHANNEL_FAST_REACQUISITION
} TNgChannelState;

/** @brief Stable receiver-local satellite/signal channel identity. */
typedef struct {
    /** Receiver-local identity of this channel instance. */
    uint32_t instanceId;
    /** Constellation identifier supplied by the channel allocator. */
    uint32_t constellation;
    /** Satellite/PRN identifier supplied by the channel allocator; zero while unassigned.
     * A scanning module may update its own registration during its ordered callback. */
    uint32_t satellite;
    /** Signal identifier for the selected satellite. */
    uint32_t signal;
} TNgChannelId;

/** @brief Replaceable synchronous processing operation for one receiver channel. */
typedef struct {
    /** @brief Process the same common window supplied to every channel.
     * @details Synchronous processing of one channel in every state. The scheduled engine
     * may invoke different channels concurrently; each channel's windows remain ordered.
     * Workspace must be per-channel or explicitly synchronized. Never retain samples or
     * rely on callback order in parallel mode. Handle gaps by invalidating coherent history.
     * No I/O is allowed on this path. A next-state proposal is validated by the engine.
     * @param context Private algorithm state, owned by this channel.
     * @param id Read-only constellation/satellite/signal identity.
     * @param state Current channel state.
     * @param window Immutable borrowed common window; valid until callback return.
     * @param next Receives the proposed next channel state.
     * @param error Optional module error destination.
     * @return NG_OK for processed input; NG_ERROR to stop the run on module failure.
     */
    int (*process)(void *context, const TNgChannelId *id, TNgChannelState state,
                   const TNgIqView *window, TNgChannelState *next, TNgError *error);
    /** Optional supported-format bitset (1u << TNgSampleFormat). Zero preserves the original
     * CI8-only contract. Declare additional formats only when the callback reads them natively. */
    unsigned nativeFormats;
    /** Optional load notification, called on the receiver thread before this window's
     * callbacks, with no channel callback active. Counts TRACKING, HOLDOVER and fast
     * reacquisition registrations. Acquisition modules can reduce admitted searches;
     * tracking still receives every sample. No I/O or allocation here. */
    void (*setTrackingLoad)(void *context, size_t trackingChannels);
    /** Optional: nonzero declares that the callback does nothing in ACQUISITION state and
     * proposes no transition there. Dispatchers may then skip it for that window. */
    int idleInAcquisition;
} TNgChannelOps;

/** @brief Channel identity, lifecycle and independently owned algorithm context. */
typedef struct {
    /** Constellation, satellite and signal identity for this channel. */
    TNgChannelId id;
    /** Current receiver/channel lifecycle state. */
    TNgChannelState state;
    /** Borrowed immutable operations table; implementation owns its storage. */
    const TNgChannelOps *ops;
    /** Opaque implementation or callback state; ownership follows the enclosing API. */
    void *context;
} TNgChannel;

/**
 * @brief Check a proposed receiver channel state transition.
 *
 * @details
 * Permits supported acquisition/tracking/holdover/reacquisition transitions, including direct
 * HOLDOVER to TRACKING. Does not modify the channel or imply a navigation solution.
 *
 * @param from Current channel state.
 * @param to Proposed next channel state.
 *
 * @return Nonzero for an allowed transition, zero otherwise.
 */
int NgChannelTransitionValid(TNgChannelState from, TNgChannelState to);
#endif
