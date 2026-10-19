/**
 * @file    neqGetNeOnVertRay.h
 *
 * @author  Paul Whitehall - Nottingham Scientific Ltd
 *
 * @date    23/3/2010
 *
 * @version 1.0.0
 *
 * @brief   This module contains the NSL implementation of the NeQuick model's function
 *          which calculates the number of electrons on a vertical ray between two
 *          points.
 *
 *          From the NeQuick model reference code with minor changes.
 */

#ifndef NEQ_GET_NE_ON_VERT_RAY_H_INCLUDED
#define NEQ_GET_NE_ON_VERT_RAY_H_INCLUDED

#include "nequickDataStructures.h"


/**
 * @brief This function calculates the electron content at a specified point along a
 *        vertical ray.
 *
 * @param dH       Height of the point (km) (I)
 * @param ptLayers Properties for all the Epstein layers (pdAmp, pdPeakHeight, pdBotThick, pdTopThick for all)  (I)
 * @param pdNmax   Electron content at F2 peak In: if already set Out: if not set before  (I/O)
 *
 * @return Electron content at specified height
 */
double NeqGetNeOnVertRay(double dH,TLayerProperties *pstLayers, double *pdNmax);


#endif /* NEQ_GET_NE_ON_VERT_RAY_H_INCLUDED */
