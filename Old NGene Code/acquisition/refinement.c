/**
 * @file refinement.c
 *
 * @author Mario Nicola
 *
 * @date 01/09/2014
 *
 * @version 1.0.0
 *
 * @brief Functions performing code delay refinement and Doppler shift refinement.
 */
#include "refinement.h"
#include "channelDataStructure.h"
#include "acquisitionDataStructure.h"
#include "spreadingCodes.h"
#include "ngeneConfiguration.h"
#include "ngeneDataStructure.h"
#include "carrierTable.h"
#include "prnTable.h"
#include "arrays.h"
#include "spreadingCodes.h"

void InitRefinement(TNGeneConfiguration *cfg, TRefinement *ref, TChannel *ch)
{
    ref->indexBin = 0;
    ref->msCounter = 0;
    ref->nonCoherentSumsIndex = 0;
    ref->pI = 0;
    ref->pQ = 0;
    ref->maximum = 0;
    ref->indexBinMax = 0;
    ref->discardedSamples = 0;
    if (ch->satType == GPS_SAT) {
	if (cfg->automaticConfigurationFlag == TRUE_FLAG) {
	    ref->numOfNonCoherentSums = cfg->acqConfiguration.gpsNumOfNonCohSums;
	} else {
	    ref->numOfNonCoherentSums = (int) (cfg->manuallyConfiguredChannels[ch->chIndex].nonCoherentAcqIntTime);
	}
    } else if (ch->satType == GALILEO_SAT) {
	if (cfg->automaticConfigurationFlag == TRUE_FLAG) {
	    ref->numOfNonCoherentSums = cfg->acqConfiguration.galileoNumOfNonCohSums;
	} else {
	    ref->numOfNonCoherentSums = (int) (cfg->manuallyConfiguredChannels[ch->chIndex].nonCoherentAcqIntTime);
	}
    } else {                    /* EGNOS_SAT */
	if (cfg->automaticConfigurationFlag == TRUE_FLAG) {
	    ref->numOfNonCoherentSums = cfg->acqConfiguration.egnosNumOfNonCohSums;
	} else {
	    ref->numOfNonCoherentSums = (int) (cfg->manuallyConfiguredChannels[ch->chIndex].nonCoherentAcqIntTime);
	}
    }
}

void StartDopplerRefinement(TNGeneConfiguration *cfg, TNGeneData *rx, TChannel *ch)
{
    TAcqCommon *acqCommon = &(rx->acqCommon);
    TCoarseAcquisition *acqCoarse = &(ch->coarse);
    char filename[2 * MAX_ROW_LENGTH];

    InitRefinement(cfg, &(ch->refinement), ch);
    /* the samples used for reference for the code rate correction (at the end of the refinement)
     * is the first sample used in the refinement
     * (StartDopplerRefinement is called at the end of the coarse acquisition, so the last blocksize samples are not discarded yet) */
    ch->refinement.discardedSamples = rx->buf.discardedSamples + ch->bufferOffset + ch->blocksize;

    if (cfg->logConfiguration.enableAcqLog == TRUE_FLAG) {
	if (acqCoarse->acqBinLog != NULL) {
	    fclose(acqCoarse->acqBinLog);
	    acqCoarse->acqBinLog = NULL;
	}
	sprintf(filename, "%s/dopplerRefinement_%s.bin", cfg->dumpPathCurrent, ch->prnName);
	acqCoarse->acqBinLog = fopen(filename, "rb");
	if (acqCoarse->acqBinLog != NULL) {
	    /* if the log file already exists, append */
	    fclose(acqCoarse->acqBinLog);
	    acqCoarse->acqBinLog = fopen(filename, "ab");
	} else {
	    /* if the log file does not exist, create and initializes */
	    acqCoarse->acqBinLog = fopen(filename, "wb");
	    fwrite(&(acqCommon->dopplerRefNumSteps), sizeof(int), 1, acqCoarse->acqBinLog);
	    fwrite(acqCommon->dopplerRefSteps, sizeof(float), acqCommon->dopplerRefNumSteps, acqCoarse->acqBinLog);
	}
    }
}

