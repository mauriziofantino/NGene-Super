/**
 * @file    neqGetNeOnVertRay.c
 *
 * @author  Paul Whitehall - Nottingham Scientific Ltd
 *
 * @date    23/3/2010
 *
 * @version 1.0.0
 *
 * @brief   This module contains the NSL implementation of the NeQuick model's function
 *          which calculates the number of electrons on a vertical ray between two
 *          points.
 *
 *          From the NeQuick model reference code with minor changes.
 */

#include "neqGetNeOnVertRay.h"
#include "neqCalcBottomsideNe.h"
#include "neqCalcTopsideNe.h"


double NeqGetNeOnVertRay(double dH,TLayerProperties *pstLayers, double *pdNmax)
{
	double dReturn; /* Return value, electron content at point */


	/* Check if the current point is above the F2 electron density peak point */
	if(dH > pstLayers->pdPeakHeight[LAYER_F2_SI])
	{
		/* Point is above F2 peak, so call NeqCalcTopsideNe */
		dReturn = NeqCalcTopsideNe(dH, pstLayers, pdNmax);
	}
	else
	{
		/* Point is at or below F2 peak, so call NeqCalcBottomsideNe */
		dReturn = NeqCalcBottomsideNe(dH, pstLayers);
	} /* end if above F2 layer peak */

	/* Return the electron content */
	return (dReturn);
}

