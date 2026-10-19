/**
 * @file manageUsbFrontEndConfiguration.c
 *
 * @author Mario Nicola
 *
 * @date 08/08/2014
 *
 * @version 1.0.0
 *
 * @brief Functions that manage USB FE parameters.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include "usbFrontEndConfiguration.h"
#include "ngeneConstants.h"
#include "ioFunctions.h"
#include "closeManager.h"

/**
 * @def ENTRIES_PER_FRONTEND
 * @brief In the front-end configuration file, each supported front-end
 *        is described by ENTRIES_PER_FRONTEND parameters.
 */
#define ENTRIES_PER_FRONTEND   8

/* Global variable */
int USB_BLOCKSIZE; /**< Each data transfer from USB will read USB_BLOCKSIZE bytes */


int ManageUsbFeConfiguration(TNGeneConfiguration *cfg)
{
    TUsbFeConfiguration *feCfg;
    char cfgFilename[MAX_ROW_LENGTH];
    char tag[MAX_TAG_LEN];
    int num;
    char *tmp;
    int idx;
    TUsbFePars *pars;
    char usbPath[MAX_ROW_LENGTH];
    int byteIdx;
    char *charPtr;

    feCfg = &(cfg->usbFeParameters);
    cfgFilename[0] = '\0';
    strcat(cfgFilename, cfg->configurationPath);
    strcat(cfgFilename, "Front_End.conf");

    /* USB Blocksize */
    USB_BLOCKSIZE = USB_BLOCKSIZE_DEFAULT;	// init with the default value
    strcpy(tag, "USB Blocksize");
    if (GetVal(cfgFilename, tag, (void *) &USB_BLOCKSIZE, F_INTEGER, 1) == SUCCESS_RETURN) {
	// check it is a power of two
	num = USB_BLOCKSIZE;
	while (num > 2)
	    num /= 2;
	if (num < 2) {
	    SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Error while parsing configuration file <%s>", cfgFilename);
	    SwrxLog(NGENE_LOG_SEVERITY_ERROR, "<%s>: value <%d> is not a power of two", tag, USB_BLOCKSIZE);
	    CloseNGene(cfg, EXIT_FAILURE);
	} else {
	    SwrxLog(NGENE_LOG_SEVERITY_INFO, "USB Blocksize %d selected by user", USB_BLOCKSIZE);
	}
    } else {
	SwrxLog(NGENE_LOG_SEVERITY_INFO, "USB Blocksize %d", USB_BLOCKSIZE);
    }

    /* List of FEs */
    strcpy(tag, "Number of supported frontends");
    if (GetVal(cfgFilename, tag, (void *) &(feCfg->numberOfSupportedFes), F_INTEGER, 1) == FAILURE_RETURN) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Error while parsing configuration file <%s>: tag <%s> not found", cfgFilename, tag);
	CloseNGene(cfg, EXIT_FAILURE);
    }
    feCfg->feList = (TUsbFePars *) malloc(feCfg->numberOfSupportedFes * sizeof(TUsbFePars));
    if (feCfg->feList == NULL) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Malloc error in ManageUsbFeConfiguration");
	CloseNGene(cfg, EXIT_FAILURE);
    }
    tmp = (char *) malloc(feCfg->numberOfSupportedFes * MAX_ROW_LENGTH * sizeof(char));
    if (tmp == NULL) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Malloc error in ManageUsbFeConfiguration");
	CloseNGene(cfg, EXIT_FAILURE);
    }
    strcpy(tag, "Frontend configurations");
    if (GetVal(cfgFilename, tag, (void *) tmp, F_STRING, ENTRIES_PER_FRONTEND * feCfg->numberOfSupportedFes) == FAILURE_RETURN) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Error while parsing configuration file <%s>: tag <%s> not found", cfgFilename, tag);
	CloseNGene(cfg, EXIT_FAILURE);
    }
    /* Parse all the possible configurations */
    charPtr = tmp;
    for (idx = 0; idx < feCfg->numberOfSupportedFes; idx++) {
	pars = &(feCfg->feList[idx]);
	sscanf(charPtr, "%d %s %f %f %d %u %u %s %n", &(pars->index), pars->name, &(pars->samplingRate), &(pars->intermediateFrequency), &(pars->dopplerCorrection), &(pars->resolution), &(pars->meanValue), usbPath, &byteIdx);
	charPtr += byteIdx;
	if (pars->index != idx) {
	    SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Front End: problem while parsing the parameters of %s front-end (wrong file format?)", pars->name);
	    CloseNGene(cfg, EXIT_FAILURE);
	}
	if ((pars->dopplerCorrection != -1) && (pars->dopplerCorrection != 1)) {
	    if (pars->dopplerCorrection < 0)
		pars->dopplerCorrection = -1;
	    else
		pars->dopplerCorrection = 1;
	}
	if ((pars->resolution == 0) || (pars->resolution > 16)) {
	    SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Front End: invalid resolution (%u) found while parsing the parameters of %s front-end", pars->resolution, pars->name);
	    CloseNGene(cfg, EXIT_FAILURE);
	}
	if (pars->meanValue > UCHAR_MAX) {
	    SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Front End: invalid mean value (%u) found while parsing the parameters of %s front-end", pars->meanValue, pars->name);
	    CloseNGene(cfg, EXIT_FAILURE);
	}
	if (sscanf(usbPath, "%x/%x", &pars->vid, &pars->pid) != 2) {
	    SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Front End: can't parse the USB front end path. Check the consistency of the string");
	    CloseNGene(cfg, EXIT_FAILURE);
	}
    }
    free(tmp);
    return SUCCESS_RETURN;
}


void DeleteUsbFeConfiguration(TNGeneConfiguration *cfg)
{
    free(cfg->usbFeParameters.feList);
}
