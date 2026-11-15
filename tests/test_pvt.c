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

/** Create a diverse healthy circular-orbit constellation and exact synthetic transmit times. */
static void Fixture(unsigned mixed, int rollover)
{
    TNgTrackingSnapshot channels[64] = {0};
    TNgPvtSnapshot solution;
    double reference = rollover ? 0.025 : 345600.025;
    double lat = 0.7853981633974483, lon = 0.13962634015954636;
    double radius = 6378137.0 / sqrt(1 - 6.6943799901413165e-3 * sin(lat) * sin(lat));
    double receiver[3] = {(radius + 100) * cos(lat) * cos(lon),
                          (radius + 100) * cos(lat) * sin(lon),
                          (radius * (1 - 6.6943799901413165e-3) + 100) * sin(lat)};
    unsigned k, j;
    for (k = 0; k < 10; k++) {
        TNgTrackingSnapshot *t = &channels[k];
        TNgBroadcast *b = &t->navigation.broadcast;
        double tx = reference - 0.08, position[3], velocity[3], clock, drift, rotated[3],
               range = 24000000;
        t->active = t->pll = t->navigation.towValid = b->ephemeridesReceived = 1;
        t->constellation = mixed && k >= 5 ? 2 : 1;
        t->prn = k + 1;
        t->epoch = 7;
        t->endSample = 2400000;
        t->codeRateHz = 1023000;
        b->eph.sqrtA = sqrt(26560000.0);
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
            angle = W * range / C;
            rotated[0] = cos(angle) * position[0] + sin(angle) * position[1];
            rotated[1] = -sin(angle) * position[0] + cos(angle) * position[1];
            rotated[2] = position[2];
            range = hypot(hypot(rotated[0] - receiver[0], rotated[1] - receiver[1]),
                          rotated[2] - receiver[2]);
            sine = ((rotated[0] - receiver[0]) * cos(lat) * cos(lon) +
                    (rotated[1] - receiver[1]) * cos(lat) * sin(lon) +
                    (rotated[2] - receiver[2]) * sin(lat)) /
                   range;
            tx = reference + clock - (range + Delay(sine)) / C;
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
            double rate = 0, angle = W * range / C, vs[3];
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
    CHECK(hypot(hypot(solution.ecef[0] - receiver[0], solution.ecef[1] - receiver[1]),
                solution.ecef[2] - receiver[2]) < 0.2);
    CHECK(fabs(solution.velocity[0] - 12) < 1e-5 && fabs(solution.velocity[1] + 3) < 1e-5 &&
          fabs(solution.velocity[2] - 1) < 1e-5);
    CHECK(fabs(solution.clockDrift - 0.7) < 1e-5);
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
    Fixture(0, 0);
    Fixture(1, 0);
    Fixture(1, 1);
    puts("broadcast analytic derivatives, common epochs, mixed clocks and week rollover passed");
    return 0;
}
