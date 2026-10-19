/**
 * @file navigationMessage.c
 *
 * @author Mario Nicola
 *
 * @date 06/10/2014
 *
 * @version 1.0.0
 *
 * @brief Some basic functions to manage navigation message.
 */



#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "ngeneDataStructure.h"
#include "ngeneConfiguration.h"
#include "navigationMessageDataStructure.h"
#include "navigationMessage.h"
#include "ngeneConstants.h"
#include "spreadingCodes.h"
#include "ioFunctions.h"
#include "closeManager.h"
#include "galileoMessageParser.h"
#include "pvtChecks.h"


void ResetEphemerides(TGnssEphemerides *eph)
{
	eph->iode1 = INVALID_VALUE;
	eph->iode2 = INVALID_VALUE;
	eph->iode3 = INVALID_VALUE;
	eph->iode4 = INVALID_VALUE;
	eph->meanAnomaly0 = 0.0;
	eph->deltaN = 0.0;
	eph->eccentricity = 0.0;
	eph->sqrtA = 0.0;
	eph->capitalOmega0 = 0.0;
	eph->i0 = 0.0;
	eph->omega = 0.0;
	eph->capitalOmegaDot = 0.0;
	eph->iDot = 0.0;
	eph->capitalCuc = 0.0;
	eph->capitalCus = 0.0;
	eph->capitalCrc = 0.0;
	eph->capitalCrs = 0.0;
	eph->capitalCic = 0.0;
	eph->capitalCis = 0.0;
	eph->toe = 0.0;
	eph->health = SIGNAL_OK;
	eph->sisa = 0;
	eph->galileoDataValidity = NAVIGATION_DATA_VALID;
}


void ResetClockCorrections(TGnssClk *clk)
{
	clk->toc = 0.0;
	clk->af[0] = 0.0;
	clk->af[1] = 0.0;
	clk->af[2] = 0.0;
	clk->tGroupDelay = 0.0;
	clk->iodc = INVALID_VALUE;
}


