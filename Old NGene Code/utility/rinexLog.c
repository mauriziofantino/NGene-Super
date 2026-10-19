/**
 * @file rinexLog.c
 *
 * @author Mario Nicola
 *
 * @date 05/02/2018
 *
 * @version 1.0.0
 *
 * @brief Functions used to save a log in the RINEX format.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include "rinexLog.h"
#include "timeConversions.h"
#include "ngeneConfiguration.h"
#include "ioFunctions.h"
#include "pvtChecks.h"
#include "ngeneDataStructure.h"




static void SubstituteChar(char *s,char old,char new)
{
	int idx;

	for(idx=0;idx<(int)strlen(s);idx++)
	{
		if(s[idx]==old)
		{
			s[idx]=new;
		}
	}
}


static double GpsSisaToMeters(double sisa)
{
	double accuracy;

	if(sisa==1)
	{
		accuracy = 2.8;
	}
	else if(sisa==3)
	{
		accuracy = 5.7;
	}
	else if(sisa==5)
	{
		accuracy = 11.3;
	}
	else if(sisa<=6)
	{
		accuracy = pow(2.0,1.0+(sisa/2.0));
	}
	else if(sisa<=15)
	{
		accuracy = pow(2.0,sisa-2.0);
	}
	else
	{
		accuracy = 0.0;
	}
	return accuracy;
}


static double GalSisaToMeters(double sisa)
{
	double accuracy;

	if((sisa>=0) && (sisa<=49))
	{
		/* 0 to 49 cm with 1 cm resolution */
		accuracy = sisa/100;
	}
	else if((sisa>=50) && (sisa<=74))
	{
		/* 50 cm to 0.98 m with 2 cm resolution */
		accuracy = 0.5 + ((sisa-50)*0.02);
	}
	else if((sisa>=75) && (sisa<=99))
	{
		/* 1 m to 2 m with 4 cm resolution */
		accuracy = 1.0 + ((sisa-75)*0.04);
	}
	else if((sisa>=100) && (sisa<=125))
	{
		/* 2 m to 6 m with 16 cm resolution */
		accuracy = 2.0 + ((sisa-100)*0.16);
	}
	else
	{
		accuracy = 0.0;
	}
	return accuracy;
}

static int MarkerTypeValid(char *markerType)
{
	if(	(strcmp(markerType,"GEODETIC            ")!=0) &&
		(strcmp(markerType,"NON_GEODETIC        ")!=0) &&
		(strcmp(markerType,"NON_PHYSICAL        ")!=0) &&
		(strcmp(markerType,"SPACEBORNE          ")!=0) &&
		(strcmp(markerType,"AIRBORNE            ")!=0) &&
		(strcmp(markerType,"WATER_CRAFT         ")!=0) &&
		(strcmp(markerType,"GROUND_CRAFT        ")!=0) &&
		(strcmp(markerType,"FIXED_BUOY          ")!=0) &&
		(strcmp(markerType,"FLOATING_BUOY       ")!=0) &&
		(strcmp(markerType,"FLOATING_ICE        ")!=0) &&
		(strcmp(markerType,"GLACIER             ")!=0) &&
		(strcmp(markerType,"BALLISTIC           ")!=0) &&
		(strcmp(markerType,"ANIMAL              ")!=0) &&
		(strcmp(markerType,"HUMAN               ")!=0))
		return FALSE_FLAG;
	else
		return TRUE_FLAG;
}

static void LoadRinexParamString(char *cfgFilename,char *tag,char *defaultString,int desiredLen,char *dest,int spaceAllowed)
{
	TFieldType typeOfString;
	char string[MAX_ROW_LENGTH];
	int idx;

	// are spaces allowed?
	if(spaceAllowed==TRUE_FLAG)
	{
		typeOfString = F_STRING_WITH_SPACES;
	}
	else
	{
		typeOfString = F_STRING;
	}
	// read and manage the length
	if(GetVal(cfgFilename,tag,(void *)string,typeOfString,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"Error while parsing configuration file <%s>: tag <%s> not found. Default value used.",cfgFilename,tag);
		memcpy(dest,defaultString,desiredLen);
	}
	else if(strlen(string)<(unsigned int)desiredLen)
	{
		// The string is short, fill up to the required using spaces
		for(idx=strlen(string);idx<desiredLen;idx++)
		{
			string[idx] = ' ';
		}
		memcpy(dest,string,desiredLen);
	}
	else
	{
		memcpy(dest,string,desiredLen);
	}
	dest[desiredLen] = '\0';
}



