/**
 * @file    neqCalcEpstParams.c
 *
 * @author  Paul Whitehall - Nottingham Scientific Ltd
 *
 * @date    23/3/2010
 *
 * @version 1.0.0
 *
 * @brief   This module contains the NSL implementation of the NeQuick model's function
 *          for calculating the properties of the Epstein layers.
 *
 *          From the NeQuick model reference code with minor changes.
 */

#include <math.h>
#include "neqCalcEpstParams.h"
#include "nequickConstants.h"
#include "neqUtils.h"

/* Function prototypes */
static double NeqCalcModip(double dLat, double dLng, TModip *pstModip);
static double NeqInterpolate(const double pdZ[NUM_INTER_PTS_SI],const double dDeltaX);
static double NeqModipToAz(double dModip, int siNumCoeff, double *pdCoeff);
static void NeqCalcSphLegCoeffs(double dUT, TCurrentCcir *pstCurrCCIR);
static double NeqGetF2FreqFromCCIR(double dCosLat, double dLng, double pdLegCoeffs[MAX_CCIRROWS_SI],double pdSinModipToN[MAX_SINMODIP_TERM_SI], int siMode);
static double NeqCriticalFreqToNe(double dF0);
static double NeqCalcF2PeakHeight(double dM3000, double dF0E, double dF0F2);
static double NeqEpstein(double dNmax, double dHmax, double dB, double dH);



