/**
 * @file pvt.c
 *
 * @author Mario Nicola
 *
 * @date 22/10/2014
 *
 * @version 1.0.0
 *
 * @brief Main functions used for PVT computation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "ngeneDataStructure.h"
#include "ngeneConfiguration.h"
#include "pvtDataStructure.h"
#include "ngeneConstants.h"
#include "ioFunctions.h"
#include "closeManager.h"
#include "pvtChecks.h"
#include "pvtUtility.h"
#include "pvtConstants.h"
#include "leastSquares.h"
#include "kalmanFilter.h"
#include "dop.h"
#include "receiverTime.h"
#include "timeConstants.h"
#include "timeConversions.h"
#include "carrierSmoothing.h"
#include "nequick.h"
#include "egnosCorrections.h"
#include "egnosIonoCorrections.h"
#include "rinexLog.h"
#include "nmeaLog.h"
#include "channelManager.h"


/**
 * @def SAVE_BAD_PVT_RESULTS
 * @brief If defined, the results of PVT are saved in the optional text and binary logs
 *        when the PVT results are discarded as not good. Enable for debug.
 */
#define SAVE_BAD_PVT_RESULTS


void InitOperativeChannels(TNGeneConfiguration *cfg,TPvtData *pvtData)
{
	int idx;
	TPvtChannel *pvtCh;

	for(idx=0;idx<cfg->numberOfChannels;idx++)
	{
		pvtCh = &(pvtData->pvtChArray[idx]);
		pvtCh->ch = NULL;
		pvtCh->satStatus = NULL;
		pvtCh->wn = 0;
		pvtCh->transmissionTime = 0.0;
		pvtCh->satClkCorr = 0.0;
		pvtCh->deltaPseudorange = 0.0;
		pvtCh->pseudorangeRaw = 0.0;
		pvtCh->pseudorange = 0.0;
		pvtCh->pseudorangeCsm = 0.0;
		pvtCh->tropoCorrection = 0.0;
		pvtCh->ionoCorrection = 0.0;
		pvtCh->posSigma2 = KF_POS_SIGMA2_LOW_VALUE;
		pvtCh->velSigma2 = KF_VEL_SIGMA2;
		pvtCh->ionoCorrectionFromEgnos=FALSE_FLAG;
		pvtCh->egnosFastCorrection=0.0;
		pvtCh->egnosClockCorr=0.0;
		pvtCh->egnosXCorr=0.0;
		pvtCh->egnosYCorr=0.0;
		pvtCh->egnosZcorr=0.0;
		pvtCh->pseudorangeResidual = 0.0;
	}
}


void DeleteOperativeChannel(TPvtData *pvtData,int chIdx)
{
	TChannel *ch;
	TPvtChannel *pvtCh;
	int srcIdx;

	pvtCh = &(pvtData->pvtChArray[chIdx]);
	ch = pvtCh->ch;

	srcIdx=chIdx+1;
	for(;srcIdx<pvtData->numOfOperativeChannels;srcIdx++)
	{
		pvtData->pvtChArray[chIdx] = pvtData->pvtChArray[srcIdx];
		chIdx++;
	}
	if(ch->satType==GPS_SAT)
	{
		pvtData->numOfGpsOpChannels--;
	}
	else /* if(ch->satType==GALILEO_SAT) */
	{
		pvtData->numOfGalileoOpChannels--;
	}
	pvtData->numOfOperativeChannels--;
}


int InitPvtData(TNGeneConfiguration *cfg,TNGeneData *rx)
{
	TPvtData *pvtData = &(rx->pvtData);
	TPvtResults *pvtRes = &(rx->pvtResults);
	TPvtResults *pvtTmpRes = &(rx->pvtTemporaryResults);
	int index;
	char filename[3*MAX_ROW_LENGTH];

	pvtData->pvtChArray = (TPvtChannel *)malloc(cfg->numberOfChannels*sizeof(TPvtChannel));
	if(pvtData->pvtChArray==NULL)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Malloc returned a NULL pointer in InitPvtData");
		CloseNGene(cfg,EXIT_FAILURE);
	}
	pvtData->pvtIndex=0;
	pvtData->lsIndex=0;
	pvtData->pvtAlgorithm=LMS_BASED_PVT;
	pvtData->rxTimeForNextPvtFlag=INVALID_TIME;
	pvtData->numOfPvtFails = 0;
	pvtData->hardRestartMade = FALSE_FLAG;
	pvtData->numOfGpsOpChannels = 0;
	pvtData->numOfGalileoOpChannels = 0;
	pvtData->numOfOperativeChannels = 0;
	pvtData->numOfChannelsReadyForPVT = 0;
	pvtData->numOfGpsChannelsReadyForPVT = 0;
	pvtData->numOfGalileoChannelsReadyForPVT = 0;
	pvtData->rxGpsTimeValid=FALSE_FLAG;
	pvtData->rxGalTimeValid=FALSE_FLAG;
	pvtData->rxUtcTimeValid=FALSE_FLAG;
	InitOperativeChannels(cfg,pvtData);
	InitLeastSquaresData(cfg,rx);
	InitKalmanFilterData(cfg,rx);
	InitCarrierSmoothing(cfg,pvtData,pvtRes);
	InitNeQuickModel(cfg,&(pvtData->nequick));
	pvtData->nequickCorrections = (double *)malloc((GPS_NUMBER_OF_PRNS + GALILEO_NUMBER_OF_PRNS)*sizeof(double));
	pvtData->nequickCorrectionsTime = (double *)malloc((GPS_NUMBER_OF_PRNS + GALILEO_NUMBER_OF_PRNS)*sizeof(double));
	if((pvtData->nequickCorrections==NULL) || (pvtData->nequickCorrectionsTime==NULL))
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Malloc returned a NULL pointer in InitPvtData");
		CloseNGene(cfg,EXIT_FAILURE);
	}
	for(index=0;index<(GPS_NUMBER_OF_PRNS + GALILEO_NUMBER_OF_PRNS);index++)
	{
		pvtData->nequickCorrections[index] = 0.0;
		pvtData->nequickCorrectionsTime[index] = -NEQUICK_RESULTS_UPDATE_INTERVAL;
	}
	/* Initialize PVT results */
	pvtRes->pvtValid=FALSE_FLAG;
	pvtRes->pvtPlotted=FALSE_FLAG;
	pvtRes->clockBiasGpsValid=FALSE_FLAG;
	pvtRes->clockBiasGalileo=FALSE_FLAG;
	pvtRes->rxGpsTime.referenceValid=FALSE_FLAG;
	pvtRes->rxGpsTime.referenceUpdated=FALSE_FLAG;
	pvtRes->rxGalTime.referenceValid=FALSE_FLAG;
	pvtRes->rxGalTime.referenceUpdated=FALSE_FLAG;
	pvtRes->utcFromGps=FALSE_FLAG;
	pvtRes->utcFromGal=FALSE_FLAG;
	pvtRes->utcFromDefault=FALSE_FLAG;
	pvtRes->rxUtcTime.referenceValid=FALSE_FLAG;
	pvtRes->rxUtcTime.referenceUpdated=FALSE_FLAG;
	pvtRes->clockBiasGpsCumulated=0.0;
	pvtRes->clockBiasGalileoCumulated=0.0;
	pvtRes->gpsSatStatus = (TSatStatus *)malloc(GPS_NUMBER_OF_PRNS*sizeof(TSatStatus));
	pvtRes->galSatStatus = (TSatStatus *)malloc(GALILEO_NUMBER_OF_PRNS*sizeof(TSatStatus));
	if((pvtRes->gpsSatStatus==NULL) || (pvtRes->galSatStatus==NULL))
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Malloc returned a NULL pointer in InitPvtData");
		CloseNGene(cfg,EXIT_FAILURE);
	}
	for(index=0;index<GPS_NUMBER_OF_PRNS;index++)
	{
		pvtRes->gpsSatStatus[index].satInformationValid=FALSE_FLAG;
	}
	for(index=0;index<GALILEO_NUMBER_OF_PRNS;index++)
	{
		pvtRes->galSatStatus[index].satInformationValid=FALSE_FLAG;
	}
	if(cfg->googleEarthFilenameValid==TRUE_FLAG)
	{
		sprintf(filename,"%s/%s",cfg->dumpPathCurrent,cfg->googleEarthFilename);
		pvtRes->kmlFile = fopen(filename,"w");
		fprintf(pvtRes->kmlFile,"<?xml version='1.0' encoding='UTF-8'?>\n");
		fprintf(pvtRes->kmlFile,"<kml xmlns='http://earth.google.com/kml/2.1'>\n");
		fprintf(pvtRes->kmlFile,"<Document>\n");
		fprintf(pvtRes->kmlFile,"<Style id='defaultPlacemark'>\n");
		fprintf(pvtRes->kmlFile,"<IconStyle>\n");
		fprintf(pvtRes->kmlFile,"<Icon>\n");
		fprintf(pvtRes->kmlFile,"<href>http://maps.google.com/mapfiles/kml/pal4/icon26.png</href>\n");
		fprintf(pvtRes->kmlFile,"</Icon>\n");
		fprintf(pvtRes->kmlFile,"</IconStyle>\n");
		fprintf(pvtRes->kmlFile,"</Style>\n");
		#ifdef LINESTRING_KML
		fprintf(pvtRes->kmlFile,"<Placemark id='track'>\n");
		fprintf(pvtRes->kmlFile,"<styleUrl>#defaultPlacemark</styleUrl>\n");
		fprintf(pvtRes->kmlFile, "<LineString>\n");
		fprintf(pvtRes->kmlFile, "<coordinates>\n");
		#endif
	}
	/* Initialize the structure for temporary PVT results */
	pvtTmpRes->pvtValid=FALSE_FLAG;
	pvtTmpRes->pvtPlotted=FALSE_FLAG;
	pvtTmpRes->clockBiasGpsValid=FALSE_FLAG;
	pvtTmpRes->clockBiasGalileo=FALSE_FLAG;
	pvtTmpRes->rxGpsTime.referenceValid=FALSE_FLAG;
	pvtTmpRes->rxGalTime.referenceValid=FALSE_FLAG;
	pvtTmpRes->utcFromGps=FALSE_FLAG;
	pvtTmpRes->utcFromGal=FALSE_FLAG;
	pvtTmpRes->utcFromDefault=FALSE_FLAG;
	pvtTmpRes->rxUtcTime.referenceValid=FALSE_FLAG;
	pvtTmpRes->clockBiasGpsCumulated=0.0;
	pvtTmpRes->clockBiasGalileoCumulated=0.0;
	pvtTmpRes->gpsSatStatus = (TSatStatus *)malloc(GPS_NUMBER_OF_PRNS*sizeof(TSatStatus));
	pvtTmpRes->galSatStatus = (TSatStatus *)malloc(GALILEO_NUMBER_OF_PRNS*sizeof(TSatStatus));
	if((pvtTmpRes->gpsSatStatus==NULL) || (pvtTmpRes->galSatStatus==NULL))
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Malloc returned a NULL pointer in InitPvtData");
		CloseNGene(cfg,EXIT_FAILURE);
	}
	for(index=0;index<GPS_NUMBER_OF_PRNS;index++)
	{
		pvtTmpRes->gpsSatStatus[index].satInformationValid=FALSE_FLAG;
	}
	for(index=0;index<GALILEO_NUMBER_OF_PRNS;index++)
	{
		pvtTmpRes->galSatStatus[index].satInformationValid=FALSE_FLAG;
	}
	pvtTmpRes->kmlFile = NULL;
	/**/
	if(cfg->logConfiguration.enablePvtTextLog==TRUE_FLAG)
	{
		sprintf(filename,"%s/pvtLog.txt",cfg->dumpPathCurrent);
		pvtData->pvtLogFile = fopen(filename,"w");
	}
	else
	{
		pvtData->pvtLogFile = NULL;
	}
	if(cfg->logConfiguration.enablePvtBinLog==TRUE_FLAG)
	{
		sprintf(filename,"%s/pvtLog.bin",cfg->dumpPathCurrent);
		pvtData->pvtBinFile = fopen(filename,"wb");
	}
	else
	{
		pvtData->pvtBinFile = NULL;
	}
	if(cfg->logConfiguration.enableGgtoLog==TRUE_FLAG)
	{
		sprintf(filename,"%s/gnssTimeLog.txt",cfg->dumpPathCurrent);
		pvtData->gnssTimeLogFile = fopen(filename,"w");
	}
	else
	{
		pvtData->gnssTimeLogFile = NULL;
	}
	return SUCCESS_RETURN;
}


void DeinitPvtData(TNGeneData *rx)
{
	TPvtData *pvtData = &(rx->pvtData);
	TPvtResults *pvtRes = &(rx->pvtResults);
	TPvtResults *pvtTmpRes = &(rx->pvtTemporaryResults);

	free(pvtData->pvtChArray);
	free(pvtRes->gpsSatStatus);
	free(pvtRes->galSatStatus);
	free(pvtTmpRes->gpsSatStatus);
	free(pvtTmpRes->galSatStatus);
	free(pvtData->nequickCorrections);
	free(pvtData->nequickCorrectionsTime);
	DeinitLeastSquaresData(rx);
	DeinitKalmanFilterData(rx);
	DeinitCarrierSmoothing(pvtData);
	DeinitNeQuickModel(&(pvtData->nequick));
	if(pvtRes->kmlFile!=NULL)
	{
		#ifdef LINESTRING_KML
		fprintf(pvtRes->kmlFile, "</coordinates>\n");
		fprintf(pvtRes->kmlFile, "</LineString>\n");
	    fprintf(pvtRes->kmlFile,"</Placemark>\n\n\n");
	    fprintf(pvtRes->kmlFile, "\n\n</Document>\n");
	    #else
		fprintf(pvtRes->kmlFile, "</Document>\n");
		#endif
		fprintf(pvtRes->kmlFile, "</kml>\n");
		fclose(pvtRes->kmlFile);
	}
	if(pvtData->pvtLogFile!=NULL)
	{
		fclose(pvtData->pvtLogFile);
		pvtData->pvtLogFile=NULL;
	}
	if(pvtData->pvtBinFile!=NULL)
	{
		fclose(pvtData->pvtBinFile);
		pvtData->pvtBinFile=NULL;
	}
	if(pvtData->gnssTimeLogFile!=NULL)
	{
		fclose(pvtData->gnssTimeLogFile);
		pvtData->gnssTimeLogFile=NULL;
	}
}


void ResetPvtInformationForChannels(TNGeneConfiguration *cfg,TNGeneData *rx)
{
	int index;

	for(index=0;index<cfg->numberOfChannels;index++)
	{
		rx->channelArray[index].pvtInfo.usedForPVT = FALSE_FLAG;
	}
}