TAcquisitionStatus PerformDopplerRefinement(TNGeneConfiguration *cfg, TNGeneData *rx, TChannel *ch, char *firstBunch, unsigned int firstBunchSize, char *secondBunch, unsigned int secondBunchSize)
{
    TRefinement *ref = &(ch->refinement);
    double fcCorrection = (double) rx->acqCommon.dopplerRefSteps[ref->indexBin];
    unsigned int p;
    double codeRateNew;
    double fd;
    long long int elapsedSamples;
    long long int elapsedSamplesCorrect;
    int sampleCorrection;

    /* generate and remove carrier */
    ch->carrierPhase = GenerateCarrier(cfg, &(rx->fifTable), ch->carrierFrequency + fcCorrection, ch->carrierPhase, ch->blocksize, rx->sinArray, rx->cosArray);
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
    ref->pI += (ArrayMac(rx->cosArray, rx->codeArray, ch->blocksize) + (ch->blocksize / 2));
    ref->pQ += (ArrayMac(rx->sinArray, rx->codeArray, ch->blocksize) + (ch->blocksize / 2));
    /* analyze the result */
    ref->msCounter++;
    if (ref->msCounter == ch->codeDuration) {
	p = (unsigned int) (ref->pI * ref->pI) + (unsigned int) (ref->pQ * ref->pQ);
	ref->msCounter = 0;
	ref->pI = 0;
	ref->pQ = 0;
	if (ref->nonCoherentSumsIndex == 0) {
	    ref->p = 0;
	}
	ref->p += (p / ref->numOfNonCoherentSums);
	ref->nonCoherentSumsIndex++;
	if (ref->nonCoherentSumsIndex >= ref->numOfNonCoherentSums) {
	    ref->nonCoherentSumsIndex = 0;
	    if (ref->p > ref->maximum) {
		ref->maximum = ref->p;
		ref->indexBinMax = ref->indexBin;
	    }

	    if (ch->coarse.acqBinLog != NULL) {
		fwrite(&(ref->p), sizeof(unsigned int), 1, ch->coarse.acqBinLog);
	    }
	    ref->indexBin++;
	    if (ref->indexBin >= rx->acqCommon.dopplerRefNumSteps) {
		/* it is finished! */
		ch->carrierFrequency += (double) rx->acqCommon.dopplerRefSteps[ref->indexBinMax];
		ch->dopplerShift = fd = ch->carrierFrequency - cfg->fif;
		codeRateNew = GPS_CODE_RATE * (1.0 + ((double) cfg->fePars->dopplerCorrection * (fd / L1_FREQUENCY)));  /* all the code rates are equal */
		elapsedSamples = rx->buf.discardedSamples + ch->bufferOffset + ch->blocksize - ref->discardedSamples;
		elapsedSamplesCorrect = (long long int) ((double) elapsedSamples * (ch->codeRate / codeRateNew));
		sampleCorrection = (int) (elapsedSamplesCorrect - elapsedSamples);
		if ((sampleCorrection >= 0) || ((-sampleCorrection) < (int) ch->blocksize)) {
		    ch->blocksize += sampleCorrection;
		} else {
		    ch->blocksize = 0;
		}
		ch->codePhase = 0.0;
		ch->codePhaseFrac = 0.0;
		ch->codeRate = codeRateNew;
		return ACQUISITION_TERMINATED;
	    }
	}
    }
    return ACQUISITION_IN_EXECUTION;
}

void StartCodeRefinement(TNGeneConfiguration *cfg, TNGeneData *rx, TChannel *ch)
{
    TAcqCommon *acqCommon = &(rx->acqCommon);
    TRefinement *ref = &(ch->refinement);
    unsigned int skippedSamples;
    TCoarseAcquisition *acqCoarse = &(ch->coarse);
    char filename[2 * MAX_ROW_LENGTH];

    InitRefinement(cfg, ref, ch);
    ref->discardedSamples = rx->buf.discardedSamples + ch->bufferOffset + ch->blocksize;
    /* apply a displacement in the input buffer in order to test all the bins over the code delays */
    if (ch->blocksize >= (unsigned int) acqCommon->codeRefDelta) {
	ch->blocksize -= acqCommon->codeRefDelta;
    } else if (ch->bufferOffset >= (unsigned int) acqCommon->codeRefDelta) {
	ch->bufferOffset -= acqCommon->codeRefDelta;
    } else if ((ch->blocksize + ch->bufferOffset) >= (unsigned int) acqCommon->codeRefDelta) {
	ch->blocksize = 0;
	ch->bufferOffset = ch->bufferOffset - (acqCommon->codeRefDelta - ch->blocksize);
    } else {
	skippedSamples = (unsigned int) ((((double) ch->codeLen - ch->codePhase) / ch->codeRate) * cfg->fs) + 1;
	UpdateCodePhase(cfg, ch->satType, &(ch->codePhase), &(ch->codePhaseFrac), skippedSamples, ch->codeRate);
	if (skippedSamples > (unsigned int) acqCommon->codeRefDelta) {
	    skippedSamples -= acqCommon->codeRefDelta;
	}
	ch->blocksize += skippedSamples;
    }

    if (cfg->logConfiguration.enableAcqLog == TRUE_FLAG) {
	if (acqCoarse->acqBinLog != NULL) {
	    fclose(acqCoarse->acqBinLog);
	    acqCoarse->acqBinLog = NULL;
	}
	sprintf(filename, "%s/codeRefinement_%s.bin", cfg->dumpPathCurrent, ch->prnName);
	acqCoarse->acqBinLog = fopen(filename, "rb");
	if (acqCoarse->acqBinLog != NULL) {
	    /* if the log file already exists, append */
	    fclose(acqCoarse->acqBinLog);
	    acqCoarse->acqBinLog = fopen(filename, "ab");
	} else {
	    /* if the log file does not exist, create and initializes */
	    acqCoarse->acqBinLog = fopen(filename, "wb");
	    fwrite(&(acqCommon->codeRefNumSteps), sizeof(int), 1, acqCoarse->acqBinLog);
	    fwrite(acqCommon->codeRefSteps, sizeof(int), acqCommon->codeRefNumSteps, acqCoarse->acqBinLog);
	}
    }
}

