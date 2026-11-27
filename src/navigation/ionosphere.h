/** @file ionosphere.h @brief Internal broadcast ionosphere models. */
#ifndef NG_IONOSPHERE_H
#define NG_IONOSPHERE_H

/** Evaluate BDGIM at B1C. Coefficients are in TECU, angles in radians and output in metres. */
double NgBdgimDelay(const double alpha[9], double mjd, double latitude, double longitude,
                    double azimuth, double elevation);
/** Evaluate NeQuick G at E1, using month 1..12, UTC hours and LLH in radians/metres. */
double NgNeQuickE1(const double alpha[3], int month, double utcHours,
                   const double receiver[3], const double satellite[3]);
#endif
