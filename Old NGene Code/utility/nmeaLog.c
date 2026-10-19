/**
 * @file nmeaLog.c
 *
 * @author Mario Nicola
 *
 * @date 21/08/2020
 *
 * @version 1.0.0
 *
 * @brief Functions used to save a log in the NMEA format.
 *        See https://gpsd.gitlab.io/gpsd/NMEA.html for reference.
 */

#include <ctype.h>
#include <string.h>
#include <math.h>
#include "nmeaLog.h"
#include "ngeneDataStructure.h"
#include "ngeneConfiguration.h"
#include "ioFunctions.h"
#include "timeConversions.h"
#include "dop.h"
#include "pvtUtility.h"


static void FromDegreeToDm(double angle,int *deg,double *min)
{
	int sign = (angle>=0.0) ? 1 : -1 ;

	angle *= sign;
	//
	*deg = (int)floor(angle);
	angle -= (double)*deg;
	*min = angle*60;
	//
	*deg *= sign;
}


static void ConcatenateChecksum(char *string)
{
	int idx;
	int len = strlen(string);
	unsigned char checkSum = 0x00;

	for(idx=1;idx<len;idx++)
	{
		if((string[idx]!='$') && (string[idx]!='*'))
			checkSum ^= (unsigned char)string[idx];
	}
	sprintf(&(string[len]),"%02x",checkSum);
	string[len+2]='\0';
}


static int GetNmeaSatId(TSatConstellation satType,int satId)
{
	if(satType==GALILEO_SAT)
		return (satId + 300);
	else if(satType==EGNOS_SAT)
		return (satId - 87);
	else
		return satId;

}


int ManageNmeaConfiguration(TNGeneConfiguration *cfg,char *cfgFilename)
{
	TNGeneLogConfiguration *logCfg = &(cfg->logConfiguration);
	char tag[MAX_TAG_LEN];
	char string[MAX_ROW_LENGTH];
	int len;
	int idx;

	strcpy(tag, "Enable NMEA");
	if(GetVal(cfgFilename,tag,(void *)&(logCfg->enableNmea),F_INTEGER,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"Error while parsing configuration file <%s>: tag <%s> not found. Default value used.",cfgFilename,tag);
		logCfg->enableNmea = FALSE_FLAG;
	}
	else
	{
		logCfg->enableNmea = (logCfg->enableNmea>0) ? TRUE_FLAG : FALSE_FLAG;
	}
	/**/
	strcpy(tag, "NMEA File Name");
	if(GetVal(cfgFilename,tag,(void *)logCfg->nmeaFilename,F_STRING,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"Error while parsing configuration file <%s>: tag <%s> not found. Default value used.",cfgFilename,tag);
		memcpy(logCfg->nmeaFilename,NMEA_DEFAULT_FILENAME,strlen(NMEA_DEFAULT_FILENAME));
		logCfg->nmeaFilename[strlen(NMEA_DEFAULT_FILENAME)+1] = '\0';
	}
	// check if it is auto
	len = strlen(logCfg->nmeaFilename);
	memcpy(string,logCfg->nmeaFilename,len+1); // copy also '\0'
	for(idx=0;idx<len;idx++)
	{
		string[idx] = tolower(string[idx]);
	}
	if(strcmp(string,"auto")==0)
	{
		logCfg->nmeaAutomaticFilename = TRUE_FLAG;
	}
	else
	{
		logCfg->nmeaAutomaticFilename = FALSE_FLAG;
	}
	/**/
	strcpy(tag, "NMEA Talker ID");
	if(GetVal(cfgFilename,tag,(void *)string,F_STRING,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"Error while parsing configuration file <%s>: tag <%s> not found. Default value used.",cfgFilename,tag);
		memcpy(logCfg->nmeaTalkerId,NMEA_DEFAULT_TALKER_ID,NMEA_TALKER_ID_LEN);
		logCfg->nmeaTalkerId[NMEA_TALKER_ID_LEN] = '\0';
	}
	else
	{
		len = strlen(string);
		for(idx=0;idx<len;idx++)
		{
			string[idx] = toupper(string[idx]);
		}
		if((len<NMEA_TALKER_ID_LEN) || (string[0]=='P' && len<NMEA_TALKER_ID_LEN))
		{
			memcpy(logCfg->nmeaTalkerId,NMEA_DEFAULT_TALKER_ID,NMEA_TALKER_ID_LEN);
			logCfg->nmeaTalkerId[NMEA_TALKER_ID_LEN] = '\0';
		}
		else
		{
			memcpy(logCfg->nmeaTalkerId,string,len);
			if(string[0]=='P')
			{
				logCfg->nmeaTalkerId[NMEA_PROPRIETARY_ID_LEN] = '\0';
			}
			else
			{
				logCfg->nmeaTalkerId[NMEA_TALKER_ID_LEN] = '\0';
			}
		}
	}
	return SUCCESS_RETURN;
}



