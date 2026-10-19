/**
 * @file manageAcqConfiguration.c
 *
 * @author Mario Nicola
 *
 * @date 07/08/2014
 *
 * @version 1.0.0
 *
 * @brief Functions that manage acquisition parameters.
 */

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "acqConfiguration.h"
#include "acquisition.h"
#include "ngeneConstants.h"
#include "timeConstants.h"
#include "ioFunctions.h"
#include "closeManager.h"

int ManageAcqConfiguration(TNGeneConfiguration *cfg)
{
    TAcqConfiguration *acqCfg;
    char cfgFilename[MAX_ROW_LENGTH];
    char tag[MAX_TAG_LEN];

    acqCfg = &(cfg->acqConfiguration);
    cfgFilename[0] = '\0';
    strcat(cfgFilename, cfg->configurationPath);
    strcat(cfgFilename, "Acquisition.conf");

    strcpy(tag, "GPS false detection probability");
    if (GetVal(cfgFilename, tag, (void *) &(acqCfg->gpsFalseDetectionProb), F_FLOAT, 1) == FAILURE_RETURN) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Error while parsing configuration file <%s>: tag <%s> not found", cfgFilename, tag);
	CloseNGene(cfg, EXIT_FAILURE);
    }
    strcpy(tag, "Galileo false detection probability");
    if (GetVal(cfgFilename, tag, (void *) &(acqCfg->galileoFalseDetectionProb), F_FLOAT, 1) == FAILURE_RETURN) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Error while parsing configuration file <%s>: tag <%s> not found", cfgFilename, tag);
	CloseNGene(cfg, EXIT_FAILURE);
    }
    strcpy(tag, "EGNOS false detection probability");
    if (GetVal(cfgFilename, tag, (void *) &(acqCfg->egnosFalseDetectionProb), F_FLOAT, 1) == FAILURE_RETURN) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Error while parsing configuration file <%s>: tag <%s> not found", cfgFilename, tag);
	CloseNGene(cfg, EXIT_FAILURE);
    }
    strcpy(tag, "GPS Maximum Doppler Frequency (Hz)");
    if (GetVal(cfgFilename, tag, (void *) &(acqCfg->gpsMaximumDoppler), F_FLOAT, 1) == FAILURE_RETURN) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Error while parsing configuration file <%s>: tag <%s> not found", cfgFilename, tag);
	CloseNGene(cfg, EXIT_FAILURE);
    }
    strcpy(tag, "Galileo Maximum Doppler Frequency (Hz)");
    if (GetVal(cfgFilename, tag, (void *) &(acqCfg->galileoMaximumDoppler), F_FLOAT, 1) == FAILURE_RETURN) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Error while parsing configuration file <%s>: tag <%s> not found", cfgFilename, tag);
	CloseNGene(cfg, EXIT_FAILURE);
    }
    strcpy(tag, "EGNOS Maximum Doppler Frequency (Hz)");
    if (GetVal(cfgFilename, tag, (void *) &(acqCfg->egnosMaximumDoppler), F_FLOAT, 1) == FAILURE_RETURN) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Error while parsing configuration file <%s>: tag <%s> not found", cfgFilename, tag);
	CloseNGene(cfg, EXIT_FAILURE);
    }
    strcpy(tag, "GPS Doppler Frequency step (Hz)");
    if (GetVal(cfgFilename, tag, (void *) &(acqCfg->gpsDopplerStep), F_FLOAT, 1) == FAILURE_RETURN) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Error while parsing configuration file <%s>: tag <%s> not found", cfgFilename, tag);
	CloseNGene(cfg, EXIT_FAILURE);
    }
    strcpy(tag, "Galileo Doppler Frequency step (Hz)");
    if (GetVal(cfgFilename, tag, (void *) &(acqCfg->galileoDopplerStep), F_FLOAT, 1) == FAILURE_RETURN) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Error while parsing configuration file <%s>: tag <%s> not found", cfgFilename, tag);
	CloseNGene(cfg, EXIT_FAILURE);
    }
    strcpy(tag, "EGNOS Doppler Frequency step (Hz)");
    if (GetVal(cfgFilename, tag, (void *) &(acqCfg->egnosDopplerStep), F_FLOAT, 1) == FAILURE_RETURN) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Error while parsing configuration file <%s>: tag <%s> not found", cfgFilename, tag);
	CloseNGene(cfg, EXIT_FAILURE);
    }
    strcpy(tag, "GPS Code delay step (Chip)");
    if (GetVal(cfgFilename, tag, (void *) &(acqCfg->gpsCodeDelayStep), F_FLOAT, 1) == FAILURE_RETURN) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Error while parsing configuration file <%s>: tag <%s> not found", cfgFilename, tag);
	CloseNGene(cfg, EXIT_FAILURE);
    }
    strcpy(tag, "Galileo Code delay step (Chip)");
    if (GetVal(cfgFilename, tag, (void *) &(acqCfg->galileoCodeDelayStep), F_FLOAT, 1) == FAILURE_RETURN) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Error while parsing configuration file <%s>: tag <%s> not found", cfgFilename, tag);
	CloseNGene(cfg, EXIT_FAILURE);
    }
    strcpy(tag, "EGNOS Code delay step (Chip)");
    if (GetVal(cfgFilename, tag, (void *) &(acqCfg->egnosCodeDelayStep), F_FLOAT, 1) == FAILURE_RETURN) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Error while parsing configuration file <%s>: tag <%s> not found", cfgFilename, tag);
	CloseNGene(cfg, EXIT_FAILURE);
    }
    strcpy(tag, "GPS non coherent sums (code periods)");
    if (GetVal(cfgFilename, tag, (void *) &(acqCfg->gpsNumOfNonCohSums), F_INTEGER, 1) == FAILURE_RETURN) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Error while parsing configuration file <%s>: tag <%s> not found", cfgFilename, tag);
	CloseNGene(cfg, EXIT_FAILURE);
    }
    strcpy(tag, "EGNOS non coherent sums (code periods)");
    if (GetVal(cfgFilename, tag, (void *) &(acqCfg->egnosNumOfNonCohSums), F_INTEGER, 1) == FAILURE_RETURN) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Error while parsing configuration file <%s>: tag <%s> not found", cfgFilename, tag);
	CloseNGene(cfg, EXIT_FAILURE);
    }
    strcpy(tag, "Galileo non coherent sums (code periods)");
    if (GetVal(cfgFilename, tag, (void *) &(acqCfg->galileoNumOfNonCohSums), F_INTEGER, 1) == FAILURE_RETURN) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Error while parsing configuration file <%s>: tag <%s> not found", cfgFilename, tag);
	CloseNGene(cfg, EXIT_FAILURE);
    }
    strcpy(tag, "GPS CN M - number of required successes");
    if (GetVal(cfgFilename, tag, (void *) &(acqCfg->gpsConfirmationM), F_INTEGER, 1) == FAILURE_RETURN) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Error while parsing configuration file <%s>: tag <%s> not found", cfgFilename, tag);
	CloseNGene(cfg, EXIT_FAILURE);
    }
    strcpy(tag, "GPS CN N - number of attempts");
    if (GetVal(cfgFilename, tag, (void *) &(acqCfg->gpsConfirmationN), F_INTEGER, 1) == FAILURE_RETURN) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Error while parsing configuration file <%s>: tag <%s> not found", cfgFilename, tag);
	CloseNGene(cfg, EXIT_FAILURE);
    }
    strcpy(tag, "Galileo CN M - number of required successes");
    if (GetVal(cfgFilename, tag, (void *) &(acqCfg->galileoConfirmationM), F_INTEGER, 1) == FAILURE_RETURN) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Error while parsing configuration file <%s>: tag <%s> not found", cfgFilename, tag);
	CloseNGene(cfg, EXIT_FAILURE);
    }
    strcpy(tag, "Galileo CN N - number of attempts");
    if (GetVal(cfgFilename, tag, (void *) &(acqCfg->galileoConfirmationN), F_INTEGER, 1) == FAILURE_RETURN) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Error while parsing configuration file <%s>: tag <%s> not found", cfgFilename, tag);
	CloseNGene(cfg, EXIT_FAILURE);
    }
    strcpy(tag, "EGNOS CN M - number of required successes");
    if (GetVal(cfgFilename, tag, (void *) &(acqCfg->egnosConfirmationM), F_INTEGER, 1) == FAILURE_RETURN) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Error while parsing configuration file <%s>: tag <%s> not found", cfgFilename, tag);
	CloseNGene(cfg, EXIT_FAILURE);
    }
    strcpy(tag, "EGNOS CN N - number of attempts");
    if (GetVal(cfgFilename, tag, (void *) &(acqCfg->egnosConfirmationN), F_INTEGER, 1) == FAILURE_RETURN) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Error while parsing configuration file <%s>: tag <%s> not found", cfgFilename, tag);
	CloseNGene(cfg, EXIT_FAILURE);
    }
    strcpy(tag, "R Delta for Doppler Frequency (Hz)");
    if (GetVal(cfgFilename, tag, (void *) &(acqCfg->dopplerDeltaRefinement), F_FLOAT, 1) == FAILURE_RETURN) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Error while parsing configuration file <%s>: tag <%s> not found", cfgFilename, tag);
	CloseNGene(cfg, EXIT_FAILURE);
    }
    strcpy(tag, "R Doppler Step (Hz)");
    if (GetVal(cfgFilename, tag, (void *) &(acqCfg->dopplerStepRefinement), F_FLOAT, 1) == FAILURE_RETURN) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Error while parsing configuration file <%s>: tag <%s> not found", cfgFilename, tag);
	CloseNGene(cfg, EXIT_FAILURE);
    }
    strcpy(tag, "R Delta for Code Delay (chip)");
    if (GetVal(cfgFilename, tag, (void *) &(acqCfg->codeDelayDeltaRefinement), F_FLOAT, 1) == FAILURE_RETURN) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Error while parsing configuration file <%s>: tag <%s> not found", cfgFilename, tag);
	CloseNGene(cfg, EXIT_FAILURE);
    }
    strcpy(tag, "R Code Delay Step (samples)");
    if (GetVal(cfgFilename, tag, (void *) &(acqCfg->codeDelayStepRefinement), F_INTEGER, 1) == FAILURE_RETURN) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Error while parsing configuration file <%s>: tag <%s> not found", cfgFilename, tag);
	CloseNGene(cfg, EXIT_FAILURE);
    }

    return SUCCESS_RETURN;
}