int InitNavigationMessage(TNGeneConfiguration *cfg,TNGeneData *rx)
{
	int index;
	TNavigationMessage *navMsg;

	rx->gpsLastDecodedPage=0;
	rx->gpsLastDecodedSubframe=0;
	rx->gpsNavigationMessage = (TNavigationMessage *)malloc(GPS_NUMBER_OF_PRNS*sizeof(TNavigationMessage));
	rx->gpsIonoValid=FALSE_FLAG;
	rx->galLastDecodedPage=0;
	rx->galLastDecodedSubframe=0;
	rx->galNavigationMessage = (TNavigationMessage *)malloc(GALILEO_NUMBER_OF_PRNS*sizeof(TNavigationMessage));
	rx->fnavNavigationMessage = (TNavigationMessage *)malloc(GALILEO_NUMBER_OF_PRNS*sizeof(TNavigationMessage));
	rx->galIonoValid=FALSE_FLAG;

	if((rx->gpsNavigationMessage==NULL) || (rx->galNavigationMessage==NULL) || (rx->fnavNavigationMessage==NULL))
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Malloc returned a NULL pointer in InitNavigationMessage");
		CloseNGene(cfg,EXIT_FAILURE);
	}
	for(index=0;index<GPS_NUMBER_OF_PRNS;index++)
	{
		navMsg = &(rx->gpsNavigationMessage[index]);
		navMsg->tow=INVALID_VALUE;
		navMsg->towValid=FALSE_FLAG;
		navMsg->wn=INVALID_VALUE;
		navMsg->wnValid=FALSE_FLAG;
		navMsg->subframe1Received=FALSE_FLAG;
		navMsg->subframe2Received=FALSE_FLAG;
		navMsg->subframe3Received=FALSE_FLAG;
		navMsg->subframe4Received=FALSE_FLAG;
		navMsg->subframe5Received=FALSE_FLAG;
		navMsg->ephemeridesReceived=FALSE_FLAG;
		navMsg->redCedReceived=FALSE_FLAG;
		navMsg->ephFromRedCed=FALSE_FLAG;
		navMsg->ionoReceived=FALSE_FLAG;
		navMsg->gnss2UtcReceived=FALSE_FLAG;
		navMsg->gnss2UtcCompleted=FALSE_FLAG;
		navMsg->gal2GpsReceived=FALSE_FLAG;
		navMsg->gal2GpsCompleted=FALSE_FLAG;
		navMsg->almanac1Received=FALSE_FLAG;
		navMsg->almanac2Received=FALSE_FLAG;
		navMsg->almanac.wna=INVALID_VALUE;
		navMsg->almanac.toa = INVALID_VALUE;
		navMsg->ephTmp.iode1 = INVALID_VALUE;
		navMsg->ephTmp.iode2 = INVALID_VALUE;
		navMsg->ephTmp.iode3 = INVALID_VALUE;
		navMsg->ephTmp.iode4 = INVALID_VALUE;
		navMsg->eph.iode1 = INVALID_VALUE;
		navMsg->eph.iode2 = INVALID_VALUE;
		navMsg->eph.iode3 = INVALID_VALUE;
		navMsg->eph.iode4 = INVALID_VALUE;
		navMsg->clkTmp.iodc = INVALID_VALUE;
		navMsg->clk.iodc = INVALID_VALUE;
		ResetEphemerides(&(navMsg->eph));
		ResetEphemerides(&(navMsg->ephTmp));
		ResetClockCorrections(&(navMsg->clk));
		ResetClockCorrections(&(navMsg->clkTmp));
	}
	for(index=0;index<GALILEO_NUMBER_OF_PRNS;index++)
	{
		navMsg = &(rx->galNavigationMessage[index]);
		navMsg->tow=INVALID_VALUE;
		navMsg->towValid=FALSE_FLAG;
		navMsg->wn=INVALID_VALUE;
		navMsg->wnValid=FALSE_FLAG;
		navMsg->subframe1Received=FALSE_FLAG;
		navMsg->subframe2Received=FALSE_FLAG;
		navMsg->subframe3Received=FALSE_FLAG;
		navMsg->subframe4Received=FALSE_FLAG;
		navMsg->subframe5Received=FALSE_FLAG;
		navMsg->ephemeridesReceived=FALSE_FLAG;
		navMsg->redCedReceived=FALSE_FLAG;
		navMsg->ephFromRedCed=FALSE_FLAG;
		navMsg->ionoReceived=FALSE_FLAG;
		navMsg->gnss2UtcReceived=FALSE_FLAG;
		navMsg->gnss2UtcCompleted=FALSE_FLAG;
		navMsg->gal2GpsReceived=FALSE_FLAG;
		navMsg->gal2GpsCompleted=FALSE_FLAG;
		navMsg->almanac1Received=FALSE_FLAG;
		navMsg->almanac2Received=FALSE_FLAG;
		navMsg->almanac.wna=INVALID_VALUE;
		navMsg->almanac.toa = INVALID_VALUE;
		navMsg->ephTmp.iode1 = INVALID_VALUE;
		navMsg->ephTmp.iode2 = INVALID_VALUE;
		navMsg->ephTmp.iode3 = INVALID_VALUE;
		navMsg->ephTmp.iode4 = INVALID_VALUE;
		navMsg->eph.iode1 = INVALID_VALUE;
		navMsg->eph.iode2 = INVALID_VALUE;
		navMsg->eph.iode3 = INVALID_VALUE;
		navMsg->eph.iode4 = INVALID_VALUE;
		navMsg->clkTmp.iodc = INVALID_VALUE;
		navMsg->clk.iodc = INVALID_VALUE;
		ResetEphemerides(&(navMsg->eph));
		ResetEphemerides(&(navMsg->ephTmp));
		ResetClockCorrections(&(navMsg->clk));
		ResetClockCorrections(&(navMsg->clkTmp));
	}
	for(index=0;index<GALILEO_NUMBER_OF_PRNS;index++)
	{
		navMsg = &(rx->fnavNavigationMessage[index]);
		navMsg->tow=INVALID_VALUE;
		navMsg->towValid=FALSE_FLAG;
		navMsg->wn=INVALID_VALUE;
		navMsg->wnValid=FALSE_FLAG;
		navMsg->subframe1Received=FALSE_FLAG;
		navMsg->subframe2Received=FALSE_FLAG;
		navMsg->subframe3Received=FALSE_FLAG;
		navMsg->subframe4Received=FALSE_FLAG;
		navMsg->subframe5Received=FALSE_FLAG;
		navMsg->ephemeridesReceived=FALSE_FLAG;
		navMsg->redCedReceived=FALSE_FLAG;
		navMsg->ephFromRedCed=FALSE_FLAG;
		navMsg->ionoReceived=FALSE_FLAG;
		navMsg->gnss2UtcReceived=FALSE_FLAG;
		navMsg->gnss2UtcCompleted=FALSE_FLAG;
		navMsg->gal2GpsReceived=FALSE_FLAG;
		navMsg->gal2GpsCompleted=FALSE_FLAG;
		navMsg->almanac1Received=FALSE_FLAG;
		navMsg->almanac2Received=FALSE_FLAG;
		navMsg->almanac.wna=INVALID_VALUE;
		navMsg->almanac.toa = INVALID_VALUE;
		navMsg->ephTmp.iode1 = INVALID_VALUE;
		navMsg->ephTmp.iode2 = INVALID_VALUE;
		navMsg->ephTmp.iode3 = INVALID_VALUE;
		navMsg->ephTmp.iode4 = INVALID_VALUE;
		navMsg->eph.iode1 = INVALID_VALUE;
		navMsg->eph.iode2 = INVALID_VALUE;
		navMsg->eph.iode3 = INVALID_VALUE;
		navMsg->eph.iode4 = INVALID_VALUE;
		navMsg->clkTmp.iodc = INVALID_VALUE;
		navMsg->clk.iodc = INVALID_VALUE;
		ResetEphemerides(&(navMsg->eph));
		ResetEphemerides(&(navMsg->ephTmp));
		ResetClockCorrections(&(navMsg->clk));
		ResetClockCorrections(&(navMsg->clkTmp));
	}
	return SUCCESS_RETURN;
}

