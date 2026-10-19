/**
 * @file egnosCorrections.c
 *
 * @author Mario Nicola
 *
 * @date 06/08/2015
 *
 * @version 1.0.0
 *
 * @brief Functions used to manage the EGNOS corrections.
 */

#include "ngeneConstants.h"
#include "timeConstants.h"
#include "egnosCorrectionsDataStructure.h"
#include "channelDataStructure.h"
#include "ngeneConfiguration.h"

/* variables used during EGNOS corrections evaluations */
static const double egnosUdreiEvaluationTable[2][16] =
{
		{0.75, 1.0, 1.25, 1.75, 2.25, 3.0, 3.75, 4.5, 5.25, 6.0, 7.5, 15.0, 50.0, 150.0, -1, -1},
		{0.0520, 0.0924, 0.1444, 0.2830, 0.4678, 0.8315, 1.2992, 1.8709, 2.5465, 3.3260, 5.1968, 20.7870, 230.9661, 2078.695, -1, -1}
};

static const double egnosFastCorrectionDegradTable[4][16] =
{
		{0.00000, 0.00005, 0.00009, 0.00012, 0.00015, 0.00020, 0.00030, 0.00045, 0.00060, 0.00090, 0.00150, 0.00210, 0.00270, 0.00330, 0.00460, 0.00580},
		{180, 180, 153, 135, 135, 117, 99, 81, 63, 45, 45, 27, 27, 27, 18, 18},
		{120, 120, 102, 90, 90, 78, 66, 54, 42, 30, 30, 18, 18, 18, 12, 12},
		{60, 60, 51, 45, 45, 39, 33, 27, 21, 15, 15, 9, 9, 9, 6, 6}
};

static const double egnosGiveiEvaluationTable[1][16] =
{
		{0.0084, 0.0333, 0.0749, 0.1331, 0.2079, 0.2994, 0.4075, 0.5322, 0.6735, 0.8315, 1.1974, 1.8709, 3.3260, 20.7870, 187.0826, -1}
};



