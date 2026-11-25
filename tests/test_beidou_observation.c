/** @file test_beidou_observation.c @brief B-CNAV1 cache and independently set timing/orbit values. */
#include "ngene/beidou_observation.h"
#include "ngene/pvt.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

/* Independently encoded SF1/SF2/SF3 with the same orbital fields as Fixture(). */
static const uint8_t encoded[] = {
    0xfc, 0xe2, 0x36, 0x3f, 0x24, 0xe5, 0x7b, 0xfd, 0x74, 0x4b, 0x44, 0x3c,
    0x18, 0xa4, 0x04, 0x01, 0x8c, 0xa2, 0x43, 0x01, 0xd3, 0xdc, 0xd0, 0x30,
    0x01, 0xa2, 0x66, 0xc3, 0x48, 0x80, 0x9c, 0x0e, 0x04, 0x10, 0x3e, 0xd4,
    0xc0, 0x00, 0x46, 0x83, 0x2d, 0x10, 0x00, 0x64, 0x89, 0xd1, 0x41, 0x54,
    0x6d, 0xf5, 0x24, 0x11, 0xd9, 0x8c, 0xd0, 0x01, 0x2b, 0xdc, 0xf5, 0x32,
    0x57, 0x66, 0xa5, 0xc1, 0x45, 0xf7, 0x3d, 0xcc, 0x04, 0x43, 0x10, 0x71,
    0x12, 0x0c, 0x91, 0xe1, 0xc9, 0x10, 0xcc, 0x38, 0x22, 0x11, 0x4c, 0xd9,
    0xfb, 0xf0, 0x04, 0xcc, 0x65, 0x4e, 0x80, 0x0c, 0x84, 0xd8, 0xb8, 0x14,
    0xcb, 0xaa, 0x4f, 0x81, 0x4d, 0xf7, 0xbb, 0x50, 0x24, 0x9c, 0x4a, 0x8a,
    0x02, 0x69, 0xd5, 0xb4, 0x20, 0x16, 0x9b, 0x53, 0x0b, 0x01, 0x69, 0x6f,
    0x00, 0x20, 0x26, 0x8c, 0x2b, 0x18, 0x82, 0x69, 0xd0, 0xdd, 0x00, 0x14,
    0x87, 0x2f, 0x16, 0x01, 0x41, 0xa1, 0x42, 0xd9, 0x20, 0x82, 0xf5, 0x98,
    0x12, 0x01, 0x5e, 0x66, 0x01, 0x10, 0x04, 0x89, 0x6d, 0x01, 0x04, 0x59,
    0x74, 0xa5, 0x20, 0x51, 0x53, 0x18, 0x83, 0x05, 0x61, 0xfc, 0xf1, 0x00,
    0x49, 0x49, 0xa3, 0x00, 0x04, 0x5a, 0xbd, 0x88, 0xb0, 0x1f, 0x77, 0xf9,
    0x12, 0x05, 0x75, 0xb6, 0xa9, 0x80, 0x46, 0x76, 0x98, 0x81, 0x05, 0xe5,
    0xac, 0x58, 0x30, 0x45, 0x06, 0x54, 0x82, 0x04, 0x5e, 0x9d, 0xa8, 0x80,
    0x03, 0x86, 0xbb, 0x09, 0x40, 0x78, 0xdc, 0x88, 0xb0, 0x05, 0xb8, 0x21,
    0x8a, 0x00, 0x58, 0xaf, 0xd9, 0x00, 0x44, 0x84, 0xb5,
};

/** Write a signed or unsigned field as its two's-complement wire bits. */
static void Pack(uint8_t *data, unsigned *cursor, unsigned width, int64_t value)
{
    for (unsigned i = 0; i < width; ++i, ++*cursor)
        data[*cursor / 8] |= (uint8_t)((((uint64_t)value >> (width - 1 - i)) & 1u)
                                       << (7u - *cursor % 8));
}

