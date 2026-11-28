/** @file navigation.c @brief Classic GPS transition synchronization and LNAV word parity.
 * Adapted from NGene Classic tracking/gpsTracking.c and utilityGnss/gpsMessageParser.c,
 * Mario Nicola, 2014. Native double prompts replace packed IF correlations.
 */
#include "ngene/navigation.h"
#include <math.h>
#include <string.h>

/** Clear all assignment-owned data before the next signal epoch. */
void NgNavigationReset(TNgNavigation *navigation)
{
    memset(navigation, 0, sizeof(*navigation));
    navigation->transitions = -1;
}

/** Carrier/timing loss does not invalidate a satellite's already parsed ephemerides. */
void NgNavigationLoseTiming(TNgNavigation *navigation)
{
    if (navigation->epochs || navigation->snapshot.symbols ||
        navigation->snapshot.bitSynchronized || navigation->galileo.count) {
        TNgBroadcast broadcast = navigation->snapshot.broadcast;
        NgNavigationReset(navigation);
        broadcast.towValid = 0;
        navigation->snapshot.broadcast = broadcast;
    }
}

/** Retain Classic's six parity masks and D30-dependent data polarity. */
int NgNavigationGpsParity(uint32_t *word)
{
    static const uint32_t masks[6] = {0xbb1f3480, 0x5d8f9a40, 0xaec7cd00,
                                      0x5763e680, 0x6bb1f340, 0x8b7a89c0};
    unsigned k, parity = 0;
    if (*word & UINT32_C(0x40000000)) {
        *word ^= UINT32_C(0x3fffffc0);
    }
    for (k = 0; k < 6; ++k) {
        uint32_t value = *word & masks[k];
        value ^= value >> 16;
        value ^= value >> 8;
        value ^= value >> 4;
        value ^= value >> 2;
        value ^= value >> 1;
        parity |= (value & 1u) << (5 - k);
    }
    return parity == (*word & 63u);
}

/** Decode a subframe only after all ten consecutive words pass parity. */
void NgNavigationGpsBit(TNgNavigation *navigation, unsigned bit, uint64_t endSample)
{
    navigation->snapshot.symbols++;
    if (navigation->sinceValidBits < 301) {
        navigation->sinceValidBits++;
    }
    if (navigation->sinceValidBits > 300) {
        navigation->snapshot.towValid = 0;
    }
    navigation->shift = (navigation->shift << 1) | (bit & 1u);
    if (!navigation->collecting) {
        if ((navigation->shift & 255u) == 0x8bu || (navigation->shift & 255u) == 0x74u) {
            navigation->collecting = 1;
            navigation->wordBits = 8;
            navigation->wordCount = 0;
        }
        return;
    }
    if (++navigation->wordBits < 30) {
        return;
    }
    navigation->wordBits = 0;
    if (!NgNavigationGpsParity(&navigation->shift)) {
        navigation->snapshot.parityErrors++;
        navigation->snapshot.towValid = 0;
        navigation->collecting = 0;
        navigation->wordCount = 0;
        return;
    }
    navigation->words[navigation->wordCount++] = navigation->shift;
    if (navigation->wordCount == 10) {
        unsigned tow = ((navigation->words[1] >> 13) & 0x1ffffu) * 6u;
        unsigned id = (navigation->words[1] >> 8) & 7u;
        if (((navigation->words[0] >> 22) & 255u) == 0x8bu && id >= 1 && id <= 5 && tow < 604800u) {
            navigation->snapshot.towValid = 1;
            navigation->snapshot.towSeconds = tow;
            navigation->snapshot.subframeId = id;
            navigation->snapshot.validSubframes++;
            navigation->snapshot.boundarySample = endSample;
            NgBroadcastGps(&navigation->snapshot.broadcast, navigation->words, id);
            navigation->sinceValidBits = 0;
        } else {
            navigation->snapshot.towValid = 0;
        }
        navigation->collecting = 0;
        navigation->wordCount = 0;
    }
}

/** Preserve Classic's five transition intervals and low PLL quality threshold (0.4).
 * Prompt intervals remain one millisecond; only data demodulation sums twenty epochs.
 */
void NgNavigationGpsPromptWithChecks(TNgNavigation *navigation, double i, double q, int pll,
                                     uint64_t endSample, unsigned transitionChecks)
{
    if (!pll || !isfinite(i) || !isfinite(q)) {
        NgNavigationLoseTiming(navigation);
        return;
    }
    navigation->epochs++;
    navigation->inPhasePower = 0.99 * navigation->inPhasePower + 0.01 * i * i;
    navigation->quadPower = 0.99 * navigation->quadPower + 0.01 * q * q;
    {
        double total = navigation->inPhasePower + navigation->quadPower;
        navigation->quality =
            total > 0 && (navigation->inPhasePower - navigation->quadPower) / total >= 0.4;
        if (!navigation->quality && !navigation->snapshot.bitSynchronized) {
            navigation->snapshot.bitSynchronized = navigation->snapshot.towValid = 0;
            navigation->transitions = -1;
            navigation->collecting = 0;
            navigation->bitMs = 0;
            navigation->sum = 0;
        }
    }
    if (!navigation->snapshot.bitSynchronized && navigation->quality &&
        i * navigation->previous < 0) {
        if (navigation->transitions < 0 ||
            (navigation->epochs - navigation->transition) % 20 != 0) {
            navigation->transitions = 0;
        } else {
            navigation->transitions++;
        }
        navigation->transition = navigation->epochs;
        if ((unsigned)navigation->transitions >= transitionChecks) {
            navigation->snapshot.bitSynchronized = 1;
            navigation->sum = 0;
            navigation->bitMs = 0;
        }
    }
    navigation->previous = i;
    if (navigation->snapshot.bitSynchronized) {
        navigation->sum += i;
        if (++navigation->bitMs == 20) {
            NgNavigationGpsBit(navigation, navigation->sum > 0 ? 0u : 1u, endSample);
            navigation->sum = 0;
            navigation->bitMs = 0;
        }
    }
}

void NgNavigationGpsPrompt(TNgNavigation *navigation, double i, double q, int pll,
                           uint64_t endSample)
{
    NgNavigationGpsPromptWithChecks(navigation, i, q, pll, endSample, 5);
}
