/**
 * @file ngene2NavMesgUtility.c
 *
 * @author Mario Nicola
 *
 * @date 01/02/2021
 *
 * @version 1.0.0
 *
 * @brief Functions used by ngene2NavMesg.c
 */

#include <string.h>
#include <stdlib.h>
#include "ngeneConstants.h"
#include "timeConstants.h"
#include "ngeneNavMesgUtility.h"
#include "ioFunctions.h"
#include "spreadingCodes.h"
#include "tracking.h"
#include "galileoMessageParser.h"
#include "gpsMessageParser.h"
#include "fnavMessageParser.h"
#include "channelManager.h"
#include "osnmaManagement.h"
#include "osnmaValidations.h"


static char rowFromNavMesgFile[MAX_ROW_LENGTH];
char navMesgFolderName[MAX_TAG_LEN];

#define IFEN_FILE_VERSION_TAG      "# FILE_VERSION"
#define IFEN_NAV_TYPE_TAG          "# NAV_TYPE"
#define IFEN_BIT_TYPE_TAG          "# BIT_TYPE"
#define IFEN_FRAME_LENGTH_BITS_TAG "# FRAME_LENGTH_BITS"
#define IFEN_FRAME_LENGTH_TIME_TAG "# FRAME_LENGTH_TIME"
#define IFEN_BIT_ORDER_TAG         "# BIT_ORDER"
#define IFEN_START_TIME_TAG        "# START_TIME_TAG"

#define FROM_SECONDS_TO_MILLISECONDS  1000