void NeqCalcEpstParams(TNeQuickInputData *pstNeQuickInputData,TSPoint stPactual, double dSinDelta, double dCosDelta,TCurrentCcir *pstCurrCCIR, double *pdNmax,TLayerProperties *pstLayers)
{
	double dModip;     /* Current modified dip lat */
	double dFlx;       /* 10.7cm solar radio flux */
	double dR12Num;    /* Average sunspot number */
	double dHiRatio;   /* Ratio for extrapolation of hi/lo CCIR map data */
	double dLoRatio;   /* Ratio for extrapolation of hi/lo CCIR map data */
	int siIndex1;      /* Loop variable */
	int siIndex2;      /* Loop variable */
	double dLocalTime; /* Local time */
	double dCosChi;    /* Cos of solar zenith angle */
	double dChi;       /* Solar zenith angle (deg) */
	int psiSeas[MONTHS_SI] = { -1, -1, 0, 0, 1, 1, 1, 1, 0, 0, -1, -1 };
	/* Array for calculating season factor for each month */
	double dSeas;      /* Season factor */
	double dEE;        /* Exponential temporary variable */
	double dChin;      /* temporary variable */
	double dSFac;      /* temporary variable */
	double dFa;        /* temporary variable */
	double pdNm[LAYER_COUNT_SI];
	/* Peak electron density for Epstein layers */
	double dNdHmx;     /* temporary variable */
	double dB2K;       /* temporary variable */
	double dX;         /* temporary variable */
	double dV;         /* temporary variable */
	double pdSinModipToN[MAX_SINMODIP_TERM_SI];
	/* Array for storage of sin(dModip) ^ N */
	double dCosLat;    /* cos(current lat) */


	/* Call NeqCalcModip to calculate the Modip value for the current position */
	dModip = NeqCalcModip(stPactual.dLat, stPactual.dLng, pstNeQuickInputData->pstModip);

	/* Modulate using Az (a0,a1,a2...) coefficients */
	dFlx = NeqModipToAz(dModip, pstNeQuickInputData->siNumCoeff, pstNeQuickInputData->pdCoeff);

	/* Calculate dR12Num sunspot number from solar flux (flx) */
	dR12Num = sqrt(((dFlx - AZTOSSN_1_D) * AZTOSSN_2_D) + AZTOSSN_3_D) - AZTOSSN_4_D;

	/* Initialise dNmax to 'not set' to force NeqCalcTopsideNe to call
	 * NeqCalcBottomsideNe */
	*pdNmax = -1.0;

	/* Check if the month or solar flux have changed */
	if((fabs(dR12Num - pstCurrCCIR->dR12) > EQUAL_R12_THRES_D) || (pstNeQuickInputData->siMonth != pstCurrCCIR->siMonth))
	{
		/* Calculate the ratio of R12 factor between lo and hi solar activity.
		 * These values are used for linear extrapolation of the current activity.
		 * If dR12Num == 100.0 then the extrapolation gives pdF0F2[1] and
		 * if dR12Num == 0.0 it gives pdF0F2[0] */
		dHiRatio = dR12Num / HI_R12_D;
		dLoRatio = 1.0 - dHiRatio;

		/* Now extrapolate between the hi and lo solar activity CCIR maps to
		 * calculate a current set to be used for F0F2 and M(3000)F2. */
		for(siIndex1 = 0; siIndex1 < CCIR_F2_DIM_1_SI; siIndex1 = siIndex1 + 1)
		{
			for(siIndex2 = 0; siIndex2 < CCIR_F2_DIM_2_SI; siIndex2 = siIndex2 + 1)
			{
				pstCurrCCIR->pdF0F2[(siIndex2 * CCIR_F2_DIM_1_SI) + siIndex1] =
						(pstNeQuickInputData->pstCCIR->pdF2[pstNeQuickInputData->siMonth - 1][siIndex1][siIndex2][0] * dLoRatio) +
						(pstNeQuickInputData->pstCCIR->pdF2[pstNeQuickInputData->siMonth - 1][siIndex1][siIndex2][1] * dHiRatio);
			} /* end for each siIndex1 */
		} /* end for each siIndex2 */

		for(siIndex1 = 0; siIndex1 < CCIR_M3000_DIM_1_SI; siIndex1 = siIndex1 + 1)
		{
			for(siIndex2 = 0; siIndex2 < CCIR_M3000_DIM_2_SI; siIndex2 = siIndex2 + 1)
			{
				pstCurrCCIR->pdM3000F2[(siIndex2 * CCIR_M3000_DIM_1_SI) + siIndex1] =
						(pstNeQuickInputData->pstCCIR->pdM3000[pstNeQuickInputData->siMonth - 1][siIndex1][siIndex2][0] * dLoRatio) +
						(pstNeQuickInputData->pstCCIR->pdM3000[pstNeQuickInputData->siMonth - 1][siIndex1][siIndex2][1] * dHiRatio);
			} /* end for each siIndex1 */
		} /* end for each siIndex2 */

		/* Remember which month / R12 sunspot number we've just set values for */
		pstCurrCCIR->dR12 = dR12Num;
		pstCurrCCIR->siMonth = pstNeQuickInputData->siMonth;

		/* Calculate the Spherical Legendre coefficients for this R12 */
		NeqCalcSphLegCoeffs(pstNeQuickInputData->dUT, pstCurrCCIR);
	}
	else if(fabs(pstCurrCCIR->dUT - pstNeQuickInputData->dUT) > EQUAL_TIME_D)
	{
		/* Recalculate the Spherical Legendre coeffs for this universal time */
		NeqCalcSphLegCoeffs(pstNeQuickInputData->dUT, pstCurrCCIR);
	} /* end if dR12 or siMonth changed */

	/* Set up the sin^N(Modip) array */
	pdSinModipToN[0] = 1.0;
	pdSinModipToN[1] = sin(dModip * DEG_TO_RAD_D);
	for(siIndex1 = TWO_SI; siIndex1 < MAX_SINMODIP_TERM_SI; siIndex1 = siIndex1 + 1)
	{
		/* Calculate the next value in the array */
		pdSinModipToN[siIndex1] = pdSinModipToN[siIndex1 - 1] * pdSinModipToN[1];

		/* Check if it is negligible */
		if(fabs(pdSinModipToN[siIndex1]) < SINMODIP_CUTOFF_D)
		{
			/* Term is negligible, set it to zero */
			pdSinModipToN[siIndex1] = 0.0;
		} /* end if term is negligible */
	} /* end for each sin^N(Modip) term */

	/* Calculate the current F0F2 and M(3000)F2 */
	dCosLat = cos(stPactual.dLat * DEG_TO_RAD_D);
	pstLayers->pdF0[LAYER_F2_SI] =  NeqGetF2FreqFromCCIR(dCosLat, stPactual.dLng, pstCurrCCIR->pdLegCoeffs_F0, pdSinModipToN, CCIR_MODE_F0_SI);
	pstLayers->dM3000 = NeqGetF2FreqFromCCIR(dCosLat, stPactual.dLng, pstCurrCCIR->pdLegCoeffs_M3000, pdSinModipToN, CCIR_MODE_M3000_SI);

	/* Limit dM3000 to be at least 1.0 to prevent possible sqrt(-ve) in
	 * NeqCalcF2PeakHeight */
	if(pstLayers->dM3000 < 1.0)
	{
		pstLayers->dM3000 = 1.0;
	} /* end if dM3000 < 1 */

	/* Calculate localtime and ensure it's between 0 and 24 */
	dLocalTime = pstNeQuickInputData->dUT + (stPactual.dLng / DEGPERHOUR_D);
	if(dLocalTime < 0.0)
	{
		dLocalTime = dLocalTime + DAYHOURS_D;
	}
	else if(dLocalTime >= DAYHOURS_D)
	{
		dLocalTime = dLocalTime - DAYHOURS_D;
	} /* end if dLocalTime < 0.0 */

	/* Calculate CosChi and Chi */
	dCosChi = (sin(stPactual.dLat * DEG_TO_RAD_D) * dSinDelta) +
			(cos(stPactual.dLat * DEG_TO_RAD_D) * dCosDelta *  cos((PI_D * (HALFDAYHOURS_D - dLocalTime)) / HALFDAYHOURS_D));

	dChi = atan2(sqrt(1.0 - dCosChi * dCosChi), dCosChi) * RADTODEG_D;


	/* Set the seasonal flag */
	dSeas = (double)psiSeas[pstNeQuickInputData->siMonth - 1];


	dEE = NeqClipExp(F0E_CONST1_D * stPactual.dLat);
	dSeas = ((dSeas * (dEE - 1.0)) / (dEE + 1.0));
	dChin = NeqJoin(F0E_CONST2_D - (F0E_CONST3_D * NeqClipExp(F0E_CONST4_D - (F0E_CONST5_D * dChi))), dChi, F0E_CONST6_D, dChi - CHI0_D);
	dSFac = (F0E_CONST7_D - (F0E_CONST8_D * dSeas)) * sqrt(sqrt(dFlx));
	dFa = dSFac * NeqClipExp(log(cos(dChin * DEG_TO_RAD_D)) * F0E_CONST9_D);

	/* Calculate the E-peak plasma frequency */
	pstLayers->pdF0[LAYER_E_SI] = sqrt((dFa * dFa) + F0E_CONST10_D);

	/* Calculate the F1-peak plasma frequency and set to zero if negligible */
	pstLayers->pdF0[LAYER_F1_SI] = NeqJoin(TITHERIDGE_CONST_D * pstLayers->pdF0[LAYER_E_SI], 0.0, F0F1_JOINFAC1_D, pstLayers->pdF0[LAYER_E_SI] - TWO_D );
	pstLayers->pdF0[LAYER_F1_SI] = NeqJoin(0.0, pstLayers->pdF0[LAYER_F1_SI], F0F1_JOINFAC1_D, pstLayers->pdF0[LAYER_E_SI] - pstLayers->pdF0[LAYER_F1_SI]);
	pstLayers->pdF0[LAYER_F1_SI] = NeqJoin(pstLayers->pdF0[LAYER_F1_SI], F0F1_CONST1_D * pstLayers->pdF0[LAYER_F1_SI], F0F1_JOINFAC2_D, (F0F1_CONST1_D * pstLayers->pdF0[LAYER_F2_SI]) - pstLayers->pdF0[LAYER_F1_SI]);
	if(pstLayers->pdF0[LAYER_F1_SI] < SMALL_F0F1_D)
	{
		pstLayers->pdF0[LAYER_F1_SI] = 0.0;
	} /* end if dF0F1 is negligible */

	/* Calculate peak electron densities for the F2, F1 and E layers */
	pdNm[LAYER_F2_SI] = NeqCriticalFreqToNe(pstLayers->pdF0[LAYER_F2_SI]);
	pdNm[LAYER_F1_SI] = NeqCriticalFreqToNe(pstLayers->pdF0[LAYER_F1_SI]);
	pdNm[LAYER_E_SI] = NeqCriticalFreqToNe(pstLayers->pdF0[LAYER_E_SI]);

	/* Calculate the height of the electron density peaks for F2, E and F1 layers.
	 * nb. F2 peak is calculated,  E layer peak is fixed at 120km, and the F1 peak
	 *     is set half-way between them. */
	pstLayers->pdPeakHeight[LAYER_F2_SI] =  NeqCalcF2PeakHeight(pstLayers->dM3000, pstLayers->pdF0[LAYER_E_SI], pstLayers->pdF0[LAYER_F2_SI]);
	pstLayers->pdPeakHeight[LAYER_E_SI] = EPEAKHEIGHT_D;
	pstLayers->pdPeakHeight[LAYER_F1_SI] = (pstLayers->pdPeakHeight[LAYER_F2_SI] + pstLayers->pdPeakHeight[LAYER_E_SI]) / TWO_D;

	/* Calculate Gradient of Ne at base of F2 layer (10^9 m^-3 km^-1)
	 * re: Mosert de Gonzalez and Radicella, 1990, Adv. Space. Res., 10, 17 */
	dNdHmx = DENSE_GRAD1_D * exp(DENSE_GRAD2_D + (DENSE_GRAD3_D * log(pstLayers->pdF0[LAYER_F2_SI] * pstLayers->pdF0[LAYER_F2_SI])) + (DENSE_GRAD4_D * log(pstLayers->dM3000)));

	/* Calculate Bottom-side thickness
	 * re: Di Giovanni and Radicella, 1990, Adv. Space. Res., 10, (11)27-(11)30 */
	pstLayers->pdBotThick[LAYER_F2_SI] = (THICK_SCALING_F2B_D * pdNm[LAYER_F2_SI]) / dNdHmx;
	pstLayers->pdTopThick[LAYER_F1_SI] = THICK_SCALING_F1T_D * (pstLayers->pdPeakHeight[LAYER_F2_SI] - pstLayers->pdPeakHeight[LAYER_F1_SI]);
	pstLayers->pdBotThick[LAYER_F1_SI] = THICK_SCALING_F1B_D * (pstLayers->pdPeakHeight[LAYER_F1_SI] - pstLayers->pdPeakHeight[LAYER_E_SI]);
	pstLayers->pdTopThick[LAYER_E_SI] = pstLayers->pdBotThick[LAYER_F1_SI];
	if(pstLayers->pdTopThick[LAYER_E_SI] < MIN_ET_THICK_D)
	{
		pstLayers->pdTopThick[LAYER_E_SI] = MIN_ET_THICK_D;
	}
	pstLayers->pdBotThick[LAYER_E_SI] = EB_THICK_D;

	pstLayers->pdAmp[LAYER_F2_SI] = EPSTNORMALISATION_D * pdNm[LAYER_F2_SI];

	/* This value is overwritten before being used anyway, so could be removed */
	pstLayers->pdAmp[LAYER_F1_SI] = EPSTNORMALISATION_D * pdNm[LAYER_F1_SI];

	/* This value is overwritten before being used except for the else...for...
     construct below, so it could be moved to between the else and the for */
	pstLayers->pdAmp[LAYER_E_SI] = EPSTNORMALISATION_D * pdNm[LAYER_E_SI];

	if(pstLayers->pdF0[LAYER_F1_SI] < F0F1_THRESHOLD_D)
	{
		pstLayers->pdAmp[LAYER_F1_SI] = 0.0;
		pstLayers->pdAmp[LAYER_E_SI] = EPSTNORMALISATION_D * (pdNm[LAYER_E_SI] - NeqEpstein(pstLayers->pdAmp[LAYER_F2_SI], pstLayers->pdPeakHeight[LAYER_F2_SI], pstLayers->pdBotThick[LAYER_F2_SI], pstLayers->pdPeakHeight[LAYER_E_SI]));
	}
	else
	{
		for(siIndex1 = 0; siIndex1 < NUM_AMP_ITER_SI; siIndex1 = siIndex1 + 1)
		{
			pstLayers->pdAmp[LAYER_F1_SI] = EPSTNORMALISATION_D * (pdNm[LAYER_F1_SI] - NeqEpstein(pstLayers->pdAmp[LAYER_F2_SI], pstLayers->pdPeakHeight[LAYER_F2_SI], pstLayers->pdBotThick[LAYER_F2_SI], pstLayers->pdPeakHeight[LAYER_F1_SI]) - NeqEpstein(pstLayers->pdAmp[LAYER_E_SI], pstLayers->pdPeakHeight[LAYER_E_SI], pstLayers->pdTopThick[LAYER_E_SI], pstLayers->pdPeakHeight[LAYER_F1_SI]));
			pstLayers->pdAmp[LAYER_F1_SI] = NeqJoin(pstLayers->pdAmp[LAYER_F1_SI], F1_AMP_CONST_D * pdNm[LAYER_F1_SI],1.0, pstLayers->pdAmp[LAYER_F1_SI] - (F1_AMP_CONST_D * pdNm[LAYER_F1_SI]));
			pstLayers->pdAmp[LAYER_E_SI] = EPSTNORMALISATION_D * (pdNm[LAYER_E_SI] - NeqEpstein(pstLayers->pdAmp[LAYER_F1_SI], pstLayers->pdPeakHeight[LAYER_F1_SI], pstLayers->pdBotThick[LAYER_F1_SI], pstLayers->pdPeakHeight[LAYER_E_SI]) - NeqEpstein(pstLayers->pdAmp[LAYER_F2_SI], pstLayers->pdPeakHeight[LAYER_F2_SI], pstLayers->pdBotThick[LAYER_F2_SI], pstLayers->pdPeakHeight[LAYER_E_SI]));
		}
	}
	pstLayers->pdAmp[LAYER_E_SI] = NeqJoin(pstLayers->pdAmp[LAYER_E_SI], E_AMP_CONST1_D, E_AMP_JOINFAC_D, pstLayers->pdAmp[LAYER_E_SI] - E_AMP_CONST2_D);

	/* Shape factor for topside F2 region */
	if((pstNeQuickInputData->siMonth > MARCH_SI) && (pstNeQuickInputData->siMonth < OCTOBER_SI))
	{
		/* April to September */
		dB2K = SHAPE_FACTOR_1_D - (SHAPE_FACTOR_2_D * dR12Num) - (SHAPE_FACTOR_3_D * pstLayers->pdPeakHeight[LAYER_F2_SI]);
	}
	else
	{
		/* October to March */
		dB2K = SHAPE_FACTOR_4_D + (SHAPE_FACTOR_5_D * NeqSquared(pstLayers->pdPeakHeight[LAYER_F2_SI] /	pstLayers->pdBotThick[LAYER_F2_SI])) + (SHAPE_FACTOR_6_D * pdNm[LAYER_F2_SI]);
	}
	dB2K = NeqJoin(dB2K, SHAPE_FACTOR_7_D, 1.0, dB2K - SHAPE_FACTOR_7_D);
	dB2K = NeqJoin(SHAPE_FACTOR_8_D, dB2K, 1.0, dB2K - SHAPE_FACTOR_8_D);

	/* Adjust the VTEC to take into account exosphere electron density */
	pstLayers->pdTopThick[LAYER_F2_SI] = dB2K * pstLayers->pdBotThick[LAYER_F2_SI];
	dX = (pstLayers->pdTopThick[LAYER_F2_SI] - TOP_CORRN_CONST1_D) / TOP_CORRN_CONST2_D;
	dV = (((TOP_CORRN_CONST3_D * dX) - TOP_CORRN_CONST4_D) * dX) + TOP_CORRN_CONST5_D;
	pstLayers->pdTopThick[LAYER_F2_SI] = pstLayers->pdTopThick[LAYER_F2_SI] / dV;
}