TAcquisitionStatus PerformCodeRefinement(TNGeneConfiguration *cfg, TNGeneData *rx, TChannel *ch, char *firstBunch, unsigned int firstBunchSize, char *secondBunch, unsigned int secondBunchSize)
{
    TRefinement *ref = &(ch->refinement);
    int curDelay = rx->acqCommon.codeRefSteps[ref->indexBin];
    char *input1;
    unsigned int size1;
    char *input2;
    unsigned int size2;
    unsigned int p;

    /* the code offset could require more samples then those available */
    if (((unsigned int) curDelay + ch->blocksize) > (firstBunchSize + secondBunchSize))
	return ACQUISITION_SKIPPED;

    /* apply the code delay  */
    if ((unsigned int) curDelay < firstBunchSize) {
	input1 = &(firstBunch[curDelay]);
	size1 = firstBunchSize - curDelay;
	input2 = secondBunch;
	size2 = secondBunchSize;
    } else {
	input1 = &(secondBunch[curDelay - firstBunchSize]);
	size1 = secondBunchSize - curDelay + firstBunchSize;
	input2 = NULL;
	size2 = 0;
    }

    /* generate and remove carrier */
    ch->carrierPhase = GenerateCarrier(cfg, &(rx->fifTable), ch->carrierFrequency, ch->carrierPhase, ch->blocksize, rx->sinArray, rx->cosArray);
    CarrierComplexWipeOff(input1, size1, input2, size2, rx->cosArray, rx->sinArray, ch->blocksize, rx->cosArray, rx->sinArray);
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
    ref->pI += (ArrayMac(rx->cosArray, rx->codeArray, ch->blocksize) + (ch->blocksize / 2));
    ref->pQ += (ArrayMac(rx->sinArray, rx->codeArray, ch->blocksize) + (ch->blocksize / 2));
    /* analyze the result */
    ref->msCounter++;
    if (ref->msCounter == ch->codeDuration) {
	p = (unsigned int) (ref->pI * ref->pI) + (unsigned int) (ref->pQ * ref->pQ);
	ref->msCounter = 0;
	ref->pI = 0;
	ref->pQ = 0;
	if (ref->nonCoherentSumsIndex == 0) {
	    ref->p = 0;
	}
	ref->p += (p / ref->numOfNonCoherentSums);
	ref->nonCoherentSumsIndex++;
	if (ref->nonCoherentSumsIndex >= ref->numOfNonCoherentSums) {
	    ref->nonCoherentSumsIndex = 0;
	    if (ref->p > ref->maximum) {
		ref->maximum = ref->p;
		ref->indexBinMax = ref->indexBin;
	    }

	    if (ch->coarse.acqBinLog != NULL) {
		fwrite(&(ref->p), sizeof(unsigned int), 1, ch->coarse.acqBinLog);
	    }

	    ref->indexBin++;
	    if (ref->indexBin >= rx->acqCommon.codeRefNumSteps) {
		/* it is finished! */
		ch->blocksize += rx->acqCommon.codeRefSteps[ref->indexBinMax];
		return ACQUISITION_TERMINATED;
	    }
	}
    }
    return ACQUISITION_IN_EXECUTION;
}