void DeinitNavigationMessage(TNGeneConfiguration *cfg,TNGeneData *rx)
{
	SaveAlmanacIntoYumaFile(cfg,rx);
	SaveGalileoAlmanacIntoXmlFile(cfg,rx);
	free(rx->gpsNavigationMessage);
	free(rx->galNavigationMessage);
	free(rx->fnavNavigationMessage);
}


void CopyFromTmpToOperative(TNavigationMessage *navMsg)
{
	TGnssEphemerides *ephTmp = &(navMsg->ephTmp);
	TGnssEphemerides *eph = &(navMsg->eph);
	TGnssClk *clkTmp = &(navMsg->clkTmp);
	TGnssClk *clk = &(navMsg->clk);

	eph->iode1 = ephTmp->iode1;
	eph->iode2 = ephTmp->iode2;
	eph->iode3 = ephTmp->iode3;
	eph->iode4 = ephTmp->iode4;
	eph->meanAnomaly0 = ephTmp->meanAnomaly0;
	eph->deltaN = ephTmp->deltaN;
	eph->eccentricity = ephTmp->eccentricity;
	eph->sqrtA = ephTmp->sqrtA;
	eph->capitalOmega0 = ephTmp->capitalOmega0;
	eph->i0 = ephTmp->i0;
	eph->omega = ephTmp->omega;
	eph->capitalOmegaDot = ephTmp->capitalOmegaDot;
	eph->iDot = ephTmp->iDot;
	eph->capitalCuc = ephTmp->capitalCuc;
	eph->capitalCus = ephTmp->capitalCus;
	eph->capitalCrc = ephTmp->capitalCrc;
	eph->capitalCrs = ephTmp->capitalCrs;
	eph->capitalCic = ephTmp->capitalCic;
	eph->capitalCis = ephTmp->capitalCis;
	eph->toe = ephTmp->toe;
	eph->health = ephTmp->health;
	eph->sisa = ephTmp->sisa;
	eph->galileoDataValidity = ephTmp->galileoDataValidity;
	clk->toc = clkTmp->toc;
	clk->af[0] = clkTmp->af[0];
	clk->af[1] = clkTmp->af[1];
	clk->af[2] = clkTmp->af[2];
	clk->tGroupDelay = clkTmp->tGroupDelay;
	clk->bgd_E1E5a = clkTmp->bgd_E1E5a;
	clk->bgd_E1E5b = clkTmp->bgd_E1E5b;
	clk->iodc = clkTmp->iodc;
}


