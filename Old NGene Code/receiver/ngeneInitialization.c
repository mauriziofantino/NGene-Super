/**
 * @file ngeneInitialization.h
 *
 * @author Mario Nicola
 *
 * @date 12/08/2014
 *
 * @version 1.0.0
 *
 * @brief Functions used to initialize/de-initialize the receiver.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "ngeneConfiguration.h"
#include "ngeneDataStructure.h"
#include "ngeneConstants.h"
#include "circularBuffer.h"
#include "prnTable.h"
#include "carrierTable.h"
#include "spreadingCodes.h"
#include "fft.h"
#include "ioFunctions.h"
#include "closeManager.h"
#include "arrays.h"
#include "acquisition.h"
#include "channelDataStructure.h"
#include "channelManager.h"
#include "navigationMessage.h"
#include "warmStart.h"
#include "egnosMessageParser.h"
#include "egnosCorrections.h"
#include "pvt.h"
#include "ncursesApi.h"
#include "additionalLogs.h"
#include "viterbi.h"
#include "reedSolomon.h"
#include "tracking.h"
#include "rinexLog.h"
#include "prnList.h"
#include "osnmaManagement.h"
#include "megaBuffer.h"
#include "checkFolder.h"
#include "osnmaValidations.h"
#include "manageLogs.h"

void PreparePaths(TNGeneConfiguration *cfg, TNGeneData *rx)
{
    // info extracted from the environment variables
    char *sudoUser;
    int sudoUserValid = TRUE_FLAG;
    char *user;
    int userValid = TRUE_FLAG;
    char *homeFolder;
    int homeFolderValid = TRUE_FLAG;
    // current folder folder from getcwd
    char currFolder[MAX_ROW_LENGTH];
    int currFolderValid = TRUE_FLAG;
    // to manage the folder for the logs
    char cfgFilename[MAX_ROW_LENGTH];
    char tag[MAX_TAG_LEN];
    int parseReturn;
    // filename for the log
    char logFilename[MAX_ROW_LENGTH];

    // init all the useful variables
    sudoUser = getenv("SUDO_USER");
    if (sudoUser == NULL) {
	sudoUserValid = FALSE_FLAG;
	cfg->sudoer = FALSE_FLAG;
    } else {
	cfg->sudoer = TRUE_FLAG;
    }
    user = getenv("USER");
    if (user == NULL) {
	userValid = FALSE_FLAG;
    }
    homeFolder = getenv("HOME");
    if (homeFolder == NULL) {
	homeFolder = FALSE_FLAG;
    }
    if (getcwd(currFolder, MAX_ROW_LENGTH) == NULL) {
	currFolderValid = FALSE_FLAG;
    }
    // feedback for the folders
    if (cfg->verbose) {
	fprintf(stdout, "\n");
	if (sudoUserValid)
	    fprintf(stdout, "sudo user:   <%s>     len: %lu\n", sudoUser, (long unsigned int) strlen(sudoUser));
	else
	    fprintf(stdout, "sudo user:   N.A.\n");
	//
	if (userValid)
	    fprintf(stdout, "user:        <%s>     len: %lu\n", user, (long unsigned int) strlen(user));
	else
	    fprintf(stdout, "user:        N.A.\n");
	//
	if (homeFolderValid)
	    fprintf(stdout, "home folder: <%s>     len: %lu\n", homeFolder, (long unsigned int) strlen(homeFolder));
	else
	    fprintf(stdout, "home folder: N.A.\n");
	//
	if (currFolderValid)
	    fprintf(stdout, "folder:      <%s>     len: %lu\n", currFolder, (long unsigned int) strlen(currFolder));
	else
	    fprintf(stdout, "folder:      N.A.\n");
	fprintf(stdout, "\n");
    }

    /* initialize the folders in the configuration structures */
    if (sudoUserValid == TRUE_FLAG) {
	sprintf(cfg->homePath, "/home/%s", sudoUser);
	sprintf(cfg->configurationPath, "/home/%s/%s/configuration/", sudoUser, GNSS_RX_CFGFOLDER);
	sprintf(cfg->otherDataPath, "/home/%s/%s/otherData/", sudoUser, GNSS_RX_CFGFOLDER);
    } else if (homeFolderValid == TRUE_FLAG) {
	sprintf(cfg->homePath, "%s", homeFolder);
	sprintf(cfg->configurationPath, "%s/%s/configuration/", homeFolder, GNSS_RX_CFGFOLDER);
	sprintf(cfg->otherDataPath, "%s/%s/otherData/", homeFolder, GNSS_RX_CFGFOLDER);
    } else if (userValid == TRUE_FLAG) {
	sprintf(cfg->homePath, "/home/%s", user);
	sprintf(cfg->configurationPath, "/home/%s/%s/configuration/", user, GNSS_RX_CFGFOLDER);
	sprintf(cfg->otherDataPath, "/home/%s/%s/otherData/", user, GNSS_RX_CFGFOLDER);
    } else {
	// Error: feedback and termination
	if (sudoUserValid)
	    fprintf(stdout, "sudo user:   <%s>     len: %lu\n", sudoUser, (long unsigned int) strlen(sudoUser));
	else
	    fprintf(stdout, "sudo user:   N.A.\n");
	//
	if (userValid)
	    fprintf(stdout, "user:        <%s>     len: %lu\n", user, (long unsigned int) strlen(user));
	else
	    fprintf(stdout, "user:        N.A.\n");
	//
	if (homeFolderValid)
	    fprintf(stdout, "home folder: <%s>     len: %lu\n", homeFolder, (long unsigned int) strlen(homeFolder));
	else
	    fprintf(stdout, "home folder: N.A.\n");
	//
	if (currFolderValid)
	    fprintf(stdout, "folder:      <%s>     len: %lu\n", currFolder, (long unsigned int) strlen(currFolder));
	else
	    fprintf(stdout, "folder:      N.A.\n");
	fprintf(stdout, "\n");
	fprintf(stderr, "Cannot find the name of the folder containing the configuration files. Exit...\n");
	exit(EXIT_FAILURE);
    }

    /* check if the configuration folder does exist (only when -s is not specified) */
    if ((IsFolderExistent(cfg->configurationPath) == FALSE_FLAG) || (IsFolderExistent(cfg->otherDataPath) == FALSE_FLAG)){
	fprintf(stderr, "The folder %s/.ngene does not exist or is incomplete\n", cfg->homePath);
	fprintf(stderr, "reinstall ngene or manually fix the proper configuration folder\n");
	exit(EXIT_FAILURE);
    }

    /* get the folder for the logs */
    // chosen by the user
    cfgFilename[0] = '\0';
    strcat(cfgFilename, cfg->configurationPath);
    strcat(cfgFilename, "Logs.conf");
    strcpy(tag, "Dump Path");
    parseReturn = GetVal(cfgFilename, tag, (void *) &(cfg->dumpPath), F_STRING, 1);
    if ((parseReturn == SUCCESS_RETURN) && (IsFolderExistent(cfg->dumpPath) == FALSE_FLAG)) {
	parseReturn = FAILURE_RETURN;
    }
    // if needed, use the current folder
    if ((parseReturn == FAILURE_RETURN) && (currFolderValid == TRUE_FLAG)) {
	strcpy(cfg->dumpPath, currFolder);
    } else if (parseReturn == FAILURE_RETURN) {
	strcpy(cfg->dumpPath, ".");
    }
    // else dumpPath already contains the folder!
    // set the current dumpPath
    sprintf(cfg->dumpPathCurrent, "%s", cfg->dumpPath);

    /* Init log splitter */
    InitLogSplitter(cfg, rx);

    /* prepare log */
    cfg->msgToGui = rx->msgToGui;
    cfg->ncursesData = NULL;
    cfg->ncursesDataInitialized = FALSE_FLAG;
    // Open the log file in the folder specified by cfg->dumpPath!
    // (if needed, i.e. if cfg->logFilename is not an empty string)
    if (strlen(cfg->logFilename) > 0) {
	logFilename[0] = '\0';
	strcat(logFilename, cfg->dumpPathCurrent);
	strcat(logFilename, "/");
	strcat(logFilename, cfg->logFilename);
	cfg->stdlog = fopen(logFilename, "a+");
	if (cfg->stdlog == NULL) {
	    cfg->stdlog = stdout;
	    fprintf(stderr, "Parsing: can't create log file, printing on the console\n");
	}
    }
    // The completion of the GoogleEarth filename is managed in InitPvtData
}