int ManageRinexConfiguration(TNGeneConfiguration *cfg,char *cfgFilename)
{
	TNGeneLogConfiguration *logCfg = &(cfg->logConfiguration);
	char tag[MAX_TAG_LEN];
	char string[MAX_ROW_LENGTH];
	int idx;

	strcpy(tag, "RINEX Station Name");
	if(GetVal(cfgFilename,tag,(void *)string,F_STRING,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"Error while parsing configuration file <%s>: tag <%s> not found. Default value used.",cfgFilename,tag);
		memcpy(logCfg->rinexStation,DEFAULT_STATION_NAME,RINEX_STATION_NAME_LEN);
	}
	else if(strlen(string)!=RINEX_STATION_NAME_LEN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"Error while parsing configuration file <%s>: <%s> with invalid length. Default value used.",cfgFilename,tag);
		memcpy(logCfg->rinexStation,DEFAULT_STATION_NAME,RINEX_STATION_NAME_LEN);
	}
	else
	{
		strncpy(logCfg->rinexStation,string,RINEX_STATION_NAME_LEN);
	}
	logCfg->rinexStation[RINEX_STATION_NAME_LEN] = '\0';
	// adjust the case
	for(idx=0;idx<RINEX_STATION_NAME_LEN;idx++)
	{
		logCfg->rinexStation[idx] = tolower(logCfg->rinexStation[idx]);
	}
	/**/
	LoadRinexParamString(cfgFilename,"RINEX Agency Name",DEFAULT_AGENCY_NAME,RINEX_AGENCY_NAME_LEN,logCfg->rinexAgency,TRUE_FLAG);
	LoadRinexParamString(cfgFilename,"RINEX Marker Name",DEFAULT_RINEX_STRING,RINEX_MARKER_NAME_LEN,logCfg->rinexMarkerName,TRUE_FLAG);
	LoadRinexParamString(cfgFilename,"RINEX Marker Type",DEFAULT_MARKER_TYPE,RINEX_MARKER_TYPE_LEN,logCfg->rinexMarkerType,FALSE_FLAG);
	if(MarkerTypeValid(logCfg->rinexMarkerType)==FALSE_FLAG)
	{
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"Invalid RINEX Marker Type in the configuration file <%s>. Default value used.",cfgFilename);
		memcpy(logCfg->rinexMarkerType,DEFAULT_MARKER_TYPE,RINEX_MARKER_TYPE_LEN);
		logCfg->rinexMarkerType[RINEX_MARKER_TYPE_LEN] = '\0';
	}
	LoadRinexParamString(cfgFilename,"RINEX Observer",DEFAULT_RINEX_STRING,RINEX_OBSERVER_LEN,logCfg->rinexObserver,TRUE_FLAG);
	LoadRinexParamString(cfgFilename,"RINEX Agency 2",DEFAULT_RINEX_STRING,RINEX_AGENCY_LEN,logCfg->rinexAgency2,TRUE_FLAG);
	LoadRinexParamString(cfgFilename,"RINEX Receiver Number",DEFAULT_RINEX_STRING,RINEX_REC_NUM_LEN,logCfg->rinexReceiverNumber,TRUE_FLAG);
	LoadRinexParamString(cfgFilename,"RINEX Antenna Number",DEFAULT_RINEX_STRING,RINEX_ANT_NUM_LEN,logCfg->rinexAntennaNumber,TRUE_FLAG);
	LoadRinexParamString(cfgFilename,"RINEX Antenna Type",DEFAULT_RINEX_STRING,RINEX_ANT_TYPE_LEN,logCfg->rinexAntennaType,TRUE_FLAG);
	/**/
	strcpy(tag, "RINEX Antenna Delta");
	if(GetVal(cfgFilename,tag,(void *)&(logCfg->rinexAntennaDelta),F_FLOAT,3) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"Error while parsing configuration file <%s>: tag <%s> not found. Default value used.",cfgFilename,tag);
		logCfg->rinexAntennaDelta[0] = 0.0;
		logCfg->rinexAntennaDelta[1] = 0.0;
		logCfg->rinexAntennaDelta[2] = 0.0;
	}
	/**/
	strcpy(tag, "Enable RINEX NAV");
	if(GetVal(cfgFilename,tag,(void *)&(logCfg->enableRinexNav),F_INTEGER,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"Error while parsing configuration file <%s>: tag <%s> not found. Default value used.",cfgFilename,tag);
		logCfg->enableRinexNav = FALSE_FLAG;
	}
	else 
	{
		logCfg->enableRinexNav = (logCfg->enableRinexNav>0) ? TRUE_FLAG : FALSE_FLAG;
	}
	/**/
	strcpy(tag, "Enable RINEX OBS");
	if(GetVal(cfgFilename,tag,(void *)&(logCfg->enableRinexObs),F_INTEGER,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"Error while parsing configuration file <%s>: tag <%s> not found. Default value used.",cfgFilename,tag);
		logCfg->enableRinexObs = FALSE_FLAG;
	}
	else 
	{
		logCfg->enableRinexObs = (logCfg->enableRinexObs>0) ? TRUE_FLAG : FALSE_FLAG;
	}
	/**/
	return SUCCESS_RETURN;
}




void InitRinexLog(TRinexLog *rinex)
{
	rinex->currDayNav = INVALID_VALUE;
	rinex->navFile = NULL;
	rinex->ionoSaved = FALSE_FLAG;
	rinex->gautSaved = FALSE_FLAG;
	rinex->gputSaved = FALSE_FLAG;
	rinex->gpgaSaved = FALSE_FLAG;
	rinex->currDayObs = INVALID_VALUE;
	rinex->obsFile = NULL;
}



void DeinitRinexLog(TRinexLog *rinex)
{
	if(rinex->navFile!=NULL)
	{
		fclose(rinex->navFile);
	}
	if(rinex->obsFile!=NULL)
	{
		fclose(rinex->obsFile);
	}
}


