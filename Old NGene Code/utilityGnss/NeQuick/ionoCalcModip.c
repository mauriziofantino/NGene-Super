/**
 * @file    ionoCalcModip.c
 *
 * @author  Tim May - Nottingham Scientific Ltd
 *
 * @date    18/5/2010
 *
 * @version 1.0.0
 *
 * @brief   The purpose of this module is to calculate the modified dip latitude for a
 *   		given latitude and longitude.
 *
 *			From the NeQuick model reference code with minor changes.
 */

#include "ionoCalcModip.h"

#include <math.h>
#include "nequickConstants.h"
#include "nequickDataStructures.h"

static TBoolean ModipDegrees(TModip *pstModipFile, double dLat, double dLon, double *pdModip);
static double Interpolate(double pdZ[NUM_INT_PTS_SI], double dDeltaX);


TBoolean IonoCalcModip(TModip *pstModipFile, double dLatRads, double dLongRads, double *pdModip)
{
	double dLatDegs;   /* Latitude in degrees */
	double dLongDegs;  /* Longitude in degrees */

	/* Convert latitude and logitude from radians to degrees */
	/* and ensure range is between -180 and 180 degrees */
	dLatDegs = dLatRads * RADTODEG_D;
	dLongDegs = dLongRads * RADTODEG_D;
	if(dLongDegs >= HALFCIRCLEDEG_D)
	{
		dLongDegs = dLongDegs - CIRCLEDEG_D;
	}

	return (ModipDegrees(pstModipFile, dLatDegs, dLongDegs, pdModip));
}


/**
 * @brief This function uses the current latitude and longitude to calculate the
 *   	  corresponding modified dip latitude (MODIP) value.
 *
 * @param pstModipFile Modip grid [in]
 * @param dLat Latitude in degrees in range between -90 and +90 [in]
 * @param dLon Longitude in degrees in range between -180 and +180 [in]
 * @param pdModip interpolated MODIP value in degrees [out]
 *
 * @return the success status of the function (FALSE/TRUE)
 */
static TBoolean ModipDegrees(TModip *pstModipFile, double dLat, double dLon, double *pdModip)
{
	/*unsigned char uszInfo[EVENT_ADD_INF_SIZE_UI+1]; */
	/* Additional event information*/
	int siLonBaseInd;               /* Base Longitude as index */
	int siLatBaseInd;               /* Base Latitude as index */
	int siLonInterInd;              /* Interpolated Longitude as index */
	int siLatInterInd;              /* Interpolated Latitude as index */
	double dDeltaLon;               /* Delta-longitude in degress*/
	double dDeltaLat;               /* Delta-latitude in degress*/
	double pdDipN[NUM_INT_PTS_SI];  /* Point values for longitude */
	/* interpolation */
	double pdDipE[NUM_INT_PTS_SI];  /* Point values for latitude */
	/* interpolation */
	TBoolean bSuccess = TRUE;      /* Error flag for function*/

	/* Check whether latitude and longitude are in valid range */
	if((dLat < RANGE_LATDEG_MIN_D) || (dLat > RANGE_LATDEG_MAX_D) || (dLon < RANGE_LONDEG_MIN_D) || (dLon > RANGE_LONDEG_MAX_D))
	{
		/* Set position to a default value */
		dLon = 0.0;
		dLat = 0.0;
		bSuccess = FALSE;
		*pdModip = 0.0;
	}
	else
	{
		/* Calculate delta-longitude and indices into the MoDip grid */
		dDeltaLon = (dLon + RANGE_LONDEG_MAX_D) / (double)LON_STEP_SI;
		siLonBaseInd = (int)floor(dDeltaLon);
		dDeltaLon = dDeltaLon - (double)siLonBaseInd;

		/* Check that siLonBaseInd is within the limits of 0 to 35 */
		if(siLonBaseInd < 0)
		{
			siLonBaseInd = siLonBaseInd + NUM_LONS_SI;
		}
		else if(siLonBaseInd >= NUM_LONS_SI)
		{
			siLonBaseInd = siLonBaseInd - NUM_LONS_SI;
		}

		/* Obtain latitude grid square and position in that square */
		/* The original calculation for siLatBaseInd (which subtracted 1e-6 from */
		/* dDeltaLat before truncation) has been modified as this subtraction */
		/* serves no useful purpose */
		dDeltaLat = ((dLat + RANGE_LATDEG_MAX_D) / (double)LAT_STEP_SI);
		siLatBaseInd = (int)floor(dDeltaLat);
		dDeltaLat = (dDeltaLat - (double)siLatBaseInd);

		/* Perform two-dimensional interpolation */
		for(siLonInterInd = 0; siLonInterInd < NUM_INT_PTS_SI; siLonInterInd = siLonInterInd + 1)
		{
			/* Interpolate the PDip values against latitude */
			for(siLatInterInd = 0; siLatInterInd < NUM_INT_PTS_SI;	siLatInterInd = siLatInterInd + 1)
			{
				pdDipN[siLatInterInd] = pstModipFile->pdModip[siLatBaseInd + siLatInterInd][siLonBaseInd + siLonInterInd];
			}
			/* Set the longitude point values by interpolating latitude values */
			pdDipE[siLonInterInd] = Interpolate(pdDipN, dDeltaLat);
		} /* End of perform two-dimensional interpolation */

		/* Interpolate the PDip values against longitude and return it */
		*pdModip = Interpolate(pdDipE, dDeltaLon);
	} /* End of check whether latitude and longitude are in valid range */

	return (bSuccess);
}


