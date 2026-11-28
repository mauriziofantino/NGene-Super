/** @file pvt.c @brief Classic broadcast orbit, Earth rotation and iterative code/Doppler LS.
 * Adapted from pvt/pvt.c, leastSquares.c and pvtUtility.c, Mario Nicola, 2014.
 * Small C Householder QR replaces the Eigen boundary, retaining double arithmetic.
 *
 *
 * @details At one receiver sample, align validated transmit timestamps to a
 * common week-based scale and propagate each satellite in its native time
 * scale. Rotate satellite coordinates for the signal flight time. For
 * pseudorange \f$\rho_k\f$, satellite clock correction \f$\delta t_k\f$,
 * receiver code bias \f$b_{g(k)}\f$, troposphere \f$T_k\f$, ionosphere
 * \f$I_k\f$ and optional additive SBAS code correction \f$S_k\f$, the
 * linearized code residual is formed from
 * \f[
 * r_k=\rho_k+S_k+c\,\delta t_k
 *      -\lVert \mathbf x-\mathbf x_k\rVert-b_{g(k)}-T_k-I_k .
 * \f]
 * Use an independent code bias for each admitted constellation. Iterative
 * column-pivoted Householder QR estimates receiver position and biases.
 * A separate Doppler system estimates velocity and common clock drift.
 * Availability requires convergence and adequate rank. The resulting
 * residual and dilution metrics describe the solution geometry and fit.
 */
#include "ngene/pvt.h"
#include "navigation/ionosphere.h"
#include "ngene/beidou_observation.h"
#include <math.h>
#include <string.h>
#define NG_C 299792458.0
#define NG_OMEGA 7.2921151467e-5
#define NG_OMEGA_BDCS 7.2921150e-5
#define NG_PVT_COLUMNS (3u + NG_CONSTELLATION_COUNT)
#define NG_PI 3.14159265358979323846

/** Signed week difference, including a reference across Sunday rollover. */
static double Week(double time)
{
    return time - 604800.0 * floor((time + 302400.0) / 604800.0);
}

/** Solve Kepler's equation and return the eccentric anomaly. */
static double EccentricAnomaly(double anomaly, double eccentricity)
{
    double estimate = anomaly;
    for (unsigned iteration = 0; iteration < 10; ++iteration) {
        const double previous = estimate;
        estimate = anomaly + eccentricity * sin(estimate);
        if (fabs(estimate - previous) < 1e-12) {
            break;
        }
    }
    return estimate;
}

/** Earth rotation in the native terrestrial frame of each broadcast. */
static double EarthRotation(unsigned system)
{
    return system == NG_CONSTELLATION_BEIDOU ? NG_OMEGA_BDCS : NG_OMEGA;
}