/**
 * @brief This function uses the current lat/lng to calculate the corresponding
 *        modified dip latitude (MODIP) value.
 *
 * @param dLat     The current latitude (deg) (I)
 * @param dLng     The current longitude (deg) (I)
 * @param pstModip Modip grid from config (I)
 *
 * @return Interpolated Modip for position lat/lng (deg)
 */
static double NeqCalcModip(double dLat, double dLng, TModip *pstModip)
{
	int    siLatBaseInd;  /* Lat base index (ie. -1pt index in pdPDip) */
	int    siLngBaseInd;  /* Lng base index (ie. -1pt index in pdPDip) */
	int    siLatInterInd; /* Loop variable for setting lat -1,0,1,2 pts */
	int    siLngInterInd; /* Loop variable for setting lng -1,0,1,2 pts */
	double dDeltaLat;     /* Interpolation offset from latitude 0 pt */
	double dDeltaLng;     /* Interpolation offset from longitude 0 pt */
	double pdDipN[NUM_INTER_PTS_SI];
	/* Point values for longitude interpolation */
	double pdDipE[NUM_INTER_PTS_SI];
	/* Point values for latitude interpolation */
	double dReturn;       /* Interpolated Modip value at lat/lng */


	/* Ensure latitude is within -90 -> +90 range.
	 * This will prevent an array out of bounds error for values outside range. */
	if(dLat <= -RANGE_LATDEG_MAX_D)
	{
		/* Latitude is less than -90, so return Modip of -90 */
		dReturn = -RANGE_LATDEG_MAX_D;
	}
	else if(dLat >= RANGE_LATDEG_MAX_D)
	{
		/* Latitude is greater than +90, so return Modip of +90 */
		dReturn = RANGE_LATDEG_MAX_D;
	}
	else
	{
		/* Calculate dDeltaLng and indices into the Modip grid */
		dDeltaLng = (dLng + RANGE_LONDEG_MAX_D) / (double)LON_STEP_SI;
		siLngBaseInd = (int)floor(dDeltaLng);
		dDeltaLng = dDeltaLng - (double)siLngBaseInd;

		/* Ensure siLngBaseInd is within limits 0-35.
		 * This will not be needed if dLng is within the range +-180. */
		while(siLngBaseInd < 0)
		{
			siLngBaseInd = siLngBaseInd + NUM_LNGS_SI;
		}
		while(siLngBaseInd > NUM_LNGS_SI - 1)
		{
			siLngBaseInd = siLngBaseInd - NUM_LNGS_SI;
		}

		/* Obtain lat grid square and position in that square.
		 * The original calculation for siLatBaseInd (which subtracted 1e-6 from
		 * dDeltaLat before truncation) has been modified as this subtraction serves
		 * no useful purpose (except for potentially fixing the north pole array
		 * index out of range bug, whilst increasing the risk of a south pole
		 * problem. */
		dDeltaLat = (dLat + RANGE_LATDEG_MAX_D) / (double)LAT_STEP_SI;
		siLatBaseInd = (int)floor(dDeltaLat);
		dDeltaLat = (dDeltaLat - (double)siLatBaseInd);

		/* Now perform 2D interpolation */
		for(siLngInterInd = 0; siLngInterInd < NUM_INTER_PTS_SI; siLngInterInd = siLngInterInd + 1)
		{
			/* Interpolate the PDip values against latitude */
			for(siLatInterInd = 0; siLatInterInd < NUM_INTER_PTS_SI;siLatInterInd = siLatInterInd + 1)
			{
				pdDipN[siLatInterInd] = pstModip->pdModip[siLatBaseInd + siLatInterInd][siLngBaseInd + siLngInterInd];
			}

			/* Set the longitude point values by interpolating latitude values */
			pdDipE[siLngInterInd] = NeqInterpolate(pdDipN, dDeltaLat);
		}

		/* Now interpolate the PDip values against longitude and return it */
		dReturn = NeqInterpolate(pdDipE, dDeltaLng);
	} /* end if dLat out of range */

	return (dReturn);
}


