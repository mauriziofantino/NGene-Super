/**
 * @file guiInterfaceFunctions.c
 *
 * @author Mario Nicola
 *
 * @date 11/08/2014
 *
 * @version 1.0.0
 *
 * @brief Functions that manage the communication between Java GUI and C receiver.
 */
#include <stdlib.h>
#include <string.h>
#include "guiInterfaceFunctions.h"
#include "ngeneConstants.h"
#include "ioFunctions.h"
#include "prnList.h"
#include "ngeneDataStructure.h"
#include "acquisition.h"
#include "dop.h"
#include "pvtConfiguration.h"


/**
 * @def FROM_KHZ_RANGE_TO_HZ_MAXIMUM_VALUE
 * @brief Translates a range (maxFd - minFd) in KHz, into a maximum value (maxFd==minFd) in Hz)
 */
#define FROM_KHZ_RANGE_TO_HZ_MAXIMUM_VALUE 500.0


int LoadGuiConfigurationIntoReceiver(void *dataFromGui,TNGeneConfiguration *rxCfg)
{
	void *ptrForRawCfg;
	TPvtConfiguration *pvtCfg = &(rxCfg->pvtConfiguration);
	TTrkConfiguration *trkCfg = &(rxCfg->trkConfiguration);
	TAcqConfiguration *acqCfg = &(rxCfg->acqConfiguration);
	TWarmStartConfiguration *warmCfg = &(rxCfg->warmConfiguration);
	TGuiConfiguration *guiCfg = &(rxCfg->guiParameters);
	int almanacLen;
	FILE *outputAlmanacFile;
	int index;
	int rowLen;
	char tmpRow[MAX_ROW_LENGTH];
	TSatConstellation constellation;
	int prn;
	TManualChannelConfiguration *chCfg;

	ptrForRawCfg = dataFromGui;

	/* General parameters */
	rxCfg->usedFrontEnd = (int)ReadAFloat(&ptrForRawCfg); /* Read FE index: formerly sampling frequency */
	ReadAFloat(&ptrForRawCfg); /* Unused intermediate frequency */
	pvtCfg->pvtRate = ReadAFloat(&ptrForRawCfg);
	pvtCfg->maxGdop = ReadAFloat(&ptrForRawCfg);
	pvtCfg->pvtMethod = ReadAnInt(&ptrForRawCfg);
	trkCfg->cutoffCno = ReadAFloat(&ptrForRawCfg);
	pvtCfg->elevationMaskFlag = ReadAnInt(&ptrForRawCfg);
	pvtCfg->elevationMask = ReadAFloat(&ptrForRawCfg) * DEGREE_TO_RADIAN;
	pvtCfg->egnosFlag = ReadAnInt(&ptrForRawCfg);
	acqCfg->gpsConfirmationM = ReadAnInt(&ptrForRawCfg);
	acqCfg->gpsConfirmationN = ReadAnInt(&ptrForRawCfg);
	acqCfg->galileoConfirmationM = ReadAnInt(&ptrForRawCfg);
	acqCfg->galileoConfirmationN = ReadAnInt(&ptrForRawCfg);
	acqCfg->egnosConfirmationM = ReadAnInt(&ptrForRawCfg);
	acqCfg->egnosConfirmationN = ReadAnInt(&ptrForRawCfg);
	warmCfg->enableWarmStart = ReadAnInt(&ptrForRawCfg);
	if(warmCfg->enableWarmStart==TRUE_FLAG)
	{
		almanacLen = ReadAnInt(&ptrForRawCfg);
		if(almanacLen>0)
		{
			outputAlmanacFile = fopen(warmCfg->almanacPathGps,"wb");
			fwrite(ptrForRawCfg,sizeof(char),almanacLen,outputAlmanacFile);
			fclose(outputAlmanacFile);
			ptrForRawCfg = (void *) (((char *)ptrForRawCfg)+almanacLen);
		}
		// Galileo almanac configured through configuration file is not overwritten
		warmCfg->warmLatitude = ReadAFloat(&ptrForRawCfg);
		warmCfg->warmLongitude = ReadAFloat(&ptrForRawCfg);
		warmCfg->warmYear = ReadAnInt(&ptrForRawCfg);
		warmCfg->warmMonth = ReadAnInt(&ptrForRawCfg);
		warmCfg->warmDay = ReadAnInt(&ptrForRawCfg);
		warmCfg->warmHour = ReadAnInt(&ptrForRawCfg);
		warmCfg->warmMinute = ReadAnInt(&ptrForRawCfg);
		warmCfg->warmSecond = ReadAnInt(&ptrForRawCfg);
	}
	acqCfg->gpsFalseDetectionProb = ReadAFloat(&ptrForRawCfg);
	acqCfg->galileoFalseDetectionProb = ReadAFloat(&ptrForRawCfg);
	acqCfg->egnosFalseDetectionProb = ReadAFloat(&ptrForRawCfg);
	acqCfg->gpsMaximumDoppler = ReadAFloat(&ptrForRawCfg)*FROM_KHZ_RANGE_TO_HZ_MAXIMUM_VALUE;
	acqCfg->galileoMaximumDoppler = ReadAFloat(&ptrForRawCfg)*FROM_KHZ_RANGE_TO_HZ_MAXIMUM_VALUE;
	acqCfg->egnosMaximumDoppler = ReadAFloat(&ptrForRawCfg)*FROM_KHZ_RANGE_TO_HZ_MAXIMUM_VALUE;
	acqCfg->gpsDopplerStep = ReadAnInt(&ptrForRawCfg);
	acqCfg->galileoDopplerStep = ReadAnInt(&ptrForRawCfg);
	acqCfg->egnosDopplerStep = ReadAnInt(&ptrForRawCfg);
	/* Channel parameters */
	guiCfg->automaticConfigurationFlag = ReadAnInt(&ptrForRawCfg);
	if(guiCfg->automaticConfigurationFlag==TRUE_FLAG)
	{
		trkCfg->gpsDllBw = ReadAFloat(&ptrForRawCfg);
		trkCfg->gpsPllBw = ReadAFloat(&ptrForRawCfg);
		trkCfg->gpsIntegrationPeriod = (int)ReadAFloat(&ptrForRawCfg);
		guiCfg->gpsCoherentAcqIntTime = ReadAFloat(&ptrForRawCfg);
		acqCfg->gpsNumOfNonCohSums = (int)ReadAFloat(&ptrForRawCfg);
		trkCfg->gpsSpacing = ReadAFloat(&ptrForRawCfg);
		trkCfg->galileoDllBw = ReadAFloat(&ptrForRawCfg);
		trkCfg->galileoPllBw = ReadAFloat(&ptrForRawCfg);
		guiCfg->galileoCoherentTrkIntTime = ReadAFloat(&ptrForRawCfg);
		guiCfg->galileoCoherentAcqIntTime = ReadAFloat(&ptrForRawCfg);
		acqCfg->galileoNumOfNonCohSums = (int)ReadAFloat(&ptrForRawCfg);
		trkCfg->galileoSpacing = ReadAFloat(&ptrForRawCfg);
		trkCfg->egnosDllBw = ReadAFloat(&ptrForRawCfg);
		trkCfg->egnosPllBw = ReadAFloat(&ptrForRawCfg);
		trkCfg->egnosIntegrationPeriod = ReadAFloat(&ptrForRawCfg);
		guiCfg->egnosCoherentAcqIntTime = ReadAFloat(&ptrForRawCfg);
		acqCfg->egnosNumOfNonCohSums = (int)ReadAFloat(&ptrForRawCfg);
		trkCfg->egnosSpacing = ReadAFloat(&ptrForRawCfg);
		rxCfg->numberOfGpsChannels = ReadAnInt(&ptrForRawCfg);
		rxCfg->numberOfGalileoChannels = ReadAnInt(&ptrForRawCfg);
		rxCfg->numberOfEgnosChannels = ReadAnInt(&ptrForRawCfg);
		rxCfg->numberOfChannels = rxCfg->numberOfGpsChannels+rxCfg->numberOfGalileoChannels+rxCfg->numberOfEgnosChannels;
		guiCfg->numberOfExcludedPrns = ReadAnInt(&ptrForRawCfg);
		guiCfg->excludedPrns.head = NULL;
		for(index=0;index<guiCfg->numberOfExcludedPrns;index++)
		{
			rowLen = ReadAnInt(&ptrForRawCfg);
			strncpy(tmpRow, (char *)ptrForRawCfg, rowLen);
			ptrForRawCfg = (void *)(((char *)ptrForRawCfg)+rowLen);
			tmpRow[rowLen]='\0';
			FromStringToPrn(tmpRow, &constellation,&prn);
			InsertElementIntoPrnList(constellation,prn,&(guiCfg->excludedPrns));
		}
		guiCfg->manuallyConfiguredChannels = NULL;
	}
	else /* if(guiCfg->automaticConfigurationFlag==FALSE_FLAG) */
	{
		guiCfg->gpsCoherentAcqIntTime = 0;
		acqCfg->gpsNumOfNonCohSums = 0;
		guiCfg->galileoCoherentTrkIntTime = 0;
		guiCfg->galileoCoherentAcqIntTime = 0;
		acqCfg->galileoNumOfNonCohSums = 0;
		guiCfg->egnosCoherentAcqIntTime = 0;
		acqCfg->egnosNumOfNonCohSums = 0;
		guiCfg->numberOfExcludedPrns = 0;
		guiCfg->excludedPrns.head = NULL;
		rxCfg->numberOfChannels = ReadAnInt(&ptrForRawCfg);
		rxCfg->numberOfGpsChannels = 0;
		rxCfg->numberOfEgnosChannels = 0;
		rxCfg->numberOfGalileoChannels = 0;
		guiCfg->manuallyConfiguredChannels = (TManualChannelConfiguration *)malloc(rxCfg->numberOfChannels*sizeof(TManualChannelConfiguration));
		for(index=0;index<rxCfg->numberOfChannels;index++)
		{
			chCfg = &(guiCfg->manuallyConfiguredChannels[index]);
			rowLen = ReadAnInt(&ptrForRawCfg);
			strncpy(tmpRow, (char *)ptrForRawCfg, rowLen);
			ptrForRawCfg = (void *)(((char *)ptrForRawCfg)+rowLen);
			tmpRow[rowLen]='\0';
			FromStringToPrn(tmpRow, &chCfg->satType,&chCfg->satId);
			if(chCfg->satType==GPS_SAT)
			{
				rxCfg->numberOfGpsChannels++;
			}
			else if(chCfg->satType==EGNOS_SAT)
			{
				rxCfg->numberOfEgnosChannels++;
			}
			else /* if(chCfg->satType==GALILEO_SAT) */
			{
				rxCfg->numberOfGalileoChannels++;
			}
			chCfg->dllBw = ReadAFloat(&ptrForRawCfg);
			chCfg->pllBw = ReadAFloat(&ptrForRawCfg);
			chCfg->coherentTrkIntTime = ReadAFloat(&ptrForRawCfg);
			chCfg->coherentAcqIntTime = ReadAFloat(&ptrForRawCfg);
			chCfg->nonCoherentAcqIntTime = ReadAFloat(&ptrForRawCfg);
			chCfg->spacing = ReadAFloat(&ptrForRawCfg);
		}
	}
	/* IF ANALYSIS and LOG PARAMETERS */
	guiCfg->spectrumSamples = ReadAnInt(&ptrForRawCfg);
	guiCfg->spectrumWindows = ReadAnInt(&ptrForRawCfg);
	guiCfg->spectrumOverlap = ReadAnInt(&ptrForRawCfg);
	guiCfg->spectrumZeroPadding = ReadAnInt(&ptrForRawCfg);
	guiCfg->spectrumWeightFunction = ReadAnInt(&ptrForRawCfg);
	guiCfg->histSamples = ReadAnInt(&ptrForRawCfg);
	guiCfg->histBits = ReadAnInt(&ptrForRawCfg);
	guiCfg->outputPathLength = ReadAnInt(&ptrForRawCfg);
	if(guiCfg->outputPathLength > 0)
	{
		guiCfg->outputPath = (char *) malloc((guiCfg->outputPathLength + 1) * sizeof(char));
	    strncpy(guiCfg->outputPath, (char *) ptrForRawCfg, guiCfg->outputPathLength);
	    guiCfg->outputPath[guiCfg->outputPathLength] = '\0';
	    ptrForRawCfg = (void *) (((char *) ptrForRawCfg) + guiCfg->outputPathLength);
	}
	else
	{
		guiCfg->outputPath = NULL;
	}
	guiCfg->ifGenerationFlag = ReadAnInt(&ptrForRawCfg);
	rxCfg->logConfiguration.enableTrkLog = ReadAnInt(&ptrForRawCfg);
	/* Input sample file */
	rowLen = ReadAnInt(&ptrForRawCfg);
	if(rowLen > 0)
	{
		if(rxCfg->sampleFilename!=NULL)
		{
			free(rxCfg->sampleFilename);
		}
	    rxCfg->sampleFilename = (char *)malloc((rowLen + 1)*sizeof(char));
	    strncpy(rxCfg->sampleFilename,(char *)ptrForRawCfg,rowLen);
	    rxCfg->sampleFilename[rowLen] = '\0';
	    ptrForRawCfg = (void *)(((char *)ptrForRawCfg)+rowLen);
	    rxCfg->inputMethod = INPUT_FILE;
	}
	else /* USB front-end is used */
	{
		if(rxCfg->sampleFilename!=NULL)
		{
			free(rxCfg->sampleFilename);
		}
		rxCfg->sampleFilename=NULL;
		rxCfg->inputMethod=USB_FRONTEND;
	}
	/* MULTIPLE CORRELATION PARAMETERS */
	guiCfg->multipleCorrelatorsFlag = ReadAnInt(&ptrForRawCfg);
	rxCfg->trkConfiguration.multipleCorrelatorsFlag=guiCfg->multipleCorrelatorsFlag;
	if (guiCfg->multipleCorrelatorsFlag==TRUE_FLAG)
	{
		guiCfg->numberOfMultipleCorrelators = (int)ReadAFloat(&ptrForRawCfg);
		guiCfg->multipleCorrelatorsSpacing = (int)ReadAFloat(&ptrForRawCfg);
		rxCfg->trkConfiguration.numberOfMultipleCorrelators=guiCfg->numberOfMultipleCorrelators;
		rxCfg->trkConfiguration.multipleCorrelatorsGpsSpacing=guiCfg->multipleCorrelatorsSpacing;
		rxCfg->trkConfiguration.multipleCorrelatorsGalileoSpacing=guiCfg->multipleCorrelatorsSpacing;
		rxCfg->trkConfiguration.multipleCorrelatorsEgnosSpacing=guiCfg->multipleCorrelatorsSpacing;;
	}

	rxCfg->automaticConfigurationFlag=guiCfg->automaticConfigurationFlag;
	rxCfg->manuallyConfiguredChannels=guiCfg->manuallyConfiguredChannels;

	return SUCCESS_RETURN;
}