void ResetPvt(TNGeneConfiguration *cfg,TPvtData *pvtData,TPvtResults *pvtRes, int hardReset)
{
	int index;

	pvtData->lsIndex=0;
	pvtData->pvtAlgorithm=LMS_BASED_PVT;
	ClearCsmData(cfg,pvtData,pvtRes);
	InitQStateNoiseCovarianceMatrices(pvtData->kfData.q,pvtData->kfData.qMulti,cfg->pvtPeriodSeconds);
	// end here if we are not performing a hard reset
	if(hardReset==FALSE_FLAG)
	{
		return;
	}
	// other variables in pvtData
	pvtData->lsIndex=0;
	pvtData->pvtAlgorithm=LMS_BASED_PVT;
	pvtData->numOfPvtFails = 0;
	pvtData->hardRestartMade = FALSE_FLAG;
	pvtData->numOfGpsOpChannels = 0;
	pvtData->numOfGalileoOpChannels = 0;
	pvtData->numOfOperativeChannels = 0;
	pvtData->numOfChannelsReadyForPVT = 0;
	pvtData->numOfGpsChannelsReadyForPVT = 0;
	pvtData->numOfGalileoChannelsReadyForPVT = 0;
	pvtData->rxGpsTimeValid=FALSE_FLAG;
	pvtData->rxGalTimeValid=FALSE_FLAG;
	pvtData->rxUtcTimeValid=FALSE_FLAG;
	// other variables in pvtRes
	pvtRes->pvtValid=FALSE_FLAG;
	pvtRes->pvtPlotted=FALSE_FLAG;
	pvtRes->clockBiasGpsValid=FALSE_FLAG;
	pvtRes->clockBiasGalileo=FALSE_FLAG;
	pvtRes->clockBiasGpsCumulated=0.0;
	pvtRes->clockBiasGalileoCumulated=0.0;
	for(index=0;index<GPS_NUMBER_OF_PRNS;index++)
	{
		pvtRes->gpsSatStatus[index].satInformationValid=FALSE_FLAG;
	}
	for(index=0;index<GALILEO_NUMBER_OF_PRNS;index++)
	{
		pvtRes->galSatStatus[index].satInformationValid=FALSE_FLAG;
	}
}


int TimeForANewPvt(TNGeneConfiguration *cfg,TNGeneData *rx)
{
	TPvtData *pvtData = &(rx->pvtData);
	int rxWn;
	double rxTow;

	/* if receiver time is not available, use receiver sample count */
	if((pvtData->rxTimeForNextPvtFlag==INVALID_TIME) && ((rx->rxCounter%cfg->pvtPeriodMilliseconds)!=0))
	{
		return FALSE_FLAG;
	}
	else if(pvtData->rxTimeForNextPvtFlag==INVALID_TIME)
	{
		return TRUE_FLAG;
	}
	/* after the receiver time has been set,
	 * use always the same time */
	if(pvtData->rxTimeForNextPvtFlag==GPS_TIME)
	{
		rxWn = pvtData->rxGpsWn;
		rxTow = pvtData->rxGpsTow;
	}
	else /* if(pvtData->rxTimeForNextPvtFlag==GALILEO_TIME) */
	{
		rxWn = pvtData->rxGalWn;
		rxTow = pvtData->rxGalTow;
	}
	/* check if the time for PVT computation is now */
	if(pvtData->rxWnForNextPvt>rxWn)
	{
		return FALSE_FLAG;
	}
	if((pvtData->rxWnForNextPvt==rxWn) && (pvtData->rxTowForNextPvt>rxTow))
	{
		return FALSE_FLAG;
	}

	return TRUE_FLAG;
}



#define MAXIMUM_EPH_AGE   12200
#define MAXIMUM_REDCED_AGE  600

void DeleteOldNavMesg(TNGeneConfiguration *cfg,TNGeneData *rx)
{
	TPvtData *pvtData = &(rx->pvtData);
	double tow;
	int index;
	TChannel *ch;
	TNavigationMessage *navMsg;
	double maximumAge;
	double currentAge;

	/* if time is not available do nothing */
	if((pvtData->rxGpsTimeValid==FALSE_FLAG) && (pvtData->rxGalTimeValid==FALSE_FLAG))
	{
		return;
	}
	/* init time */
	tow = (pvtData->rxGpsTimeValid==TRUE_FLAG) ? pvtData->rxGpsTow : pvtData->rxGalTow;
	/* check each channel */
	for(index=0;index<cfg->numberOfChannels;index++)
	{
		ch = &(rx->channelArray[index]);
		// preliminary check
		if(ReadyForPvt(ch)==FALSE_FLAG)
		{
			continue;
		}
		navMsg = (ch->satType==GPS_SAT) ? ch->gpsParser->navMsg : ch->galParser->navMsg;
		// check
		maximumAge = (navMsg->ephFromRedCed==TRUE_FLAG) ? MAXIMUM_REDCED_AGE : MAXIMUM_EPH_AGE;
		currentAge = fabs(tow-navMsg->eph.toe);
		if(currentAge>maximumAge)
		{
			// reset validity flag only when it is not a change of week
			if(fabs(currentAge-SECONDS_IN_A_WEEK)>maximumAge)
			{
				navMsg->ephemeridesReceived = FALSE_FLAG;
			}
			//
			if((navMsg->ephemeridesReceived==FALSE_FLAG) && (pvtData->pvtLogFile!=NULL))
			{
				if(ch->satType==GPS_SAT)
					fprintf(pvtData->pvtLogFile,"  Discarded Navigation Message of GPS %d (tow %.1f vs toe %.1f)\n",ch->satId,tow,navMsg->eph.toe);
				else
					fprintf(pvtData->pvtLogFile,"  Discarded Navigation Message of Galileo %d (tow %.1f vs toe %.1f)\n",ch->satId,tow,navMsg->eph.toe);
			}
		}
	}
}


int SelectChannelsForPvt(TNGeneConfiguration *cfg,TNGeneData *rx)
{
	TPvtConfiguration *pvtCfg = &(cfg->pvtConfiguration);
	TPvtData *pvtData = &(rx->pvtData);
	TPvtResults *pvtRes = &(rx->pvtTemporaryResults);
	TPvtChannel *pvtChArray = pvtData->pvtChArray;
	int index;
	TChannel *ch;

	pvtData->numOfGpsOpChannels = 0;
	pvtData->numOfGalileoOpChannels = 0;
	pvtData->numOfOperativeChannels = 0;
	for(index=0;index<cfg->numberOfChannels;index++)
	{
		ch = &(rx->channelArray[index]);
		if((ReadyForPvt(ch)==TRUE_FLAG) &&
		   ((pvtCfg->pvtAuthSel==PVT_DO_NOT_DISCARD) ||
		    ((pvtCfg->pvtAuthSel==PVT_DISCARD_AUTHENTICATION_FAILURES) && (rx->osnmaAuth.authenticationFlags[index]!=NOT_VALID)) ||
			((pvtCfg->pvtAuthSel==PVT_DISCARD_NOT_AUTHENTICATED) && (rx->osnmaAuth.authenticationFlags[index]==VALIDATED))))
		{
			pvtChArray[pvtData->numOfOperativeChannels].ch = ch;
			if(ch->satType==GPS_SAT)
			{
				pvtChArray[pvtData->numOfOperativeChannels].satStatus = &(pvtRes->gpsSatStatus[ch->satId-BASE_PRN]);
				pvtData->numOfGpsOpChannels++;
			}
			else if(ch->satType==GALILEO_SAT)
			{
				pvtChArray[pvtData->numOfOperativeChannels].satStatus = &(pvtRes->galSatStatus[ch->satId-BASE_PRN]);
				pvtData->numOfGalileoOpChannels++;
			}
			pvtData->numOfOperativeChannels++;
		}
	}
	/* reset PVT from Kalman to LS if needed */
	if(pvtData->pvtAlgorithm==KALMAN_BASED_PVT)
	{
		if((pvtData->numOfGpsOpChannels>0) && (pvtRes->clockBiasGpsValid==FALSE_FLAG))
		{
			/* first PVT with GPS */
			pvtData->pvtAlgorithm = LMS_BASED_PVT;
			pvtData->lsIndex = 0;
		}
		if((pvtData->numOfGalileoOpChannels>0) && (pvtRes->clockBiasGalileoValid==FALSE_FLAG))
		{
			/* first PVT with Galileo */
			pvtData->pvtAlgorithm = LMS_BASED_PVT;
			pvtData->lsIndex = 0;
		}
	}
	/**/
	pvtData->numOfChannelsReadyForPVT=pvtData->numOfOperativeChannels;
	pvtData->numOfGpsChannelsReadyForPVT=pvtData->numOfGpsOpChannels;
	pvtData->numOfGalileoChannelsReadyForPVT=pvtData->numOfGalileoOpChannels;
	/**/
	/* analyze the minimum required number of channels */
	if(pvtData->pvtAlgorithm==KALMAN_BASED_PVT)
	{
		/* Kalman Filter (executed without limitations about the number of channels) */
		pvtData->minNumOfChannels = 0;
		return SUCCESS_RETURN;
	}
	if((pvtData->numOfGpsOpChannels>0) && (pvtData->numOfGalileoOpChannels>0))
	{
		/* Least Squares multiple constellations */
		pvtData->minNumOfChannels = LS_MULTI_SIZE;
	}
	else
	{
		/* Least Squares single constellation (at least one of numOfGpsOpChannels and numOfGalileoOpChannels is zero) */
		pvtData->minNumOfChannels = LS_SINGLE_SIZE;
	}
	if(pvtData->numOfOperativeChannels>=pvtData->minNumOfChannels)
	{
		return SUCCESS_RETURN;
	}
	return FAILURE_RETURN;
}


void PlotPvtLogFile(TNGeneData *rx,TPvtData *pvtData)
{
	if(pvtData->pvtLogFile!=NULL)
	{
		fprintf(pvtData->pvtLogFile,"\n\n\n*********************************************\n");
		fprintf(pvtData->pvtLogFile,"Rx Samples: %llu\n",rx->buf.discardedSamples);
		fprintf(pvtData->pvtLogFile,"Rx Counter: %llu\n",rx->rxCounter);
		if(pvtData->rxUtcTimeValid==TRUE_FLAG)
		{
			fprintf(pvtData->pvtLogFile,"Rx UTC time: WN %4d - TOW %11.4f\n",pvtData->rxUtcWn,pvtData->rxUtcTow);
		}
		if(pvtData->rxGpsTimeValid==TRUE_FLAG)
		{
			fprintf(pvtData->pvtLogFile,"Rx GPS time: WN %4d - TOW %11.4f\n",pvtData->rxGpsWn,pvtData->rxGpsTow);
		}
		if(pvtData->rxGalTimeValid==TRUE_FLAG)
		{
			fprintf(pvtData->pvtLogFile,"Rx Gal time: WN %4d - TOW %11.4f\n",pvtData->rxGalWn,pvtData->rxGalTow);
		}
		fprintf(pvtData->pvtLogFile,"\n");
	}
}


void ComputeTransmissionTime(TNGeneConfiguration *cfg,TPvtData *pvtData)
{
	TPvtChannel *pvtCh;
	TChannel *ch;
	TTracking *trk;
	int index;

	for(index=0;index<pvtData->numOfOperativeChannels;index++)
	{
		pvtCh = &(pvtData->pvtChArray[index]);
		ch = pvtCh->ch;
		trk = &(ch->tracking);
		if(ch->satType==GPS_SAT)
		{
			pvtCh->wn = ch->gpsParser->navMsg->wn;
			pvtCh->transmissionTime = (double)ch->gpsParser->navMsg->tow +
									  (((double)ch->gpsParser->bitIndexTotal*GPS_BIT_DURATION)/SECONDS_TO_MILLISECONDS) +
									  ((double)trk->msCounter/SECONDS_TO_MILLISECONDS) +
									  ((ch->codePhase - floor(ch->codePhase)) / GPS_CODE_RATE) -
									  ((double)ch->bufferOffset / cfg->fs);
			if(pvtData->pvtLogFile!=NULL)
			{
				fprintf(pvtData->pvtLogFile,"[GPS] CHANNEL:%2d  SAT:%2d  TxTime:%18.12f\n",ch->chIndex,ch->satId,pvtCh->transmissionTime);
			}
		}
		if(ch->satType==GALILEO_SAT)
		{
			pvtCh->wn = ch->galParser->navMsg->wn;
			pvtCh->transmissionTime  = (double)ch->galParser->navMsg->tow +
									   (((double)ch->galParser->bitIndexTotal*GALILEO_BIT_DURATION)/SECONDS_TO_MILLISECONDS) +
									   ((double)trk->msCounter/SECONDS_TO_MILLISECONDS) +
									   ((ch->codePhase - floor(ch->codePhase)) / GALILEO_CODE_RATE) -
									   ((double)ch->bufferOffset / cfg->fs);
			if(pvtData->pvtLogFile!=NULL)
			{
				fprintf(pvtData->pvtLogFile,"[GAL] CHANNEL:%2d  SAT:%2d  TxTime:%18.12f\n",ch->chIndex,ch->satId,pvtCh->transmissionTime);
			}
		}
	}
	if(pvtData->pvtLogFile!=NULL)
	{
		fprintf(pvtData->pvtLogFile,"\n");
	}
}