/**
 * @brief This function performs a third order interpolation. It is used when
 *         calculating the modified dip latitude (MODIP) value.
 *
 * @param dZ      Array containing the -1, 0, 1 and 2 point values (I)
 * @param dDeltaX Position to interpolate to (offset from 0pt to 1pt) (I)
 *
 * @return The interpolated value at the point
 */
static double NeqInterpolate(const double pdZ[NUM_INTER_PTS_SI], const double dDeltaX)
{
	double dIntZ;                 /* Return value for this function */
	double dDX;                   /* Working variable used in interpolation */
	double dSumPt0Pt1;            /* Sum of point 0 and 1 */
	double dGradPt0Pt1;           /* Gradient between point 0 and 1 */
	double dSumPtM1Pt2;           /* Sum of point -1 and 2 */
	double dGradPtM1Pt2;          /* Gradient between point -1 and 2 */
	double pdA[NUM_INTER_PTS_SI]; /* Working variable used in interpolation */
	int    siIndex;               /* Loop variable */


	/* Check if dDeltaX is small */
	if(fabs(dDeltaX) < INTER_THRESH_D)
	{
		/* dDeltaX is small, so return the zero point */
		dIntZ = pdZ[INT_PT1_SI];
	}
	else
	{
		/* Interpolate */
		dSumPt0Pt1 = pdZ[INT_PT2_SI] + pdZ[INT_PT1_SI];
		dGradPt0Pt1 = pdZ[INT_PT2_SI] - pdZ[INT_PT1_SI];
		dSumPtM1Pt2 = pdZ[INT_PT3_SI] + pdZ[INT_PT0_SI];
		dGradPtM1Pt2 = (pdZ[INT_PT3_SI] - pdZ[INT_PT0_SI]) / (double)(NUM_INTER_PTS_SI - 1);

		pdA[INTER_COEFF1_SI] = (INTERPOL_CONST1_D * dSumPt0Pt1) - dSumPtM1Pt2;
		pdA[INTER_COEFF2_SI] = (INTERPOL_CONST1_D * dGradPt0Pt1) - dGradPtM1Pt2;
		pdA[INTER_COEFF3_SI] = dSumPtM1Pt2 - dSumPt0Pt1;
		pdA[INTER_COEFF4_SI] = dGradPtM1Pt2 - dGradPt0Pt1;

		dDX = (TWO_D * dDeltaX) - 1.0;

		/* Now calculate dIntZ = 1/16(dA[3]dDX^3 + dA[2]dDX^2 + dA[1]dDX + dA[0])
		 * where dDX3 means 'dDX cubed' and dDX2 means 'dDX squared'.*/
		dIntZ = 0;
		for(siIndex = NUM_INTER_PTS_SI - 1; siIndex >= 0; siIndex = siIndex - 1)
		{
			dIntZ = (dIntZ * dDX) + pdA[siIndex];
		}
		dIntZ = dIntZ / INTERPOL_CONST2_D;
	} /* end if dDeltaX is small */

	/* Return the interpolated value */
	return (dIntZ);
}