/** Assemble known signed IGSO elements independently of the unpacker. */
static TNgBcnv1Frame Fixture(void)
{
    const unsigned widths[] = {11, 2, 26, 25, 17, 23, 33, 33, 33, 33, 33, 19, 15, 16, 16, 24, 24, 21, 21, 11, 25, 22, 11, 12, 12, 12};
    const int64_t values[] = {2015, 2, -65536, 2097152, -3200, 240000, -INT64_C(2147483648), 171798692, 429496730, -858993459, 1288490189, -1024, 512, 1024, -2048, 512, -768, 1024, -2048, 2015, 1200000, -87654, 5, 23, -19, 41};
    TNgBcnv1Frame frame = {.prn = 63, .week = 501, .hour = 167, .soh = 199,
                           .iodc = 18, .iode = 18, .pageId = 1};
    unsigned bit = 39;
    for (unsigned k = 0; k < sizeof(widths) / sizeof(*widths); ++k)
        Pack(frame.sf2, &bit, widths[k], values[k]);
    CHECK(bit == 569);
    return frame;
}

/** Verify the independently computed ICD Newton/finite-difference orbital vector. */
static void Orbit(const TNgBroadcast *broadcast)
{
    const double expected[] = {-6418115.80112963, -26200197.5520795, -32396556.8903401};
    const double speed[] = {615.3923961334, -1092.670279145, 800.4763627052};
    double x[3], v[3], clock, drift;
    CHECK(NgPvtSatellite(broadcast, NG_CONSTELLATION_BEIDOU, 25.5, x, v, &clock, &drift));
    for (unsigned k = 0; k < 3; ++k) {
        CHECK(fabs(x[k] - expected[k]) < 0.002);
        CHECK(fabs(v[k] - speed[k]) < 1e-4);
    }
    CHECK(fabs(clock - 0.0000698503112122878) < 1e-12);
    CHECK(fabs(drift - (-7.78809120330e-11)) < 1e-13);
    CHECK(!NgPvtSatellite(broadcast, NG_CONSTELLATION_BEIDOU, 14500, x, v, &clock, &drift));
}