void NmeaSavePvt(TNGeneConfiguration *cfg,TNGeneData *rx)
{
	TNGeneLogConfiguration *logCfg = &(cfg->logConfiguration);
	TPvtResults *pvtRes = &(rx->pvtResults);
	TPvtData *pvtData = &(rx->pvtData);
	TPvtChannel *pvtCh;
	TChannel *ch;
	TSatStatus *satStatus;
	int numOpCh = pvtData->numOfGalileoOpChannels + pvtData->numOfGpsOpChannels;
	char string[3*MAX_ROW_LENGTH];
	TDateTime date;
	int deg;
	double min;
	double geoidalSeparation;
	int idx;
	int numGsv;
	int idxGsv;
	int elevation;
	int azimuth;
	int cno;

	/* preliminary checks */
	if(logCfg->enableNmea==FALSE_FLAG)
	{
		return;
	}
	if(pvtRes->rxUtcTime.referenceUpdated==FALSE_FLAG)
	{
		// UTC time is needed!
		return;
	}

	/* get UTC date/time */
	if(pvtRes->utcFromGps==TRUE_FLAG)
	{
		FromGnssTimeToDate(GPS_SAT,pvtRes->rxUtcTime.referenceWn,pvtRes->rxUtcTime.referenceTow,&date);
	}
	else if(pvtRes->utcFromGal==TRUE_FLAG)
	{
		FromGnssTimeToDate(GALILEO_SAT,pvtRes->rxUtcTime.referenceWn,pvtRes->rxUtcTime.referenceTow,&date);
	}
	else
	{
		// cannot find the current date
		return;
	}

	/* start the new file */
	if(rx->nmeaFile==NULL)
	{
		if(logCfg->nmeaAutomaticFilename==FALSE_FLAG)
		{
			sprintf(string,"%s/%s",cfg->dumpPathCurrent,logCfg->nmeaFilename);
		}
		else
		{
			if(cfg->inputMethod==INPUT_FILE)
			{
				sprintf(string,"%s/%s_FILE_%04d%02d%02d_%02d%02d%02d.nmea",
						cfg->dumpPathCurrent,cfg->fePars->name,date.year,date.month,date.day,date.hour,date.minute,(int)floor(date.second));
			}
			else
			{
				sprintf(string,"%s/%s_USB_%04d%02d%02d_%02d%02d%02d.nmea",
						cfg->dumpPathCurrent,cfg->fePars->name,date.year,date.month,date.day,date.hour,date.minute,(int)floor(date.second));
			}
		}
		rx->nmeaFile = fopen(string,"w");
		if(rx->nmeaFile==NULL)
		{
			// something went wrong, disable NMEA log
			logCfg->enableNmea=FALSE_FLAG;
			return;
		}
	}

	/* write the data */
	// NB each row must terminate with \r\n i.e. <CR><LF> (0x0D 0x0A)
	// GGA
	sprintf(string,"$%sGGA,%02d%02d%05.2f,",logCfg->nmeaTalkerId,date.hour,date.minute,date.second);
	FromDegreeToDm(pvtRes->geodPosition[1], &deg, &min);
	if(deg>=0)
		sprintf(&(string[strlen(string)]),"%02d%05.2f,N,",deg,min);
	else
		sprintf(&(string[strlen(string)]),"%02d%05.2f,S,",-deg,min);
	FromDegreeToDm(pvtRes->geodPosition[0], &deg, &min);
	if(deg>=0)
		sprintf(&(string[strlen(string)]),"%02d%05.2f,E,",deg,min);
	else
		sprintf(&(string[strlen(string)]),"%02d%05.2f,W,",-deg,min);
	geoidalSeparation = GetWgs84Separation(pvtRes->geodPosition[1],pvtRes->geodPosition[0]);
	// age of differential data and differential reference station ID are not available
	sprintf(&(string[strlen(string)]),"1,%02d,%.2f,%.2f,M,%.2f,M,,*",numOpCh,pvtRes->dop[HDOP_INDEX_IN_DOP_ARRAY],pvtRes->geodPosition[2],geoidalSeparation);
	ConcatenateChecksum(string);
	fprintf(rx->nmeaFile,"%s\r\n",string);
	// GSV
	numGsv = (int)ceil((float)numOpCh/(float)NMEA_SAT_PER_GSV_MSG);
	idx = 0;
	idxGsv = 1;
	while(idx<numOpCh)
	{
		sprintf(string,"$%sGSV,%d,%d,%02d",logCfg->nmeaTalkerId,numGsv,idxGsv,numOpCh);
		do
		{
			pvtCh = &(pvtData->pvtChArray[idx]);
			ch = pvtCh->ch;
			satStatus = pvtCh->satStatus;
			elevation = (int)floor(satStatus->elevation*RADIAN_TO_DEGREE);
			azimuth = (int)floor(satStatus->azimuth*RADIAN_TO_DEGREE);
			cno = (int)floor(ch->tracking.trkStatusChecker.cnoEstimator.estimatedCNo);
			sprintf(&(string[strlen(string)]),",%03d,%02d,%03d,%02d",GetNmeaSatId(ch->satType,ch->satId),elevation,azimuth,cno);
			idx++;
		} while((idx<numOpCh) && ((idx%NMEA_SAT_PER_GSV_MSG)!=0));
		sprintf(&(string[strlen(string)]),"*");
		ConcatenateChecksum(string);
		fprintf(rx->nmeaFile,"%s\r\n",string);
		idxGsv++;
	}
}
