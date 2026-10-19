/**
 * @file tracking.c
 *
 * @author Mario Nicola
 *
 * @date 03/09/2014
 *
 * @version 1.0.0
 *
 * @brief Functions used to perform tracking.
 */

#include <stdlib.h>
#include <math.h>
#include "ngeneConfiguration.h"
#include "ngeneDataStructure.h"
#include "channelDataStructure.h"
#include "trackingDataStructure.h"
#include "ngeneConstants.h"
#include "timeConstants.h"
#include "mathFunctions.h"
#include "ioFunctions.h"
#include "closeManager.h"
#include "arrays.h"
#include "trackingDiscriminatorsLoops.h"
#include "tracking.h"
#include "cno.h"


int InitTracking(TNGeneConfiguration *cfg,TNGeneData *rx,TChannel *ch)
{
	TTracking *trk = &(ch->tracking);
	TTrkConfiguration *trkCfg = &(cfg->trkConfiguration);
	TGuiConfiguration *guiCfg = &(cfg->guiParameters);

	#ifdef CSC_DISCRIMINATOR_FUNCTION
	int veryEarlyDisp = 0;
	#endif
	int multipleCorrMaxDisp = 0;
	int promptDisp = 0; // [samples]
	int earlyDisp = 0;  // [samples]
	int sidePeakDisp=0; // [samples]
	int index;
	int errorsFound = 0;

	/* set some configuration dependent parameters */
	if(cfg->automaticConfigurationFlag==TRUE_FLAG)
	{
		if(ch->satType==GPS_SAT)
			trk->integrationPeriodUser = trkCfg->gpsIntegrationPeriod;
		else if(ch->satType==EGNOS_SAT)
			trk->integrationPeriodUser = trkCfg->egnosIntegrationPeriod;
		else
			trk->integrationPeriodUser = GALILEO_BIT_DURATION;
	}
	else
	{
		trk->integrationPeriodUser = (int)cfg->manuallyConfiguredChannels[ch->chIndex].coherentTrkIntTime;
	}
	/* init codes pointers */
	if(cfg->automaticConfigurationFlag==TRUE_FLAG)
	{
		if(ch->satType==GPS_SAT)
		{
			earlyDisp = NearestInteger((trkCfg->gpsSpacing*cfg->fs)/(2.0*GPS_CODE_RATE));
			#ifdef CSC_DISCRIMINATOR_FUNCTION
			veryEarlyDisp = NearestInteger((trkCfg->veryEarlyVeryLateGpsSpacing*cfg->fs)/(2.0*GPS_CODE_RATE));
			#endif
			trk->spacing = trkCfg->gpsSpacing/2.0;
		}
		else if(ch->satType==GALILEO_SAT)
		{
			earlyDisp = NearestInteger((trkCfg->galileoSpacing*cfg->fs)/(2.0*GPS_CODE_RATE));
			#ifdef CSC_DISCRIMINATOR_FUNCTION
			veryEarlyDisp = NearestInteger((trkCfg->veryEarlyVeryLateGalileoSpacing*cfg->fs)/(2.0*GPS_CODE_RATE));
			#endif
			trk->spacing = trkCfg->galileoSpacing/2.0;
		}
		else /* EGNOS_SAT */
		{
			earlyDisp = NearestInteger((trkCfg->egnosSpacing*cfg->fs)/(2.0*GPS_CODE_RATE));
			#ifdef CSC_DISCRIMINATOR_FUNCTION
			veryEarlyDisp = NearestInteger((trkCfg->veryEarlyVeryLateEgnosSpacing*cfg->fs)/(2.0*GPS_CODE_RATE));
			#endif
			trk->spacing = trkCfg->egnosSpacing/2.0;
		}
	}
	else
	{
		earlyDisp = NearestInteger((cfg->manuallyConfiguredChannels[ch->chIndex].spacing*cfg->fs)/(2.0*GPS_CODE_RATE));
		#ifdef CSC_DISCRIMINATOR_FUNCTION
		veryEarlyDisp = NearestInteger((guiCfg->manuallyConfiguredChannels[ch->chIndex].veryEarlyVeryLateSpacing*cfg->fs)/(2.0*GPS_CODE_RATE));
		#endif
		trk->spacing = cfg->manuallyConfiguredChannels[ch->chIndex].spacing/2.0;
	}
	if(guiCfg->multipleCorrelatorsFlag==TRUE_FLAG)
	{
		multipleCorrMaxDisp = (int)(guiCfg->numberOfMultipleCorrelators/2)*guiCfg->multipleCorrelatorsSpacing;
	}
	else if (trkCfg->multipleCorrelatorsFlag==TRUE_FLAG)
	{
		if(ch->satType==GPS_SAT)
		{
			multipleCorrMaxDisp = (int)(trkCfg->numberOfMultipleCorrelators/2)*trkCfg->multipleCorrelatorsGpsSpacing;
		}
		else if(ch->satType==GALILEO_SAT)
		{
			multipleCorrMaxDisp = (int)(trkCfg->numberOfMultipleCorrelators/2)*trkCfg->multipleCorrelatorsGalileoSpacing;
		}
		else if(ch->satType==EGNOS_SAT)
		{
			multipleCorrMaxDisp = (int)(trkCfg->numberOfMultipleCorrelators/2)*trkCfg->multipleCorrelatorsEgnosSpacing;
		}
	}
	#ifdef CSC_DISCRIMINATOR_FUNCTION
	if(multipleCorrMaxDisp>veryEarlyDisp && veryEarlyDisp>earlyDisp)
	{
		promptDisp = multipleCorrMaxDisp;
	}
	else if(veryEarlyDisp>earlyDisp)
	{
		promptDisp = veryEarlyDisp;
	}
	else
	{
		promptDisp = earlyDisp;
	}
	#else
	if(multipleCorrMaxDisp>earlyDisp && earlyDisp>sidePeakDisp)
	{
		promptDisp = multipleCorrMaxDisp;
	}
	else if(earlyDisp>sidePeakDisp)
	{
		promptDisp = earlyDisp;
	}
	else
	{
		promptDisp = sidePeakDisp;
	}
	#endif
	promptDisp = ((promptDisp/XMM_REGISTER_SIZE)+1)*XMM_REGISTER_SIZE;
	if(cfg->lowFsElaboration==FALSE_FLAG)
	{
		trk->promptCode = &(rx->codeArray[promptDisp]);
		trk->earlyCode = &(rx->codeArray[promptDisp - earlyDisp]);
		trk->lateCode = &(rx->codeArray[promptDisp + earlyDisp]);
	}
	else
	{
		trk->promptCode = &(rx->codeArray[promptDisp]);
		trk->earlyCode = rx->codeEarlyArray;
		trk->lateCode = rx->codeLateArray;
	}
	#ifdef CSC_DISCRIMINATOR_FUNCTION
	trk->veryEarlyCode = &(rx->codeArray[promptDisp - veryEarlyDisp]);
	trk->veryLateCode = &(rx->codeArray[promptDisp + veryEarlyDisp]);
	#endif
	if (trkCfg->multipleCorrelatorsFlag==TRUE_FLAG)
	{
		trk->numberOfMulti = trkCfg->numberOfMultipleCorrelators;
		trk->multiCodes = (char **)malloc(trkCfg->numberOfMultipleCorrelators*sizeof(char *));
		if(trk->multiCodes==NULL)
		{
			SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Malloc returned a NULL pointer in InitTracking");
			CloseNGene(cfg,EXIT_FAILURE);
		}
		trk->multiCodes[0] = &(rx->codeArray[promptDisp-multipleCorrMaxDisp]);
		if(ch->satType==GPS_SAT)
		{
			for(index=1;index<trkCfg->numberOfMultipleCorrelators;index++)
			{
				trk->multiCodes[index] = trk->multiCodes[index-1] + trkCfg->multipleCorrelatorsGpsSpacing;
			}
		}
		else if(ch->satType==GALILEO_SAT)
		{
			for(index=1;index<trkCfg->numberOfMultipleCorrelators;index++)
			{
				trk->multiCodes[index] = trk->multiCodes[index-1] + trkCfg->multipleCorrelatorsGalileoSpacing;
			}
		}
		else if(ch->satType==EGNOS_SAT)
		{
			for(index=1;index<trkCfg->numberOfMultipleCorrelators;index++)
			{
				trk->multiCodes[index] = trk->multiCodes[index-1] + trkCfg->multipleCorrelatorsEgnosSpacing;
			}
		}
		trk->multiI = (float *)malloc(trkCfg->numberOfMultipleCorrelators*sizeof(float));
		if(trk->multiI==NULL)
			errorsFound++;
		trk->multiQ = (float *)malloc(trkCfg->numberOfMultipleCorrelators*sizeof(float));
		if(trk->multiQ==NULL)
			errorsFound++;
		trk->multiOld = (float *)malloc(trkCfg->numberOfMultipleCorrelators*sizeof(float));
		if(trk->multiOld==NULL)
			errorsFound++;

	}
	else
	{
		trk->numberOfMulti=0;
		trk->multiI=NULL;
		trk->multiQ=NULL;
		trk->multiOld=NULL;
	}
	if(errorsFound>0)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Malloc returned a NULL pointer in InitTracking");
		CloseNGene(cfg,EXIT_FAILURE);
	}
	InitTrackingStatusChecker(cfg,ch,trk);
	return SUCCESS_RETURN;
}