int CheckNewDayNumberNav(TNGeneConfiguration *cfg,TRinexLog *rinex,TChannel *ch,TNavigationMessage *navMsg)
{
	TNGeneLogConfiguration *logCfg = &(cfg->logConfiguration);
	TDateTime date;
	int newDay;

	if(logCfg->enableRinexNav==FALSE_FLAG)
	{
		return FALSE_FLAG;
	}
	if(rinex->navFile==NULL)
	{
		return FALSE_FLAG;
	}
	if((navMsg->towValid==TRUE_FLAG) && (navMsg->wnValid==TRUE_FLAG))
	{
		FromGnssTimeToDate(ch->satType,navMsg->wn,navMsg->tow,&date);
		newDay = FromDateToDayNumber(&date);
		if(newDay==rinex->currDayNav)
		{
			return FALSE_FLAG;
		}
		else
		{
			return TRUE_FLAG;
		}
	}
	return FALSE_FLAG;
}


void RinexSaveGpsNav(TNGeneConfiguration *cfg,TRinexLog *rinex,TChannel *ch,TNavigationMessage *navMsg)
{
	TNGeneLogConfiguration *logCfg = &(cfg->logConfiguration);
	FILE *navFile = rinex->navFile;
	int startNewFile = FALSE_FLAG;
	char row[82];
	TDateTime date;
	double accuracy;

	/* preliminary checks */
	if(logCfg->enableRinexNav==FALSE_FLAG)
	{
		return;
	}
	if(ReadyForPvt(ch)==FALSE_FLAG)
	{
		return;
	}

	/* check if a new file must be open */
	if(navFile==NULL)
	{
		startNewFile = TRUE_FLAG;
	}
	else if(CheckNewDayNumberNav(cfg,rinex,ch,navMsg)==TRUE_FLAG)
	{
		startNewFile = TRUE_FLAG;
		fclose(navFile);
	}

	/* start the new file */
	if(startNewFile==TRUE_FLAG)
	{
		FromGnssTimeToDate(GPS_SAT,navMsg->wn,navMsg->tow,&date);
		rinex->currDayNav = FromDateToDayNumber(&date);
		/* create file */
		sprintf(rinex->filenameNav,"%s/%s%03d0.%02dn",cfg->dumpPath,logCfg->rinexStation,rinex->currDayNav,date.year%100);
		navFile = fopen(rinex->filenameNav,"w");
		rinex->navFile = navFile;
		if(navFile==NULL)
		{
			// cannot create the RINEX file, disable the log
			logCfg->enableRinexNav = FALSE_FLAG;
			return;
		}
		/* HEADER */
		fprintf(navFile,"%9.2f           N                   G                   RINEX VERSION / TYPE\n",3.00);
		fprintf(navFile,"%s%s%4d%02d%02d %02d%02d%02d UTC PGM / RUN BY / DATE\n",
				DEFAULT_PROGRAM_NAME,logCfg->rinexAgency,date.year,date.month,date.day,date.hour,date.minute,(int)date.second);
		/* Iono corrections */
		if(navMsg->ionoReceived==TRUE_FLAG)
		{
			sprintf(row,"GPSA %12.4e%12.4e%12.4e%12.4e       IONOSPHERIC CORR\n",navMsg->iono.alphaGps[0],navMsg->iono.alphaGps[1],navMsg->iono.alphaGps[2],navMsg->iono.alphaGps[3]);
			SubstituteChar(row,'e',CHAR_FOR_EXPONENTIAL);
			fprintf(navFile,"%s",row);
			sprintf(row,"GPSB %12.4e%12.4e%12.4e%12.4e       IONOSPHERIC CORR\n",navMsg->iono.beta[0],navMsg->iono.beta[1],navMsg->iono.beta[2],navMsg->iono.beta[3]);
			SubstituteChar(row,'e',CHAR_FOR_EXPONENTIAL);
			fprintf(navFile,"%s",row);
			rinex->ionoSaved = TRUE_FLAG;
		}
		else
		{
			fgetpos(navFile,&(rinex->ionoPosition));
			sprintf(row,"GPSA IONO CORRECTION NOT AVAILABLE                          COMMENT         \n");
			fprintf(navFile,"%s",row);
			sprintf(row,"GPSB IONO CORRECTION NOT AVAILABLE                          COMMENT         \n");
			fprintf(navFile,"%s",row);
			rinex->ionoSaved = FALSE_FLAG;
		}
		/* GPS-UTC */
		if(navMsg->gnss2UtcCompleted==TRUE_FLAG)
		{
			sprintf(row,"GPUT %17.10e%16.9e%7d%5d          TIME SYSTEM CORR\n",navMsg->gnss2Utc.a0,navMsg->gnss2Utc.a1,navMsg->gnss2Utc.t0T,navMsg->gnss2Utc.wn0T);
			SubstituteChar(row,'e',CHAR_FOR_EXPONENTIAL);
			fprintf(navFile,"%s",row);
			rinex->gputSaved = TRUE_FLAG;
		}
		else
		{
			fgetpos(navFile,&(rinex->gputPosition));
			sprintf(row,"GPUT PARAMETERS NOT AVAILABLE                               COMMENT         \n");
			fprintf(navFile,"%s",row);
			rinex->gputSaved = FALSE_FLAG;
		}
		/**/
		fprintf(navFile,"                                                            END OF HEADER\n");
	}

	/* SV / EPOCH / SV CLK */
	FromGnssTimeToDate(GPS_SAT,navMsg->wn,navMsg->clk.toc,&date);
	sprintf(row,"G%02d %4d %02d %02d %02d %02d %02d%19.12e%19.12e%19.12e\n",
			ch->satId,date.year,date.month,date.day,date.hour,date.minute,(int)date.second,navMsg->clk.af[0],navMsg->clk.af[1],navMsg->clk.af[2]);
	SubstituteChar(row,'e',CHAR_FOR_EXPONENTIAL);
	fprintf(navFile,"%s",row);
	/* BROADCAST ORBIT 1 */
	sprintf(row,"    %19.12e%19.12e%19.12e%19.12e\n",(double)navMsg->eph.iode2,navMsg->eph.capitalCrs,navMsg->eph.deltaN,navMsg->eph.meanAnomaly0);
	SubstituteChar(row,'e',CHAR_FOR_EXPONENTIAL);
	fprintf(navFile,"%s",row);
	/* BROADCAST ORBIT 2 */
	sprintf(row,"    %19.12e%19.12e%19.12e%19.12e\n",navMsg->eph.capitalCuc,navMsg->eph.eccentricity,navMsg->eph.capitalCus,navMsg->eph.sqrtA);
	SubstituteChar(row,'e',CHAR_FOR_EXPONENTIAL);
	fprintf(navFile,"%s",row);
	/* BROADCAST ORBIT 3 */
	sprintf(row,"    %19.12e%19.12e%19.12e%19.12e\n",navMsg->eph.toe,navMsg->eph.capitalCic,navMsg->eph.capitalOmega0,navMsg->eph.capitalCis);
	SubstituteChar(row,'e',CHAR_FOR_EXPONENTIAL);
	fprintf(navFile,"%s",row);
	/* BROADCAST ORBIT 4 */
	sprintf(row,"    %19.12e%19.12e%19.12e%19.12e\n",navMsg->eph.i0,navMsg->eph.capitalCrc,navMsg->eph.omega,navMsg->eph.capitalOmegaDot);
	SubstituteChar(row,'e',CHAR_FOR_EXPONENTIAL);
	fprintf(navFile,"%s",row);
	/* BROADCAST ORBIT 5 */
	sprintf(row,"    %19.12e%19.12e%19.12e%19.12e\n",(double)navMsg->eph.iDot,1.0,(double)navMsg->wn,0.0);
	SubstituteChar(row,'e',CHAR_FOR_EXPONENTIAL);
	fprintf(navFile,"%s",row);
	/* BROADCAST ORBIT 6 */
	accuracy = GpsSisaToMeters((double)navMsg->eph.sisa);
	sprintf(row,"    %19.12e%19.12e%19.12e%19.12e\n",accuracy,0.0,navMsg->clk.tGroupDelay,(double)navMsg->clk.iodc);
	SubstituteChar(row,'e',CHAR_FOR_EXPONENTIAL);
	fprintf(navFile,"%s",row);
	/* BROADCAST ORBIT 7 */
	sprintf(row,"    %19.12e%19.12e\n",(double)navMsg->tow,4.0);
	SubstituteChar(row,'e',CHAR_FOR_EXPONENTIAL);
	fprintf(navFile,"%s",row);
}

