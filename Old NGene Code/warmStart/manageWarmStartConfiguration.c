/**
 * @file manageWarmStartConfiguration.c
 *
 * @author Mario Nicola
 *
 * @date 08/08/2014
 *
 * @version 1.0.0
 *
 * @brief Functions that manage Warm Start configuration.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "warmStartConfiguration.h"
#include "ngeneConstants.h"
#include "ioFunctions.h"
#include "closeManager.h"
#include "timeConversions.h"
#include "pvtUtility.h"

int ManageWarmStartConfiguration(TNGeneConfiguration *cfg)
{
	TWarmStartConfiguration *warmCfg;
	char cfgFilename[MAX_ROW_LENGTH];
	char tag[MAX_TAG_LEN];
	int tmp[3];
	char filenameGps[MAX_ROW_LENGTH];
	char filenameGal[MAX_ROW_LENGTH];
	FILE *tmpFile;
	int filenameGpsLen;
	int filenameGalLen;
	TDateTime refTime;
	double geodPos[3];

	warmCfg = &(cfg->warmConfiguration);
	cfgFilename[0] = '\0';
	strcat(cfgFilename, cfg->configurationPath);
	strcat(cfgFilename, "Warm.conf");

	/* default values of configuration */
	warmCfg->enableWarmStart=FALSE_FLAG;
	warmCfg->almanacPathGps = NULL;
	warmCfg->almanacPathGps = NULL;
	warmCfg->warmLatitude = INVALID_VALUE;
	warmCfg->warmLongitude = INVALID_VALUE;
	warmCfg->warmYear = INVALID_VALUE;
	warmCfg->warmMonth = INVALID_VALUE;
	warmCfg->warmDay = INVALID_VALUE;
	warmCfg->warmHour = INVALID_VALUE;
	warmCfg->warmMinute = INVALID_VALUE;
	warmCfg->warmSecond = INVALID_VALUE;

	/* parse the configuration file */
	strcpy(tag, "Enable Warm Start");
	if(GetVal(cfgFilename,tag,(void *)&(warmCfg->enableWarmStart),F_INTEGER,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	if(warmCfg->enableWarmStart!=TRUE_FLAG)
	{
		return SUCCESS_RETURN;
	}

	#ifdef FAKE_OSNMA_RECEIVER
	if(warmCfg->enableWarmStart==TRUE_FLAG)
	{
		warmCfg->enableWarmStart = FALSE_FLAG;
		SwrxLog(NGENE_LOG_SEVERITY_WARNING,"NGene \"Navigation Message Receiver\" will not use warm start");
		return SUCCESS_RETURN;
	}
	#endif

	/* manage almanac names */
	strcpy(tag, "GPS Almanac file");
	if(GetVal(cfgFilename,tag,(void *)filenameGps,F_STRING,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error in file <%s>: tag <%s> not found.",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	tmpFile = fopen(filenameGps,"rb");
	if(tmpFile==NULL)
	{
		SwrxLog(NGENE_LOG_SEVERITY_WARNING,"Error in <%s>: GPS almanac file not existing.",cfgFilename);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	fclose(tmpFile);
	//
	strcpy(tag, "Galileo Almanac file");
	if(GetVal(cfgFilename,tag,(void *)filenameGal,F_STRING,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error in <%s>: tag <%s> not found.",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	tmpFile = fopen(filenameGal,"rb");
	if(tmpFile==NULL)
	{
		SwrxLog(NGENE_LOG_SEVERITY_WARNING,"Error in <%s>: Galileo almanac file not existing.",cfgFilename);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	fclose(tmpFile);
	//
	filenameGpsLen = strlen(filenameGps)+1;
	warmCfg->almanacPathGps = (char *)malloc(filenameGpsLen*sizeof(char));
	filenameGalLen = strlen(filenameGal)+1;
	warmCfg->almanacPathGal = (char *)malloc(filenameGalLen*sizeof(char));
	if((warmCfg->almanacPathGps==NULL) || (warmCfg->almanacPathGal==NULL))
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Malloc error in ManageWarmStartConfiguration");
		CloseNGene(cfg,EXIT_FAILURE);
	}
	memcpy(warmCfg->almanacPathGps,filenameGps,filenameGpsLen);
	memcpy(warmCfg->almanacPathGal,filenameGal,filenameGalLen);
	/**/
	strcpy(tag, "Known latitude");
	if(GetVal(cfgFilename,tag,(void *)&(warmCfg->warmLatitude),F_DOUBLE,1) == FAILURE_RETURN)
	{
		warmCfg->enableWarmStart=FALSE_FLAG;
		SwrxLog(NGENE_LOG_SEVERITY_WARNING,"Error in <%s>: Position not configured. Warm start disabled.",cfgFilename);
		return FAILURE_RETURN;
	}
	/**/
	strcpy(tag, "Known longitude");
	if(GetVal(cfgFilename,tag,(void *)&(warmCfg->warmLongitude),F_DOUBLE,1) == FAILURE_RETURN)
	{
		warmCfg->enableWarmStart=FALSE_FLAG;
		SwrxLog(NGENE_LOG_SEVERITY_WARNING,"Error in <%s>: Position not configured. Warm start disabled.",cfgFilename);
		return FAILURE_RETURN;
	}
	/**/
	strcpy(tag, "UTC date DD MM YYYY");
	if(GetVal(cfgFilename,tag,(void *)tmp,F_INTEGER,3) == FAILURE_RETURN)
	{
		warmCfg->enableWarmStart=FALSE_FLAG;
		SwrxLog(NGENE_LOG_SEVERITY_WARNING,"Error in <%s>: Date not configured. Warm start disabled.",cfgFilename);
		return FAILURE_RETURN;
	}
	warmCfg->warmDay = tmp[0];
	warmCfg->warmMonth = tmp[1];
	warmCfg->warmYear = tmp[2];
	/**/
	strcpy(tag, "UTC hour HH MM SS");
	if(GetVal(cfgFilename,tag,(void *)tmp,F_INTEGER,3) == FAILURE_RETURN)
	{
		warmCfg->enableWarmStart=FALSE_FLAG;
		SwrxLog(NGENE_LOG_SEVERITY_WARNING,"Error in <%s>: Time not configured. Warm start disabled.",cfgFilename);
		return FAILURE_RETURN;
	}
	warmCfg->warmHour = tmp[0];
	warmCfg->warmMinute = tmp[1];
	warmCfg->warmSecond = tmp[2];
	/**/
	strcpy(tag, "Minimum Elevation");
	if(GetVal(cfgFilename,tag,(void *)&(warmCfg->minElevation),F_DOUBLE,1) == FAILURE_RETURN)
	{
		warmCfg->minElevation = 0.0;
	}
	if((warmCfg->minElevation<-90.0) || (warmCfg->minElevation>90.0))
	{
		warmCfg->minElevation = 0.0;
	}
	warmCfg->minElevation *= DEGREE_TO_RADIAN;
	/**/
	strcpy(tag, "Prediction update interval");
	if(GetVal(cfgFilename,tag,(void *)&(warmCfg->updateInterval),F_INTEGER,1) == FAILURE_RETURN)
	{
		warmCfg->enableWarmStart=FALSE_FLAG;
		SwrxLog(NGENE_LOG_SEVERITY_WARNING,"Error in <%s>: Update interval not configured. Warm start disabled.",cfgFilename);
		return FAILURE_RETURN;
	}

	/* complete with the remaining parameters */
	geodPos[0] = warmCfg->warmLongitude;
	geodPos[1] = warmCfg->warmLatitude;
	geodPos[2] = 0.0;
	GeodToEcef(geodPos,warmCfg->warmEcef);
	//
	refTime.day = warmCfg->warmDay;
	refTime.month = (TMonth)warmCfg->warmMonth;
	refTime.year = warmCfg->warmYear;
	refTime.hour = warmCfg->warmHour;
	refTime.minute = warmCfg->warmMinute;
	refTime.second = (double)warmCfg->warmSecond;
	FromDateToGnssTime(GPS_SAT,&(warmCfg->gpsWn),&(warmCfg->gpsTow),&refTime);
	FromDateToGnssTime(GALILEO_SAT,&(warmCfg->galWn),&(warmCfg->galTow),&refTime);
	return SUCCESS_RETURN;
}


void DeleteWarmStartConfiguration(TNGeneConfiguration *cfg)
{
	if(cfg->warmConfiguration.almanacPathGps!=NULL)
	{
		free(cfg->warmConfiguration.almanacPathGps);
		cfg->warmConfiguration.almanacPathGps = NULL;
	}
	if(cfg->warmConfiguration.almanacPathGal!=NULL)
	{
		free(cfg->warmConfiguration.almanacPathGal);
		cfg->warmConfiguration.almanacPathGal = NULL;
	}
}