void DeinitTracking(TTracking *trk)
{
	DeinitTrackingStatusChecker(trk);
	if(trk->numberOfMulti>0)
	{
		free(trk->multiCodes);
		free(trk->multiI);
		free(trk->multiQ);
		free(trk->multiOld);
	}
}

void StartTracking(TNGeneConfiguration *cfg,TChannel *ch,TTracking *trk)
{
	int index;

	trk->trkStatus = TRACKING_INITIAL_TRANSIENT;
	trk->msCounter = 0;
	trk->trkCounter = 0;
	trk->lockCounter = INT_MAX;
	if(ch->satType==GPS_SAT)
	{
		trk->integrationPeriod = GPS_CODE_DURATION;
		trk->bitSynchronizationFound = FALSE_FLAG;
	}
	else if(ch->satType==GALILEO_SAT)
	{
		trk->integrationPeriod = GALILEO_CODE_DURATION;
		trk->bitSynchronizationFound = TRUE_FLAG;	/* for Galileo, bit and code are aligned */
	}
	else /* EGNOS_SAT */
	{
		trk->integrationPeriod = EGNOS_CODE_DURATION;
		trk->bitSynchronizationFound = FALSE_FLAG;
	}
	trk->fllLocked = FALSE_FLAG;
	trk->integrationPeriodUserActive = FALSE_FLAG;
	/**/
	if(cfg->lowFsElaboration==FALSE_FLAG)
	{
		trk->codeEarlyPhase = 0.0;
		trk->codeEarlyPhaseFrac = 0.0;
		trk->codeLatePhase = 0.0;
		trk->codeLatePhaseFrac = 0.0;
	}
	else
	{
		AddCodePhases(ch->satType, ch->codePhase,-trk->spacing,&(trk->codeEarlyPhase),&(trk->codeEarlyPhaseFrac));
		AddCodePhases(ch->satType, ch->codePhase, trk->spacing,&(trk->codeLatePhase),&(trk->codeLatePhaseFrac));
	}
	/**/
	trk->eI = 0.0;
	trk->eQ = 0.0;
	trk->pI = 0.0;
	trk->pQ = 0.0;
	trk->lI = 0.0;
	trk->lQ = 0.0;
	#ifdef CSC_DISCRIMINATOR_FUNCTION
	trk->vEI = 0.0;
	trk->vEQ = 0.0;
	trk->vLI = 0.0;
	trk->vLQ = 0.0;
	#endif
	trk->pIOld = 0.0;
	trk->pQOld = 0.0;
	trk->pIForNav = 0.0;
	trk->pIForCno = 0.0;
	trk->pQForCno = 0.0;
	trk->eOld = 0.0;
	trk->pOld = 0.0;
	trk->lOld = 0.0;
	for(index=0;index<trk->numberOfMulti;index++)
	{
		trk->multiI[index] = 0.0;
		trk->multiQ[index] = 0.0;
		trk->multiOld[index] = 0.0;
	}
	InitAverageWindow(&(ch->codeRateMean),PLL_FALSE_LOCK_FILTER_LENGTH,ch->codeRate);
	InitTrackingLoops(cfg,ch,trk);
	trk->bitSynchronizationIndex = -1;
	trk->lastValidDataTransition = 0;
	StartTrackingStatusChecker(cfg,trk);
}

