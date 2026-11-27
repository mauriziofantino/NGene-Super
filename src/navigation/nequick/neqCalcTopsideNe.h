/**
 * @file    neqCalcTopsideNe.h
 *
 * @author  Paul Whitehall - Nottingham Scientific Ltd
 *
 * @date    23/3/2010
 *
 * @version 1.0.0
 *
 * @brief   This module contains the NSL implementation of the NeQuick model's function
 *          which calculates the electron content at a specified point in the botom part
 *          of the ionosphere (i.e. below the F2 electron density peak).
 *
 *          From the NeQuick model reference code with minor changes.
 */
#ifndef NEQ_CALC_TOPSIDE_NE_H_INCLUDED
#define NEQ_CALC_TOPSIDE_NE_H_INCLUDED
#include "nequickDataStructures.h"

/**
 * @brief This function calculates electron content at the specified height (in the
 *        top part of the ionosphere above pdPeakHeight[LAYER_F2], the F2 electron
 *        density peak point).
 *
 * @param dH       Height of the point (I)
 * @param ptLayers Properties for all the Epstein layers (pdAmp, pdPeakHeight, pdBotThick,
 * pdTopThick) (I)
 * @param pdNmax   Electron content at the F2 peak In: if already set, Out: if not set before (I/O)
 *
 * @return Electron content at specified height
 */
double NeqCalcTopsideNe(double dH, TLayerProperties *pstLayers, double *pdNmax);
#endif /* NEQ_CALC_TOPSIDE_NE_H_INCLUDED */
