/**
 * @file spreadingCodes.c
 *
 * @author Mario Nicola
 *
 * @date 21/08/2014
 *
 * @version 1.0.0
 *
 * @brief Functions that manage acquisition data structures.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include "acquisition.h"
#include "ngeneConfiguration.h"
#include "acquisitionDataStructure.h"
#include "ngeneConstants.h"
#include "ioFunctions.h"
#include "closeManager.h"
#include "spreadingCodes.h"
#include "ngeneDataStructure.h"
#include "arrays.h"
#include "prnTable.h"
#include "sampleCompression.h"
#include "fft.h"
#include "mathFunctions.h"
#include "nonCoherentSumsManager.h"

/**
 * @def LOCAL_CODE_FFT_SIZE_FACTOR
 * @brief The FFT of the local codes are evaluated at the initialization and when stored are duplicated
 *        to ease the execution of the coarse acquisition.
 */
#define LOCAL_CODE_FFT_SIZE_FACTOR 4

int InitLocalCodesFft(TNGeneConfiguration *cfg, TNGeneData *rx)
{
    TAcqConfiguration *acqCfg = &(cfg->acqConfiguration);
    int index;
    int errorsFound = 0;
    TAcqCommon *acqCommon = &(rx->acqCommon);
    int codesFftLen;            /* = 2*2*ACQ_FFT_SIZE */

    /* Allocates memory */
    codesFftLen = LOCAL_CODE_FFT_SIZE_FACTOR * acqCfg->gpsFftSize;
    for (index = 0; index < GPS_NUMBER_OF_PRNS; index++) {
	posix_memalign((void **) &(acqCommon->gpsCodesFft[index]), POSIX_ALIGNMENT, codesFftLen * sizeof(short int));
	if (acqCommon->gpsCodesFft[index] == NULL)
	    errorsFound++;
    }
    for (index = 0; index < EGNOS_NUMBER_OF_PRNS; index++) {
	posix_memalign((void **) &(acqCommon->egnosCodesFft[index]), POSIX_ALIGNMENT, codesFftLen * sizeof(short int));
	if (acqCommon->egnosCodesFft[index] == NULL)
	    errorsFound++;
    }
    codesFftLen = LOCAL_CODE_FFT_SIZE_FACTOR * acqCfg->galileoFftSize;
    for (index = 0; index < GALILEO_NUMBER_OF_PRNS; index++) {
	posix_memalign((void **) &(acqCommon->galileoCodesFft[index]), POSIX_ALIGNMENT, codesFftLen * sizeof(short int));
	if (acqCommon->galileoCodesFft[index] == NULL)
	    errorsFound++;
    }
    if (errorsFound > 0) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Malloc returned a NULL pointer in InitLocalCodesFft");
	CloseNGene(cfg, EXIT_FAILURE);
    }

    /* Init the FFT of local codes: read them from file
     * - if it is enabled (FFT_OF_LOCAL_CODES_FROM_FILE defined)
     * - fs is not very different from the standard values (fs > FS_LOW)
     * - if the FFT size for the current constellation is equal to the default value
     *
     * The file of the FFT of the local codes is saved
     * - SAVE_FFT_OF_LOCAL_CODES is defined
     * - both GPS and Galileo use the default FFT size */

    /* Declare and init variables */
    // variable used when FFT of codes is loaded from file
#ifdef FFT_OF_LOCAL_CODES_FROM_FILE
    char filename[MAX_ROW_LENGTH];
    FILE *fftFile;

    filename[0] = '\0';
    strcat(filename, cfg->otherDataPath);
    strcat(filename, "FFT_codes.dat");
    fftFile = fopen(filename, "rb");
    if (fftFile == NULL) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Cannot read file with FFT of despreading codes (%s)", filename);
	CloseNGene(cfg, EXIT_FAILURE);
    }
#endif
    /* other variables */
    short int *signalCompressed = NULL; /* Array used to store the result of a compression */
    short int *signalFft = NULL;        /* Array used to store the result of an FFT */
    int size = (GALILEO_CODE_DURATION + 1) * (int) cfg->samplesPerMillisecond * sizeof(char);
    int prn;
    char *code = NULL;
    char *zeros = NULL;
    int blocksize;
    double codePhase;
    double codePhaseFrac;
    int index2;
    short *ptr1;
    short *ptr2;
