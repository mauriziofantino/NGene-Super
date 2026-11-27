/**
 * @file    neqGetNeOnSlantRay.h
 *
 * @author  Paul Whitehall - Nottingham Scientific Ltd
 *
 * @date    23/3/2010
 *
 * @version 1.0.0
 *
 * @brief   This module contains the NSL implementation of the NeQuick model's function
 *          which calculates the number of electrons on a slanted (non-vertical) ray
 *          between two points.
 *
 *          From the NeQuick model reference code with minor changes.
 */
#ifndef NEQ_GET_NE_ON_SLANT_RAY_H_INCLUDED
#define NEQ_GET_NE_ON_SLANT_RAY_H_INCLUDED
#include "nequickDataStructures.h"

/**
 * @brief This function calculates electron content at the specified distance along a
 *        slanted ray.
 *
 *   It does this by calling NeqCalcLLHOnRay to update the current lat/lng/h,
 *   before calling NeqCalcEpstParams to update ionosphere properties at this
 *   new point and then calling NeqCalcTopsideNe or NeqCalcBottomsideNe
 *   (depending on position height relative to the F2 electron density peak
 *   point) to calculate the electron content.
 *
 * @param pstNeQuickInputData Structure containing read-only NeQuick inputs (I)
 * @param pstGeom
 * @param dS Distance of the point along the ray (km) (I)
 * @param ptCurrCCIR In: if already set, Out: if not set before (I/O)
 * @param ptPactual dLat/dLng/dH only updated for current position along slant (O)
 * @param pdNmax Electron content at F2 peak (O)
 * @param ptLayers Current ionospheric properties (see NeqCalcEpstParams) (O)
 *
 *
 * @return Electron content at specified distance along the slanted ray
 */
double NeqGetNeOnSlantRay(TNeQuickInputData *pstNeQuickInputData, TGeometryData *pstGeom, double dS,
                          TCurrentCcir *pstCurrCCIR, TSPoint *pstPactual, double *pdNmax,
                          TLayerProperties *pstLayers);
#endif /* NEQ_GET_NE_ON_SLANT_RAY_H_INCLUDED */