int ParseNGene2NavMesgParameters(TNGeneConfiguration *cfg,TNGeneNavMesgPars *pars)
{
	char cfgFilename[MAX_ROW_LENGTH];
	char tag[MAX_TAG_LEN];
	char filename[MAX_TAG_LEN];
	int delay;
	int wn;
	int tow;

	cfgFilename[0] = '\0';
	strcat(cfgFilename, cfg->configurationPath);
	strcat(cfgFilename, "GNSS_RX_NavMesg.conf");

	/* Common folder */
	strcpy(tag, "NavMesg folder");
	if(GetVal(cfgFilename,tag,(void *)navMesgFolderName,F_STRING,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found. Default value used.",cfgFilename,tag);
		return FAILURE_RETURN;
	}
	strcat(navMesgFolderName,"/");

	/* Galileo I/NAV */
	strcpy(tag, "NavMesg E1b file");
	if(GetVal(cfgFilename,tag,(void *)filename,F_STRING,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found. Default value used.",cfgFilename,tag);
		return FAILURE_RETURN;
	}
	pars->galE1bNavMesgFilename[0] = '\0';
	strcat(pars->galE1bNavMesgFilename,navMesgFolderName);
	strcat(pars->galE1bNavMesgFilename,filename);
	if(CheckFileExistence(pars->galE1bNavMesgFilename)==FALSE_FLAG)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"NGene2NavMesg I/NAV input file <%s> does not exist.",pars->galE1bNavMesgFilename);
		return FAILURE_RETURN;
	}
	else
	{
		SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"I/NAV message read from %s",pars->galE1bNavMesgFilename);
	}

	/* GPS L1 C/A */
	strcpy(tag, "NavMesg L1 CA file");
	if(GetVal(cfgFilename,tag,(void *)filename,F_STRING,1) == FAILURE_RETURN)
	{
		pars->gpsL1caEnabled = FALSE_FLAG;
		SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"L1 C/A message elaboration disabled");
	}
	else
	{
		pars->gpsL1caNavMesgFilename[0] = '\0';
		strcat(pars->gpsL1caNavMesgFilename,navMesgFolderName);
		strcat(pars->gpsL1caNavMesgFilename,filename);
		if(CheckFileExistence(pars->gpsL1caNavMesgFilename)==FALSE_FLAG)
		{
			pars->gpsL1caEnabled = FALSE_FLAG;
			SwrxLog(NGENE_LOG_SEVERITY_WARNING,"NGene2NavMesg L1 C/A input file <%s> does not exist.",pars->gpsL1caNavMesgFilename);
		}
		else
		{
			pars->gpsL1caEnabled = TRUE_FLAG;
			SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"L1 C/A message read from %s",pars->gpsL1caNavMesgFilename);
		}
	}

	/* Galileo E5a */
	strcpy(tag, "NavMesg E5a file");
	if(GetVal(cfgFilename,tag,(void *)filename,F_STRING,1) == FAILURE_RETURN)
	{
		pars->galE5aEnabled = FALSE_FLAG;
		SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"F/NAV message elaboration disabled");
	}
	else
	{
		pars->galE5aNavMesgFilename[0] = '\0';
		strcat(pars->galE5aNavMesgFilename,navMesgFolderName);
		strcat(pars->galE5aNavMesgFilename,filename);
		if(CheckFileExistence(pars->galE5aNavMesgFilename)==FALSE_FLAG)
		{
			pars->galE5aEnabled = FALSE_FLAG;
			SwrxLog(NGENE_LOG_SEVERITY_WARNING,"NGene2NavMesg F/NAV input file <%s> does not exist.",pars->galE5aNavMesgFilename);
		}
		else
		{
			pars->galE5aEnabled = TRUE_FLAG;
			SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"F/NAV message read from %s",pars->galE5aNavMesgFilename);
		}
	}

	/* Revert GPS Parity inversion */
	strcpy(tag, "GPS Parity inversion");
	if(GetVal(cfgFilename,tag,(void *)&pars->revertGpsParityInversion,F_INTEGER,1) == FAILURE_RETURN)
	{
		pars->revertGpsParityInversion = 0;
	}
	pars->revertGpsParityInversion = (pars->revertGpsParityInversion>0) ? TRUE_FLAG : FALSE_FLAG;
	// must revert also the parity bits?
	if(pars->revertGpsParityInversion==FALSE_FLAG)
	{
		pars->numberOfGpsRevertedBits = 0; // not used, do not care
	}
	else
	{
		strcpy(tag, "GPS Parity bits inverted");
		if(GetVal(cfgFilename,tag,(void *)&pars->numberOfGpsRevertedBits,F_INTEGER,1) == FAILURE_RETURN)
		{
			pars->numberOfGpsRevertedBits = 0;
		}
		// GPS_WORD_LENGTH = all the bits in the word
		// GPS_DATA_BITS_PER_WORD = only the data bits in the word
		pars->numberOfGpsRevertedBits = (pars->numberOfGpsRevertedBits>0) ? GPS_WORD_LENGTH : GPS_DATA_BITS_PER_WORD;
	}

	/* Elaboration start/end */
	strcpy(tag, "NavMesg Start Time");
	if(GetVal(cfgFilename,tag,(void *)rowFromNavMesgFile,F_STRING,1) == FAILURE_RETURN)
	{
		pars->startGalTime = INVALID_VALUE;
	}
	else
	{
		// check format and parse
		if(strstr(rowFromNavMesgFile,",")!=NULL)
		{
			// WN,TOW
			if(sscanf(rowFromNavMesgFile,"%d,%d",&wn,&tow)!=2)
			{
				SwrxLog(NGENE_LOG_SEVERITY_WARNING, "Invalid format for %s (WN,TOW). Elaboration will start from the begin of the file",tag);
				pars->startGalTime = INVALID_VALUE;
			}
			else
			{
				pars->startGalTime = ((long long int)wn*SECONDS_PER_WEEK) + (long long int)tow;
				pars->startGalTime *= FROM_SECONDS_TO_MILLISECONDS;
			}
		}
		else
		{
			if(sscanf(rowFromNavMesgFile,"%lld",&(pars->startGalTime))!=1)
			{
				SwrxLog(NGENE_LOG_SEVERITY_WARNING, "Invalid format for %s. Elaboration will start from the begin of the file",tag);
				pars->startGalTime = INVALID_VALUE;
			}
			else
			{
				pars->startGalTime *= FROM_SECONDS_TO_MILLISECONDS;
			}
		}
	}
	//
	strcpy(tag, "NavMesg End Time");
	if(GetVal(cfgFilename,tag,(void *)rowFromNavMesgFile,F_STRING,1) == FAILURE_RETURN)
	{
		pars->endGalTime = INVALID_VALUE;
	}
	else
	{
		// check format and parse
		if(strstr(rowFromNavMesgFile,",")!=NULL)
		{
			// WN,TOW
			if(sscanf(rowFromNavMesgFile,"%d,%d",&wn,&tow)!=2)
			{
				SwrxLog(NGENE_LOG_SEVERITY_WARNING, "Invalid format for %s (WN,TOW). Elaboration will continue till the end of the file",tag);
				pars->endGalTime = INVALID_VALUE;
			}
			else
			{
				pars->endGalTime = ((long long int)wn*SECONDS_PER_WEEK) + (long long int)tow;
				pars->endGalTime *= FROM_SECONDS_TO_MILLISECONDS;
			}
		}
		else
		{
			if(sscanf(rowFromNavMesgFile,"%lld",&(pars->endGalTime))!=1)
			{
				SwrxLog(NGENE_LOG_SEVERITY_WARNING, "Invalid format for %s. Elaboration will continue till the end of the file",tag);
				pars->endGalTime = INVALID_VALUE;
			}
			else
			{
				pars->endGalTime *= FROM_SECONDS_TO_MILLISECONDS;
			}
		}
	}

	/* Execution delay */
	strcpy(tag, "NGene2NavMesg delay");
	if(GetVal(cfgFilename,tag,(void *)&delay,F_INTEGER,1) == FAILURE_RETURN)
	{
		delay = DEFAULT_NCURSES_DELAY;
	}
	if(delay<0)
		delay = DEFAULT_NCURSES_DELAY;
	else if(delay>MAXIMUM_NCURSES_DELAY)
		delay = MAXIMUM_NCURSES_DELAY;
	pars->delayForNCurses.tv_sec = 0;
	pars->delayForNCurses.tv_nsec = delay;

	return SUCCESS_RETURN;
}


