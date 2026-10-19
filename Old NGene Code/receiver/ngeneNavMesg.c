/**
 * @file ngene2NavMesg.c
 *
 * @author Mario Nicola
 * @date 01/02/2021
 * @version 1.0.0
 * @brief Replaces NGene's main (ngene.c) when FAKE_OSNMA_RECEIVER is defined in ngeneConstants.h. In this case, the receiver is initialized, but does not process IF samples, only the navigation message saved using the IFEN convention. Used to test OSNMA without having IF samples, but only the output of Tu's code and the data shared by JRC during the OSNMA tests between 2020 and 2021.
 */

#include "ngeneConstants.h"

#if defined(FAKE_OSNMA_RECEIVER) && !defined(FAKE_CHIMERA_RECEIVER)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <math.h>
#include <ctype.h>

#include "frontendThreadFunctions.h"
#include "frontend.h"
#include "ngeneConfiguration.h"
#include "manageConfiguration.h"
#include "argvParser.h"
#include "code_protection.h"
#include "ioFunctions.h"
#include "guiInterfaceFunctions.h"
#include "ngeneInitialization.h"
#include "commModule.h"
#include "closeManager.h"
#include "signalHandler.h"
#include "circularBuffer.h"
#include "channelManager.h"
#include "acquisition.h"
#include "usbInterface.h"
#include "pvt.h"
#include "additionalLogs.h"
#include "ncursesApi.h"
#include "osnmaDataStructures.h"
#include "mackList.h"
#include "osnmaManagement.h"
#include "osnmaValidations.h"
#include "galileoMessageParser.h"
#include "tracking.h"
#include "ngene2NavMesgUtility.h"
#include "profilingSupport.h"

/* define global variables for the backup of xmm registers' values */
extern char *ngeneXmmsBackup;

static TNGeneConfiguration ngeneCfg;  /**< Contains all the receiver configuration! */
static TNGeneData ngene;	      /**< Contains all the receiver data! */
static TNGeneNavMesgPars navMesgPars; /**< Parameters specific for NGene2NavMesg */
static TNGeneNavMesgData navMesgINav; /**< Variables used to manage I/NAV from E1b */
static TNGeneNavMesgData navMesgFNav; /**< Variables used to manage F/NAV from E5a */
static TNGeneNavMesgData navMesgL1ca;  /**< Variables used to manage nav mesg from GPS L1 C/A */

/* GPS time is ahead 1024 weeks with respect to Galileo [ms] */
#define BIAS_GAL_GPS_TIME  1024*SECONDS_PER_WEEK*SECONDS_TO_MILLISECONDS

/* If the time difference between two navigation messages exceeds this limit,
 * then stop execution!!! [ms] */
#define MAXIMUM_TIME_DIFFERENCE_BETWEEN_SIGNALS  SECONDS_PER_WEEK*SECONDS_TO_MILLISECONDS

/* return the first time of the elaboration and the step, Galileo time
 * The first symbols must heve been read when this function is called! */
int GetStartTime(long long int *startTime, int *stepTime)
{
    long long int minStartTime;
    int minStepTime;
    int idx;
    long long int currTime;

    // check Galileo E1b
    idx = 0;
    while ((idx < navMesgINav.numPrns) && (navMesgINav.t[idx] == INVALID_TIME)) {
	idx++;
    }
    if (idx == navMesgINav.numPrns) {
	return FAILURE_RETURN;
    }
    minStartTime = navMesgINav.t[idx];
    minStepTime = navMesgINav.symbolDuration;
    // check GPS
    if (navMesgL1ca.navMesgFile != NULL) {
	idx = 0;
	while ((idx < navMesgL1ca.numPrns) && (navMesgL1ca.t[idx] == INVALID_TIME)) {
	    idx++;
	}
	if (idx < navMesgL1ca.numPrns) {
	    currTime = navMesgL1ca.t[idx] - BIAS_GAL_GPS_TIME;
	    // check
	    if (llabs(currTime - minStartTime) > MAXIMUM_TIME_DIFFERENCE_BETWEEN_SIGNALS) {
		return FAILURE_RETURN;
	    }
	    // find minimum
	    minStartTime = (currTime < minStartTime) ? currTime : minStartTime;
	}
	minStepTime = (navMesgL1ca.symbolDuration < minStepTime) ? navMesgL1ca.symbolDuration : minStepTime;
    }
    // check Galileo E5a
    if (navMesgFNav.navMesgFile != NULL) {
	idx = 0;
	while ((idx < navMesgFNav.numPrns) && (navMesgFNav.t[idx] == INVALID_TIME)) {
	    idx++;
	}
	if (idx < navMesgFNav.numPrns) {
	    currTime = navMesgFNav.t[idx];
	    // check
	    if (llabs(currTime - minStartTime) > MAXIMUM_TIME_DIFFERENCE_BETWEEN_SIGNALS) {
		return FAILURE_RETURN;
	    }
	    // find minimum
	    minStartTime = (currTime < minStartTime) ? currTime : minStartTime;
	}
	minStepTime = (navMesgFNav.symbolDuration < minStepTime) ? navMesgFNav.symbolDuration : minStepTime;
    }
    // output
    *startTime = minStartTime;
    *stepTime = minStepTime;
    return SUCCESS_RETURN;
}