/** Broadcast Kepler/harmonic equations, analytic velocity, relativistic clock correction. */
int NgPvtSatellite(const TNgBroadcast *broadcast, unsigned constellation, double transmitTime,
                   double position[3], double velocity[3], double *clock, double *drift)
{
    const TNgOrbit *o = &broadcast->eph;
    const TNgBroadcastClock *c = &broadcast->clk;
    double dt, a, tk, n, m, e, dtr, nu, phi, u, r, inc, omega;
    double ed, nud, ud, rd, id, xp, yp, xpd, ypd, od;
    const int beidou = constellation == NG_CONSTELLATION_BEIDOU;
    const double earth = EarthRotation(constellation);
    const double relativity =
        constellation == NG_CONSTELLATION_GPS ? -4.442807633e-10 : -4.442807309e-10;
    if (!broadcast->ephemeridesReceived ||
        !NgConstellationPrnLimit((TNgConstellation)constellation) || !isfinite(transmitTime) ||
        !isfinite(o->sqrtA) || o->sqrtA < 4000 || o->sqrtA > 7000 || !isfinite(o->eccentricity) ||
        o->eccentricity < 0 || o->eccentricity >= 1 || fabs(Week(transmitTime - o->toe)) > 14400) {
        return 0;
    }
    dt = Week(transmitTime - c->toc);
    *clock = (c->af[2] * dt + c->af[1]) * dt + c->af[0] - c->tGroupDelay;
    a = o->sqrtA * o->sqrtA;
    tk = Week(transmitTime - *clock - o->toe);
    n = sqrt((constellation == NG_CONSTELLATION_GPS ? 3.986005e14 : 3.986004418e14) / (a * a * a)) +
        o->deltaN;
    const double a0 = a, n0 = n;
    const double axisRate = beidou ? o->axisRate : 0;
    const double motionRate = beidou ? o->meanMotionRate : 0;
    if (beidou && (fabs(Week(transmitTime - c->toc)) > NG_BCNV1_MAX_BROADCAST_AGE ||
                   !isfinite(axisRate) || !isfinite(motionRate))) {
        return 0;
    }
    unsigned iteration = 0;
    do {
        if (beidou) {
            dt = Week(transmitTime - *clock - c->toc);
            tk = Week(transmitTime - *clock - o->toe);
            a = a0 + axisRate * tk;
            if (!(a > 0)) {
                return 0;
            }
        }
        m = fmod(o->meanAnomaly0 + (n0 + 0.5 * motionRate * tk) * tk + 2 * NG_PI, 2 * NG_PI);
        e = EccentricAnomaly(m, o->eccentricity);
        if (fabs(e - m - o->eccentricity * sin(e)) > 1e-10) {
            return 0;
        }
        dtr = relativity * o->eccentricity * (beidou ? sqrt(a) : o->sqrtA) * sin(e);
        if (beidou) {
            *clock = (c->af[2] * dt + c->af[1]) * dt + c->af[0] - c->tGroupDelay + dtr;
        }
    } while (++iteration < (beidou ? 3u : 1u));
    n = n0 + motionRate * tk;
    nu = atan2(sqrt(1 - o->eccentricity * o->eccentricity) * sin(e), cos(e) - o->eccentricity);
    phi = nu + o->omega;
    u = phi + o->capitalCuc * cos(2 * phi) + o->capitalCus * sin(2 * phi);
    r = a * (1 - o->eccentricity * cos(e)) + o->capitalCrc * cos(2 * phi) +
        o->capitalCrs * sin(2 * phi);
    inc = o->i0 + o->iDot * tk + o->capitalCic * cos(2 * phi) + o->capitalCis * sin(2 * phi);
    omega = o->capitalOmega0 + (o->capitalOmegaDot - earth) * tk - earth * o->toe;
    xp = r * cos(u);
    yp = r * sin(u);
    position[0] = xp * cos(omega) - yp * cos(inc) * sin(omega);
    position[1] = xp * sin(omega) + yp * cos(inc) * cos(omega);
    position[2] = yp * sin(inc);
    ed = n / (1 - o->eccentricity * cos(e));
    nud = sqrt(1 - o->eccentricity * o->eccentricity) * ed / (1 - o->eccentricity * cos(e));
    ud = nud * (1 + 2 * (o->capitalCus * cos(2 * phi) - o->capitalCuc * sin(2 * phi)));
    rd = axisRate * (1 - o->eccentricity * cos(e)) + a * o->eccentricity * sin(e) * ed +
         2 * (o->capitalCrs * cos(2 * phi) - o->capitalCrc * sin(2 * phi)) * nud;
    id = o->iDot + 2 * (o->capitalCis * cos(2 * phi) - o->capitalCic * sin(2 * phi)) * nud;
    xpd = rd * cos(u) - yp * ud;
    ypd = rd * sin(u) + xp * ud;
    od = o->capitalOmegaDot - earth;
    velocity[0] = (xpd - yp * cos(inc) * od) * cos(omega) -
                  (xp * od + ypd * cos(inc) - yp * sin(inc) * id) * sin(omega);
    velocity[1] = (xpd - yp * cos(inc) * od) * sin(omega) +
                  (xp * od + ypd * cos(inc) - yp * sin(inc) * id) * cos(omega);
    velocity[2] = ypd * sin(inc) + yp * cos(inc) * id;
    if (!beidou) {
        *clock += dtr;
    }
    const double sqrtAxis = beidou ? sqrt(a) : o->sqrtA;
    *drift = c->af[1] + 2 * c->af[2] * dt +
             relativity * o->eccentricity *
                 (sqrtAxis * cos(e) * ed + axisRate / (2 * sqrtAxis) * sin(e));
    return isfinite(position[0]) && isfinite(position[1]) && isfinite(position[2]) &&
           isfinite(*clock) && isfinite(velocity[0]) && isfinite(velocity[1]) &&
           isfinite(velocity[2]) && isfinite(*drift);
}