int VerifyTransmissionTime(TPvtData *pvtData)
{
	int *toBeDeleted;
	TPvtChannel *pvtCh;
	TChannel *ch;
	int chIdx;
	double tRxGpsComplete = 0;
	double tRxGalComplete = 0;
	double tTxComplete;
	int chIdxMin;
	double minTtx;
	int chIdxMax;
	double maxTtx;
	int counter;

	/* TODO check threshold for the first check!
	 * 1 second is a very large threshold, but it should exclude errors coming from the parse of the navigation message */

	/* first, compare with the receiver time and exclude transmission times with large errors
	 * moreover, exclude satellites for which trx is smaller than ttx (can happen if samples are lost) */
	toBeDeleted = (int *)malloc(pvtData->numOfOperativeChannels*sizeof(int));
	// get receiver time
	if(pvtData->rxGpsTimeValid==TRUE_FLAG)
		tRxGpsComplete = (pvtData->rxGpsWn*SECONDS_IN_A_WEEK) + pvtData->rxGpsTow;
	if(pvtData->rxGalTimeValid==TRUE_FLAG)
		tRxGalComplete = (pvtData->rxGalWn*SECONDS_IN_A_WEEK) + pvtData->rxGalTow;
	// check each transmission time
	for(chIdx=0;chIdx<pvtData->numOfOperativeChannels;chIdx++)
	{
		pvtCh = &(pvtData->pvtChArray[chIdx]);
		ch = pvtCh->ch;
		//
		toBeDeleted[chIdx] = FALSE_FLAG;
		tTxComplete = (pvtCh->wn*SECONDS_IN_A_WEEK) + pvtCh->transmissionTime;
		if((ch->satType==GPS_SAT) && (pvtData->rxGpsTimeValid==TRUE_FLAG))
		{
			if(((tRxGpsComplete-tTxComplete)>1.0) || (tRxGpsComplete<tTxComplete))
			{
				toBeDeleted[chIdx] = TRUE_FLAG;
			}
		}
		else if((ch->satType==GALILEO_SAT) && (pvtData->rxGalTimeValid==TRUE_FLAG))
		{
			if(((tRxGalComplete-tTxComplete)>1.0) || (tRxGalComplete<tTxComplete))
			{
				toBeDeleted[chIdx] = TRUE_FLAG;
			}
		}
	}
	// delete bad channels
	for(chIdx=pvtData->numOfOperativeChannels-1;chIdx>=0;chIdx--)
	{
		if(toBeDeleted[chIdx]==TRUE_FLAG)
		{
			DeleteOperativeChannel(pvtData,chIdx);
		}
	}
	free(toBeDeleted);


	/* second, compare transmission times amongst them and exclude incompatible ones */
	/* GPS */
	if(pvtData->numOfGpsOpChannels>1)
	{
		do
		{
			/* find minimum and maximum transmission time (GPS) */
			minTtx = HUGE_VAL;
			maxTtx = -1.0;
			chIdxMin = 0;
			chIdxMax = 0;
			for(chIdx=0;chIdx<pvtData->numOfOperativeChannels;chIdx++)
			{
				pvtCh = &(pvtData->pvtChArray[chIdx]);
				ch = pvtCh->ch;
				if(ch->satType==GPS_SAT)
				{
					if(pvtCh->transmissionTime<minTtx)
					{
						minTtx = pvtCh->transmissionTime;
						chIdxMin = chIdx;
					}
					if(pvtCh->transmissionTime>maxTtx)
					{
						maxTtx = pvtCh->transmissionTime;
						chIdxMax = chIdx;
					}
				}
			}
			/* if the difference between minimum and maximum is over a threshold, delete one or two channels */
			if((maxTtx-minTtx)>GPS_MAXIMUM_TTX_DIFFERENCE)
			{
				if(pvtData->numOfGpsOpChannels==2)
				{
					/* cannot find which is good, delete both minimum and maximum */
					DeleteOperativeChannel(pvtData,chIdxMin);
					if(chIdxMax>chIdxMin)
						chIdxMax--;
					DeleteOperativeChannel(pvtData,chIdxMax);
					maxTtx = minTtx;
				}
				else
				{
					/* decide which channel will be discarded */
					counter = 0;
					for(chIdx=0;chIdx<pvtData->numOfOperativeChannels;chIdx++)
					{
						pvtCh = &(pvtData->pvtChArray[chIdx]);
						ch = pvtCh->ch;
						if(ch->satType==GPS_SAT)
						{
							if((pvtCh->transmissionTime-minTtx)>(maxTtx-pvtCh->transmissionTime))
								counter++;
							else
								counter--;
						}
					}
					if(counter>0)
					{
						DeleteOperativeChannel(pvtData,chIdxMin);
					}
					else
					{
						DeleteOperativeChannel(pvtData,chIdxMax);
					}
				}
			}
		} while((maxTtx-minTtx)>GPS_MAXIMUM_TTX_DIFFERENCE);
	}

	/* Galileo */
	if(pvtData->numOfGalileoOpChannels>1)
	{
		do
		{
			/* find minimum and maximum transmission time (Galileo) */
			minTtx = HUGE_VAL;
			maxTtx = -1.0;
			chIdxMin = 0;
			chIdxMax = 0;
			for(chIdx=0;chIdx<pvtData->numOfOperativeChannels;chIdx++)
			{
				pvtCh = &(pvtData->pvtChArray[chIdx]);
				ch = pvtCh->ch;
				if(ch->satType==GALILEO_SAT)
				{
					if(pvtCh->transmissionTime<minTtx)
					{
						minTtx = pvtCh->transmissionTime;
						chIdxMin = chIdx;
					}
					if(pvtCh->transmissionTime>maxTtx)
					{
						maxTtx = pvtCh->transmissionTime;
						chIdxMax = chIdx;
					}
				}
			}
			/* if the difference between minimum and maximum is over a threshold, delete one or two channels */
			if((maxTtx-minTtx)>GALILEO_MAXIMUM_TTX_DIFFERENCE)
			{
				if(pvtData->numOfGalileoOpChannels==2)
				{
					/* cannot find which is good, delete both minimum and maximum */
					DeleteOperativeChannel(pvtData,chIdxMin);
					if(chIdxMax>chIdxMin)
						chIdxMax--;
					DeleteOperativeChannel(pvtData,chIdxMax);
					maxTtx = minTtx;
				}
				else
				{
					/* decide which channel will be discarded */
					counter = 0;
					for(chIdx=0;chIdx<pvtData->numOfOperativeChannels;chIdx++)
					{
						pvtCh = &(pvtData->pvtChArray[chIdx]);
						ch = pvtCh->ch;
						if(ch->satType==GALILEO_SAT)
						{
							if((pvtCh->transmissionTime-minTtx)>(maxTtx-pvtCh->transmissionTime))
								counter++;
							else
								counter--;
						}
					}
					if(counter>0)
					{
						DeleteOperativeChannel(pvtData,chIdxMin);
					}
					else
					{
						DeleteOperativeChannel(pvtData,chIdxMax);
					}
				}
			}
		} while((maxTtx-minTtx)>GALILEO_MAXIMUM_TTX_DIFFERENCE);
	}

	/* verify if PVT is still possible */
	if(pvtData->numOfOperativeChannels>=pvtData->minNumOfChannels)
	{
		return TRUE_FLAG;
	}
	else
	{
		return FALSE_FLAG;
	}
}


void ComputeSatsPositions(TNGeneConfiguration *cfg,TNGeneData *rx,TPvtData *pvtData,TPvtResults *pvtRes)
{
	double dt, time;
	double a, tk, n0, n, m, e, ii, eOld, dE, dtr, nu, phi, u, r, i, omega;
	double tak, xpk, ypk, takdot, ekdot, ukdot, rkdot, ikdot, xpkdot, ypkdot, omegak, omegakdot;
	double xkdot, ykdot, zkdot;
	int index;
	TPvtChannel *pvtCh;
	TChannel *ch;
	TGnssEphemerides *usedEphem;
	TGnssClk *usedClock;
	TSatStatus *satStatus;
	//double egnosClockCorr;
	//double egnosXCorr, egnosYCorr, egnosZcorr;

	for(index=0;index<pvtData->numOfOperativeChannels;index++)
	{
		pvtCh = &(pvtData->pvtChArray[index]);
		ch = pvtCh->ch;
		satStatus = pvtCh->satStatus;
		if(ch->satType==GPS_SAT)
		{
			usedEphem = &(ch->gpsParser->navMsg->eph);
			usedClock = &(ch->gpsParser->navMsg->clk);
		}
		else //if(ch->satType==GALILEO_SAT)
		{
			usedEphem = &(ch->galParser->navMsg->eph);
			usedClock = &(ch->galParser->navMsg->clk);
		}
		/* transmitted clock corrections */
		dt = CheckT(pvtCh->transmissionTime,usedClock->toc);
		pvtCh->satClkCorr =	(((usedClock->af[2] * dt) + (usedClock->af[1])) * dt) + usedClock->af[0] - usedClock->tGroupDelay;
		/*  EGNOS long term satellite clock correction (0.0 if not enabled or not available) */
		pvtCh->egnosClockCorr = GetEgnosLongTermClockCorrection(cfg,usedClock->iodc,pvtCh->ch->egnosPrnMaskIndex,&(rx->egnosCorrections),pvtCh->transmissionTime);
		pvtCh->satClkCorr += pvtCh->egnosClockCorr;
		if((pvtData->pvtLogFile!=NULL) && (cfg->pvtConfiguration.egnosFlag==TRUE_FLAG))
		{
			fprintf(pvtData->pvtLogFile,"      CHANNEL %d (EGNOS long term correction): %f s\n",ch->chIndex,pvtCh->egnosClockCorr);
		}
		/* correct the transmission time */
		time = pvtCh->transmissionTime - pvtCh->satClkCorr;

		/* compute sat position */
		a = usedEphem->sqrtA * usedEphem->sqrtA;
		tk = CheckT(time, usedEphem->toe);
		n0 = sqrt(GM / (a * a * a));
		n = n0 + usedEphem->deltaN;
		m = usedEphem->meanAnomaly0 + n * tk;
		m = fmod(m + TWICE_M_PI,TWICE_M_PI);
		e = m;
		for(ii=0;ii<10;ii++)
		{
			eOld = e;
			e = m + usedEphem->eccentricity * sin(e);
			dE = fmod(e-eOld,TWICE_M_PI);
			if(fabs(dE)<1E-12)
			{
				// Necessary precision is reached, exit from the loop
				break;
			}
		}
		e = fmod(e + TWICE_M_PI, TWICE_M_PI);
		dtr = F_CONST * usedEphem->eccentricity *	usedEphem->sqrtA * sin(e);
		nu = atan2(sqrt(1 - usedEphem->eccentricity *	usedEphem->eccentricity) * sin(e), cos(e)- usedEphem->eccentricity);
		phi = nu + usedEphem->omega;
		phi = fmod(phi,TWICE_M_PI);
		u = phi + usedEphem->capitalCuc * cos(2*phi) + usedEphem->capitalCus * sin(2*phi);
		r = a * (1 - usedEphem->eccentricity * cos(e)) + usedEphem->capitalCrc * cos(2*phi) + usedEphem->capitalCrs * sin(2*phi);
		i = usedEphem->i0 + usedEphem->iDot * tk + usedEphem->capitalCic * cos(2*phi) +usedEphem->capitalCis * sin(2*phi);
		omega = usedEphem->capitalOmega0 + (usedEphem->capitalOmegaDot - OMEGAE_DOT) * tk -	OMEGAE_DOT * usedEphem->toe;
		omega = fmod(omega + TWICE_M_PI, TWICE_M_PI);
		satStatus->ecefSatPos[0] = (cos(u) * r * cos(omega)) - (sin(u) * r * cos(i) * sin(omega));
		satStatus->ecefSatPos[1] = (cos(u) * r * sin(omega)) + (sin(u) * r * cos(i) * cos(omega));
		satStatus->ecefSatPos[2] = (sin(u) * r * sin(i));
		/*  EGNOS long term satellite position correction (0.0 if not enabled or not available) */
		GetEgnosLongTermPositionCorrection(cfg,usedEphem->iode2,pvtCh->ch->egnosPrnMaskIndex,&(rx->egnosCorrections),pvtCh->transmissionTime,&(pvtCh->egnosXCorr),&(pvtCh->egnosYCorr),&(pvtCh->egnosZcorr));
		satStatus->ecefSatPos[0] += pvtCh->egnosXCorr;
		satStatus->ecefSatPos[1] += pvtCh->egnosYCorr;
		satStatus->ecefSatPos[2] += pvtCh->egnosZcorr;
		if((pvtData->pvtLogFile!=NULL) && (cfg->pvtConfiguration.egnosFlag==TRUE_FLAG))
		{
			fprintf(pvtData->pvtLogFile,"      CHANNEL %d (EGNOS long term correction): %f m, %f m, %f m\n",ch->chIndex,pvtCh->egnosXCorr,pvtCh->egnosYCorr,pvtCh->egnosZcorr);
		}

		/* compute sat velocity */
		ekdot = n/(1.0 - usedEphem->eccentricity * cos(e));
		tak = atan2( sqrt(1.0 - usedEphem->eccentricity * usedEphem->eccentricity) * sin(e), cos(e) - usedEphem->eccentricity);
		takdot = sin(e) * ekdot * (1.0 + usedEphem->eccentricity *cos(tak)) / (sin(tak) * (1.0 - usedEphem->eccentricity * cos(e)));
		ukdot = takdot + 2.0 *((usedEphem->capitalCus * cos(2.0 * u)) -	(usedEphem->capitalCuc * sin(2.0 * u))) * takdot;
		rkdot = ((a * usedEphem->eccentricity * sin(e) * n) / (1.0 - usedEphem->eccentricity * cos(e))) + (2.0 * (usedEphem->capitalCrs * cos(2.0 * u) - usedEphem->capitalCrc * sin(2.0 * u)) * takdot);
		ikdot = usedEphem->iDot + ((usedEphem->capitalCis *	cos(2.0 * u)) - (usedEphem->capitalCic * sin(2.0 * u))) * 2.0 * takdot;
		xpk = r*cos(u);
		ypk = r*sin(u);
		xpkdot = rkdot * cos(u) - ypk * ukdot;
		ypkdot = rkdot * sin(u) + xpk * ukdot;
		omegak = omega;
		omegakdot = (usedEphem->capitalOmegaDot - OMEGAE_DOT);
		xkdot = (xpkdot - ypk * cos(i) * omegakdot) * cos(omegak) - (xpk * omegakdot + ypkdot * cos(i) - ypk * sin(i) * ikdot) * sin(omegak);
		ykdot = (xpkdot - ypk * cos(i) * omegakdot) * sin(omegak) + (xpk * omegakdot + ypkdot * cos(i) - ypk * sin(i) * ikdot) * cos(omegak);
		zkdot = ypkdot * sin(i) + ypk * cos(i) * ikdot;
		satStatus->ecefSatVel[0] = xkdot;
		satStatus->ecefSatVel[1] = ykdot;
		satStatus->ecefSatVel[2] = zkdot;
		satStatus->ecefSatVelTot = sqrt(pow(xkdot,2) + pow(ykdot,2) + pow(zkdot,2))/1000;

		/* azimuth and elevation */
		satStatus->azimuth = 0.0;
		satStatus->elevation = M_PI;
		if(pvtData->pvtIndex>0)
		{
			ComputeAzEl(satStatus->ecefSatPos,pvtRes->ecefPosition,&(satStatus->azimuth),&(satStatus->elevation));
			GetIonosphericPiercePoint(pvtRes->geodPosition,satStatus->azimuth,satStatus->elevation,&(satStatus->ippLatitude),&(satStatus->ippLongitude),&(satStatus->fpp));
		}

		/* clock correction for the pseudorange */
		pvtCh->satClkCorr += dtr;
		satStatus->satInformationValid=TRUE_FLAG;
		/* TODO dove rimetterlo a FALSE? */

		if(pvtData->pvtLogFile!=NULL)
		{
			if(ch->satType==GPS_SAT)
			{
				fprintf(pvtData->pvtLogFile,"[GPS] CHANNEL:%2d  SAT:%2d   X: %13.2f,  Y: %13.2f,  Z: %13.2f\n",ch->chIndex,ch->satId,satStatus->ecefSatPos[0],satStatus->ecefSatPos[1],satStatus->ecefSatPos[2]);
			}
			else /* if(ch->satType==GALILEO_SAT) */
			{
				if(ch->galParser->navMsg->ephFromRedCed==TRUE_FLAG)
				{
					fprintf(pvtData->pvtLogFile,"[GAL] CHANNEL:%2d  SAT:%2d uses Red CED\n",ch->chIndex,ch->satId);
				}
				fprintf(pvtData->pvtLogFile,"[GAL] CHANNEL:%2d  SAT:%2d   X: %13.2f,  Y: %13.2f,  Z: %13.2f\n",ch->chIndex,ch->satId,satStatus->ecefSatPos[0],satStatus->ecefSatPos[1],satStatus->ecefSatPos[2]);
			}
			fprintf(pvtData->pvtLogFile,"                          VX: %13.2f, VY: %13.2f, VZ: %13.2f\n",satStatus->ecefSatVel[0],satStatus->ecefSatVel[1],satStatus->ecefSatVel[2]);
			if(pvtData->pvtIndex>0)
			{
				fprintf(pvtData->pvtLogFile,"                               EL: %f,  AZ: %f\n",satStatus->elevation,satStatus->azimuth);
				fprintf(pvtData->pvtLogFile,"                          IPP Lat: %f, Lon: %f\n",satStatus->ippLatitude,satStatus->ippLongitude);
			}
		}
	}
}


