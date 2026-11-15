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

/** Retain Classic's six parity masks and D30-dependent data polarity. */
int NgNavigationGpsParity(uint32_t *word)
{
    static const uint32_t masks[6] = {0xbb1f3480, 0x5d8f9a40, 0xaec7cd00,
                                      0x5763e680, 0x6bb1f340, 0x8b7a89c0};
    unsigned k, parity = 0;
    if (*word & UINT32_C(0x40000000))
        *word ^= UINT32_C(0x3fffffc0);
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
void NgNavigationGpsBit(TNgNavigation *n, unsigned bit, uint64_t endSample)
{
    n->snapshot.symbols++;
    if (n->sinceValidBits < 301)
        n->sinceValidBits++;
    if (n->sinceValidBits > 300)
        n->snapshot.towValid = 0;
    n->shift = (n->shift << 1) | (bit & 1u);
    if (!n->collecting) {
        if ((n->shift & 255u) == 0x8bu || (n->shift & 255u) == 0x74u) {
            n->collecting = 1;
            n->wordBits = 8;
            n->wordCount = 0;
        }
        return;
    }
    if (++n->wordBits < 30)
        return;
    n->wordBits = 0;
    if (!NgNavigationGpsParity(&n->shift)) {
        n->snapshot.parityErrors++;
        n->snapshot.towValid = 0;
        n->collecting = 0;
        n->wordCount = 0;
        return;
    }
    n->words[n->wordCount++] = n->shift;
    if (n->wordCount == 10) {
        unsigned tow = ((n->words[1] >> 13) & 0x1ffffu) * 6u;
        unsigned id = (n->words[1] >> 8) & 7u;
        if (((n->words[0] >> 22) & 255u) == 0x8bu && id >= 1 && id <= 5 && tow < 604800u) {
            n->snapshot.towValid = 1;
            n->snapshot.towSeconds = tow;
            n->snapshot.subframeId = id;
            n->snapshot.validSubframes++;
            n->snapshot.boundarySample = endSample;
            NgBroadcastGps(&n->snapshot.broadcast, n->words, id);
            n->sinceValidBits = 0;
        } else {
            n->snapshot.towValid = 0;
        }
        n->collecting = 0;
        n->wordCount = 0;
    }
}

/** Preserve Classic's five transition intervals and low PLL quality threshold (0.4).
 * Prompt intervals remain one millisecond; only data demodulation sums twenty epochs.
 */
void NgNavigationGpsPrompt(TNgNavigation *n, double i, double q, int pll, uint64_t endSample)
{
    if (!pll || !isfinite(i) || !isfinite(q)) {
        NgNavigationReset(n);
        return;
    }
    n->epochs++;
    n->inPhasePower += i * i;
    n->quadPower += q * q;
    if (++n->qualityCount == 100) {
        double total = n->inPhasePower + n->quadPower;
        n->quality = total > 0 && (n->inPhasePower - n->quadPower) / total >= 0.4;
        n->qualityCount = 0;
        n->inPhasePower = n->quadPower = 0;
        if (!n->quality) {
            n->snapshot.bitSynchronized = n->snapshot.towValid = 0;
            n->transitions = -1;
            n->collecting = 0;
            n->bitMs = 0;
            n->sum = 0;
        }
    }
    if (!n->snapshot.bitSynchronized && n->quality && i * n->previous < 0) {
        if (n->transitions < 0 || (n->epochs - n->transition) % 20 != 0)
            n->transitions = 0;
        else
            n->transitions++;
        n->transition = n->epochs;
        if (n->transitions >= 5) {
            n->snapshot.bitSynchronized = 1;
            n->sum = 0;
            n->bitMs = 0;
        }
    }
    n->previous = i;
    if (n->snapshot.bitSynchronized) {
        n->sum += i;
        if (++n->bitMs == 20) {
            NgNavigationGpsBit(n, n->sum > 0 ? 0u : 1u, endSample);
            n->sum = 0;
            n->bitMs = 0;
        }
    }
}
