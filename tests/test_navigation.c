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
        for (bit = 0; bit < 32; ++bit)
            count += ((word & matrix[k]) >> bit) & 1u;
        word |= (count & 1u) << (5 - k);
    }
    if (previous & 1u)
        word ^= UINT32_C(0x3fffffc0);
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
        if (corrupt && k == 5)
            word ^= 1u << 15;
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
        for (k = 0; k < 301; ++k)
            NgNavigationGpsBit(&n, 0, 0);
        CHECK(!n.snapshot.towValid); /* A stale HOW never remains current indefinitely. */
    }
    NgNavigationReset(&n);
    n.snapshot.bitSynchronized = 1;
    Frame(&n, 0, 1);
    CHECK(!n.snapshot.validSubframes && !n.snapshot.towValid && n.snapshot.parityErrors);
    NgNavigationReset(&n);
    for (k = 0; k < 300; ++k)
        NgNavigationGpsPrompt(&n, (k / 20) & 1u ? -20 : 20, 0, 1, k * 2400u);
    CHECK(n.snapshot.bitSynchronized && n.snapshot.symbols > 0);
    NgNavigationGpsPrompt(&n, 20, 0, 0, 0);
    CHECK(!n.snapshot.bitSynchronized && !n.snapshot.towValid && !n.snapshot.symbols);
    for (k = 0; k < 300; ++k)
        NgNavigationGpsPrompt(&n, (k / 20) & 1u ? -1 : 1, 20, 1, k * 2400u);
    CHECK(!n.snapshot.bitSynchronized && !n.snapshot.towValid);
    puts("GPS prompt synchronization, polarity, full-word parity and reset passed");
    return 0;
}
