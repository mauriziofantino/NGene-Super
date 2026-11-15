/** @file pvt.c @brief Classic broadcast orbit, Earth rotation and iterative code/Doppler LS.
 * Adapted from pvt/pvt.c, leastSquares.c and pvtUtility.c, Mario Nicola, 2014.
 * Small C Householder QR replaces the Eigen boundary, retaining double arithmetic.
 */
#include "ngene/pvt.h"
#include <math.h>
#include <string.h>
#define NG_C 299792458.0
#define NG_OMEGA 7.2921151467e-5
#define NG_PI 3.14159265358979323846

/** Signed week difference, including a reference across Sunday rollover. */
static double Week(double time)
{
    return time - 604800.0 * floor((time + 302400.0) / 604800.0);
}

/** Broadcast Kepler/harmonic equations, analytic velocity, relativistic clock correction. */
int NgPvtSatellite(const TNgBroadcast *b, unsigned system, double tx, double x[3], double v[3],
                   double *clock, double *drift)
{
    const TNgOrbit *o = &b->eph;
    const TNgBroadcastClock *c = &b->clk;
    double dt, a, tk, n, m, e, dtr, nu, phi, u, r, inc, omega;
    double ed, nud, ud, rd, id, xp, yp, xpd, ypd, od;
    double relativity = system == 1 ? -4.442807633e-10 : -4.442807309e-10;
    unsigned k;
    if (!b->ephemeridesReceived || (system != 1 && system != 2) || !isfinite(tx) ||
        !isfinite(o->sqrtA) || o->sqrtA < 4000 || o->sqrtA > 7000 || !isfinite(o->eccentricity) ||
        o->eccentricity < 0 || o->eccentricity >= 1 || fabs(Week(tx - o->toe)) > 14400)
        return 0;
    dt = Week(tx - c->toc);
    *clock = (c->af[2] * dt + c->af[1]) * dt + c->af[0] - c->tGroupDelay;
    a = o->sqrtA * o->sqrtA;
    tk = Week(tx - *clock - o->toe);
    n = sqrt((system == 1 ? 3.986005e14 : 3.986004418e14) / (a * a * a)) + o->deltaN;
    m = fmod(o->meanAnomaly0 + n * tk + 2 * NG_PI, 2 * NG_PI);
    e = m;
    for (k = 0; k < 10; ++k) {
        double previous = e;
        e = m + o->eccentricity * sin(e);
        if (fabs(e - previous) < 1e-12)
            break;
    }
    if (fabs(e - m - o->eccentricity * sin(e)) > 1e-10)
        return 0;
    dtr = relativity * o->eccentricity * o->sqrtA * sin(e);
    nu = atan2(sqrt(1 - o->eccentricity * o->eccentricity) * sin(e), cos(e) - o->eccentricity);
    phi = nu + o->omega;
    u = phi + o->capitalCuc * cos(2 * phi) + o->capitalCus * sin(2 * phi);
    r = a * (1 - o->eccentricity * cos(e)) + o->capitalCrc * cos(2 * phi) +
        o->capitalCrs * sin(2 * phi);
    inc = o->i0 + o->iDot * tk + o->capitalCic * cos(2 * phi) + o->capitalCis * sin(2 * phi);
    omega = o->capitalOmega0 + (o->capitalOmegaDot - NG_OMEGA) * tk - NG_OMEGA * o->toe;
    xp = r * cos(u);
    yp = r * sin(u);
    x[0] = xp * cos(omega) - yp * cos(inc) * sin(omega);
    x[1] = xp * sin(omega) + yp * cos(inc) * cos(omega);
    x[2] = yp * sin(inc);
    ed = n / (1 - o->eccentricity * cos(e));
    nud = sqrt(1 - o->eccentricity * o->eccentricity) * ed / (1 - o->eccentricity * cos(e));
    ud = nud * (1 + 2 * (o->capitalCus * cos(2 * phi) - o->capitalCuc * sin(2 * phi)));
    rd = a * o->eccentricity * sin(e) * ed +
         2 * (o->capitalCrs * cos(2 * phi) - o->capitalCrc * sin(2 * phi)) * nud;
    id = o->iDot + 2 * (o->capitalCis * cos(2 * phi) - o->capitalCic * sin(2 * phi)) * nud;
    xpd = rd * cos(u) - yp * ud;
    ypd = rd * sin(u) + xp * ud;
    od = o->capitalOmegaDot - NG_OMEGA;
    v[0] = (xpd - yp * cos(inc) * od) * cos(omega) -
           (xp * od + ypd * cos(inc) - yp * sin(inc) * id) * sin(omega);
    v[1] = (xpd - yp * cos(inc) * od) * sin(omega) +
           (xp * od + ypd * cos(inc) - yp * sin(inc) * id) * cos(omega);
    v[2] = ypd * sin(inc) + yp * cos(inc) * id;
    *clock += dtr;
    *drift = c->af[1] + 2 * c->af[2] * dt + relativity * o->eccentricity * o->sqrtA * cos(e) * ed;
    return isfinite(x[0]) && isfinite(x[1]) && isfinite(x[2]) && isfinite(*clock) &&
           isfinite(v[0]) && isfinite(v[1]) && isfinite(v[2]) && isfinite(*drift);
}

