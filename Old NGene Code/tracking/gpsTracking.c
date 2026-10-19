/**
 * @file gpsTracking.c
 *
 * @author Mario Nicola
 *
 * @date 04/09/2014
 *
 * @version 1.0.0
 *
 * @brief Functions used to perform tracking for GPS and EGNOS.
 */

#include "ngeneConfiguration.h"
#include "ngeneDataStructure.h"
#include "channelDataStructure.h"
#include "trackingDataStructure.h"
#include "arrays.h"
#include "tracking.h"
#include "trackingDiscriminatorsLoops.h"
#include "ioFunctions.h"
#include "gpsMessageParser.h"
#include "egnosMessageParser.h"
#include "receiverTime.h"


void CheckBitSynchronization(TNGeneConfiguration *cfg, TNGeneData *rx, TChannel *ch, TTracking *trk)
{
    TTrkConfiguration *trkCfg = &(cfg->trkConfiguration);

    /* Work only with PLL */
    if(trk->fllLocked==FALSE_FLAG)
    	return;

    /* here the value of trk->dataTransitionFound is set
     * - if 1, then tracking passes from integration period equal to 1 code period to the user defined one
     * - if 0, tracking does not pass from integration period equal to 1 code period to the user defined one */

    if(trk->bitSynchronizationFound == FALSE_FLAG)
    {
    	/* A minimum level of the PLL lock quality must be available */
    	if((trk->trkStatusChecker.nbd/trk->trkStatusChecker.nbp)<PLL_CHECKER_LOW_THRESHOLD)
    	{
    		trk->bitSynchronizationIndex = -1;
    		return;
    	}

        /* if the passage from PLL 1 ms to user integration period is enabled,
         * and the passage has not been done yet,
         * verify: there was a data transition? */
        if(((trk->pI<0.0) && (trk->pIOld>0.0)) || ((trk->pI>0.0) && (trk->pIOld<0.0)))
        {
            if(trkCfg->changePeriodCtrlLength == 0)
            {
                /* no control on data transition. The first sign transition is the good one!!! */
                trk->bitSynchronizationFound = TRUE_FLAG;
            }
            else
            {
                if(trk->bitSynchronizationIndex == -1)
                {
                    /* this is the first data transition found */
                    trk->bitSynchronizationIndex = 0;
                    trk->lastValidDataTransition = trk->trkCounter;
                }
                else
                {
                    if(((trk->trkCounter - trk->lastValidDataTransition) % ch->bitDuration) == 0)
                    {
                        /* the data transition is good, because a multiple of bitLength passed from the previous one.
                         * trk->time is the actual number of code periods from the start of tracking,
                         * filter->lastDataTransition is trk->time when the previous sign transitions was found */
                        /* changePeriodCtrlIndex counts the number of valid sign transitions: then, here it's incremented */
                        trk->bitSynchronizationIndex++;
                    }
                    else
                    {
                        /* the data transition is no good, restart:
                         * changePeriodCtrlIndex is put to 0 */
                        trk->bitSynchronizationIndex = 0;
                    }
                    trk->lastValidDataTransition = trk->trkCounter;
                    /* if enough data transitions have been verified, pass to user integration period */
                    if(trk->bitSynchronizationIndex >= trkCfg->changePeriodCtrlLength)
                    {
                    	SwrxLog(NGENE_LOG_SEVERITY_NOTE,"Channel %2d %s tracking bit synchronization found at sample %llu with Doppler %f Hz!",ch->satType,ch->prnName,rx->buf.discardedSamples+ch->bufferOffset+ch->blocksize,ch->dopplerShift);
                    	trk->bitSynchronizationFound = TRUE_FLAG;
                    	trk->msCounter = 0;
                    }
                }
            }
        }
    }
}


