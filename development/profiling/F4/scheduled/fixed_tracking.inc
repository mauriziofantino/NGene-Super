/* Confirmed epoch-0 seeds from the RTL capture's direct INT32 PRN trace. */
static int F4SeedTracking(TNgChannelPool *pool, const TNgIqView *window, TNgError *error)
{
    static const struct {
        unsigned system, prn;
        double doppler, phase;
    } seeds[] = {
        {1, 18, 636, 151.77830078124998},
        {1, 26, 1096, 546.4658203125},
        {1, 29, -2119, 832.11326171874998},
        {1, 31, -2032, 215.27930078124998},
        {2, 10, 1742, 3391.111796875},
        {2, 14, 1722, 170.8330078125}
    };
    if (window->meta.epoch || window->meta.firstSample || pool->sampleRate != 2400000)
        return NgFail(error, "F4 fixed tracking requires the RTL capture from sample zero");
    for (size_t i = 0; i < sizeof(seeds) / sizeof(seeds[0]); ++i) {
        unsigned slot = FreeSlot(pool, seeds[i].system, 0);
        if (slot == pool->config.channelCount)
            return NgFail(error, "Insufficient slots for F4 fixed tracking");
        const TNgAcquisitionResult seed = {.prn = seeds[i].prn, .confirmed = 1,
            .dopplerHz = seeds[i].doppler, .codePhaseChips = seeds[i].phase, .successes = 3};
        if (NgTrackingSetSignal(pool->tracking[slot], seeds[i].system, error) != NG_OK ||
            NgTrackingSeed(pool->tracking[slot], &seed, error) != NG_OK)
            return NG_ERROR;
        pool->jobs[slot].state = NG_CHANNEL_TRACKING;
        Refresh(pool);
    }
    return NG_OK;
}