/** Column-pivoted Householder QR; no normal-equation squaring of the condition number. */
static int Solve(double matrix[64][NG_PVT_COLUMNS], double rightHandSide[64], unsigned rows,
                 unsigned columns, double solution[NG_PVT_COLUMNS],
                 double covariance[NG_PVT_COLUMNS][NG_PVT_COLUMNS])
{
    unsigned permutation[NG_PVT_COLUMNS];
    unsigned factorIndex;
    unsigned rowIndex;
    unsigned columnIndex;
    double permutedSolution[NG_PVT_COLUMNS] = {0};
    for (rowIndex = 0; rowIndex < columns; ++rowIndex) {
        permutation[rowIndex] = rowIndex;
    }
    if (rows < columns) {
        return 0;
    }
    for (factorIndex = 0; factorIndex < columns; ++factorIndex) {
        unsigned pivot = factorIndex;
        double maximum = 0, norm, alpha, beta;
        for (columnIndex = factorIndex; columnIndex < columns; ++columnIndex) {
            double value = 0;
            for (rowIndex = factorIndex; rowIndex < rows; ++rowIndex) {
                value = hypot(value, matrix[rowIndex][columnIndex]);
            }
            if (value > maximum) {
                maximum = value;
                pivot = columnIndex;
            }
        }
        if (!(maximum > 1e-10) || !isfinite(maximum)) {
            return 0;
        }
        for (rowIndex = 0; rowIndex < rows; ++rowIndex) {
            double temp = matrix[rowIndex][factorIndex];
            matrix[rowIndex][factorIndex] = matrix[rowIndex][pivot];
            matrix[rowIndex][pivot] = temp;
        }
        {
            unsigned temp = permutation[factorIndex];
            permutation[factorIndex] = permutation[pivot];
            permutation[pivot] = temp;
        }
        norm = maximum;
        alpha = -copysign(norm, matrix[factorIndex][factorIndex]);
        beta = 1.0 / (norm * (norm + fabs(matrix[factorIndex][factorIndex])));
        matrix[factorIndex][factorIndex] -= alpha;
        for (columnIndex = factorIndex + 1; columnIndex <= columns; ++columnIndex) {
            double product = 0;
            for (rowIndex = factorIndex; rowIndex < rows; ++rowIndex) {
                product += matrix[rowIndex][factorIndex] * (columnIndex == columns
                                                                ? rightHandSide[rowIndex]
                                                                : matrix[rowIndex][columnIndex]);
            }
            product *= beta;
            for (rowIndex = factorIndex; rowIndex < rows; ++rowIndex) {
                if (columnIndex == columns) {
                    rightHandSide[rowIndex] -= product * matrix[rowIndex][factorIndex];
                } else {
                    matrix[rowIndex][columnIndex] -= product * matrix[rowIndex][factorIndex];
                }
            }
        }
        matrix[factorIndex][factorIndex] = alpha;
        for (rowIndex = factorIndex + 1; rowIndex < rows; ++rowIndex) {
            matrix[rowIndex][factorIndex] = 0;
        }
    }
    for (rowIndex = columns; rowIndex-- > 0;) {
        double value = rightHandSide[rowIndex];
        for (columnIndex = rowIndex + 1; columnIndex < columns; ++columnIndex) {
            value -= matrix[rowIndex][columnIndex] * permutedSolution[columnIndex];
        }
        permutedSolution[rowIndex] = value / matrix[rowIndex][rowIndex];
        if (!isfinite(permutedSolution[rowIndex])) {
            return 0;
        }
    }
    for (rowIndex = 0; rowIndex < columns; ++rowIndex) {
        solution[permutation[rowIndex]] = permutedSolution[rowIndex];
    }
    if (covariance) {
        double inverse[NG_PVT_COLUMNS][NG_PVT_COLUMNS] = {{0}};
        memset(covariance, 0, sizeof(inverse));
        for (columnIndex = 0; columnIndex < columns; ++columnIndex) {
            for (rowIndex = columns; rowIndex-- > 0;) {
                double value = rowIndex == columnIndex ? 1.0 : 0.0;
                for (factorIndex = rowIndex + 1; factorIndex < columns; ++factorIndex) {
                    value -= matrix[rowIndex][factorIndex] * inverse[factorIndex][columnIndex];
                }
                inverse[rowIndex][columnIndex] = value / matrix[rowIndex][rowIndex];
            }
        }
        for (rowIndex = 0; rowIndex < columns; ++rowIndex) {
            for (columnIndex = 0; columnIndex < columns; ++columnIndex) {
                for (factorIndex = 0; factorIndex < columns; ++factorIndex) {
                    covariance[permutation[rowIndex]][permutation[columnIndex]] +=
                        inverse[rowIndex][factorIndex] * inverse[columnIndex][factorIndex];
                }
            }
        }
    }
    return 1;
}

