/**
 * @file nonCoherentSumsManager.c
 *
 * @author Mario Nicola
 *
 * @date 17/12/2014
 *
 * @version 1.0.0
 *
 * @brief Functions that manage the current number of non coherent sums.
 */

#include "nonCoherentSumsManager.h"
#include "ngeneConfiguration.h"
#include "ngeneDataStructure.h"
#include "prnList.h"
#include "ngeneConstants.h"
#include "ioFunctions.h"

int InitNonCoherentSumsManager(TNGeneConfiguration *cfg, TNGeneData *rx)
{
    TNonCoherentSumsManager *nonCohManager = &(rx->acqCommon.nonCohManager);
    TManualChannelConfiguration *chCfg;
    int idx;

    nonCohManager->currentNumberOfNonCoherentSums = 1;
    nonCohManager->maximumNumberOfNonCoherentSums = 1;
    if (cfg->automaticConfigurationFlag == FALSE_FLAG) {
	nonCohManager->listLength = cfg->numberOfChannels;
	for (idx = 0; idx < cfg->numberOfChannels; idx++) {
	    chCfg = &(cfg->manuallyConfiguredChannels[idx]);
	    InsertElementIntoPrnList(chCfg->satType, chCfg->satId, &(nonCohManager->testedPrns));
	    if (chCfg->nonCoherentAcqIntTime > nonCohManager->maximumNumberOfNonCoherentSums)
		nonCohManager->maximumNumberOfNonCoherentSums = chCfg->nonCoherentAcqIntTime;
	}
    } else {
	nonCohManager->listLength = GetListLength(&(rx->channelScheduler.prnList));
	if (CopyPrnList(&(rx->channelScheduler.prnList), &(nonCohManager->testedPrns)) != SUCCESS_RETURN)
	    return FAILURE_RETURN;

	nonCohManager->maximumNumberOfNonCoherentSums = cfg->acqConfiguration.gpsNumOfNonCohSums;
	if (cfg->acqConfiguration.galileoNumOfNonCohSums > nonCohManager->maximumNumberOfNonCoherentSums)
	    nonCohManager->maximumNumberOfNonCoherentSums = cfg->acqConfiguration.galileoNumOfNonCohSums;
	if (cfg->acqConfiguration.egnosNumOfNonCohSums > nonCohManager->maximumNumberOfNonCoherentSums)
	    nonCohManager->maximumNumberOfNonCoherentSums = cfg->acqConfiguration.egnosNumOfNonCohSums;

	/* In warm start uses the maximum from the start
	 * (cannot manage the increment of the integration time, because the list of PRNs is not fixed */
	if (cfg->warmConfiguration.enableWarmStart == TRUE_FLAG) {
	    nonCohManager->currentNumberOfNonCoherentSums = nonCohManager->maximumNumberOfNonCoherentSums;
	}
    }
    return SUCCESS_RETURN;
}

void DeinitNonCoherentSumsManager(TNonCoherentSumsManager *nonCohManager)
{
    DestroyPrnList(&(nonCohManager->testedPrns));
}

void PerformNonCoherentSumsManager(TNGeneConfiguration *cfg, TNGeneData *rx, TSatConstellation constellation, int prn, TNonCoherentSumsManager *nonCohManager)
{
    int idx;
    TChannel *ch;
    int freeGps = TRUE_FLAG;
    int freeGal = TRUE_FLAG;
    int freeEgnos = TRUE_FLAG;

    if (nonCohManager->currentNumberOfNonCoherentSums >= nonCohManager->maximumNumberOfNonCoherentSums)
	return;

    if (GetNumberOfAvailableElementsInTheList(&(nonCohManager->testedPrns)) == 0) {
	/* time to pass to the next number of non coherent sums */
	nonCohManager->currentNumberOfNonCoherentSums++;
	if (nonCohManager->currentNumberOfNonCoherentSums > nonCohManager->maximumNumberOfNonCoherentSums) {
	    nonCohManager->currentNumberOfNonCoherentSums = nonCohManager->maximumNumberOfNonCoherentSums;
	}
	SwrxLog(NGENE_LOG_SEVERITY_NOTE, "NGene activated %d non coherent sums acquisition", nonCohManager->currentNumberOfNonCoherentSums);
	/* reset the tested PRNs list: if all channels for a constellation are in tracking,
	 * then this scheduler does not take into account that constellation */
	if (rx->channelScheduler.gpsChannelsInTracking >= cfg->numberOfGpsChannels)
	    freeGps = FALSE_FLAG;
	if (rx->channelScheduler.galileoChannelsInTracking >= cfg->numberOfGalileoChannels)
	    freeGal = FALSE_FLAG;
	if (rx->channelScheduler.egnosChannelsInTracking >= cfg->numberOfEgnosChannels)
	    freeEgnos = FALSE_FLAG;
	FreeAllElementsInTheList(&(nonCohManager->testedPrns), freeGps, freeGal, freeEgnos);
	/* tag as acquired all PRNs in tracking */
	for (idx = 0; idx < cfg->numberOfChannels; idx++) {
	    ch = &(rx->channelArray[idx]);
	    if (ch->chState == TRACKING_STATE) {
		FindAndOccupyInPrnList(ch->satType, ch->satId, &(nonCohManager->testedPrns));
	    }
	}
    }
    FindAndOccupyInPrnList(constellation, prn, &(nonCohManager->testedPrns));
}
