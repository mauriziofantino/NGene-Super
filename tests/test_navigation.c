/** @file test_navigation.c @brief LNAV framing, polarity, corruption and prompt timing. */
#include "ngene/navigation.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "line %d: %s\n", __LINE__, #x);                                        \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

/** Encode fixture words from the Classic parity matrix, independently of decoder folding. */
static uint32_t Word(uint32_t data, unsigned previous)
{
    static const uint32_t matrix[6] = {0xbb1f3480, 0x5d8f9a40, 0xaec7cd00,
                                       0x5763e680, 0x6bb1f340, 0x8b7a89c0};
    uint32_t word = (previous << 30) | (data << 6);
    unsigned k, bit;
    for (k = 0; k < 6; ++k) {
        unsigned count = 0;
        for (bit = 0; bit < 32; ++bit) {
            count += ((word & matrix[k]) >> bit) & 1u;
        }
        word |= (count & 1u) << (5 - k);
    }
    if (previous & 1u) {
        word ^= UINT32_C(0x3fffffc0);
    }
    return word & UINT32_C(0x3fffffff);
}

/** Feed known data through complete 20-ms coherent prompt integrations. */
static void Frame(TNgNavigation *n, int inverted, int corrupt)
{
    unsigned k, b, ms, previous = 0;
    uint64_t sample = 0;
    for (k = 0; k < 10; ++k) {
        uint32_t data = k == 0 ? UINT32_C(0x8b0000) : k == 1 ? (12345u << 7) | (2u << 2) : 0;
        uint32_t word = Word(data, previous);
        previous = word & 3u;
        if (corrupt && k == 5) {
            word ^= 1u << 15;
        }
        for (b = 0; b < 30; ++b) {
            unsigned bit = ((word >> (29 - b)) & 1u) ^ (unsigned)inverted;
            for (ms = 0; ms < 20; ++ms) {
                sample += 2400;
                NgNavigationGpsPrompt(n, bit ? -20.0 : 20.0, 0, 1, sample);
            }
        }
    }
}

/** Verify real prompt-to-bit framing and rejection, independently of browser telemetry. */
int main(void)
{
    TNgNavigation n;
    unsigned inversion, k;
    {
        TNgBroadcast b = {0};
        unsigned char bytes[40] = {0};
        uint32_t words[10];
        bytes[8] = 0x1e;  /* Data ID 1, page SV ID 56. */
        bytes[10] = 0x40; /* Alpha 0 raw coefficient +1. */
        for (k = 0; k < 10; ++k) {
            words[k] = ((uint32_t)bytes[4 * k] << 24) | ((uint32_t)bytes[4 * k + 1] << 16) |
                       ((uint32_t)bytes[4 * k + 2] << 8) | bytes[4 * k + 3];
        }
        CHECK(NgBroadcastGps(&b, words, 4) == 0 && b.ionoReceived);
        CHECK(b.iono.alphaGps[0] > 0 && b.iono.alphaGps[0] < 1e-8);
        bytes[8] = 0x1d; /* Different page must retain prior coefficients. */
        words[2] = ((uint32_t)bytes[8] << 24) | ((uint32_t)bytes[9] << 16) |
                   ((uint32_t)bytes[10] << 8) | bytes[11];
        CHECK(NgBroadcastGps(&b, words, 4) == 0 && b.iono.alphaGps[0] > 0);
    }
    for (inversion = 0; inversion < 2; ++inversion) {
        NgNavigationReset(&n);
        /* Already aligned bit timing isolates framing; full synchronization tested below. */
        n.snapshot.bitSynchronized = 1;
        n.shift = inversion ? 3u : 0u; /* Preceding D29/D30 in received polarity. */
        Frame(&n, (int)inversion, 0);
        CHECK(n.snapshot.symbols == 300);
        CHECK(n.snapshot.validSubframes == 1 && n.snapshot.towValid);
        CHECK(n.snapshot.towSeconds == 12345u * 6u && n.snapshot.subframeId == 2);
        CHECK(n.snapshot.boundarySample == UINT64_C(14400000));
        for (k = 0; k < 301; ++k) {
            NgNavigationGpsBit(&n, 0, 0);
        }
        CHECK(!n.snapshot.towValid); /* A stale HOW never remains current indefinitely. */
    }
    NgNavigationReset(&n);
    n.snapshot.bitSynchronized = 1;
    Frame(&n, 0, 1);
    CHECK(!n.snapshot.validSubframes && !n.snapshot.towValid && n.snapshot.parityErrors);
    NgNavigationReset(&n);
    for (k = 0; k < 300; ++k) {
        NgNavigationGpsPrompt(&n, (k / 20) & 1u ? -20 : 20, 0, 1, k * 2400u);
    }
    CHECK(n.snapshot.bitSynchronized && n.snapshot.symbols > 0);
    NgNavigationGpsPrompt(&n, 20, 0, 0, 0);
    CHECK(!n.snapshot.bitSynchronized && !n.snapshot.towValid && !n.snapshot.symbols);
    n.snapshot.broadcast.ephemeridesReceived = 1;
    n.snapshot.broadcast.eph.sqrtA = 5153.7;
    NgNavigationGpsPrompt(&n, 20, 0, 0, 0);
    CHECK(n.snapshot.broadcast.ephemeridesReceived && n.snapshot.broadcast.eph.sqrtA == 5153.7);
    for (k = 0; k < 300; ++k) {
        NgNavigationGpsPrompt(&n, (k / 20) & 1u ? -1 : 1, 20, 1, k * 2400u);
    }
    CHECK(!n.snapshot.bitSynchronized && !n.snapshot.towValid);
    puts("GPS prompt synchronization, polarity, full-word parity and reset passed");
    return 0;
}
