/**
 * @file    nequickComputation.c
 *
 * @author  Paul Whitehall - Nottingham Scientific Ltd
 *
 * @date    23/3/2010
 *
 * @version 1.0.0
 *
 * @brief   This module contains the NSL implementation of the NeQuick model main entry point.
 *
 *          From the NeQuick model reference code with minor changes.
 */

/*
===============================
> > NeQuickGalileoSF < <
Version: 1.0.3
Release date: 23/07/2012 (see CHANGELOG)
===============================
NeQuick electron density model is used by the Galileo ionospheric algorithm 
for single frequency receivers for correcting the ionospheric error contribution 
due to group delay on the signal. NeQuick is an empirical climatological 
ionospheric model, which generates electron density from analytical profiles, 
for the input values: R12 or F10.7, month, geographic latitude and longitude, 
height and UT (see [RD1, RD2]). Changing the height leads to the height profile 
of electron density. The use of an additional great circle subroutine allows 
calculating electron density along arbitrary ground-to-satellite or 
satellite-to-satellite ray paths. The model depends on geophysical parameters 
(R12 or F10.7) and empirical estimated coefficients such as the CCIR files. 

This version NeQuickGalileoSF v1.0.3 was developed by Nottingham Scientific Limited 
and the European Space Agency. This version is based on the monthly mean 
climatological NeQuickv1 model recommended by ITU-R with a number of adaptations
as required for Galileo real-time use based on broadcast parameters and incorporating 
a number of enhancements proposed by some of the original ITU-R version authors.

NeQuickv1 recommended by ITU-R was developed at Abdus Salam ICTP Trieste 
and at the University of Graz. It is based on the Di Giovanni - Radicella (DGR) model
(Di Giovanni, G., S. R. Radicella, An analytical model of the electron density 
profile in the ionosphere, Adv. Space Res. 10, 27-30, 1990) which was modified 
to the requirements of the COST 238 Action PRIME to give vertical electron content 
from ground to 1000 km consistent with the COST 238 regional electron content 
model (Radicella, S.M., M.-L. Zhang, The improved DGR analytical model of electron 
density height profile and total electron content in theionosphere. A. Geofisica, 38, 35-41, 1995).
Authors of the modified Di Giovanni - Radicella (DGR) model code:
	Man-Lian Zhang and S.M. Radicella, Abdus Salam ICTP Trieste.
Author of modifications of DGR code, of adaptations of "CCIR map code" and responsible 
author for ITU-R NeQuickv1 package: Dr. Reinhart Leitinger (IGAM, UGraz) with 
contributions by Gerald Hochegger (IGAM), Bruno Nava and Pierdavide Coisson (Abdus Salam ICTP)
 */


#include <math.h>
#include <stdio.h>
#include "nequickComputation.h"


/* Function prototypes */
static int NeqCheckInputs(TNeQuickInputData *pstNeQuickInputData);
static void DoTECIntegration(TIntegrateData *pstIntegrateData,TBoolean bVert, TSPoint stP0, double *pdNmax,TLayerProperties *pstLayers, double *pdTec);


