/** @file beidou_observation.c @brief B-CNAV1 broadcast cache and sample-anchored BDT. */
#include "ngene/beidou_observation.h"
#include "ngene/channel.h"
#include <limits.h>
#include <math.h>

#define NG_BDS_PI 3.14159265358979323846
#define NG_BDS_WN_MODULUS 8192u

/** Consume an unsigned field from SF2 in transmission order. */
static uint64_t Read(const uint8_t *data, unsigned *bit, unsigned width)
{
    uint64_t value = 0;
    for (unsigned n = 0; n < width; ++n, ++*bit)
        value = (value << 1) | (uint64_t)((data[*bit / 8] >> (7u - *bit % 8)) & 1);
    return value;
}

/** Decode signed broadcast fields without implementation-defined integer shifts. */
static double Signed(const uint8_t *data, unsigned *bit, unsigned width, int scale)
{
    const uint64_t raw = Read(data, bit, width);
    const uint64_t sign = UINT64_C(1) << (width - 1);
    const int64_t integer = (int64_t)(raw ^ sign) - (int64_t)sign;
    return ldexp((double)integer, scale);
}

/** Signed age modulo BDT week, for broadcast toe/toc. */
static double Age(double time, double reference)
{
    return remainder(time - reference, NG_BDT_WEEK_SECONDS);
}

/** Decode a complete SF2 pair into native SI units, covering MEO and IGSO. */
static int DecodePair(const TNgBcnv1Frame *frame, double tow, TNgOrbit *orbit,
                      TNgBroadcastClock *clock)
{
    const uint8_t *data = frame->sf2;
    unsigned bit = 39;
    orbit->toe = 300.0 * (double)Read(data, &bit, 11);
    const unsigned type = (unsigned)Read(data, &bit, 2);
    const double deltaA = Signed(data, &bit, 26, -9);
    if (type != 2 && type != 3)
        return 0;
    orbit->sqrtA = sqrt((type == 3 ? 27906100.0 : 42162200.0) + deltaA);
    orbit->axisRate = Signed(data, &bit, 25, -21);
    orbit->deltaN = NG_BDS_PI * Signed(data, &bit, 17, -44);
    orbit->meanMotionRate = NG_BDS_PI * Signed(data, &bit, 23, -57);
    orbit->meanAnomaly0 = NG_BDS_PI * Signed(data, &bit, 33, -32);
    orbit->eccentricity = ldexp((double)Read(data, &bit, 33), -34);
    orbit->omega = NG_BDS_PI * Signed(data, &bit, 33, -32);
    orbit->capitalOmega0 = NG_BDS_PI * Signed(data, &bit, 33, -32);
    orbit->i0 = NG_BDS_PI * Signed(data, &bit, 33, -32);
    orbit->capitalOmegaDot = NG_BDS_PI * Signed(data, &bit, 19, -44);
    orbit->iDot = NG_BDS_PI * Signed(data, &bit, 15, -44);
    orbit->capitalCis = Signed(data, &bit, 16, -30);
    orbit->capitalCic = Signed(data, &bit, 16, -30);
    orbit->capitalCrs = Signed(data, &bit, 24, -8);
    orbit->capitalCrc = Signed(data, &bit, 24, -8);
    orbit->capitalCus = Signed(data, &bit, 21, -30);
    orbit->capitalCuc = Signed(data, &bit, 21, -30);
    clock->toc = 300.0 * (double)Read(data, &bit, 11);
    clock->af[0] = Signed(data, &bit, 25, -34);
    clock->af[1] = Signed(data, &bit, 22, -50);
    clock->af[2] = Signed(data, &bit, 11, -66);
    clock->beidouB2aTgd = Signed(data, &bit, 12, -34);
    clock->beidouDataIsc = Signed(data, &bit, 12, -34);
    clock->tGroupDelay = Signed(data, &bit, 12, -34);
    clock->iodc = (int)frame->iodc;
    orbit->iode1 = (int)frame->iode;
    return orbit->toe < NG_BDT_WEEK_SECONDS && clock->toc < NG_BDT_WEEK_SECONDS &&
           fabs(Age(tow, orbit->toe)) <= NG_BCNV1_MAX_BROADCAST_AGE &&
           fabs(Age(tow, clock->toc)) <= NG_BCNV1_MAX_BROADCAST_AGE;
}

