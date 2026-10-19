/**
 * @file confirmation.c
 *
 * @author Mario Nicola
 *
 * @date 03/09/2014
 *
 * @version 1.0.0
 *
 * @brief Functions performing M-on-N confirmation.
 */

#include <limits.h>
#include "confirmation.h"
#include "channelDataStructure.h"
#include "ngeneConfiguration.h"
#include "acquisitionDataStructure.h"
#include "ioFunctions.h"
#include "ngeneDataStructure.h"
#include "prnTable.h"
#include "carrierTable.h"
#include "arrays.h"
#include "acquisition.h"

#ifndef DISABLE_CONFIRMATION_THRESHOLD
void InitConfirmation(TNGeneConfiguration *cfg, TNGeneData *rx, TChannel *ch)
#else
void InitConfirmation(TNGeneConfiguration *cfg, TChannel *ch)
#endif
{
    TConfirmation *conf = &(ch->confirmation);

    if (ch->satType == GPS_SAT) {
	conf->successesRequired = cfg->acqConfiguration.gpsConfirmationM;
	conf->maxNumOfTries = cfg->acqConfiguration.gpsConfirmationN;
	conf->threshold = UINT_MAX;
    } else if (ch->satType == GALILEO_SAT) {
	conf->successesRequired = cfg->acqConfiguration.galileoConfirmationM;
	conf->maxNumOfTries = cfg->acqConfiguration.galileoConfirmationN;
	conf->threshold = UINT_MAX;
    } else {                    /* EGNOS_SAT */
	conf->successesRequired = cfg->acqConfiguration.egnosConfirmationM;
	conf->maxNumOfTries = cfg->acqConfiguration.egnosConfirmationN;
	conf->threshold = UINT_MAX;
    }
#ifndef DISABLE_CONFIRMATION_THRESHOLD
    StartConfirmation(cfg, rx, ch);
#else
    StartConfirmation(ch);
#endif
}

#ifndef DISABLE_CONFIRMATION_THRESHOLD

void StartConfirmation(TNGeneConfiguration *cfg, TNGeneData *rx, TChannel *ch)
#else
void StartConfirmation(TChannel *ch)
#endif
{
    TConfirmation *conf = &(ch->confirmation);
    TCoarseAcquisition *coarse = &(ch->coarse);
    TCoarseAcquisition *acqCoarse = &(ch->coarse);

    if (acqCoarse->acqBinLog != NULL) {
	fclose(acqCoarse->acqBinLog);
	acqCoarse->acqBinLog = NULL;
    }

    conf->msCounter = 0;
    conf->numOfNonCoherentSums = coarse->numOfNonCoherentSums;
    conf->nonCoherentSumsIndex = 0;
    conf->pI = 0;
    conf->pQ = 0;
    conf->successCounter = 0;
    conf->tryCounter = 0;
#ifndef DISABLE_CONFIRMATION_THRESHOLD
    UpdateConfirmationThresholds(cfg, rx, ch);
#else
    UpdateConfirmationThresholds(ch);
#endif
}

TAcquisitionStatus PerformConfirmation(TNGeneConfiguration *cfg, TNGeneData *rx, TChannel *ch, char *firstBunch, unsigned int firstBunchSize, char *secondBunch, unsigned int secondBunchSize)
{
    TConfirmation *conf = &(ch->confirmation);
    unsigned int p;

    /* generate and remove carrier */
    ch->carrierPhase = GenerateCarrier(cfg, &(rx->fifTable), ch->carrierFrequency, ch->carrierPhase, ch->blocksize, rx->sinArray, rx->cosArray);
    CarrierComplexWipeOff(firstBunch, firstBunchSize, secondBunch, secondBunchSize, rx->cosArray, rx->sinArray, ch->blocksize, rx->cosArray, rx->sinArray);
    /* code despreading */
    if (ch->satType == GALILEO_SAT) {
	/* if Galileo, generate also the subcarrier */
	GenerateSine(cfg, &(rx->fscTable), ch->codeRate, TWICE_M_PI * ch->codePhaseFrac, ch->blocksize, rx->subcarrierArray);
    }
    GenerateCode(cfg, &(rx->prnTable), ch->satType, ch->satId, ch->codeRate, &(ch->codePhase), &(ch->codePhaseFrac), NGENE_MODULATION, ch->blocksize, rx->codeArray);
    if (ch->satType == GALILEO_SAT) {
	/* if Galileo, remove also the subcarrier */
	ArrayXor(rx->subcarrierArray, rx->codeArray, rx->codeArray, ch->blocksize);
    }
    conf->pI += (ArrayMac(rx->cosArray, rx->codeArray, ch->blocksize) + (ch->blocksize / 2));
    conf->pQ += (ArrayMac(rx->sinArray, rx->codeArray, ch->blocksize) + (ch->blocksize / 2));
    /* analyze the result */
    conf->msCounter++;
    if (conf->msCounter == ch->codeDuration) {
	p = (unsigned int) (conf->pI * conf->pI) + (unsigned int) (conf->pQ * conf->pQ);
	conf->msCounter = 0;
	conf->pI = 0;
	conf->pQ = 0;
	if (conf->nonCoherentSumsIndex == 0) {
	    conf->p = 0;
	}
	conf->p += (p / conf->numOfNonCoherentSums);
	conf->nonCoherentSumsIndex++;
	if (conf->nonCoherentSumsIndex >= conf->numOfNonCoherentSums) {
	    conf->nonCoherentSumsIndex = 0;
	    /* analyze the result */
	    conf->tryCounter++;
	    if (conf->p > (conf->threshold / conf->numOfNonCoherentSums)) {
		SwrxLog(NGENE_LOG_SEVERITY_NOTE, "    %s: confirmation %u vs %u (success)", ch->prnName, p, conf->threshold / conf->numOfNonCoherentSums);
		conf->successCounter++;
		if (conf->successCounter >= conf->successesRequired) {
		    return ACQUISITION_SUCCESS;
		}
	    } else {
		SwrxLog(NGENE_LOG_SEVERITY_NOTE, "    %s: confirmation %u vs %u (failure)", ch->prnName, p, conf->threshold / conf->numOfNonCoherentSums);
		if ((conf->tryCounter >= conf->maxNumOfTries) || ((conf->maxNumOfTries - conf->tryCounter) < (conf->successesRequired - conf->successCounter))) {
		    return ACQUISITION_FAILURE;
		}
	    }
	}
    }
    return ACQUISITION_IN_EXECUTION;
}
