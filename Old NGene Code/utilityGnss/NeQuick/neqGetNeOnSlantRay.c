/**
 * @file    neqGetNeOnSlantRay.c
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

#include <math.h>
#include "neqGetNeOnSlantRay.h"
#include "neqCalcBottomsideNe.h"
#include "neqCalcEpstParams.h"
#include "neqCalcTopsideNe.h"
#include "nequickConstants.h"


/* Function prototypes */
static void NeqCalcLLHOnRay(double dS, TSPoint *pstRay, TSPoint *pstP1,double dSinSig, double dCosSig, TSPoint *pstPactual);


double NeqGetNeOnSlantRay(TNeQuickInputData *pstNeQuickInputData,TGeometryData *pstGeom, double dS,TCurrentCcir *pstCurrCCIR, TSPoint *pstPactual, double *pdNmax, TLayerProperties *pstLayers)
{
	double dReturn; /* Return value, electron content at point */

	/* Calculate the current position (new lat/lng/h) */
	NeqCalcLLHOnRay(dS, &(pstGeom->stRay), &(pstGeom->stP1), pstGeom->dSinSig, pstGeom->dCosSig, pstPactual);

	/* Call NeqCalcEpstParams to calculate the ionosphere properties at new pt */
	NeqCalcEpstParams(pstNeQuickInputData, *pstPactual, pstGeom->dSinDelta, pstGeom->dCosDelta, pstCurrCCIR, pdNmax, pstLayers);

	if(pstPactual->dH > pstLayers->pdPeakHeight[LAYER_F2_SI])
	{
		/* Point is above F2 peak, so call NeqCalcTopsideNe */
		dReturn = NeqCalcTopsideNe(pstPactual->dH, pstLayers, pdNmax);
	}
	else
	{
		/* Point is at or below F2 peak, so call NeqCalcBottomsideNe */
		dReturn = NeqCalcBottomsideNe(pstPactual->dH, pstLayers);
	} /* end if above F2 layer peak */

	/* Return the electron content */
	return (dReturn);
}


/**
 * @brief  This function adjust tPactual to be the current position (lat/lng/h)
 *         corresponding to slant position at a specified distance (s) along the
 *         slanted ray.
 *
 * @param dS        Distance of the point along the ray (km) (I)
 * @param pstRay    dR and dLng only (I)
 * @param pstP1     dSinLat and dCosLat only (I)
 * @param dSinSig   (I)
 * @param dCosSig   (I)
 * @param ptPactual dLat/dLng/dH updated for current position along slant (O)
 */
static void NeqCalcLLHOnRay(double dS, TSPoint *pstRay, TSPoint *pstP1,double dSinSig, double dCosSig, TSPoint *pstPactual)
{
	double dCLong;  /* temporary variable */
	double dTanDel; /* temporary variable */
	double dCosDel; /* temporary variable */
	double dSinDel; /* temporary variable */
	double dArg;    /* temporary variable */

	/* Calculate schtuff */
	dTanDel = dS / pstRay->dR;
	dCosDel = 1.0 / sqrt(1.0 + (dTanDel * dTanDel));
	dSinDel = dTanDel * dCosDel;

	dArg = (pstP1->dSinLat * dCosDel) + (pstP1->dCosLat * dSinDel * dCosSig);

	pstPactual->dLat = atan2(dArg, sqrt(1.0 - (dArg * dArg))) * RADTODEG_D;

	dCLong = atan2(dSinDel * dSinSig * pstP1->dCosLat, dCosDel - (pstP1->dSinLat * dArg)) * RADTODEG_D;
	pstPactual->dLng = dCLong + pstRay->dLng;
	pstPactual->dH = sqrt((dS * dS) + (pstRay->dR * pstRay->dR)) - EARTH_RADIUS_D;
}


