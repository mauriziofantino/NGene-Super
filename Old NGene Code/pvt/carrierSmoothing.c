/**
 * @file carrierSmoothing.c
 *
 * @author Mario Nicola
 *
 * @date 07/11/2014
 *
 * @version 1.0.0
 *
 * @brief Functions used to perform carrier smoothing.
 */
#include <stdlib.h>
#include "carrierSmoothing.h"
#include "pvtDataStructure.h"
#include "ngeneConfiguration.h"
#include "ngeneConstants.h"
#include "ioFunctions.h"
#include "closeManager.h"


void ClearCsmDataChannel(TCarrierSmoothingData *csmDataCh)
{
	csmDataCh->csmMeasure = 0.0;
	csmDataCh->lpc = 0.0;
	csmDataCh->lpcOut = 0.0;
	csmDataCh->lpcOld = 0.0;
	csmDataCh->pseudorangeCsmOld = 0;
	csmDataCh->csmCounter = 0;
}


void ClearCsmData(TNGeneConfiguration *cfg,TPvtData *pvtData,TPvtResults *pvtRes)
{
	int idx;

	for(idx=0;idx<cfg->numberOfChannels;idx++)
	{
		ClearCsmDataChannel(&(pvtData->csmData[idx]));
	}
	pvtData->csmTransient = TRUE_FLAG;
	pvtData->csmTransientStart = pvtData->pvtIndex;
	if(cfg->pvtConfiguration.csmFilterLen>1)
	{
		pvtData->weightCode = 1.0/(double)cfg->pvtConfiguration.csmFilterLen;
		pvtData->weightCarrier = ((double)cfg->pvtConfiguration.csmFilterLen-1.0)*pvtData->weightCode;
	}
	else
	{
		pvtData->weightCode = 1.0;
		pvtData->weightCarrier = 0.0;
	}
	pvtRes->clockBiasGpsCumulated = 0.0;
	pvtRes->clockBiasGalileoCumulated = 0.0;
}


int InitCarrierSmoothing(TNGeneConfiguration *cfg,TPvtData *pvtData,TPvtResults *pvtRes)
{
	pvtData->csmData = (TCarrierSmoothingData *)malloc(cfg->numberOfChannels*sizeof(TCarrierSmoothingData));
	if(pvtData->csmData==NULL)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Malloc returned a NULL pointer in InitPvtData");
		CloseNGene(cfg,EXIT_FAILURE);
	}
	ClearCsmData(cfg,pvtData,pvtRes);
	return SUCCESS_RETURN;
}


void  DeinitCarrierSmoothing(TPvtData *pvtData)
{
	free(pvtData->csmData);
}


void UpdateCarrierSmoothingMeasure(TNGeneConfiguration *cfg,TChannel *ch,TPvtData *pvtData,TPvtResults *pvtRes)
{
	TCarrierSmoothingData *csmData = &(pvtData->csmData[ch->chIndex]);
	double fd;

	/* NB the sign of the Doppler shift must be corrected in function of the used RF front-end
	 *    but the correction of the clock drift MUST not take into account this sign!!! */
	fd = (double)cfg->fePars->dopplerCorrection * ch->dopplerShift;
	fd = fd - (pvtRes->clockDrift / ((double)SPEED_OF_LIGHT/(double)L1_FREQUENCY));
	csmData->csmMeasure += (TWICE_M_PI*fd*(double)ch->blocksize*cfg->ts);
}