int CompleteAcqConfiguration(TNGeneConfiguration *cfg)
{
    TAcqConfiguration *acqCfg = &(cfg->acqConfiguration);
    int samplesPerCodePeriod;

    /* FFT size */
    /* GPS, EGNOS */
    acqCfg->gpsFftSize = ACQ_DEFAULT_FFT_SIZE;
    acqCfg->gpsLog2FftSize = ACQ_DEFAULT_LOG2_FFT_SIZE;
    samplesPerCodePeriod = (int) floor(GPS_CODE_DURATION * cfg->fs / SECONDS_TO_MILLISECONDS);
    while (samplesPerCodePeriod < acqCfg->gpsFftSize) {
	acqCfg->gpsFftSize /= 2;
	acqCfg->gpsLog2FftSize--;
    }
    /* Galileo */
    acqCfg->galileoFftSize = ACQ_DEFAULT_FFT_SIZE;
    acqCfg->galileoLog2FftSize = ACQ_DEFAULT_LOG2_FFT_SIZE;
    samplesPerCodePeriod = (int) floor(GALILEO_CODE_DURATION * cfg->fs / SECONDS_TO_MILLISECONDS);
    while (samplesPerCodePeriod < acqCfg->galileoFftSize) {
	acqCfg->galileoFftSize /= 2;
	acqCfg->galileoLog2FftSize--;
    }
    /* FFT/IFFT Gain */
    if (cfg->lowFsElaboration == TRUE_FLAG) {
	acqCfg->gpsLocalCodeFftGain = -6;
	acqCfg->galileoLocalCodeFftGain = -6;
	acqCfg->gpsFftGain = -3;
	acqCfg->galileoFftGain = -3;
	acqCfg->gpsIfftGain = -1;
	acqCfg->galileoIfftGain = -1;
    } else {
	acqCfg->gpsLocalCodeFftGain = -6;
	acqCfg->galileoLocalCodeFftGain = -6;
	acqCfg->gpsFftGain = -4;
	acqCfg->galileoFftGain = -6;
	acqCfg->gpsIfftGain = -1;
	acqCfg->galileoIfftGain = -1;
    }

    /* Check if code delay parameters are compliant with the current sampling frequency
     * (a code delay step must be at least one sample!).
     * If needed, correct the user configuration */

    /* GPS Coarse Acquisition */
    if ((acqCfg->gpsCodeDelayStep * cfg->samplesPerChip) < 1.0) {
	acqCfg->gpsCodeDelayStep = cfg->ts * GPS_CODE_RATE;
    }
    /* Galileo Coarse Acquisition */
    if ((acqCfg->galileoCodeDelayStep * cfg->samplesPerChip) < 1.0) {
	acqCfg->galileoCodeDelayStep = cfg->ts * GPS_CODE_RATE;
    }
    /* EGNOS Coarse Acquisition */
    if ((acqCfg->egnosCodeDelayStep * cfg->samplesPerChip) < 1.0) {
	acqCfg->egnosCodeDelayStep = cfg->ts * GPS_CODE_RATE;
    }
    /* Code Refinement */
    if ((acqCfg->codeDelayDeltaRefinement * cfg->samplesPerChip) < 1.0) {
	acqCfg->codeDelayDeltaRefinement = cfg->ts * GPS_CODE_RATE;
    }

    return SUCCESS_RETURN;
}