TReturnValue NeQuickComputation(TNeQuickInputData *pstNeQuickInputData,TCurrentCcir *pstCurrCCIR, double *pdSTEC)
{
	TReturnValue eReturnValue; /* Function return value */
	TBoolean bError;           /* Flag indicating whether or not error occured */
	TBoolean bVertical = FALSE;    /* Flag indicating
	 * whether P2 directly above P1 */
	double dAmrad;   /* Used to calc sine and cosine of declination of sun */
	TSPoint stP0;  /* Positional information */
	double dTec;     /* Total Electron Content between point 1 and point 2 */
	TLayerProperties stLayers;      /* Properties for Epstein layers */
	double dNmax;                     /* Electron content at F2 peak */
	TGeometryData stGeom;           /* Geometry data */
	TIntegrateData stIntegrateData; /* Input data for integration function */

	stIntegrateData.pstNeQuickInputData = pstNeQuickInputData;
	stIntegrateData.pstGeom = &stGeom;
	stIntegrateData.pstCurrCCIR = pstCurrCCIR;

	/* Check input values are within required range */
	bError = NeqCheckInputs(pstNeQuickInputData);
	if(bError != TRUE)
	{
		eReturnValue = E_OK;

		/* Set the tP1 and tP2 to the values in the inputs. Coordinates given in WGS-84
	     * ellipsoidal coordinates: geodetic latitude, geodetic longitude and ellipsoidal height.  */
		stGeom.stP1.dLat = pstNeQuickInputData->pdGssPosLLH[COORD_LAT_SI] *
				HALFCIRCLEDEG_D / PI_D;
		stGeom.stP1.dLng = pstNeQuickInputData->pdGssPosLLH[COORD_LNG_SI] *
				HALFCIRCLEDEG_D / PI_D;
		stGeom.stP1.dH = pstNeQuickInputData->pdGssPosLLH[COORD_H_SI] / KMTOM_D;
		stGeom.stP2.dLat = pstNeQuickInputData->pdSatPosLLH[COORD_LAT_SI] *
				HALFCIRCLEDEG_D / PI_D;
		stGeom.stP2.dLng = pstNeQuickInputData->pdSatPosLLH[COORD_LNG_SI] *
				HALFCIRCLEDEG_D / PI_D;
		stGeom.stP2.dH = pstNeQuickInputData->pdSatPosLLH[COORD_H_SI] / KMTOM_D;

		/* Ensure dLngs are between 0.0 and 360.0 */
		stGeom.stP1.dLng = fmod(stGeom.stP1.dLng + CIRCLEDEG_D, CIRCLEDEG_D);
		stGeom.stP2.dLng = fmod(stGeom.stP2.dLng + CIRCLEDEG_D, CIRCLEDEG_D);

		/* Set the 'Old' variables to invalid values (it doesn't matter what) so
		 * that they will be flagged as changed in NeqCalcEpstParams */
		pstCurrCCIR->siMonth = -1;
		pstCurrCCIR->dR12 = -1.0;
		pstCurrCCIR->dUT = -1.0;

		/* Call NeqGetRayProperties to calculate ray properties and check that it
		 * is valid */
		bError = NeqGetRayProperties(&(stGeom.stP1), &(stGeom.stP2),&(stGeom.stRay), &(stGeom.dZeta),&(stGeom.dSinSig), &(stGeom.dCosSig));
		if(bError != TRUE)
		{
			eReturnValue = E_OK;

			/* Ray is valid. Calculate slant distance of each point */
			stGeom.stP1.dS = sqrt((stGeom.stP1.dR * stGeom.stP1.dR) -
					(stGeom.stRay.dR * stGeom.stRay.dR));
			stGeom.stP2.dS = sqrt((stGeom.stP2.dR * stGeom.stP2.dR) -
					(stGeom.stRay.dR * stGeom.stRay.dR));

			/* Ensure P0 is on or above earth surface by setting height and radius */
			if(stGeom.stP1.dH > 0.0)
			{
				stP0.dH = stGeom.stP1.dH;
			}
			else
			{
				stP0.dH = 0.0;
			} /* end if p1 above surface */
			stP0.dR = stP0.dH + EARTH_RADIUS_D;

			/* Calculate slant distance of point zero */
			stP0.dS = sqrt((stP0.dR * stP0.dR) - (stGeom.stRay.dR * stGeom.stRay.dR));

			/* Initialize tPactual (the 'current' position). tPactual stores dH,dLat,
			 * dLng only (other attributes not used) */
			stGeom.stPactual.dH = stGeom.stP2.dH;
			stGeom.stPactual.dLat = stGeom.stP1.dLat;
			stGeom.stPactual.dLng = stGeom.stP1.dLng;

			/* Calculate sine and cosine of delta */
			dAmrad = ((SOLARDECCONST_1_D * (((double)pstNeQuickInputData->siMonth * AVEMONTHDAYS_D) - MIDMONTHPT_D + ((SOLARDECCONST_2_D - pstNeQuickInputData->dUT) / DAYHOURS_D))) - SOLARDECCONST_3_D) * DEG_TO_RAD_D;
			stGeom.dSinDelta = SOLARDECCONST_4_D * sin(dAmrad + (((SOLARDECCONST_5_D * sin(dAmrad)) + (SOLARDECCONST_6_D * sin(TWO_SI*dAmrad)) + SOLARDECCONST_7_D) * DEG_TO_RAD_D));
			stGeom.dCosDelta = sqrt(1.0 - (stGeom.dSinDelta * stGeom.dSinDelta));

			/* Check if P2 is directly above P1 */
			if(stGeom.stRay.dR < VERTTOLRAYDR_D)
			{ /* P2 directly above P1 */

				/* Calculate the current ionosphere values by calling */
				/* NeqCalcEpstParams() as it is not called later in */
				/* NeqGetNeOnSlantRay() */
				NeqCalcEpstParams(pstNeQuickInputData, stGeom.stPactual, stGeom.dSinDelta, stGeom.dCosDelta, pstCurrCCIR, &dNmax, &stLayers);

				bVertical = TRUE;

			} /* End of check if P2 is directly above P1 */
			DoTECIntegration(&stIntegrateData, bVertical, stP0, &dNmax, &stLayers, &dTec);

		}
		else
		{

			eReturnValue = E_ERROR;
			dTec = -1.0;

		} /* End if NeqGetRayProperties successful */

	}
	else
	{ /* Error occured during input check */

		eReturnValue = E_ERROR;
		dTec = -TWO_D;

	} /* End of check input values are within required range */

	/* Set output value and return error status */
	*pdSTEC = UNITCONVERSION_D * dTec;
	return eReturnValue;
}


