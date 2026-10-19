/**
 * @file    nequick.c
 *
 * @author  Mario Nicola
 * @author  Gianluca Falco
 *
 * @date    09/01/2015
 *
 * @version 1.0.0
 *
 * @brief   Functions that evaluates the ionosphere corrections through NeQuick model reference model
 *
 */

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "ngeneConfiguration.h"
#include "ioFunctions.h"
#include "ionoCalcModip.h"
#include "nequickComputation.h"
#include "nequickConstants.h"
#include "nequickDataStructures.h"
#include "ngeneConstants.h"
#include "spreadingCodes.h"

/* parameters file */
#define MODIP_FILE 			"nequickParameters/modip2001_wrapped.asc"
#define CCIR_FILE_PATTERN 	"nequickParameters/ccir"
#define CCIR_FILE_EXTENSION ".asc"

/* NeQuick configuration options */
#define KRONROD_TOL_1 (0.001)
#define KRONROD_TOL_2 (0.01)
#define RECURSION_LIMIT (50)

static TModip * LoadModipGrid(TNGeneConfiguration *cfg, TModip *pstModip)
{
	FILE *fpFile;
	int siLatIndex;
	int siLngIndex;
	char filename[MAX_ROW_LENGTH];

	filename[0] = '\0';
	strcat(filename, cfg->otherDataPath);
	strcat(filename, MODIP_FILE);
	fpFile = fopen(filename, "r");
	if(fpFile == NULL)
	{
		SwrxLog(NGENE_LOG_SEVERITY_WARNING,"NeQuick model: unable to open file %s", filename);
		return NULL;
	}

	for(siLatIndex = 0; siLatIndex < NUM_MODIP_DIM_1_SI; siLatIndex++)
	{
		for(siLngIndex = 0; siLngIndex < NUM_MODIP_DIM_2_SI; siLngIndex++)
		{
			if(fscanf(fpFile, "%lf", &(pstModip->pdModip[siLatIndex][siLngIndex])) == EOF)
			{
				fclose(fpFile);
				SwrxLog(NGENE_LOG_SEVERITY_WARNING,"NeQuick model: Unexpected EOF in %s", filename);
				return NULL;
			}
		}
	}

	fclose(fpFile);
	return pstModip;
}


static TCcir * LoadCcirData(TNGeneConfiguration *cfg, TCcir *pstCCIR)
{
	int siMonth;
	char filename[2*MAX_ROW_LENGTH];
	FILE* pAsc[MONTHS_SI];
	int siIndex1;
	int siIndex2;
	int siSolar;

	/* Open the input files */
	for(siMonth = 0; siMonth < MONTHS_SI; siMonth++)
	{
		/* Attempt to open the files ccir11.asc -> ccir22.asc */
		sprintf(filename, "%s%s%d%s",cfg->otherDataPath,CCIR_FILE_PATTERN,siMonth + 11,CCIR_FILE_EXTENSION);
		pAsc[siMonth] = fopen(filename, "r");

		/* Check if file was opened correctly */
		if(pAsc[siMonth] == NULL)
		{
			SwrxLog(NGENE_LOG_SEVERITY_WARNING,"NeQuick model: unable to open file %s", filename);
			/* Open has failed so need to close all files already opened */
			for(siIndex1 = 0; siIndex1 < siMonth; siIndex1++)
			{
				fclose(pAsc[siIndex1]);
			}

			return NULL;
		}
	}

	/* Files are all open, so read in the data */
	for(siSolar = 0; siSolar < CCIR_F2_DIM_3_SI; siSolar++)
	{
		for(siIndex2 = 0; siIndex2 < CCIR_F2_DIM_2_SI; siIndex2++)
		{
			for(siIndex1 = 0; siIndex1 < CCIR_F2_DIM_1_SI; siIndex1++)
			{
				for(siMonth = 0; siMonth < MONTHS_SI; siMonth++)
				{
					fscanf(pAsc[siMonth], "%lf", &(pstCCIR->pdF2[siMonth][siIndex1][siIndex2][siSolar]));
				}
			}
		}
	}

	for(siSolar = 0; siSolar < CCIR_M3000_DIM_3_SI; siSolar++)
	{
		for(siIndex2 = 0; siIndex2 < CCIR_M3000_DIM_2_SI; siIndex2++)
		{
			for(siIndex1 = 0; siIndex1 < CCIR_M3000_DIM_1_SI; siIndex1++)
			{
				for(siMonth = 0; siMonth < MONTHS_SI; siMonth++)
				{
					fscanf(pAsc[siMonth], "%lf",&(pstCCIR->pdM3000[siMonth][siIndex1][siIndex2][siSolar]));
				}
			}
		}
	}

	/* Close the files */
	for(siMonth = 0; siMonth < MONTHS_SI; siMonth++)
	{
		fclose(pAsc[siMonth]);
	}

	return pstCCIR;
}

