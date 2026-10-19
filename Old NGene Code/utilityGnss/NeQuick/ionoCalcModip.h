/**
 * @file    ionoCalcModip.h
 *
 * @author  Tim May - Nottingham Scientific Ltd
 *
 * @date    18/5/2010
 *
 * @version 1.0.0
 *
 * @brief   The purpose of this module is to calculate the modified dip latitude for a
 *   		given latitude and longitude.
 *
 *			From the NeQuick model reference code with minor changes.
 */

#ifndef IONO_CALC_MODIP_H_INCLUDED
#define IONO_CALC_MODIP_H_INCLUDED

#include "nequickDataStructures.h"

/**
 * @brief The purpose of this function is to calculate the modified dip latitude for a
 *   	  given latitude and longitude.
 *
 * @param pstModipFile  Modip grid [in]
 * @param dLatRads Latitude in radians with range between -0.5pi and 0.5pi [in]
 * @param dLongRads Longitude in radians with range between -pi and pi [in]
 * @param pdModip The calculated MODIP value in degrees [out]
 *
 * @return the success status of the function (FALSE/TRUE)
 */
TBoolean IonoCalcModip(TModip *pstModipFile,double dLatRads,double dLongRads,double *pdModip);

#endif /* IONO_CALC_MODIP_H_INCLUDED */
