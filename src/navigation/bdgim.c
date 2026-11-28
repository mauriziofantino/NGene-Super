/** @file bdgim.c @brief Single-frequency BeiDou B1C broadcast ionospheric delay. */
#include "navigation/ionosphere.h"
#include <math.h>
#include <stdlib.h>

#define NG_IONO_PI 3.14159265358979323846
#define NG_B1C_HZ 1575420000.0

/** B1C SIS ICD Table 7-12, in the tabulated harmonic order. */
static const double mean[17] = {-0.61, -1.31, -2.00, -0.03, 0.15, -0.48, -0.40, 2.28, -0.16,
                                -0.21, -0.10, -0.13, 0.21,  0.68, 1.06,  0,     -0.12};

static const struct {
    double period, cosine[17], sine[17];
} terms[] = {{1,
              {-0.51, -0.43, 0.34, -0.01, 0.17, 0.02, -0.06, 0.30, 0.44, -0.28, -0.31, -0.17, 0.04,
               0.39, -0.12, 0.12, 0},
              {0.23, -0.20, -0.31, 0.16, -0.03, 0.02, 0.04, 0.18, 0.34, 0.45, 0.19, -0.25, -0.12,
               0.18, 0.40, -0.09, 0.21}},
             {0.5,
              {-0.06, -0.05, 0.06, 0.17, 0.15, 0, 0.11, -0.05, -0.16, 0.02, 0.11, 0.04, 0.12, 0.07,
               0.02, -0.14, -0.14},
              {0.02, -0.08, -0.06, -0.11, 0.15, -0.14, 0.01, 0.01, 0.04, -0.14, -0.05, 0.08, 0.08,
               -0.01, 0.01, 0.11, -0.12}},
             {0.33,
              {0.01, -0.03, 0.01, -0.01, 0.05, -0.03, 0.05, -0.03, -0.01, 0, -0.08, -0.04, 0, -0.02,
               -0.03, 0, -0.03},
              {0, -0.02, -0.03, -0.05, -0.01, -0.07, -0.03, -0.01, 0.02, -0.01, 0.03, -0.10, 0.01,
               0.05, -0.01, 0.04, 0.00}},
             {14.6,
              {-0.01, 0, 0.01, 0, 0.01, 0, -0.01, -0.01, 0, 0, 0, 0, 0, 0, 0, 0, 0},
              {0, -0.02, 0.01, 0, -0.01, 0.01, 0, -0.02, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
             {27.0,
              {0, 0, 0.03, 0.01, 0.02, 0.01, 0, -0.02, 0, 0, 0, 0, 0, 0, 0, 0, 0},
              {0.01, 0, 0, 0.01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
             {121.6,
              {-0.19, -0.02, 0.12, -0.10, 0.06, 0, -0.02, -0.08, -0.02, -0.07, 0.01, 0.03, 0.15,
               0.06, -0.05, -0.03, -0.10},
              {-0.09, 0.07, 0.03, 0.06, 0.09, 0.01, 0.02, 0, -0.04, -0.02, -0.01, 0.01, -0.10, 0,
               -0.01, 0.02, 0.05}},
             {182.51,
              {-0.18, 0.06, -0.55, -0.02, 0.09, -0.08, 0, 0.86, -0.18, -0.05, -0.07, 0.04, 0.14,
               -0.03, 0.37, -0.11, -0.12},
              {0.15, -0.31, 0.13, 0.05, -0.09, -0.03, 0.06, -0.36, 0.08, 0.05, 0.06, -0.02, -0.05,
               0.06, -0.20, 0.04, 0.07}},
             {365.25,
              {1.09, -0.14, -0.21, 0.52, 0.27, 0, 0.11, 0.17, 0.23, 0.35, -0.05, 0.02, -0.60, 0.02,
               0.01, 0.27, 0.32},
              {0.50, -0.08, -0.38, 0.36, 0.14, 0.04, 0, 0.25, 0.17, 0.27, -0.03, -0.03, -0.32,
               -0.10, 0.20, 0.10, 0.30}},
             {4028.71,
              {-0.34, -0.09, -1.22, 0.05, 0.15, -0.29, -0.17, 1.58, -0.06, -0.15, 0.00, 0.13, 0.28,
               -0.08, 0.62, -0.01, -0.04},
              {0, -0.11, -0.22, 0.01, 0.02, -0.03, -0.01, 0.49, -0.03, -0.02, 0.01, 0.02, 0.04,
               -0.04, 0.16, -0.02, -0.01}},
             {2014.35,
              {-0.13, 0.07, -0.37, 0.05, 0.06, -0.11, -0.07, 0.46, 0.00, -0.04, 0.01, 0.07, 0.09,
               -0.05, 0.15, -0.01, 0.01},
              {0.05, 0.03, 0.07, 0.02, -0.01, 0.03, 0.02, -0.04, -0.01, -0.01, 0.02, 0.03, 0.02,
               -0.04, -0.04, -0.01, 0}},
             {1342.90,
              {-0.06, 0.13, -0.07, 0.03, 0.02, -0.05, -0.05, 0.01, 0, 0, 0, 0, 0, 0, 0, 0, 0},
              {0.03, -0.02, 0.04, -0.01, -0.03, 0.02, 0.01, 0.04, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
             {1007.18,
              {-0.03, 0.08, -0.01, 0.04, 0.01, -0.02, -0.02, -0.04, 0, 0, 0, 0, 0, 0, 0, 0, 0},
              {0.04, -0.02, -0.04, 0.00, -0.01, 0, 0.01, 0.07, 0, 0, 0, 0, 0, 0, 0, 0, 0}}};

/** Evaluate the real, fully normalized spherical harmonics through degree five. */
static double Harmonic(double p[6][6], int n, int m, double longitude)
{
    int order = abs(m);
    double norm = (2.0 * n + 1.0) * (order ? 2.0 : 1.0);
    for (int k = n - order + 1; k <= n + order; ++k) {
        norm /= k;
    }
    return sqrt(norm) * p[n][order] * (m < 0 ? sin(order * longitude) : cos(order * longitude));
}

double NgBdgimDelay(const double alpha[9], double mjd, double latitude, double longitude,
                    double azimuth, double elevation)
{
    static const int degree[26] = {0, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3,
                                   3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5};
    static const int order[26] = {0,  0, 1,  -1, 0, 1,  -1, 2,  -2, 0, 1,  -1, 2,
                                  -2, 3, -3, 0,  1, -1, 2,  -2, 0,  1, -1, 2,  -2};
    const double mapping = 6378.0 / 6778.0 * cos(elevation);
    const double psi = NG_IONO_PI / 2 - elevation - asin(mapping);
    const double sinLat = sin(latitude), cosLat = cos(latitude);
    const double sinPierce = sinLat * cos(psi) + cosLat * sin(psi) * cos(azimuth);
    const double cosPierce = sqrt(fmax(0, 1 - sinPierce * sinPierce));
    const double pierceLon =
        longitude + atan2(sin(psi) * sin(azimuth) * cosLat, cos(psi) - sinLat * sinPierce);
    const double poleLatitude = 80.27 * NG_IONO_PI / 180.0;
    const double poleLongitude = -72.58 * NG_IONO_PI / 180.0;
    const double relative = pierceLon - poleLongitude;
    const double sinPole = sin(poleLatitude), cosPole = cos(poleLatitude);
    const double x = fmax(-1, fmin(1, sinPole * sinPierce + cosPole * cosPierce * cos(relative)));
    const double magneticLon =
        atan2(cosPierce * sin(relative), sinPole * cosPierce * cos(relative) - cosPole * sinPierce);
    const double solarLon = NG_IONO_PI * (1 - 2 * (mjd - floor(mjd))) - poleLongitude;
    const double fixedLon = magneticLon - atan2(sin(solarLon), sinPole * cos(solarLon));
    const double nearestOddHour = (floor(mjd * 12.0) + 0.5) / 12.0;
    const double cosMagnetic = sqrt(fmax(0, 1 - x * x));
    double p[6][6] = {{1}};
    double tec = 0;
    for (int m = 0; m <= 5; ++m) {
        if (m) {
            p[m][m] = (2 * m - 1) * cosMagnetic * p[m - 1][m - 1];
        }
        for (int n = m + 1; n <= 5; ++n) {
            p[n][m] = (2 * n - 1) * x * p[n - 1][m] / (n - m);
            if (n > m + 1) {
                p[n][m] -= (n + m - 1) * p[n - 2][m] / (n - m);
            }
        }
    }
    double predicted[17];
    for (unsigned j = 0; j < 17; ++j) {
        predicted[j] = mean[j];
    }
    for (unsigned k = 0; k < sizeof(terms) / sizeof(terms[0]); ++k) {
        const double angle =
            2 * NG_IONO_PI * remainder(nearestOddHour, terms[k].period) / terms[k].period;
        const double c = cos(angle), s = sin(angle);
        for (unsigned j = 0; j < 17; ++j) {
            predicted[j] += c * terms[k].cosine[j] + s * terms[k].sine[j];
        }
    }
    for (unsigned k = 0; k < 26; ++k) {
        tec += (k < 9 ? alpha[k] : predicted[k - 9]) * Harmonic(p, degree[k], order[k], fixedLon);
    }
    return 40.28e16 / (NG_B1C_HZ * NG_B1C_HZ) * fmax(0, tec) / sqrt(1 - mapping * mapping);
}