/* functions */
int InitEgnosCorrections(TNGeneConfiguration *cfg,TEgnosCorrections *egnosCorr)
{
	int idx;
	TEgnosFastCorrection *fastCorr;
	TEgnosIntegrityInformation *integrity;
	TEgnosFastCorrectionDegradationFactor *fastDegFact;
	TEgnosDegradationFactors *degFact;
	TEgnosLongTermCorrections *longTermCorr;
	int bandIdx;
	int pointIdx;
	TEgnosIonoMask *ionoMask;
	TEgnosIonoIgpDelayCorrections *ionoCorr;
	char filename[2*MAX_ROW_LENGTH];

	egnosCorr->maskIodp = INVALID_VALUE;
	egnosCorr->numOfPrnsInTheMask = 0;
	/* degradation factors */
	degFact = &(egnosCorr->degradationFactors);
	degFact->valid = FALSE_FLAG;
	degFact->bRrc = 0.0;
	degFact->cLtcLsb = 0.0;
	degFact->cLtcV1 = 0.0;
	degFact->iLtcV1 = 0;
	degFact->cLtcV0 = 0.0;
	degFact->iLtcV0 = 0;
	degFact->cGeoLsb = 0.0;
	degFact->cGeoV = 0.0;
	degFact->iGeo = 0;
	degFact->cEr = 0.0;
	degFact->cIonoStep = 0.0;
	degFact->iIono = 0;
	degFact->cIonoRamp = 0.0;
	degFact->rssUdre = 0;
	degFact->rssIono = 0;
	degFact->cCovariance = 0.0;
	/* arrays of corrections */
	for(idx=0;idx<EGNOS_ENTRIES;idx++)
	{
		egnosCorr->prnMask[idx].satId = INVALID_VALUE;
		egnosCorr->prnMask[idx].satType = INVALID_EGNOS_CONSTELLATION;
		/* fast corrections */
		fastCorr = &(egnosCorr->fastCorrections[idx]);
		fastCorr->iodp = INVALID_VALUE;
		fastCorr->iodf = INVALID_VALUE;
		fastCorr->valid = FALSE_FLAG;
		fastCorr->prc = 0.0;
		fastCorr->udrei = 0;
		fastCorr->towOld = INVALID_VALUE;
		fastCorr->towOldValid = FALSE_FLAG;
		fastCorr->rrc = 0.0;
		fastCorr->udreiMeters = 0.0;
		fastCorr->sigma2Udre = 0.0;
		fastCorr->monitored = FALSE_FLAG;
		fastCorr->toBeUsed = FALSE_FLAG;
		/* integrity information */
		integrity = &(egnosCorr->integrityInformation[idx]);
		integrity->iodf = INVALID_VALUE;
		integrity->valid = FALSE_FLAG;
		integrity->udrei = 0;
		integrity->udreiMeters = 0.0;
		integrity->sigma2Udre = 0.0;
		integrity->monitored = FALSE_FLAG;
		integrity->toBeUsed = FALSE_FLAG;
		/* fast correction degradation factors */
		fastDegFact = &(egnosCorr->fastDegradationFactors[idx]);
		fastDegFact->iodp = INVALID_VALUE;
		fastDegFact->valid = FALSE_FLAG;
		fastDegFact->tLat = 0;
		fastDegFact->ai = 0;
		fastDegFact->a = 0.0;
		fastDegFact->ifc = 0;
		fastDegFact->ifcLp = 0;
		fastDegFact->updateInterval = 0;
		/* long term satellite error correction */
		longTermCorr = &(egnosCorr->longTermCorrections[idx]);
		longTermCorr->iodp = INVALID_VALUE;
		longTermCorr->validVelocityCode0 = FALSE_FLAG;
		longTermCorr->validVelocityCode1 = FALSE_FLAG;
		longTermCorr->iod = INVALID_VALUE;
		longTermCorr->deltaX = 0.0;
		longTermCorr->deltaY = 0.0;
		longTermCorr->deltaZ = 0.0;
		longTermCorr->deltaAf0 = 0.0;
		longTermCorr->rateDeltaX = 0.0;
		longTermCorr->rateDeltaY = 0.0;
		longTermCorr->rateDeltaZ = 0.0;
		longTermCorr->deltaAf1 = 0.0;
		longTermCorr->to = INVALID_VALUE;
	}
	/* iono model */
	egnosCorr->numOfIonoBands = 0;
	egnosCorr->numOfReceivedIonoBands = 0;
	for(bandIdx=0;bandIdx<EGNOS_IONO_BANDS;bandIdx++)
	{
		ionoMask = &(egnosCorr->ionoMask[bandIdx]);
		ionoMask->maskIodi = INVALID_VALUE;
		ionoMask->numOfPointInTheMask = 0;
		for(pointIdx=0;pointIdx<EGNOS_IONO_POINTS;pointIdx++)
		{
			ionoCorr = &(egnosCorr->bandIonoCorrections[bandIdx].igpIonoCorrections[pointIdx]);
			ionoCorr->iodi = INVALID_VALUE;
			ionoCorr->valid = FALSE_FLAG;
			ionoCorr->igpVerticalDelay = 0.0;
			ionoCorr->usable = FALSE_FLAG;
			ionoCorr->igpGivei = 0;
			ionoCorr->giveMeters = 0.0;
			ionoCorr->sigma2Give = 0.0;
			ionoCorr->monitored = FALSE_FLAG;
		}
	}
	//
	if((cfg->pvtConfiguration.egnosFlag==FALSE_FLAG) || (cfg->logConfiguration.enablePvtTextLog==FALSE_FLAG))
	{
		egnosCorr->ionoDebugFile = NULL;
	}
	else
	{
		sprintf(filename,"%s/egnosIonoCorrectionsDebug.txt",cfg->dumpPathCurrent);
		egnosCorr->ionoDebugFile = fopen(filename,"w");
	}
	return SUCCESS_RETURN;
}


void DeinitEgnosCorrections(TEgnosCorrections *egnosCorr)
{
	if(egnosCorr->ionoDebugFile!=NULL)
	{
		fclose(egnosCorr->ionoDebugFile);
		egnosCorr->ionoDebugFile = NULL;
	}
}


/* TODO Salvare la vecchia maschera/vecchie correzioni quando cambia IODP?
 * Qui quando cambia maschera, le vecchie correzioni vengono tutte scartate.
 * Se si salvassero due maschere (vecchia e nuova) e due set di correzioni
 * (vecchie e nuove) si potrebbero continuare ad usare le correzioni vecchie
 * finchè non arrivano quelle nuove. Ne vale la pena?
 */