void PerformAllTrackingCorrelations(TChannel *ch, TTracking *trk, char *cosArray, char *sinArray)
{
	int index;

	trk->eI += ((float)ArrayMac(cosArray,trk->earlyCode,ch->blocksize)+((float)ch->blocksize/2))/(float)trk->integrationPeriod;
	trk->eQ += ((float)ArrayMac(sinArray,trk->earlyCode,ch->blocksize)+((float)ch->blocksize/2))/(float)trk->integrationPeriod;
	trk->pIForCno = ((float)ArrayMac(cosArray,trk->promptCode,ch->blocksize)+((float)ch->blocksize/2))/(float)trk->integrationPeriod;
	trk->pI += trk->pIForCno;
	trk->pIForNav += trk->pIForCno;
	trk->pQForCno = ((float)ArrayMac(sinArray,trk->promptCode,ch->blocksize)+((float)ch->blocksize/2))/(float)trk->integrationPeriod;
	trk->pQ += trk->pQForCno;
	trk->lI += ((float)ArrayMac(cosArray,trk->lateCode,ch->blocksize)+((float)ch->blocksize/2))/(float)trk->integrationPeriod;
	trk->lQ += ((float)ArrayMac(sinArray,trk->lateCode,ch->blocksize)+((float)ch->blocksize/2))/(float)trk->integrationPeriod;
	for(index=0;index<trk->numberOfMulti;index++)
	{
		trk->multiI[index] += ((float)ArrayMac(cosArray,trk->multiCodes[index],ch->blocksize)+((float)ch->blocksize/2))/(float)trk->integrationPeriod;
		trk->multiQ[index] += ((float)ArrayMac(sinArray,trk->multiCodes[index],ch->blocksize)+((float)ch->blocksize/2))/(float)trk->integrationPeriod;
	}
	#ifdef CSC_DISCRIMINATOR_FUNCTION
	trk->vEI += ((float)ArrayMac(cosArray,trk->veryEarlyCode,ch->blocksize)+((float)ch->blocksize/2))/(float)trk->integrationPeriod;
	trk->vEQ += ((float)ArrayMac(sinArray,trk->veryEarlyCode,ch->blocksize)+((float)ch->blocksize/2))/(float)trk->integrationPeriod;
	trk->vLI += ((float)ArrayMac(cosArray,trk->veryLateCode,ch->blocksize)+((float)ch->blocksize/2))/(float)trk->integrationPeriod;
	trk->vLQ += ((float)ArrayMac(sinArray,trk->veryLateCode,ch->blocksize)+((float)ch->blocksize/2))/(float)trk->integrationPeriod;
	#endif
}