/**
 * @brief This function checks that all the values passed into NeQuick are within valid ranges.
 *        Note that the configuration data (pstModip, pstCCIR, pdKronrodTol and
 *        siKronrodMaxRecursion will have been checked by SFIONO and IDF
 *
 * @param pstNeQuickInputData Input data for NeQuick function [in]
 *
 * @return TRUE if error found, otherwise FALSE
 */
static int NeqCheckInputs(TNeQuickInputData *pstNeQuickInputData)
{
	TBoolean bError = FALSE; /* Return value for function */

	/* Check pdGssPosLLH */
	if((pstNeQuickInputData->pdGssPosLLH[0] < -HALFPI_D) || (pstNeQuickInputData->pdGssPosLLH[0] > HALFPI_D))
	{
		bError = TRUE;
	}

	/* Check pdSatPosLLH */
	if((pstNeQuickInputData->pdSatPosLLH[0] < -HALFPI_D) || (pstNeQuickInputData->pdSatPosLLH[0] > HALFPI_D))
	{
		bError = TRUE;
	}

	/* Check month and universal time */
	if((pstNeQuickInputData->siMonth <= 0) || (pstNeQuickInputData->siMonth > MONTHS_SI))
	{
		bError = TRUE;
	}
	if((pstNeQuickInputData->dUT < 0.0) || (pstNeQuickInputData->dUT > DAYHOURS_D))
	{
		bError = TRUE;
	}

	/* Check the coefficients */
	if(pstNeQuickInputData->siNumCoeff < 1)
	{
		bError = TRUE;
	}
	/* Check array containing the a0, a1 and a2 values */
	if(pstNeQuickInputData->pdCoeff == NULL)
	{
		bError = TRUE;
	}

	/* Return TRUE if error found, otherwise FALSE */
	return bError;
}

/**
 * @brief The purpose of this function is to perform TEC integration.
 *
 * @param pstIntegrateData Integration input data [in]
 * @param bVert            [in]
 * @param stP0             Positional information [in]
 * @param pdNmax           Electron content at F2 peak [out]
 * @param pstLayers        Current ionospheric properties (see NeqCalcEpstParams) [out]
 * @param pdTec            Total Electron Content between point 1 and point 2 [out]
 */
