/**
 * @file    nequickComputation.h
 *
 * @author  Paul Whitehall - Nottingham Scientific Ltd
 *
 * @date    23/3/2010
 *
 * @version 1.0.0
 *
 * @brief   This module contains the NSL implementation of the NeQuick model main entry point.
 *
 *          From the NeQuick model reference code with minor changes.
 */

#ifndef NEQUICK_COMPUTATION_H_INCLUDED
#define NEQUICK_COMPUTATION_H_INCLUDED

#include "neqCalcEpstParams.h"
#include "neqGetRayProperties.h"
#include "neqIntegrate.h"
#include "nequickDataStructures.h"
#include "nequickConstants.h"

/**
 * @brief This function calculates the total electron content (TEC) between two
 *        points. It calculates STEC between the points or VTEC if point 2 is almost
 *        directly above point 1. Point 1 is the sensor station and point 2 is the
 *        satellite. The value is calculated from the month, time and Az value, as
 *        well as the two positions.
 *
 * @param pstNeQuickInputData Input data for NeQuick function [in]
 * @param pstCurrCCIR         Current CCIR data (CCIR map for current month and R12 values)- used internally.  [out]
 * @param pdSTEC              Calculated TEC between points p1 and p2 [out]
 *
 * @return E_ERROR if error found, otherwise E_OK
 */
TReturnValue NeQuickComputation(TNeQuickInputData *pstNeQuickInputData,TCurrentCcir *pstCurrCCIR, double *pdSTEC);

#endif /* NEQUICK_COMPUTATION_H_INCLUDED */