void ManageUserIntegrationPeriod(TNGeneConfiguration *cfg,TNGeneData *rx,TChannel *ch,TTracking *trk)
{
	if(trk->integrationPeriodUserActive==TRUE_FLAG)
		return;
	if(trk->msCounter<ch->bitDuration)
		return;
	if(trk->bitSynchronizationFound==FALSE_FLAG)
		return;

	trk->integrationPeriod = trk->integrationPeriodUser;
	trk->integrationPeriodUserActive = TRUE_FLAG;
	CalculateLoopCoefficients(cfg,ch,trk);
	RestartCarrierLoop(ch,trk);
	RestartCodeLoop(ch,trk);

	SwrxLog(NGENE_LOG_SEVERITY_NOTE,"Channel %2d %s tracking passed to integration period %d at sample %llu!",ch->chIndex,ch->prnName,trk->integrationPeriod,rx->buf.discardedSamples+ch->bufferOffset+ch->blocksize);
}


TTrackingStatus PerformGpsTracking(TNGeneConfiguration *cfg,TNGeneData *rx,TChannel *ch,char *firstBunch, unsigned int firstBunchSize,char *secondBunch, unsigned int secondBunchSize)
{
	TTracking *trk = &(ch->tracking);

	/* carrier wipe-off */
	ch->carrierPhase = GenerateCarrier(cfg,&(rx->fifTable),ch->carrierFrequency,ch->carrierPhase,ch->blocksize,rx->sinArray,rx->cosArray);
	CarrierComplexWipeOff(firstBunch,firstBunchSize,secondBunch,secondBunchSize,rx->cosArray,rx->sinArray,ch->blocksize,rx->cosArray,rx->sinArray);
	ch->carrierFrequencyOld = ch->carrierFrequency;
	ch->dopplerShiftOld = ch->dopplerShift;
	/* code wipeoff */
	// generate the code
	// prompt
	GenerateCode(cfg,&(rx->prnTable),ch->satType,ch->satId,ch->codeRate,&(ch->codePhase),&(ch->codePhaseFrac),NGENE_MODULATION,ch->blocksize,trk->promptCode);
	if(cfg->lowFsElaboration==TRUE_FLAG)
	{
		// early
		GenerateCode(cfg,&(rx->prnTable),ch->satType,ch->satId,ch->codeRate,&(trk->codeEarlyPhase),&(trk->codeEarlyPhaseFrac),NGENE_MODULATION,ch->blocksize,trk->earlyCode);
		// late
		GenerateCode(cfg,&(rx->prnTable),ch->satType,ch->satId,ch->codeRate,&(trk->codeLatePhase),&(trk->codeLatePhaseFrac),NGENE_MODULATION,ch->blocksize,trk->lateCode);
	}

	// correlation
	PerformAllTrackingCorrelations(ch,trk,rx->cosArray,rx->sinArray);

	/* try to obtain bit synchronization (if not reached yet) */
	CheckBitSynchronization(cfg,rx,ch,trk);

	/* update ms counter, evaluate loops */
	trk->msCounter++;
	if(((trk->msCounter%trk->integrationPeriod)==0) || (trk->msCounter==ch->bitDuration))
	{
		DumpCorrelationsToFile(cfg,rx,ch,trk);
		if(ChooseCarrierLoop(cfg,rx,ch,trk)==CARRIER_FLL)
		{
			FllDiscriminator(trk);
		}
		else
		{
			PllDiscriminator(trk);
		}
		CarrierLoop(cfg,ch,trk);
		DllDiscriminator(trk);
		CodeLoop(ch,trk);
	}

	/* manage navigation message */
	if((trk->msCounter==ch->bitDuration) && (ch->satType==GPS_SAT))
	{
		ParseGpsMessage(cfg,rx,ch,trk->pIForNav);
	}
	if((trk->msCounter==ch->bitDuration) && (ch->satType==EGNOS_SAT))
	{
		PredictRxTime(cfg,rx);
		if(rx->pvtData.rxGpsTimeValid==TRUE_FLAG)
		{
			ParseEgnosMessage(cfg,rx,ch,trk->pIForNav,rx->pvtData.rxGpsTow);
		}
	}

	/* manage the integration period chosen by the user */
	ManageUserIntegrationPeriod(cfg,rx,ch,trk);

	/* check the status */
	CheckTrackingLock(cfg,rx,ch,trk);

	/* reset finished correlations */
	UpdateCorrelations(ch,trk);

	/* Final update to the counters */
	if(trk->msCounter>=ch->bitDuration)
	{
		trk->msCounter = 0;
	}
	trk->trkCounter++;

	return trk->trkStatus;
}