int ApplyElevationMask(TNGeneConfiguration *cfg,TPvtData *pvtData)
{
	int chIdx;
	TPvtChannel *pvtCh;
	TSatStatus *satStatus;

	if(cfg->pvtConfiguration.elevationMaskFlag==TRUE_FLAG)
	{
		for(chIdx=0;chIdx<pvtData->numOfOperativeChannels;chIdx++)
		{
			pvtCh = &(pvtData->pvtChArray[chIdx]);
			satStatus = pvtCh->satStatus;
			if((satStatus->satInformationValid==TRUE_FLAG) && (satStatus->elevation<cfg->pvtConfiguration.elevationMask))
			{
				// log
				if(pvtData->pvtLogFile!=NULL)
				{
					if(pvtCh->ch->satType==GPS_SAT)
						fprintf(pvtData->pvtLogFile,"  Discarded GPS %d from PVT (el: %.1f)\n",pvtCh->ch->satId,satStatus->elevation*(double)RADIAN_TO_DEGREE);
					else
						fprintf(pvtData->pvtLogFile,"  Discarded Galileo %d from PVT (el: %.1f)\n",pvtCh->ch->satId,satStatus->elevation*(double)RADIAN_TO_DEGREE);
				}
				/* Exclude the satellite from the PVT, because its elevation is under the elevation mask */
				DeleteOperativeChannel(pvtData,chIdx);
				chIdx--;
			}
		}
	}
	if(pvtData->numOfOperativeChannels>=pvtData->minNumOfChannels)
	{
		return TRUE_FLAG;
	}
	else
	{
		return FALSE_FLAG;
	}
}

int ApplyUrbanCanyonMask(TNGeneConfiguration *cfg,TPvtData *pvtData)
{
	int chIdx;
	TPvtChannel *pvtCh;
	TSatStatus *satStatus;

	/* if the Urban Canyon Mask is disabled, do nothing */
	if(cfg->pvtConfiguration.urbanCanyonMaskFlag==FALSE_FLAG)
	{
		return TRUE_FLAG;
	}

	/* From ETSI TS 103 246-3 pag. 34 Document */
	double elevationMaskZoneA=5*DEGREE_TO_RADIAN;
	double elevationMaskZoneB=60*DEGREE_TO_RADIAN;
	double azimuthMaskZoneBMin=210*DEGREE_TO_RADIAN;
	double azimuthMaskZoneBMax=330*DEGREE_TO_RADIAN;
	double azimuthMaskZoneCMin=30*DEGREE_TO_RADIAN;
	double azimuthMaskZoneCMax=150*DEGREE_TO_RADIAN;

	if(cfg->pvtConfiguration.urbanCanyonMaskFlag==TRUE_FLAG)
	{
		for(chIdx=0;chIdx<pvtData->numOfOperativeChannels;chIdx++)
		{
			pvtCh = &(pvtData->pvtChArray[chIdx]);
			satStatus = pvtCh->satStatus;
			if((satStatus->satInformationValid==TRUE_FLAG) && (satStatus->elevation<elevationMaskZoneA))
			{
				// log
				if(pvtData->pvtLogFile!=NULL)
				{
					if(pvtCh->ch->satType==GPS_SAT)
						fprintf(pvtData->pvtLogFile,"  Discarded GPS %d from PVT (urban canyon mask)\n",pvtCh->ch->satId);
					else
						fprintf(pvtData->pvtLogFile,"  Discarded Galileo %d from PVT (urban canyon mask)\n",pvtCh->ch->satId);
				}
				/* Exclude the satellite from the PVT, because its elevation is under the elevation mask */
				SwrxLog(NGENE_LOG_SEVERITY_NOTE,"Satellite %s excluded from PVT",pvtCh->ch->prnName);
				DeleteOperativeChannel(pvtData,chIdx);
				chIdx--;
			}
			else if((satStatus->satInformationValid==TRUE_FLAG) && (satStatus->elevation>=elevationMaskZoneA) && (satStatus->elevation<elevationMaskZoneB))
			{
				if(((satStatus->azimuth>azimuthMaskZoneBMin) && (satStatus->azimuth<azimuthMaskZoneBMax)) || ((satStatus->azimuth>azimuthMaskZoneCMin) && (satStatus->azimuth<azimuthMaskZoneCMax)))
				{
					// log
					if(pvtData->pvtLogFile!=NULL)
					{
						if(pvtCh->ch->satType==GPS_SAT)
							fprintf(pvtData->pvtLogFile,"  Discarded GPS %d from PVT (urban canyon mask)\n",pvtCh->ch->satId);
						else
							fprintf(pvtData->pvtLogFile,"  Discarded Galileo %d from PVT (urban canyon mask)\n",pvtCh->ch->satId);
					}
					/* Exclude the satellite from the PVT */
					SwrxLog(NGENE_LOG_SEVERITY_NOTE,"Satellite %s excluded from PVT since out of the Urban Canyon Mask",pvtCh->ch->prnName);
					DeleteOperativeChannel(pvtData,chIdx);
					chIdx--;
				}
			}
		}
	}
	if(pvtData->numOfOperativeChannels>=pvtData->minNumOfChannels)
	{
		return TRUE_FLAG;
	}
	else
	{
		return FALSE_FLAG;
	}
}


/* minimum value: 2 */
#define MINIMUM_CSM_ITERATION_BEFORE_USE     10

void DeleteNotSmoothed(TNGeneConfiguration *cfg,TPvtData *pvtData)
{
	int chIdx;
	TPvtChannel *pvtCh;
	TChannel *ch;
	TCarrierSmoothingData *csmData;
	int counterCsm = 0;

	/* If carrier smoothing is disabled, this check is not needed */
	if(cfg->pvtConfiguration.csmFilterLen<=1)
	{
		return;
	}

	/* Count how many measurements are smoothed */
	for(chIdx=0;chIdx<pvtData->numOfOperativeChannels;chIdx++)
	{
		pvtCh = &(pvtData->pvtChArray[chIdx]);
		ch = pvtCh->ch;
		csmData = &(pvtData->csmData[ch->chIndex]);
		if(csmData->csmCounter>MINIMUM_CSM_ITERATION_BEFORE_USE)
		{
			/* The comparison is with 2, because this function is called after the smoothing
			 * and the counter has been already incremented */
			counterCsm++;
		}
	}
	/* If there are enough smoothed measurements for the PVT computation,
	 * exclude non smoothed measurements from PVT */
	if(counterCsm>=pvtData->minNumOfChannels)
	{
		for(chIdx=0;chIdx<pvtData->numOfOperativeChannels;chIdx++)
		{
			pvtCh = &(pvtData->pvtChArray[chIdx]);
			ch = pvtCh->ch;
			csmData = &(pvtData->csmData[ch->chIndex]);
			if(csmData->csmCounter<=MINIMUM_CSM_ITERATION_BEFORE_USE)
			{
				/* Exclude the satellite from the PVT */
				DeleteOperativeChannel(pvtData,chIdx);
				chIdx--;
				// log
				if(pvtData->pvtLogFile!=NULL)
				{
					if(ch->satType==GPS_SAT)
						fprintf(pvtData->pvtLogFile,"  Discarded GPS %d from PVT (CSM counter: %d)\n",ch->satId,csmData->csmCounter);
					else
						fprintf(pvtData->pvtLogFile,"  Discarded Galileo %d from PVT (CSM counter: %d)\n",ch->satId,csmData->csmCounter);
				}
			}
		}
	}
}


void AdjustMissingRxTime(TNGeneConfiguration *cfg,TNGeneData *rx,TPvtData *pvtData)
{
	double gstToGpsTimeOffset;

	/* This function is not needed when
	 * - when a single constellation configuration is used
	 * - when both time values are available */
	if((cfg->numberOfGalileoChannels==0) || (cfg->numberOfGpsChannels==0))
	{
		return;
	}
	if((pvtData->rxGalTimeValid==TRUE_FLAG) && (pvtData->rxGpsTimeValid==TRUE_FLAG))
	{
		return;
	}

	if((pvtData->numOfGalileoOpChannels>0) && (pvtData->rxGalTimeValid==FALSE_FLAG) && (pvtData->rxGpsTimeValid==TRUE_FLAG))
	{
		/* Obtain the Galileo time from the GPS one */
		pvtData->rxGalWn = pvtData->rxGpsWn - GPS_WN_TO_GAL_WN_BIAS;
		pvtData->rxGalTow = pvtData->rxGpsTow;
		pvtData->rxGalTimeValid = TRUE_FLAG;
		/* Adjust with the GPS to GST conversion */
		GetGstToGpsTimeOffset(rx,pvtData->rxGalWn,pvtData->rxGalTow,&gstToGpsTimeOffset);
		pvtData->rxGalTow += gstToGpsTimeOffset;
		if(pvtData->rxGalTow>=SECONDS_IN_A_WEEK)
		{
			pvtData->rxGalTow -= SECONDS_IN_A_WEEK;
			pvtData->rxGalWn++;
		}
		else if(pvtData->rxGalTow<0.0)
		{
			pvtData->rxGalTow += SECONDS_IN_A_WEEK;
			pvtData->rxGalWn--;
		}
	}
	if((pvtData->numOfGpsOpChannels>0) && (pvtData->rxGpsTimeValid==FALSE_FLAG) && (pvtData->rxGalTimeValid==TRUE_FLAG))
	{
		/* Obtain the GPS time from the Galileo one */
		pvtData->rxGpsWn = pvtData->rxGalWn + GPS_WN_TO_GAL_WN_BIAS;
		pvtData->rxGpsTow = pvtData->rxGalTow;
		pvtData->rxGpsTimeValid = TRUE_FLAG;
		/* Adjust with the GPS to GST conversion */
		GetGstToGpsTimeOffset(rx,pvtData->rxGalWn,pvtData->rxGalTow,&gstToGpsTimeOffset);
		pvtData->rxGpsTow -= gstToGpsTimeOffset;
		if(pvtData->rxGpsTow>=SECONDS_IN_A_WEEK)
		{
			pvtData->rxGpsTow -= SECONDS_IN_A_WEEK;
			pvtData->rxGpsWn++;
		}
		else if(pvtData->rxGpsTow<0.0)
		{
			pvtData->rxGpsTow += SECONDS_IN_A_WEEK;
			pvtData->rxGpsWn--;
		}
	}
}


void ComputePseudoranges(TNGeneConfiguration *cfg,TNGeneData *rx,TPvtData *pvtData,TPvtResults *pvtRes)
{
	int index;
	int maxGpsWn = 0;
	double maxGpsTow = 0;
	int maxGalWn = 0;
	double maxGalTow = 0;
	double gpsBase = 0;
	double galBase = 0;
	TPvtChannel *pvtCh;
	TChannel *ch;
	//double egnosFastCorrection;

	/* Look for the nearest satellite for each constellation
	 * take into account also the week number
	 * If CheckEphemerides is successful (and a satellite can be used for PVT)
	 * then both TOW and WN are valid! */
	for(index=0;index<pvtData->numOfOperativeChannels;index++)
	{
		pvtCh = &(pvtData->pvtChArray[index]);
		ch = pvtCh->ch;
		if(ch->satType==GPS_SAT)
		{
			if(pvtCh->wn>maxGpsWn)
			{
				maxGpsWn = pvtCh->wn;
				maxGpsTow = pvtCh->transmissionTime;
			}
			else if((pvtCh->wn==maxGpsWn) && (pvtCh->transmissionTime>maxGpsTow))
			{
				maxGpsTow = pvtCh->transmissionTime;
			}
		}
		if(ch->satType==GALILEO_SAT)
		{
			if(pvtCh->wn>maxGalWn)
			{
				maxGalWn = pvtCh->wn;
				maxGalTow = pvtCh->transmissionTime;
			}
			else if((pvtCh->wn==maxGalWn) && (pvtCh->transmissionTime>maxGalTow))
			{
				maxGalTow = pvtCh->transmissionTime;
			}
		}
	}
	/* Calculates the shortest pseudorange for each constellation,
	 * correct when WN changes */
	if(pvtData->numOfGpsOpChannels>0)
	{
		if(pvtData->rxGpsTimeValid==TRUE_FLAG)
		{
			gpsBase = pvtData->rxGpsTow - maxGpsTow;
			if(gpsBase<0.0)
			{
				gpsBase += SECONDS_IN_A_WEEK;
			}
		}
		else
		{
			ResetPvt(cfg,pvtData,pvtRes,FALSE_FLAG);
			gpsBase = DEFAULT_PSEUDORANGE_GPS;
		}
	}
	if(pvtData->numOfGalileoOpChannels>0)
	{
		if(pvtData->rxGalTimeValid==TRUE_FLAG)
		{
			galBase = pvtData->rxGalTow - maxGalTow;
			if(galBase<0.0)
			{
				galBase += SECONDS_IN_A_WEEK;
			}
		}
		else
		{
			ResetPvt(cfg,pvtData,pvtRes,FALSE_FLAG);
			galBase = DEFAULT_PSEUDORANGE_GAL;
		}
	}
	if(pvtData->pvtLogFile!=NULL)
	{
		if(cfg->pvtConfiguration.egnosFlag==TRUE_FLAG)
		{
			fprintf(pvtData->pvtLogFile,"\n");
			fprintf(pvtData->pvtLogFile,"EGNOS fast corrections\n");
		}
	}
	/* Compute all the pseudoranges and store Doppler frequency */
	for(index=0;index<pvtData->numOfOperativeChannels;index++)
	{
		pvtCh = &(pvtData->pvtChArray[index]);
		ch = pvtCh->ch;
		pvtCh->deltaPseudorange = (double)cfg->fePars->dopplerCorrection*ch->dopplerShift;
		if(ch->satType==GPS_SAT)
		{
			pvtCh->pseudorangeRaw = (maxGpsTow - pvtCh->transmissionTime + gpsBase)*SPEED_OF_LIGHT;
		}
		else if(ch->satType==GALILEO_SAT)
		{
			pvtCh->pseudorangeRaw = (maxGalTow - pvtCh->transmissionTime + galBase)*SPEED_OF_LIGHT;
		}
		/* correct week roll-over */
		if(pvtCh->pseudorangeRaw<0.0)
		{
			pvtCh->pseudorangeRaw += (SECONDS_IN_A_WEEK * SPEED_OF_LIGHT);
		}
		/* correct the satellite clock error */
		pvtCh->pseudorange = pvtCh->pseudorangeRaw + (pvtCh->satClkCorr * SPEED_OF_LIGHT);
		/* EGNOS fast correction (0.0 if not enabled or not available) */
		pvtCh->egnosFastCorrection = GetEgnosFastCorrection(cfg,ch->egnosPrnMaskIndex,&(rx->egnosCorrections),pvtData->rxGpsTow);
		pvtCh->pseudorange += pvtCh->egnosFastCorrection;
		if((pvtData->pvtLogFile!=NULL) && (cfg->pvtConfiguration.egnosFlag==TRUE_FLAG))
		{
			fprintf(pvtData->pvtLogFile,"         CHANNEL %d: %f m\n",ch->chIndex,pvtCh->egnosFastCorrection);
		}
	}
	if(pvtData->pvtLogFile!=NULL)
	{
		fprintf(pvtData->pvtLogFile,"\n");
	}
}