#ifdef SAVE_FFT_OF_LOCAL_CODES
    FILE *fftFile;

    fftFile = fopen("FFT_codes.dat", "wb");
    if (fftFile == NULL) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Cannot write file with FFT of despreading codes");
	CloseNGene(cfg, EXIT_FAILURE);
    }
#endif
    posix_memalign((void **) &code, POSIX_ALIGNMENT, size);
    posix_memalign((void **) &zeros, POSIX_ALIGNMENT, size);
    posix_memalign((void **) &signalCompressed, POSIX_ALIGNMENT, 2 * ACQ_DEFAULT_FFT_SIZE * sizeof(short int));
    posix_memalign((void **) &signalFft, POSIX_ALIGNMENT, 2 * ACQ_DEFAULT_FFT_SIZE * sizeof(short int));
    ArrayClean(zeros, size);

    /* GPS and EGNOS */
    codesFftLen = LOCAL_CODE_FFT_SIZE_FACTOR * acqCfg->gpsFftSize;
#ifdef FFT_OF_LOCAL_CODES_FROM_FILE
    if ((cfg->fs > LOW_FS) && (acqCfg->gpsFftSize == ACQ_DEFAULT_FFT_SIZE)) {
	for (index = 0; index < GPS_NUMBER_OF_PRNS; index++) {
	    fread(acqCommon->gpsCodesFft[index], sizeof(short int), codesFftLen, fftFile);
	}
	for (index = 0; index < EGNOS_NUMBER_OF_PRNS; index++) {
	    /* If the FFT of PRN 136 is not available use a fake array made of 0s */
	    if ((rx->prnTable.egnos136Valid == FALSE_FLAG) && (index == (EGNOS_NUMBER_OF_PRNS - 1))) {
		ArrayClean((char *) acqCommon->egnosCodesFft[index], sizeof(short int) * codesFftLen);
	    } else {
		fread(acqCommon->egnosCodesFft[index], sizeof(short int), codesFftLen, fftFile);
	    }
	}
    } else