void CleanEgnosCorrections(TEgnosCorrections *egnosCorr)
{
	int idx;
	TEgnosFastCorrection *fastCorr;
	TEgnosIntegrityInformation *integrity;
	TEgnosFastCorrectionDegradationFactor *fastDegFact;
	TEgnosLongTermCorrections *longTermCorr;

	for(idx=0;idx<EGNOS_ENTRIES;idx++)
	{
		/* fast corrections */
		fastCorr = &(egnosCorr->fastCorrections[idx]);
		if((fastCorr->iodp!=egnosCorr->maskIodp) || (idx>=egnosCorr->numOfPrnsInTheMask))
		{
			fastCorr->iodp = INVALID_VALUE;
			fastCorr->iodf = INVALID_VALUE;
			fastCorr->valid = FALSE_FLAG;
			fastCorr->prc = 0.0;
			fastCorr->udrei = 0;
			fastCorr->towOld = INVALID_VALUE;
			fastCorr->towOldValid = FALSE_FLAG;
			fastCorr->rrc = 0.0;
			fastCorr->udreiMeters = 0.0;
			fastCorr->sigma2Udre = 0.0;
			fastCorr->monitored = FALSE_FLAG;
			fastCorr->toBeUsed = FALSE_FLAG;
		}
		/* integrity information */
		integrity = &(egnosCorr->integrityInformation[idx]);
		integrity->iodf = INVALID_VALUE;
		integrity->valid = FALSE_FLAG;
		integrity->udrei = 0;
		integrity->udreiMeters = 0.0;
		integrity->sigma2Udre = 0.0;
		integrity->monitored = FALSE_FLAG;
		integrity->toBeUsed = FALSE_FLAG;
		/* fast correction degradation factors */
		fastDegFact = &(egnosCorr->fastDegradationFactors[idx]);
		if((fastDegFact->iodp!=egnosCorr->maskIodp) || (idx>=egnosCorr->numOfPrnsInTheMask))
		{
			fastDegFact->iodp = INVALID_VALUE;
			fastDegFact->valid = FALSE_FLAG;
			fastDegFact->tLat = 0;
			fastDegFact->ai = 0;
			fastDegFact->a = 0.0;
			fastDegFact->ifc = 0;
			fastDegFact->ifcLp = 0;
			fastDegFact->updateInterval = 0;
		}
		/* long term satellite error correction */
		longTermCorr = &(egnosCorr->longTermCorrections[idx]);
		if((longTermCorr->iodp!=egnosCorr->maskIodp) || (idx>=egnosCorr->numOfPrnsInTheMask))
		{
			longTermCorr->iodp = INVALID_VALUE;
			longTermCorr->validVelocityCode0 = FALSE_FLAG;
			longTermCorr->validVelocityCode1 = FALSE_FLAG;
			longTermCorr->iod = INVALID_VALUE;
			longTermCorr->deltaX = 0.0;
			longTermCorr->deltaY = 0.0;
			longTermCorr->deltaZ = 0.0;
			longTermCorr->deltaAf0 = 0.0;
			longTermCorr->rateDeltaX = 0.0;
			longTermCorr->rateDeltaY = 0.0;
			longTermCorr->rateDeltaZ = 0.0;
			longTermCorr->deltaAf1 = 0.0;
			longTermCorr->to = INVALID_VALUE;
		}
	}
}

void CleanEgnosIonoCorrections(TEgnosCorrections *egnosCorr,int bandNumber)
{
	char maskIodi;
	int numPoints;
	int idx;
	TEgnosIonoIgpDelayCorrections *ionoCorr;

	maskIodi = egnosCorr->ionoMask[bandNumber].maskIodi;
	numPoints = egnosCorr->ionoMask[bandNumber].numOfPointInTheMask;

	for(idx=0;idx<EGNOS_IONO_POINTS;idx++)
	{
		ionoCorr = &(egnosCorr->bandIonoCorrections[bandNumber].igpIonoCorrections[idx]);
		if((ionoCorr->iodi!=maskIodi) || (idx>=numPoints))
		{
			ionoCorr->iodi = INVALID_VALUE;
			ionoCorr->valid = FALSE_FLAG;
			ionoCorr->igpVerticalDelay = 0.0;
			ionoCorr->igpGivei = 0;
			ionoCorr->usable = FALSE_FLAG;
			ionoCorr->giveMeters = 0.0;
			ionoCorr->sigma2Give = 0.0;
			ionoCorr->monitored = FALSE_FLAG;
		}
	}
}


