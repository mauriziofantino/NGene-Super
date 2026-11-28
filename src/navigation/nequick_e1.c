/** @file nequick_e1.c @brief Galileo E1 adapter for the Classic NeQuick integration. */
#include "navigation/ionosphere.h"
#include "navigation/nequick/ionoCalcModip.h"
#include "navigation/nequick/nequickComputation.h"
#include <math.h>
#include "navigation/nequick/tables.inc"

double NgNeQuickE1(const double alpha[3], int month, double utcHours, const double receiver[3],
                   const double satellite[3])
{
    const double l1 = 1575420000.0;
    double modip = 0;
    double stec = 0;
    double tolerance[2] = {0.001, 0.01};
    double rx[3] = {receiver[0], receiver[1], receiver[2]};
    double sv[3] = {satellite[0], satellite[1], satellite[2]};
    TCurrentCcir cache = {0};
    if (!IonoCalcModip(&modipGrid, rx[0], rx[1], &modip)) {
        return 0;
    }
    double az = alpha[0] + modip * (alpha[1] + modip * alpha[2]);
    az = alpha[0] == 0 && alpha[1] == 0 && alpha[2] == 0 ? 63.7 : fmax(0, fmin(400, az));
    TNeQuickInputData input = {.pstModip = &modipGrid,
                               .pstCCIR = &ccir,
                               .pdKronrodTol = tolerance,
                               .siMaxRecurse = 50,
                               .pdGssPosLLH = rx,
                               .pdSatPosLLH = sv,
                               .siMonth = month,
                               .dUT = utcHours,
                               .siNumCoeff = 1,
                               .pdCoeff = &az};
    if (NeQuickComputation(&input, &cache, &stec) != E_OK || !isfinite(stec)) {
        return 0;
    }
    return 40.3 * fmax(0, stec) / (l1 * l1);
}
