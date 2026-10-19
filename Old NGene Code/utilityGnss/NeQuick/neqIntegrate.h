/**
 * @file    neqIntegrate.h
 *
 * @author  Paul Whitehall - Nottingham Scientific Ltd
 *
 * @date    23/3/2010
 *
 * @version 1.0.0
 *
 * @brief   This module contains the NSL implementation of the NeQuick model integration routine.
 *
 *          From the NeQuick model reference code with minor changes.
 */

#ifndef NEQ_INTEGRATE_H_INCLUDED
#define NEQ_INTEGRATE_H_INCLUDED


#include "nequickDataStructures.h"


/**
 * @brief This function performs integration to calculate the TEC along rays using
 *        the Kronrod G7-K15 adaptive quadrature method (see Knezevich and Radicella paper).
 *
 *   This method involves sampling values at 15 points and calculating the
 *   integration from them. At the same time it misses out half of the points to
 *   see what difference it makes and therefore the likely error contained in
 *   the result, before deciding whether to accept the result, or to split the
 *   portion into two and try again in order to improve accuracy.
 *
 *   This function is recursive and an appropriate safeguards has been
 *   implemented to prevent infinite looping.
 *
 *   Interestingly, when an integration is split into two equal parts the
 *   tolerance isn't halved. So, the accuracy of the result is +- (2*dTolerance)
 *   
 *
 * @param pstIntegrateData Structure containing readonly NeQuick inputs (I)
 * @param dH1              Height of point 1 (I)
 * @param dH2              Height of point 2 (I)
 * @param siCurrLevel      Current level of integration recursion (I)
 * @param pstPactual       dLat/dLng/dH updated for current position along slant (O)
 * @param pdNmax           Electron content at F2 peak (O)
 * @param pstLayers        Current ionospheric properties (see NeqCalcEpstParams) (O)
 *
 * @return Result of numerical integration
 */
double NeqIntegrate(TIntegrateData *pstIntegrateData, double dH1,double dH2, int siCurrLevel, TSPoint *pstPactual, double *pdNmax,TLayerProperties *pstLayers);

#endif /* NEQ_INTEGRATE_H_INCLUDED */