/**
 * @brief This function calculates Az from the provided coefficients and modified dip
 *        latitude value (dModip). If only one coefficient (A0) is supplied then that
 *        is Az (ie. it doesn't vary with Modip).
 *
 * @param dModip     The modified dip latitude of current position (I)
 * @param siNumCoeff The number of items in the pdCoeff array (I)
 * @param pdCoeff    Array containing values of A0, A1, A2, etc. (I)
 *
 *
 * @return The calculated Az for given Modip
 */
static double NeqModipToAz(double dModip, int siNumCoeff, double *pdCoeff)
{
	double dFlx;  /* The flux (Az) value */
	double dTerm; /* The value of the next term of polynomial before any coeff
	 * is applied (ie. dMoDIP to the powers of 0,1,2,etc.) */
	int siIndex;  /* Loop variable */


	/* Initialise dFlx to zero */
	dFlx = 0.0;

	/* Now calculate Az (= A0 + A1*dModip + A2*dModip^2 +...) by iteration */
	dTerm = 1.0;
	for(siIndex = 0; siIndex < siNumCoeff; siIndex = siIndex + 1)
	{
		dFlx = dFlx + (pdCoeff[siIndex] * dTerm);
		dTerm = dTerm * dModip;
	}

	/* Calculation is now over, so return flux */
	return (dFlx);
}