int main(int argc, char *argv[])
{
    int run = TRUE_FLAG;
    long long int currentTimeGalileo;	// ms
    long long int currentTimeGps;	// ms
    int stepTime_ms;
    double stepTime_s;
    long long int nextPlotTime;
    int finished = FALSE_FLAG;

#ifdef OSNMA_PROFILING
    unsigned long long int loopCyclesCounter = 0;
    int numberOfMacsVerified = 0;
#endif

    /* prepare the XMM registers' backup */
    posix_memalign((void **) &(ngeneXmmsBackup), POSIX_ALIGNMENT, NUM_XMM_REGISTERS * POSIX_ALIGNMENT * sizeof(char));
    if (ngeneXmmsBackup == NULL) {
	fprintf(stderr, "Malloc returned a NULL pointer in main");
	exit(EXIT_FAILURE);
    }

    GetMainName(argv, &ngeneCfg);
    HandleCmdLine(argc, argv, &ngeneCfg);

    /* Preliminary initializations */
    PreparePaths(&ngeneCfg, &ngene);
    PreliminaryInitialization(&ngeneCfg, &ngene);

    /* Parse configuration files */
    ParseConfigurationFiles(&ngeneCfg);

    /* Some checks and adaptations */
    if (ngeneCfg.userInterface == RX_UI_GUI) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "NGene2 \"Navigation Message Receiver\" cannot work with GUI!!! Exit...");
	exit(EXIT_FAILURE);
    }
    if (ngeneCfg.inputMethod == USB_FRONTEND) {
	ngeneCfg.inputMethod = INPUT_FILE;
	SwrxLog(NGENE_LOG_SEVERITY_WARNING, "USB input is selected but NGene2 \"Navigation Message Receiver\" will work from file.");
    }

    CompleteConfiguration(&ngeneCfg);
    // NGene2NavMesg configuration
    if (ParseNGene2NavMesgParameters(&ngeneCfg, &navMesgPars) == FAILURE_RETURN)
	exit(EXIT_FAILURE);
    if (AdaptTheNumberOReceiverChannels(&ngeneCfg, &navMesgPars) == FAILURE_RETURN)
	exit(EXIT_FAILURE);
    //
    PlotConfiguration(&ngeneCfg);
    InitReceiver(&ngeneCfg, &ngene);

    // NGene2NavMesg initialization
    if (InitNGene2NavMesgData(&navMesgPars, GALILEO_SAT, E1_BC, &navMesgINav) == FAILURE_RETURN)
	CloseNGene(&ngeneCfg, EXIT_FAILURE);
    if (InitNGene2NavMesgData(&navMesgPars, GALILEO_SAT, E5_A, &navMesgFNav) == FAILURE_RETURN)
	CloseNGene(&ngeneCfg, EXIT_FAILURE);
    if (InitNGene2NavMesgData(&navMesgPars, GPS_SAT, L1_CA, &navMesgL1ca) == FAILURE_RETURN)
	CloseNGene(&ngeneCfg, EXIT_FAILURE);

    // Skip part of the input in case of delayed start selected through configuration file
    FastForwardNavMesgData(&navMesgPars, &navMesgINav);
    FastForwardNavMesgData(&navMesgPars, &navMesgFNav);
    FastForwardNavMesgData(&navMesgPars, &navMesgL1ca);

    // Load the first symbols
    finished = (ReadTheNextNavMesgRows(&ngeneCfg, &ngene, &navMesgPars, &navMesgINav, INVALID_TIME) == 0) ? TRUE_FLAG : FALSE_FLAG;
    ReadTheNextNavMesgRows(&ngeneCfg, &ngene, &navMesgPars, &navMesgFNav, INVALID_TIME);
    ReadTheNextNavMesgRows(&ngeneCfg, &ngene, &navMesgPars, &navMesgL1ca, INVALID_TIME);

    /* Manage the time */
    if (GetStartTime(&currentTimeGalileo, &stepTime_ms) == FAILURE_RETURN) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Problems in the timing of the input files. Exit...");
	run = FALSE_FLAG;
    }
    currentTimeGps = currentTimeGalileo + BIAS_GAL_GPS_TIME;
    stepTime_s = ((double) stepTime_ms) / SECONDS_TO_MILLISECONDS;
    // feedback to the user
    nextPlotTime = currentTimeGalileo + SECONDS_TO_MILLISECONDS;
    ngene.pvtResults.utcFromGal = TRUE_FLAG;
    ngene.pvtResults.rxGalTime.referenceWn = (int) (currentTimeGalileo / (SECONDS_TO_MILLISECONDS * SECONDS_PER_WEEK));
    ngene.pvtResults.rxGalTime.referenceTow = ((double) (currentTimeGalileo - (ngene.pvtResults.rxGalTime.referenceWn * SECONDS_TO_MILLISECONDS * SECONDS_PER_WEEK))) / SECONDS_TO_MILLISECONDS;
    ngene.pvtResults.rxGalTime.referenceUpdated = TRUE_FLAG;
    ngene.pvtResults.rxGalTime.referenceValid = TRUE_FLAG;


    // to correctly manage the closing of NGene2 with CTRL-C
    StartListeningSignals(&ngeneCfg, &run);