void DumpCorrelationsToFile(TNGeneConfiguration *cfg,TNGeneData *rx,TChannel *ch,TTracking *trk)
{
	double *tmpD = (double *)rx->trkLogArray;
	float *tmpF = (float *)rx->trkLogArray;

	if(rx->trkCorrelatorsFile!=NULL)
	{
		tmpD[0] = (double)ch->chIndex;
		tmpD[1] = (double)ch->satType;
		tmpD[2] = (double)ch->satId;
		tmpD[3] = (double)(rx->buf.discardedSamples+ch->bufferOffset+ch->blocksize)/cfg->fs;
		tmpD[4] = (double)trk->pI;
		tmpD[5] = (double)trk->pQ;
		tmpD[6] = (double)trk->eI;
		tmpD[7] = (double)trk->eQ;
		tmpD[8] = (double)trk->lI;
		tmpD[9] = (double)trk->lQ;
		tmpD[10] = (double)trk->trkStatusChecker.cnoEstimator.estimatedCNo;
		tmpD[11] = (double)ch->dopplerShift;
		tmpD[12] = (double)ch->codeRate-(double)GPS_CODE_RATE;
		tmpD[13] = (double)(trk->trkStatusChecker.nbd/trk->trkStatusChecker.nbp);
		tmpD[14] = (double)(trk->carrierLoop.initialCarrierFrequency);
		tmpD[15] = (double)(trk->carrierLoop.carrierNcoOld);
		fwrite(tmpD,sizeof(double),TRACKING_LOG_SET_SIZE,rx->trkCorrelatorsFile);
	}
	if(rx->trkMultiCorrelatorsFile!=NULL)
	{
		tmpF[0] = (float)ch->chIndex;
		tmpF[1] = (float)ch->satType;
		tmpF[2] = (float)ch->satId;
		tmpF[3] = (float)(rx->buf.discardedSamples+ch->bufferOffset+ch->blocksize)/cfg->fs;
		ArrayCopy((char *)trk->multiI,(char *)&(tmpF[4]),sizeof(float)*trk->numberOfMulti);
		ArrayCopy((char *)trk->multiQ,(char *)&(tmpF[4+trk->numberOfMulti]),sizeof(float)*trk->numberOfMulti);
		fwrite(tmpF,sizeof(float),4+(2*trk->numberOfMulti),rx->trkMultiCorrelatorsFile);
	}
}

void UpdateCorrelations(TChannel *ch,TTracking *trk)
{
	int index;

	if(((trk->msCounter%trk->integrationPeriod)==0) || (trk->msCounter==ch->bitDuration))
	{
		trk->pIOld = trk->pI;
		trk->pQOld = trk->pQ;
		trk->eOld = (trk->eI*trk->eI)+(trk->eQ*trk->eQ);
		trk->pOld = (trk->pI*trk->pI)+(trk->pQ*trk->pQ);
		trk->lOld = (trk->lI*trk->lI)+(trk->lQ*trk->lQ);
		trk->eI = 0;
		trk->eQ = 0;
		trk->pI = 0;
		trk->pQ = 0;
		trk->lI = 0;
		trk->lQ = 0;
		#ifdef CSC_DISCRIMINATOR_FUNCTION
		trk->vEI = 0;
		trk->vEQ = 0;
		trk->vLI = 0;
		trk->vLQ = 0;
		#endif
		for(index=0;index<trk->numberOfMulti;index++)
		{
			trk->multiOld[index] = (trk->multiI[index]*trk->multiI[index])+(trk->multiQ[index]*trk->multiQ[index]);
			trk->multiI[index] = 0;
			trk->multiQ[index] = 0;
		}
	}
	if(trk->msCounter==ch->bitDuration)
	{
		trk->pIForNav = 0;
	}
}

/**
 *  @def PLL_FALSE_LOCK_CHECK_PERIOD
 *  @brief Time (ms) between two consecutive executions of the PLL False Lock Checker
 */
#define PLL_FALSE_LOCK_CHECK_PERIOD  100

/**
 *  @def PLL_FALSE_LOCK_CONFIRMATION_PERIOD
 *  @brief Time (ms) after the PLL false lock correction to repeat the check and eventually confirm the correction
 */
#define PLL_FALSE_LOCK_CONFIRMATION_PERIOD  1000

/**
 *  @def PLL_FALSE_LOCK_NUM_OF_SUCCESSES
 *  @brief Number of successes required by the M-on-N PLL False Lock Checker
 */
#define PLL_FALSE_LOCK_NUM_OF_SUCCESSES  4

/**
 * @def PLL_FALSE_LOCK_NUM_OF_TESTS
 *  @brief Number of tests required by the M-on-N PLL False Lock Checker
 */
#define PLL_FALSE_LOCK_NUM_OF_TESTS  5

/**
 * @def GPS_FALSE_LOCKS_TO_BE_TESTED
 * @brief How many PLL false locks will be tested for GPS signal
 *        (it should be an even number)
 */
#define GPS_FALSE_LOCKS_TO_BE_TESTED 2

/**
 * @def GALILEO_FALSE_LOCKS_TO_BE_TESTED
 * @brief How many PLL false locks will be tested for Galileo signal
 *        (it should be an even number)
 */
#define GALILEO_FALSE_LOCKS_TO_BE_TESTED 8


void InitTrackingStatusChecker(TNGeneConfiguration *cfg,TChannel *ch,TTracking *trk)
{
	TTrackingStatusChecker *checker = &(trk->trkStatusChecker);

    /* pll false lock checker */
	if(ch->satType==GALILEO_SAT)
	{
		checker->pflcArraySize = GALILEO_FALSE_LOCKS_TO_BE_TESTED+1;
	}
	else
	{
		checker->pflcArraySize = GPS_FALSE_LOCKS_TO_BE_TESTED+1;
	}
	checker->pflcLocks = (double *)malloc(checker->pflcArraySize*sizeof(double));
	checker->pflcDistances = (double *)malloc(checker->pflcArraySize*sizeof(double));
	checker->pflcTests = (int *)malloc(checker->pflcArraySize*sizeof(int));
	if((checker->pflcLocks==NULL) || (checker->pflcDistances==NULL) || (checker->pflcTests==NULL))
    {
    	SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Malloc returned a NULL pointer in InitTrackingStatusChecker");
    	CloseNGene(cfg,EXIT_FAILURE);
    }
}