/**
 * @brief This function calculates the Spherical Legendre Coefficients to be used in
 *        calculating the foF2 or M(3000)F2 frequencies. These coefficients are based
 *        on the current interpolated CCIR map (based on Month and R12) and universal
 *        time and should be recalculated if the CCIR map or time changes.
 *
 * @param dUT        The universal time (I)
 * @param ptCurrCCIR In: pdF0F2 and pdM3000F2 Out: pdLegCoeffs_F0 and pdLegCoeffs_M3000
 */
static void NeqCalcSphLegCoeffs(double dUT, TCurrentCcir *pstCurrCCIR)
{
	double dSunLng; /* Sun Longitude, where the sun is directly overhead */
	double pdSinHarm[MAX_HARMONICS_SI]; /* sin(dSunLng) & harmonics */
	double pdCosHarm[MAX_HARMONICS_SI]; /* cos(dSunLng) & harmonics */
	int siHarmInd;  /* Loop index for harmonic number */
	int siRowInd;   /* Loop index for row in the CCIR map data */
	int siMxRowInd; /* Array index to start of current row in CCIR arrays */
	int siMxInd;    /* Array index into CCIR arrays */


	/* Calculate the sun longitude */
	dSunLng = ((DEGPERHOUR_D * dUT) - RANGE_LONDEG_MAX_D) * DEG_TO_RAD_D;

	/* Calculate the sine and cosine of the fundamental and the harmonics.
	 * Use sin(nA)=sin[(n-1)A + A] with sin(A+B) = sin(A)cos(B) + cos(A)sin(B),
	 * and cos(nA)=sin[(n-1)A + A] with cos(A+B) = cos(A)cos(B) - sin(A)sin(B). */
	pdSinHarm[0] = sin(dSunLng);
	pdCosHarm[0] = cos(dSunLng);
	for(siHarmInd = 1; siHarmInd < MAX_HARMONICS_SI; siHarmInd = siHarmInd + 1)
	{
		pdSinHarm[siHarmInd] = (pdSinHarm[siHarmInd - 1] * pdCosHarm[0]) + (pdCosHarm[siHarmInd - 1] * pdSinHarm[0]);
		pdCosHarm[siHarmInd] = (pdCosHarm[siHarmInd - 1] * pdCosHarm[0]) - (pdSinHarm[siHarmInd - 1] * pdSinHarm[0]);
	} /* end for each harmonic */

	/* Calculate the coefficients for spherical Legendre function for FoF2 */
	for(siRowInd = 0; siRowInd < CCIR_F2_DIM_2_SI; siRowInd = siRowInd + 1)
	{
		/* Calculate the index for the row in CurrCCIR->pdF0F2 */
		siMxRowInd = siRowInd * CCIR_F2_DIM_1_SI;

		/* Now calculate the spherical Legendre coefficients */
		pstCurrCCIR->pdLegCoeffs_F0[siRowInd] = pstCurrCCIR->pdF0F2[siMxRowInd];
		for(siHarmInd = 0; siHarmInd < NUM_F0_HARMONICS_SI; siHarmInd = siHarmInd + 1)
		{
			siMxInd = siMxRowInd + (TWO_SI * (siHarmInd + 1));
			pstCurrCCIR->pdLegCoeffs_F0[siRowInd] = pstCurrCCIR->pdLegCoeffs_F0[siRowInd] + (pstCurrCCIR->pdF0F2[siMxInd - 1] * pdSinHarm[siHarmInd]) + (pstCurrCCIR->pdF0F2[siMxInd] * pdCosHarm[siHarmInd]);
		} /* end for each harmonic */
	} /* end for each matrix row */

	/* Calculate the coefficients for spherical Legendre function for M3000F2 */
	for(siRowInd = 0; siRowInd < CCIR_M3000_DIM_2_SI; siRowInd = siRowInd + 1)
	{
		/* Calculate the index for the row in CurrCCIR->pdM3000F2 */
		siMxRowInd = siRowInd * CCIR_M3000_DIM_1_SI;

		/* Now calculate the spherical Legendre coefficients */
		pstCurrCCIR->pdLegCoeffs_M3000[siRowInd] = pstCurrCCIR->pdM3000F2[siMxRowInd];
		for(siHarmInd = 0; siHarmInd < NUM_M3K_HARMONICS_SI; siHarmInd = siHarmInd + 1)
		{
			siMxInd = siMxRowInd + (TWO_SI * (siHarmInd + 1));
			pstCurrCCIR->pdLegCoeffs_M3000[siRowInd] = pstCurrCCIR->pdLegCoeffs_M3000[siRowInd] + (pstCurrCCIR->pdM3000F2[siMxInd - 1] * pdSinHarm[siHarmInd]) + (pstCurrCCIR->pdM3000F2[siMxInd] * pdCosHarm[siHarmInd]);
		} /* end for each harmonic */
	} /* end for each matrix row */

	/* Remember the universal time for which these coefficients are correct */
	pstCurrCCIR->dUT = dUT;
}