#ifdef OSNMA_PROFILING
    StartProfilingUpdate(&(ngene.osnmaData.mainLoop));
#endif

    /* Execution loop */
    while ((finished == FALSE_FLAG) && (run == TRUE_FLAG)) {
	/* Read a bunch of symbols (if needed) */
	if (AreSymbolsAvailable(&navMesgINav) == FALSE_FLAG) {
	    // if the symbols of I/NAV are terminated, the elaboration must finish
	    finished = (ReadTheNextNavMesgRows(&ngeneCfg, &ngene, &navMesgPars, &navMesgINav, currentTimeGalileo) == 0) ? TRUE_FLAG : FALSE_FLAG;
	}
	if ((navMesgPars.galE5aEnabled == TRUE_FLAG) && (AreSymbolsAvailable(&navMesgFNav) == FALSE_FLAG)) {
	    ReadTheNextNavMesgRows(&ngeneCfg, &ngene, &navMesgPars, &navMesgFNav, currentTimeGalileo);
	}
	if ((navMesgPars.gpsL1caEnabled == TRUE_FLAG) && (AreSymbolsAvailable(&navMesgL1ca) == FALSE_FLAG)) {
	    ReadTheNextNavMesgRows(&ngeneCfg, &ngene, &navMesgPars, &navMesgL1ca, currentTimeGps);
	}

	/* Elaborate a symbol for each received satellite */
	ElaborateNextSymbol(&ngeneCfg, &ngene, &navMesgINav, currentTimeGalileo);
	ElaborateNextSymbol(&ngeneCfg, &ngene, &navMesgFNav, currentTimeGalileo);
	ElaborateNextSymbol(&ngeneCfg, &ngene, &navMesgL1ca, currentTimeGps);

	/* Update the time */
	ngene.rxCounter += stepTime_ms;
	currentTimeGalileo += stepTime_ms;
	currentTimeGps += stepTime_ms;
	ngene.pvtResults.rxGalTime.referenceTow += stepTime_s;
	while (ngene.pvtResults.rxGalTime.referenceTow >= SECONDS_PER_WEEK) {
	    ngene.pvtResults.rxGalTime.referenceWn++;
	    ngene.pvtResults.rxGalTime.referenceTow -= SECONDS_PER_WEEK;
	}

	/* Manage ncurses (if enabled) */
#ifndef OSNMA_PROFILING
	if ((ngeneCfg.userInterface == RX_UI_NCURSES) && (currentTimeGalileo >= nextPlotTime)) {
	    /* ncurses log */
	    NcCompleteLog(&ngeneCfg, &ngene);
	    /* update the time plot */
	    nextPlotTime += SECONDS_TO_MILLISECONDS;
	    /* introduce a small delay to make interface visible */
	    nanosleep(&(navMesgPars.delayForNCurses), NULL);
	}
#endif

#ifdef OSNMA_PROFILING
	StartProfilingUpdate(&(ngene.osnmaData.allMacsProfiling));
	CheckOsnmaDsmTeslaMacseq(&ngeneCfg, &ngene);
	numberOfMacsVerified = MACVerificationAll(&ngeneCfg, &ngene);
	if (numberOfMacsVerified > 0) {
	    StopProfilingUpdate(&(ngene.osnmaData.allMacsProfiling), 1);
	    ManageOsnmaAuthentication(ngene.rxCounter, &(ngene.osnmaData), &(ngene.osnmaAuth));
	    SaveAuthenticationLog(ngene.pvtResults.rxGalTime.referenceWn, ngene.pvtResults.rxGalTime.referenceTow, &(ngene.osnmaData), &(ngene.osnmaAuth));
	} else {
	    StopProfilingUpdate(&(ngene.osnmaData.allMacsProfiling), 0);
	}
	ManageOsnmaTimeout(&ngeneCfg, &ngene);
#else
	CheckOsnmaDsmTeslaMacseq(&ngeneCfg, &ngene);
	if (MACVerificationAll(&ngeneCfg, &ngene) > 0) {
	    ManageOsnmaAuthentication(ngene.rxCounter, &(ngene.osnmaData), &(ngene.osnmaAuth));
	    SaveAuthenticationLog(ngene.pvtResults.rxGalTime.referenceWn, ngene.pvtResults.rxGalTime.referenceTow, &(ngene.osnmaData), &(ngene.osnmaAuth));
	}
	ManageOsnmaTimeout(&ngeneCfg, &ngene);
#endif

	ngene.pvtData.rxGalWn = ngene.pvtResults.rxGalTime.referenceWn;
	ngene.pvtData.rxGalTow = ngene.pvtResults.rxGalTime.referenceTow;
	SaveOsnmaSimpleLog(&ngeneCfg, &ngene);

	/* Check early termination */
	if ((navMesgPars.endGalTime > INVALID_VALUE) && (currentTimeGalileo >= navMesgPars.endGalTime)) {
	    SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO, "Early termination set from configuration file.");
	    run = FALSE_FLAG;
	}