int AdaptTheNumberOReceiverChannels(TNGeneConfiguration *cfg,TNGeneNavMesgPars *pars)
{
	/* Some Galileo E1b channels must be allocated */
	if(cfg->numberOfGalileoChannels==0)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"No Galileo E1b channels allocated for NGene2NavMesg. Allocate some channel to allow the elaboration.");
		return FAILURE_RETURN;
	}
	/* The number of Galileo E5a channels is zero by default;
	   if F/NAV elaboration is enabled, the number of E5a channels is equal to the number of E1b channels */
	if(pars->galE5aEnabled==TRUE_FLAG)
	{
		cfg->numberOfGalileoE5aChannels = cfg->numberOfGalileoChannels;
	}
	/* Check the number of GPS channels */
	if((pars->gpsL1caEnabled==FALSE_FLAG) && (cfg->numberOfGpsChannels>0))
	{
		cfg->numberOfGpsChannels = 0;
		SwrxLog(NGENE_LOG_SEVERITY_WARNING,"GPS L1 C/A NavMesg is not elaborated, no receiver channels are allocated to GPS.");
	}
	else if((pars->gpsL1caEnabled==TRUE_FLAG) && (cfg->numberOfGpsChannels==0))
	{
		pars->gpsL1caEnabled = FALSE_FLAG;
		SwrxLog(NGENE_LOG_SEVERITY_WARNING,"No receiver channels allocated to GPS, then the GPS L1 C/A NavMesg is not elaborated.");
	}
	/* Check the number of EGNOS channels */
	if(cfg->numberOfEgnosChannels>0)
	{
		cfg->numberOfEgnosChannels = 0;
		SwrxLog(NGENE_LOG_SEVERITY_WARNING,"NGene2 NavMesg does not support EGNOS, no receiver channels are allocated to EGNOS.");
	}

	/* adjust the total number of receiver channels */
	cfg->numberOfChannels = cfg->numberOfGpsChannels + cfg->numberOfGalileoChannels + cfg->numberOfEgnosChannels + cfg->numberOfGalileoE5aChannels;
	return SUCCESS_RETURN;
}


