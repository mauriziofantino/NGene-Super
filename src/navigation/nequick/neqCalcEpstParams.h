/**
 * @file    neqCalcEpstParams.h
 *
 * @author  Paul Whitehall - Nottingham Scientific Ltd
 *
 * @date    23/3/2010
 *
 * @version 1.0.0
 *
 * @brief   This module contains the NSL implementation of the NeQuick model's function
 *          for calculating the properties of the Epstein layers.
 *
 *          From the NeQuick model reference code with minor changes.
 */
#ifndef NEQ_CALC_EPST_PARAMS_H_INCLUDED
#define NEQ_CALC_EPST_PARAMS_H_INCLUDED
#include "nequickDataStructures.h"

/**
 * @brief This function calculates the values of ionospheric properties for the
 *        current latitude, longitude, time, etc.
 *
 *   The ionospheric properties calculated (in order) are:
 *     pdF0         ([F2])
 *     dM3000
 *     pdF0         (    [E][F1])
 *     pdPeakHeight ([F2][E][F1])
 *     pdBotThick   ([F2]    [F1]   [E]                )
 *     pdTopThick   (    [F1]    [E]              [F2] )
 *     dAmp         (                  [F2][F1][E]     )
 *
 * @param pstNeQuickInputData Structure containing read only NeQuick inputs (I)
 * @param tPactual            The current position (dLat and dLng only) (I)
 * @param dSinDelta           Declination of sun (I)
 * @param dCosDelta           Declination of sun (I)
 * @param ptCurrCCIR          In: if already set, Out: if not set before (I/O)
 * @param pdNmax              (O)
 * @param ptLayers            The current ionospheric properties (O)
 */
void NeqCalcEpstParams(TNeQuickInputData *pstNeQuickInputData, TSPoint stPactual, double dSinDelta,
                       double dCosDelta, TCurrentCcir *pstCurrCCIR, double *pdNmax,
                       TLayerProperties *pstLayers);
#endif /* NEQ_CALC_EPST_PARAMS_H_INCLUDED */
