/** @file test_pvt.c @brief Broadcast analytic derivatives and common-epoch LS regression. */
#include "ngene/pvt.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "line %d: %s\n", __LINE__, #x);                                        \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
#define C 299792458.0
#define W 7.2921151467e-5

/** Independent default-atmosphere path generator in dry/wet zenith terms. */
static double Delay(double sineElevation)
{
    double factor = 1 - 0.00558 * 100.0 / 283.15;
    double dry =
        1e-6 * 77.604 * 287.054 * 1015.75 / 9.784 * pow(factor, 9.80665 / (287.054 * 0.00558));
    double wet = 1e-6 * 382000 * 287.054 / (9.784 * 3.57 - 0.00558 * 287.054) * 11.66 / 283.15 *
                 pow(factor, 3.57 * 9.80665 / (287.054 * 0.00558) - 1);
    return sineElevation > 0 ? (dry + wet) * 1.001 / sqrt(0.002001 + sineElevation * sineElevation)
                             : 0;
}

/** Compute DOP independently by inverting the fixture's ENU geometry Gram matrix. */
static void CheckDop(const TNgPvtSnapshot *s)
{
    double augmented[6][12] = {{0}};
    unsigned columns = 3, clock[3] = {0};
    const unsigned present[] = {s->gps, s->galileo, s->beidou};
    for (unsigned c = 0; c < 3; ++c)
        if (present[c])
            clock[c] = columns++;
    for (unsigned k = 0; k < s->skyCount; ++k) {
        const TNgPvtSatelliteView *sky = &s->sky[k];
        double row[6] = {cos(sky->elevation) * sin(sky->azimuth),
                         cos(sky->elevation) * cos(sky->azimuth), sin(sky->elevation)};
        row[clock[sky->constellation - 1]] = -1;
        for (unsigned i = 0; i < columns; ++i)
            for (unsigned j = 0; j < columns; ++j)
                augmented[i][j] += row[i] * row[j];
    }
    for (unsigned i = 0; i < columns; ++i)
        augmented[i][i + columns] = 1;
    for (unsigned i = 0; i < columns; ++i) {
        const double scale = augmented[i][i];
        CHECK(scale > 1e-10);
        for (unsigned j = 0; j < 2 * columns; ++j)
            augmented[i][j] /= scale;
        for (unsigned k = 0; k < columns; ++k) {
            if (k == i)
                continue;
            const double factor = augmented[k][i];
            for (unsigned j = 0; j < 2 * columns; ++j)
                augmented[k][j] -= factor * augmented[i][j];
        }
    }
    double trace = 0;
    for (unsigned i = 0; i < columns; ++i)
        trace += augmented[i][i + columns];
    CHECK(fabs(s->gdop - sqrt(trace)) < 1e-8);
    CHECK(fabs(s->hdop - sqrt(augmented[0][columns] + augmented[1][columns + 1])) < 1e-8);
    CHECK(fabs(s->vdop - sqrt(augmented[2][columns + 2])) < 1e-8);
}