#endif
    {
	/* GPS */
	blocksize = (int) (GPS_CODE_DURATION * cfg->samplesPerMillisecond);
	for (prn = BASE_PRN; prn <= GPS_NUMBER_OF_PRNS; prn++) {
	    codePhase = 0.0;
	    codePhaseFrac = 0.0;
	    GenerateCode(cfg, &(rx->prnTable), GPS_SAT, prn, (float) GPS_CODE_RATE, &codePhase, &codePhaseFrac, BPSK_MODULATION, blocksize, code);
	    SampleCompression(0, 0, code, zeros, blocksize, signalCompressed, acqCfg->gpsFftSize);
	    PerformFft(cfg, &(rx->fftTable), signalCompressed, signalFft, acqCfg->gpsLog2FftSize, acqCfg->gpsLocalCodeFftGain);
	    ptr1 = signalFft;
	    ptr2 = acqCommon->gpsCodesFft[prn - BASE_PRN];
	    for (index2 = 0; index2 < acqCfg->gpsFftSize; index2++) {
		ptr2[0] = +ptr1[0];     /* real  part of the sample */
		ptr2[1] = +ptr1[1];     /* imag  part of the sample */
		ptr2[2] = -ptr1[1];     /* -imag part of the sample */
		ptr2[3] = +ptr1[0];     /* real  part of the sample */
		ptr1 += 2;
		ptr2 += 4;
	    }
#ifdef SAVE_FFT_OF_LOCAL_CODES
	    if ((acqCfg->gpsFftSize == ACQ_DEFAULT_FFT_SIZE) && (acqCfg->galileoFftSize == ACQ_DEFAULT_FFT_SIZE)) {
		fwrite(acqCommon->gpsCodesFft[prn - BASE_PRN], sizeof(short int), codesFftLen, fftFile);
	    }
#endif
	}
	/* EGNOS */
	blocksize = (int) (EGNOS_CODE_DURATION * cfg->samplesPerMillisecond);
	for (index = 0; index < EGNOS_NUMBER_OF_PRNS; index++) {
	    if (index == 0)
		prn = EGNOS_FIRST_PRN;
	    else if (index == 1)
		prn = EGNOS_SECOND_PRN;
	    else if (index == 2)
		prn = EGNOS_THIRD_PRN;
	    else
		prn = EGNOS_FOURTH_PRN;
	    codePhase = 0.0;
	    codePhaseFrac = 0.0;
	    GenerateCode(cfg, &(rx->prnTable), EGNOS_SAT, prn, (float) EGNOS_CODE_RATE, &codePhase, &codePhaseFrac, BPSK_MODULATION, blocksize, code);
	    SampleCompression(0, 0, code, zeros, blocksize, signalCompressed, acqCfg->gpsFftSize);
	    PerformFft(cfg, &(rx->fftTable), signalCompressed, signalFft, acqCfg->gpsLog2FftSize, acqCfg->gpsLocalCodeFftGain);
	    ptr1 = signalFft;
	    ptr2 = acqCommon->egnosCodesFft[index];
	    for (index2 = 0; index2 < acqCfg->gpsFftSize; index2++) {
		ptr2[0] = +ptr1[0];     /* real  part of the sample */
		ptr2[1] = +ptr1[1];     /* imag  part of the sample */
		ptr2[2] = -ptr1[1];     /* -imag part of the sample */
		ptr2[3] = +ptr1[0];     /* real  part of the sample */
		ptr1 += 2;
		ptr2 += 4;
	    }
#ifdef SAVE_FFT_OF_LOCAL_CODES
	    if ((acqCfg->gpsFftSize == ACQ_DEFAULT_FFT_SIZE) && (acqCfg->galileoFftSize == ACQ_DEFAULT_FFT_SIZE)) {
		fwrite(acqCommon->egnosCodesFft[index], sizeof(short int), codesFftLen, fftFile);
	    }
#endif
	}
    }
    /* Galileo */
    codesFftLen = LOCAL_CODE_FFT_SIZE_FACTOR * acqCfg->galileoFftSize;
#ifdef FFT_OF_LOCAL_CODES_FROM_FILE
    if ((cfg->fs > LOW_FS) && (acqCfg->galileoFftSize == ACQ_DEFAULT_FFT_SIZE)) {
	for (index = 0; index < GALILEO_NUMBER_OF_PRNS; index++) {
	    fread(acqCommon->galileoCodesFft[index], sizeof(short int), codesFftLen, fftFile);
	}
    } else
#endif
    {
	blocksize = (int) (GALILEO_CODE_DURATION * cfg->samplesPerMillisecond);
	for (prn = BASE_PRN; prn <= GALILEO_NUMBER_OF_PRNS; prn++) {
	    codePhase = 0.0;
	    codePhaseFrac = 0.0;
	    GenerateCode(cfg, &(rx->prnTable), GALILEO_SAT, prn, (float) GALILEO_CODE_RATE, &codePhase, &codePhaseFrac, BPSK_MODULATION, blocksize, code);
	    SampleCompression(0, 0, code, zeros, blocksize, signalCompressed, acqCfg->galileoFftSize);
	    PerformFft(cfg, &(rx->fftTable), signalCompressed, signalFft, acqCfg->galileoLog2FftSize, acqCfg->galileoLocalCodeFftGain);
	    ptr1 = signalFft;
	    ptr2 = acqCommon->galileoCodesFft[prn - BASE_PRN];
	    for (index2 = 0; index2 < acqCfg->galileoFftSize; index2++) {
		ptr2[0] = +ptr1[0];     /* real  part of the sample */
		ptr2[1] = +ptr1[1];     /* imag  part of the sample */
		ptr2[2] = -ptr1[1];     /* -imag part of the sample */
		ptr2[3] = +ptr1[0];     /* real  part of the sample */
		ptr1 += 2;
		ptr2 += 4;
	    }
#ifdef SAVE_FFT_OF_LOCAL_CODES
	    if ((acqCfg->gpsFftSize == ACQ_DEFAULT_FFT_SIZE) && (acqCfg->galileoFftSize == ACQ_DEFAULT_FFT_SIZE)) {
		fwrite(acqCommon->galileoCodesFft[prn - BASE_PRN], sizeof(short int), codesFftLen, fftFile);
	    }
#endif
	}
    }
    /* Deinit */
    free(code);
    free(zeros);
    free(signalCompressed);
    free(signalFft);
