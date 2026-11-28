/** @file test_sbas.c @brief EGNOS wire fields, FEC phase and correction eligibility. */
#include "ngene/sbas.h"
#include "navigation/sbas_stream.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

/** Put a synthetic field into an initially zero frame. */
static void Pack(uint8_t *bytes, unsigned start, unsigned size, unsigned value)
{
    for (unsigned k = 0; k < size; ++k) {
        const unsigned bit = start + k;
        bytes[bit / 8] |= (uint8_t)(((value >> (size - 1 - k)) & 1u) << (7 - bit % 8));
    }
}

/** Append the check field using explicit GF(2) polynomial division. */
static void Seal(uint8_t *bytes)
{
    uint8_t work[250] = {0};
    const unsigned poly = 0x1864cfb;
    for (unsigned k = 0; k < 226; ++k) {
        work[k] = (uint8_t)(bytes[k / 8] >> (7 - k % 8) & 1u);
    }
    for (unsigned k = 0; k < 226; ++k) {
        if (work[k]) {
            for (unsigned j = 0; j <= 24; ++j) {
                work[k + j] ^= (uint8_t)(poly >> (24 - j) & 1u);
            }
        }
    }
    for (unsigned k = 226; k < 250; ++k) {
        Pack(bytes, k, 1, work[k]);
    }
}

/** Create MT1 for GPS 7, MT2 for +5 m, MT25 for a matched static correction or MT0. */
static void Frame(uint8_t *bytes, unsigned preamble, unsigned type)
{
    memset(bytes, 0, NG_SBAS_FRAME_BYTES);
    Pack(bytes, 0, 8, preamble);
    Pack(bytes, 8, 6, type);
    if (type == 1) {
        Pack(bytes, 20, 1, 1);
        Pack(bytes, 224, 2, 2);
    } else if (type == 2) {
        Pack(bytes, 14, 2, 1);
        Pack(bytes, 16, 2, 2);
        Pack(bytes, 18, 12, 40);
    } else if (type == 24) {
        Pack(bytes, 14, 12, 48);
        Pack(bytes, 110, 2, 2);
        Pack(bytes, 114, 2, 1);
    } else if (type == 25) {
        Pack(bytes, 15, 6, 1);
        Pack(bytes, 21, 8, 73);
        Pack(bytes, 29, 9, 16);
        Pack(bytes, 38, 9, (unsigned)-8);
        Pack(bytes, 56, 10, 80);
        Pack(bytes, 117, 2, 2);
    }
    Seal(bytes);
}

/** Encode newest-to-oldest symbol taps independently of the decoder register masks. */
static unsigned Encode(unsigned state, unsigned polynomial)
{
    unsigned result = 0;
    for (unsigned k = 0; k < 7; ++k) {
        result ^= (state >> k & 1u) & (polynomial >> (6 - k) & 1u);
    }
    return result;
}

/** Simulate a receiver starting at either sample phase and either Costas polarity. */
static void Stream(unsigned shift, int polarity)
{
    TNgSbasStream stream = {0};
    TNgSbasCorrections bank = {0};
    uint8_t frame[NG_SBAS_FRAME_BYTES];
    const unsigned types[] = {1, 2, 25, 63, 63};
    const unsigned preambles[] = {0x53, 0x9a, 0xc6};
    unsigned state = 0, ms = 0;
    for (unsigned k = 0; k < shift; ++k) {
        NgSbasPrompt(&stream, &bank, 0, (double)++ms / 1000);
    }
    for (unsigned f = 0; f < 5; ++f) {
        Frame(frame, preambles[f % 3], types[f]);
        for (unsigned k = 0; k < 250; ++k) {
            state = ((state << 1) | ((unsigned)frame[k / 8] >> (7 - k % 8) & 1u)) & 127u;
            const unsigned symbols[] = {Encode(state, 0171u), Encode(state, 0133u)};
            for (unsigned s = 0; s < 2; ++s) {
                const double prompt = symbols[s] ? -polarity : polarity;
                NgSbasPrompt(&stream, &bank, prompt, (double)++ms / 1000);
                NgSbasPrompt(&stream, &bank, prompt, (double)++ms / 1000);
            }
        }
    }
    TNgSbasCorrection result;
    CHECK(bank.frameCount == 4);
    CHECK(NgSbasRead(&bank, 7, 73, (double)ms / 1000, 123, &result));
    CHECK(result.range == 5 && result.position[0] == 2 && result.position[1] == -1);
    CHECK(result.clock == ldexp(80, -31));
}