void InitNeQuickModel(TNGeneConfiguration *cfg,TNeQuickData *nq)
{
	if(LoadModipGrid(cfg,&(nq->stModip))==NULL)
	{
		nq->stModipValid = FALSE;
	}
	else
	{
		nq->stModipValid = TRUE;
	}
	if(LoadCcirData(cfg,&(nq->stCcir))==NULL)
	{
		nq->stCcirValid = FALSE;
	}
	else
	{
		nq->stCcirValid = TRUE;
	}
	nq->modip = 0.0;
	nq->modipRegion = UNKNOWN_REGION;
	nq->modipValid = FALSE;
}


void DeinitNeQuickModel(TNeQuickData *nq)
{
	nq->stModipValid = FALSE;
	nq->stCcirValid = FALSE;
}


double GalileoIonosphericCorrection(TNeQuickData *nq,double *pdCoeffs, int siMonth, double dTime, double *pdRecvLLHdeg, double *pdSatLLHdeg)
{
	double dModip = 0.0;
	double dStec = 0.0;
	double pdKronrodTol[2] = {KRONROD_TOL_1, KRONROD_TOL_2};
	double pdRecvLlhRad[3];
	double pdSatLlhRad[3];
	double dAzValue;

	if((nq->stModipValid==FALSE) || (nq->stCcirValid==FALSE))
		return 0.0;

	pdRecvLlhRad[0] = pdRecvLLHdeg[0] * PI_D / 180.0;
	pdRecvLlhRad[1] = pdRecvLLHdeg[1] * PI_D / 180.0;
	pdRecvLlhRad[2] = pdRecvLLHdeg[2];
	pdSatLlhRad[0] = pdSatLLHdeg[0] * PI_D / 180.0;
	pdSatLlhRad[1] = pdSatLLHdeg[1] * PI_D / 180.0;
	pdSatLlhRad[2] = pdSatLLHdeg[2];

	nq->stNeQuickInputData.pstModip = &(nq->stModip);
	nq->stNeQuickInputData.pstCCIR = &(nq->stCcir);
	nq->stNeQuickInputData.pdGssPosLLH = pdRecvLlhRad;
	nq->stNeQuickInputData.pdSatPosLLH = pdSatLlhRad;
	nq->stNeQuickInputData.pdCoeff = pdCoeffs;
	nq->stNeQuickInputData.pdKronrodTol = pdKronrodTol;
	nq->stNeQuickInputData.siMaxRecurse = RECURSION_LIMIT;
	nq->stNeQuickInputData.siMonth = siMonth;
	nq->stNeQuickInputData.dUT = dTime;

	IonoCalcModip(&(nq->stModip), pdRecvLlhRad[0], pdRecvLlhRad[1], &dModip);

	dAzValue = pdCoeffs[0] + (dModip * pdCoeffs[1]) + (dModip * dModip * pdCoeffs[2]);

	nq->stNeQuickInputData.pdCoeff = &dAzValue;
	nq->stNeQuickInputData.siNumCoeff = 1;

	if(NeQuickComputation(&(nq->stNeQuickInputData), &(nq->stCurrCcir), &dStec)==E_ERROR)
		return 0.0;

	/*save MODIP */
	nq->modip = dModip;
	if(nq->modip > 60.0)
	{
		nq->modipRegion = NORTHERN_REGION;
	}
	else if(nq->modip > 30.0)
	{
		nq->modipRegion = NORTHERN_MIDDLE_REGION;
	}
	else if(nq->modip > -30.0)
	{
		nq->modipRegion = SOUTHERN_MIDDLE_REGION;
	}
	else
	{
		nq->modipRegion = SOUTHERN_REGION;
	}
	nq->modipValid = TRUE;

	/* return value */
	dStec = (dStec * 40.3)/(L1_FREQUENCY*L1_FREQUENCY);
	return dStec;
}