static void PrintAlmanac(FILE *logFile,TGnssAlmanac *alm, int prn,int wna,TSatConstellation satType)
{
	if((satType==GPS_SAT) && (alm->wna!=INVALID_VALUE))
	{
		wna = alm->wna;
	}
	if(satType==GALILEO_SAT)
	{
		if(wna==INVALID_VALUE)
			wna = 0;
		else
			wna = (wna & 0xfffffffc);
	}

	fprintf(logFile,"******** Week %4d almanac for PRN-%02d ********\n",wna,prn);
	fprintf(logFile,"ID:                         %02d\n",prn);
	fprintf(logFile,"Health:                     %d\n",alm->health);
	fprintf(logFile,"Eccentricity:               %e\n",alm->eccentricity);
	fprintf(logFile,"Time of Applicability(s):   %f\n",alm->toa);
	fprintf(logFile,"Orbital Inclination(rad):   %f\n",alm->i0);
	fprintf(logFile,"Rate of Right Ascen(r/s):   %e\n",alm->capitalOmegaDot);
	fprintf(logFile,"SQRT(A)  (m 1/2):           %f\n",alm->sqrtA);
	fprintf(logFile,"Right Ascen at Week(rad):   %e\n",alm->capitalOmega0);
	fprintf(logFile,"Argument of Perigee(rad):   %f\n",alm->omega);
	fprintf(logFile,"Mean Anom(rad):             %e\n",alm->meanAnomaly0);
	fprintf(logFile,"Af0(s):                     %e\n",alm->af[0]);
	fprintf(logFile,"Af1(s/s):                   %e\n",alm->af[1]);
	if(satType==GPS_SAT)
		fprintf(logFile,"week:                       %d\n",wna);
	else
		fprintf(logFile,"week:                       %d\n",wna+alm->wna);
	fprintf(logFile,"\n");
}


void SaveAlmanacIntoYumaFile(TNGeneConfiguration *cfg,TNGeneData *rx)
{
	char filename[MAX_ROW_LENGTH];
	FILE *yumaFile;
	int idx;
	int wna;
	TGnssAlmanac *alm;

	if(cfg->logConfiguration.enableAlmanacLog==FALSE_FLAG)
	{
		// log is disabled, nothing to do
		return;
	}

	/* for GPS */
	if(cfg->numberOfGpsChannels>0)
	{
		/* If no GPS channels are allocated, no GPS almanac is saved */
		filename[0] = '\0';
		strcat(filename, cfg->dumpPathCurrent);
		strcat(filename, "/gpsYumaAlmanac.alm");
		yumaFile = fopen(filename,"wb");
		if(yumaFile!=NULL)
		{
			/* find a valid week number */
			wna = INVALID_VALUE;
			idx=0;
			while((idx<GPS_NUMBER_OF_PRNS) && (rx->gpsNavigationMessage[idx].almanac.wna==INVALID_VALUE))
			{
				idx++;
			}
			if(idx<GPS_NUMBER_OF_PRNS)
			{
				wna = rx->gpsNavigationMessage[idx].almanac.wna;
			}
			else
			{
				idx=0;
				while((idx<GPS_NUMBER_OF_PRNS) && (rx->gpsNavigationMessage[idx].wnValid==FALSE_FLAG))
				{
					idx++;
				}
				if(idx<GPS_NUMBER_OF_PRNS)
				{
					wna = rx->gpsNavigationMessage[idx].wn;
				}
			}
			/* write the almanac for all the satellites */
			for(idx=0;idx<GPS_NUMBER_OF_PRNS;idx++)
			{
				if(rx->gpsNavigationMessage[idx].almanac1Received==TRUE_FLAG)
				{
					alm = &(rx->gpsNavigationMessage[idx].almanac);
					PrintAlmanac(yumaFile,alm,idx+BASE_PRN,wna,GPS_SAT);
				}
			}
			fclose(yumaFile);
		}
	}

	/* for Galileo */
	if(cfg->numberOfGalileoChannels>0) 
	{
		/* If no Galileo channels are allocated, no Galileo almanac is saved */
		filename[0] = '\0';
		strcat(filename, cfg->dumpPathCurrent);
		strcat(filename, "/galYumaAlmanac.alm");
		yumaFile = fopen(filename,"wb");
		if(yumaFile!=NULL)
		{
			/* find a valid week number */
			wna = INVALID_VALUE;
			idx=0;
			while((idx<GALILEO_NUMBER_OF_PRNS) && (rx->galNavigationMessage[idx].wnValid==FALSE_FLAG))
			{
				idx++;
			}
			if(idx<GALILEO_NUMBER_OF_PRNS)
			{
				wna = rx->galNavigationMessage[idx].wn;
			}
			/* write the almanac for all the satellites */
			for(idx=0;idx<GALILEO_NUMBER_OF_PRNS;idx++)
			{
				if((rx->galNavigationMessage[idx].almanac1Received==TRUE_FLAG) && (rx->galNavigationMessage[idx].almanac2Received==TRUE_FLAG))
				{
					alm = &(rx->galNavigationMessage[idx].almanac);
					PrintAlmanac(yumaFile,alm,idx+BASE_PRN,wna,GALILEO_SAT);
				}
			}
			fclose(yumaFile);
		}
	}
}