int PreliminaryInitialization(TNGeneConfiguration *cfg, TNGeneData *rx)
{
    /* Memory devoted to GUI-receiver communication */
    if (cfg->userInterface != RX_UI_GUI) {
	rx->bytesFromGui = NULL;
	rx->bytesToGuiSize = 0;
	rx->bytesToGui = NULL;
    }
    /* log messages */
    memset(rx->msgToGui, '\0', MAX_ROW_LENGTH);
    memset(rx->msgToGuiOld, '\0', MAX_ROW_LENGTH);

    /* NCurses must be initialized after the number of channels is known!!! */

    /* prepare the management of generic text logs through the SwrxLog function */
    InitSwrxLog(cfg, rx);
    return SUCCESS_RETURN;
}


int InitReceiver(TNGeneConfiguration *cfg, TNGeneData *rx)
{
    int errorsFound = 0;
    unsigned int arraySize = NGENE_ARRAYS_SIZE * cfg->samplesPerMillisecond;
    int index;
    TChannel *ch;
    char filename[MAX_ROW_LENGTH];

    /* NCurses */
    // NCurses must be initialized here because the number of channel is known (now)
    if (cfg->userInterface == RX_UI_NCURSES) {
	InitNCurses(cfg, rx, TRUE_FLAG);
	cfg->ncursesDataInitialized = TRUE_FLAG;
	/* update the management of the generic text logs through the SwrxLog function */
	InitSwrxLog(cfg, rx);
    }

    /**/ rx->rxCounter = 0;
    posix_memalign((void **) &(rx->codeArray), POSIX_ALIGNMENT, arraySize * sizeof(char));
    if (rx->codeArray == NULL)
	errorsFound++;
    if (cfg->lowFsElaboration == TRUE_FLAG) {
	posix_memalign((void **) &(rx->codeEarlyArray), POSIX_ALIGNMENT, arraySize * sizeof(char));
	if (rx->codeEarlyArray == NULL)
	    errorsFound++;
	posix_memalign((void **) &(rx->codeLateArray), POSIX_ALIGNMENT, arraySize * sizeof(char));
	if (rx->codeLateArray == NULL)
	    errorsFound++;
    } else {
	rx->codeEarlyArray = NULL;
	rx->codeLateArray = NULL;
    }
    posix_memalign((void **) &(rx->cosArray), POSIX_ALIGNMENT, arraySize * sizeof(char));
    if (rx->cosArray == NULL)
	errorsFound++;
    posix_memalign((void **) &(rx->sinArray), POSIX_ALIGNMENT, arraySize * sizeof(char));
    if (rx->sinArray == NULL)
	errorsFound++;
    posix_memalign((void **) &(rx->subcarrierArray), POSIX_ALIGNMENT, arraySize * sizeof(char));
    if (rx->subcarrierArray == NULL)
	errorsFound++;
    posix_memalign((void **) &(rx->subcarrierArrayEarly), POSIX_ALIGNMENT, arraySize * sizeof(char));
    if (rx->subcarrierArrayEarly == NULL)
	errorsFound++;
    posix_memalign((void **) &(rx->subcarrierArrayLate), POSIX_ALIGNMENT, arraySize * sizeof(char));
    if (rx->subcarrierArrayLate == NULL)
	errorsFound++;

    if (errorsFound > 0) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Malloc returned a NULL pointer in InitReceiver");
	CloseNGene(cfg, EXIT_FAILURE);
    }
    ArrayClean(rx->codeArray, arraySize * sizeof(char));
    if (cfg->lowFsElaboration == TRUE_FLAG) {
	ArrayClean(rx->codeEarlyArray, arraySize * sizeof(char));
	ArrayClean(rx->codeLateArray, arraySize * sizeof(char));
    }
    ArrayClean(rx->cosArray, arraySize * sizeof(char));
    ArrayClean(rx->sinArray, arraySize * sizeof(char));
    ArrayClean(rx->subcarrierArray, arraySize * sizeof(char));
    ArrayClean(rx->subcarrierArrayEarly, arraySize * sizeof(char));
    ArrayClean(rx->subcarrierArrayLate, arraySize * sizeof(char));
    /* other initializations */
    if (InitCircularBuffer(cfg, &(rx->buf)) != SUCCESS_RETURN)
	return FAILURE_RETURN;
    if (InitPrnTable(cfg, &(rx->prnTable)) != SUCCESS_RETURN)
	return FAILURE_RETURN;
    if (InitCarrierTable(cfg, &(rx->fifTable), (double) cfg->fif) != SUCCESS_RETURN)
	return FAILURE_RETURN;
    if (InitCarrierTable(cfg, &(rx->fscTable), (double) GALILEO_CODE_RATE) != SUCCESS_RETURN)
	return FAILURE_RETURN;
    if (InitFftTable(cfg, &(rx->fftTable)) != SUCCESS_RETURN)
	return FAILURE_RETURN;
    if (InitChannelScheduler(cfg, &(rx->channelScheduler), rx->prnTable.numberOfRealGalileoSat, rx->prnTable.prnOfRealGalileoSat) != SUCCESS_RETURN)
	return FAILURE_RETURN;
    if (InitAcquisitionCommon(cfg, rx) != SUCCESS_RETURN)
	return FAILURE_RETURN;
    InitViterbi();
    InitReedSolomon();
    if (InitNavigationMessage(cfg, rx) != SUCCESS_RETURN)
	return FAILURE_RETURN;
    InitWarmStartData(cfg, rx);
    if (InitEgnosCorrections(cfg, &(rx->egnosCorrections)) != SUCCESS_RETURN)
	return FAILURE_RETURN;
    InitOsnmaData(cfg, &(rx->osnmaData));
    InitOsnmaAuthenticationStruct(cfg, &(rx->osnmaAuth));
    if (InitPvtData(cfg, rx) != SUCCESS_RETURN)
	return FAILURE_RETURN;
    /* channels */
    rx->channelArray = (TChannel *) malloc(cfg->numberOfChannels * sizeof(TChannel));
    if (rx->channelArray == NULL) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Malloc returned a NULL pointer in InitReceiver");
	CloseNGene(cfg, EXIT_FAILURE);
    }
    for (index = 0; index < cfg->numberOfChannels; index++) {
	ch = &(rx->channelArray[index]);
	ch->chIndex = index;
	if (cfg->automaticConfigurationFlag == FALSE_FLAG) {
	    ch->satType = cfg->manuallyConfiguredChannels[index].satType;
	    if (ch->satType == GPS_SAT)
		ch->satSignal = L1_CA;
	    else if (ch->satType == GALILEO_SAT)
		ch->satSignal = E1_BC;
	    else		// if(ch->satType==EGNOS_SAT)
		ch->satSignal = L1;
	    ch->satId = INVALID_PRN;
	} else {
	    if (index < cfg->numberOfGpsChannels) {
		ch->satType = GPS_SAT;
		ch->satSignal = L1_CA;
	    } else if (index < (cfg->numberOfGpsChannels + cfg->numberOfGalileoChannels)) {
		ch->satType = GALILEO_SAT;
		ch->satSignal = E1_BC;
	    } else if (index < (cfg->numberOfGpsChannels + cfg->numberOfGalileoChannels + cfg->numberOfGalileoE5aChannels)) {
		ch->satType = GALILEO_SAT;
		ch->satSignal = E5_A;
	    } else {
		ch->satType = EGNOS_SAT;
		ch->satSignal = L1;
	    }
	    ch->satId = INVALID_PRN;
	}
	FromPrnToString(ch->satType, ch->satId, ch->prnName);
	InitChannel(cfg, rx, ch);
    }
	/*** logs ****/
    /* ephemerides for the GUI */
    if (cfg->userInterface == RX_UI_GUI) {
	rx->ephemeridesPacketToGui = (char *) malloc(GUI_EPHEMERIDES_PACKET_SIZE * sizeof(char));
    } else {
	rx->ephemeridesPacketToGui = NULL;
    }
    if (cfg->logConfiguration.enableTrkLog == TRUE_FLAG) {
	filename[0] = '\0';
	strcat(filename, cfg->dumpPathCurrent);
	strcat(filename, "/trackingCorrelators.bin");
	rx->trkCorrelatorsFile = fopen(filename, "wb");
	if (rx->trkCorrelatorsFile == NULL) {
	    SwrxLog(NGENE_LOG_SEVERITY_NOTE, "The file trackingCorrelators.bin has not been correctly created!");
	}
    } else {
	rx->trkCorrelatorsFile = NULL;
    }
    /* tracking multiple correlations file */
    if ((cfg->logConfiguration.enableTrkLog == TRUE_FLAG) && (cfg->trkConfiguration.multipleCorrelatorsFlag == TRUE_FLAG)) {
	filename[0] = '\0';
	strcat(filename, cfg->dumpPathCurrent);
	strcat(filename, "/trackingMultiCorrelators.bin");
	rx->trkMultiCorrelatorsFile = fopen(filename, "wb");
	if (rx->trkMultiCorrelatorsFile == NULL) {
	    SwrxLog(NGENE_LOG_SEVERITY_NOTE, "The file trackingMultiCorrelators.bin has not been correctly created!");
	} else {
	    fwrite(&(cfg->trkConfiguration.numberOfMultipleCorrelators), sizeof(int), 1, rx->trkMultiCorrelatorsFile);
	    fwrite(&(cfg->trkConfiguration.multipleCorrelatorsGpsSpacing), sizeof(int), 1, rx->trkMultiCorrelatorsFile);
	    fwrite(&(cfg->trkConfiguration.multipleCorrelatorsGalileoSpacing), sizeof(int), 1, rx->trkMultiCorrelatorsFile);
	    fwrite(&(cfg->trkConfiguration.multipleCorrelatorsEgnosSpacing), sizeof(int), 1, rx->trkMultiCorrelatorsFile);
	}
    } else {
	rx->trkMultiCorrelatorsFile = NULL;
    }
    if ((TRACKING_LOG_SET_SIZE * sizeof(double)) > (((2 * cfg->trkConfiguration.numberOfMultipleCorrelators) + 4) * sizeof(float))) {
	rx->trkLogArray = (char *) malloc(TRACKING_LOG_SET_SIZE * sizeof(double));
    } else {
	rx->trkLogArray = (char *) malloc(((2 * cfg->trkConfiguration.numberOfMultipleCorrelators) + 4) * sizeof(float));
    }
    /* histogram and spectrum for the GUI */
    if (cfg->userInterface == RX_UI_GUI) {
	InitHistogram(&(rx->samplesHistogram), cfg->guiParameters.histBits, cfg->guiParameters.histSamples);
	InitSamplesSpectrum(cfg, &(rx->samplesSpectrum));
    }
    /* Navigation message RINEX files */
    InitRinexLog(&(rx->rinexGps));
    InitRinexLog(&(rx->rinexGal));
    /* NMEA */
    rx->nmeaFile = NULL;
    /* Navigation messages in IFEN format */
    if ((cfg->logConfiguration.enableNavMesgIfenLog == TRUE_FLAG) && (cfg->numberOfGpsChannels > 0)) {
	filename[0] = '\0';
	strcat(filename, cfg->dumpPathCurrent);
	strcat(filename, "/navigationMessage_GPS.txt");
	rx->ifenLogGps = fopen(filename, "w");
	if (rx->ifenLogGps != NULL) {
	    fprintf(rx->ifenLogGps, "# FILE_VERSION = 1\n");
	    fprintf(rx->ifenLogGps, "# NAV_TYPE = GPS_L1_NAV\n");
	    fprintf(rx->ifenLogGps, "# BIT_TYPE = MESSAGE_BITS\n");
	    fprintf(rx->ifenLogGps, "# FRAME_LENGTH_BITS = 300\n");
	    fprintf(rx->ifenLogGps, "# FRAME_LENGTH_TIME = 6\n");
	    fprintf(rx->ifenLogGps, "# BIT_ORDER = MSB_FIRST\n");
	}
    } else {
	rx->ifenLogGps = NULL;
    }
    if ((cfg->logConfiguration.enableNavMesgIfenLog == TRUE_FLAG) && (cfg->numberOfGalileoChannels > 0)) {
	filename[0] = '\0';
	strcat(filename, cfg->dumpPathCurrent);
	strcat(filename, "/navigationMessage_GAL.txt");
	rx->ifenLogGal = fopen(filename, "w");
	if (rx->ifenLogGal != NULL) {
	    fprintf(rx->ifenLogGal, "# FILE_VERSION = 1\n");
	    fprintf(rx->ifenLogGal, "# NAV_TYPE = GAL_E1_INAV\n");
	    fprintf(rx->ifenLogGal, "# BIT_TYPE = MESSAGE_SYMBOLS\n");
	    fprintf(rx->ifenLogGal, "# FRAME_LENGTH_BITS = 250\n");
	    fprintf(rx->ifenLogGal, "# FRAME_LENGTH_TIME = 1\n");
	    fprintf(rx->ifenLogGal, "# BIT_ORDER = MSB_FIRST\n");
	}
    } else {
	rx->ifenLogGal = NULL;
    }
    if ((cfg->logConfiguration.enableNavMesgIfenLog == TRUE_FLAG) && (cfg->numberOfGalileoE5aChannels > 0)) {
	filename[0] = '\0';
	strcat(filename, cfg->dumpPathCurrent);
	strcat(filename, "/navigationMessage_FNAV.txt");
	rx->ifenLogFNav = fopen(filename, "w");
	if (rx->ifenLogFNav != NULL) {
	    fprintf(rx->ifenLogFNav, "# FILE_VERSION = 1\n");
	    fprintf(rx->ifenLogFNav, "# NAV_TYPE = GAL_E5_FNAV\n");
	    fprintf(rx->ifenLogFNav, "# BIT_TYPE = MESSAGE_SYMBOLS\n");
	    fprintf(rx->ifenLogFNav, "# FRAME_LENGTH_BITS = 500\n");
	    fprintf(rx->ifenLogFNav, "# FRAME_LENGTH_TIME = 10\n");
	    fprintf(rx->ifenLogFNav, "# BIT_ORDER = MSB_FIRST\n");
	}
    } else {
	rx->ifenLogFNav = NULL;
    }
    rx->ifenLogGpsTime = -1;	// not valid
    rx->ifenLogGalTime = -1;	// not valid
    rx->ifenLogFNavTime = -1;	// not valid

    /* finish */
    rx->dataValid = TRUE_FLAG;