#ifdef FFT_OF_LOCAL_CODES_FROM_FILE
    fclose(fftFile);
#endif
#ifdef SAVE_FFT_OF_LOCAL_CODES
    fclose(fftFile);
#endif

    return SUCCESS_RETURN;
}

int InitThresholds(TNGeneConfiguration *cfg, TNGeneData *rx)
{
    TThresholdManager *thr = &(rx->acqCommon.thr);
    int errorsFound = 0;
    double phase;
    double phaseFrac;

    thr->coarseVarianceGps = 0;
    thr->coarseVarianceGal = 0;
    thr->blocksize = (unsigned int) (GALILEO_CODE_DURATION + 1) * cfg->samplesPerMillisecond;
    posix_memalign((void **) &(thr->code), POSIX_ALIGNMENT, thr->blocksize * sizeof(char));
    if (thr->code == NULL)
	errorsFound++;
    posix_memalign((void **) &(thr->carrierCos), POSIX_ALIGNMENT, thr->blocksize * sizeof(char));
    if (thr->carrierCos == NULL)
	errorsFound++;
    posix_memalign((void **) &(thr->carrierSin), POSIX_ALIGNMENT, thr->blocksize * sizeof(char));
    if (thr->carrierSin == NULL)
	errorsFound++;
    posix_memalign((void **) &(thr->demodulatedCos), POSIX_ALIGNMENT, thr->blocksize * sizeof(char));
    if (thr->demodulatedCos == NULL)
	errorsFound++;
    posix_memalign((void **) &(thr->demodulatedSin), POSIX_ALIGNMENT, thr->blocksize * sizeof(char));
    if (thr->demodulatedSin == NULL)
	errorsFound++;
    if (errorsFound > 0) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Malloc returned a NULL pointer in InitThresholds");
	CloseNGene(cfg, EXIT_FAILURE);
    }
    thr->blocksize = (unsigned int) GALILEO_CODE_DURATION *cfg->samplesPerMillisecond;
    phase = 0.0;
    phaseFrac = 0.0;
    GenerateCarrier(cfg, &(rx->fscTable), GALILEO_CODE_RATE, phase, thr->blocksize, thr->carrierSin, thr->carrierCos);
    GenerateCode(cfg, &(rx->prnTable), GALILEO_SAT, GALILEO_NUMBER_OF_PRNS, GALILEO_CODE_RATE, &phase, &(phaseFrac), NGENE_MODULATION, thr->blocksize, thr->code);
    ArrayXor(thr->carrierSin, thr->code, thr->code, thr->blocksize);
    phase = 0.0;
    GenerateCarrier(cfg, &(rx->fifTable), cfg->fif + THRESHOLD_FAKE_DOPPLER, phase, thr->blocksize, thr->carrierSin, thr->carrierCos);
    InitAverageWindow(&(thr->averageCorrGps), THRESHOLD_NUMBER_OF_ESTIMATIONS, 0.0);
    InitAverageWindow(&(thr->averageCorr2Gps), THRESHOLD_NUMBER_OF_ESTIMATIONS, 0.0);
    InitAverageWindow(&(thr->averageCorrGal), THRESHOLD_NUMBER_OF_ESTIMATIONS, 0.0);
    InitAverageWindow(&(thr->averageCorr2Gal), THRESHOLD_NUMBER_OF_ESTIMATIONS, 0.0);
    thr->numberOfEstimationsForConfirmation = 0;
    return SUCCESS_RETURN;
}

void DeinitThresholds(TThresholdManager *thr)
{
    free(thr->code);
    free(thr->carrierCos);
    free(thr->carrierSin);
    free(thr->demodulatedCos);
    free(thr->demodulatedSin);
}