void CompleteUdreiInformation(unsigned int udrei,double *udreiMeters,double *sigma2Udre,unsigned char *monitored,unsigned char *toBeUsed)
{
	if(udrei>=14)
	{
		*monitored = FALSE_FLAG;
		*toBeUsed = FALSE_FLAG;
		*udreiMeters = 0.0;
		*sigma2Udre = 0.0;
	}
	else
	{
		*monitored = TRUE_FLAG;
		*toBeUsed = TRUE_FLAG;
		*udreiMeters = egnosUdreiEvaluationTable[0][udrei];
		*sigma2Udre = egnosUdreiEvaluationTable[1][udrei];
	}
}


void CompleteFastDegradationFactorInformation(TEgnosFastCorrectionDegradationFactor *fastDegFact)
{
	if(fastDegFact->valid==FALSE_FLAG)
		return;

	fastDegFact->a = egnosFastCorrectionDegradTable[0][fastDegFact->ai];
	fastDegFact->ifc = (int)egnosFastCorrectionDegradTable[1][fastDegFact->ai];
	fastDegFact->ifcLp = (int)egnosFastCorrectionDegradTable[2][fastDegFact->ai];
	fastDegFact->updateInterval = (int)egnosFastCorrectionDegradTable[3][fastDegFact->ai];
}

void CompleteGiveInformation(TEgnosIonoIgpDelayCorrections *ionoCorr)
{
	if(ionoCorr->valid==FALSE_FLAG)
		return;

	if(ionoCorr->igpGivei>=15)
	{
		ionoCorr->monitored = FALSE_FLAG;
		ionoCorr->giveMeters = 0.0;
		ionoCorr->sigma2Give = 0.0;
	}
	else
	{
		ionoCorr->monitored = TRUE_FLAG;
		ionoCorr->giveMeters = ((double)ionoCorr->igpGivei+1.0) * 0.3;
		ionoCorr->sigma2Give = egnosGiveiEvaluationTable[0][ionoCorr->igpGivei];
	}
}


void SetEgnosPrnMaskIndex(int nChannels,TChannel *chArray,TEgnosCorrections *egnosCorr)
{
	int chIdx;
	TChannel *ch;
	int maskIdx;
	TEgnosMaskElement *maskElement;

	for(chIdx=0;chIdx<nChannels;chIdx++)
	{
		ch=&(chArray[chIdx]);
		ch->egnosPrnMaskIndex=INVALID_VALUE;
		if((ch->chState==TRACKING_STATE) && (ch->satType==GPS_SAT))
		{
			maskIdx=0;
			while((maskIdx<egnosCorr->numOfPrnsInTheMask) && (ch->egnosPrnMaskIndex==INVALID_VALUE))
			{
				maskElement = &(egnosCorr->prnMask[maskIdx]);
				if((maskElement->satType==GPS_PRN) && (maskElement->satId==ch->satId))
				{
					ch->egnosPrnMaskIndex=maskIdx;
				}
				maskIdx++;
			}
		}
		/* consider only GPS satellites (for now) */
	}
}


double GetEgnosFastCorrection(TNGeneConfiguration *cfg,int maskIndex,TEgnosCorrections *egnosCorr,double rxTime)
{
	TEgnosFastCorrection *fastCorr;
	double correction = 0.0;
	double deltaTime;

	/* use of EGNOS is disabled */
	if(cfg->pvtConfiguration.egnosFlag==FALSE_FLAG)
	{
		return 0.0;
	}
	/* invalid index */
	if((maskIndex==INVALID_VALUE) || (maskIndex>=egnosCorr->numOfPrnsInTheMask))
	{
		return 0.0;
	}
	fastCorr = &(egnosCorr->fastCorrections[maskIndex]);
	/* fast correction not received */
	if(fastCorr->towOldValid==FALSE_FLAG)
	{
		return 0.0;
	}
	/* correction and mask are not coherent */
	if(egnosCorr->maskIodp!=fastCorr->iodp)
	{
		return 0.0;
	}
	/* get the correction */
	correction = fastCorr->prc;
	if(fastCorr->valid==TRUE_FLAG)
	{
		/* also RRC is valid */
		deltaTime = rxTime-fastCorr->towOld;
		if(deltaTime<0.0)
		{
			/* change of week */
			deltaTime += SECONDS_IN_A_WEEK;
		}
		correction += (fastCorr->rrc*deltaTime);
	}
	return correction;
}

