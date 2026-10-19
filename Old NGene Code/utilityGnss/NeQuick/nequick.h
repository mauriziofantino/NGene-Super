/**
 * @file    nequick.h
 *
 * @author  Mario Nicola
 * @author  Gianluca Falco
 *
 * @date    09/01/2015
 *
 * @version 1.0.0
 *
 * @brief   Functions that evaluates the ionosphere corrections through NeQuick model reference code
 *
 */

#ifndef NEQUICK_NEQUICK_H_INCLUDED
#define NEQUICK_NEQUICK_H_INCLUDED

#include "nequickDataStructures.h"

/**
 * @brief Initializes the NeQuick model data structures loading the parameters tables
 *
 * @param cfg points to the receiver configuration
 * @param nq points to the NeQuick model data structure
 */
void InitNeQuickModel(TNGeneConfiguration *cfg,TNeQuickData *nq);

/**
 * @brief De-initializes the NeQuick model data structures loading the parameters tables
 *
 * @param nq points to the NeQuick model data structure
 */
void DeinitNeQuickModel(TNeQuickData *nq);

/**
 * @brief Manages the inputs for the NeQuick model, then execute NeQuick model and returns the iono correction
 *
 * @param nq points to the NeQuick model data structure
 * @param pdCoeffs are coefficients from the navigation message
 * @param siMonth is the current month (1-12)
 * @param dTime is the time in the day [hours]
 * @param pdRecvLLHdeg is latitude, longitude and height of the receiver [degrees]
 * @param pdSatLLHdeg is latitude, longitude and height of the satellite [degrees]
 *
 * return iono correction [m]
 */
double GalileoIonosphericCorrection(TNeQuickData *nq,double *pdCoeffs, int siMonth, double dTime, double *pdRecvLLHdeg, double *pdSatLLHdeg);

#endif /* NEQUICK_NEQUICK_H_INCLUDED */