void StartTrackingStatusChecker(TNGeneConfiguration *cfg,TTracking *trk)
{
	TTrackingStatusChecker *checker = &(trk->trkStatusChecker);
	TCnoTable *cnoEstimator = &(checker->cnoEstimator);

	/* cno */
	InitAverageWindow(&(cnoEstimator->x2), 10000, 0.0);
	InitAverageWindow(&(cnoEstimator->muX), 10000, 0.0);
    cnoEstimator->samplingFrequency = cfg->fs;
    cnoEstimator->estimatedCNo = 0.0;
    /* pll false lock checker */
    ArrayClean((char *)checker->pflcLocks,checker->pflcArraySize*sizeof(double));
    ArrayClean((char *)checker->pflcDistances,checker->pflcArraySize*sizeof(double));
    ArrayClean((char *)checker->pflcTests,checker->pflcArraySize*sizeof(int));
    checker->pflcTotalTests = 0;
    /* PLL lock checker */
    checker->pllStartCounter = 0;
    InitAverageWindow(&(checker->pIMean),PLL_CHECKER_SUMS,0.0);
    InitAverageWindow(&(checker->pQMean),PLL_CHECKER_SUMS,0.0);
    checker->nbd = 0.0;
    checker->nbp = 1.0;
    /* Other variables */
    checker->pflcLastCheckCounter = 0;
    checker->pllLastCheckCounter = 0;
    checker->trkCounterForPllError = 0;
    checker->cnoLastCheckCounter = 0;
    checker->trkCounterForCnoError = 0;
}


void DeinitTrackingStatusChecker(TTracking *trk)
{
	TTrackingStatusChecker *checker = &(trk->trkStatusChecker);
	free(checker->pflcLocks);
	free(checker->pflcDistances);
	free(checker->pflcTests);
}


/**
 * @brief CorrectPllFalseLock corrects the value of the Doppler frequency in the case of a PLL false lock.
 *
 * @details Finds the Doppler shift estimated by the DLL and compares it with the one
 *          estimated by the PLL: if they are not coherent, then corrects the Doppler frequency
 *          in the PLL loop (false lock detected).
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 * @param ch is the pointer to the actual channel
 * @param trk is the pointer to the tracking data structure for the Galileo structure.
 *
 * @return Status of the PLL False Lock Checker
 */
TTrackingPflcStatus CorrectPllFalseLock(TNGeneConfiguration *cfg,TNGeneData *rx,TChannel *ch,TTracking *trk)
{
	TTrackingStatusChecker *checker = &(trk->trkStatusChecker);
	double nominalCodeRate;
	int numOfFd;
	int numOfFd2;
	double fdDll;
	double deltaFd;
	int idx;
	int idxMinDistance;
	double newFd;

	/* Restart if already been used */
	if(checker->pflcTotalTests==PLL_FALSE_LOCK_NUM_OF_TESTS)
	{
		checker->pflcTotalTests = 0;
		ArrayClean((char *)checker->pflcTests,checker->pflcArraySize*sizeof(int));
	}
	/* Initializations depending on the constellation */
	numOfFd = checker->pflcArraySize;
	if(ch->satType==GALILEO_SAT)
	{
		nominalCodeRate = (double)GALILEO_CODE_RATE;
	}
	else
	{
		nominalCodeRate = (double)GPS_CODE_RATE;
	}
	numOfFd2 = numOfFd/2;
	/* FD from the code rate */
	fdDll = (double)cfg->fePars->dopplerCorrection * ((GetAverageWindowValue(&(ch->codeRateMean))/nominalCodeRate)-1.0);
	fdDll *= (double)L1_FREQUENCY;
	/* False locks */
	deltaFd = (0.5*SECONDS_TO_MILLISECONDS)/trk->integrationPeriod;
	checker->pflcLocks[0] = ch->dopplerShift - (deltaFd*numOfFd2);
	checker->pflcDistances[0] = fabs(fdDll - checker->pflcLocks[0]);
	for(idx=1;idx<numOfFd;idx++)
	{
		checker->pflcLocks[idx] = checker->pflcLocks[idx-1] + deltaFd;
		checker->pflcDistances[idx] = fabs(fdDll - checker->pflcLocks[idx]);
	}
	/* Find the most probable false lock */
	idxMinDistance = 0;
	for(idx=1;idx<numOfFd;idx++)
	{
		if(checker->pflcDistances[idx]<checker->pflcDistances[idxMinDistance])
		{
			idxMinDistance = idx;
		}
	}
	checker->pflcTotalTests++;
	checker->pflcTests[idxMinDistance]++;
	/* Analyze the results */
	if(checker->pflcTotalTests==PLL_FALSE_LOCK_NUM_OF_TESTS)
	{
		/* Find the most chosen fd value */
		idxMinDistance = 0;
		for(idx=1;idx<numOfFd;idx++)
		{
			if(checker->pflcTests[idx]>checker->pflcTests[idxMinDistance])
			{
				idxMinDistance = idx;
			}
		}
		/* Verify M-on-N */
		if((idxMinDistance!=numOfFd2) && (checker->pflcTests[idxMinDistance]>PLL_FALSE_LOCK_NUM_OF_SUCCESSES))
		{
			newFd = checker->pflcLocks[idxMinDistance];
			SwrxLog(NGENE_LOG_SEVERITY_INFO,"Channel %2d %s PLL lock corrected from %f Hz to %f Hz at sample %llu",ch->chIndex,ch->prnName,ch->dopplerShift,newFd,rx->buf.discardedSamples+ch->bufferOffset+ch->blocksize);
			ch->dopplerShift = ch->dopplerShiftOld = newFd;
			ch->carrierFrequency = ch->carrierFrequencyOld = cfg->fif + newFd;
			RestartCarrierLoop(ch,trk);
		}
		return PFLC_FINISHED;
	}
		
	return PFLC_RUNNING;
}