#ifdef OSNMA_PROFILING
	loopCyclesCounter++;
#endif
    }

#ifdef OSNMA_PROFILING
    ngene.osnmaData.mainLoop.totalTimeS -= ngene.osnmaData.teslaProfiling.totalTimeS;
    StopProfilingUpdate(&(ngene.osnmaData.mainLoop), loopCyclesCounter);
    ngene.osnmaData.saveMackInMackListExcludingTESLA.totalTimeS = ngene.osnmaData.saveMackInMackList.totalTimeS - ngene.osnmaData.teslaProfiling.totalTimeS;
    ngene.osnmaData.saveMackInMackListExcludingTESLA.numberOfExecutions = ngene.osnmaData.saveMackInMackList.numberOfExecutions;
#endif

    /* Bye Bye */
    if (finished == TRUE_FLAG)
	SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO, "Input file finished. Exit...");
    else
	SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO, "Stopped by the user. Exit...");
    // NGene2NavMesg deinitialization
    DeinitNGene2NavMesgData(&ngeneCfg, &ngene, &navMesgINav);
    DeinitNGene2NavMesgData(&ngeneCfg, &ngene, &navMesgFNav);
    DeinitNGene2NavMesgData(&ngeneCfg, &ngene, &navMesgL1ca);
    //
    DeinitReceiver(&ngeneCfg, &ngene);
    DeleteConfiguration(&ngeneCfg);
    free(ngeneXmmsBackup);
    return 0;
}
#endif