int InitAcquisitionCommon(TNGeneConfiguration *cfg, TNGeneData *rx)
{
    TAcqCommon *acq = &(rx->acqCommon);
    TAcqConfiguration *acqPars = &(cfg->acqConfiguration);
    int errorsFound = 0;
    unsigned int arraySize = NGENE_ARRAYS_SIZE * cfg->samplesPerMillisecond;
    int index;
    float compressionRatio;
    int codeStep;

    posix_memalign((void **) &(acq->demodulatedCos), POSIX_ALIGNMENT, arraySize * sizeof(char));
    if (acq->demodulatedCos == NULL)
	errorsFound++;
    posix_memalign((void **) &(acq->demodulatedSin), POSIX_ALIGNMENT, arraySize * sizeof(char));
    if (acq->demodulatedSin == NULL)
	errorsFound++;
    if (errorsFound > 0) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Malloc returned a NULL pointer in InitAcquisitionCommon");
	CloseNGene(cfg, EXIT_FAILURE);
    }

    /* Coarse acquisition */
    InitLocalCodesFft(cfg, rx);
    /* Non coherent sums */
    if (InitNonCoherentSumsManager(cfg, rx) != SUCCESS_RETURN) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Error value returned by InitNonCoherentSumsManager in InitAcquisitionCommon");
	CloseNGene(cfg, EXIT_FAILURE);
    }
    /* Thresholds */
    InitThresholds(cfg, rx);
    /* Search spaces */
    acq->gpsNumDopplers = NearestInteger((2.0 * acqPars->gpsMaximumDoppler) / acqPars->gpsDopplerStep) + 1;
    acq->gpsDopplers = (float *) malloc(acq->gpsNumDopplers * sizeof(float));
    acq->gpsDopplers[0] = -acqPars->gpsMaximumDoppler;
    for (index = 1; index < acq->gpsNumDopplers; index++)
	acq->gpsDopplers[index] = acq->gpsDopplers[index - 1] + acqPars->gpsDopplerStep;
    compressionRatio = (float) GPS_CODE_LEN / (float) acqPars->gpsFftSize;
    if (acqPars->gpsCodeDelayStep > compressionRatio) {
	acq->gpsNumDelays = 1;
    } else {
	acq->gpsNumDelays = (int) floor(compressionRatio / acqPars->gpsCodeDelayStep);
    }
    codeStep = NearestInteger(cfg->samplesPerChip * acqPars->gpsCodeDelayStep);
    acq->gpsDelays = (int *) malloc(acq->gpsNumDelays * sizeof(int));
    acq->gpsDelays[0] = 0;
    for (index = 1; index < acq->gpsNumDelays; index++) {
	acq->gpsDelays[index] = acq->gpsDelays[index - 1] + codeStep;
    }

    acq->egnosNumDopplers = NearestInteger((2.0 * acqPars->egnosMaximumDoppler) / acqPars->egnosDopplerStep) + 1;
    acq->egnosDopplers = (float *) malloc(acq->egnosNumDopplers * sizeof(float));
    acq->egnosDopplers[0] = -acqPars->egnosMaximumDoppler;
    for (index = 1; index < acq->egnosNumDopplers; index++)
	acq->egnosDopplers[index] = acq->egnosDopplers[index - 1] + acqPars->egnosDopplerStep;
    compressionRatio = (float) EGNOS_CODE_LEN / (float) acqPars->gpsFftSize;
    if (acqPars->egnosCodeDelayStep > compressionRatio) {
	acq->egnosNumDelays = 1;
    } else {
	acq->egnosNumDelays = (int) floor(compressionRatio / acqPars->egnosCodeDelayStep);
    }
    codeStep = NearestInteger(cfg->samplesPerChip * acqPars->egnosCodeDelayStep);
    acq->egnosDelays = (int *) malloc(acq->egnosNumDelays * sizeof(int));
    acq->egnosDelays[0] = 0;
    for (index = 1; index < acq->egnosNumDelays; index++) {
	acq->egnosDelays[index] = acq->egnosDelays[index - 1] + codeStep;
    }

    acq->galileoNumDopplers = NearestInteger((2.0 * acqPars->galileoMaximumDoppler) / acqPars->galileoDopplerStep) + 1;
    acq->galileoDopplers = (float *) malloc(acq->galileoNumDopplers * sizeof(float));
    acq->galileoDopplers[0] = -acqPars->galileoMaximumDoppler;
    for (index = 1; index < acq->galileoNumDopplers; index++)
	acq->galileoDopplers[index] = acq->galileoDopplers[index - 1] + acqPars->galileoDopplerStep;
    compressionRatio = (float) GALILEO_CODE_LEN / (float) acqPars->galileoFftSize;
    if (acqPars->galileoCodeDelayStep > compressionRatio) {
	acq->galileoNumDelays = 1;
    } else {
	acq->galileoNumDelays = (int) floor(compressionRatio / acqPars->galileoCodeDelayStep);
    }
    codeStep = NearestInteger(cfg->samplesPerChip * acqPars->galileoCodeDelayStep);
    acq->galileoDelays = (int *) malloc(acq->galileoNumDelays * sizeof(int));
    acq->galileoDelays[0] = 0;
    for (index = 1; index < acq->galileoNumDelays; index++) {
	acq->galileoDelays[index] = acq->galileoDelays[index - 1] + codeStep;
    }
    /* Doppler refinement */
    acq->dopplerRefNumSteps = NearestInteger((2.0 * acqPars->dopplerDeltaRefinement) / acqPars->dopplerStepRefinement) + 1;
    acq->dopplerRefSteps = (float *) malloc(acq->dopplerRefNumSteps * sizeof(float));
    acq->dopplerRefSteps[0] = -acqPars->dopplerDeltaRefinement;
    for (index = 1; index < acq->dopplerRefNumSteps; index++) {
	acq->dopplerRefSteps[index] = acq->dopplerRefSteps[index - 1] + acqPars->dopplerStepRefinement;
    }
    /* Code refinement */
    acq->codeRefDelta = NearestInteger((acqPars->codeDelayDeltaRefinement * cfg->fs) / GPS_CODE_RATE);
    acq->codeRefNumSteps = NearestInteger((2.0 * acq->codeRefDelta) / acqPars->codeDelayStepRefinement) + 1;
    acq->codeRefSteps = (int *) malloc(acq->codeRefNumSteps * sizeof(int));
    acq->codeRefSteps[0] = 0;
    for (index = 1; index < acq->codeRefNumSteps; index++) {
	acq->codeRefSteps[index] = acq->codeRefSteps[index - 1] + acqPars->codeDelayStepRefinement;
    }

    return SUCCESS_RETURN;
}

