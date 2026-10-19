/**
 * @file    neqCalcBottomsideNe.c
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


#include <math.h>
#include "neqCalcBottomsideNe.h"
#include "nequickDataStructures.h"
#include "neqUtils.h"


double NeqCalcBottomsideNe(double dHH, TLayerProperties *pstLayers)
{
	double dAN0;             /* Anchor Points */
	double dBF;              /* Thickness parameters for bottom of F2, */
	double dSum = 0.0;       /* Sum variable*/
	double dDSum = 0.0;      /* Sum variable*/
	double dArg;             /* temporary argument*/
	double dS0;              /* temporary variable */
	double dDS;              /* temporary variable */
	double pdB[LAYER_COUNT_SI]; /* Thickness parameters for bottom and top */
	int siLayer;             /* Loop variable for Epstein layers */
	double dZ;               /* temporary variable */
	double dEE;              /* temporary variable */
	double dReturn;          /* Return value, electron content at point */

	/* Set dB[LAYER_F2] to F2 bottom side density (as this function is only called
	 * for points beneath the F2 peak) */
	pdB[LAYER_F2_SI] = pstLayers->pdBotThick[LAYER_F2_SI];

	/* Set dB[LAYER_F1] to F1 top/bottom-side density (dependent on current
	 * height) */
	if(dHH > pstLayers->pdPeakHeight[LAYER_F1_SI])
	{
		pdB[LAYER_F1_SI] = pstLayers->pdTopThick[LAYER_F1_SI];
	}
	else
	{
		pdB[LAYER_F1_SI] = pstLayers->pdBotThick[LAYER_F1_SI];
	}

	/* Set dB[LAYER_E] to E top/bottom-side density (dependent on current
	 * height) */
	if(dHH > pstLayers->pdPeakHeight[LAYER_E_SI])
	{
		pdB[LAYER_E_SI] = pstLayers->pdTopThick[LAYER_E_SI];
	}
	else
	{
		pdB[LAYER_E_SI] = pstLayers->pdBotThick[LAYER_E_SI];
	}

	/* Check if we're beneath 100km */
	if(dHH < BOTTOM_CORRN_ANCHOR_D)
	{
		/* We are beneath 100km */
		for(siLayer = LAYER_F2_SI; siLayer < LAYER_COUNT_SI; siLayer = siLayer + 1)
		{
			/* Calculate dArg */
			dArg = (BOTTOM_CORRN_ANCHOR_D - pstLayers->pdPeakHeight[siLayer]) / pdB[siLayer];
			if(siLayer > LAYER_F2_SI)
			{
				dArg = dArg * exp(BOTTOM_CORRN_FAC1_D / ((BOTTOM_CORRN_FAC2_D * fabs(BOTTOM_CORRN_ANCHOR_D - pstLayers->pdPeakHeight[LAYER_F2_SI])) + 1.0));
			} /* end if not F2 layer */

			/* Calculate dS0 and dDS */
			if(fabs(dArg) > ARG_CLIP_THRESH_D)
			{
				dS0 = 0.0;
				dDS = 0.0;
			}
			else
			{
				dEE = exp(dArg);
				dS0 = pstLayers->pdAmp[siLayer] * dEE / NeqSquared(dEE + 1.0);
				dDS = ((1.0 - dEE) / (1.0 + dEE)) / pdB[siLayer];
			} /* end if |dArg| > ARG_CLIP_THRESH_D */

			/* Add dS0 and (dS0 * dDS) to the summations */
			dSum = dSum + dS0;
			dDSum = dDSum + (dS0 * dDS);
		} /* end for each layer */

		dBF = 1.0 - ((dDSum / dSum) * BOTTOM_CORRN_SCALE_D);
		dAN0 = dSum * NE_SCALING_D;

		dZ = (dHH - BOTTOM_CORRN_ANCHOR_D) / BOTTOM_CORRN_SCALE_D;
		dReturn = dAN0 * NeqClipExp(1 - (dBF * dZ) - NeqClipExp(-dZ));
	}
	else
	{
		/* We are above 100km */
		for(siLayer = LAYER_F2_SI; siLayer < LAYER_COUNT_SI; siLayer = siLayer + 1)
		{
			/* Calculate dArg */
			dArg = (dHH - pstLayers->pdPeakHeight[siLayer]) / pdB[siLayer];
			if(siLayer > LAYER_F2_SI)
			{
				dArg = dArg * exp(BOTTOM_CORRN_FAC1_D / ((BOTTOM_CORRN_FAC2_D * fabs(dHH - pstLayers->pdPeakHeight[LAYER_F2_SI])) + 1.0));
			} /* end if not N2 layer */

			/* if |dArg| > ARG_CLIP_THRESH_D */
			if(fabs(dArg) > ARG_CLIP_THRESH_D)
			{
				dS0 = 0.0;
			}
			else
			{
				dEE = exp(dArg);
				dS0 = pstLayers->pdAmp[siLayer] * dEE / NeqSquared(dEE + 1.0);
			} /* end if |dArg| > ARG_CLIP_THRESH_D */

			/* Add dS0 to the summation */
			dSum = dSum + dS0;
		} /* end for each layer */
		dReturn = dSum * NE_SCALING_D;
	} /* end if dHH < H0_D */

	/* Return the electron content */
	return (dReturn);
}

/* End of file: $RCSfile: NeqCalcBottomsideNe.c,v $ */
