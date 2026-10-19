/**
 * @file    neqCalcTopsideNe.c
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

#include "neqCalcTopsideNe.h"

#include <math.h>
#include "neqCalcBottomsideNe.h"
#include "nequickDataStructures.h"
#include "nequickConstants.h"
#include "neqUtils.h"



double NeqCalcTopsideNe(double dH, TLayerProperties *pstLayers, double *pdNmax)
{
	double dDeltaH;  /* Height above F2 Peak */
	double dGDeltaH; /* G_D * dDeltaH */
	double dEE;      /* temporary variable */
	double dEP;      /* temporary variable */
	double dReturn;  /* Return value, electron content at point */


	/* Calculate dEE. N.B. The formula quoted in 'Radicella & Leitinger' 2001 is
	 * incorrect, i.e. ... ((1.0 + R_FAC_D * dGDeltaH) / ...) */
	dDeltaH = dH - pstLayers->pdPeakHeight[LAYER_F2_SI];
	dGDeltaH = TOPSIDE_CORRN1_D * dDeltaH;
	dEE =  NeqClipExp(dDeltaH / (pstLayers->pdTopThick[LAYER_F2_SI] * (1.0 + ((TOPSIDE_CORRN2_D * dGDeltaH) / ((TOPSIDE_CORRN2_D * pstLayers->pdTopThick[LAYER_F2_SI]) + dGDeltaH)))));

	/* Check if dEE is large */
	if(dEE > LARGE_EE_D)
	{
		/* dEE is large so can approximate the formula for ep
		 * from         4.dEE       to    4
		 *         ---------------      -----
		 *          (dEE + 1) ^ 2        dEE  */
		dEP = EPSTNORMALISATION_D / dEE;
	}
	else
	{
		/* dEE is not large to allow approximation so use full formula */
		dEP = (EPSTNORMALISATION_D * dEE) / NeqSquared(dEE + 1.0);
	}

	/* fortran doesn't do this [see 'NeqGetNeOnSlantRay' C function] */
	if(fabs(*pdNmax + 1.0) < UNSET_NMAX_TOL_D)
	{
		*pdNmax = NeqCalcBottomsideNe(pstLayers->pdPeakHeight[LAYER_F2_SI], pstLayers);
	}

	/* Calculate the electron content */
	dReturn = *pdNmax * dEP;

	/* Return the electron content */
	return (dReturn);
}