/** WGS84 iterative latitude and ellipsoid height; longitude uses atan2. */
static void Geodetic(const double position[3], double *latitude, double *longitude, double *height)
{
    const double e2 = 6.6943799901413165e-3;
    double horizontalRadius = hypot(position[0], position[1]),
           lat = atan2(position[2], horizontalRadius * (1 - e2)), radius;
    unsigned iteration;
    for (iteration = 0; iteration < 12; ++iteration) {
        radius = 6378137.0 / sqrt(1 - e2 * sin(lat) * sin(lat));
        lat = atan2(position[2] + e2 * radius * sin(lat), horizontalRadius);
    }
    radius = 6378137.0 / sqrt(1 - e2 * sin(lat) * sin(lat));
    *latitude = lat;
    *longitude = atan2(position[1], position[0]);
    *height = horizontalRadius * cos(lat) + position[2] * sin(lat) -
              radius * (1 - e2 * sin(lat) * sin(lat));
}

/** Classic Hopfield default atmosphere, positive code delay in metres. */
static double Troposphere(double elevation, double height)
{
    double base = 1 - 0.00558 * height / 283.15;
    double dry = 1e-6 * 77.604 * 287.054 * 1015.75 / 9.784;
    double wet = 1e-6 * 382000 * 287.054 / (9.784 * 3.57 - 0.00558 * 287.054) * 11.66 / 283.15;
    if (base <= 0) {
        return 0;
    }
    dry *= pow(base, 9.80665 / (287.054 * 0.00558));
    wet *= pow(base, 3.57 * 9.80665 / (287.054 * 0.00558) - 1);
    return (dry + wet) * 1.001 / sqrt(0.002001 + sin(elevation) * sin(elevation));
}

/** Convert calendar days since the GST origin to the month used by NeQuick. */
static int GalileoMonth(double daysSinceGst)
{
    static const unsigned monthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int year = 1999;
    /* GST starts on 22 August 1999. */
    unsigned day = (unsigned)floor(daysSinceGst + 233.0);
    for (;;) {
        const unsigned leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
        if (day < 365 + leap) {
            for (unsigned month = 0; month < 12; ++month) {
                unsigned days = monthDays[month] + (month == 1 ? leap : 0);
                if (day < days) {
                    return (int)month + 1;
                }
                day -= days;
            }
        }
        day -= 365 + leap;
        ++year;
    }
}

/** GPS LNAV Klobuchar L1 group delay, metres; coefficients must be parity validated. */
static double GpsIonosphere(const TNgBroadcast *b, double latitude, double longitude,
                            double azimuth, double elevation, double tow)
{
    double el = elevation / NG_PI, phi = 0.0137 / (el + 0.11) - 0.022;
    double subLat = fmax(-0.416, fmin(0.416, latitude / NG_PI + phi * cos(azimuth)));
    double subLon = longitude / NG_PI + phi * sin(azimuth) / cos(NG_PI * subLat);
    double magnetic = subLat + 0.064 * cos(NG_PI * (subLon - 1.617));
    double local = fmod(tow + 43200.0 * subLon + 86400.0, 86400.0);
    double period = b->iono.betaGps[0] +
                    magnetic * (b->iono.betaGps[1] +
                                magnetic * (b->iono.betaGps[2] + magnetic * b->iono.betaGps[3]));
    double amplitude =
        b->iono.alphaGps[0] +
        magnetic * (b->iono.alphaGps[1] +
                    magnetic * (b->iono.alphaGps[2] + magnetic * b->iono.alphaGps[3]));
    double x, delay;
    period = fmax(72000.0, period);
    amplitude = fmax(0.0, amplitude);
    x = 2 * NG_PI * (local - 50400.0) / period;
    delay = fabs(x) >= 1.57 ? 5e-9 : 5e-9 + amplitude * (1 - x * x / 2 + x * x * x * x / 24);
    return NG_C * (1 + 16 * pow(0.53 - el, 3)) * delay;
}