void DeinitAcquisitionCommon(TAcqCommon *acqCommon)
{
    int index;

    for (index = 0; index < GPS_NUMBER_OF_PRNS; index++) {
	free(acqCommon->gpsCodesFft[index]);
    }
    for (index = 0; index < EGNOS_NUMBER_OF_PRNS; index++) {
	free(acqCommon->egnosCodesFft[index]);
    }
    for (index = 0; index < GALILEO_NUMBER_OF_PRNS; index++) {
	free(acqCommon->galileoCodesFft[index]);
    }
    free(acqCommon->demodulatedCos);
    free(acqCommon->demodulatedSin);
    free(acqCommon->gpsDopplers);
    free(acqCommon->gpsDelays);
    free(acqCommon->egnosDopplers);
    free(acqCommon->egnosDelays);
    free(acqCommon->galileoDopplers);
    free(acqCommon->galileoDelays);
    free(acqCommon->dopplerRefSteps);
    free(acqCommon->codeRefSteps);
    DeinitNonCoherentSumsManager(&(acqCommon->nonCohManager));
    DeinitThresholds(&(acqCommon->thr));
}

double FindAcquisitionThreshold(double noiseVariance, int nonCoherentSums, double falseDetectionProbability)
{
    double equationParameters[3];
    double x1 = 0.0;
    double x2 = 0.0;
    unsigned int errore;
    double acqThreshold;
    /* threshold is calculated accordingly to statistical properties and chosen false alarm probability */
    equationParameters[0] = falseDetectionProbability;
    equationParameters[1] = noiseVariance;
    equationParameters[2] = (double) nonCoherentSums;
    Edges(&x1, &x2, FalseAlarm, equationParameters);
    acqThreshold = SolveEqn(x1, x2, FalseAlarm, equationParameters, 1E-3, 1000000, &errore);
    return acqThreshold;
}