void CheckAndCorrectSidePeakLock(TNGeneConfiguration *cfg,TChannel *ch,TTracking *trk)
{

	double codeDelayShiftTotalInChips=0.5;
	double codeDelayShiftTotalInSamples=0;
	double cycles;
	double f;

	 trk->sideLeft=(double)(trk->sideLeftI*trk->sideLeftI)+(double)(trk->sideLeftQ*trk->sideLeftQ);
	 trk->sideRight=(double)(trk->sideRightI*trk->sideRightI)+(double)(trk->sideRightQ*trk->sideRightQ);
	 trk->p=(double)(trk->pI*trk->pI)+(double)(trk->pQ*trk->pQ);
	 if((trk->p>trk->sideLeft) && (trk->p>trk->sideRight))
	 {
		 SwrxLog(NGENE_LOG_SEVERITY_INFO,"Channel %2d %s tracking locked to the main peak!",ch->chIndex,ch->prnName);
	 }
	 else if(((trk->p>trk->sideRight) && (trk->p<trk->sideLeft)) || ((trk->p<trk->sideRight) && (trk->p>trk->sideLeft)))
	 {
		if((trk->p>trk->sideRight) && (trk->p<trk->sideLeft))
		{
			SwrxLog(NGENE_LOG_SEVERITY_INFO,"Channel %2d %s tracking locked to the left side peak: corrected!",ch->chIndex,ch->prnName);
			codeDelayShiftTotalInSamples=-NearestInteger(((float)codeDelayShiftTotalInChips*cfg->fs)/ch->codeRate);
		}
		else
		{
			 SwrxLog(NGENE_LOG_SEVERITY_INFO,"Channel %2d %s tracking locked to the right side peak: corrected!",ch->chIndex,ch->prnName);
			 codeDelayShiftTotalInSamples=NearestInteger(((float)codeDelayShiftTotalInChips*cfg->fs)/ch->codeRate);
		}
		 /* update carrier phase */
		f = ch->carrierFrequency/cfg->fs;
		ch->carrierPhase = ch->carrierPhase + (TWICE_M_PI*(double)codeDelayShiftTotalInSamples*f);
		cycles = floor(ch->carrierPhase*INV_2PI);
		ch->carrierPhase = ch->carrierPhase - (cycles*TWICE_M_PI);
		ch->blocksize+=codeDelayShiftTotalInSamples;
	 }

}


/**
 * @brief PerformPllChecks evaluates the PLL lock metric (nbd/nbp ratio)
 *        and the C/No.
 *
 * @param cfg points to the receiver configuration
 * @param ch is the pointer to the actual channel
 * @param trk is the pointer to the tracking data structure for the Galileo structure.
 */
void PerformPllChecks(TNGeneConfiguration *cfg,TChannel *ch,TTracking *trk)
{
	TTrackingStatusChecker *checker = &(trk->trkStatusChecker);
	TCnoTable *cnoEstimator = &(checker->cnoEstimator);
	float pI;
	float pQ;
	double pIMean;
	double pQMean;

	if(ch->satType==GALILEO_SAT)
	{
		if(((trk->msCounter%trk->integrationPeriod)==0) && (trk->fllLocked==TRUE_FLAG))
		{
			pI = trk->pI;
			pQ = trk->pQ;
			/* PLL check */
			pIMean = PerformAverageWindow(&(checker->pIMean),pI*pI);
			pQMean = PerformAverageWindow(&(checker->pQMean),pQ*pQ);
			checker->nbd = pIMean - pQMean;
			checker->nbp = pIMean + pQMean;
			/* nbd/nbp ratio is evaluated, verify nbp==0 in order to avoid division by zero */
			if(checker->nbp==0.0)
				checker->nbp = 1.0;
			/* CNo */
			ComputeEquivalentBandwidth(cnoEstimator,1.0/ch->codeRate,trk->integrationPeriod*(int)ch->blocksize,(double)3.0);
			cnoEstimator->estimatedCNo = MeasureCno(cnoEstimator,pI,pQ,cfg)+GALILEO_CN0_CORRECTION_FACTOR;
		}
	}
	else
	{
		if(trk->fllLocked==TRUE_FLAG)
		{
			pI = trk->pIForCno;
			pQ = trk->pQForCno;
			/* PLL check */
			pIMean = PerformAverageWindow(&(checker->pIMean),pI*pI);
			pQMean = PerformAverageWindow(&(checker->pQMean),pQ*pQ);
			checker->nbd = pIMean - pQMean;
			checker->nbp = pIMean + pQMean;
			/* nbd/nbp ratio is evaluated, verify nbp==0 in order to avoid division by zero */
			if(checker->nbp==0.0)
				checker->nbp = 1.0;
			/* CNo */
			ComputeEquivalentBandwidth(cnoEstimator,1.0/ch->codeRate,ch->blocksize,(double)1.0);
			cnoEstimator->estimatedCNo = MeasureCno(cnoEstimator,pI,pQ,cfg)+GPS_CN0_CORRECTION_FACTOR;
		}
	}
}