void PerformCarrierSmoothing(TNGeneConfiguration *cfg,TPvtData *pvtData,TPvtResults *pvtRes)
{
	int chIdx;
	TPvtChannel *pvtCh;
	TChannel *ch;
	TCarrierSmoothingData *csmData;
	double fdOld;
	double diff;
	double pseudoP;

	/* if carrier smoothing is disabled, returns immediately */
	if(cfg->pvtConfiguration.csmFilterLen<1)
	{
		for(chIdx=0;chIdx<pvtData->numOfOperativeChannels;chIdx++)
		{
			pvtCh = &(pvtData->pvtChArray[chIdx]);
			ch = pvtCh->ch;
			pvtCh->pseudorangeCsm = pvtCh->pseudorange;
			if(pvtData->pvtLogFile!=NULL)
			{
				if(ch->satType==GPS_SAT)
				{
					fprintf(pvtData->pvtLogFile,"[GPS] CHANNEL:%2d  SAT:%2d   pseudorange: %f   smoothed: %f\n",ch->chIndex,ch->satId,pvtCh->pseudorange,pvtCh->pseudorangeCsm);
				}
				else /* if(ch->satType==GALILEO_SAT) */
				{
					fprintf(pvtData->pvtLogFile,"[GAL] CHANNEL:%2d  SAT:%2d   pseudorange: %f   smoothed: %f\n",ch->chIndex,ch->satId,pvtCh->pseudorange,pvtCh->pseudorangeCsm);
				}
				fprintf(pvtData->pvtLogFile,"\n");
			}
		}
		return;
	}

	if((pvtData->pvtAlgorithm==KALMAN_BASED_PVT) && ((pvtData->numOfGalileoOpChannels==0) || (pvtData->numOfGpsOpChannels==0)))
	{
		ClearCsmData(cfg,pvtData,pvtRes);
	}

	for(chIdx=0;chIdx<pvtData->numOfOperativeChannels;chIdx++)
	{
		pvtCh = &(pvtData->pvtChArray[chIdx]);
		ch = pvtCh->ch;
		csmData = &(pvtData->csmData[ch->chIndex]);
		/* NB the sign of the Doppler shift must be corrected in function of the used RF front-end
		 *    but the correction of the clock drift MUST not take into account this sign!!! */
		fdOld = (double)cfg->fePars->dopplerCorrection*ch->dopplerShift;
		fdOld = fdOld - (pvtRes->clockDrift / ((double)SPEED_OF_LIGHT/(double)L1_FREQUENCY));
		/* translate from the value evaluated by the tracking to the value relative to the PVT reference time */
		diff = TWICE_M_PI*fdOld*(double)ch->bufferOffset*cfg->ts;
		csmData->lpc = csmData->csmMeasure-diff;
		/* used for output (analogous to the measure in a Rinex file) */
		csmData->lpcOut = csmData->lpc/TWICE_M_PI;
		/* from radiant to meter */
		csmData->lpc *= ((double)SPEED_OF_LIGHT/((double)L1_FREQUENCY*TWICE_M_PI));
		/* correct bias */
		if(ch->satType==GPS_SAT)
		{
			csmData->lpc += pvtRes->clockBiasGpsCumulated;
		}
		else /* if(ch->satType==GALILEO_SAT) */
		{
			csmData->lpc += pvtRes->clockBiasGalileoCumulated;
		}
		if(pvtData->pvtLogFile!=NULL)
		{
			if(ch->satType==GALILEO_SAT)
			{
				fprintf(pvtData->pvtLogFile,"[GAL] ");
				fprintf(pvtData->pvtLogFile,"CHANNEL:%2d  SAT:%2d      fd=%f    counter=%d    pseudorangeOld=%f\n",ch->chIndex,ch->satId,ch->dopplerShiftOld,csmData->csmCounter,csmData->pseudorangeCsmOld);
				fprintf(pvtData->pvtLogFile,"                              LPC=%f     LPC_OLD=%f     BIAS=%f\n",csmData->lpc,csmData->lpcOld,pvtRes->clockBiasGalileoCumulated);
			}
			else
			{
				fprintf(pvtData->pvtLogFile,"[GPS] ");
				fprintf(pvtData->pvtLogFile,"CHANNEL:%2d  SAT:%2d      fd=%f    counter=%d    pseudorangeOld=%f\n",ch->chIndex,ch->satId,ch->dopplerShiftOld,csmData->csmCounter,csmData->pseudorangeCsmOld);
				fprintf(pvtData->pvtLogFile,"                              LPC=%f     LPC_OLD=%f     BIAS=%f\n",csmData->lpc,csmData->lpcOld,pvtRes->clockBiasGpsCumulated);
			}
		}
		/* perform smoothing */
		pvtCh->pseudorangeCsm = pvtCh->pseudorange;
		pseudoP = 0.0;
		if(csmData->csmCounter>1)
		{
			pseudoP = csmData->pseudorangeCsmOld - (csmData->lpc - csmData->lpcOld);
			pvtCh->pseudorangeCsm = (pvtCh->pseudorange*pvtData->weightCode)+(pseudoP*pvtData->weightCarrier);
		}
		csmData->pseudorangeCsmOld = pvtCh->pseudorangeCsm;
		csmData->lpcOld = csmData->lpc;
		csmData->csmCounter++;
		/* update the CSM  */
		/* set the posSigma for Kalman filter */
		if((pvtData->csmTransient==FALSE_FLAG) && (csmData->csmCounter<CSM_TRANSIENT_LENGTH))
		{
			pvtCh->posSigma2 = KF_POS_SIGMA2_HIGH_VALUE;
		}
		else
		{
			pvtCh->posSigma2 = KF_POS_SIGMA2_LOW_VALUE;
		}
		if((pvtData->csmTransient==TRUE_FLAG) && ((pvtData->pvtIndex-pvtData->csmTransientStart)>CSM_TRANSIENT_LENGTH))
		{
			pvtData->csmTransient = FALSE_FLAG;
		}
		/* log */
		if(pvtData->pvtLogFile!=NULL)
		{
			fprintf(pvtData->pvtLogFile,"                              pseudoP: %f   pseudorange: %f   smoothed: %f\n",pseudoP,pvtCh->pseudorange,pvtCh->pseudorangeCsm);
		}
	}
	if(pvtData->pvtLogFile!=NULL)
	{
		fprintf(pvtData->pvtLogFile,"\n");
	}
}


void UpdateCumulatedBiases(TPvtResults *pvtRes)
{
	pvtRes->clockBiasGpsCumulated += pvtRes->clockBiasGps;
	pvtRes->clockBiasGalileoCumulated += pvtRes->clockBiasGalileo;
}