void UpdateCoarseThreshold(TNGeneConfiguration *cfg, TChannel *ch, int nonCoherentSums)
{
    TAcqConfiguration *acqCfg = &(cfg->acqConfiguration);
    TCoarseAcquisition *acqCoarse = &(ch->coarse);
    TConfirmation *acqConf = &(ch->confirmation);
    int avg = 0;
    unsigned long long int avg2 = 0;
    double avgFloat;
    double avg2Float;
    double variance;
    int index;
    int signalFftLen;           /* = 2*ACQ_FFT_SIZE */

    /* FFT size is function of the signal */
    if (ch->satType == GPS_SAT)
	signalFftLen = 2 * acqCfg->gpsFftSize;
    else if (ch->satType == GALILEO_SAT)
	signalFftLen = 2 * acqCfg->galileoFftSize;
    else                        /* EGNOS_SAT */
	signalFftLen = 2 * acqCfg->gpsFftSize;
    variance = 0;
    /* In ifftOut the result of IFFT is already present! */
    for (index = 0; index < signalFftLen; index++) {
	avg += (int) acqCoarse->ifftOut[index];
	avg2 += (unsigned long long int) (acqCoarse->ifftOut[index] * (unsigned long long int) acqCoarse->ifftOut[index]);
    }
    avgFloat = (double) avg / (double) signalFftLen;
    avg2Float = (double) avg2 / (double) signalFftLen;
    variance = avg2Float - (avgFloat * avgFloat);
    if (ch->satType == GPS_SAT) {
	acqCoarse->threshold = (unsigned int) FindAcquisitionThreshold(variance, nonCoherentSums, cfg->acqConfiguration.gpsFalseDetectionProb);
	acqConf->threshold = acqCoarse->threshold / THRESHOLD_COARSE_TO_CONFIRMATION_FACTOR;
	SwrxLog(NGENE_LOG_SEVERITY_NOTE, "Set GPS threshold for coarse acquisition to %u", acqCoarse->threshold);
    } else if (ch->satType == GALILEO_SAT) {
	acqCoarse->threshold = (unsigned int) FindAcquisitionThreshold(variance, nonCoherentSums, cfg->acqConfiguration.galileoFalseDetectionProb);
	acqConf->threshold = acqCoarse->threshold / THRESHOLD_COARSE_TO_CONFIRMATION_FACTOR;
	SwrxLog(NGENE_LOG_SEVERITY_NOTE, "Set Galileo threshold for coarse acquisition to %u", acqCoarse->threshold);
    } else {                    /* if(satType==EGNOS_SAT) */
	acqCoarse->threshold = (unsigned int) FindAcquisitionThreshold(variance, nonCoherentSums, cfg->acqConfiguration.egnosFalseDetectionProb);
	acqConf->threshold = acqCoarse->threshold / THRESHOLD_COARSE_TO_CONFIRMATION_FACTOR;
	SwrxLog(NGENE_LOG_SEVERITY_NOTE, "Set EGNOS threshold for coarse acquisition to %u", acqCoarse->threshold);
    }
}