void CheckTrackingLock(TNGeneConfiguration *cfg,TNGeneData *rx,TChannel *ch,TTracking *trk)
{
	TTrackingStatusChecker *checker = &(trk->trkStatusChecker);
	TCnoTable *cnoEstimator = &(checker->cnoEstimator);

	/* Updates some metrics used for tracking lock check  */
	PerformPllChecks(cfg,ch,trk);

	/* following tests are performed only at the end of a code period  */
	if((trk->msCounter%ch->codeDuration)>0)
		return;

	/* manage the status */
	switch(trk->trkStatus)
	{
		case TRACKING_INITIAL_TRANSIENT:
			if((trk->trkCounter>=FLL_TIMEOUT) && (trk->fllLocked==FALSE_FLAG))
			{
				SwrxLog(NGENE_LOG_SEVERITY_INFO,"Channel %2d %s tracking lost at sample %llu, trk counter %llu (FLL timeout)",ch->chIndex,ch->prnName,rx->buf.discardedSamples+ch->bufferOffset+ch->blocksize,trk->trkCounter);
				trk->trkStatus = TRACKING_LOST;
			}
			else if((trk->trkCounter>=PLL_TIMEOUT) && (trk->bitSynchronizationFound==FALSE_FLAG))
			{
				SwrxLog(NGENE_LOG_SEVERITY_INFO,"Channel %2d %s tracking lost at sample %llu, trk counter %llu (no bit synchronization)",ch->chIndex,ch->prnName,rx->buf.discardedSamples+ch->bufferOffset+ch->blocksize,trk->trkCounter);
				trk->trkStatus = TRACKING_LOST;
			}
			else if((trk->fllLocked==TRUE_FLAG) && (trk->trkCounter<PLL_TIMEOUT) && ((trk->trkCounter+1-checker->pllStartCounter)>PLL_CHECKER_SUMS))
			{
				if((checker->nbd/checker->nbp)>PLL_CHECKER_HIGH_THRESHOLD)
				{
					checker->pllLastCheckCounter = trk->trkCounter;
					checker->cnoLastCheckCounter = trk->trkCounter;
					SwrxLog(NGENE_LOG_SEVERITY_INFO,"Channel %2d %s tracking PLL locked at sample %llu, trk counter %llu,  with Doppler %f Hz!",ch->chIndex,ch->prnName,rx->buf.discardedSamples+ch->bufferOffset+ch->blocksize,trk->trkCounter,ch->dopplerShift);
					trk->trkStatus = TRACKING_PLL_FALSE_LOCK_CHECK;
					if(trk->lockCounter==INT_MAX)
					{
						trk->lockCounter=trk->trkCounter;
					}
				}
			}
			else if(trk->trkCounter>=PLL_TIMEOUT)
			{
				SwrxLog(NGENE_LOG_SEVERITY_INFO,"Channel %2d %s tracking lost at sample %llu, trk counter %llu (PLL timeout)",ch->chIndex,ch->prnName,rx->buf.discardedSamples+ch->bufferOffset+ch->blocksize,trk->trkCounter);
				trk->trkStatus = TRACKING_LOST;
			}
			break;

		case TRACKING_PLL_FALSE_LOCK_CHECK:
			if(cnoEstimator->estimatedCNo<cfg->trkConfiguration.cutoffCno)
			{
				SwrxLog(NGENE_LOG_SEVERITY_NOTE,"Channel %2d %s found a problem with CNo value at sample %llu, trk counter %llu",ch->chIndex,ch->prnName,rx->buf.discardedSamples+ch->bufferOffset+ch->blocksize,trk->trkCounter);
				checker->trkCounterForCnoError = trk->trkCounter;
				trk->trkStatus = TRACKING_CN0_CHECK_FAILED;
				checker->pflcLastCheckCounter = 0;
				checker->pflcTotalTests=PLL_FALSE_LOCK_NUM_OF_TESTS;
			}
			else if((checker->nbd/checker->nbp)<PLL_CHECKER_HIGH_THRESHOLD)
			{
				SwrxLog(NGENE_LOG_SEVERITY_NOTE,"Channel %2d %s found a problem with PLL lock at sample %llu, trk counter %llu",ch->chIndex,ch->prnName,rx->buf.discardedSamples+ch->bufferOffset+ch->blocksize,trk->trkCounter);
				checker->trkCounterForPllError = trk->trkCounter;
				trk->trkStatus = TRACKING_PLL_CHECK_FAILED;
				checker->pflcLastCheckCounter = 0;
				checker->pflcTotalTests=PLL_FALSE_LOCK_NUM_OF_TESTS;
			}
			else if(trk->trkCounter > (checker->pflcLastCheckCounter+PLL_FALSE_LOCK_CHECK_PERIOD))
			{
				if(CorrectPllFalseLock(cfg,rx,ch,trk)==PFLC_FINISHED)
				{
					trk->trkStatus = TRACKING_PLL_FALSE_LOCK_CONFIRMATION;
					checker->pflcLastCheckCounter = trk->trkCounter + PLL_FALSE_LOCK_CONFIRMATION_PERIOD;
				}
				else
				{
					checker->pflcLastCheckCounter = trk->trkCounter;
				}
			}
			break;

		case TRACKING_PLL_FALSE_LOCK_CONFIRMATION:
			if(cnoEstimator->estimatedCNo<cfg->trkConfiguration.cutoffCno)
			{
				SwrxLog(NGENE_LOG_SEVERITY_NOTE,"Channel %2d %s found a problem with CNo value at sample %llu, trk counter %llu",ch->chIndex,ch->prnName,rx->buf.discardedSamples+ch->bufferOffset+ch->blocksize,trk->trkCounter);
				checker->trkCounterForCnoError = trk->trkCounter;
				trk->trkStatus = TRACKING_CN0_CHECK_FAILED;
				checker->pflcLastCheckCounter = 0;
				checker->pflcTotalTests=PLL_FALSE_LOCK_NUM_OF_TESTS;
			}
			else if((checker->nbd/checker->nbp)<PLL_CHECKER_HIGH_THRESHOLD)
			{
				SwrxLog(NGENE_LOG_SEVERITY_NOTE,"Channel %2d %s found a problem with PLL lock at sample %llu, trk counter %llu",ch->chIndex,ch->prnName,rx->buf.discardedSamples+ch->bufferOffset+ch->blocksize,trk->trkCounter);
				checker->trkCounterForPllError = trk->trkCounter;
				trk->trkStatus = TRACKING_PLL_CHECK_FAILED;
				checker->pflcLastCheckCounter = 0;
				checker->pflcTotalTests=PLL_FALSE_LOCK_NUM_OF_TESTS;
			}
			else if(trk->trkCounter > (checker->pflcLastCheckCounter+PLL_FALSE_LOCK_CHECK_PERIOD))
			{
				if(CorrectPllFalseLock(cfg,rx,ch,trk)==PFLC_FINISHED)
				{
					trk->trkStatus = TRACKING_LOCKED;
					checker->pflcLastCheckCounter = 0;
				}
				else
				{
					checker->pflcLastCheckCounter = trk->trkCounter;
				}
			}
			break;

		case TRACKING_LOCKED:
			if((trk->trkCounter-checker->pllLastCheckCounter)>PLL_CHECKER_SUMS)
			{
				checker->pllLastCheckCounter = trk->trkCounter;
				if((checker->nbd/checker->nbp)<PLL_CHECKER_LOW_THRESHOLD)
				{
					SwrxLog(NGENE_LOG_SEVERITY_NOTE,"Channel %2d %s found a problem with PLL lock at sample %llu, trk counter %llu",ch->chIndex,ch->prnName,rx->buf.discardedSamples+ch->bufferOffset+ch->blocksize,trk->trkCounter);
					checker->trkCounterForPllError = trk->trkCounter;
					trk->trkStatus = TRACKING_PLL_CHECK_FAILED;
				}
			}
			if((trk->trkCounter-checker->cnoLastCheckCounter)>CNO_CHECK_PERIOD)
			{
				checker->cnoLastCheckCounter = trk->trkCounter;
				if(cnoEstimator->estimatedCNo<cfg->trkConfiguration.cutoffCno)
				{
					SwrxLog(NGENE_LOG_SEVERITY_NOTE,"Channel %2d %s found a problem with CNo value at sample %llu, trk counter %llu",ch->chIndex,ch->prnName,rx->buf.discardedSamples+ch->bufferOffset+ch->blocksize,trk->trkCounter);
					checker->trkCounterForCnoError = trk->trkCounter;
					trk->trkStatus = TRACKING_CN0_CHECK_FAILED;
				}
			}
			break;

		case TRACKING_PLL_CHECK_FAILED:
			if((checker->nbd/checker->nbp)>PLL_CHECKER_HIGH_THRESHOLD)
			{
				SwrxLog(NGENE_LOG_SEVERITY_NOTE,"Channel %2d %s again locked at sample %llu, trk counter %llu, after a PLL lock problem",ch->chIndex,ch->prnName,rx->buf.discardedSamples+ch->bufferOffset+ch->blocksize,trk->trkCounter);
				trk->trkStatus = TRACKING_PLL_FALSE_LOCK_CHECK;
			}
			else if((trk->trkCounter-checker->trkCounterForPllError)>RELOCK_TIMEOUT)
			{
				SwrxLog(NGENE_LOG_SEVERITY_INFO,"Channel %2d %s tracking lost at sample %llu, trk counter %llu (PLL lock problem)",ch->chIndex,ch->prnName,rx->buf.discardedSamples+ch->bufferOffset+ch->blocksize,trk->trkCounter);
				trk->trkStatus = TRACKING_LOST;
			}
			break;

		case TRACKING_CN0_CHECK_FAILED:
			if(cnoEstimator->estimatedCNo>(float)(cfg->trkConfiguration.cutoffCno+CNO_HIGH_THRESHOLD_DIFF))
			{
				SwrxLog(NGENE_LOG_SEVERITY_NOTE,"Channel %2d %s again locked at sample %llu, trk counter %llu, after a low CNo",ch->chIndex,ch->prnName,rx->buf.discardedSamples+ch->bufferOffset+ch->blocksize,trk->trkCounter);
				trk->trkStatus = TRACKING_PLL_FALSE_LOCK_CHECK;
			}
			else if ((trk->trkCounter-checker->trkCounterForCnoError)>RELOCK_TIMEOUT)
			{
				SwrxLog(NGENE_LOG_SEVERITY_INFO,"Channel %2d %s tracking lost at sample %llu, trk counter %llu (low CNo)",ch->chIndex,ch->prnName,rx->buf.discardedSamples+ch->bufferOffset+ch->blocksize,trk->trkCounter);
				trk->trkStatus = TRACKING_LOST;
			}
			break;

		default:
			break;
	}
}