/** Column-pivoted Householder QR; no normal-equation squaring of the condition number. */
static int Solve(double a[64][5], double b[64], unsigned rows, unsigned cols, double x[5],
                 double covariance[5][5])
{
    unsigned permutation[5] = {0, 1, 2, 3, 4}, k, i, j;
    double y[5] = {0};
    if (rows < cols)
        return 0;
    for (k = 0; k < cols; ++k) {
        unsigned pivot = k;
        double maximum = 0, norm, alpha, beta;
        for (j = k; j < cols; ++j) {
            double value = 0;
            for (i = k; i < rows; ++i)
                value = hypot(value, a[i][j]);
            if (value > maximum) {
                maximum = value;
                pivot = j;
            }
        }
        if (!(maximum > 1e-10) || !isfinite(maximum))
            return 0;
        for (i = 0; i < rows; ++i) {
            double temp = a[i][k];
            a[i][k] = a[i][pivot];
            a[i][pivot] = temp;
        }
        {
            unsigned temp = permutation[k];
            permutation[k] = permutation[pivot];
            permutation[pivot] = temp;
        }
        norm = maximum;
        alpha = -copysign(norm, a[k][k]);
        beta = 1.0 / (norm * (norm + fabs(a[k][k])));
        a[k][k] -= alpha;
        for (j = k + 1; j <= cols; ++j) {
            double product = 0;
            for (i = k; i < rows; ++i)
                product += a[i][k] * (j == cols ? b[i] : a[i][j]);
            product *= beta;
            for (i = k; i < rows; ++i)
                if (j == cols)
                    b[i] -= product * a[i][k];
                else
                    a[i][j] -= product * a[i][k];
        }
        a[k][k] = alpha;
        for (i = k + 1; i < rows; ++i)
            a[i][k] = 0;
    }
    for (i = cols; i-- > 0;) {
        double value = b[i];
        for (j = i + 1; j < cols; ++j)
            value -= a[i][j] * y[j];
        y[i] = value / a[i][i];
        if (!isfinite(y[i]))
            return 0;
    }
    for (i = 0; i < cols; ++i)
        x[permutation[i]] = y[i];
    if (covariance) {
        double inverse[5][5] = {{0}};
        memset(covariance, 0, 25 * sizeof(double));
        for (j = 0; j < cols; ++j)
            for (i = cols; i-- > 0;) {
                double value = i == j ? 1.0 : 0.0;
                for (k = i + 1; k < cols; ++k)
                    value -= a[i][k] * inverse[k][j];
                inverse[i][j] = value / a[i][i];
            }
        for (i = 0; i < cols; ++i)
            for (j = 0; j < cols; ++j)
                for (k = 0; k < cols; ++k)
                    covariance[permutation[i]][permutation[j]] += inverse[i][k] * inverse[j][k];
    }
    return 1;
}

/** WGS84 iterative latitude and ellipsoid height; longitude uses atan2. */
static void Geodetic(const double x[3], double *latitude, double *longitude, double *height)
{
    const double e2 = 6.6943799901413165e-3;
    double p = hypot(x[0], x[1]), lat = atan2(x[2], p * (1 - e2)), radius;
    unsigned k;
    for (k = 0; k < 12; ++k) {
        radius = 6378137.0 / sqrt(1 - e2 * sin(lat) * sin(lat));
        lat = atan2(x[2] + e2 * radius * sin(lat), p);
    }
    radius = 6378137.0 / sqrt(1 - e2 * sin(lat) * sin(lat));
    *latitude = lat;
    *longitude = atan2(x[1], x[0]);
    *height = p * cos(lat) + x[2] * sin(lat) - radius * (1 - e2 * sin(lat) * sin(lat));
}

/** Classic Hopfield default atmosphere, positive code delay in metres. */
static double Troposphere(double elevation, double height)
{
    double base = 1 - 0.00558 * height / 283.15;
    double dry = 1e-6 * 77.604 * 287.054 * 1015.75 / 9.784;
    double wet = 1e-6 * 382000 * 287.054 / (9.784 * 3.57 - 0.00558 * 287.054) * 11.66 / 283.15;
    if (base <= 0)
        return 0;
    dry *= pow(base, 9.80665 / (287.054 * 0.00558));
    wet *= pow(base, 3.57 * 9.80665 / (287.054 * 0.00558) - 1);
    return (dry + wet) * 1.001 / sqrt(0.002001 + sin(elevation) * sin(elevation));
}