#ifdef RX_PROFILING
    InitProfilingSupport(&(rx->pvtProfiling), "PVT");
    rx->pvtTime = FALSE_FLAG;
#endif

    SwrxLog(NGENE_LOG_SEVERITY_NOTE, "Receiver initialized!");
    return SUCCESS_RETURN;
}


void DeinitReceiver(TNGeneConfiguration *cfg, TNGeneData *rx)
{
    int index;

    if (rx->dataValid == TRUE_FLAG) {
#ifdef RX_PROFILING
	ReportProfilingData(&(rx->pvtProfiling));
#endif
	free(rx->codeArray);
	if (cfg->lowFsElaboration == TRUE_FLAG) {
	    free(rx->codeEarlyArray);
	    free(rx->codeLateArray);
	}
	free(rx->cosArray);
	free(rx->sinArray);
	free(rx->subcarrierArray);
	free(rx->subcarrierArrayEarly);
	free(rx->subcarrierArrayLate);
	DeinitCircularBuffer(&(rx->buf));
	DeinitPrnTable(&(rx->prnTable));
	DeinitCarrierTable(&(rx->fifTable));
	DeinitCarrierTable(&(rx->fscTable));
	DeinitFftTable(&(rx->fftTable));
	DeinitAcquisitionCommon(&(rx->acqCommon));
	DeinitChannelScheduler(&(rx->channelScheduler));
	DeinitReedSolomon();
	DeinitNavigationMessage(cfg, rx);
	DeinitWarmStartData(&(rx->warmStartData));
	DeinitEgnosCorrections(&(rx->egnosCorrections));
	DeinitOsnmaData(&(rx->osnmaData));
	DeinitOsnmaAuthenticationStruct(&(rx->osnmaAuth));
	DeinitPvtData(rx);
	for (index = 0; index < cfg->numberOfChannels; index++) {
	    DenitChannel(&(rx->channelArray[index]));
	}
	free(rx->channelArray);
	if (rx->ephemeridesPacketToGui != NULL) {
	    free(rx->ephemeridesPacketToGui);
	}
	if (rx->trkCorrelatorsFile != NULL) {
	    fclose(rx->trkCorrelatorsFile);
	}
	if (rx->trkMultiCorrelatorsFile != NULL) {
	    fclose(rx->trkMultiCorrelatorsFile);
	}
	free(rx->trkLogArray);
	DeinitRinexLog(&(rx->rinexGps));
	DeinitRinexLog(&(rx->rinexGal));
	if (rx->nmeaFile != NULL) {
	    fclose(rx->nmeaFile);
	    rx->nmeaFile = NULL;
	}
	if (rx->ifenLogGps != NULL) {
	    fclose(rx->ifenLogGps);
	    rx->ifenLogGps = NULL;
	}
	if (rx->ifenLogGal != NULL) {
	    fclose(rx->ifenLogGal);
	    rx->ifenLogGal = NULL;
	}
	if (rx->ifenLogFNav != NULL) {
	    fclose(rx->ifenLogFNav);
	    rx->ifenLogFNav = NULL;
	}
	if (cfg->userInterface == RX_UI_GUI) {
	    DeinitHistogram(&(rx->samplesHistogram));
	    DeinitSamplesSpectrum(&(rx->samplesSpectrum));
	}
	rx->dataValid = FALSE_FLAG;
	SwrxLog(NGENE_LOG_SEVERITY_NOTE, "Receiver deinitialized!");
	/* NCurses */
	if (cfg->userInterface == RX_UI_NCURSES) {
	    DeinitNCurses(&(rx->ncursesData));
	}
    }
}