/**
 * @brief This function returns foF2 or M(3000)F2 calculated from CCIR map file data.
 *
 * @param dCosLat       Cosine of the current latitude (I)
 * @param dLng          Current longitude (deg) (I)
 * @param pdLegCoeffs   Current set of Spherical Legendre coefficients (I)
 * @param pdSinModipToN Array containing sin(Modip)^N terms (I)
 * @param siMode        Enum indicating which frequency to calculated (I)
 *
 * @return The calculated foF2 or M(3000)F2 value
 */
static double NeqGetF2FreqFromCCIR(double dCosLat, double dLng, double pdLegCoeffs[MAX_CCIRROWS_SI],double pdSinModipToN[MAX_SINMODIP_TERM_SI], int siMode)
{
	int psiQFo[NUM_QF0_SI] = { /* Coeffs for spherical Legendre on F0F2 */
			QF0_1_SI,           QF0_2_SI,           QF0_3_SI,
			QF0_4_SI,           QF0_5_SI,           QF0_6_SI,
			QF0_7_SI,           QF0_8_SI,           QF0_9_SI
	};
	int psiQM3000[NUM_QM3000_SI] = { /* Coeffs for spherical Legendre on M3000 */
			QM3000_1_SI,        QM3000_2_SI,        QM3000_3_SI,
			QM3000_4_SI,        QM3000_5_SI,        QM3000_6_SI,
			QM3000_7_SI
	};
	int *psiNQ;        /* Constants used in the spherical Legendre expansion */
	int siK1;          /* Number of 'siNQ[]'s */
	double dResult;    /* Function return value, either foF2 or M(3000)F2 */
	int siIndex1;      /* Loop variable */
	int siCoeffInd;    /* Index into pdLegCoeffs array */
	double dCosLatToN; /* cos(current lat) ^ N */
	int siNQInd;       /* Loop index into siNQ array */
	double dJLng;      /* J * current lng (radians) */
	double dSinJLng;   /* sin(dJLng) */
	double dCosJLng;   /* cos(dJLng) */


	/* Set up the indices dependent on which frequency we are calculating */
	if(siMode == CCIR_MODE_F0_SI)
	{
		psiNQ = &psiQFo[0];
		siK1 = NUM_QF0_SI;
	}
	else
	{
		psiNQ = &psiQM3000[0];
		siK1 = NUM_QM3000_SI;
	} /* end if CCIR_MODE_F0 */

	/* Initially set the result to the first Legendre coefficient */
	dResult = pdLegCoeffs[0];

	/* Add sum of (coeff * sin^N(Modip)) */
	for(siIndex1 = 1; siIndex1 <= psiNQ[0]; siIndex1 = siIndex1 + 1)
	{
		/* Check sin^N(Modip) term is not negligible */
		if(fabs(pdSinModipToN[siIndex1]) >= SINMODIP_CUTOFF_D)
		{
			/* Add next term to the result */
			dResult = dResult + (pdLegCoeffs[siIndex1] * pdSinModipToN[siIndex1]);
		} /* end if pdSinModipToN term is not negligible */
	} /* end for each (coeff * sin^N(Modip)) term */

	/* Set the initial pdLegCoeffs index & variables for cos^N(lat) calculations*/
	siCoeffInd = psiNQ[0] + 1;
	dCosLatToN = dCosLat;

	/* Add sum of (sum of (sin^N(Modip) * cos^N(Lat) *
	 *                     (coeff*cos(j*lng) + coeff*sin(j*lng)))) */
	for(siNQInd = 1; siNQInd < siK1; siNQInd = siNQInd + 1)
	{
		/* Calculate cos(j*lng) and sin(j*lng) */
		dJLng = (double)siNQInd * dLng * DEG_TO_RAD_D;
		dSinJLng = sin(dJLng);
		dCosJLng = cos(dJLng);

		/* Add sum of (sin^N(Modip) * cos^N(Lat) *
		 *             (coeff*cos(j*lng) + coeff*sin(j*lng))) */
		for(siIndex1 = 0; siIndex1 <= psiNQ[siNQInd]; siIndex1 = siIndex1 + 1)
		{
			/* Add next term to the result and incrememnt siCoeffInd appropriately */
			dResult = dResult + (pdSinModipToN[siIndex1] * dCosLatToN * ((pdLegCoeffs[siCoeffInd] * dCosJLng) + (pdLegCoeffs[siCoeffInd + 1] * dSinJLng)));
			siCoeffInd = siCoeffInd + TWO_SI;
		} /* end for each siIndex1 */

		/* Calculate next power of dCosLat */
		dCosLatToN = dCosLatToN * dCosLat;
	} /* end for each siNQInd */

	/* Return the summation of the spherical Legendre functions */
	return (dResult);
}