/** Align observations to one sample, then solve separate GPS/Galileo clock biases. */
void NgPvtCompute(const TNgTrackingSnapshot channels[64], uint64_t epoch, uint64_t sample,
                  double fs, TNgPvtSnapshot *s)
{
    double satellite[64][3], velocity[64][3], clocks[64], drifts[64], tx[64], doppler[64];
    unsigned systems[64], prns[64], n = 0, k, j, iteration, cols, gps = 0, galileo = 0;
    double nominal = 0, pos[5] = {0}, matrix[64][5], rhs[64], correction[5];
    double lat = 0, lon = 0, height = 0, residual = 0;
    memset(s, 0, sizeof(*s));
    s->epoch = epoch;
    s->sample = sample;
    if (!(fs > 0) || !isfinite(fs))
        return;
    for (k = 0; k < 64; ++k) {
        const TNgTrackingSnapshot *t = &channels[k];
        const TNgNavigationSnapshot *nav = &t->navigation;
        const TNgBroadcast *b = &nav->broadcast;
        double time;
        if (!t->active || !t->pll || t->epoch != epoch || !nav->towValid ||
            !b->ephemeridesReceived || b->eph.health || b->eph.galileoDataValidity ||
            (t->constellation == 2 && b->satelliteId && (unsigned)b->satelliteId != t->prn) ||
            sample < t->endSample || (double)(sample - t->endSample) / fs > 0.01 ||
            !isfinite(nav->transmitTimeSeconds) || !isfinite(t->codeRateHz) ||
            !isfinite(t->dopplerHz) || (t->constellation != 1 && t->constellation != 2))
            continue;
        time = nav->transmitTimeSeconds +
               (double)(sample - t->endSample) / fs * t->codeRateHz / 1023000.0;
        if (n)
            time = tx[0] + Week(time - tx[0]);
        if (!NgPvtSatellite(b, t->constellation, time, satellite[n], velocity[n], &clocks[n],
                            &drifts[n]))
            continue;
        tx[n] = time;
        doppler[n] = t->dopplerHz;
        systems[n] = t->constellation;
        prns[n] = t->prn;
        if (!n || time > nominal)
            nominal = time;
        if (t->constellation == 1)
            gps++;
        else
            galileo++;
        n++;
    }
    s->satellites = n;
    s->gps = gps;
    s->galileo = galileo;
    cols = gps && galileo ? 5u : 4u;
    if (n < cols)
        return;
    nominal += 0.07;
    s->towSeconds = fmod(nominal + 604800.0, 604800.0);
    for (iteration = 0; iteration < 20; ++iteration) {
        residual = 0;
        if (iteration)
            Geodetic(pos, &lat, &lon, &height);
        memset(matrix, 0, sizeof(matrix));
        for (k = 0; k < n; ++k) {
            double rotated[3], range, angle, elevation = 0, tropo = 0;
            unsigned clockIndex = cols == 5 && systems[k] == 2 ? 4u : 3u;
            range = hypot(hypot(satellite[k][0] - pos[0], satellite[k][1] - pos[1]),
                          satellite[k][2] - pos[2]);
            for (j = 0; j < 2; ++j) {
                angle = NG_OMEGA * range / NG_C;
                rotated[0] = cos(angle) * satellite[k][0] + sin(angle) * satellite[k][1];
                rotated[1] = -sin(angle) * satellite[k][0] + cos(angle) * satellite[k][1];
                rotated[2] = satellite[k][2];
                range = hypot(hypot(rotated[0] - pos[0], rotated[1] - pos[1]), rotated[2] - pos[2]);
            }
            if (!(range > 0) || !isfinite(range))
                return;
            for (j = 0; j < 3; ++j)
                matrix[k][j] = (pos[j] - rotated[j]) / range;
            matrix[k][clockIndex] = 1;
            if (iteration && fabs(height) < 1e6) {
                double up = -matrix[k][0] * cos(lat) * cos(lon) -
                            matrix[k][1] * cos(lat) * sin(lon) - matrix[k][2] * sin(lat);
                elevation = asin(fmax(-1, fmin(1, up)));
                if (elevation > 0)
                    tropo = Troposphere(elevation, height);
            }
            rhs[k] = NG_C * (nominal - tx[k] + clocks[k]) - range - pos[clockIndex] - tropo;
            residual += rhs[k] * rhs[k];
        }
        if (!Solve(matrix, rhs, n, cols, correction, NULL))
            return;
        for (j = 0; j < cols; ++j)
            pos[j] += correction[j];
        if (hypot(hypot(correction[0], correction[1]), correction[2]) < 1e-4 &&
            fabs(correction[3]) < 1e-4 && (cols != 5 || fabs(correction[4]) < 1e-4))
            break;
    }
    if (iteration == 20)
        return;
    Geodetic(pos, &s->latitude, &s->longitude, &s->height);
    if (!isfinite(s->height) || fabs(s->height) > 1e6)
        return;
    memcpy(s->ecef, pos, sizeof(s->ecef));
    s->clockGps = gps ? pos[3] : 0;
    s->clockGalileo = galileo ? pos[cols == 5 ? 4 : 3] : 0;
    s->residualRms = sqrt(residual / n);
    s->available = 1;
    memset(matrix, 0, sizeof(matrix));
    for (k = 0; k < n; ++k) {
        double rotated[3], rotatedVelocity[3], angle;
        double range = hypot(hypot(satellite[k][0] - pos[0], satellite[k][1] - pos[1]),
                             satellite[k][2] - pos[2]);
        for (j = 0; j < 2; ++j) {
            angle = NG_OMEGA * range / NG_C;
            rotated[0] = cos(angle) * satellite[k][0] + sin(angle) * satellite[k][1];
            rotated[1] = -sin(angle) * satellite[k][0] + cos(angle) * satellite[k][1];
            rotated[2] = satellite[k][2];
            range = hypot(hypot(rotated[0] - pos[0], rotated[1] - pos[1]), rotated[2] - pos[2]);
        }
        angle = NG_OMEGA * range / NG_C;
        rotatedVelocity[0] = cos(angle) * velocity[k][0] + sin(angle) * velocity[k][1];
        rotatedVelocity[1] = -sin(angle) * velocity[k][0] + cos(angle) * velocity[k][1];
        rotatedVelocity[2] = velocity[k][2];
        rhs[k] = -doppler[k] * NG_C / 1575420000.0 + NG_C * drifts[k];
        for (j = 0; j < 3; ++j) {
            double unit = (rotated[j] - pos[j]) / range;
            matrix[k][j] = -unit;
            rhs[k] -= unit * rotatedVelocity[j];
        }
        matrix[k][cols == 5 && systems[k] == 2 ? 4 : 3] = 1;
        {
            TNgPvtSatelliteView *view = &s->sky[k];
            double east = matrix[k][0] * sin(s->longitude) - matrix[k][1] * cos(s->longitude);
            double north = matrix[k][0] * sin(s->latitude) * cos(s->longitude) +
                           matrix[k][1] * sin(s->latitude) * sin(s->longitude) -
                           matrix[k][2] * cos(s->latitude);
            double up = -matrix[k][0] * cos(s->latitude) * cos(s->longitude) -
                        matrix[k][1] * cos(s->latitude) * sin(s->longitude) -
                        matrix[k][2] * sin(s->latitude);
            view->prn = prns[k];
            view->constellation = systems[k];
            view->azimuth = atan2(east, north);
            if (view->azimuth < 0)
                view->azimuth += 2 * NG_PI;
            view->elevation = asin(fmax(-1, fmin(1, up)));
        }
    }
    s->skyCount = n;
    {
        double geometry[64][5], zero[64] = {0}, covariance[5][5], unused[5];
        double enu[3][3] = {{-sin(s->longitude), cos(s->longitude), 0},
                            {-sin(s->latitude) * cos(s->longitude),
                             -sin(s->latitude) * sin(s->longitude), cos(s->latitude)},
                            {cos(s->latitude) * cos(s->longitude),
                             cos(s->latitude) * sin(s->longitude), sin(s->latitude)}};
        double variance[3] = {0}, trace = 0;
        memcpy(geometry, matrix, sizeof(geometry));
        if (Solve(geometry, zero, n, cols, unused, covariance)) {
            for (k = 0; k < 3; ++k)
                for (j = 0; j < 3; ++j)
                    for (unsigned axis = 0; axis < 3; ++axis)
                        variance[k] += enu[k][j] * covariance[j][axis] * enu[k][axis];
            for (k = 0; k < cols; ++k)
                trace += covariance[k][k];
            s->hdop = sqrt(fmax(0, variance[0] + variance[1]));
            s->vdop = sqrt(fmax(0, variance[2]));
            s->gdop = sqrt(fmax(0, trace));
            s->dopAvailable = isfinite(s->hdop) && isfinite(s->vdop) && isfinite(s->gdop);
        }
    }
    /* Doppler uses one common clock drift, independent of the code-bias columns. */
    for (k = 0; k < n; ++k) {
        matrix[k][3] = 1;
        matrix[k][4] = 0;
    }
    if (Solve(matrix, rhs, n, 4, correction, NULL)) {
        memcpy(s->velocity, correction, sizeof(s->velocity));
        s->clockDrift = correction[3];
        s->velocityAvailable = 1;
    }
}