/**
 * @brief This function performs a third order interpolation. It is used when
 *   	  calculating the modified dip latitude (MODIP) value.
 *
 * @param pdZ -1, 0, 1, 2 point values [in]
 * @param dDeltaX Position to interpolate to (offset from 0pt to 1pt) [in]
 *
 * @return the interpolated value at the point
 */
static double Interpolate(double pdZ[NUM_INT_PTS_SI], double dDeltaX)
{
	double dIntZ = 0.0; /* Interpolated value */
	double pdG[NUM_INT_PTS_SI]; /* Variable used for interpolation */
	double pdA[NUM_INT_PTS_SI]; /* Variable used for interpolation */
	int siPt;  /* Loop variable for points */

	/* Check whether delta X is large enough */
	if(fabs(TWO_D * dDeltaX) > MIN_DELTAX_D)
	{
		/* Interpolate */
		pdG[INT_PT0_SI] = pdZ[INT_PT2_SI] + pdZ[INT_PT1_SI];
		pdG[INT_PT1_SI] = pdZ[INT_PT2_SI] - pdZ[INT_PT1_SI];
		pdG[INT_PT2_SI] = pdZ[INT_PT3_SI] + pdZ[INT_PT0_SI];
		pdG[INT_PT3_SI] = (pdZ[INT_PT3_SI] - pdZ[INT_PT0_SI]) / THREE_D;

		pdA[INT_PT0_SI] = (NINE_D * pdG[INT_PT0_SI]) - pdG[INT_PT2_SI];
		pdA[INT_PT1_SI] = (NINE_D * pdG[INT_PT1_SI]) - pdG[INT_PT3_SI];
		pdA[INT_PT2_SI] = pdG[INT_PT2_SI] - pdG[INT_PT0_SI];
		pdA[INT_PT3_SI] = pdG[INT_PT3_SI] - pdG[INT_PT1_SI];

		dDeltaX = (TWO_D * dDeltaX) - 1.0;

		/* Calculate dIntZ = 1 / 16(dA[3]dDeltaX^3 + */
		/*                          dA[2]dDeltaX^2 + */
		/*                          dA[1]dDeltaX + */
		/*                          dA[0]) */
		for(siPt = INT_PT3_SI; siPt >= INT_PT0_SI; siPt = siPt - 1)
		{
			dIntZ = (dIntZ * dDeltaX) + pdA[siPt];
		}
		dIntZ = dIntZ / SIXTEEN_D;

	}
	else
	{
		/* Delta X is not large enough */
		/* Set the interpolated value to the zero point */
		dIntZ = pdZ[1];

	} /* End of check whether delta X is large enough */

	/* Return the interpolated value */
	return (dIntZ);
} 
