/** @file galileo.c @brief Classic E1-B I/NAV framing, deinterleaving, FEC and CRC24Q.
 * Adapted from galileoMessageParser.c, Mario Nicola, 2014. All state is channel-owned.
 */
#include "ngene/navigation.h"
#include <math.h>
#include <string.h>

/** Classic rate-1/2 K=7 decoder; bounded 120-bit terminated trellis, private metrics. */
int NgGalileoViterbi(long *finalPathMetric, unsigned char *decData, unsigned char *symbols,
                     unsigned numOutBits, unsigned startState, unsigned endState);

/** Polynomial division identical to Classic's byte table; includes the transmitted CRC. */
static int Crc(const unsigned char data[28])
{
    uint32_t remainder = 0;
    unsigned i, b;
    for (i = 0; i < 28; ++i)
        for (b = 0; b < 8; ++b) {
            unsigned carry = remainder >> 23;
            remainder = ((remainder << 1) | ((data[i] >> (7 - b)) & 1u)) & 0xffffffu;
            if (carry)
                remainder ^= 0x864cfbu;
        }
    return remainder == 0;
}

/** Consume every 4-ms E1-B data symbol; no display or controller sampling participates. */
void NgNavigationGalileoPrompt(TNgNavigation *n, double i, int pll, uint64_t endSample)
{
    unsigned symbol;
    unsigned k;
    unsigned char coded[240], decoded[15], crc[28] = {0}, data[16] = {0};
    long metric;
    if (!pll || !isfinite(i)) {
        NgNavigationLoseTiming(n);
        return;
    }
    n->snapshot.symbols++;
    if (++n->galileo.age > 7500)
        n->snapshot.towValid = 0;
    symbol = i > 0 ? 0u : 1u;
    if (!n->galileo.collecting) {
        n->galileo.preamble = ((n->galileo.preamble << 1) | symbol) & 1023u;
        if (n->galileo.preamble == 0x160 || n->galileo.preamble == 0x29f) {
            n->galileo.polarity = n->galileo.preamble == 0x29f;
            n->galileo.count = 0;
            n->galileo.collecting = 1;
        }
        return;
    }
    n->galileo.symbols[n->galileo.count++] = (unsigned char)(symbol ^ n->galileo.polarity);
    if (n->galileo.count != 240)
        return;
    n->galileo.collecting = 0;
    n->galileo.preamble = 0;
    for (k = 0; k < 240; ++k)
        coded[k] = n->galileo.symbols[(k & 7u) * 30 + k / 8];
    if (NgGalileoViterbi(&metric, decoded, coded, 120, 0, 0))
        return;
    if (!(decoded[0] & 128u)) {
        memcpy(n->galileo.even, decoded, 15);
        n->galileo.haveEven = 1;
        n->galileo.evenSymbol = n->snapshot.symbols;
        return;
    }
    if (!n->galileo.haveEven || n->snapshot.symbols - n->galileo.evenSymbol != 250)
        return;
    n->galileo.haveEven = 0;
    memcpy(crc, n->galileo.even, 14);
    crc[14] = n->galileo.even[14] & 0xc0u;
    for (k = 14; k < 27; ++k) {
        crc[k] |= (decoded[k - 14] >> 2) & 63u;
        crc[k + 1] = (unsigned char)(decoded[k - 14] << 6);
    }
    crc[27] |= (decoded[13] >> 2) & 0x30u;
    if (!Crc(crc)) {
        n->snapshot.parityErrors++;
        return;
    }
    /* Alert pages have no operational navigation data. */
    if ((decoded[0] | n->galileo.even[0]) & 64u)
        return;
    /* Classic ExtractGalreservedBits: odd-page bits 18..57 hold 8 HKROOT
       and 32 MACK bits. Publish only after the page CRC has passed. */
    n->snapshot.osnma.hkroot = (uint8_t)((decoded[2] << 2) | (decoded[3] >> 6));
    for (k = 0; k < 4; ++k)
        n->snapshot.osnma.mack[k] = (uint8_t)((decoded[3 + k] << 2) | (decoded[4 + k] >> 6));
    n->snapshot.osnma.boundarySample = endSample;
    n->snapshot.osnma.pages++;
    n->snapshot.osnma.valid = 1;
    for (k = 0; k < 14; ++k)
        data[k] = (unsigned char)((n->galileo.even[k] << 2) | (n->galileo.even[k + 1] >> 6));
    for (; k < 16; ++k)
        data[k] = (unsigned char)((decoded[k - 14] << 2) | (decoded[k - 13] >> 6));
    n->snapshot.bitSynchronized = 1;
    n->snapshot.validSubframes++;
    n->snapshot.subframeId = data[0] >> 2;
    n->galileo.age = 0;
    if (NgBroadcastGalileo(&n->snapshot.broadcast, data)) {
        /* Galileo time words refer to the start of the two-second page. */
        n->snapshot.towSeconds = ((unsigned)n->snapshot.broadcast.tow + 2u) % 604800u;
        n->snapshot.towValid = 1;
        n->snapshot.boundarySample = endSample;
    }
}
