/** @file test_beidou_b1c.c @brief Primary code and baseband conformance offline. */
#include "ngene/dsp/beidou_b1c.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "line %d: %s\n", __LINE__, #x);                                        \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

/* Full-period FNV-1a oracles, data then pilot, from the independent generator
 * described in PIANO_INCREMENTALE_EVO.md. Hash bytes are logical bits 0 or 1. */
static const uint32_t expected[2][63] = {
    {0xad45bf20u, 0x9ea9691au, 0x85bd1ebcu, 0xe04c6de0u, 0x11a9e814u, 0x35cfbbb0u, 0x3bdedaceu,
     0xb8332884u, 0xbb0df7e8u, 0x52685bc2u, 0xecc59586u, 0x38e60ac8u, 0x386c12b8u, 0x45dd4604u,
     0x1f1594aau, 0x0a3432f4u, 0x1970c1b8u, 0xba79610eu, 0x67fe5606u, 0x47167282u, 0x7e387694u,
     0xebea766cu, 0xfdf6697cu, 0xab41be0au, 0xabdf6e20u, 0x1c21b4d0u, 0xb1847ce2u, 0xcfeec526u,
     0xc320e36cu, 0xdc3fb6bau, 0xaf4c5e42u, 0x7931b522u, 0xfdffb2c4u, 0x16c0194cu, 0x3d1933d4u,
     0x1932856au, 0x07065e38u, 0xf97159f4u, 0xa204a1d8u, 0xad958864u, 0xed18472eu, 0x8a297e62u,
     0x535f84aeu, 0x5a7064cau, 0xf1fbfc24u, 0xcc033720u, 0xf86c1f72u, 0x5bba7cfau, 0x85a33c6au,
     0xdd646b32u, 0x3b95f710u, 0x6bc09e4eu, 0x7dd2dfb4u, 0x8c83731eu, 0x9742d3f8u, 0x9d393520u,
     0x6afdbc8eu, 0x028861e6u, 0x1e4a0a26u, 0xcac11ff6u, 0x8552eca2u, 0x9bd82fa6u, 0x9fd987a0u},
    {0x4204afdau, 0xe957c160u, 0x83faadc0u, 0x5225d8f4u, 0x688e5cf0u, 0x825fb938u, 0x84825740u,
     0xc82a27feu, 0xef34c33eu, 0xcb0659dcu, 0xaee24862u, 0xb4a3abeeu, 0x0eb01adcu, 0xec26f86eu,
     0xb63bd772u, 0xdf14fb14u, 0xafc20bccu, 0x82e884a2u, 0x7950018cu, 0xf197da5cu, 0x2f7c0aa8u,
     0xf39bc0bcu, 0xa099b5a2u, 0x3ed0a6beu, 0x64762584u, 0xa12c0742u, 0xbc81bbb8u, 0xb09536b4u,
     0xc505c398u, 0xe5a26404u, 0xb2452200u, 0x85055be8u, 0x0e5023d0u, 0x535ad406u, 0x3d468eaeu,
     0x1aac692au, 0x983dc59cu, 0x9dc9ae04u, 0x422d3692u, 0xac137210u, 0x5015e074u, 0x685d0df0u,
     0xb8e4cdbcu, 0x3f05e5b6u, 0x9279d566u, 0x821f5e28u, 0x75ce963au, 0xcd2efbbau, 0x861e4ddau,
     0x140dd94au, 0xcf421160u, 0x86b94566u, 0xc2136730u, 0x932a43b4u, 0xf3f22022u, 0xceae8e0au,
     0xe1dbfa4au, 0xa33d5676u, 0x1aefd14au, 0x64725104u, 0x5e939074u, 0xe8ec1db6u, 0x761e085au}};

/* First and last 24 logical bits, MSB first, from ICD Tables 5-2 and 5-3. */
static const unsigned boundaryPrns[] = {1, 32, 33, 63};
static const uint32_t boundaries[2][4][2] = {{{053773116u, 042711657u},
                                              {060523643u, 052040645u},
                                              {030522043u, 036645510u},
                                              {027571255u, 047160627u}},
                                             {{071676756u, 013053205u},
                                              {073077145u, 034360276u},
                                              {055454316u, 045431517u},
                                              {003210227u, 056250500u}}};

static uint32_t Pack24(const int8_t *chips)
{
    uint32_t bits = 0;
    for (unsigned i = 0; i < 24; ++i)
        bits = (bits << 1) | (uint32_t)(chips[i] < 0);
    return bits;
}

