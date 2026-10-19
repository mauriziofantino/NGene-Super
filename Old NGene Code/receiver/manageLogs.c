/**
 * @file manageLogs.h
 *
 * @author Mario Nicola
 *
 * @date 12/11/2020
 *
 * @version 1.0.0
 *
 * @brief Functions used to manage the receiver logs and their splitting.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "manageLogs.h"
#include "ioFunctions.h"
#include "timeConstants.h"
#include "checkFolder.h"
#include "timeConversions.h"


/**
 * @def SPLIT_PERIOD_IN_MINUTES
 * @brief The split period chosen by the user is in hours
 *        for debug purposes it can be forced in minutes,
 *        declare this define to enable this debug behavior
 */
//#define SPLIT_PERIOD_IN_MINUTES



void InitLogSplitter(TNGeneConfiguration *cfg,TNGeneData *rx)
{
	char cfgFilename[MAX_ROW_LENGTH];
	char tag[MAX_TAG_LEN];
	int parseReturn;
	TLogSplitter *logSplitter = &(rx->logSplitter);

	/* split log parameters */
	cfgFilename[0] = '\0';
	strcat(cfgFilename, cfg->configurationPath);
	strcat(cfgFilename, "Logs.conf");
	//
	strcpy(tag, "Enable split log");
	if(GetVal(cfgFilename,tag,(void *)&(cfg->splitLogsEnabled),F_INTEGER,1)==FAILURE_RETURN)
	{
		cfg->splitLogsEnabled = FALSE_FLAG;
	}
	cfg->splitLogsEnabled = (cfg->splitLogsEnabled>0) ? TRUE_FLAG : FALSE_FLAG;
	//
	strcpy(tag, "Split log period");
	parseReturn = GetVal(cfgFilename,tag,(void *)&(cfg->splitLogsPeriod),F_INTEGER,1);
	if((parseReturn==FAILURE_RETURN) || (cfg->splitLogsPeriod<=0))
	{
		cfg->splitLogsEnabled = FALSE_FLAG;
		cfg->splitLogsPeriod = 0;
	}
	//
	strcpy(tag, "Split log aligned");
	if(GetVal(cfgFilename,tag,(void *)&(cfg->splitLogsAligned),F_INTEGER,1)==FAILURE_RETURN)
	{
		cfg->splitLogsAligned = FALSE_FLAG;
	}
	cfg->splitLogsAligned = (cfg->splitLogsAligned>0) ? TRUE_FLAG : FALSE_FLAG;

	/* Init the log splitter structure */
	#ifdef SPLIT_PERIOD_IN_MINUTES
	logSplitter->splitPeriod = cfg->splitLogsPeriod * SECONDS_IN_A_MINUTE;
	#else
	logSplitter->splitPeriod = cfg->splitLogsPeriod * SECONDS_IN_A_HOUR;
	#endif
	logSplitter->splitTimeValid = FALSE_FLAG;
	logSplitter->splitTimeConst = INVALID_SAT;
	logSplitter->splitWn = INVALID_VALUE;
	logSplitter->splitTow = INVALID_VALUE;
}


/*******
 * TODO IMPORTANTE!!! FARE CHECK DELLA GESTIONE LOG IN CASO DI MULTI_THREAD!!!
 */

