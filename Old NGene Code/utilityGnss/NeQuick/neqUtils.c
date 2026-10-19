/**
 * @file    neqUtils.c
 *
 * @author  Paul Whitehall - Nottingham Scientific Ltd
 *
 * @date    23/3/2010
 *
 * @version 1.0.0
 *
 * @brief   This module contains the NSL implementation of the NeQuick model's utility functions.
 *
 *          From the NeQuick model reference code with minor changes.
 */


#include <math.h>
#include "neqUtils.h"



double NeqClipExp(double dPower)
{
	double dReturn; /* Return value, clipped value of exponential */

	/* Check if dPower is outside the limits and set dReturn accordingly */
	if(dPower > CLIPEXP_CUTOFF_D)
	{
		dReturn = CLIPEXP_HIGH_D;
	}
	else if(dPower < -CLIPEXP_CUTOFF_D)
	{
		dReturn = CLIPEXP_LOW_D;
	}
	else
	{
		dReturn = exp(dPower);
	}

	/* Return the result */
	return (dReturn);
}


double NeqJoin(double dF1, double dF2, double dAlpha, double dX)
{
	double dTemp; /* Intermediate e ^ (dAlpha * dX) term (via NeqClipExp) */

	/* Calculate and return the value */
	dTemp = NeqClipExp(dAlpha * dX);
	return (((dF1 * dTemp) + dF2) / (dTemp + 1.0));
}


double NeqSquared(double dValue)
{
	return (dValue * dValue);
}