void SaveGalileoAlmanacIntoXmlFile(TNGeneConfiguration *cfg,TNGeneData *rx)
{
	time_t currentTime;
	char *timeString;
	char filename[MAX_ROW_LENGTH];
	FILE *xmlFile;
	int idx;
	int wna;
	TGnssAlmanac *alm;

	/* preliminary checks */
	if(cfg->logConfiguration.enableAlmanacLog==FALSE_FLAG)
	{
		// log is disabled, nothing to do
		return;
	}
	if(cfg->numberOfGalileoChannels==0)
	{
		// no Galileo channels, nothing to do
		return;
	}

	/* prepare the string with the current time */
	currentTime = time(NULL);
	timeString = ctime(&currentTime);
	timeString[strlen(timeString) - 1] = '\0';

	/* for Galileo */
	// prepare filename and create file
	filename[0] = '\0';
	strcat(filename, cfg->dumpPathCurrent);
	strcat(filename, "/galXmlAlmanac.xml");
	xmlFile = fopen(filename,"w");
	if(xmlFile!=NULL)
	{
		/* find a valid week number */
		wna = INVALID_VALUE;
		idx=0;
		while((idx<GALILEO_NUMBER_OF_PRNS) && (rx->galNavigationMessage[idx].wnValid==FALSE_FLAG))
		{
			idx++;
		}
		if(idx<GALILEO_NUMBER_OF_PRNS)
		{
			wna = rx->galNavigationMessage[idx].wn;
		}

		/* write header */
		fprintf(xmlFile,"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
		fprintf(xmlFile,"<!-- XML file generated by NGene2 -->\n");
		fprintf(xmlFile,"<!-- Warning: the complete xml format for Galileo almanac is not documented, so some fields in the header could be wrong -->\n");
		fprintf(xmlFile,"<!-- Warning: the health flag of E1b I/Nav is used also for E5b I/Nav and F/Nav -->\n");
		fprintf(xmlFile,"<signalData xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:noNamespaceSchemaLocation=\"XSD/INF_AlmanacData_V01.02.xsd\">\n");
		fprintf(xmlFile,"  <header>\n");
		fprintf(xmlFile,"    <GAL-header>\n");
		fprintf(xmlFile,"      <source>\n");
		fprintf(xmlFile,"        <GAL-GMS-MUCF-GLAd>\n");
		fprintf(xmlFile,"          <mission>GAL</mission>\n");
		fprintf(xmlFile,"          <segment>TGVF</segment>\n");
		fprintf(xmlFile,"          <element>DSF</element>\n");
		fprintf(xmlFile,"          <site>GESS</site>\n");
		fprintf(xmlFile,"          <instance>1</instance>\n");
		fprintf(xmlFile,"          <subsystem>GMOS</subsystem>\n");
		fprintf(xmlFile,"        </GAL-GMS-MUCF-GLAd>\n");
		fprintf(xmlFile,"      </source>\n");
		fprintf(xmlFile,"      <destination>\n");
		fprintf(xmlFile,"        <GAL-EXT-GLAd>\n");
		fprintf(xmlFile,"          <mission>GAL</mission>\n");
		fprintf(xmlFile,"          <segment>EXT</segment>\n");
		fprintf(xmlFile,"          <element>any</element>\n");
		fprintf(xmlFile,"        </GAL-EXT-GLAd>\n");
		fprintf(xmlFile,"      </destination>\n");
		fprintf(xmlFile,"      <issueDate>%sZ</issueDate>\n",timeString);
		fprintf(xmlFile,"      <signalVersion>0.0</signalVersion>\n");
		fprintf(xmlFile,"      <dataVersion>0.0</dataVersion>\n");
		fprintf(xmlFile,"      <identifier>String</identifier>\n");
		fprintf(xmlFile,"      <information>String</information>\n");
		fprintf(xmlFile,"    </GAL-header>\n");
		fprintf(xmlFile,"  </header>\n");
		fprintf(xmlFile,"  <body>\n");
		fprintf(xmlFile,"    <Almanacs>\n");

		/* write the almanac for all the satellites */
		for(idx=0;idx<GALILEO_NUMBER_OF_PRNS;idx++)
		{
			if((rx->galNavigationMessage[idx].almanac1Received==TRUE_FLAG) && (rx->galNavigationMessage[idx].almanac2Received==TRUE_FLAG))
			{
				alm = &(rx->galNavigationMessage[idx].almanac);
				//
				fprintf(xmlFile,"      <svAlmanac>\n");
				fprintf(xmlFile,"        <SVID>%d</SVID>\n",idx+1);
				fprintf(xmlFile,"        <almanac>\n");
				fprintf(xmlFile,"          <aSqRoot>%.8f</aSqRoot>\n",alm->sqrtA-GAL_NOMINAL_SQRTA);
				fprintf(xmlFile,"          <ecc>%.16f</ecc>\n",alm->eccentricity);
				fprintf(xmlFile,"          <deltai>%.33f</deltai>\n",(alm->i0 - GAL_NOMINAL_I0)/M_PI);
				fprintf(xmlFile,"          <omega0>%.30f</omega0>\n",alm->capitalOmega0/M_PI);
				fprintf(xmlFile,"          <omegaDot>%.29e</omegaDot>\n",alm->capitalOmegaDot/M_PI);
				fprintf(xmlFile,"          <w>%.30f</w>\n",alm->omega/M_PI);
				fprintf(xmlFile,"          <m0>%.30f</m0>\n",alm->meanAnomaly0/M_PI);
				fprintf(xmlFile,"          <af0>%.32f</af0>\n",alm->af[0]);
				fprintf(xmlFile,"          <af1>%.29e</af1>\n",alm->af[1]);
				fprintf(xmlFile,"          <iod>%d</iod>\n",alm->ioda);
				fprintf(xmlFile,"          <t0a>%d</t0a>\n",(int)round(alm->toa));
				fprintf(xmlFile,"          <wna>%d</wna>\n",wna%4); // wna is modulo 4
				fprintf(xmlFile,"        </almanac>\n");
				fprintf(xmlFile,"        <svFNavSignalStatus>\n");
				fprintf(xmlFile,"          <statusE5a>%d</statusE5a>\n",alm->health);
				fprintf(xmlFile,"        </svFNavSignalStatus>\n");
				fprintf(xmlFile,"        <svINavSignalStatus>\n");
				fprintf(xmlFile,"          <statusE5b>%d</statusE5b>\n",alm->health);
				fprintf(xmlFile,"          <statusE1B>%d</statusE1B>\n",alm->health);
				fprintf(xmlFile,"        </svINavSignalStatus>\n");
				fprintf(xmlFile,"      </svAlmanac>\n");
			}
		}

		/* terminate and close the file */
		fprintf(xmlFile,"    </Almanacs>\n");
		fprintf(xmlFile,"  </body>\n");
		fprintf(xmlFile,"</signalData>\n");
		fclose(xmlFile);
	}
}


void GetATransmissionTime(TNGeneConfiguration *cfg,TNGeneData *rx,int chIndex,TSatConstellation *satType,int *wn,double *tow)
{
	int index;
	TChannel *ch;

	// init
	*satType = INVALID_SAT;
	*wn = INVALID_VALUE;
	*tow = INVALID_VALUE;

	// search a valid TOW and (optionally) WN
	for(index=0;index<cfg->numberOfChannels;index++)
	{
		ch = &(rx->channelArray[index]);
		if(ch->chIndex==chIndex)
		{
			// the current channel is the destination, do not look here
			continue;
		}
		// check time availability
		if(CheckTow(ch)==FALSE_FLAG)
		{
			// tow not available, pass to the next channel
			continue;
		}
		//
		if(ch->satType==GPS_SAT)
		{
			*satType = GPS_SAT;
			*tow = (double)ch->gpsParser->navMsg->tow +
				  (((double)ch->gpsParser->bitIndexTotal*GPS_BIT_DURATION)/SECONDS_TO_MILLISECONDS);
			if(ch->gpsParser->navMsg->wnValid==TRUE_FLAG)
			{
				*wn = ch->gpsParser->navMsg->wn;
				return;
			}
		}
		else if(ch->satType==GALILEO_SAT)
		{
			*satType = GALILEO_SAT;
			*tow = (double)ch->galParser->navMsg->tow +
			      (((double)ch->galParser->bitIndexTotal*GALILEO_BIT_DURATION)/SECONDS_TO_MILLISECONDS);
			if(ch->galParser->navMsg->wnValid==TRUE_FLAG)
			{
				*wn = ch->galParser->navMsg->wn;
				return;
			}
		}
		// if here, a valid TOW has been found, but not the WN
		// pass to the next channel and look for one valid WN
	}
}