void UpdateVariablesForConfirmationThresholds(TNGeneData *rx)
{
    TAcqCommon *acqCommon = &(rx->acqCommon);
    TThresholdManager *thr = &(acqCommon->thr);
    char *inputSamples = rx->buf.buffer;
    double pi;
    double pq;
    int gpsBlocksize = thr->blocksize / 4;

    ArrayXor(inputSamples, thr->carrierCos, thr->demodulatedCos, thr->blocksize);
    ArrayXor(inputSamples, thr->carrierSin, thr->demodulatedSin, thr->blocksize);
    /* GPS */
    pi = (double) ArrayMac(thr->code, thr->carrierCos, gpsBlocksize) + (gpsBlocksize / 2);
    pq = (double) ArrayMac(thr->code, thr->carrierSin, gpsBlocksize) + (gpsBlocksize / 2);
    PerformAverageWindow(&(thr->averageCorrGps), pi);
    PerformAverageWindow(&(thr->averageCorr2Gps), pi * pi);
    PerformAverageWindow(&(thr->averageCorrGps), pq);
    PerformAverageWindow(&(thr->averageCorr2Gps), pq * pq);
    /* Galileo */
    pi += (double) ArrayMac(&(thr->code[gpsBlocksize]), &(thr->carrierCos[gpsBlocksize]), thr->blocksize - gpsBlocksize);
    pi += ((thr->blocksize - gpsBlocksize) / 2);
    pq += (double) ArrayMac(&(thr->code[gpsBlocksize]), &(thr->carrierSin[gpsBlocksize]), thr->blocksize - gpsBlocksize);
    pq += ((thr->blocksize - gpsBlocksize) / 2);
    PerformAverageWindow(&(thr->averageCorrGal), pi);
    PerformAverageWindow(&(thr->averageCorr2Gal), pi * pi);
    PerformAverageWindow(&(thr->averageCorrGal), pq);
    PerformAverageWindow(&(thr->averageCorr2Gal), pq * pq);
    if (thr->numberOfEstimationsForConfirmation < THRESHOLD_NUMBER_OF_ESTIMATIONS) {
	thr->numberOfEstimationsForConfirmation++;
    }
}

#ifndef DISABLE_CONFIRMATION_THRESHOLD
void UpdateConfirmationThresholds(TNGeneConfiguration *cfg, TNGeneData *rx, TChannel *ch)
{
    TConfirmation *conf = &(ch->confirmation);
    TCoarseAcquisition *coarse = &(ch->coarse);
    TAcqCommon *acqCommon = &(rx->acqCommon);
    TThresholdManager *thr = &(acqCommon->thr);
    double variance;
    double avg;
    double avg2;

    if (thr->numberOfEstimationsForConfirmation >= THRESHOLD_NUMBER_OF_ESTIMATIONS) {
	/* thr->numberOfEstimationsForConfirmation=0; */
	if (ch->satType == GALILEO_SAT) {
	    avg = GetAverageWindowValue(&(thr->averageCorrGal));
	    avg2 = GetAverageWindowValue(&(thr->averageCorr2Gal));
	    variance = avg2 - (avg * avg);
	    conf->threshold = (unsigned int) FindAcquisitionThreshold(variance, conf->numOfNonCoherentSums, cfg->acqConfiguration.galileoFalseDetectionProb);
	    SwrxLog(NGENE_LOG_SEVERITY_NOTE, "Set Galileo threshold for confirmation to %u", conf->threshold);
	} else {
	    avg = GetAverageWindowValue(&(thr->averageCorrGps));
	    avg2 = GetAverageWindowValue(&(thr->averageCorr2Gps));
	    variance = avg2 - (avg * avg);
	    if (ch->satType == GPS_SAT) {
		conf->threshold = (unsigned int) FindAcquisitionThreshold(variance, conf->numOfNonCoherentSums, cfg->acqConfiguration.gpsFalseDetectionProb);
		SwrxLog(NGENE_LOG_SEVERITY_NOTE, "Set GPS threshold for confirmation to %u", conf->threshold);
	    } else {            /* if(ch->satType==EGNOS_SAT) */
		conf->threshold = (unsigned int) FindAcquisitionThreshold(variance, conf->numOfNonCoherentSums, cfg->acqConfiguration.egnosFalseDetectionProb);
		SwrxLog(NGENE_LOG_SEVERITY_NOTE, "Set EGNOS threshold for confirmation to %u", conf->threshold);
	    }
	}
    } else {
	conf->threshold = coarse->threshold / THRESHOLD_COARSE_TO_CONFIRMATION_FACTOR;
    }
}
#else
void UpdateConfirmationThresholds(TChannel *ch)
{
    TConfirmation *conf = &(ch->confirmation);
    TCoarseAcquisition *coarse = &(ch->coarse);
    {
	conf->threshold = coarse->threshold / THRESHOLD_COARSE_TO_CONFIRMATION_FACTOR;
    }
}
#endif