void RinexAddGpsIono(TNGeneConfiguration *cfg,TRinexLog *rinex,TNavigationMessage *navMsg)
{
	TNGeneLogConfiguration *logCfg = &(cfg->logConfiguration);
	FILE *navFile = rinex->navFile;
	fpos_t currPos;
	char row[82];

	if(logCfg->enableRinexNav==FALSE_FLAG)
	{
		return;
	}

	if(navFile==NULL)
	{
		/* The RINEX file does not exist: cannot save iono.
		   The RINEX file will be created by RinexSaveGpsNav */
		return;
	}

	if((rinex->ionoSaved==FALSE_FLAG) && (navMsg->ionoReceived==TRUE_FLAG))
	{
		/* if the IONO corrections are available for the first time, save them into the header */
		fgetpos(navFile,&currPos);
		fsetpos(navFile,&(rinex->ionoPosition));
		sprintf(row,"GPSA %12.4e%12.4e%12.4e%12.4e       IONOSPHERIC CORR\n",navMsg->iono.alphaGps[0],navMsg->iono.alphaGps[1],navMsg->iono.alphaGps[2],navMsg->iono.alphaGps[3]);
		SubstituteChar(row,'e',CHAR_FOR_EXPONENTIAL);
		fprintf(navFile,"%s",row);
		sprintf(row,"GPSB %12.4e%12.4e%12.4e%12.4e       IONOSPHERIC CORR\n",navMsg->iono.beta[0],navMsg->iono.beta[1],navMsg->iono.beta[2],navMsg->iono.beta[3]);
		SubstituteChar(row,'e',CHAR_FOR_EXPONENTIAL);
		fprintf(navFile,"%s",row);
		rinex->ionoSaved = TRUE_FLAG;
		fsetpos(navFile,&currPos);
	}
}

void RinexAddGput(TNGeneConfiguration *cfg,TRinexLog *rinex,TNavigationMessage *navMsg)
{
	TNGeneLogConfiguration *logCfg = &(cfg->logConfiguration);
	FILE *navFile = rinex->navFile;
	fpos_t currPos;
	char row[82];

	if(logCfg->enableRinexNav==FALSE_FLAG)
	{
		return;
	}

	if(navFile==NULL)
	{
		/* The RINEX file does not exist: cannot save GPUT.
		   The RINEX file will be created by RinexSaveGpsNav */
		return;
	}

	if((rinex->gputSaved==FALSE_FLAG) && (navMsg->gnss2UtcCompleted==TRUE_FLAG))
	{
		/* if the GPS-UTC parameters are available for the first time, save them into the header */
		fgetpos(navFile,&currPos);
		fsetpos(navFile,&(rinex->gputPosition));
		sprintf(row,"GPUT %17.10e%16.9e%7d%5d          TIME SYSTEM CORR\n",navMsg->gnss2Utc.a0,navMsg->gnss2Utc.a1,navMsg->gnss2Utc.t0T,navMsg->gnss2Utc.wn0T);
		SubstituteChar(row,'e',CHAR_FOR_EXPONENTIAL);
		fprintf(navFile,"%s",row);
		rinex->gputSaved = TRUE_FLAG;
		fsetpos(navFile,&currPos);
	}
}