/** Validate full cache life cycle, independent time/range signs and the week transition. */
int main(void)
{
    TNgBcnv1Frame expected = Fixture(), frame;
    int8_t symbols[NG_BCNV1_FRAME_SYMBOLS];
    for (size_t n = 0; n < sizeof(symbols); ++n)
        symbols[n] = (encoded[n / 8] >> (7u - n % 8)) & 1 ? -100 : 100;
    CHECK(NgBcnv1Decode(symbols, 63, &frame) == NG_OK);
    CHECK(frame.prn == expected.prn && frame.week == expected.week &&
          frame.hour == expected.hour && frame.soh == expected.soh &&
          frame.iodc == expected.iodc && frame.iode == expected.iode &&
          frame.pageId == expected.pageId);
    CHECK(memcmp(frame.sf2 + 5, expected.sf2 + 5, 67) == 0);
    TNgBcnv1Cache cache = {0};
    TNgTrackingSnapshot tracking = {0};
    TNgNavigationSnapshot nav;
    TNgPvtObservation obs;
    unsigned gpsWeek;
    double tow;
    const uint64_t first = 10000, end = first + 36500000;
    CHECK(NgBcnv1Accept(&cache, &frame, 7, first) == NG_OK);
    CHECK(cache.broadcast.ephemeridesReceived && cache.broadcast.eph.iode1 == 18);
    CHECK(cache.broadcast.eph.axisRate == 1 && cache.broadcast.eph.meanMotionRate > 0);
    CHECK(cache.broadcast.clk.beidouB2aTgd == ldexp(23.0, -34));
    CHECK(cache.broadcast.clk.beidouDataIsc == ldexp(-19.0, -34));
    CHECK(cache.broadcast.clk.tGroupDelay == ldexp(41.0, -34));
    Orbit(&cache.broadcast);
    CHECK(NgBcnv1Accept(&cache, &frame, 7, first + 1) == NG_END);
    CHECK(NgBcnv1Accept(&cache, &frame, 6, first + 1) == NG_END);
    CHECK(NgBcnv1Navigation(&cache, 6, end, 0, NG_BEIDOU_B1C_PILOT, &nav) == NG_END);
    CHECK(NgBcnv1Navigation(&cache, 7, end, 37 * 1023000.0, NG_BEIDOU_B1C_PILOT,
                            &nav) == NG_END);
    CHECK(NgBcnv1Navigation(&cache, 7, end, 18.25 * 1023000.0, NG_BEIDOU_B1C_DATA,
                            &tracking.navigation) == NG_OK);
    CHECK(tracking.navigation.broadcast.wn == 502);
    CHECK(tracking.navigation.transmitTimeSeconds == 0.25);
    CHECK(tracking.navigation.broadcast.clk.tGroupDelay == ldexp(22.0, -34));
    CHECK(NgBdtToGps(0, 0, &gpsWeek, &tow) == NG_OK && gpsWeek == 1356 && tow == 14);
    CHECK(NgBdtToGps(501, 604799.5, &gpsWeek, &tow) == NG_OK &&
          gpsWeek == 1858 && tow == 13.5);

    tracking.active = tracking.pll = 1;
    tracking.constellation = NG_CONSTELLATION_BEIDOU;
    tracking.prn = 63;
    tracking.epoch = 7;
    tracking.endSample = end;
    tracking.codeRateHz = 1023000;
    tracking.dopplerHz = 1000;
    CHECK(NgPvtObserve(&tracking, 7, end + 1000, 2000000, 14.33, &obs) == NG_OK);
    CHECK(fabs(obs.nativeTransmitTow - 0.2505) < 1e-12);
    CHECK(fabs(obs.commonTransmitTow - 14.2505) < 1e-12);
    CHECK(fabs(obs.pseudorange - 23833500.411) < 1e-4);
    CHECK(fabs(obs.rangeRate - (-190.293672798365)) < 1e-8);
    CHECK(NgPvtObserve(&tracking, 8, end, 2000000, 14.33, &obs) == NG_END);
    CHECK(NgPvtObserve(&tracking, 7, end + 20001, 2000000, 14.33, &obs) == NG_END);

    frame.week = 502;
    frame.hour = frame.soh = 0;
    frame.iodc = 275;
    CHECK(NgBcnv1Accept(&cache, &frame, 7, end + 1) == NG_OK);
    CHECK(cache.broadcast.ephemeridesReceived && cache.broadcast.clk.iodc == 18);
    frame.soh = 1;
    frame.iode = 19;
    frame.health = 1;
    CHECK(NgBcnv1Accept(&cache, &frame, 7, end + 2) == NG_OK);
    CHECK(cache.broadcast.clk.iodc == 275);
    CHECK(NgBcnv1Navigation(&cache, 7, end + 2, 0, NG_BEIDOU_B1C_PILOT, &nav) == NG_END);
    frame.soh = 2;
    frame.health = 0;
    CHECK(NgBcnv1Accept(&cache, &frame, 7, end + 3) == NG_OK);
    CHECK(NgBcnv1Navigation(&cache, 7, end + 3, 0, NG_BEIDOU_B1C_PILOT, &nav) == NG_OK);
    frame.hour = 6;
    CHECK(NgBcnv1Accept(&cache, &frame, 7, end + 4) == NG_OK);
    CHECK(!cache.broadcast.ephemeridesReceived);
    frame.hour = frame.soh = 0;
    frame.iodc = 276;
    CHECK(NgBcnv1Accept(&cache, &frame, 8, 1) == NG_OK);
    CHECK(!cache.broadcast.ephemeridesReceived);
    puts("B1C orbit/clock, issue cache, health, BDT rollover and range/range-rate passed");
    return 0;
}