void GetPseudorangeCorrections(TNGeneConfiguration *cfg,TNGeneData *rx,TPvtData *pvtData,TPvtResults *pvtRes)
{
	int index;
	TPvtChannel *pvtCh;
	TChannel *ch;
	TSatStatus *satStatus;
	TGnssIono *iono;
	double latu = pvtRes->geodPosition[1]*DEGREE_TO_RADIAN;
	double lonu = pvtRes->geodPosition[0]*DEGREE_TO_RADIAN;
	TDateTime date;
	double dTime;
	double rxLatLonAlt[3];
	double satLonLatAlt[3];
	double satLatLonAlt[3];
	double currentTime = (double)rx->buf.discardedSamples/cfg->fs;
	//char ionoCorrectionFromEgnos;

	rxLatLonAlt[0] = pvtRes->geodPosition[1]; /* Lat */
	rxLatLonAlt[1] = pvtRes->geodPosition[0]; /* Lon */
	rxLatLonAlt[2] = pvtRes->geodPosition[2]; /* Height */

	/* for each channel used in PVT */
	for(index=0;index<pvtData->numOfOperativeChannels;index++)
	{
		pvtCh = &(pvtData->pvtChArray[index]);
		iono = NULL;
		pvtCh->ionoCorrection = 0.0;
		pvtCh->tropoCorrection = 0.0;
		if(pvtData->pvtIndex>0)
		{
			ch = pvtCh->ch;
			satStatus = pvtCh->satStatus;
			/* Ionosphere correction */
			/* First, try EGNOS model.
			 * If EGNOS model is not available, use the model broadcasted by the elaborated constellation */
			pvtCh->ionoCorrectionFromEgnos = EgnosIonosphericCorrection(cfg,&(rx->egnosCorrections),satStatus->ippLatitude,satStatus->ippLongitude,satStatus->fpp,&(pvtCh->ionoCorrection));
			if((cfg->pvtConfiguration.egnosFlag==TRUE_FLAG) && (pvtCh->ionoCorrectionFromEgnos==TRUE_FLAG) && (rx->egnosCorrections.ionoDebugFile!=NULL))
			{
				fprintf(rx->egnosCorrections.ionoDebugFile,"PVT index: %5llu -- Tx time: %10.3f -- Iono correction for %s\n\n",pvtData->pvtIndex,pvtCh->transmissionTime,ch->prnName);
			}

			if(pvtCh->ionoCorrectionFromEgnos==FALSE_FLAG)
			{
				if(ch->satType==GPS_SAT)
				{
					if(ch->gpsParser->navMsg->ionoReceived==TRUE_FLAG)
					{
						iono = &(ch->gpsParser->navMsg->iono);
					}
					else if(rx->gpsIonoValid==TRUE_FLAG)
					{
						iono = &(rx->gpsIono);
					}
					else if(cfg->pvtConfiguration.ionoDefaultValid==TRUE_FLAG)
					{
						iono = &(cfg->pvtConfiguration.ionoDefault);
					}
					if(iono!=NULL)
					{
						pvtCh->ionoCorrection = SPEED_OF_LIGHT * GpsIonosphericCorrection(iono,satStatus->elevation,satStatus->azimuth,latu,lonu,pvtCh->transmissionTime);
					}
					else if(rx->galIonoValid==TRUE_FLAG)
					{
						// apply Nequick model (if available)
						iono = &(rx->galIono);
						if((iono!=NULL) && (currentTime>(pvtData->nequickCorrectionsTime[ch->satId+GALILEO_NUMBER_OF_PRNS-BASE_PRN] + NEQUICK_RESULTS_UPDATE_INTERVAL)))
						{
							FromGnssTimeToDate(GPS_SAT,(int)pvtCh->wn,pvtCh->transmissionTime,&date);
							dTime = date.hour+((date.minute*SECONDS_IN_A_MINUTE)+date.second)/SECONDS_IN_A_HOUR;
							Ecef2Geod(satStatus->ecefSatPos,satLonLatAlt);
							satLatLonAlt[0] = satLonLatAlt[1]; /* Lat */
							satLatLonAlt[1] = satLonLatAlt[0]; /* Lon */
							satLatLonAlt[2] = satLonLatAlt[2]; /* Height */
							pvtCh->ionoCorrection = GalileoIonosphericCorrection(&(pvtData->nequick),iono->alphaGal,(int)date.month,dTime,rxLatLonAlt,satLatLonAlt);
							pvtData->nequickCorrections[ch->satId+GALILEO_NUMBER_OF_PRNS-BASE_PRN] = pvtCh->ionoCorrection;
							pvtData->nequickCorrectionsTime[ch->satId+GALILEO_NUMBER_OF_PRNS-BASE_PRN] = currentTime;
						}
						else if(iono!=NULL)
						{
							pvtCh->ionoCorrection = pvtData->nequickCorrections[ch->satId+GALILEO_NUMBER_OF_PRNS-BASE_PRN];
						}
					}
				}
				else /* if(ch->satType==GALILEO_SAT) */
				{
					if(ch->galParser->navMsg->ionoReceived==TRUE_FLAG)
					{
						iono = &(ch->galParser->navMsg->iono);
					}
					else if(rx->galIonoValid==TRUE_FLAG)
					{
						iono = &(rx->galIono);
					}
					else if(cfg->pvtConfiguration.ionoDefaultValid==TRUE_FLAG)
					{
						iono = &(cfg->pvtConfiguration.ionoDefault);
					}					
					/* If available, Iono correction for Galileo are updated
					 * every NEQUICK_RESULTS_UPDATE_INTERVAL seconds */
					if((iono!=NULL) && (currentTime>(pvtData->nequickCorrectionsTime[ch->satId-BASE_PRN] + NEQUICK_RESULTS_UPDATE_INTERVAL)))
					{
						FromGnssTimeToDate(GALILEO_SAT,(int)pvtCh->wn,pvtCh->transmissionTime,&date);
						dTime = date.hour+((date.minute*SECONDS_IN_A_MINUTE)+date.second)/SECONDS_IN_A_HOUR;
						Ecef2Geod(satStatus->ecefSatPos,satLonLatAlt);
						satLatLonAlt[0] = satLonLatAlt[1]; /* Lat */
						satLatLonAlt[1] = satLonLatAlt[0]; /* Lon */
						satLatLonAlt[2] = satLonLatAlt[2]; /* Height */
						pvtCh->ionoCorrection = GalileoIonosphericCorrection(&(pvtData->nequick),iono->alphaGal,(int)date.month,dTime,rxLatLonAlt,satLatLonAlt);
						pvtData->nequickCorrections[ch->satId-BASE_PRN] = pvtCh->ionoCorrection;
						pvtData->nequickCorrectionsTime[ch->satId-BASE_PRN] = currentTime;
					}
					else if(iono!=NULL)
					{
						pvtCh->ionoCorrection = pvtData->nequickCorrections[ch->satId-BASE_PRN];
					}
				}
			}
			/* Troposphere correction */
			pvtCh->tropoCorrection = -TropoHopfieldModel(satStatus->elevation,pvtRes->geodPosition[2]);
			//pvtCh->tropoCorrection = Tropo(sin(satStatus->elevation), 0.0, 1013.0, 293.0, 50.0, 0.0, 0.0, 0.0);

			if(pvtData->pvtLogFile!=NULL)
			{
				if(ch->satType==GPS_SAT)
				{
					fprintf(pvtData->pvtLogFile,"[GPS] CHANNEL:%2d  SAT:%2d   tropo: %f   iono: %f",ch->chIndex,ch->satId,pvtCh->tropoCorrection,pvtCh->ionoCorrection);
				}
				else /* if(ch->satType==GALILEO_SAT) */
				{
					fprintf(pvtData->pvtLogFile,"[GAL] CHANNEL:%2d  SAT:%2d   tropo: %f   iono: %f",ch->chIndex,ch->satId,pvtCh->tropoCorrection,pvtCh->ionoCorrection);
				}
				if(pvtCh->ionoCorrectionFromEgnos==TRUE_FLAG)
				{
					fprintf(pvtData->pvtLogFile," (EGNOS)\n");
				}
				else
				{
					fprintf(pvtData->pvtLogFile,"\n");
				}
			}
		}
	}
}


void SetRxTimeForNextPvt(TNGeneConfiguration *cfg,TNGeneData *rx)
{
	TPvtData *pvtData = &(rx->pvtData);
	TPvtResults *pvtRes = &(rx->pvtResults);
	double trxFrac;

	/* if the receiver time is already used for the PVT scheduling,
	 * update rxTowForNextPvt and rxWnForNextPvt */
	if(pvtData->rxTimeForNextPvtFlag!=INVALID_TIME)
	{
		pvtData->rxTowForNextPvt += cfg->pvtPeriodSeconds;
		if(pvtData->rxTowForNextPvt >= SECONDS_IN_A_WEEK)
		{
			pvtData->rxWnForNextPvt++;
			pvtData->rxTowForNextPvt -= SECONDS_IN_A_WEEK;
		}
		return;
	}
	/* if the receiver time is available for the first time,
	 * initialize rxTowForNextPvt and rxWnForNextPvt */
	if(pvtRes->rxGpsTime.referenceValid==TRUE_FLAG)
	{
		pvtData->rxTimeForNextPvtFlag=GPS_TIME;
		pvtData->rxWnForNextPvt = pvtRes->rxGpsTime.referenceWn;
		pvtData->rxTowForNextPvt = pvtRes->rxGpsTime.referenceTow;
	}
	else if(pvtRes->rxGalTime.referenceValid==TRUE_FLAG)
	{
		pvtData->rxTimeForNextPvtFlag=GALILEO_TIME;
		pvtData->rxWnForNextPvt = pvtRes->rxGalTime.referenceWn;
		pvtData->rxTowForNextPvt = pvtRes->rxGalTime.referenceTow;
	}
	// set the time for the next PVT only when a reference time is valid
	if(pvtData->rxTimeForNextPvtFlag!=INVALID_TIME)
	{
		trxFrac = pvtData->rxTowForNextPvt - floor(pvtData->rxTowForNextPvt);
		trxFrac *= cfg->pvtConfiguration.pvtRate;
		pvtData->rxTowForNextPvt = floor(pvtData->rxTowForNextPvt) + ((floor(trxFrac)+1.0)/cfg->pvtConfiguration.pvtRate);
		if(pvtData->rxTowForNextPvt >= SECONDS_IN_A_WEEK)
		{
			pvtData->rxWnForNextPvt++;
			pvtData->rxTowForNextPvt -= SECONDS_IN_A_WEEK;
		}
	}
	// else the sample counter will be used to schedule the next PVT (in TimeForANewPvt)
}


void ComputePseudorangeResiduals(TPvtData *pvtData,TPvtResults *pvtRes)
{
	int index;
	int nCh = pvtData->numOfOperativeChannels;
	TPvtChannel *pvtCh;
	TChannel *ch;
	TSatStatus *satStatus;
	double bias;
	double residual;

	if(pvtData->pvtLogFile!=NULL)
	{
		fprintf(pvtData->pvtLogFile,"\n");
		fprintf(pvtData->pvtLogFile,"Pseudorange residuals\n");
	}

	// computation
	for(index=0;index<nCh;index++)
	{
		pvtCh = &(pvtData->pvtChArray[index]);
		ch = pvtCh->ch;
		satStatus = pvtCh->satStatus;
		bias = (ch->satType==GPS_SAT) ? pvtRes->clockBiasGps : pvtRes->clockBiasGalileo;
		residual  = ((satStatus->ecefSatPosRot[0]-pvtRes->ecefPosition[0])*(satStatus->ecefSatPosRot[0]-pvtRes->ecefPosition[0]));
		residual += ((satStatus->ecefSatPosRot[1]-pvtRes->ecefPosition[1])*(satStatus->ecefSatPosRot[1]-pvtRes->ecefPosition[1]));
		residual += ((satStatus->ecefSatPosRot[2]-pvtRes->ecefPosition[2])*(satStatus->ecefSatPosRot[2]-pvtRes->ecefPosition[2]));
		residual = sqrt(residual);
		residual -= (pvtCh->pseudorangeCsm-bias);
		pvtCh->pseudorangeResidual = residual;
		// PVT text log
		if(pvtData->pvtLogFile!=NULL)
		{
			if(ch->satType==GPS_SAT)
				fprintf(pvtData->pvtLogFile,"[GPS] ");
			else
				fprintf(pvtData->pvtLogFile,"[GAL] ");
			fprintf(pvtData->pvtLogFile,"CHANNEL:%2d  SAT:%2d  residual: %.1f m\n",ch->chIndex,ch->satId,pvtCh->pseudorangeResidual);
		}
	}

	if(pvtData->pvtLogFile!=NULL)
	{
		fprintf(pvtData->pvtLogFile,"\n\n");
	}
}