void ManageLogSplitter(TNGeneConfiguration *cfg,TNGeneData *rx)
{
	TLogSplitter *logSplitter = &(rx->logSplitter);
	int currWn;
	double currTow;
	TDateTime currDate;
	char command[2*MAX_ROW_LENGTH];
	char filename[MAX_ROW_LENGTH];
	char path[2*MAX_ROW_LENGTH];
	int chIdx;
	TChannel *ch;

	/* if log splitting is disabled, return */
	if(cfg->splitLogsEnabled==FALSE_FLAG)
	{
		return;
	}

	/* manage the time for the next split */
	if(logSplitter->splitTimeValid==FALSE_FLAG)
	{
		// get the receiver time
		if(rx->pvtData.rxGalTimeValid==TRUE_FLAG)
		{
			logSplitter->splitTimeConst = GALILEO_SAT;
			currWn =  rx->pvtData.rxGalWn;
			currTow	= rx->pvtData.rxGalTow;
		}
		else if(rx->pvtData.rxGpsTimeValid==TRUE_FLAG)
		{
			logSplitter->splitTimeConst = GPS_SAT;
			currWn =  rx->pvtData.rxGpsWn;
			currTow	= rx->pvtData.rxGpsTow;
		}
		else
		{
			// no time available
			return;
		}
		// init the time for the next split
		logSplitter->splitWn = currWn;
		if(cfg->splitLogsAligned==FALSE_FLAG)
		{
			logSplitter->splitTow = (int)round(currTow) + logSplitter->splitPeriod;
		}
		else
		{
			logSplitter->splitTow = (floor(currTow/(double)logSplitter->splitPeriod)+1) * logSplitter->splitPeriod;
		}
		// manage the change of the week
		if(logSplitter->splitTow >= SECONDS_IN_A_WEEK)
		{
			logSplitter->splitTow -= SECONDS_IN_A_WEEK;
			logSplitter->splitWn++;
		}
		logSplitter->splitTimeValid = TRUE_FLAG;
		// for now it is enough
		return;
	}

	/* get the last estimated receiver time */
	if(logSplitter->splitTimeConst==GALILEO_SAT)
	{
		currWn =  rx->pvtData.rxGalWn;
		currTow	= rx->pvtData.rxGalTow;
	}
	else if(logSplitter->splitTimeConst==GPS_SAT)
	{
		currWn =  rx->pvtData.rxGpsWn;
		currTow	= rx->pvtData.rxGpsTow;
	}
	else
	{
		// cannot continue
		return;
	}

	/* check if the split is required */
	if((currWn<logSplitter->splitWn) || ((currWn==logSplitter->splitWn) && ((int)floor(currTow)<logSplitter->splitTow)))
	{
		// do not split now, nothing to do!
		return;
	}

	/* split the logs */
	// get the name of the new folder
	FromGnssTimeToDate(logSplitter->splitTimeConst, logSplitter->splitWn, (double)logSplitter->splitTow, &currDate);
	currDate.second = round(currDate.second);
	sprintf(cfg->dumpPathCurrent,"%s/NGene2Log_%04d%02d%02d_%02d%02d%02d",
			cfg->dumpPath,currDate.year,currDate.month,currDate.day,currDate.hour,currDate.minute,(int)currDate.second);
	// create the new folder (if needed)
	if(IsFolderExistent(cfg->dumpPathCurrent)==FALSE_FLAG)
	{
		sprintf(command,"mkdir %s", cfg->dumpPathCurrent);
		system(command);
	}
	// feedback to the user
	SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO, "LOG FOLDER IS CHANGED!");
	SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO, "New log folder is set to %s",cfg->dumpPathCurrent);

	// split the logs
	// standard log
	if((cfg->stdlog!=stdout) && (cfg->stdlog!=NULL))
	{
		if(GetFilenameFromFilePointer(cfg->stdlog,filename)==TRUE_FLAG)
		{
			fclose(cfg->stdlog);
			sprintf(path,"%s/%s",cfg->dumpPathCurrent,filename);
			cfg->stdlog = fopen(path, "a+");
			RestartSwrxLog(cfg);
		}
	}
	// acquisition, the split of the log files is managed by the coarse acquisition and refinements
	// tracking
	if(GetFilenameFromFilePointer(rx->trkCorrelatorsFile,filename)==TRUE_FLAG)
	{
		fclose(rx->trkCorrelatorsFile);
		sprintf(path,"%s/%s",cfg->dumpPathCurrent,filename);
		rx->trkCorrelatorsFile = fopen(path, "wb");
	}
	if(GetFilenameFromFilePointer(rx->trkMultiCorrelatorsFile,filename)==TRUE_FLAG)
	{
		fclose(rx->trkMultiCorrelatorsFile);
		sprintf(path,"%s/%s",cfg->dumpPathCurrent,filename);
		rx->trkMultiCorrelatorsFile = fopen(path, "wb");
	}
	// navigation message
	for(chIdx=0;chIdx<cfg->numberOfChannels;chIdx++)
	{
		ch = &(rx->channelArray[chIdx]);
		if(ch->satType==GPS_SAT)
		{
			if(GetFilenameFromFilePointer(ch->gpsParser->gpsParseLogFile,filename)==TRUE_FLAG)
			{
				fclose(ch->gpsParser->gpsParseLogFile);
				sprintf(path,"%s/%s",cfg->dumpPathCurrent,filename);
				ch->gpsParser->gpsParseLogFile = fopen(path,"a");
			}
		}
		else if(ch->satType==GALILEO_SAT)
		{
			if(GetFilenameFromFilePointer(ch->galParser->galParseLogFile,filename)==TRUE_FLAG)
			{
				fclose(ch->galParser->galParseLogFile);
				sprintf(path,"%s/%s",cfg->dumpPathCurrent,filename);
				ch->galParser->galParseLogFile = fopen(path,"a");
			}
		}
		else if(ch->satType==EGNOS_SAT)
		{
			if(GetFilenameFromFilePointer(ch->egnosParser->egnosParseLogFile,filename)==TRUE_FLAG)
			{
				fclose(ch->egnosParser->egnosParseLogFile);
				sprintf(path,"%s/%s",cfg->dumpPathCurrent,filename);
				ch->egnosParser->egnosParseLogFile = fopen(path,"a");
			}
			if(GetFilenameFromFilePointer(ch->egnosParser->egnosBitsLogFile,filename)==TRUE_FLAG)
			{
				fclose(ch->egnosParser->egnosBitsLogFile);
				sprintf(path,"%s/%s",cfg->dumpPathCurrent,filename);
				ch->egnosParser->egnosBitsLogFile = fopen(path,"a");
			}
		}
	}
	// navigation message (ifen)
	if(GetFilenameFromFilePointer(rx->ifenLogGps,filename)==TRUE_FLAG)
	{
		fclose(rx->ifenLogGps);
		sprintf(path,"%s/%s",cfg->dumpPathCurrent,filename);
		rx->ifenLogGps = fopen(path,"w");
		if(rx->ifenLogGps!=NULL)
		{
			fprintf(rx->ifenLogGps,"# FILE_VERSION = 1\n");
			fprintf(rx->ifenLogGps,"# NAV_TYPE = GPS_L1_NAV\n");
			fprintf(rx->ifenLogGps,"# BIT_TYPE = MESSAGE_BITS\n");
			fprintf(rx->ifenLogGps,"# FRAME_LENGTH_BITS = 300\n");
			fprintf(rx->ifenLogGps,"# FRAME_LENGTH_TIME = 6\n");
			fprintf(rx->ifenLogGps,"# BIT_ORDER = MSB_FIRST\n");
			if(rx->ifenLogGpsTime>=0)
			{
				fprintf(rx->ifenLogGps,"# START_TIME_TAG = %lld\n",rx->ifenLogGpsTime);
			}
		}
	}
	if(GetFilenameFromFilePointer(rx->ifenLogGal,filename)==TRUE_FLAG)
	{
		fclose(rx->ifenLogGal);
		sprintf(path,"%s/%s",cfg->dumpPathCurrent,filename);
		rx->ifenLogGal = fopen(path,"w");
		if(rx->ifenLogGal!=NULL)
		{
			fprintf(rx->ifenLogGal,"# FILE_VERSION = 1\n");
			fprintf(rx->ifenLogGal,"# NAV_TYPE = GAL_E1_INAV\n");
			fprintf(rx->ifenLogGal,"# BIT_TYPE = MESSAGE_SYMBOLS\n");
			fprintf(rx->ifenLogGal,"# FRAME_LENGTH_BITS = 250\n");
			fprintf(rx->ifenLogGal,"# FRAME_LENGTH_TIME = 1\n");
			fprintf(rx->ifenLogGal,"# BIT_ORDER = MSB_FIRST\n");
			if(rx->ifenLogGalTime>=0)
			{
				fprintf(rx->ifenLogGal,"# START_TIME_TAG = %lld\n",rx->ifenLogGalTime);
			}
		}
	}
	if(GetFilenameFromFilePointer(rx->ifenLogFNav,filename)==TRUE_FLAG)
	{
		fclose(rx->ifenLogFNav);
		sprintf(path,"%s/%s",cfg->dumpPathCurrent,filename);
		rx->ifenLogFNav = fopen(path,"w");
		if(rx->ifenLogFNav!=NULL)
		{
			fprintf(rx->ifenLogFNav,"# FILE_VERSION = 1\n");
			fprintf(rx->ifenLogFNav,"# NAV_TYPE = GAL_E5_FNAV\n");
			fprintf(rx->ifenLogFNav,"# BIT_TYPE = MESSAGE_SYMBOLS\n");
			fprintf(rx->ifenLogFNav,"# FRAME_LENGTH_BITS = 500\n");
			fprintf(rx->ifenLogFNav,"# FRAME_LENGTH_TIME = 10\n");
			fprintf(rx->ifenLogFNav,"# BIT_ORDER = MSB_FIRST\n");
			if(rx->ifenLogFNavTime>=0)
			{
				fprintf(rx->ifenLogFNav,"# START_TIME_TAG = %lld\n",rx->ifenLogFNavTime);
			}
		}
	}
	// almanac -> nothing to do, yuma almanac is saved only at the receiver de-initialization
	// OSNMA
	if(GetFilenameFromFilePointer(rx->osnmaData.parsLogFile,filename)==TRUE_FLAG)
	{
		fclose(rx->osnmaData.parsLogFile);
		sprintf(path,"%s/%s",cfg->dumpPathCurrent,filename);
		rx->osnmaData.parsLogFile = fopen(path,"w");
		// If available, save OSNMA parameters in the new file, to allow the execution of the analysis scripts
		if((rx->osnmaData.parsLogFile!=NULL) && (rx->osnmaData.osnmaPars.parametersValid==TRUE_FLAG))
		{
			fprintf(rx->osnmaData.parsLogFile,"%d\n",rx->osnmaData.osnmaPars.dsmKrootNb);
			fprintf(rx->osnmaData.parsLogFile,"%d\n",rx->osnmaData.osnmaPars.pkId);
			fprintf(rx->osnmaData.parsLogFile,"%d\n",rx->osnmaData.osnmaPars.chainId);
			fprintf(rx->osnmaData.parsLogFile,"%d\n",rx->osnmaData.osnmaPars.nmack);
			fprintf(rx->osnmaData.parsLogFile,"%d\n",rx->osnmaData.osnmaPars.hashFunction);
			fprintf(rx->osnmaData.parsLogFile,"%d\n",rx->osnmaData.osnmaPars.macFunction);
			fprintf(rx->osnmaData.parsLogFile,"%d\n",rx->osnmaData.osnmaPars.keySize);
			fprintf(rx->osnmaData.parsLogFile,"%d\n",rx->osnmaData.osnmaPars.macSize);
			fprintf(rx->osnmaData.parsLogFile,"%d\n",rx->osnmaData.osnmaPars.macLutIndex);
			fprintf(rx->osnmaData.parsLogFile,"%d\n",rx->osnmaData.osnmaPars.mackOffset);
			fprintf(rx->osnmaData.parsLogFile,"%d\n",rx->osnmaData.osnmaPars.krootWn);
			fprintf(rx->osnmaData.parsLogFile,"%d\n",rx->osnmaData.osnmaPars.krootTow);
			fprintf(rx->osnmaData.parsLogFile,"%02x%02x%02x%02x%02x%02x\n",rx->osnmaData.osnmaPars.alpha[0],rx->osnmaData.osnmaPars.alpha[1],rx->osnmaData.osnmaPars.alpha[2],rx->osnmaData.osnmaPars.alpha[3],rx->osnmaData.osnmaPars.alpha[4],rx->osnmaData.osnmaPars.alpha[5]);
			fprintf(rx->osnmaData.parsLogFile,"%d\n",rx->osnmaData.osnmaPars.ns);
			fprintf(rx->osnmaData.parsLogFile,"%d\n",rx->osnmaData.osnmaPars.mackBlockOffset);
		}
	}
	//
	if(GetFilenameFromFilePointer(rx->osnmaData.macLogFile,filename)==TRUE_FLAG)
	{
		fclose(rx->osnmaData.macLogFile);
		sprintf(path,"%s/%s",cfg->dumpPathCurrent,filename);
		rx->osnmaData.macLogFile = fopen(path,"w");
	}
	//
	if(GetFilenameFromFilePointer(rx->osnmaData.macseqLogFile,filename)==TRUE_FLAG)
	{
		fclose(rx->osnmaData.macseqLogFile);
		sprintf(path,"%s/%s",cfg->dumpPathCurrent,filename);
		rx->osnmaData.macseqLogFile = fopen(path,"w");
	}
	//
	if(GetFilenameFromFilePointer(rx->osnmaData.teslaChainFile,filename)==TRUE_FLAG)
	{
		fclose(rx->osnmaData.teslaChainFile);
		sprintf(path,"%s/%s",cfg->dumpPathCurrent,filename);
		rx->osnmaData.teslaChainFile = fopen(path,"w");
	}
	//
	if(GetFilenameFromFilePointer(rx->osnmaData.dsmLogFile,filename)==TRUE_FLAG)
	{
		fclose(rx->osnmaData.dsmLogFile);
		sprintf(path,"%s/%s",cfg->dumpPathCurrent,filename);
		rx->osnmaData.dsmLogFile = fopen(path,"w");
	}
	//
	if(GetFilenameFromFilePointer(rx->osnmaData.osnmaStatusLogFile,filename)==TRUE_FLAG)
	{
		fclose(rx->osnmaData.osnmaStatusLogFile);
		sprintf(path,"%s/%s",cfg->dumpPathCurrent,filename);
		rx->osnmaData.osnmaStatusLogFile = fopen(path,"w");
	}
	//
	if(GetFilenameFromFilePointer(rx->osnmaData.navdataLogFile,filename)==TRUE_FLAG)
	{
		fclose(rx->osnmaData.navdataLogFile);
		sprintf(path,"%s/%s",cfg->dumpPathCurrent,filename);
		rx->osnmaData.navdataLogFile = fopen(path,"w");
	}
	//
	if(GetFilenameFromFilePointer(rx->osnmaData.validationLogFile,filename)==TRUE_FLAG)
	{
		fclose(rx->osnmaData.validationLogFile);
		sprintf(path,"%s/%s",cfg->dumpPathCurrent,filename);
		rx->osnmaData.validationLogFile = fopen(path,"w");
	}
	//
	if(GetFilenameFromFilePointer(rx->osnmaData.authLogFile,filename)==TRUE_FLAG)
	{
		fclose(rx->osnmaData.authLogFile);
		sprintf(path,"%s/%s",cfg->dumpPathCurrent,filename);
		rx->osnmaData.authLogFile = fopen(path,"w");
		// the header is printed by SaveAuthenticationLog
	}
	//
	if(GetFilenameFromFilePointer(rx->osnmaData.simpleLogFile,filename)==TRUE_FLAG)
	{
		fclose(rx->osnmaData.simpleLogFile);
		sprintf(path,"%s/%s",cfg->dumpPathCurrent,filename);
		rx->osnmaData.simpleLogFile = fopen(path,"w");
	}
	//
	if(GetFilenameFromFilePointer(rx->osnmaData.simpleLogBinFile,filename)==TRUE_FLAG)
	{
		fclose(rx->osnmaData.simpleLogBinFile);
		sprintf(path,"%s/%s",cfg->dumpPathCurrent,filename);
		rx->osnmaData.simpleLogBinFile = fopen(path,"w");
	}
	// PVT text
	if(GetFilenameFromFilePointer(rx->pvtData.pvtLogFile,filename)==TRUE_FLAG)
	{
		fclose(rx->pvtData.pvtLogFile);
		sprintf(path,"%s/%s",cfg->dumpPathCurrent,filename);
		rx->pvtData.pvtLogFile = fopen(path,"w");
	}
	// PVT binary
	if(GetFilenameFromFilePointer(rx->pvtData.pvtBinFile,filename)==TRUE_FLAG)
	{
		fclose(rx->pvtData.pvtBinFile);
		sprintf(path,"%s/%s",cfg->dumpPathCurrent,filename);
		rx->pvtData.pvtBinFile = fopen(path,"wb");
	}
	// GGTO
	if(GetFilenameFromFilePointer(rx->pvtData.gnssTimeLogFile,filename)==TRUE_FLAG)
	{
		fclose(rx->pvtData.gnssTimeLogFile);
		sprintf(path,"%s/%s",cfg->dumpPathCurrent,filename);
		rx->pvtData.gnssTimeLogFile = fopen(path,"w");
	}
	// RINEX files are managed according the RINEX standard
	// NMEA -> close it but the new file is managed by NmeaSavePvt
	if(rx->nmeaFile!=NULL)
	{
		fclose(rx->nmeaFile);
		rx->nmeaFile = NULL;
	}

	/* update the split time */
	logSplitter->splitTow += logSplitter->splitPeriod;
	if(logSplitter->splitTow>=SECONDS_IN_A_WEEK)
	{
		logSplitter->splitTow -= SECONDS_IN_A_WEEK;
		logSplitter->splitWn++;
	}
}