/** Align native code time to one receiver sample and derive a nominal GPST label. */
static int Align(const TNgTrackingSnapshot *tracking, uint64_t epoch, uint64_t sample,
                 double sampleRate, double *native, double *common, double *rangeRate)
{
    const TNgNavigationSnapshot *nav = &tracking->navigation;
    const TNgBroadcast *b = &nav->broadcast;
    const unsigned system = tracking->constellation;
    if (!tracking->active || !tracking->pll || tracking->epoch != epoch || !nav->towValid ||
        !b->ephemeridesReceived || b->eph.health || b->eph.galileoDataValidity ||
        !NgSatellitePrnValid((TNgConstellation)system, tracking->prn) ||
        (system != NG_CONSTELLATION_GPS && b->satelliteId &&
         (unsigned)b->satelliteId != tracking->prn) ||
        sample < tracking->endSample ||
        (double)(sample - tracking->endSample) >
            (system == NG_CONSTELLATION_BEIDOU ? ceil(0.01 * sampleRate) : 0.01 * sampleRate) ||
        !isfinite(nav->transmitTimeSeconds) || !(tracking->codeRateHz > 0) ||
        !isfinite(tracking->codeRateHz) || !isfinite(tracking->dopplerHz)) {
        return 0;
    }
    *native = nav->transmitTimeSeconds + (double)(sample - tracking->endSample) / sampleRate *
                                             tracking->codeRateHz / 1023000.0;
    *common = *native + (system == NG_CONSTELLATION_BEIDOU ? NG_BDT_TO_GPST_SECONDS : 0);
    *rangeRate = -tracking->dopplerHz * NG_C / 1575420000.0;
    return isfinite(*native) && isfinite(*rangeRate);
}

int NgPvtObserve(const TNgTrackingSnapshot *channel, uint64_t epoch, uint64_t sample,
                 double sampleRate, double receiverTow, TNgPvtObservation *observation)
{
    TNgPvtObservation next = {0};
    if (!channel || !observation || !(sampleRate > 0) || !isfinite(sampleRate) ||
        !isfinite(receiverTow)) {
        return NG_ERROR;
    }
    if (!Align(channel, epoch, sample, sampleRate, &next.nativeTransmitTow, &next.commonTransmitTow,
               &next.rangeRate)) {
        return NG_END;
    }
    double position[3], velocity[3], clock, drift;
    if (!NgPvtSatellite(&channel->navigation.broadcast, channel->constellation,
                        next.nativeTransmitTow, position, velocity, &clock, &drift)) {
        return NG_END;
    }
    next.pseudorange = NG_C * Week(receiverTow - next.commonTransmitTow);
    next.commonTransmitTow -=
        NG_BDT_WEEK_SECONDS * floor(next.commonTransmitTow / NG_BDT_WEEK_SECONDS);
    next.nativeTransmitTow -=
        NG_BDT_WEEK_SECONDS * floor(next.nativeTransmitTow / NG_BDT_WEEK_SECONDS);
    next.dopplerHz = channel->dopplerHz;
    next.prn = channel->prn;
    next.constellation = channel->constellation;
    next.epoch = epoch;
    next.sample = sample;
    *observation = next;
    return NG_OK;
}