static void Ionosphere(void)
{
    TNgSbasCorrections bank = {0};
    uint8_t frame[NG_SBAS_FRAME_BYTES] = {0};
    const unsigned points[] = {122, 123, 148, 149};
    const double degree = 0.017453292519943295;
    double delay;
    Pack(frame, 0, 8, 0x53);
    Pack(frame, 8, 6, 18);
    Pack(frame, 14, 4, 1);
    Pack(frame, 18, 4, 4);
    Pack(frame, 22, 2, 2);
    for (unsigned i = 0; i < 4; ++i) {
        Pack(frame, 23 + points[i], 1, 1);
    }
    Seal(frame);
    CHECK(NgSbasPushFrame(&bank, frame, 1));
    CHECK(!NgSbasIonosphere(&bank, 1, 42.5 * degree, 2.5 * degree, 0, 90 * degree, &delay));
    for (unsigned issue = 1; issue <= 2; ++issue) {
        memset(frame, 0, sizeof(frame));
        Pack(frame, 0, 8, 0x9a);
        Pack(frame, 8, 6, 26);
        Pack(frame, 14, 4, 4);
        Pack(frame, 217, 2, issue);
        for (unsigned i = 0; i < 4; ++i) {
            Pack(frame, 22 + 13 * i, 9, 8 * (i + 1));
        }
        Seal(frame);
        CHECK(NgSbasPushFrame(&bank, frame, 1 + issue));
        CHECK(NgSbasIonosphere(&bank, 1 + issue, 42.5 * degree, 2.5 * degree, 0,
                               90 * degree, &delay) == (issue == 2));
    }
    CHECK(fabs(delay - 2.5) < 1e-12);
    memset(frame, 0, sizeof(frame));
    Pack(frame, 0, 8, 0xc6);
    Pack(frame, 8, 6, 26);
    Pack(frame, 14, 4, 4);
    Pack(frame, 217, 2, 2);
    for (unsigned i = 0; i < 3; ++i) {
        Pack(frame, 22 + 13 * i, 9, 8 * (i + 1));
    }
    Pack(frame, 70, 4, 15);
    Seal(frame);
    CHECK(NgSbasPushFrame(&bank, frame, 4));
    CHECK(NgSbasIonosphere(&bank, 4, 41 * degree, degree, 0, 90 * degree, &delay));
    CHECK(fabs(delay - 1.6) < 1e-12);
    CHECK(!NgSbasIonosphere(&bank, 4, 44 * degree, 4 * degree, 0, 90 * degree, &delay));
    CHECK(!NgSbasIonosphere(&bank, 9, 42.5 * degree, 2.5 * degree, 0, 90 * degree, &delay));
}

/** Exercise corruption, issue matching, test-mode suppression and decoder phase. */
int main(void)
{
    Ionosphere();
    for (unsigned k = 0; k < 4; ++k) {
        Stream(k, 1);
        Stream(k, -1);
    }
    TNgSbasCorrections bank = {0};
    TNgSbasCorrection result;
    uint8_t frame[NG_SBAS_FRAME_BYTES];
    Frame(frame, 0x53, 1);
    frame[8] ^= 0x40;
    CHECK(!NgSbasPushFrame(&bank, frame, 0.0));
    frame[8] ^= 0x40;
    CHECK(NgSbasPushFrame(&bank, frame, 1.0));
    Frame(frame, 0x9a, 2);
    CHECK(NgSbasPushFrame(&bank, frame, 2.0));
    Frame(frame, 0xc6, 25);
    CHECK(NgSbasPushFrame(&bank, frame, 3.0));
    CHECK(NgSbasRead(&bank, 7, 73, 4.0, 0, &result));
    CHECK(!NgSbasRead(&bank, 7, 74, 4.0, 0, &result));
    CHECK(!NgSbasRead(&bank, 7, 73, 16.0, 0, &result));
    Frame(frame, 0x53, 24);
    CHECK(NgSbasPushFrame(&bank, frame, 4.0));
    CHECK(NgSbasRead(&bank, 7, 73, 4.0, 0, &result) && result.range == 6);
    Frame(frame, 0x53, 0);
    CHECK(NgSbasPushFrame(&bank, frame, 5.0));
    CHECK(!NgSbasRead(&bank, 7, 73, 5.0, 0, &result));
    Frame(frame, 0x9a, 1);
    CHECK(NgSbasPushFrame(&bank, frame, 6.0) && bank.count == 0);
    puts("EGNOS FEC, corrections, CRC, aging, and test-mode suppression passed");
    return 0;
}
