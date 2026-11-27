/**
 * @file    neqCalcBottomsideNe.h
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
#ifndef NEQ_CALC_BOTTOMSIDE_NE_H_INCLUDED
#define NEQ_CALC_BOTTOMSIDE_NE_H_INCLUDED
#include <math.h>
#include "nequickDataStructures.h"

/**
 * This function calculates electron content at the specified height (in the
 * bottom part of the ionosphere below pdPeakHeight[LAYER_F2], the F2 electron
 * density peak point).
 *
 * @param dHH      Height of the point (I)
 * @param ptLayers Properties for the Epstein layers (I)
 *
 * @return Electron content at specified height
 */
double NeqCalcBottomsideNe(double dHH, TLayerProperties *pstLayers);
#endif /* NEQ_CALC_BOTTOMSIDE_NE_H_INCLUDED */