void PrintPvtResults(TNGeneConfiguration *cfg,TNGeneData *rx,int performedPvt)
{
	TPvtData *pvtData = &(rx->pvtData);
	TPvtResults *pvtRes = &(rx->pvtResults);
	TDateTime curTime;
	double velocity = sqrt(pow(pvtRes->ecefVelocity[0],2.0)+pow(pvtRes->ecefVelocity[1],2.0)+pow(pvtRes->ecefVelocity[2],2.0));
	// for the binary log
	unsigned int pvtIndex;
	int chIndex;
	double invalidDouble = -1.0;
	int invalidInt = -1;
	TPvtChannel *pvtCh;
	TChannel *ch;
	TSatStatus *satStatus;
	double timeTag;
	

	/* preliminary check */
	if((cfg==NULL) || (rx==NULL))
	{
		// pointer(s) not valid: do nothing
		return;
	}

	/* standard log */
	SwrxLog(NGENE_LOG_SEVERITY_INFO,"############### PVT COUNTER  %4llu ################",pvtData->pvtIndex);
	if(pvtRes->rxUtcTime.referenceUpdated==TRUE_FLAG)
	{
		if(pvtRes->utcFromGps==TRUE_FLAG)
		{
			FromGnssTimeToDate(GPS_SAT,pvtRes->rxUtcTime.referenceWn,pvtRes->rxUtcTime.referenceTow,&curTime);
			SwrxLog(NGENE_LOG_SEVERITY_INFO,"# UTC %02d/%02d/%4d  %02d:%02d:%04.1f",curTime.day,curTime.month,curTime.year,curTime.hour,curTime.minute,curTime.second);
		}
		if(pvtRes->utcFromGal==TRUE_FLAG)
		{
			FromGnssTimeToDate(GALILEO_SAT,pvtRes->rxUtcTime.referenceWn,pvtRes->rxUtcTime.referenceTow,&curTime);
			SwrxLog(NGENE_LOG_SEVERITY_INFO,"# UTC %02d/%02d/%4d  %02d:%02d:%04.1f",curTime.day,curTime.month,curTime.year,curTime.hour,curTime.minute,curTime.second);
		}
	}
	else if((pvtRes->utcFromGps==TRUE_FLAG) && (pvtData->rxUtcTimeValid==TRUE_FLAG))
	{
		FromGnssTimeToDate(GPS_SAT,pvtData->rxUtcWn,pvtData->rxUtcTow,&curTime);
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"# UTC %02d/%02d/%4d  %02d:%02d:%04.1f",curTime.day,curTime.month,curTime.year,curTime.hour,curTime.minute,curTime.second);
	}
	else if((pvtRes->utcFromGal==TRUE_FLAG) && (pvtData->rxUtcTimeValid==TRUE_FLAG))
	{
		FromGnssTimeToDate(GALILEO_SAT,pvtData->rxUtcWn,pvtData->rxUtcTow,&curTime);
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"# UTC %02d/%02d/%4d  %02d:%02d:%04.1f",curTime.day,curTime.month,curTime.year,curTime.hour,curTime.minute,curTime.second);
	}
	SwrxLog(NGENE_LOG_SEVERITY_INFO,"# RX      : %llu ms", rx->rxCounter);
	if(pvtRes->rxUtcTime.referenceUpdated==TRUE_FLAG)
	{
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"# UTC  WN : %4d  TOW : %11.4f", pvtRes->rxUtcTime.referenceWn, pvtRes->rxUtcTime.referenceTow);
	}
	else if(pvtData->rxUtcTimeValid==TRUE_FLAG)
	{
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"# UTC  WN : %4d  TOW : %11.4f", pvtData->rxUtcWn, pvtData->rxUtcTow);
	}
	if(pvtRes->rxGpsTime.referenceUpdated==TRUE_FLAG)
	{
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"# GPS  WN : %4d  TOW : %11.4f", pvtRes->rxGpsTime.referenceWn, pvtRes->rxGpsTime.referenceTow);
	}
	else if(pvtData->rxGpsTimeValid==TRUE_FLAG)
	{
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"# GPS  WN : %4d  TOW : %11.4f", pvtData->rxGpsWn, pvtData->rxGpsTow);
	}
	if(pvtRes->rxGalTime.referenceUpdated==TRUE_FLAG)
	{
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"# Gal  WN : %4d  TOW : %11.4f", pvtRes->rxGalTime.referenceWn, pvtRes->rxGalTime.referenceTow);
	}
	else if(pvtData->rxGalTimeValid==TRUE_FLAG)
	{
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"# Gal  WN : %4d  TOW : %11.4f", pvtData->rxGalWn, pvtData->rxGalTow);
	}
	SwrxLog(NGENE_LOG_SEVERITY_INFO,"# Galileo : %d satellites", pvtData->numOfGalileoOpChannels);
	SwrxLog(NGENE_LOG_SEVERITY_INFO,"# GPS     : %d satellites", pvtData->numOfGpsOpChannels);
	SwrxLog(NGENE_LOG_SEVERITY_INFO,"# GDOP    : %f", pvtRes->dop[GDOP_INDEX_IN_DOP_ARRAY]);
	if(performedPvt==LMS_BASED_PVT)
	{
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"############### PVT RESULTS (LS) #################");
	}
	else if(performedPvt==KALMAN_BASED_PVT)
	{
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"############### PVT RESULTS (KF) #################");
	}
	SwrxLog(NGENE_LOG_SEVERITY_INFO,"# Lon: %fd   Lat: %fd   Alt: %6.2fm", pvtRes->geodPosition[0],pvtRes->geodPosition[1],pvtRes->geodPosition[2]);
	SwrxLog(NGENE_LOG_SEVERITY_INFO,"# V : %.3f m/s (%.3f km/h)",velocity,velocity*3.6);
	SwrxLog(NGENE_LOG_SEVERITY_INFO,"# Vx: %.3f m/s   Vy: %.3f m/s   Vz: %.3f m/s", pvtRes->ecefVelocity[0],pvtRes->ecefVelocity[1],pvtRes->ecefVelocity[2]);
	if(pvtRes->clockBiasGpsValid==TRUE_FLAG)
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"# Bias GPS: %.3f m", pvtRes->clockBiasGps);
	if(pvtRes->clockBiasGalileoValid==TRUE_FLAG)
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"# Bias Galileo: %.3f m", pvtRes->clockBiasGalileo);
	SwrxLog(NGENE_LOG_SEVERITY_INFO,"# Drift: %.3f m/s", pvtRes->clockDrift);
	SwrxLog(NGENE_LOG_SEVERITY_INFO,"##################################################\n");

	/* KML for Google Earth */
	if(pvtRes->kmlFile!=NULL)
	{
		#ifndef LINESTRING_KML
		fprintf(pvtRes->kmlFile, "<Placemark><styleUrl>#defaultPlacemark</styleUrl>\n");
		if(pvtRes->rxUtcTime.referenceUpdated==TRUE_FLAG)
		{
			fprintf(pvtRes->kmlFile, "<name>%9.2f</name>\n", pvtRes->rxUtcTime.referenceTow);
		}
		else if(pvtData->rxUtcTimeValid==TRUE_FLAG)
		{
			fprintf(pvtRes->kmlFile, "<name>%9.2f</name>\n", pvtData->rxUtcTow);
		}
		else if(pvtRes->rxGpsTime.referenceValid==TRUE_FLAG)
		{
			fprintf(pvtRes->kmlFile, "<name>%9.2f</name>\n", pvtRes->rxGpsTime.referenceTow);
		}
		else if(pvtData->rxGpsTimeValid==TRUE_FLAG)
		{
			fprintf(pvtRes->kmlFile, "<name>%9.2f</name>\n", pvtData->rxGpsTow);
		}
		else if(pvtRes->rxGalTime.referenceValid==TRUE_FLAG)
		{
			fprintf(pvtRes->kmlFile, "<name>%9.2f</name>\n", pvtRes->rxGalTime.referenceTow);
		}
		else if(pvtData->rxGalTimeValid==TRUE_FLAG)
		{
			fprintf(pvtRes->kmlFile, "<name>%9.2f</name>\n", pvtData->rxGalTow);
		}
		else
		{
			fprintf(pvtRes->kmlFile, "<name>%llu</name>\n", pvtData->pvtIndex);
		}
		fprintf(pvtRes->kmlFile, "<Point>\n");
		fprintf(pvtRes->kmlFile, "<coordinates>\n");
		fprintf(pvtRes->kmlFile,"%f, ", pvtRes->geodPosition[0]);
		fprintf(pvtRes->kmlFile,"%f, ", pvtRes->geodPosition[1]);
		fprintf(pvtRes->kmlFile,"%f\n", pvtRes->geodPosition[2]);
		fprintf(pvtRes->kmlFile, "</coordinates>\n");
		fprintf(pvtRes->kmlFile, "</Point>\n");
		fprintf(pvtRes->kmlFile, "</Placemark>\n");
		#else
		fprintf(pvtRes->kmlFile,"\t%.20f,%.20f,%.20f \n",pvtRes->geodPosition[0],pvtRes->geodPosition[1],pvtRes->geodPosition[2]);
		#endif
	}

	/* optional PVT text log */
	if(pvtData->pvtLogFile!=NULL)
	{
		fprintf(pvtData->pvtLogFile,"############### PVT COUNTER  %4llu ################\n",pvtData->pvtIndex);
		if(pvtRes->rxUtcTime.referenceUpdated==TRUE_FLAG)
		{
			if(pvtRes->utcFromGps==TRUE_FLAG)
			{
				FromGnssTimeToDate(GPS_SAT,pvtRes->rxUtcTime.referenceWn,pvtRes->rxUtcTime.referenceTow,&curTime);
				fprintf(pvtData->pvtLogFile,"# UTC %02d/%02d/%4d  %02d:%02d:%04.1f\n",curTime.day,curTime.month,curTime.year,curTime.hour,curTime.minute,curTime.second);
			}
			if(pvtRes->utcFromGal==TRUE_FLAG)
			{
				FromGnssTimeToDate(GALILEO_SAT,pvtRes->rxUtcTime.referenceWn,pvtRes->rxUtcTime.referenceTow,&curTime);
				fprintf(pvtData->pvtLogFile,"# UTC %02d/%02d/%4d  %02d:%02d:%04.1f\n",curTime.day,curTime.month,curTime.year,curTime.hour,curTime.minute,curTime.second);
			}
		}
		else if((pvtRes->utcFromGps==TRUE_FLAG) && (pvtData->rxUtcTimeValid==TRUE_FLAG))
		{
			FromGnssTimeToDate(GPS_SAT,pvtData->rxUtcWn,pvtData->rxUtcTow,&curTime);
			fprintf(pvtData->pvtLogFile,"# UTC %02d/%02d/%4d  %02d:%02d:%04.1f\n",curTime.day,curTime.month,curTime.year,curTime.hour,curTime.minute,curTime.second);
		}
		else if((pvtRes->utcFromGal==TRUE_FLAG) && (pvtData->rxUtcTimeValid==TRUE_FLAG))
		{
			FromGnssTimeToDate(GALILEO_SAT,pvtData->rxUtcWn,pvtData->rxUtcTow,&curTime);
			fprintf(pvtData->pvtLogFile,"# UTC %02d/%02d/%4d  %02d:%02d:%04.1f\n",curTime.day,curTime.month,curTime.year,curTime.hour,curTime.minute,curTime.second);
		}
		fprintf(pvtData->pvtLogFile,"# RX      : %llu ms\n", rx->rxCounter);
		if(pvtRes->rxUtcTime.referenceUpdated==TRUE_FLAG)
		{
			fprintf(pvtData->pvtLogFile,"# UTC  WN : %4d  TOW : %11.4f\n", pvtRes->rxUtcTime.referenceWn, pvtRes->rxUtcTime.referenceTow);
		}
		else if(pvtData->rxUtcTimeValid==TRUE_FLAG)
		{
			fprintf(pvtData->pvtLogFile,"# UTC  WN : %4d  TOW : %11.4f\n", pvtData->rxUtcWn, pvtData->rxUtcTow);
		}
		if(pvtRes->rxGpsTime.referenceUpdated==TRUE_FLAG)
		{
			fprintf(pvtData->pvtLogFile,"# GPS  WN : %4d  TOW : %11.4f\n", pvtRes->rxGpsTime.referenceWn, pvtRes->rxGpsTime.referenceTow);
		}
		else if(pvtData->rxGpsTimeValid==TRUE_FLAG)
		{
			fprintf(pvtData->pvtLogFile,"# GPS  WN : %4d  TOW : %11.4f\n", pvtData->rxGpsWn, pvtData->rxGpsTow);
		}
		if(pvtRes->rxGalTime.referenceUpdated==TRUE_FLAG)
		{
			fprintf(pvtData->pvtLogFile,"# Gal  WN : %4d  TOW : %11.4f\n", pvtRes->rxGalTime.referenceWn, pvtRes->rxGalTime.referenceTow);
		}
		else if(pvtData->rxGalTimeValid==TRUE_FLAG)
		{
			fprintf(pvtData->pvtLogFile,"# Gal  WN : %4d  TOW : %11.4f\n", pvtData->rxGalWn, pvtData->rxGalTow);
		}
		fprintf(pvtData->pvtLogFile,"# Galileo : %d satellites\n", pvtData->numOfGalileoOpChannels);
		fprintf(pvtData->pvtLogFile,"# GPS     : %d satellites\n", pvtData->numOfGpsOpChannels);
		fprintf(pvtData->pvtLogFile,"# GDOP    : %f\n", pvtRes->dop[GDOP_INDEX_IN_DOP_ARRAY]);
		if(performedPvt==LMS_BASED_PVT)
		{
			fprintf(pvtData->pvtLogFile,"############### PVT RESULTS (LS) #################\n");
		}
		else if(performedPvt==KALMAN_BASED_PVT)
		{
			fprintf(pvtData->pvtLogFile,"############### PVT RESULTS (KF) #################\n");
		}
		fprintf(pvtData->pvtLogFile,"# Lon: %fd   Lat: %fd   Alt: %6.2fm\n", pvtRes->geodPosition[0],pvtRes->geodPosition[1],pvtRes->geodPosition[2]);
		fprintf(pvtData->pvtLogFile,"# V : %.3f m/s (%.3f km/h)\n",velocity,velocity*3.6);
		fprintf(pvtData->pvtLogFile,"# Vx: %.3f m/s   Vy: %.3f m/s   Vz: %.3f m/s\n", pvtRes->ecefVelocity[0],pvtRes->ecefVelocity[1],pvtRes->ecefVelocity[2]);
		if(pvtRes->clockBiasGpsValid==TRUE_FLAG)
			fprintf(pvtData->pvtLogFile,"# Bias GPS: %.3f m\n", pvtRes->clockBiasGps);
		if(pvtRes->clockBiasGalileoValid==TRUE_FLAG)
			fprintf(pvtData->pvtLogFile,"# Bias Galileo: %.3f m\n", pvtRes->clockBiasGalileo);
		fprintf(pvtData->pvtLogFile,"# Drift: %.3f m/s\n", pvtRes->clockDrift);
		fprintf(pvtData->pvtLogFile,"##################################################\n\n");
	}

	/* optional PVT binary log */
	if(pvtData->pvtBinFile!=NULL)
	{
		pvtIndex = (unsigned int)pvtData->pvtIndex;
		fwrite(&pvtIndex,sizeof(unsigned int),1,pvtData->pvtBinFile);
		if(pvtRes->rxUtcTime.referenceUpdated==TRUE_FLAG)
		{
			fwrite(&(pvtRes->rxUtcTime.referenceTow),sizeof(double),1,pvtData->pvtBinFile);
		}
		else
		{
			fwrite(&invalidDouble,sizeof(double),1,pvtData->pvtBinFile);
		}
		if(pvtRes->rxGpsTime.referenceUpdated==TRUE_FLAG)
		{
			fwrite(&(pvtRes->rxGpsTime.referenceTow),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(pvtRes->rxGpsTime.referenceWn),sizeof(int),1,pvtData->pvtBinFile);
		}
		else
		{
			fwrite(&invalidDouble,sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&invalidInt,sizeof(int),1,pvtData->pvtBinFile);
		}
		if(pvtRes->rxGalTime.referenceUpdated==TRUE_FLAG)
		{
			fwrite(&(pvtRes->rxGalTime.referenceTow),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(pvtRes->rxGalTime.referenceWn),sizeof(int),1,pvtData->pvtBinFile);
		}
		else
		{
			fwrite(&invalidDouble,sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&invalidInt,sizeof(int),1,pvtData->pvtBinFile);
		}
		fwrite(pvtRes->dop,sizeof(double),5,pvtData->pvtBinFile);
		fwrite(pvtRes->ecefPosition,sizeof(double),3,pvtData->pvtBinFile);
		fwrite(pvtRes->geodPosition,sizeof(double),3,pvtData->pvtBinFile);
		fwrite(pvtRes->ecefVelocity,sizeof(double),3,pvtData->pvtBinFile);
		fwrite(&(pvtRes->clockBiasGpsValid),sizeof(int),1,pvtData->pvtBinFile);
		fwrite(&(pvtRes->clockBiasGps),sizeof(double),1,pvtData->pvtBinFile);
		fwrite(&(pvtRes->clockBiasGalileoValid),sizeof(int),1,pvtData->pvtBinFile);
		fwrite(&(pvtRes->clockBiasGalileo),sizeof(double),1,pvtData->pvtBinFile);
		fwrite(&(pvtRes->clockDrift),sizeof(double),1,pvtData->pvtBinFile);
		timeTag=(double)rx->buf.discardedSamples/(double) cfg->fs;
		fwrite(&(timeTag),sizeof(double),1,pvtData->pvtBinFile);
		fwrite(&(pvtData->numOfChannelsReadyForPVT),sizeof(int),1,pvtData->pvtBinFile);
		fwrite(&(pvtData->numOfGpsChannelsReadyForPVT),sizeof(int),1,pvtData->pvtBinFile);
		fwrite(&(pvtData->numOfGalileoChannelsReadyForPVT),sizeof(int),1,pvtData->pvtBinFile);
		fwrite(&(pvtData->numOfOperativeChannels),sizeof(int),1,pvtData->pvtBinFile);
		fwrite(&(cfg->pvtConfiguration.egnosFlag),sizeof(int),1,pvtData->pvtBinFile);
		for(chIndex=0;chIndex<pvtData->numOfOperativeChannels;chIndex++)
		{
			pvtCh = &(pvtData->pvtChArray[chIndex]);
			ch = pvtCh->ch;
			satStatus = pvtCh->satStatus;
			fwrite(&(ch->satType),sizeof(int),1,pvtData->pvtBinFile);
			fwrite(&(ch->satId),sizeof(int),1,pvtData->pvtBinFile);
			fwrite(&(pvtCh->transmissionTime),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(pvtCh->wn),sizeof(int),1,pvtData->pvtBinFile);
			fwrite(&(pvtCh->deltaPseudorange),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(pvtCh->pseudorangeRaw),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(pvtCh->satClkCorr),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(pvtCh->tropoCorrection),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(pvtCh->ionoCorrection),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(pvtData->csmData[ch->chIndex].csmMeasure),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(pvtCh->pseudorangeCsm),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(satStatus->ecefSatPos,sizeof(double),3,pvtData->pvtBinFile);
			fwrite(satStatus->ecefSatVel,sizeof(double),3,pvtData->pvtBinFile);
			fwrite(&(satStatus->azimuth),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(satStatus->elevation),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(pvtData->csmData[ch->chIndex].lpcOut),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(pvtCh->ionoCorrectionFromEgnos),sizeof(char),1,pvtData->pvtBinFile);
			fwrite(&(pvtCh->egnosFastCorrection),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(pvtCh->egnosClockCorr),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(pvtCh->egnosXCorr),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(pvtCh->egnosYCorr),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(pvtCh->egnosZcorr),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(ch->tracking.trkStatusChecker.cnoEstimator.estimatedCNo),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(pvtCh->pseudorangeResidual),sizeof(double),1,pvtData->pvtBinFile);
		}
	}

	/* Optional RINEX Observation data file */
	RinexSaveObs(cfg,rx);

	/* Optional NMEA file */
	NmeaSavePvt(cfg,rx);
}