int InitNGene2NavMesgData(TNGeneNavMesgPars *pars,TSatConstellation satType,TSatSignal satSignal,TNGeneNavMesgData *data)
{
	int idx;
	char *navMesgFilename;
	char *charPtr;
	int num;

	/* Init variables */
	data->satType = satType;
	data->satSignal = satSignal;
	data->numPrns = 0;
	data->symbolsPerRow = 0;
	data->symbolDuration = 0;
	data->navMesgFile = NULL;
	data->startTime = INVALID_TIME;
	data->chIndex = NULL;
	data->symbolsPtr = NULL;
	data->symbolsIndex = NULL;
	data->t = NULL;
	data->gpsInvertedPreamble = NULL;
	//
	if((satType==GALILEO_SAT) && (satSignal==E1_BC))
	{
		data->numPrns = GALILEO_NUMBER_OF_PRNS;
		data->symbolsPerRow = GAL_PAGE_PART_LENGTH;
		data->symbolDuration = GALILEO_CODE_DURATION;
		navMesgFilename = pars->galE1bNavMesgFilename;
	}
	else if((pars->gpsL1caEnabled==TRUE_FLAG) && (satType==GPS_SAT) && (satSignal==L1_CA))
	{
		data->numPrns = GPS_NUMBER_OF_PRNS;
		data->symbolsPerRow = GPS_SUBFRAME_LENGTH*GPS_WORD_LENGTH;
		data->symbolDuration = GPS_BIT_DURATION;
		if(pars->gpsL1caEnabled==FALSE_FLAG)
		{
			// not initialized due to configuration, not an error
			return SUCCESS_RETURN;
		}
		navMesgFilename = pars->gpsL1caNavMesgFilename;
	}
	else if((pars->galE5aEnabled==TRUE_FLAG) && (satType==GALILEO_SAT) && (satSignal==E5_A))
	{
		data->numPrns = GALILEO_NUMBER_OF_PRNS;
		data->symbolsPerRow = FNAV_PAGE_LENGTH;
		data->symbolDuration = GALILEO_E5A_BIT_DURATION;
		if(pars->galE5aEnabled==FALSE_FLAG)
		{
			// not initialized due to configuration, not an error
			return SUCCESS_RETURN;
		}
		navMesgFilename = pars->galE5aNavMesgFilename;
	}
	else
	{
		// disabled or invalid values for sat type and signal
		return SUCCESS_RETURN;
	}

	data->navMesgFile = fopen(navMesgFilename,"r");
	if(data->navMesgFile==NULL)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Cannot open the input file <%s>.",navMesgFilename);
		return FAILURE_RETURN;
	}

	/* Allocate arrays */
	data->chIndex = (int *)malloc(data->numPrns*sizeof(int));
	data->symbolsPtr = (char **)malloc(data->numPrns*sizeof(char *));
	data->symbolsIndex = (int *)malloc(data->numPrns*sizeof(int));
	data->t = (long long int *)malloc(data->numPrns*sizeof(long long int));
	data->gpsInvertedPreamble = (char *)malloc(data->numPrns*sizeof(char));
	for(idx=0;idx<data->numPrns;idx++)
	{
		data->chIndex[idx] = INVALID_VALUE;
		data->symbolsPtr[idx] = (char *)malloc(data->symbolsPerRow*sizeof(char));
		data->symbolsIndex[idx] = INVALID_VALUE;
		data->t[idx] = INVALID_TIME;
		data->gpsInvertedPreamble[idx] = INVALID_VALUE;
	}

	/* Discard and check the header */
	for(idx=0;idx<7;idx++)
	{
		fgets(rowFromNavMesgFile,MAX_ROW_LENGTH,data->navMesgFile);
		// check the header content, stop when the content is suspicious
		if((strstr(rowFromNavMesgFile,IFEN_FILE_VERSION_TAG)==NULL) &&
		   (strstr(rowFromNavMesgFile,IFEN_NAV_TYPE_TAG)==NULL) &&
		   (strstr(rowFromNavMesgFile,IFEN_BIT_TYPE_TAG)==NULL) &&
		   (strstr(rowFromNavMesgFile,IFEN_FRAME_LENGTH_BITS_TAG)==NULL) &&
		   (strstr(rowFromNavMesgFile,IFEN_FRAME_LENGTH_TIME_TAG)==NULL) &&
		   (strstr(rowFromNavMesgFile,IFEN_BIT_ORDER_TAG)==NULL) &&
		   (strstr(rowFromNavMesgFile,IFEN_START_TIME_TAG)==NULL))
		{
			SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Invalid header in I/NAV input file <%s> does not exist.",pars->galE1bNavMesgFilename);
			return FAILURE_RETURN;
		}
		//
		if(strstr(rowFromNavMesgFile,IFEN_FRAME_LENGTH_BITS_TAG)!=NULL)
		{
			charPtr = strstr(rowFromNavMesgFile,"=");
			sscanf(charPtr+1,"%d",&num);
			if(num!=data->symbolsPerRow)
			{
				SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Invalid tag FRAME_LENGTH_BITS in the header of the navigation message file.");
				return FAILURE_RETURN;
			}
		}
		if(strstr(rowFromNavMesgFile,IFEN_FRAME_LENGTH_TIME_TAG)!=NULL)
		{
			charPtr = strstr(rowFromNavMesgFile,"=");
			sscanf(charPtr+1,"%d",&num);
			if((num*FROM_SECONDS_TO_MILLISECONDS)!=(data->symbolsPerRow*data->symbolDuration))
			{
				SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Invalid tag FRAME_LENGTH_TIME in the header of the navigation message file.");
				return FAILURE_RETURN;
			}
		}
		if(strstr(rowFromNavMesgFile,IFEN_START_TIME_TAG)!=NULL)
		{
			charPtr = strstr(rowFromNavMesgFile,"=");
			if(sscanf(charPtr+1,"%d",&num)!=1)
			{
				SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Invalid tag IFEN_START_TIME_TAG in the header of the navigation message file.");
				return FAILURE_RETURN;
			}
			data->startTime = (long long int)num*(long long int)FROM_SECONDS_TO_MILLISECONDS;
		}
	}

	return SUCCESS_RETURN;
}