static void CheckCodes(void)
{
    int8_t chips[NG_BEIDOU_B1C_LENGTH] = {0};
    TNgError error = {{0}};
    CHECK(NgBeidouB1cGenerate(0, NG_BEIDOU_B1C_DATA, chips, &error) == NG_ERROR);
    CHECK(NgBeidouB1cGenerate(64, NG_BEIDOU_B1C_PILOT, chips, &error) == NG_ERROR);
    CHECK(NgBeidouB1cGenerate(1, (TNgBeidouB1cComponent)2, chips, &error) == NG_ERROR);
    CHECK(NgBeidouB1cGenerate(1, NG_BEIDOU_B1C_DATA, NULL, &error) == NG_ERROR);
    for (unsigned i = 0; i < NG_BEIDOU_B1C_LENGTH; ++i)
        CHECK(chips[i] == 0);

    for (unsigned c = 0; c < 2; ++c) {
        for (unsigned prn = 1; prn <= 63; ++prn) {
            uint32_t hash = UINT32_C(2166136261);
            CHECK(NgBeidouB1cGenerate(prn, (TNgBeidouB1cComponent)c, chips, &error) == NG_OK);
            for (unsigned i = 0; i < NG_BEIDOU_B1C_LENGTH; ++i) {
                CHECK(chips[i] == 1 || chips[i] == -1);
                hash = (hash ^ (uint32_t)(chips[i] < 0)) * UINT32_C(16777619);
            }
            if (hash != expected[c][prn - 1])
                fprintf(stderr, "primary component %u PRN %u hash mismatch\n", c, prn);
            CHECK(hash == expected[c][prn - 1]);
        }
        for (unsigned i = 0; i < sizeof(boundaryPrns) / sizeof(*boundaryPrns); ++i) {
            CHECK(NgBeidouB1cGenerate(boundaryPrns[i], (TNgBeidouB1cComponent)c, chips, &error) ==
                  NG_OK);
            CHECK(Pack24(chips) == boundaries[c][i][0]);
            CHECK(Pack24(chips + NG_BEIDOU_B1C_LENGTH - 24) == boundaries[c][i][1]);
        }
    }
}

static void CheckWaveforms(void)
{
    int8_t data[NG_BEIDOU_B1C_LENGTH], pilot[NG_BEIDOU_B1C_LENGTH];
    const uint64_t period = NG_BEIDOU_B1C_LENGTH * NG_BEIDOU_B1C_SUBCHIPS;
    const int boc1[] = {1, 1, 1, 1, 1, 1, -1, -1, -1, -1, -1, -1};
    const int boc6[] = {1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1};
    double dataPower = 0.0, pilotI = 0.0, pilotQ = 0.0, cross = 0.0;
    CHECK(NgBeidouB1cGenerate(1, NG_BEIDOU_B1C_DATA, data, NULL) == NG_OK);
    CHECK(NgBeidouB1cGenerate(63, NG_BEIDOU_B1C_PILOT, pilot, NULL) == NG_OK);

    for (uint64_t p = 0; p < period; ++p) {
        unsigned chip = (unsigned)(p / NG_BEIDOU_B1C_SUBCHIPS);
        unsigned sub = (unsigned)(p % NG_BEIDOU_B1C_SUBCHIPS);
        const double d = NgBeidouB1cDataAt(data, p);
        TNgComplex q = NgBeidouB1cPilotAt(pilot, p);
        const TNgComplex wrapped = NgBeidouB1cPilotAt(pilot, p + period);
        CHECK(d == 0.5 * data[chip] * boc1[sub]);
        CHECK(fabs(q.i - sqrt(1.0 / 11.0) * pilot[chip] * boc6[sub]) < 1e-7);
        CHECK(fabs(q.q - sqrt(29.0 / 44.0) * pilot[chip] * boc1[sub]) < 1e-7);
        CHECK(d == NgBeidouB1cDataAt(data, p + period));
        CHECK(q.i == wrapped.i && q.q == wrapped.q);
        dataPower += d * d;
        pilotI += (double)q.i * q.i;
        pilotQ += (double)q.q * q.q;
        cross += d * q.i;
    }
    const double n = (double)period;
    CHECK(fabs(dataPower / n - 0.25) < 1e-7);
    CHECK(fabs(pilotI / n - 1.0 / 11.0) < 1e-7);
    CHECK(fabs(pilotQ / n - 29.0 / 44.0) < 1e-7);
    CHECK(fabs(cross / n) < 1e-7);
    puts("126 full B1C primary periods, ICD boundaries, waveform phase and powers verified");
}

int main(void)
{
    CheckCodes();
    CheckWaveforms();
    return 0;
}