/* TODO Uso delle correzioni long term in presenza di un cambio di IODE/IODC
 * In PROSA vengono salvate le effemeridi/correzioni del clock vecchie quando c'è un cambio di IODE/IODC
 * in modo da poter continuare ad usare le correzioni long
 * (vengono selezionati le effemeridi /correzioni del clock in funzione dell'IOD trasmesso da EGNOS).
 * Qui invece vengono disabilitate le correzioni fintanto che non arrivano quelle giuste.
 * Introdurre il meccanismo di PROSA?
 */

double GetEgnosLongTermClockCorrection(TNGeneConfiguration *cfg,int iodc,int maskIndex,TEgnosCorrections *egnosCorr,double txTime)
{
	TEgnosLongTermCorrections *longTermCorr;
	double correction = 0.0;
	double deltaTime;

	/* use of EGNOS is disabled */
	if(cfg->pvtConfiguration.egnosFlag==FALSE_FLAG)
	{
		return 0.0;
	}
	/* invalid index */
	if((maskIndex==INVALID_VALUE) || (maskIndex>=egnosCorr->numOfPrnsInTheMask))
	{
		return 0.0;
	}
	longTermCorr = &(egnosCorr->longTermCorrections[maskIndex]);
	/* long term corrections not received */
	if((longTermCorr->validVelocityCode0==FALSE_FLAG) && (longTermCorr->validVelocityCode1==FALSE_FLAG))
	{
		return 0.0;
	}
	/* correction and mask are not coherent */
	if(egnosCorr->maskIodp!=longTermCorr->iodp)
	{
		return 0.0;
	}
	/* Issue Of Data is not coherent */
	if(longTermCorr->iod!=(iodc & 0x00ff))
	{
		return 0.0;
	}
	/* get the correction */
	correction = longTermCorr->deltaAf0;
	if(longTermCorr->validVelocityCode1==TRUE_FLAG)
	{
		/* also af1 is valid */
		deltaTime = txTime-longTermCorr->to;
		if(deltaTime<0.0)
		{
			/* change of week */
			deltaTime += SECONDS_IN_A_WEEK;
		}
		correction += (longTermCorr->deltaAf1*deltaTime);
	}
	return correction;
}

void GetEgnosLongTermPositionCorrection(TNGeneConfiguration *cfg,int iode,int maskIndex,TEgnosCorrections *egnosCorr,double txTime,double *dx,double *dy,double *dz)
{
	TEgnosLongTermCorrections *longTermCorr;
	double correctionX = 0.0;
	double correctionY = 0.0;
	double correctionZ = 0.0;
	double deltaTime;

	*dx = 0.0;
	*dy = 0.0;
	*dz = 0.0;
	/* use of EGNOS is disabled */
	if(cfg->pvtConfiguration.egnosFlag==FALSE_FLAG)
	{
		return;
	}
	/* invalid index */
	if((maskIndex==INVALID_VALUE) || (maskIndex>=egnosCorr->numOfPrnsInTheMask))
	{
		return;
	}
	longTermCorr = &(egnosCorr->longTermCorrections[maskIndex]);
	/* long term corrections not received */
	if((longTermCorr->validVelocityCode0==FALSE_FLAG) && (longTermCorr->validVelocityCode1==FALSE_FLAG))
	{
		return;
	}
	/* correction and mask are not coherent */
	if(egnosCorr->maskIodp!=longTermCorr->iodp)
	{
		return;
	}
	/* Issue Of Data is not coherent */
	if(longTermCorr->iod!=iode)
	{
		return;
	}
	/* get the correction */
	correctionX = longTermCorr->deltaX;
	correctionY = longTermCorr->deltaY;
	correctionZ = longTermCorr->deltaZ;
	if(longTermCorr->validVelocityCode1==TRUE_FLAG)
	{
		/* also rates are valid */
		deltaTime = txTime-longTermCorr->to;
		if(deltaTime<0.0)
		{
			/* change of week */
			deltaTime += SECONDS_IN_A_WEEK;
		}
		correctionX += (longTermCorr->rateDeltaX*deltaTime);
		correctionY += (longTermCorr->rateDeltaY*deltaTime);
		correctionZ += (longTermCorr->rateDeltaZ*deltaTime);
	}
	*dx = correctionX;
	*dy = correctionY;
	*dz = correctionZ;
}