void PrintBadPvtResults(TNGeneConfiguration *cfg,TNGeneData *rx,int performedPvt)
{
	TPvtData *pvtData = &(rx->pvtData);
	TPvtResults *pvtRes = &(rx->pvtTemporaryResults);
	TDateTime curTime;
	double velocity = sqrt(pow(pvtRes->ecefVelocity[0],2.0)+pow(pvtRes->ecefVelocity[1],2.0)+pow(pvtRes->ecefVelocity[2],2.0));
	// for the binary log
	unsigned int pvtIndex;
	int chIndex;
	double invalidDouble = -1.0;
	int invalidInt = -1;
	TPvtChannel *pvtCh;
	TChannel *ch;
	TSatStatus *satStatus;
	double timeTag;


	/* preliminary check */
	if((cfg==NULL) || (rx==NULL))
	{
		// pointer(s) not valid: do nothing
		return;
	}

	/* optional PVT text log */
	if(pvtData->pvtLogFile!=NULL)
	{
		fprintf(pvtData->pvtLogFile,"############### PVT COUNTER  %4llu ################\n",pvtData->pvtIndex);
		if(pvtRes->rxUtcTime.referenceUpdated==TRUE_FLAG)
		{
			if(pvtRes->utcFromGps==TRUE_FLAG)
			{
				FromGnssTimeToDate(GPS_SAT,pvtRes->rxUtcTime.referenceWn,pvtRes->rxUtcTime.referenceTow,&curTime);
				fprintf(pvtData->pvtLogFile,"# UTC %02d/%02d/%4d  %02d:%02d:%04.1f\n",curTime.day,curTime.month,curTime.year,curTime.hour,curTime.minute,curTime.second);
			}
			if(pvtRes->utcFromGal==TRUE_FLAG)
			{
				FromGnssTimeToDate(GALILEO_SAT,pvtRes->rxUtcTime.referenceWn,pvtRes->rxUtcTime.referenceTow,&curTime);
				fprintf(pvtData->pvtLogFile,"# UTC %02d/%02d/%4d  %02d:%02d:%04.1f\n",curTime.day,curTime.month,curTime.year,curTime.hour,curTime.minute,curTime.second);
			}
		}
		else if((pvtRes->utcFromGps==TRUE_FLAG) && (pvtData->rxUtcTimeValid==TRUE_FLAG))
		{
			FromGnssTimeToDate(GPS_SAT,pvtData->rxUtcWn,pvtData->rxUtcTow,&curTime);
			fprintf(pvtData->pvtLogFile,"# UTC %02d/%02d/%4d  %02d:%02d:%04.1f\n",curTime.day,curTime.month,curTime.year,curTime.hour,curTime.minute,curTime.second);
		}
		else if((pvtRes->utcFromGal==TRUE_FLAG) && (pvtData->rxUtcTimeValid==TRUE_FLAG))
		{
			FromGnssTimeToDate(GALILEO_SAT,pvtData->rxUtcWn,pvtData->rxUtcTow,&curTime);
			fprintf(pvtData->pvtLogFile,"# UTC %02d/%02d/%4d  %02d:%02d:%04.1f\n",curTime.day,curTime.month,curTime.year,curTime.hour,curTime.minute,curTime.second);
		}
		fprintf(pvtData->pvtLogFile,"# RX      : %llu ms\n", rx->rxCounter);
		if(pvtRes->rxUtcTime.referenceUpdated==TRUE_FLAG)
		{
			fprintf(pvtData->pvtLogFile,"# UTC  WN : %4d  TOW : %11.4f\n", pvtRes->rxUtcTime.referenceWn, pvtRes->rxUtcTime.referenceTow);
		}
		else if(pvtData->rxUtcTimeValid==TRUE_FLAG)
		{
			fprintf(pvtData->pvtLogFile,"# UTC  WN : %4d  TOW : %11.4f\n", pvtData->rxUtcWn, pvtData->rxUtcTow);
		}
		if(pvtRes->rxGpsTime.referenceUpdated==TRUE_FLAG)
		{
			fprintf(pvtData->pvtLogFile,"# GPS  WN : %4d  TOW : %11.4f\n", pvtRes->rxGpsTime.referenceWn, pvtRes->rxGpsTime.referenceTow);
		}
		else if(pvtData->rxGpsTimeValid==TRUE_FLAG)
		{
			fprintf(pvtData->pvtLogFile,"# GPS  WN : %4d  TOW : %11.4f\n", pvtData->rxGpsWn, pvtData->rxGpsTow);
		}
		if(pvtRes->rxGalTime.referenceUpdated==TRUE_FLAG)
		{
			fprintf(pvtData->pvtLogFile,"# Gal  WN : %4d  TOW : %11.4f\n", pvtRes->rxGalTime.referenceWn, pvtRes->rxGalTime.referenceTow);
		}
		else if(pvtData->rxGalTimeValid==TRUE_FLAG)
		{
			fprintf(pvtData->pvtLogFile,"# Gal  WN : %4d  TOW : %11.4f\n", pvtData->rxGalWn, pvtData->rxGalTow);
		}
		fprintf(pvtData->pvtLogFile,"# Galileo : %d satellites\n", pvtData->numOfGalileoOpChannels);
		fprintf(pvtData->pvtLogFile,"# GPS     : %d satellites\n", pvtData->numOfGpsOpChannels);
		fprintf(pvtData->pvtLogFile,"# GDOP    : %f\n", pvtRes->dop[GDOP_INDEX_IN_DOP_ARRAY]);
		if(performedPvt==LMS_BASED_PVT)
		{
			fprintf(pvtData->pvtLogFile,"############### PVT RESULTS (LS) #################\n");
		}
		else if(performedPvt==KALMAN_BASED_PVT)
		{
			fprintf(pvtData->pvtLogFile,"############### PVT RESULTS (KF) #################\n");
		}
		fprintf(pvtData->pvtLogFile,"# Lon: %fd   Lat: %fd   Alt: %6.2fm\n", pvtRes->geodPosition[0],pvtRes->geodPosition[1],pvtRes->geodPosition[2]);
		fprintf(pvtData->pvtLogFile,"# V : %.3f m/s (%.3f km/h)\n",velocity,velocity*3.6);
		fprintf(pvtData->pvtLogFile,"# Vx: %.3f m/s   Vy: %.3f m/s   Vz: %.3f m/s\n", pvtRes->ecefVelocity[0],pvtRes->ecefVelocity[1],pvtRes->ecefVelocity[2]);
		if(pvtRes->clockBiasGpsValid==TRUE_FLAG)
			fprintf(pvtData->pvtLogFile,"# Bias GPS: %.3f m\n", pvtRes->clockBiasGps);
		if(pvtRes->clockBiasGalileoValid==TRUE_FLAG)
			fprintf(pvtData->pvtLogFile,"# Bias Galileo: %.3f m\n", pvtRes->clockBiasGalileo);
		fprintf(pvtData->pvtLogFile,"# Drift: %.3f m/s\n", pvtRes->clockDrift);
		fprintf(pvtData->pvtLogFile,"##################################################\n\n");
	}

	/* optional PVT binary log */
	if(pvtData->pvtBinFile!=NULL)
	{
		pvtIndex = (unsigned int)pvtData->pvtIndex;
		fwrite(&pvtIndex,sizeof(unsigned int),1,pvtData->pvtBinFile);
		if(pvtRes->rxUtcTime.referenceUpdated==TRUE_FLAG)
		{
			fwrite(&(pvtRes->rxUtcTime.referenceTow),sizeof(double),1,pvtData->pvtBinFile);
		}
		else
		{
			fwrite(&invalidDouble,sizeof(double),1,pvtData->pvtBinFile);
		}
		if(pvtRes->rxGpsTime.referenceUpdated==TRUE_FLAG)
		{
			fwrite(&(pvtRes->rxGpsTime.referenceTow),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(pvtRes->rxGpsTime.referenceWn),sizeof(int),1,pvtData->pvtBinFile);
		}
		else
		{
			fwrite(&invalidDouble,sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&invalidInt,sizeof(int),1,pvtData->pvtBinFile);
		}
		if(pvtRes->rxGalTime.referenceUpdated==TRUE_FLAG)
		{
			fwrite(&(pvtRes->rxGalTime.referenceTow),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(pvtRes->rxGalTime.referenceWn),sizeof(int),1,pvtData->pvtBinFile);
		}
		else
		{
			fwrite(&invalidDouble,sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&invalidInt,sizeof(int),1,pvtData->pvtBinFile);
		}
		fwrite(pvtRes->dop,sizeof(double),5,pvtData->pvtBinFile);
		fwrite(pvtRes->ecefPosition,sizeof(double),3,pvtData->pvtBinFile);
		fwrite(pvtRes->geodPosition,sizeof(double),3,pvtData->pvtBinFile);
		fwrite(pvtRes->ecefVelocity,sizeof(double),3,pvtData->pvtBinFile);
		fwrite(&(pvtRes->clockBiasGpsValid),sizeof(int),1,pvtData->pvtBinFile);
		fwrite(&(pvtRes->clockBiasGps),sizeof(double),1,pvtData->pvtBinFile);
		fwrite(&(pvtRes->clockBiasGalileoValid),sizeof(int),1,pvtData->pvtBinFile);
		fwrite(&(pvtRes->clockBiasGalileo),sizeof(double),1,pvtData->pvtBinFile);
		fwrite(&(pvtRes->clockDrift),sizeof(double),1,pvtData->pvtBinFile);
		timeTag=(double)rx->buf.discardedSamples/(double) cfg->fs;
		fwrite(&(timeTag),sizeof(double),1,pvtData->pvtBinFile);
		fwrite(&(pvtData->numOfChannelsReadyForPVT),sizeof(int),1,pvtData->pvtBinFile);
		fwrite(&(pvtData->numOfGpsChannelsReadyForPVT),sizeof(int),1,pvtData->pvtBinFile);
		fwrite(&(pvtData->numOfGalileoChannelsReadyForPVT),sizeof(int),1,pvtData->pvtBinFile);
		fwrite(&(pvtData->numOfOperativeChannels),sizeof(int),1,pvtData->pvtBinFile);
		fwrite(&(cfg->pvtConfiguration.egnosFlag),sizeof(int),1,pvtData->pvtBinFile);
		for(chIndex=0;chIndex<pvtData->numOfOperativeChannels;chIndex++)
		{
			pvtCh = &(pvtData->pvtChArray[chIndex]);
			ch = pvtCh->ch;
			satStatus = pvtCh->satStatus;
			fwrite(&(ch->satType),sizeof(int),1,pvtData->pvtBinFile);
			fwrite(&(ch->satId),sizeof(int),1,pvtData->pvtBinFile);
			fwrite(&(pvtCh->transmissionTime),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(pvtCh->wn),sizeof(int),1,pvtData->pvtBinFile);
			fwrite(&(pvtCh->deltaPseudorange),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(pvtCh->pseudorangeRaw),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(pvtCh->satClkCorr),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(pvtCh->tropoCorrection),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(pvtCh->ionoCorrection),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(pvtData->csmData[ch->chIndex].csmMeasure),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(pvtCh->pseudorangeCsm),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(satStatus->ecefSatPos,sizeof(double),3,pvtData->pvtBinFile);
			fwrite(satStatus->ecefSatVel,sizeof(double),3,pvtData->pvtBinFile);
			fwrite(&(satStatus->azimuth),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(satStatus->elevation),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(pvtData->csmData[ch->chIndex].lpcOut),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(pvtCh->ionoCorrectionFromEgnos),sizeof(char),1,pvtData->pvtBinFile);
			fwrite(&(pvtCh->egnosFastCorrection),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(pvtCh->egnosClockCorr),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(pvtCh->egnosXCorr),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(pvtCh->egnosYCorr),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(pvtCh->egnosZcorr),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(ch->tracking.trkStatusChecker.cnoEstimator.estimatedCNo),sizeof(double),1,pvtData->pvtBinFile);
			fwrite(&(pvtCh->pseudorangeResidual),sizeof(double),1,pvtData->pvtBinFile);
		}
	}
}


void PerformGnssTimeAnalysis(TNGeneData *rx)
{
	TPvtData *pvtData = &(rx->pvtData);
	TPvtResults *pvtRes = &(rx->pvtResults);
	double timeDiff;
	double ggto;

	if(pvtData->gnssTimeLogFile==NULL)
	{
		return;
	}

	/* get time values */
	if((pvtRes->rxGpsTime.referenceValid==FALSE_FLAG) || (pvtRes->rxGalTime.referenceValid==FALSE_FLAG))
	{
		/* to continue with the analysis, both time values must be available */
		return;
	}
	if(pvtRes->rxGpsTime.referenceSample != pvtRes->rxGalTime.referenceSample)
	{
		/* if GPS and Galileo time values are not updated at the same time (e.g. Galileo not used in the PVT),
		 the analyis is not performed */
		 return;
	}
	timeDiff = pvtRes->rxGalTime.referenceTow - pvtRes->rxGpsTime.referenceTow;
	/* if possible, get GGTO */
	if(GetGstToGpsTimeOffset(rx,pvtRes->rxGalTime.referenceWn,pvtRes->rxGalTime.referenceTow,&ggto)==FALSE_FLAG)
	{
		/* to continue with the analysis, GGTO must be available */
		return;
	}

	/* now plot the time analysis */
	fprintf(pvtData->gnssTimeLogFile,"%10llu  ",rx->buf.discardedSamples);
	fprintf(pvtData->gnssTimeLogFile, "%9.2f  ",pvtRes->rxGalTime.referenceTow); // [s]
	fprintf(pvtData->gnssTimeLogFile, "%9.2f  ",pvtRes->rxGpsTime.referenceTow); // [s]
	fprintf(pvtData->gnssTimeLogFile, "%9.2f  ",timeDiff * 1.0e9); // [ns]
	fprintf(pvtData->gnssTimeLogFile, "%9.2f\n",ggto * 1.0e9); // [ns]
}


void ExtractPvtInformationForChannels(TNGeneConfiguration *cfg,TNGeneData *rx)
{
	TPvtData *pvtData = &(rx->pvtData);
	TPvtChannel *pvtCh;
	TChannel *ch;
	TPvtInfoAbouChannel *pvtInfo;
	int index;

	ResetPvtInformationForChannels(cfg,rx);
	for(index=0;index<pvtData->numOfOperativeChannels;index++)
	{
		pvtCh = &(pvtData->pvtChArray[index]);
		ch = pvtCh->ch;
		pvtInfo = &(ch->pvtInfo);
		pvtInfo->usedForPVT=TRUE_FLAG;
		pvtInfo->pseudorange = pvtCh->pseudorangeRaw;
		pvtInfo->satElevation = pvtCh->satStatus->elevation;
		pvtInfo->satAzimuth = pvtCh->satStatus->azimuth;
		pvtInfo->satEcefX = pvtCh->satStatus->ecefSatPos[0];
		pvtInfo->satEcefY = pvtCh->satStatus->ecefSatPos[1];
		pvtInfo->satEcefZ = pvtCh->satStatus->ecefSatPos[2];
	}
}


void PositionVelocityConversions(TPvtResults *pvtRes)
{
	Ecef2Geod(pvtRes->ecefPosition,pvtRes->geodPosition);
	/* User Position from geodetic to ENU */
	LlaToUtm(DEGREE_TO_RADIAN*pvtRes->geodPosition[1],DEGREE_TO_RADIAN*pvtRes->geodPosition[0],pvtRes->geodPosition[2],pvtRes->enuPosition, &pvtRes->utmZone);
	/* User Velocity from ECEF to ENU */
	EcefVelToEnuVel(pvtRes->ecefPosition,pvtRes->ecefVelocity,pvtRes->enuPosition,pvtRes->enuVelocity);
}


void CopySatStatusStructure(TSatStatus *dst,TSatStatus *src)
{
	dst->satInformationValid = src->satInformationValid;
	memcpy(dst->ecefSatPos,src->ecefSatPos,3*sizeof(double));
	memcpy(dst->ecefSatVel,src->ecefSatVel,3*sizeof(double));
	dst->ecefSatVelTot = src->ecefSatVelTot;
	dst->azimuth = src->azimuth;
	dst->elevation = src->elevation;
}


void CopyPvtResultsStructure(TPvtResults *dst,TPvtResults *src)
{
	int index;

	dst->pvtValid = src->pvtValid;
	dst->pvtPlotted = src->pvtPlotted;
	dst->referenceSample = src->referenceSample;
	dst->clockBiasGpsValid = src->clockBiasGpsValid;
	dst->clockBiasGalileoValid = src->clockBiasGalileoValid;
	memcpy(dst->ecefPosition,src->ecefPosition,3*sizeof(double));
	memcpy(dst->ecefVelocity,src->ecefVelocity,3*sizeof(double));
	memcpy(dst->geodPosition,src->geodPosition,3*sizeof(double));
	dst->utmZone = src->utmZone;
	memcpy(dst->enuPosition,src->enuPosition,3*sizeof(double));
	memcpy(dst->enuVelocity,src->enuVelocity,3*sizeof(double));
	dst->clockBiasGps = src->clockBiasGps;
	dst->clockBiasGalileo = src->clockBiasGalileo;
	dst->clockBiasGpsCumulated = src->clockBiasGpsCumulated;
	dst->clockBiasGalileoCumulated = src->clockBiasGalileoCumulated;
	dst->clockDrift = src->clockDrift;
	memcpy(dst->dop,src->dop,5*sizeof(double));
	memcpy(&(dst->rxGpsTime),&(src->rxGpsTime),sizeof(TReceiverTime));
	memcpy(&(dst->rxGalTime),&(src->rxGalTime),sizeof(TReceiverTime));
	dst->utcFromGps = src->utcFromGps;
	dst->utcFromGal = src->utcFromGal;
	dst->utcFromDefault = src->utcFromDefault;
	memcpy(&(dst->rxUtcTime),&(src->rxUtcTime),sizeof(TReceiverTime));
	for(index=0;index<GPS_NUMBER_OF_PRNS;index++)
	{
		memcpy(&(dst->gpsSatStatus[index]),&(src->gpsSatStatus[index]),sizeof(TSatStatus));
	}
	for(index=0;index<GALILEO_NUMBER_OF_PRNS;index++)
	{
		memcpy(&(dst->galSatStatus[index]),&(src->galSatStatus[index]),sizeof(TSatStatus));
	}
}


int PvtManager(TNGeneConfiguration *cfg,TNGeneData *rx)
{
	TPvtData *pvtData = &(rx->pvtData);
	TPvtResults *pvtRes = &(rx->pvtResults);
	TPvtResults *pvtTmpRes = &(rx->pvtTemporaryResults);
	int pvtSuccess = FALSE_FLAG;
	int failureFlag = FALSE_FLAG;

	/* Predict the receiver time if possible */
	PredictRxTime(cfg,rx);
	/* Verify whenever the PVT computation is needed */
	if(TimeForANewPvt(cfg,rx)==FALSE_FLAG)
	{
		#ifdef RX_PROFILING
		rx->pvtTime=FALSE_FLAG;
		#endif
		return SUCCESS_RETURN;
	}

	#ifdef RX_PROFILING
	rx->pvtTime=TRUE_FLAG;
	#endif

	/* time for a new PVT, reset validity flags */
	pvtTmpRes->pvtValid = FALSE_FLAG;
	pvtTmpRes->rxUtcTime.referenceUpdated = pvtRes->rxUtcTime.referenceUpdated = FALSE_FLAG;
	pvtTmpRes->rxGpsTime.referenceUpdated = pvtRes->rxGpsTime.referenceUpdated = FALSE_FLAG;
	pvtTmpRes->rxGalTime.referenceUpdated = pvtRes->rxGalTime.referenceUpdated = FALSE_FLAG;
	ResetPvtInformationForChannels(cfg,rx);

	/* Select channels suitable for PVT */
	DeleteOldNavMesg(cfg,rx);
	if(SelectChannelsForPvt(cfg,rx)==FAILURE_RETURN)
	{
		/* Force the CSM to restart for all the channels */
		ClearCsmData(cfg,pvtData,pvtRes);
		/* This PVT computation is skipped */
		SetRxTimeForNextPvt(cfg,rx);
		ResetPvt(cfg,pvtData,pvtRes,FALSE_FLAG);
		ResetPvtInformationForChannels(cfg,rx);
		return FAILURE_RETURN;
	}
	if(pvtData->numOfOperativeChannels==0)
	{
		/* If the PVT is executed without measurements (it happens only with the Kalman filter)
		 * force the CSM to restart for all the channels */
		ClearCsmData(cfg,pvtData,pvtRes);
		return SUCCESS_RETURN;
	}
	/* Copy previous PVT results into temporary variable */
	CopyPvtResultsStructure(pvtTmpRes,pvtRes);
	pvtTmpRes->referenceSample = rx->buf.discardedSamples;
	/* Starts with PVT execution!!! */
	PlotPvtLogFile(rx,pvtData);
	/* Prepare observables */
	ComputeTransmissionTime(cfg,pvtData);
	if(VerifyTransmissionTime(pvtData)==FALSE_FLAG)
	{
		/* This PVT computation is skipped
		 * Also time information is reset in the case something went wrong */
		ResetPvt(cfg,pvtData,pvtRes,FALSE_FLAG);
		ResetPvtInformationForChannels(cfg,rx);
		ResetReferenceRxTime(rx);
		return FAILURE_RETURN;
	}
	ComputeSatsPositions(cfg,rx,pvtData,pvtTmpRes);
	if(ApplyElevationMask(cfg,pvtData)==FALSE_FLAG)
	{
		/* This PVT computation is skipped */
		SetRxTimeForNextPvt(cfg,rx);
		ResetPvt(cfg,pvtData,pvtRes,FALSE_FLAG);
		ResetPvtInformationForChannels(cfg,rx);
		return FAILURE_RETURN;
	}
	/* Urban Canyon Mask */
	if(ApplyUrbanCanyonMask(cfg,pvtData)==FALSE_FLAG)
	{
		/* This PVT computation is skipped */
		SetRxTimeForNextPvt(cfg,rx);
		ResetPvt(cfg,pvtData,pvtRes,FALSE_FLAG);
		ResetPvtInformationForChannels(cfg,rx);
		return FAILURE_RETURN;
	}
	AdjustMissingRxTime(cfg,rx,pvtData);
	ComputePseudoranges(cfg,rx,pvtData,pvtTmpRes);
	PerformCarrierSmoothing(cfg,pvtData,pvtTmpRes);
	DeleteNotSmoothed(cfg,pvtData);
	GetPseudorangeCorrections(cfg,rx,pvtData,pvtTmpRes);
	/* PVT computation */
	if(pvtData->pvtAlgorithm==LMS_BASED_PVT)
	{
		if(PerformLeastSquares(cfg,rx)==FAILURE_RETURN)
			return FAILURE_RETURN;
	}
	else
	{
		if(PerformKalmanFilter(cfg,rx)==FAILURE_RETURN)
			return FAILURE_RETURN;
	}
	/* Find remaining results of PVT computation */
	ComputePseudorangeResiduals(pvtData,pvtTmpRes);
	PositionVelocityConversions(pvtTmpRes);
	pvtTmpRes->pvtValid = TRUE_FLAG;
	pvtTmpRes->pvtPlotted = FALSE_FLAG;
	DopEvaluation(rx,pvtData->pvtAlgorithm);
	SetReferenceRxTime(rx,cfg->navMsgConfiguration.defaultLeapSeconds);
	/* check the PVT result: if an error condition happens, the result is discarded */
	pvtSuccess = CheckPvtResultsGoodness(cfg,pvtData,pvtTmpRes,pvtRes,&failureFlag);
	/* Analyze PVT results */
	if(pvtSuccess==TRUE_FLAG)
	{
		/* The PVT results are good */
		/* All the results obtained have been saved in the pvtTemporaryResults structure.
		 * The PVT results are copied in the pvtResults structure only when no error condition occurs! */
		CopyPvtResultsStructure(pvtRes,pvtTmpRes);
		PrintPvtResults(cfg,rx,pvtData->pvtAlgorithm);
		PerformGnssTimeAnalysis(rx);
		ExtractPvtInformationForChannels(cfg,rx);
		UpdateCumulatedBiases(pvtRes);
		pvtData->pvtIndex++;
		pvtData->numOfPvtFails = 0;
		pvtData->hardRestartMade = FALSE_FLAG;
		/* check the possibility to use Kalman filter */
		if((pvtData->pvtAlgorithm==LMS_BASED_PVT) && (cfg->pvtConfiguration.pvtMethod==KALMAN_BASED_PVT) && (pvtData->lsIndex<NUMBER_OF_LS_BASED_PVTS))
		{
			pvtData->lsIndex++;
			if(pvtData->lsIndex==NUMBER_OF_LS_BASED_PVTS)
			{
				pvtData->pvtAlgorithm = KALMAN_BASED_PVT;
			}
		}
	}
	else
	{
		/* The PVT results are not good: discard! */
		#ifdef SAVE_BAD_PVT_RESULTS
		PrintBadPvtResults(cfg,rx,pvtData->pvtAlgorithm);
		pvtData->pvtIndex++;
		#endif
		ResetPvt(cfg,pvtData,pvtRes,FALSE_FLAG);
		ResetPvtInformationForChannels(cfg,rx);
		if(failureFlag)
		{
			ResetReferenceRxTime(rx);
			pvtData->numOfPvtFails++;
			SwrxLog(NGENE_LOG_SEVERITY_ERROR,"PVT failure %d",pvtData->numOfPvtFails);
			if(pvtData->pvtLogFile!=NULL)
			{
				fprintf(pvtData->pvtLogFile,"\nPVT results discarded (failure %d) after a large error\n\n",pvtData->numOfPvtFails);
			}
			#ifdef PVT_ENABLE_HARD_RESTART
			/* hard restart if too many consecutive PVT computations fail */
			if(pvtData->numOfPvtFails>PVT_MAXIMUM_CONSECUTIVE_FAILURES)
			{
				ResetPvt(cfg,pvtData,pvtRes,TRUE_FLAG);
				HardRestartChannels(cfg,rx);
				pvtData->numOfPvtFails = 0;
				pvtData->hardRestartMade = TRUE_FLAG;
				SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Hard Receiver Restart after too many PVT failures");
				if(pvtData->pvtLogFile!=NULL)
				{
					fprintf(pvtData->pvtLogFile,"\nHard Receiver Restart after too many PVT failures\n\n");
				}
			}
			#endif
		}
	}
	/* Prepare the next execution */
	SetRxTimeForNextPvt(cfg,rx);
	return SUCCESS_RETURN;
}