void DeinitNGene2NavMesgData(TNGeneConfiguration *cfg,TNGeneData *rx,TNGeneNavMesgData *data)
{
	int prnIdx;
	int chIdx;
	TChannel *ch;

	if(data->navMesgFile==NULL)
	{
		// data structure empty, deinitialization not needed
		return;
	}

	fclose(data->navMesgFile);
	data->navMesgFile = NULL;
	for(prnIdx=0;prnIdx<data->numPrns;prnIdx++)
	{
		if(data->chIndex[prnIdx]!=INVALID_VALUE)
		{
			chIdx = data->chIndex[prnIdx];
			// free the receiver channel
			ch = &(rx->channelArray[chIdx]);
			ReleaseTracking(cfg,&(rx->channelScheduler),ch);
			DeleteChannelFromOsnmaData(&(rx->osnmaData),chIdx);
			ResetChannelAuthenticationStruct(&(rx->osnmaAuth),chIdx);
			ch->chState = POWERON_STATE;
			data->chIndex[prnIdx] = INVALID_VALUE;
		}
		free(data->symbolsPtr[prnIdx]);
		data->symbolsIndex[prnIdx] = INVALID_VALUE;
		data->t[prnIdx] = INVALID_TIME;
	}
	free(data->chIndex);
	free(data->symbolsPtr);
	free(data->symbolsIndex);
	free(data->t);
	free(data->gpsInvertedPreamble);
}


int AreSymbolsAvailable(TNGeneNavMesgData *data)
{
	int idx;

	if(data->navMesgFile==NULL)
	{
		// disabled, no symbols available
		return FALSE_FLAG;
	}

	for(idx=0;idx<data->numPrns;idx++)
	{
		if((data->symbolsIndex[idx]>INVALID_VALUE) && (data->symbolsIndex[idx]<data->symbolsPerRow))
		{
			// symbols available if the current page is valid (symbolsIndex!=-1) and not empty (<symbolsPerRow)
			return TRUE_FLAG;
		}
	}

	// if here, all entries are invalid or empty; no symbols available
	return FALSE_FLAG;
}


static void UnpackHexBytes(char *hexArray,char *byteArray,int num)
{
	int rdIdx = 0;
	unsigned int tmpInt;
	unsigned char tmpChar;
	int wrIdx = 0;
	unsigned char mask = 0x80;

	sscanf(&(hexArray[rdIdx]),"%2x",&tmpInt);
	tmpChar = (unsigned char)tmpInt;
	rdIdx += 2;
	while(wrIdx<num)
	{
		byteArray[wrIdx] = ((tmpChar & mask)==0) ? 0 : 1;
		wrIdx++;
		mask = mask >>1;
		if((wrIdx<num) && (mask==0x00))
		{
			sscanf(&(hexArray[rdIdx]),"%2x",&tmpInt);
			tmpChar = (unsigned char)tmpInt;
			rdIdx += 2;
			mask = 0x80;
		}
	}
}


