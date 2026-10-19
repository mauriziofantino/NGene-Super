/**
 * @file    neqGetRayProperties.h
 *
 * @author  Paul Whitehall - Nottingham Scientific Ltd
 *
 * @date    23/3/2010
 *
 * @version 1.0.0
 *
 * @brief   This module contains the NSL implementation of the NeQuick model's function
 *          which calculates geometrical properties for a ray (the path between two
 *          points in space).
 *
 *          From the NeQuick model reference code with minor changes.
 */

#ifndef NEQ_GET_RAY_PROPERTIES_H_INCLUDED
#define NEQ_GET_RAY_PROPERTIES_H_INCLUDED

#include "nequickDataStructures.h"
#include "nequickConstants.h"

/**
 * @brief This function checks if the ray is vertical and set P2 if it is. It then
 *        calculates the radius of P1 and P2 before calling NeqCalcRayProperties1 to
 *        calculate the properties of the ray and checks if it is valid. If the ray
 *        is slanted it then calculates some other ray properties by calling
 *        NeqCalcRayProperties2.
 *
 * @param ptP1     In: dLat/dLng(deg) and dH(km) only Out: dSinLat/dCosLat(slant) & dR(km) only  (I/O)
 * @param ptP2     In: dLat/dLng(deg) and dH(km) only Out: dLng(deg)(vert), dSinLat/dCosLat(slant) & dR(km)  (I/O)
 * @param ptRay    Ray perigee (dLat/dLng(deg) and dR(km) only)  (O)
 * @param pdZeta   Ray zenith angle (deg) (O)
 * @param pdSinSig Sin of azimuth(slant) (O)
 * @param pdCosSig Cos of azimuth(slant) (O)
 *
 * @return TRUE if error found, otherwise FALSE
 */
TBoolean NeqGetRayProperties(TSPoint *pstP1, TSPoint *pstP2, TSPoint *pstRay, double *pdZeta, double *pdSinSig, double *pdCosSig);

#endif /* NEQGETRAYPROPERTIES_H */