/*
 * NB
 * In the RINEX file, for GAUT and GPGA, the reference week number is aligned with GPS, so
 * the WN of Galileo is incremented by GPS_WN_TO_GAL_WN_BIAS (i.e. 1024) before the write operation
 */

void RinexSaveGalNav(TNGeneConfiguration *cfg,TRinexLog *rinex,TChannel *ch,TNavigationMessage *navMsg)
{
	TNGeneLogConfiguration *logCfg = &(cfg->logConfiguration);
	FILE *navFile = rinex->navFile;
	int startNewFile = FALSE_FLAG;
	char row[82];
	TDateTime date;
	double accuracy;
	int wn;

	/* preliminary checks */
	if(logCfg->enableRinexNav==FALSE_FLAG)
	{
		return;
	}
	if(ReadyForPvt(ch)==FALSE_FLAG)
	{
		return;
	}

	/* check if a new file must be open */
	if(navFile==NULL)
	{
		startNewFile = TRUE_FLAG;
	}
	else if(CheckNewDayNumberNav(cfg,rinex,ch,navMsg)==TRUE_FLAG)
	{
		startNewFile = TRUE_FLAG;
		fclose(navFile);
	}

	/* start the new file */
	if(startNewFile==TRUE_FLAG)
	{
		FromGnssTimeToDate(GALILEO_SAT,navMsg->wn,navMsg->tow,&date);
		rinex->currDayNav = FromDateToDayNumber(&date);
		/* create file */
		sprintf(rinex->filenameNav,"%s/%s%03d0.%02dl",cfg->dumpPath,logCfg->rinexStation,rinex->currDayNav,date.year%100);
		navFile = fopen(rinex->filenameNav,"w");
		rinex->navFile = navFile;
		if(navFile==NULL)
		{
			// cannot create the RINEX file, disable the log
			logCfg->enableRinexNav = FALSE_FLAG;
			return;
		}
		/* HEADER */
		fprintf(navFile,"%9.2f           N                   E                   RINEX VERSION / TYPE\n",3.00);
		fprintf(navFile,"%s%s%4d%02d%02d %02d%02d%02d UTC PGM / RUN BY / DATE\n",
				DEFAULT_PROGRAM_NAME,logCfg->rinexAgency,date.year,date.month,date.day,date.hour,date.minute,(int)date.second);
		/* Iono correction */
		if(navMsg->ionoReceived==TRUE_FLAG)
		{
			sprintf(row,"GAL  %12.4e%12.4e%12.4e%12.4e       IONOSPHERIC CORR\n",navMsg->iono.alphaGal[0],navMsg->iono.alphaGal[1],navMsg->iono.alphaGal[2],0.0);
			SubstituteChar(row,'e',CHAR_FOR_EXPONENTIAL);
			fprintf(navFile,"%s",row);
			rinex->ionoSaved = TRUE_FLAG;
		}
		else
		{
			fgetpos(navFile,&(rinex->ionoPosition));
			sprintf(row,"GAL  IONO CORRECTION NOT AVAILABLE                          COMMENT         \n");
			fprintf(navFile,"%s",row);
			rinex->ionoSaved = FALSE_FLAG;
		}
		/* Galileo-UTC */
		if(navMsg->gnss2UtcCompleted==TRUE_FLAG)
		{
			wn = (navMsg->gnss2Utc.wn0T+GPS_WN_TO_GAL_WN_BIAS)%100000; // maximum 5 digits (to avoid compilation warning...)
			sprintf(row,"GAUT %17.10e%16.9e%7d%5d          TIME SYSTEM CORR\n",navMsg->gnss2Utc.a0,navMsg->gnss2Utc.a1,navMsg->gnss2Utc.t0T,wn);
			SubstituteChar(row,'e',CHAR_FOR_EXPONENTIAL);
			fprintf(navFile,"%s",row);
			rinex->gautSaved = TRUE_FLAG;
		}
		else
		{
			fgetpos(navFile,&(rinex->gautPosition));
			sprintf(row,"GAUT PARAMETERS NOT AVAILABLE                               COMMENT         \n");
			fprintf(navFile,"%s",row);
			rinex->gautSaved = FALSE_FLAG;
		}
		/* Galileo-GPS */
		if(navMsg->gal2GpsCompleted==TRUE_FLAG)
		{
			wn = (navMsg->gal2Gps.wn0G+GPS_WN_TO_GAL_WN_BIAS)%100000; // maximum 5 digits (to avoid compilation warning...)
			sprintf(row,"GPGA %17.10e%16.9e%7d%5d          TIME SYSTEM CORR\n",navMsg->gal2Gps.a0G,navMsg->gal2Gps.a1G,navMsg->gal2Gps.t0G,wn);
			SubstituteChar(row,'e',CHAR_FOR_EXPONENTIAL);
			fprintf(navFile,"%s",row);
			rinex->gpgaSaved = TRUE_FLAG;
		}
		else
		{
			fgetpos(navFile,&(rinex->gpgaPosition));
			sprintf(row,"GPGA PARAMETERS NOT AVAILABLE                               COMMENT         \n");
			fprintf(navFile,"%s",row);
			rinex->gpgaSaved = FALSE_FLAG;
		}
		/**/
		fprintf(navFile,"                                                            END OF HEADER\n");
	}

	/* SV / EPOCH / SV CLK */
	FromGnssTimeToDate(GALILEO_SAT,navMsg->wn,navMsg->clk.toc,&date);
	sprintf(row,"E%02d %4d %02d %02d %02d %02d %02d%19.12e%19.12e%19.12e\n",
			ch->satId,date.year,date.month,date.day,date.hour,date.minute,(int)date.second,navMsg->clk.af[0],navMsg->clk.af[1],navMsg->clk.af[2]);
	SubstituteChar(row,'e',CHAR_FOR_EXPONENTIAL);
	fprintf(navFile,"%s",row);
	/* BROADCAST ORBIT 1 */
	sprintf(row,"    %19.12e%19.12e%19.12e%19.12e\n",(double)navMsg->eph.iode2,navMsg->eph.capitalCrs,navMsg->eph.deltaN,navMsg->eph.meanAnomaly0);
	SubstituteChar(row,'e',CHAR_FOR_EXPONENTIAL);
	fprintf(navFile,"%s",row);
	/* BROADCAST ORBIT 2 */
	sprintf(row,"    %19.12e%19.12e%19.12e%19.12e\n",navMsg->eph.capitalCuc,navMsg->eph.eccentricity,navMsg->eph.capitalCus,navMsg->eph.sqrtA);
	SubstituteChar(row,'e',CHAR_FOR_EXPONENTIAL);
	fprintf(navFile,"%s",row);
	/* BROADCAST ORBIT 3 */
	sprintf(row,"    %19.12e%19.12e%19.12e%19.12e\n",navMsg->eph.toe,navMsg->eph.capitalCic,navMsg->eph.capitalOmega0,navMsg->eph.capitalCis);
	SubstituteChar(row,'e',CHAR_FOR_EXPONENTIAL);
	fprintf(navFile,"%s",row);
	/* BROADCAST ORBIT 4 */
	sprintf(row,"    %19.12e%19.12e%19.12e%19.12e\n",navMsg->eph.i0,navMsg->eph.capitalCrc,navMsg->eph.omega,navMsg->eph.capitalOmegaDot);
	SubstituteChar(row,'e',CHAR_FOR_EXPONENTIAL);
	fprintf(navFile,"%s",row);
	/* BROADCAST ORBIT 5 */
	sprintf(row,"    %19.12e%19.12e%19.12e%19.12e\n",(double)navMsg->eph.iDot,513.0,(double)navMsg->wn + 1024.0,0.0); /* NB 513 => E1b */
	SubstituteChar(row,'e',CHAR_FOR_EXPONENTIAL);
	fprintf(navFile,"%s",row);
	/* BROADCAST ORBIT 6 */
	accuracy = GalSisaToMeters((double)navMsg->eph.sisa);
	sprintf(row,"    %19.12e%19.12e%19.12e%19.12e\n",accuracy,0.0,navMsg->clk.bgd_E1E5a,navMsg->clk.bgd_E1E5b);
	SubstituteChar(row,'e',CHAR_FOR_EXPONENTIAL);
	fprintf(navFile,"%s",row);
	/* BROADCAST ORBIT 7 */
	sprintf(row,"    %19.12e\n",(double)navMsg->tow);
	SubstituteChar(row,'e',CHAR_FOR_EXPONENTIAL);
	fprintf(navFile,"%s",row);
}