static void RevertParityInversion(char *byteArray,int num,int bitsToBeReverted)
{
	int firstIdx = 0;
	int idx;
	int refIdx = num -1; // last bit of the subframe

	while(firstIdx<num)
	{
		/*
		 * The last bit of the previous word is used to decide when
		 * the parity inversion must be reverted or not
		 * For the first word, it uses the last bit of the current subframe
		 * (theoretically it should the last bit of the previous subframe)
		 */
		if(byteArray[refIdx]==0x01)
		{
			/* The bit is 1, revert the parity inversion.
			 * Apply only bits required by the [GPS Parity bits inverted] parameter
			 * (data + parity bits or data bits only)
			 */
			for(idx=firstIdx;idx<(firstIdx+bitsToBeReverted);idx++)
			{
				byteArray[idx] = byteArray[idx] ^ 0x01;
			}
		}
		//
		firstIdx += GPS_WORD_LENGTH;
		refIdx = (refIdx+GPS_WORD_LENGTH)%num;
	}
}


static void ModulateBpsk(char *byteArray,int num)
{
	int idx;

	for(idx=0;idx<num;idx++)
	{
		byteArray[idx] = 1 - (2*byteArray[idx]);
	}
}


void ManageMissingPrns(TNGeneConfiguration *cfg,TNGeneData *rx,TNGeneNavMesgData *data)
{
	int prnIdx;
	int chIdx;
	TChannel *ch;

	for(prnIdx=0;prnIdx<data->numPrns;prnIdx++)
	{
		if((data->symbolsIndex[prnIdx]==data->symbolsPerRow) && (data->chIndex[prnIdx]!=INVALID_VALUE))
		{
			// the page variable is empty
			// free the receiver channel
			chIdx = data->chIndex[prnIdx];
			ch = &(rx->channelArray[chIdx]);
			ReleaseTracking(cfg,&(rx->channelScheduler),ch);
			DeleteChannelFromOsnmaData(&(rx->osnmaData),chIdx);
			ResetChannelAuthenticationStruct(&(rx->osnmaAuth),chIdx);
			ch->chState = POWERON_STATE;
			// reset other variables
			data->chIndex[prnIdx] = INVALID_VALUE;
			data->symbolsIndex[prnIdx] = INVALID_VALUE;
			data->t[prnIdx] = INVALID_TIME;
			data->gpsInvertedPreamble[prnIdx] = INVALID_VALUE;
		}
	}
}


void ManageGpsInvertedPreamble(TNGeneNavMesgData *data,int prn)
{
	char *symbols;
	int idx;
	unsigned char preamble = 0x00;
	unsigned char mask = 0x80;

	/* check the constellation */
	if(data->satType!=GPS_SAT)
	{
		/* nothing to do */
		return;
	}

	/* get the current preamble */
	symbols = data->symbolsPtr[prn-1];
	for(idx=0;idx<GPS_PREAMBLE_LENGTH;idx++)
	{
		if(symbols[idx]==0x01)
		{
			preamble = preamble | mask;
		}
		mask = mask >> 1;
	}
	/* manage the flag */
	if(data->gpsInvertedPreamble[prn-1]==INVALID_VALUE)
	{
		// the first received subframe, set the flag
		if(preamble==(unsigned char)GPS_PREAMBLE_BITS)
			data->gpsInvertedPreamble[prn-1] = FALSE_FLAG;
		else if(preamble==(unsigned char)GPS_INV_PREAMBLE_BITS)
			data->gpsInvertedPreamble[prn-1] = TRUE_FLAG;
	}
	else
	{
		/* check the flag */
		if(((data->gpsInvertedPreamble[prn-1]==FALSE_FLAG) && (preamble==(unsigned char)GPS_INV_PREAMBLE_BITS)) ||
		   ((data->gpsInvertedPreamble[prn-1]==TRUE_FLAG) && (preamble==(unsigned char)GPS_PREAMBLE_BITS)))
		{
			// a phase change occurred, revert the bits
			for(idx=0;idx<data->symbolsPerRow;idx++)
			{
				symbols[idx] ^= 0x01;
			}
		}
	}
}


