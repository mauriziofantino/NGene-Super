/**
 * @file  galileoTracking.c
 *
 * @author Mario Nicola
 *
 * @date 08/09/2014
 *
 * @version 1.0.0
 *
 * @brief Functions used to perform tracking for Galileo.
 */

#include "ngeneConfiguration.h"
#include "ngeneDataStructure.h"
#include "channelDataStructure.h"
#include "trackingDataStructure.h"
#include "arrays.h"
#include "tracking.h"
#include "trackingDiscriminatorsLoops.h"
#include "galileoMessageParser.h"

TTrackingStatus PerformGalileoTracking(TNGeneConfiguration *cfg,TNGeneData *rx,TChannel *ch,char *firstBunch, unsigned int firstBunchSize,char *secondBunch, unsigned int secondBunchSize)
{
	TTracking *trk = &(ch->tracking);

	/* carrier wipe-off */
	ch->carrierPhase = GenerateCarrier(cfg,&(rx->fifTable),ch->carrierFrequency,ch->carrierPhase,ch->blocksize,rx->sinArray,rx->cosArray);
	CarrierComplexWipeOff(firstBunch,firstBunchSize,secondBunch,secondBunchSize,rx->cosArray,rx->sinArray,ch->blocksize,rx->cosArray,rx->sinArray);
	ch->carrierFrequencyOld = ch->carrierFrequency;
	ch->dopplerShiftOld = ch->dopplerShift;
	/* code wipeoff */
	// generate the subcarrier
	// prompt
	GenerateSine(cfg,&(rx->fscTable),ch->codeRate,TWICE_M_PI*ch->codePhaseFrac,ch->blocksize,rx->subcarrierArray);
	if(cfg->lowFsElaboration==TRUE_FLAG)
	{
		// early
		GenerateSine(cfg,&(rx->fscTable),ch->codeRate,TWICE_M_PI*trk->codeEarlyPhaseFrac,ch->blocksize,rx->subcarrierArrayEarly);
		// late
		GenerateSine(cfg,&(rx->fscTable),ch->codeRate,TWICE_M_PI*trk->codeLatePhaseFrac,ch->blocksize,rx->subcarrierArrayLate);
	}
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
	// add subcarrier to the code
	ArrayXor(rx->subcarrierArray,trk->promptCode,trk->promptCode,ch->blocksize);
	if(cfg->lowFsElaboration==TRUE_FLAG)
	{
		ArrayXor(rx->subcarrierArrayEarly,trk->earlyCode,trk->earlyCode,ch->blocksize);
		ArrayXor(rx->subcarrierArrayLate,trk->lateCode,trk->lateCode,ch->blocksize);
	}
	// correlations
	PerformAllTrackingCorrelations(ch,trk,rx->cosArray,rx->sinArray);

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
	if(trk->msCounter==ch->bitDuration)
	{
		ParseGalMessage(cfg,rx,ch,trk->pIForNav);
	}

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


