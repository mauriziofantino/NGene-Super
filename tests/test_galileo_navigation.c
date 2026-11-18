/** @file test_galileo_navigation.c @brief Independent convolutional encoder and CRC fixtures. */
#include "ngene/navigation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "line %d: %s\n", __LINE__, #x);                                        \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

/** Set MSB-first field bits in the test's unpacked navigation word. */
static void Field(unsigned char *data, unsigned offset, unsigned count, unsigned value)
{
    unsigned k;
    for (k = 0; k < count; k++)
        data[offset + k] = (unsigned char)((value >> (count - k - 1)) & 1u);
}

/** Independent parity by population count. */
static unsigned Parity(unsigned value)
{
    unsigned parity = 0;
    while (value) {
        parity ^= value & 1u;
        value >>= 1;
    }
    return parity;
}

/** Encode terminated K=7 with G1=171oct/G2=133oct in the Classic shift orientation. */
static void Half(TNgNavigation *n, const unsigned char bits[120], unsigned inverted)
{
    unsigned char coded[240], interleaved[240];
    unsigned k, state = 0;
    for (k = 0; k < 120; k++) {
        state = ((state << 1) | bits[k]) & 127u;
        coded[2 * k] = (unsigned char)Parity(state & 0x4fu);
        coded[2 * k + 1] = (unsigned char)(Parity(state & 0x6du) ^ 1u);
    }
    for (k = 0; k < 240; k++)
        interleaved[(k & 7u) * 30 + k / 8] = coded[k];
    for (k = 0; k < 10; k++) {
        unsigned symbol = ((0x160u >> (9 - k)) & 1u) ^ inverted;
        NgNavigationGalileoPrompt(n, symbol ? -20 : 20, 1, n->snapshot.symbols * 9600 + 9600);
    }
    for (k = 0; k < 240; k++) {
        unsigned symbol = interleaved[k] ^ inverted;
        NgNavigationGalileoPrompt(n, symbol ? -20 : 20, 1, n->snapshot.symbols * 9600 + 9600);
    }
}

/** Build an I/NAV time word and its full even/odd protected page. */
static void Page(TNgNavigation *n, unsigned inverted, int corrupt, const unsigned char osnma[5])
{
    unsigned char data[128] = {0}, even[120] = {0}, odd[120] = {0};
    uint32_t crc = 0;
    unsigned k;
    Field(data, 6, 2, 2);
    Field(data, 96, 12, 1419);
    Field(data, 108, 20, 345600);
    memcpy(even + 2, data, 112);
    odd[0] = 1;
    memcpy(odd + 2, data + 112, 16);
    if (osnma)
        for (k = 0; k < 40; ++k)
            odd[18 + k] = (unsigned char)((osnma[k / 8] >> (7 - k % 8)) & 1u);
    for (k = 0; k < 220; k++) {
        unsigned bit = k < 114 ? even[k] : k < 196 ? odd[k - 114] : 0;
        crc = (crc << 1) | bit;
        if (crc & 0x1000000u)
            crc ^= 0x1864cfbu;
    }
    Field(odd, 82, 24, crc);
    if (corrupt)
        odd[90] ^= 1;
    Half(n, even, inverted);
    Half(n, odd, inverted);
}

/** Verify end-to-end FEC, both carrier polarities, time boundary and bad CRC rejection. */
int main(void)
{
    TNgNavigation n;
    const unsigned char osnma[5] = {0xa5, 0x13, 0x42, 0xbe, 0x7c};
    unsigned inverted;
    for (inverted = 0; inverted < 2; inverted++) {
        NgNavigationReset(&n);
        Page(&n, inverted, 0, osnma);
        CHECK(n.snapshot.validSubframes == 1 && n.snapshot.towValid);
        CHECK(n.snapshot.broadcast.wn == 1419 && n.snapshot.towSeconds == 345602);
        CHECK(n.snapshot.boundarySample == 4800000);
        CHECK(n.snapshot.osnma.valid && n.snapshot.osnma.pages == 1);
        CHECK(n.snapshot.osnma.hkroot == osnma[0] &&
              memcmp(n.snapshot.osnma.mack, osnma + 1, 4) == 0);
        CHECK(n.snapshot.osnma.boundarySample == 4800000);
    }
    NgNavigationReset(&n);
    Page(&n, 0, 1, osnma);
    CHECK(!n.snapshot.validSubframes && !n.snapshot.towValid && n.snapshot.parityErrors == 1);
    CHECK(!n.snapshot.osnma.valid && n.snapshot.osnma.pages == 0);
    puts("Galileo convolutional FEC, deinterleaving, CRC and time boundary passed");
    return 0;
}