/** Align observations to one sample, then solve one code-clock bias per active system. */
void NgPvtComputeAugmented(const TNgTrackingSnapshot channels[64], uint64_t epoch, uint64_t sample,
                           double sampleRate, int broadcastIonosphere,
                           const TNgSbasCorrections *sbas, TNgPvtSnapshot *solution)
{
    double satellite[64][3], velocity[64][3], clocks[64], drifts[64], tx[64], rates[64], code[64];
    double nativeTx[64], rangeCorrection[64] = {0};
    const TNgBroadcast *broadcasts[64];
    unsigned ionosphereCorrected[3], egnosIonoCount = 0;
    unsigned systems[64], prns[64], n = 0, k, j, iteration, cols = 3;
    unsigned counts[NG_CONSTELLATION_COUNT] = {0}, clockColumn[NG_CONSTELLATION_COUNT] = {0};
    double nominal = 0, pos[NG_PVT_COLUMNS] = {0}, matrix[64][NG_PVT_COLUMNS], rhs[64];
    double correction[NG_PVT_COLUMNS];
    double lat = 0, lon = 0, height = 0, residual = 0;
    memset(solution, 0, sizeof(*solution));
    solution->epoch = epoch;
    solution->sample = sample;
    if (!(sampleRate > 0) || !isfinite(sampleRate)) {
        return;
    }
    for (k = 0; k < 64; ++k) {
        const TNgTrackingSnapshot *t = &channels[k];
        const TNgBroadcast *b = &t->navigation.broadcast;
        double native, time;
        if (!Align(t, epoch, sample, sampleRate, &native, &time, &rates[n])) {
            continue;
        }
        if (!NgPvtSatellite(b, t->constellation, native, satellite[n], velocity[n], &clocks[n],
                            &drifts[n])) {
            continue;
        }
        if (n) {
            time = tx[0] + Week(time - tx[0]);
        }
        TNgSbasCorrection sbasCorrection;
        if (sbas && t->constellation == NG_CONSTELLATION_GPS &&
            NgSbasRead(sbas, t->prn, (unsigned)b->eph.iode2, (double)sample / sampleRate, native,
                       &sbasCorrection)) {
            for (unsigned axis = 0; axis < 3; ++axis) {
                satellite[n][axis] += sbasCorrection.position[axis];
                velocity[n][axis] += sbasCorrection.velocity[axis];
            }
            clocks[n] += sbasCorrection.clock;
            drifts[n] += sbasCorrection.drift;
            rangeCorrection[n] = sbasCorrection.range;
            ++solution->egnosCorrected;
        }
        tx[n] = time;
        nativeTx[n] = native;
        systems[n] = t->constellation;
        broadcasts[n] = b;
        prns[n] = t->prn;
        if (!n || time > nominal) {
            nominal = time;
        }
        counts[t->constellation - 1]++;
        n++;
    }
    solution->satellites = n;
    solution->gps = counts[NG_CONSTELLATION_GPS - 1];
    solution->galileo = counts[NG_CONSTELLATION_GALILEO - 1];
    solution->beidou = counts[NG_CONSTELLATION_BEIDOU - 1];
    for (k = 0; k < NG_CONSTELLATION_COUNT; ++k) {
        if (counts[k]) {
            clockColumn[k] = cols++;
        }
    }
    if (n < cols) {
        return;
    }
    nominal += 0.07;
    solution->towSeconds = fmod(nominal + 604800.0, 604800.0);
    for (k = 0; k < n; ++k) {
        code[k] = NG_C * (nominal - tx[k]) + rangeCorrection[k];
    }
    for (iteration = 0; iteration < 20; ++iteration) {
        residual = 0;
        egnosIonoCount = 0;
        memset(ionosphereCorrected, 0, sizeof(ionosphereCorrected));
        if (iteration) {
            Geodetic(pos, &lat, &lon, &height);
        }
        memset(matrix, 0, sizeof(matrix));
        for (k = 0; k < n; ++k) {
            double rotated[3], range, angle, elevation = 0, tropo = 0, iono = 0;
            unsigned clockIndex = clockColumn[systems[k] - 1];
            range = hypot(hypot(satellite[k][0] - pos[0], satellite[k][1] - pos[1]),
                          satellite[k][2] - pos[2]);
            for (j = 0; j < 2; ++j) {
                angle = EarthRotation(systems[k]) * range / NG_C;
                rotated[0] = cos(angle) * satellite[k][0] + sin(angle) * satellite[k][1];
                rotated[1] = -sin(angle) * satellite[k][0] + cos(angle) * satellite[k][1];
                rotated[2] = satellite[k][2];
                range = hypot(hypot(rotated[0] - pos[0], rotated[1] - pos[1]), rotated[2] - pos[2]);
            }
            if (!(range > 0) || !isfinite(range)) {
                return;
            }
            for (j = 0; j < 3; ++j) {
                matrix[k][j] = (pos[j] - rotated[j]) / range;
            }
            matrix[k][clockIndex] = 1;
            if (iteration && fabs(height) < 1e6) {
                double up = -matrix[k][0] * cos(lat) * cos(lon) -
                            matrix[k][1] * cos(lat) * sin(lon) - matrix[k][2] * sin(lat);
                elevation = asin(fmax(-1, fmin(1, up)));
                if (elevation > 0) {
                    tropo = Troposphere(elevation, height);
                }
                if (elevation > 0 &&
                    (sbas || (broadcastIonosphere && broadcasts[k]->ionoReceived))) {
                    double east = matrix[k][0] * sin(lon) - matrix[k][1] * cos(lon);
                    double north = matrix[k][0] * sin(lat) * cos(lon) +
                                   matrix[k][1] * sin(lat) * sin(lon) - matrix[k][2] * cos(lat);
                    const double azimuth = atan2(east, north);
                    const TNgBroadcast *broadcast = broadcasts[k];
                    const int sbasIono = systems[k] == NG_CONSTELLATION_GPS &&
                                         NgSbasIonosphere(sbas, (double)sample / sampleRate, lat,
                                                          lon, azimuth, elevation, &iono);
                    if (sbasIono) {
                        ++egnosIonoCount;
                    } else if (broadcastIonosphere && broadcast->ionoReceived &&
                               systems[k] == NG_CONSTELLATION_GPS) {
                        iono = GpsIonosphere(broadcast, lat, lon, azimuth, elevation, tx[k]);
                    } else if (broadcastIonosphere && broadcast->ionoReceived &&
                               systems[k] == NG_CONSTELLATION_GALILEO && broadcast->wnValid &&
                               broadcast->wn >= 0 && broadcast->wn < 4096) {
                        const double rxLlh[3] = {lat, lon, height};
                        double satelliteLlh[3];
                        const double utc =
                            7.0 * broadcast->wn + (broadcast->tow + Week(tx[k] - broadcast->tow) -
                                                   broadcast->iono.utcLeap) /
                                                      86400.0;
                        Geodetic(rotated, &satelliteLlh[0], &satelliteLlh[1], &satelliteLlh[2]);
                        iono = NgNeQuickE1(broadcast->iono.alphaGal, GalileoMonth(utc),
                                           24 * (utc - floor(utc)), rxLlh, satelliteLlh);
                    } else if (broadcastIonosphere && broadcast->ionoReceived &&
                               systems[k] == NG_CONSTELLATION_BEIDOU && broadcast->wnValid) {
                        const double bdt = nativeTx[k];
                        const double mjd = 53736.0 + 7.0 * broadcast->wn +
                                           (bdt - broadcast->iono.utcLeap) / 86400.0;
                        iono = NgBdgimDelay(broadcast->iono.alphaBds, mjd, lat, lon, azimuth,
                                            elevation);
                    }
                }
            }
            if (iono > 0) {
                ++ionosphereCorrected[systems[k] - 1];
            }
            rhs[k] = code[k] + NG_C * clocks[k] - range - pos[clockIndex] - tropo - iono;
            residual += rhs[k] * rhs[k];
        }
        if (!Solve(matrix, rhs, n, cols, correction, NULL)) {
            return;
        }
        for (j = 0; j < cols; ++j) {
            pos[j] += correction[j];
        }
        double maximumClockStep = 0;
        for (j = 3; j < cols; ++j) {
            maximumClockStep = fmax(maximumClockStep, fabs(correction[j]));
        }
        if (hypot(hypot(correction[0], correction[1]), correction[2]) < 1e-4 &&
            maximumClockStep < 1e-4) {
            break;
        }
    }
    if (iteration == 20) {
        return;
    }
    Geodetic(pos, &solution->latitude, &solution->longitude, &solution->height);
    if (!isfinite(solution->height) || fabs(solution->height) > 1e6) {
        return;
    }
    memcpy(solution->ecef, pos, sizeof(solution->ecef));
    if (solution->gps) {
        solution->clockGps = pos[clockColumn[NG_CONSTELLATION_GPS - 1]];
    }
    if (solution->galileo) {
        solution->clockGalileo = pos[clockColumn[NG_CONSTELLATION_GALILEO - 1]];
    }
    if (solution->beidou) {
        solution->clockBeidou = pos[clockColumn[NG_CONSTELLATION_BEIDOU - 1]];
    }
    solution->residualRms = sqrt(residual / n);
    memcpy(solution->ionosphereCorrected, ionosphereCorrected, sizeof(ionosphereCorrected));
    solution->egnosIonosphereCorrected = egnosIonoCount;
    solution->available = 1;
    memset(matrix, 0, sizeof(matrix));
    for (k = 0; k < n; ++k) {
        double rotated[3], rotatedVelocity[3], angle;
        double range = hypot(hypot(satellite[k][0] - pos[0], satellite[k][1] - pos[1]),
                             satellite[k][2] - pos[2]);
        for (j = 0; j < 2; ++j) {
            angle = EarthRotation(systems[k]) * range / NG_C;
            rotated[0] = cos(angle) * satellite[k][0] + sin(angle) * satellite[k][1];
            rotated[1] = -sin(angle) * satellite[k][0] + cos(angle) * satellite[k][1];
            rotated[2] = satellite[k][2];
            range = hypot(hypot(rotated[0] - pos[0], rotated[1] - pos[1]), rotated[2] - pos[2]);
        }
        angle = EarthRotation(systems[k]) * range / NG_C;
        rotatedVelocity[0] = cos(angle) * velocity[k][0] + sin(angle) * velocity[k][1];
        rotatedVelocity[1] = -sin(angle) * velocity[k][0] + cos(angle) * velocity[k][1];
        rotatedVelocity[2] = velocity[k][2];
        rhs[k] = rates[k] + NG_C * drifts[k];
        for (j = 0; j < 3; ++j) {
            double unit = (rotated[j] - pos[j]) / range;
            matrix[k][j] = -unit;
            rhs[k] -= unit * rotatedVelocity[j];
        }
        matrix[k][clockColumn[systems[k] - 1]] = 1;
        {
            TNgPvtSatelliteView *view = &solution->sky[k];
            double east =
                matrix[k][0] * sin(solution->longitude) - matrix[k][1] * cos(solution->longitude);
            double north = matrix[k][0] * sin(solution->latitude) * cos(solution->longitude) +
                           matrix[k][1] * sin(solution->latitude) * sin(solution->longitude) -
                           matrix[k][2] * cos(solution->latitude);
            double up = -matrix[k][0] * cos(solution->latitude) * cos(solution->longitude) -
                        matrix[k][1] * cos(solution->latitude) * sin(solution->longitude) -
                        matrix[k][2] * sin(solution->latitude);
            view->prn = prns[k];
            view->constellation = systems[k];
            view->azimuth = atan2(east, north);
            if (view->azimuth < 0) {
                view->azimuth += 2 * NG_PI;
            }
            view->elevation = asin(fmax(-1, fmin(1, up)));
        }
    }
    solution->skyCount = n;
    {
        double geometry[64][NG_PVT_COLUMNS], zero[64] = {0};
        double covariance[NG_PVT_COLUMNS][NG_PVT_COLUMNS], unused[NG_PVT_COLUMNS];
        double enu[3][3] = {
            {-sin(solution->longitude), cos(solution->longitude), 0},
            {-sin(solution->latitude) * cos(solution->longitude),
             -sin(solution->latitude) * sin(solution->longitude), cos(solution->latitude)},
            {cos(solution->latitude) * cos(solution->longitude),
             cos(solution->latitude) * sin(solution->longitude), sin(solution->latitude)}};
        double variance[3] = {0}, trace = 0;
        memcpy(geometry, matrix, sizeof(geometry));
        if (Solve(geometry, zero, n, cols, unused, covariance)) {
            for (k = 0; k < 3; ++k) {
                for (j = 0; j < 3; ++j) {
                    for (unsigned axis = 0; axis < 3; ++axis) {
                        variance[k] += enu[k][j] * covariance[j][axis] * enu[k][axis];
                    }
                }
            }
            for (k = 0; k < cols; ++k) {
                trace += covariance[k][k];
            }
            solution->hdop = sqrt(fmax(0, variance[0] + variance[1]));
            solution->vdop = sqrt(fmax(0, variance[2]));
            solution->gdop = sqrt(fmax(0, trace));
            solution->dopAvailable =
                isfinite(solution->hdop) && isfinite(solution->vdop) && isfinite(solution->gdop);
        }
    }
    /* Doppler uses one common clock drift, independent of the code-bias columns. */
    for (k = 0; k < n; ++k) {
        matrix[k][3] = 1;
    }
    if (Solve(matrix, rhs, n, 4, correction, NULL)) {
        memcpy(solution->velocity, correction, sizeof(solution->velocity));
        solution->clockDrift = correction[3];
        solution->velocityAvailable = 1;
    }
}

void NgPvtComputeWithIonosphere(const TNgTrackingSnapshot channels[64], uint64_t epoch,
                                uint64_t sample, double rate, int broadcastIonosphere,
                                TNgPvtSnapshot *solution)
{
    NgPvtComputeAugmented(channels, epoch, sample, rate, broadcastIonosphere, NULL, solution);
}

/** Preserve the existing solver entry point with broadcast corrections enabled. */
void NgPvtCompute(const TNgTrackingSnapshot channels[64], uint64_t epoch, uint64_t sample,
                  double sampleRate, TNgPvtSnapshot *solution)
{
    NgPvtComputeWithIonosphere(channels, epoch, sample, sampleRate, 1, solution);
}