void ManageNewPage(TNGeneConfiguration *cfg,TNGeneData *rx,TNGeneNavMesgData *data,int prn,int currentTime)
{
	int idx = 0;
	TChannel *ch = &(rx->channelArray[0]);

	if(data->chIndex[prn-1]==INVALID_VALUE)
	{
		// allocate a new receiver channel
		while(idx<cfg->numberOfChannels)
		{
			ch = &(rx->channelArray[idx]);
			if((ch->satType==data->satType) && (ch->satSignal==data->satSignal) && (ch->satId==INVALID_PRN))
			{
				break;
			}
			idx++;
		}
		if(idx==cfg->numberOfChannels)
		{
			// no channels available
			return;
		}
		// save the index of the channel
		data->chIndex[prn-1] = idx;
		// init the receiver channel
		ch->satId = prn;
		StartTracking(cfg,ch,&(ch->tracking));
		StartGalParser(cfg,rx,ch);
		StartGpsParser(cfg,rx,ch);
		StartFNavParser(cfg,rx,ch);
		ch->chState = TRACKING_STATE;
		ch->tracking.trkStatus = TRACKING_LOCKED;
	}
	// manage other variables
	data->t[prn-1] = data->startTime + (currentTime*SECONDS_TO_MILLISECONDS);
	data->symbolsIndex[prn-1] = 0; // full
}


/**
 * @def MAXIMUM_NCURSES_DELAY
 * @brief Maximum distance [s] between the last time stamp read and the next entry.
 *        If the next row has a timestamp which exceeds this difference,
 *        the entire row is discarded.
 */
#define MAXIMUM_TIME_STAMP_ERROR    3600


int ReadTheNextNavMesgRows(TNGeneConfiguration *cfg,TNGeneData *rx,TNGeneNavMesgPars *pars,TNGeneNavMesgData *data,long long int lastTime)
{
	int timeCheck = (lastTime-data->startTime)/SECONDS_TO_MILLISECONDS; // [s]
	int time1;
	int prn;
	int pos;
	int numEntries = 0;
	fpos_t msgPos;
	int time2;

	if(data->navMesgFile==NULL)
	{
		// current NavMesg is disabled, do nothing
		return 0;
	}

	/* read the first entry from file */
	if(fgets(rowFromNavMesgFile,MAX_ROW_LENGTH,data->navMesgFile)==NULL)
	{
		// file finished, disable current NavMesg, then return
		DeinitNGene2NavMesgData(cfg,rx,data);
		return 0;
	}
	sscanf(rowFromNavMesgFile,"%d,%d%n",&time1,&prn,&pos);
	if(lastTime!=INVALID_TIME)
	{
		// check the timestamp
		while(((time1-timeCheck)<0) || ((time1-timeCheck)>MAXIMUM_TIME_STAMP_ERROR))
		{
			// if the new time stamp is in the past, or too much in the future, discard the row
			if(fgets(rowFromNavMesgFile,MAX_ROW_LENGTH,data->navMesgFile)==NULL)
			{
				// file finished, disable current NavMesg, then return
				DeinitNGene2NavMesgData(cfg,rx,data);
				return 0;
			}
			sscanf(rowFromNavMesgFile,"%d,%d%n",&time1,&prn,&pos);
		}
	}
	UnpackHexBytes(&(rowFromNavMesgFile[pos+1]),data->symbolsPtr[prn-1],data->symbolsPerRow);
	if((data->satType==GPS_SAT) && (pars->revertGpsParityInversion==TRUE_FLAG))
	{
		RevertParityInversion(data->symbolsPtr[prn-1],data->symbolsPerRow,pars->numberOfGpsRevertedBits);
		ManageGpsInvertedPreamble(data,prn);
	}
	ModulateBpsk(data->symbolsPtr[prn-1],data->symbolsPerRow);
	ManageNewPage(cfg,rx,data,prn,time1);
	numEntries++;

	/* other entries with the same time */
	time2 = time1;
	while(time2==time1)
	{
		fgetpos(data->navMesgFile,&msgPos);
		if(fgets(rowFromNavMesgFile,MAX_ROW_LENGTH,data->navMesgFile)==NULL)
			return 0;

		sscanf(rowFromNavMesgFile,"%d,%d%n",&time2,&prn,&pos);
		// check the timestamp
		while(((time2-time1)<0) || ((time2-time1)>MAXIMUM_TIME_STAMP_ERROR))
		{
			// if the new time stamp is in the past, or too much in the future, discard the row
			fgetpos(data->navMesgFile,&msgPos);
			if(fgets(rowFromNavMesgFile,MAX_ROW_LENGTH,data->navMesgFile)==NULL)
				return 0;

			sscanf(rowFromNavMesgFile,"%d,%d%n",&time2,&prn,&pos);
		}
		// the new timestamp is valid, does it belong to the same set of data?
		if(time2==time1)
		{
			UnpackHexBytes(&(rowFromNavMesgFile[pos+1]),data->symbolsPtr[prn-1],data->symbolsPerRow);
			if((data->satType==GPS_SAT) && (pars->revertGpsParityInversion==TRUE_FLAG))
			{
				RevertParityInversion(data->symbolsPtr[prn-1],data->symbolsPerRow,pars->numberOfGpsRevertedBits);
				ManageGpsInvertedPreamble(data,prn);
			}
			ModulateBpsk(data->symbolsPtr[prn-1],data->symbolsPerRow);
			ManageNewPage(cfg,rx,data,prn,time1);
			numEntries++;
		}
	}
	/* put back */
	fsetpos(data->navMesgFile,&msgPos);
	/* free missing PRNs */
	ManageMissingPrns(cfg,rx,data);
	/* return value */
	return numEntries;
}


