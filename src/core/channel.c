/** @file channel.c @brief Shared channel transition contract, independent of engine/platform. */
#include "ngene/channel.h"
/**
 * @brief Check a proposed receiver channel state transition.
 *
 * @details
 * Permits supported acquisition/tracking/holdover/reacquisition transitions, including direct
 * HOLDOVER to TRACKING. Does not modify the channel or imply a navigation solution.
 *
 * Parameter from: Current channel state.
 * Parameter to: Proposed next channel state.
 *
 * Result: Nonzero for an allowed transition, zero otherwise.
 */
int NgChannelTransitionValid(TNgChannelState from, TNgChannelState to)
{
    if (from < NG_CHANNEL_ACQUISITION || from > NG_CHANNEL_FAST_REACQUISITION ||
        to < NG_CHANNEL_ACQUISITION || to > NG_CHANNEL_FAST_REACQUISITION) {
        return 0;
    }
    if (from == to) {
        return 1;
    }
    switch (from) {
    case NG_CHANNEL_ACQUISITION:
        return to == NG_CHANNEL_TRACKING;
    case NG_CHANNEL_TRACKING:
        return to == NG_CHANNEL_HOLDOVER;
    case NG_CHANNEL_HOLDOVER:
        return to == NG_CHANNEL_TRACKING || to == NG_CHANNEL_FAST_REACQUISITION;
    case NG_CHANNEL_FAST_REACQUISITION:
        return to == NG_CHANNEL_TRACKING || to == NG_CHANNEL_ACQUISITION;
    default:
        return 0;
    }
}