int NgBcnv1Accept(TNgBcnv1Cache *cache, const TNgBcnv1Frame *frame, uint64_t epoch,
                  uint64_t firstSample)
{
    if (!cache || !frame || !NgSatellitePrnValid(NG_CONSTELLATION_BEIDOU, frame->prn) ||
        frame->week >= NG_BDS_WN_MODULUS || frame->hour >= 168 || frame->soh >= 200 ||
        frame->iodc >= 1024 || frame->iode >= 256 || (cache->prn && cache->prn != frame->prn))
        return NG_ERROR;
    if (cache->prn && epoch < cache->epoch)
        return NG_END;
    const unsigned tow = frame->hour * 3600u + frame->soh * 18u;
    TNgBcnv1Cache next = {0};
    if (cache->prn && epoch == cache->epoch) {
        const unsigned deltaWeek = (frame->week + NG_BDS_WN_MODULUS - cache->week) %
                                   NG_BDS_WN_MODULUS;
        const double elapsed = (double)deltaWeek * NG_BDT_WEEK_SECONDS + tow - cache->tow;
        if (firstSample <= cache->firstSample || elapsed <= 0 ||
            deltaWeek >= NG_BDS_WN_MODULUS / 2)
            return NG_END;
        if (elapsed <= NG_BCNV1_MAX_BROADCAST_AGE)
            next = *cache;
    }
    TNgOrbit orbit = {0};
    TNgBroadcastClock clock = {0};
    if (frame->iode == (frame->iodc & 255u) && DecodePair(frame, tow, &orbit, &clock)) {
        next.broadcast.eph = orbit;
        next.broadcast.clk = clock;
        next.broadcast.ephemeridesReceived = 1;
    }
    if (fabs(Age(tow, next.broadcast.eph.toe)) > NG_BCNV1_MAX_BROADCAST_AGE ||
        fabs(Age(tow, next.broadcast.clk.toc)) > NG_BCNV1_MAX_BROADCAST_AGE)
        next.broadcast.ephemeridesReceived = 0;
    next.broadcast.satelliteId = (int)frame->prn;
    next.broadcast.eph.health = frame->pageId < 1 || frame->pageId > 4 ||
                                frame->health || frame->dif || frame->sif || frame->aif;
    next.broadcast.subframe2Received = 1;
    next.broadcast.subframe3Received = frame->pageId >= 1 && frame->pageId <= 4;
    next.broadcast.wn = (int)frame->week;
    next.broadcast.tow = (int)tow;
    next.broadcast.wnValid = next.broadcast.towValid = 1;
    next.prn = frame->prn;
    next.epoch = epoch;
    next.firstSample = firstSample;
    next.week = frame->week;
    next.tow = tow;
    *cache = next;
    return NG_OK;
}

int NgBcnv1Navigation(const TNgBcnv1Cache *cache, uint64_t epoch, uint64_t sample,
                      double advanceChips, TNgBeidouB1cComponent component,
                      TNgNavigationSnapshot *navigation)
{
    if (!cache || !navigation ||
        (component != NG_BEIDOU_B1C_DATA && component != NG_BEIDOU_B1C_PILOT))
        return NG_ERROR;
    if (!cache->prn || !cache->broadcast.ephemeridesReceived || cache->broadcast.eph.health ||
        epoch != cache->epoch || sample < cache->firstSample || !isfinite(advanceChips) ||
        advanceChips < 0 || advanceChips > NG_BCNV1_MAX_ANCHOR_AGE * NG_BEIDOU_B1C_RATE_HZ)
        return NG_END;
    double time = cache->tow + advanceChips / NG_BEIDOU_B1C_RATE_HZ;
    unsigned week = cache->week;
    if (time >= NG_BDT_WEEK_SECONDS) {
        time -= NG_BDT_WEEK_SECONDS;
        week = (week + 1) % NG_BDS_WN_MODULUS;
    }
    if (fabs(Age(time, cache->broadcast.eph.toe)) > NG_BCNV1_MAX_BROADCAST_AGE ||
        fabs(Age(time, cache->broadcast.clk.toc)) > NG_BCNV1_MAX_BROADCAST_AGE)
        return NG_END;
    TNgNavigationSnapshot next = {0};
    next.broadcast = cache->broadcast;
    if (component == NG_BEIDOU_B1C_DATA)
        next.broadcast.clk.tGroupDelay += next.broadcast.clk.beidouDataIsc;
    next.broadcast.wn = (int)week;
    next.transmitTimeSeconds = time;
    next.towSeconds = (unsigned)time;
    next.towValid = 1;
    next.boundarySample = cache->firstSample;
    *navigation = next;
    return NG_OK;
}

int NgBdtToGps(unsigned bdtWeek, double bdtTow, unsigned *gpsWeek, double *gpsTow)
{
    if (!gpsWeek || !gpsTow || bdtWeek > UINT_MAX - NG_BDT_GPS_WEEK_ORIGIN - 1 ||
        !isfinite(bdtTow) || bdtTow < 0 || bdtTow >= NG_BDT_WEEK_SECONDS)
        return NG_ERROR;
    double time = bdtTow + NG_BDT_TO_GPST_SECONDS;
    unsigned week = bdtWeek + NG_BDT_GPS_WEEK_ORIGIN;
    if (time >= NG_BDT_WEEK_SECONDS) {
        time -= NG_BDT_WEEK_SECONDS;
        ++week;
    }
    *gpsWeek = week;
    *gpsTow = time;
    return NG_OK;
}