void RinexAddGalIono(TNGeneConfiguration *cfg,TRinexLog *rinex,TNavigationMessage *navMsg)
{
	TNGeneLogConfiguration *logCfg = &(cfg->logConfiguration);
	FILE *navFile = rinex->navFile;
	fpos_t currPos;
	char row[82];

	if(logCfg->enableRinexNav==FALSE_FLAG)
	{
		return;
	}

	if(navFile==NULL)
	{
		/* The RINEX file does not exist: cannot save iono.
		   The RINEX file will be created by RinexSaveGalNav */
		return;
	}

	if((rinex->ionoSaved==FALSE_FLAG) && (navMsg->ionoReceived==TRUE_FLAG))
	{
		/* if the IONO corrections are available for the first time, save them into the header */
		fgetpos(navFile,&currPos);
		fsetpos(navFile,&(rinex->ionoPosition));
		sprintf(row,"GAL  %12.4e%12.4e%12.4e%12.4e       IONOSPHERIC CORR\n",navMsg->iono.alphaGal[0],navMsg->iono.alphaGal[1],navMsg->iono.alphaGal[2],0.0);
		SubstituteChar(row,'e',CHAR_FOR_EXPONENTIAL);
		fprintf(navFile,"%s",row);
		rinex->ionoSaved = TRUE_FLAG;
		fsetpos(navFile,&currPos);
	}
}