static void DoTECIntegration(TIntegrateData *pstIntegrateData,TBoolean bVert, TSPoint stP0, double *pdNmax,TLayerProperties *pstLayers, double *pdTec)
{
	double dHeightP0; /* Height (km) at position P0*/
	double dHeightP1; /* Height (km) at position P1*/
	double dHeightP2; /* Height (km) at position P2*/
	double dR1a;      /* Radius (km) of the first integration breakpoint */
	double dS1a;      /* Slant distance (km) of the first breakpoint */
	double dR1b;      /* Radius (km) of the second integration breakpoint */
	double dS1b;      /* Slant (km) distance of the second breakpoint */
	double dTec1;     /* Temporary TEC values */
	double dTec2;     /* Temporary TEC values */
	double *pdKronrodIntTol = pstIntegrateData->pstNeQuickInputData->pdKronrodTol;
	/* Tolerances for the Kronrod integration */
	TGeometryData *pstGeometry = pstIntegrateData->pstGeom; /* Geometry data */

	/* Get height of points P0, P1 and P2 */
	if(bVert == TRUE)
	{
		dHeightP0 = stP0.dH;
		dHeightP1 = pstGeometry->stP1.dH;
		dHeightP2 = pstGeometry->stP2.dH;
	}
	else
	{
		dHeightP0 = stP0.dS;
		dHeightP1 = pstGeometry->stP1.dS;
		dHeightP2 = pstGeometry->stP2.dS;
	}

	pstIntegrateData->dTolerance = pdKronrodIntTol[0];

	/* Integration should be broken down if the integration path from P1(P0) */
	/* to P2 crosses either of the integration breakpoints. These tests are */
	/* based on the assumption that P1 is always lower than P2. */

	if(pstGeometry->stP2.dH <= INT_BREAK1_D)
	{
		/* P1 (assumed) and P2 are both beneath the first integration breakpoint */
		/* so integrate straight between P0 and P2 for TEC. P0 is used because P1 */
		/* could be less than zero. */
		pstIntegrateData->bVert = bVert;

		*pdTec = NeqIntegrate(pstIntegrateData, dHeightP0, dHeightP2, 0, &(pstGeometry->stPactual), pdNmax, pstLayers);
	}
	else
	{
		if(bVert == TRUE)
		{
			dS1a = INT_BREAK1_D;
		}
		else
		{
			/* Calculate the slant distance of the first integration breakpoint */
			dR1a = INT_BREAK1_D + EARTH_RADIUS_D;
			dS1a = sqrt(dR1a * dR1a - pstGeometry->stRay.dR * pstGeometry->stRay.dR);
		}

		if(pstGeometry->stP2.dH <= INT_BREAK2_D)
		{
			if(pstGeometry->stP1.dH >= INT_BREAK1_D)
			{
				/* Both P1 and P2 are between the two integration breakpoints so */
				/* integrate straight between P1 and P2 for TEC */
				pstIntegrateData->bVert = bVert;

				*pdTec = NeqIntegrate(pstIntegrateData,dHeightP1,dHeightP2, 0, &(pstGeometry->stPactual),pdNmax, pstLayers);
			}
			else
			{
				if(bVert != TRUE)
				{
					pstIntegrateData->pstGeom->stP2.dS = sqrt(pstGeometry->stP2.dR * pstGeometry->stP2.dR - pstGeometry->stRay.dR * pstGeometry->stRay.dR);
				}

				/* Path between P1 and P2 crosses the first integration breakpoint */
				/* so integrate between P0 and the first breakpoint and from the */
				/* first breakpoint to P2. P0 is used because P1 could be */
				/* less than zero. */
				pstIntegrateData->bVert = bVert;

				dTec1 = NeqIntegrate(pstIntegrateData, dHeightP0, dS1a, 0, &(pstGeometry->stPactual), pdNmax, pstLayers);
				pstIntegrateData->dTolerance = pdKronrodIntTol[1];
				dTec2 = NeqIntegrate(pstIntegrateData, dS1a, dHeightP2, 0, &(pstGeometry->stPactual), pdNmax, pstLayers);
				*pdTec = dTec1 + dTec2;
			} /* End if P1 above first breakpoint */

		}
		else
		{

			if(pstGeometry->stP1.dH >= INT_BREAK2_D)
			{
				/* P1 and P2 are both above the second integration breakpoint so */
				/* integrate straight between P0 and P2 for TEC */
				pstIntegrateData->bVert = bVert;

				*pdTec = NeqIntegrate(pstIntegrateData, dHeightP1, dHeightP2,0, &(pstGeometry->stPactual), pdNmax, pstLayers);
			}
			else
			{

				if(bVert == TRUE)
				{
					dS1b = INT_BREAK2_D;
				}
				else
				{
					/* Calculate slant distance of second integration breakpoint */
					dR1b = INT_BREAK2_D + EARTH_RADIUS_D;
					dS1b = sqrt((dR1b * dR1b) - (pstGeometry->stRay.dR * pstGeometry->stRay.dR));
				}

				if(pstGeometry->stP1.dH >= INT_BREAK1_D)
				{
					/* Path between P1 and P2 crosses the second integration */
					/* breakpoint so integrate between P1 and the second breakpoint */
					/* and from the second breakpoint to P2 */
					pstIntegrateData->bVert = bVert;

					*pdTec = NeqIntegrate(pstIntegrateData, dHeightP1, dS1b,0, &(pstGeometry->stPactual), pdNmax, pstLayers) + NeqIntegrate(pstIntegrateData, dS1b, dHeightP2, 0, &(pstGeometry->stPactual), pdNmax, pstLayers);
				}
				else
				{
					/* Path between P1 and P2 crosses both integration breakpoints */
					/* so integrate between P0 and the first breakpoint, between the */
					/* two breakpoints and from the second breakpoint to P2. */
					/* P0 is used because P1 could be less than zero. */
					pstIntegrateData->bVert = bVert;

					dTec1 = NeqIntegrate(pstIntegrateData, dHeightP0, dS1a, 0, &(pstGeometry->stPactual), pdNmax, pstLayers);

					pstIntegrateData->dTolerance = pdKronrodIntTol[1];
					dTec2 = NeqIntegrate(pstIntegrateData, dS1a, dS1b, 0, &(pstGeometry->stPactual), pdNmax, pstLayers) + NeqIntegrate(pstIntegrateData, dS1b, dHeightP2, 0, &(pstGeometry->stPactual), pdNmax, pstLayers);

					*pdTec = dTec1 + dTec2;

				} /* End if P1 above first breakpoint */

			} /* End if P1 above both breakpoints */

		} /* End if P2 beneath second breakpoint */

	} /* End if P2 (& P1) beneath first breakpoint */

}