/**
 * @brief This function calculates the electron density from the peak plasma
 *        frequency of a layer.
 *
 * @param dF0 The peak plasma frequency for the layer (I)
 *
 * @return Electron density
 */
static double NeqCriticalFreqToNe(double dF0)
{
	/* Return the electron density */
	return (FREQ2NE_FACTOR_D * dF0 * dF0);
}


/**
 * @brief This function calculates the height at which the electron density peak of
 *        the F2 layer occurs.
 *
 *   This expression is a modified version of Dudeney, 1983, J. Atmos. Terr.
 *   Phys., 45, 629-640.
 *
 *   It is modified in that it uses 1.2967 instead of 1.296, 1490 instead of
 *   1470 and performs a NeqJoin on (dF0F2/dF0E).
 *
 * @param dM3000 The current M(3000)F2 (I)
 * @param dF0E   The current foE (E-peak plasma frequency) (I)
 * @param dF0F2  The current foF2 (E-peak plasma frequency) (I)
 *
 * @return Height at which electron density peak occurs in F2
 */
static double NeqCalcF2PeakHeight(double dM3000, double dF0E, double dF0F2)
{
	double dM3000Squared;  /* M(3000)F2 squared */
	double dNumerator;     /* Used to store top half of formula */
	double dRatioF0F2_F0E; /* Ratio of F0F2 to F0E */
	double dDenominator;   /* Used to store bottom half of formula */


	/* Calculate the numerator */
	dM3000Squared = dM3000 * dM3000;
	dNumerator = dM3000 * sqrt(((DUDENEY_CONST1_D * dM3000Squared) + 1.0) / ((DUDENEY_CONST2_D * dM3000Squared) - 1.0));

	/* Check if dF0E is so small that (0.253/...) term can be approximated to
	 * zero */
	if(dF0E < DUDENEY_SMALL_F0E_D)
	{
		/* dF0E is so small that term can be approximated */
		dDenominator = dM3000 - DUDENEY_CONST3_D;
	}
	else
	{
		/* Term cannot be approximated so use full formula */
		dRatioF0F2_F0E = dF0F2 / dF0E;
		dRatioF0F2_F0E = NeqJoin(dRatioF0F2_F0E, DUDENEY_CLIP_D, DUDENEY_JOIN_D, dRatioF0F2_F0E - DUDENEY_CLIP_D);
		dDenominator = dM3000 - DUDENEY_CONST3_D + (DUDENEY_CONST4_D / (dRatioF0F2_F0E - DUDENEY_CONST5_D));
	}

	/* Calculate and return the peak height */
	return (((DUDENEY_CONST6_D * dNumerator) / dDenominator) - DUDENEY_CONST7_D);
}


/**
 * @brief This function uses the Epstein Layer formula to calculate 1/4 of the
 *        electron density at a height dH.
 *
 *   The full formula for an Epstein layer is:
 *
 *                        (h-Hm)/B
 *       N(h) = 4 * Nm * e
 *              ------------------
 *               { (h-Hm)/B   } 2
 *               {e        + 1}
 *
 *   where:
 *       N(h) = electron density at height h
 *       Nm   = electron density peak for the layer
 *       Hm   = height of layer electron density peak
 *       B    = layer thickness parameter
 *
 *   This function only calculates a 1/4 of it because the factor of 4* is
 *   completed once other mathematical formulas have been done to decrease
 *   computation time.
 *
 * @param dNmax The peak density for the layer (I)
 * @param dHmax The height of the layer electron density peak (I)
 * @param dB    The layer thickness parameter (I)
 * @param dH    THe height at which the electron density is required (I)
 *
 *
 * @return 1/4 of the electron density at the point
 */
static double NeqEpstein(double dNmax, double dHmax, double dB, double dH)
{
	double dExpTerm; /* Temporary variable used to store e^[(h-hm)/B] */

	/* Calculate 1/4 * electron density */
	dExpTerm = NeqClipExp((dH - dHmax) / dB);
	return (dNmax * dExpTerm / NeqSquared(dExpTerm + 1.0));
}