void RinexAddGaut(TNGeneConfiguration *cfg,TRinexLog *rinex,TNavigationMessage *navMsg)
{
	TNGeneLogConfiguration *logCfg = &(cfg->logConfiguration);
	FILE *navFile = rinex->navFile;
	fpos_t currPos;
	char row[82];
	int wn;

	if(logCfg->enableRinexNav==FALSE_FLAG)
	{
		return;
	}

	if(navFile==NULL)
	{
		/* The RINEX file does not exist: cannot save GAUT.
		   The RINEX file will be created by RinexSaveGalNav */
		return;
	}

	if((rinex->gautSaved==FALSE_FLAG) && (navMsg->gnss2UtcCompleted==TRUE_FLAG))
	{
		/* if the Galileo-UTC parameters are available for the first time, save them into the header */
		fgetpos(navFile,&currPos);
		fsetpos(navFile,&(rinex->gautPosition));
		wn = (navMsg->gnss2Utc.wn0T+GPS_WN_TO_GAL_WN_BIAS)%100000; // maximum 5 digits (to avoid compilation warning...)
		sprintf(row,"GAUT %17.10e%16.9e%7d%5d          TIME SYSTEM CORR\n",navMsg->gnss2Utc.a0,navMsg->gnss2Utc.a1,navMsg->gnss2Utc.t0T,wn);
		SubstituteChar(row,'e',CHAR_FOR_EXPONENTIAL);
		fprintf(navFile,"%s",row);
		rinex->gautSaved = TRUE_FLAG;
		fsetpos(navFile,&currPos);
	}
}


void RinexAddGpga(TNGeneConfiguration *cfg,TRinexLog *rinex,TNavigationMessage *navMsg)
{
	TNGeneLogConfiguration *logCfg = &(cfg->logConfiguration);
	FILE *navFile = rinex->navFile;
	fpos_t currPos;
	char row[82];
	int wn;

	if(logCfg->enableRinexNav==FALSE_FLAG)
	{
		return;
	}

	if(navFile==NULL)
	{
		/* The RINEX file does not exist: cannot save GPGA.
		   The RINEX file will be created by RinexSaveGalNav */
		return;
	}

	if((rinex->gpgaSaved==FALSE_FLAG) && (navMsg->gal2GpsCompleted==TRUE_FLAG))
	{
		/* if the Galileo-UTC parameters are available for the first time, save them into the header */
		fgetpos(navFile,&currPos);
		fsetpos(navFile,&(rinex->gpgaPosition));
		wn = (navMsg->gal2Gps.wn0G+GPS_WN_TO_GAL_WN_BIAS)%100000; // maximum 5 digits (to avoid compilation warning...)
		sprintf(row,"GPGA %17.10e%16.9e%7d%5d          TIME SYSTEM CORR\n",navMsg->gal2Gps.a0G,navMsg->gal2Gps.a1G,navMsg->gal2Gps.t0G,wn);
		SubstituteChar(row,'e',CHAR_FOR_EXPONENTIAL);
		fprintf(navFile,"%s",row);
		rinex->gpgaSaved = TRUE_FLAG;
		fsetpos(navFile,&currPos);
	}
}




int CheckNewDayNumberObs(TNGeneConfiguration *cfg,TRinexLog *rinex,TSatConstellation refConst,int refWn,double refTow)
{
	TNGeneLogConfiguration *logCfg = &(cfg->logConfiguration);
	TDateTime date;
	int newDay;

	if(logCfg->enableRinexObs==FALSE_FLAG)
	{
		return FALSE_FLAG;
	}
	if(rinex->obsFile==NULL)
	{
		return FALSE_FLAG;
	}

	FromGnssTimeToDate(refConst,refWn,(int)floor(refTow),&date);
	newDay = FromDateToDayNumber(&date);
	if(newDay==rinex->currDayObs)
	{
		return FALSE_FLAG;
	}
	else
	{
		return TRUE_FLAG;
	}
}