/** Create a diverse healthy circular-orbit constellation and exact synthetic transmit times. */
static void Fixture(unsigned mask, int rollover)
{
    TNgTrackingSnapshot channels[64] = {0};
    TNgPvtSnapshot solution;
    double reference = rollover ? 0.025 : 345600.025;
    double lat = 0.7853981633974483, lon = 0.13962634015954636;
    double radius = 6378137.0 / sqrt(1 - 6.6943799901413165e-3 * sin(lat) * sin(lat));
    double receiver[3] = {(radius + 100) * cos(lat) * cos(lon),
                          (radius + 100) * cos(lat) * sin(lon),
                          (radius * (1 - 6.6943799901413165e-3) + 100) * sin(lat)};
    unsigned k, j, active[3], count = 0;
    const double biases[3] = {0, 35, -47};
    unsigned nextPrn[3] = {1, 1, 1};
    for (unsigned system = 0; system < 3; ++system)
        if (mask & (1u << system))
            active[count++] = system;
    for (k = 0; k < 10; k++) {
        TNgTrackingSnapshot *t = &channels[k];
        TNgBroadcast *b = &t->navigation.broadcast;
        double tx = reference - 0.08, position[3], velocity[3], clock, drift, rotated[3],
               range = 24000000;
        t->active = t->pll = t->navigation.towValid = b->ephemeridesReceived = 1;
        const unsigned system = active[k * count / 10];
        const double timeOffset = system == 2 ? 14 : 0;
        const double earth = system == 2 ? 7.2921150e-5 : W;
        const double codeBias = biases[system];
        tx -= timeOffset;
        t->constellation = system + 1;
        t->prn = nextPrn[system]++;
        t->epoch = 7;
        t->endSample = 2400000;
        t->codeRateHz = 1023000;
        b->eph.sqrtA = sqrt(system == 2 ? 27906100.0 : 26560000.0);
        b->eph.axisRate = system == 2 ? -0.3 : 0;
        b->eph.meanMotionRate = system == 2 ? 3e-12 : 0;
        b->eph.i0 = 0.95;
        b->eph.eccentricity = 0.01;
        b->eph.toe = rollover ? 604000 : 345000;
        b->clk.toc = b->eph.toe;
        b->eph.meanAnomaly0 = (double)k * 2.1;
        b->eph.capitalOmega0 = (double)k * 1.7;
        b->clk.af[0] = 1e-5 * (k + 1);
        b->clk.af[1] = 1e-11 * (k + 1);
        for (j = 0; j < 20; j++) {
            double angle, sine;
            CHECK(NgPvtSatellite(b, t->constellation, tx, position, velocity, &clock, &drift));
            angle = earth * range / C;
            rotated[0] = cos(angle) * position[0] + sin(angle) * position[1];
            rotated[1] = -sin(angle) * position[0] + cos(angle) * position[1];
            rotated[2] = position[2];
            range = hypot(hypot(rotated[0] - receiver[0], rotated[1] - receiver[1]),
                          rotated[2] - receiver[2]);
            sine = ((rotated[0] - receiver[0]) * cos(lat) * cos(lon) +
                    (rotated[1] - receiver[1]) * cos(lat) * sin(lon) +
                    (rotated[2] - receiver[2]) * sin(lat)) /
                   range;
            tx = reference - timeOffset + clock - (range + Delay(sine) + codeBias) / C;
        }
        t->navigation.transmitTimeSeconds = fmod(tx + 604800, 604800);
        /* Independently check analytic orbital derivatives against central differences. */
        {
            double xp[3], xm[3], scratch[3], cp, cm, d;
            unsigned axis;
            CHECK(NgPvtSatellite(b, t->constellation, tx + 0.05, xp, scratch, &cp, &d));
            CHECK(NgPvtSatellite(b, t->constellation, tx - 0.05, xm, scratch, &cm, &d));
            for (axis = 0; axis < 3; axis++)
                CHECK(fabs((xp[axis] - xm[axis]) / 0.1 - velocity[axis]) < 0.01);
            CHECK(fabs((cp - cm) / 0.1 - drift) < 1e-12);
        }
        {
            double rate = 0, angle = earth * range / C, vs[3];
            vs[0] = cos(angle) * velocity[0] + sin(angle) * velocity[1];
            vs[1] = -sin(angle) * velocity[0] + cos(angle) * velocity[1];
            vs[2] = velocity[2];
            for (j = 0; j < 3; j++)
                rate += (rotated[j] - receiver[j]) / range *
                        (vs[j] - (j == 0   ? 12
                                  : j == 1 ? -3
                                           : 1));
            t->dopplerHz = -(rate + 0.7 - C * drift) * 1575420000.0 / C;
        }
    }
    NgPvtCompute(channels, 7, 2400000, 2400000, &solution);
    CHECK(solution.available && solution.velocityAvailable && solution.satellites == 10);
    CHECK(solution.dopAvailable && solution.skyCount == 10);
    CHECK(solution.residualRms < 0.02);
    CheckDop(&solution);
    CHECK(solution.hdop > 0 && solution.vdop > 0 && solution.gdop > 0);
    CHECK(solution.gdop * solution.gdop >=
          solution.hdop * solution.hdop + solution.vdop * solution.vdop);
    if (mask & 1u) {
        TNgPvtSnapshot corrected;
        for (k = 0; k < 10; ++k)
            if (channels[k].constellation == 1) {
                channels[k].navigation.broadcast.ionoReceived = 1;
                channels[k].navigation.broadcast.iono.betaGps[0] = 72000;
            }
        NgPvtCompute(channels, 7, 2400000, 2400000, &corrected);
        CHECK(corrected.available && fabs(corrected.clockGps - solution.clockGps) > 0.1);
        for (k = 0; k < 10; ++k)
            channels[k].navigation.broadcast.ionoReceived = 0;
    }
    const unsigned admitted[] = {solution.gps, solution.galileo, solution.beidou};
    const double clocks[] = {solution.clockGps, solution.clockGalileo, solution.clockBeidou};
    for (unsigned system = 0; system < 3; ++system) {
        CHECK((admitted[system] > 0) == ((mask & (1u << system)) != 0));
        if (admitted[system])
            CHECK(fabs((clocks[system] - clocks[active[0]]) -
                        (biases[system] - biases[active[0]])) < 0.2);
    }
    for (k = 0; k < solution.skyCount; k++) {
        const TNgPvtSatelliteView *sky = &solution.sky[k];
        const double earth = sky->constellation == 3 ? 7.2921150e-5 : W;
        double position[3], velocity[3], clock, drift, range = 24000000, rotated[3];
        CHECK(sky->prn == channels[k].prn && sky->constellation == channels[k].constellation);
        CHECK(NgPvtSatellite(&channels[k].navigation.broadcast, sky->constellation,
                             channels[k].navigation.transmitTimeSeconds, position, velocity, &clock,
                             &drift));
        for (j = 0; j < 3; j++) {
            double angle = earth * range / C;
            rotated[0] = cos(angle) * position[0] + sin(angle) * position[1];
            rotated[1] = -sin(angle) * position[0] + cos(angle) * position[1];
            rotated[2] = position[2];
            range = hypot(hypot(rotated[0] - receiver[0], rotated[1] - receiver[1]),
                          rotated[2] - receiver[2]);
        }
        double dx = rotated[0] - receiver[0], dy = rotated[1] - receiver[1],
               dz = rotated[2] - receiver[2];
        double east = -sin(lon) * dx + cos(lon) * dy;
        double north = -sin(lat) * cos(lon) * dx - sin(lat) * sin(lon) * dy + cos(lat) * dz;
        double az = atan2(east, north);
        if (az < 0)
            az += 6.283185307179586;
        double up = cos(lat) * cos(lon) * dx + cos(lat) * sin(lon) * dy + sin(lat) * dz;
        CHECK(fabs(sky->azimuth - az) < 1e-7 && fabs(sky->elevation - asin(up / range)) < 1e-7);
    }
    CHECK(hypot(hypot(solution.ecef[0] - receiver[0], solution.ecef[1] - receiver[1]),
                solution.ecef[2] - receiver[2]) < 0.2);
    CHECK(fabs(solution.velocity[0] - 12) < 1e-5 && fabs(solution.velocity[1] + 3) < 1e-5 &&
          fabs(solution.velocity[2] - 1) < 1e-5);
    CHECK(fabs(solution.clockDrift - 0.7) < 1e-5);
    if (count == 3) {
        TNgTrackingSnapshot same = channels[0];
        for (k = 0; k < 10; ++k) {
            const unsigned system = channels[k].constellation;
            channels[k] = same;
            channels[k].constellation = system;
            channels[k].prn = k + 1;
        }
        NgPvtCompute(channels, 7, 2400000, 2400000, &solution);
        CHECK(!solution.available && solution.satellites == 10);
    }
    for (k = 0; k < 10; k++)
        channels[k].epoch = 8;
    NgPvtCompute(channels, 7, 2400000, 2400000, &solution);
    CHECK(!solution.available);
    channels[0].epoch = 7;
    NgPvtCompute(channels, 7, 2400000, 2400000, &solution);
    CHECK(!solution.available);
}

/** Check GPS-only, mixed biases, rollover and invalid-generation rejection. */
int main(void)
{
    for (unsigned mask = 1; mask < 8; ++mask) {
        Fixture(mask, 0);
        Fixture(mask, 1);
    }
    puts("broadcast derivatives, all seven constellation sets and week rollover passed");
    return 0;
}