int UpdateDataPacket(int *bufferSize, void **buffer,TNGeneConfiguration *rxCfg,TNGeneData *rx)
{
    void *pBuff;
    char emptyString[MAX_ROW_LENGTH];
    int index;
    int nChannels = rxCfg->numberOfChannels;
    int dataSize;
    int numberOfMultiCorrelators = 0;
    int multiCorrelatorsEnabled = rxCfg->guiParameters.multipleCorrelatorsFlag;
    int multiCorrelatorsIndex;
    TChannel *ch;
    TPvtData *pvtData = &(rx->pvtData);
    TPvtResults *pvtRes = &(rx->pvtResults);
    TDataToGuiChannel chOutputs;

    memset(emptyString,' ',MAX_ROW_LENGTH);

    dataSize = DATA_TO_GUI_HEADER_SIZE + (DATA_TO_GUI_CHANNEL_SIZE*nChannels);
    if(multiCorrelatorsEnabled==TRUE_FLAG)
    {
    	numberOfMultiCorrelators = rxCfg->guiParameters.numberOfMultipleCorrelators;
		dataSize += (nChannels * (numberOfMultiCorrelators*sizeof(double)));
	}

    if((*buffer)==NULL)
    {
    	*buffer = malloc(dataSize);
    	*bufferSize = dataSize;
    }
    else if(*bufferSize<dataSize)
    {
    	free(*buffer);
    	*buffer = malloc(dataSize);
    	*bufferSize = dataSize;
    }

    if((*buffer)==NULL)
    	return 0;

    pBuff = *buffer;

    /* HEADER SECTION
     * directly write in the buffer for the GUI */
    WriteAnInt(&pBuff,MAX_MSG_LENGTH);
    if(strcmp(rx->msgToGui, rx->msgToGuiOld))
    {
    	// Message updated
        WriteAString(&pBuff,rx->msgToGui,MAX_MSG_LENGTH);
        strncpy(rx->msgToGuiOld,rx->msgToGui,MAX_ROW_LENGTH);
    }
    else
    {
        WriteAString(&pBuff,emptyString,MAX_MSG_LENGTH);
    }
    if((pvtRes->pvtValid==TRUE_FLAG) && (pvtRes->pvtPlotted==FALSE_FLAG))
    {
    	WriteAnInt(&pBuff,TRUE_FLAG);
    	pvtRes->pvtPlotted = TRUE_FLAG;
    }
    else
    {
    	WriteAnInt(&pBuff,FALSE_FLAG);
    }
    WriteAnInt(&pBuff,pvtData->rxGpsTow);
    WriteAnInt(&pBuff,pvtData->rxGalTow);
    WriteAnInt(&pBuff,pvtData->rxUtcTow);
    WriteAnInt(&pBuff,pvtData->rxGpsWn);
    WriteAnInt(&pBuff,pvtData->rxUtcTow);
    WriteAFloat(&pBuff,pvtRes->geodPosition[1]);
    WriteAFloat(&pBuff,pvtRes->geodPosition[0]);
    WriteAFloat(&pBuff,pvtRes->geodPosition[2]);
    WriteAFloat(&pBuff,pvtRes->enuVelocity[1]);
    WriteAFloat(&pBuff,pvtRes->enuVelocity[0]);
    WriteAFloat(&pBuff,pvtRes->enuVelocity[2]);
    WriteAFloat(&pBuff,pvtRes->ecefPosition[0]);
    WriteAFloat(&pBuff,pvtRes->ecefPosition[1]);
    WriteAFloat(&pBuff,pvtRes->ecefPosition[2]);
    WriteAFloat(&pBuff,pvtRes->enuPosition[0]);
    WriteAFloat(&pBuff,pvtRes->enuPosition[1]);
    WriteAFloat(&pBuff,pvtRes->enuPosition[2]);
    WriteAFloat(&pBuff,pvtRes->dop[GDOP_INDEX_IN_DOP_ARRAY]);
    WriteAFloat(&pBuff,pvtRes->dop[HDOP_INDEX_IN_DOP_ARRAY]);
    WriteAFloat(&pBuff,pvtRes->dop[VDOP_INDEX_IN_DOP_ARRAY]);
    WriteAFloat(&pBuff,pvtRes->dop[PDOP_INDEX_IN_DOP_ARRAY]);
    WriteAnInt(&pBuff,rx->gpsLastDecodedSubframe);
    WriteAnInt(&pBuff,rx->gpsLastDecodedPage);
    WriteAnInt(&pBuff,rx->galLastDecodedSubframe);
    WriteAnInt(&pBuff,rx->galLastDecodedPage);
    WriteAnInt(&pBuff,rxCfg->pvtConfiguration.egnosFlag);
    WriteALongLongInt(&pBuff,(long long int)rx->buf.discardedSamples);     /* TODO portare discardedSamples ad unsigned long long anche in GUI */
    WriteAnInt(&pBuff,nChannels);

    /* CHANNELS SECTION */
    for(index=0;index<nChannels;index++)
    {
    	ch = &(rx->channelArray[index]);
    	/* default values */
    	chOutputs.prnLength = PRN_NAME_LENGTH;
    	memcpy(chOutputs.prn,emptyString,PRN_NAME_LENGTH);
    	sprintf(chOutputs.prn,"%s",ch->prnName);
    	chOutputs.channelIndex = index;
    	chOutputs.statusLength = STATUS_LENGTH;
    	memcpy(chOutputs.status,emptyString,STATUS_LENGTH);
    	chOutputs.doppler = 0.0;
    	chOutputs.cn0 = 0.0;
    	chOutputs.lockStatus = 0;
    	chOutputs.pseudorangePrecision = 0.0;
    	chOutputs.accumDopplerPrecision = 0.0;
    	chOutputs.satElevation = 0.0;
    	chOutputs.satAzimuth = 0.0;
    	chOutputs.pseudorange = 0.0;
    	chOutputs.earlyCorr = 0.0;
    	chOutputs.promptCorr = 0.0;
    	chOutputs.lateCorr = 0.0;
    	chOutputs.tracked = FALSE_FLAG;
    	chOutputs.usedForPVT = 0;
    	chOutputs.healthySat = TRUE_FLAG;
    	chOutputs.satEcefX = 0.0;
    	chOutputs.satEcefY = 0.0;
    	chOutputs.satEcefZ = 0.0;
    	/* overwrite values w.r.t. the current channel status */
    	switch(ch->chState)
    	{
    		case POWERON_STATE:
    		case UNUSED_STATE:
    			memcpy(chOutputs.status,"idle         ",STATUS_LENGTH);
    			break;
    		case COARSE_ACQUISITION_STATE:
    			memcpy(chOutputs.status,"acquisition  ",STATUS_LENGTH);
    			chOutputs.doppler = (float)ch->dopplerShift;
    			break;
    		case CODE_REFINEMENT_STATE:
    			chOutputs.doppler = (float)ch->dopplerShift;
    			memcpy(chOutputs.status,"refinement   ",STATUS_LENGTH);
    			break;
    		case DOPPLER_REFINEMENT_STATE:
    			chOutputs.doppler = (float)ch->dopplerShift;
    			memcpy(chOutputs.status,"refinement   ",STATUS_LENGTH);
    			break;
    		case CONFIRMATION_STATE:
    			chOutputs.doppler = (float)ch->dopplerShift;
    			memcpy(chOutputs.status,"confirmation ",STATUS_LENGTH);
    			break;
    		case TRACKING_STATE:
    			chOutputs.doppler = (float)ch->dopplerShift;
    			chOutputs.cn0 = (float)ch->tracking.trkStatusChecker.cnoEstimator.estimatedCNo;
    			chOutputs.tracked = TRUE_FLAG;
    			chOutputs.earlyCorr = ch->tracking.eOld;
    			chOutputs.promptCorr = ch->tracking.pOld;
    			chOutputs.lateCorr = ch->tracking.lOld;
    			if(ch->tracking.trkStatus==TRACKING_LOCKED)
    			{
    				chOutputs.lockStatus = TRUE_FLAG;
    			}
    			else
    			{
    				chOutputs.lockStatus = FALSE_FLAG;
    			}
    			if(ch->satType==GPS_SAT)
    			{
    				if(ch->gpsParser->navMsg->eph.health==SIGNAL_OK)
    				{
    					chOutputs.healthySat=TRUE_FLAG;
    				}
    				else
    				{
    					chOutputs.healthySat=FALSE_FLAG;
    				}
    			}
    			else if(ch->satType==GALILEO_SAT)
    			{
    				if((ch->galParser->navMsg->eph.health==SIGNAL_OK) && (ch->galParser->navMsg->eph.galileoDataValidity==NAVIGATION_DATA_VALID))
    				{
    					chOutputs.healthySat=TRUE_FLAG;
    				}
    				else
    				{
    					chOutputs.healthySat=FALSE_FLAG;
    				}
    			}
    			chOutputs.satEcefX = ch->pvtInfo.satEcefX;
    			chOutputs.satEcefY = ch->pvtInfo.satEcefY;
    			chOutputs.satEcefZ = ch->pvtInfo.satEcefZ;
    			chOutputs.satElevation = ch->pvtInfo.satElevation * RADIAN_TO_DEGREE;
    			chOutputs.satAzimuth = ch->pvtInfo.satAzimuth * RADIAN_TO_DEGREE;
    			chOutputs.pseudorange = ch->pvtInfo.pseudorange;
    			if((ch->pvtInfo.usedForPVT==TRUE_FLAG) && (rx->osnmaAuth.authenticationFlags[index]==VALIDATED))
    			{
    				chOutputs.usedForPVT = TRUE_FLAG;
    				memcpy(chOutputs.status,"nm auth     ",STATUS_LENGTH);
    			}
    			else if(ch->pvtInfo.usedForPVT==TRUE_FLAG)
    			{
    				chOutputs.usedForPVT = TRUE_FLAG;
    				memcpy(chOutputs.status,"navigation  ",STATUS_LENGTH);
    			}
    			else if(rx->osnmaAuth.authenticationFlags[index]==NOT_VALID)
    			{
    				/* not used in PVT and not validated by OSNMA */
    				chOutputs.usedForPVT = FALSE_FLAG;
    				memcpy(chOutputs.status,"nm not auth ",STATUS_LENGTH);
    			}
    			else if(chOutputs.healthySat==FALSE_FLAG)
    			{
    				/* not used in PVT and not healthy */
    				chOutputs.usedForPVT = FALSE_FLAG;
    				memcpy(chOutputs.status,"unhealthy   ",STATUS_LENGTH);
    			}
    			else if((ch->satType==GALILEO_SAT) && (ch->galParser->dummyMsg==TRUE_FLAG))
    			{
    				/* not used in PVT and with a dummy navigation message */
    				chOutputs.usedForPVT = FALSE_FLAG;
    				memcpy(chOutputs.status,"dummy navmsg",STATUS_LENGTH);
    			}
    			else
    			{
    				/* not used in PVT but healthy and without a dummy navigation message */
    				chOutputs.usedForPVT = FALSE_FLAG;
    				memcpy(chOutputs.status,"tracking    ",STATUS_LENGTH);
    			}
    			break;
    	}
    	/* write in the buffer for the GUI */
    	WriteAnInt(&pBuff,chOutputs.prnLength);
    	WriteAString(&pBuff,chOutputs.prn,PRN_NAME_LENGTH);
    	WriteAnInt(&pBuff,chOutputs.channelIndex);
    	WriteAnInt(&pBuff,chOutputs.statusLength);
    	WriteAString(&pBuff,chOutputs.status,STATUS_LENGTH);
    	WriteAFloat(&pBuff,chOutputs.doppler);
    	WriteAFloat(&pBuff,chOutputs.cn0);
    	WriteAnInt(&pBuff,chOutputs.lockStatus);
    	WriteAFloat(&pBuff,chOutputs.pseudorangePrecision);
    	WriteAFloat(&pBuff,chOutputs.accumDopplerPrecision);
    	WriteAFloat(&pBuff,chOutputs.satElevation);
    	WriteAFloat(&pBuff,chOutputs.satAzimuth);
    	WriteAFloat(&pBuff,chOutputs.pseudorange);
    	WriteAFloat(&pBuff,chOutputs.earlyCorr);
    	WriteAFloat(&pBuff,chOutputs.promptCorr);
    	WriteAFloat(&pBuff,chOutputs.lateCorr);
    	WriteAnInt(&pBuff,chOutputs.tracked);
    	WriteAnInt(&pBuff,chOutputs.usedForPVT);
    	WriteAnInt(&pBuff,chOutputs.healthySat);
    	WriteAFloat(&pBuff,chOutputs.satEcefX);
    	WriteAFloat(&pBuff,chOutputs.satEcefY);
    	WriteAFloat(&pBuff,chOutputs.satEcefZ);
    	/* if multiple correlation is enabled from the GUI, copy the correlation values into the output buffer */
    	if (multiCorrelatorsEnabled == TRUE_FLAG)
    	{
    		for(multiCorrelatorsIndex=0;multiCorrelatorsIndex<numberOfMultiCorrelators;multiCorrelatorsIndex++)
    		{
    			WriteADouble(&pBuff,(double)ch->tracking.multiOld[multiCorrelatorsIndex]);
    		}
    	}
    }

    return dataSize;
}