void ElaborateNextSymbol(TNGeneConfiguration *cfg,TNGeneData *rx,TNGeneNavMesgData *data,long long int t)
{
	int prn;
	int idx;
	TChannel *ch;
	int symbolIndex;
	char *ptrChar;
	char symbol;

	if(data->navMesgFile==NULL)
		return;

	for(idx=0;idx<cfg->numberOfChannels;idx++)
	{
		ch = &(rx->channelArray[idx]);
		if((ch->satType==data->satType) && (ch->satSignal==data->satSignal) && (ch->satId!=INVALID_VALUE))
		{
			prn = ch->satId;
			if((data->symbolsIndex[prn-1]>INVALID_VALUE) && (data->symbolsIndex[prn-1]<data->symbolsPerRow) && (data->t[prn-1]==t))
			{
				// the stored page is valid, not empty, and the time to use the next symbol is now!
				// use the symbol
				ptrChar = data->symbolsPtr[prn-1];
				symbolIndex = data->symbolsIndex[prn-1];
				symbol = ptrChar[symbolIndex];
				if((data->satType==GALILEO_SAT) && (ch->satSignal==E1_BC))
				{
					ParseGalMessage(cfg,rx,ch,symbol);
				}
				else if((data->satType==GPS_SAT) && (ch->satSignal==L1_CA))
				{
					ParseGpsMessage(cfg,rx,ch,symbol);
				}
				else if((data->satType==GALILEO_SAT) && (ch->satSignal==E5_A))
				{
					ParseFNavMessage(rx,ch,symbol);
				}

				// update, pass to the next symbol
				data->symbolsIndex[prn-1]++;
				data->t[prn-1] += data->symbolDuration;
			}
		}
	}
}





/* GPS time is ahead 1024 weeks with respect to Galileo [ms] */
#define BIAS_GAL_GPS_TIME  1024*SECONDS_PER_WEEK*SECONDS_TO_MILLISECONDS


void FastForwardNavMesgData(TNGeneNavMesgPars *pars,TNGeneNavMesgData *data)
{
	long long int startTime = pars->startGalTime; // [ms]
	int timeCheck; // [s]
	fpos_t msgPos;
	int time1;

	/* Preliminary checks */
	if(data->navMesgFile==NULL)
	{
		// current NavMesg is disabled, do nothing
		return;
	}
	if(startTime==INVALID_VALUE)
	{
		// Delayed start is disabled, do nothing
		return;
	}

	/* Adjust time for GPS (if needed) */
	if(data->satType==GPS_SAT)
	{
		startTime += BIAS_GAL_GPS_TIME;
	}

	/* Ease the comparison */
	timeCheck = (startTime - data->startTime)/SECONDS_TO_MILLISECONDS;
	if(timeCheck<=0)
	{
		// Elaboration start time already expired, do nothing
		return;
	}

	/* Discard entries that precede the elaboration start time */
	time1 = INVALID_VALUE;
	while(time1<timeCheck)
	{
		fgetpos(data->navMesgFile,&msgPos);
		if(fgets(rowFromNavMesgFile,MAX_ROW_LENGTH,data->navMesgFile)==NULL)
		{
			SwrxLog(NGENE_LOG_SEVERITY_WARNING, "Input NavMesg ends before the selected elaboration start time");
			return;
		}

		// read only the time tag
		sscanf(rowFromNavMesgFile,"%d",&time1);
	}
	/* First entry to be elaborated has been found
	 * Go back to the start of the row */
	fsetpos(data->navMesgFile,&msgPos);
}