void RinexSaveObs(TNGeneConfiguration *cfg,TNGeneData *rx)
{
	TPvtData *pvtData = &(rx->pvtData);
	TPvtResults *pvtRes = &(rx->pvtResults);
	TPvtChannel *pvtCh;
	TChannel *ch;
	TNGeneLogConfiguration *logCfg = &(cfg->logConfiguration);
	TRinexLog *rinex = &(rx->rinexGps);
	FILE *obsFile = rinex->obsFile;
	int numOpCh = pvtData->numOfGalileoOpChannels + pvtData->numOfGpsOpChannels;
	int startNewFile = FALSE_FLAG;
	int refWn;
	double refTow;
	TSatConstellation refConst;
	char row[82];
	TDateTime date;
	int idx;
	double fd;
	double carrierPhase;
	double cno;

	/* preliminary checks */
	if(logCfg->enableRinexObs==FALSE_FLAG)
	{
		return;
	}
	if((pvtRes->rxGpsTime.referenceUpdated==FALSE_FLAG) && (pvtRes->rxGalTime.referenceUpdated==FALSE_FLAG))
	{
		// if no time is available, save nothing
		return;
	}
	if(numOpCh==0)
	{
		// no data to be saved
		return;
	}

	/* Init the current time */
	if(pvtRes->rxGpsTime.referenceUpdated==TRUE_FLAG)
	{
		refWn = pvtRes->rxGpsTime.referenceWn;
		refTow = pvtRes->rxGpsTime.referenceTow;
		refConst = GPS_SAT;
	}
	else // if(pvtRes->rxGalTime.referenceUpdated==TRUE_FLAG)
	{
		refWn = pvtRes->rxGalTime.referenceWn;
		refTow = pvtRes->rxGalTime.referenceTow;
		refConst = GALILEO_SAT;
	}

	/* check if a new file must be open */
	if(obsFile==NULL)
	{
		startNewFile = TRUE_FLAG;
	}
	else if(CheckNewDayNumberObs(cfg,rinex,refConst,refWn,refTow)==TRUE_FLAG)
	{
		startNewFile = TRUE_FLAG;
		fclose(obsFile);
	}

	/* start the new file */
	if(startNewFile==TRUE_FLAG)
	{
		FromGnssTimeToDate(refConst,refWn,refTow,&date);
		rinex->currDayObs = FromDateToDayNumber(&date);
		/* create file */
		sprintf(rinex->filenameObs,"%s/%s%03d0.%02do",cfg->dumpPath,logCfg->rinexStation,rinex->currDayObs,date.year%100);
		obsFile = fopen(rinex->filenameObs,"w");
		rinex->obsFile = obsFile;
		if(obsFile==NULL)
		{
			// cannot create the RINEX file, disable the log
			logCfg->enableRinexObs = FALSE_FLAG;
			return;
		}
		/* HEADER */
		fprintf(obsFile,"%9.2f           O                   M                   RINEX VERSION / TYPE\n",3.00);
		fprintf(obsFile,"%s%s%4d%02d%02d %02d%02d%02d UTC PGM / RUN BY / DATE\n",
				DEFAULT_PROGRAM_NAME,logCfg->rinexAgency,date.year,date.month,date.day,date.hour,date.minute,(int)date.second);
		// observables list (it is fixed!)
		fprintf(obsFile,"G  %3d C1C D1C L1C S1C                                      SYS / # / OBS TYPES\n",4);
		fprintf(obsFile,"E  %3d C1B D1B L1B S1B                                      SYS / # / OBS TYPES\n",4);
		// other fields
		fprintf(obsFile,"%sMARKER NAME\n",logCfg->rinexMarkerName);
		fprintf(obsFile,"%s                                        MARKER TYPE\n",logCfg->rinexMarkerType);
		fprintf(obsFile,"%s%sOBSERVER / AGENCY\n",logCfg->rinexObserver,logCfg->rinexAgency2);
		// The length of NGENE_VERSION_INDEX could change
		// Check the length to grant the number of characters in each line
		sprintf(row,"%s%s%s",logCfg->rinexReceiverNumber,DEFAULT_PROGRAM_NAME,NGENE_VERSION_INDEX);
		idx = strlen(row);
		fprintf(obsFile,"%s",row);
		for(;idx<60;idx++)
			fprintf(obsFile," ");
		fprintf(obsFile,"REC # / TYPE / VERS\n");
		//
		fprintf(obsFile,"%s%s                    ANT # / TYPE\n",logCfg->rinexAntennaNumber,logCfg->rinexAntennaType);
		fprintf(obsFile,"%14.4f%14.4f%14.4f                  APPROX POSITION XYZ\n",pvtRes->ecefPosition[0],pvtRes->ecefPosition[1],pvtRes->ecefPosition[2]);
		fprintf(obsFile,"%14.4f%14.4f%14.4f                  ANTENNA: DELTA H/E/N\n",logCfg->rinexAntennaDelta[0],logCfg->rinexAntennaDelta[1],logCfg->rinexAntennaDelta[2]);
		/**/
		FromGnssTimeToDate(refConst,refWn,refTow,&date);
		fprintf(obsFile,"%6d%6d%6d%6d%6d%13.7f     ",date.year,date.month,date.day,date.hour,date.minute,date.second);
		if(refConst==GPS_SAT)
			fprintf(obsFile,"GPS");
		else // if(refConst==GALILEO_SAT)
			fprintf(obsFile,"GAL");
		fprintf(obsFile,"         TIME OF FIRST OBS\n");

		/**/
		fprintf(obsFile,"                                                            END OF HEADER\n");
	}

	/* Observation data */
	FromGnssTimeToDate(refConst,refWn,refTow,&date);
	sprintf(row,"> %4d %02d %02d %02d %02d %11.7f  0%3d\n",
			date.year,date.month,date.day,date.hour,date.minute,date.second,numOpCh);
	fprintf(obsFile,"%s",row);
	// write data for each satellite used in PVT
	for(idx=0;idx<pvtData->numOfOperativeChannels;idx++)
	{
		pvtCh = &(pvtData->pvtChArray[idx]);
		ch = pvtCh->ch;
		if(ch->satType==GPS_SAT)
			row[0] = 'G';
		else if(ch->satType==GALILEO_SAT)
			row[0] = 'E';
		else
			continue;
		//
		// Doppler shift: compensate sign change due to front-end
		fd = (double)cfg->fePars->dopplerCorrection * ch->dopplerShift;
		// Carrier phase measure in cycles
		// revert the sign because "the phase changes in the same sense as the range (negative doppler)".
		carrierPhase = -pvtData->csmData[ch->chIndex].lpcOut;
		// CNo: get just a shorter name
		cno = ch->tracking.trkStatusChecker.cnoEstimator.estimatedCNo;
		//
		sprintf(&(row[1]),"%02d%14.3f  %14.3f  %14.3f  %14.3f  \n",
				ch->satId,pvtCh->pseudorangeRaw,fd,carrierPhase,cno);
		fprintf(obsFile,"%s",row);
	}
}
