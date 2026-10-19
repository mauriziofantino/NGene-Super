/**
 * @file warmStart.c
 *
 * @author Mario Nicola
 *
 * @date 13/04/2021
 *
 * @version 1.0.0
 *
 * @brief Functions that manage the Warm Start.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "ngeneConstants.h"
#include "spreadingCodes.h"
#include "galileoMessageParser.h"
#include "ioFunctions.h"
#include "closeManager.h"
#include "warmStart.h"
#include "pvtUtility.h"
#include "timeConstants.h"
#include "prnList.h"


/**
 * @def MINIMUM_PRNS_IN_ALAMANAC
 * @brief MINIMUM_PRNS_IN_ALAMANAC is the minimum number of PRNs available in the almanac file to use the warm start
 */
#define MINIMUM_PRNS_IN_ALMANAC 	10

/**
 * @def DEBUG_WARM_START
 * @brief If defined enables the creation of a log file with the results of the warm start
 */
//#define DEBUG_WARM_START


/* Parser of GPS Almanac in YUMA format */
/* Constants */
#define NUMBER_OF_FIELDS_IN_YUMA_ALMANAC	12

#define YUMA_PRN_TAG	     "ID:"
#define YUMA_ASQROOT_TAG     "SQRT(A)  (m 1/2):"
#define YUMA_ECC_TAG         "Eccentricity:"
#define YUMA_I0_TAG          "Orbital Inclination(rad):"
#define YUMA_OMEGA0_TAG      "Right Ascen at Week(rad):"
#define YUMA_OMEGADOT_TAG    "Rate of Right Ascen(r/s):"
#define YUMA_W_TAG           "Argument of Perigee(rad):"
#define YUMA_M0_TAG          "Mean Anom(rad):"
#define YUMA_AF0_TAG         "Af0(s):"
#define YUMA_AF1_TAG         "Af1(s/s):"
#define YUMA_T0A_TAG         "Time of Applicability(s):"
#define YUMA_WNA_TAG         "week:"
#define YUMA_HEALTH_TAG      "Health:"
#define YUMA_HEADER_LINE     "******** Week"

/* Parser */
int LoadGpsAlmanacFromYumaFile(TWarmStartConfiguration *warmCfg,TNavigationMessage *navMesg)
{
	FILE *filePtr;
	char row[MAX_ROW_LENGTH];
	char *ptrChar;
	int prn;
	TGnssAlmanac *alm;
	int fieldsFound;
	int tmpInt;
	double tmpDouble;
	int prnsFound = 0;

	/* open the file*/
	filePtr = fopen(warmCfg->almanacPathGps,"r");
	if(filePtr==NULL)
	{
		return FAILURE_RETURN;
	}

	/* parse until the file is not finished */
	while(fgets(row,MAX_ROW_LENGTH,filePtr)!=NULL)
	{
		// look for PRN
		ptrChar = strstr(row,YUMA_PRN_TAG);
		if(ptrChar==NULL)
		{
			// not found, continue to the next row
			continue;
		}
		// else an almanac for a new PRN is found, parse it!
		ptrChar += strlen(YUMA_PRN_TAG);
		if(sscanf(ptrChar,"%d",&prn)!=1)
		{
			// PRN not found, discard, continue to the next row
			continue;
		}
		alm = &(navMesg[prn-BASE_PRN].almanac);
		fieldsFound = 0;
		do
		{
			if(fgets(row,MAX_ROW_LENGTH,filePtr)==NULL)
			{
				// file finished? stop the parse
				break;
			}
			// Check the presence of a field and parse it accordingly
			ptrChar = strstr(row,YUMA_ASQROOT_TAG);
			if(ptrChar!=NULL)
			{
				ptrChar += strlen(YUMA_ASQROOT_TAG);
				if(sscanf(ptrChar,"%lf",&tmpDouble)==1)
				{
					alm->sqrtA = tmpDouble;
					fieldsFound++;
				}
				continue;
			}
			//
			ptrChar = strstr(row,YUMA_ECC_TAG);
			if(ptrChar!=NULL)
			{
				ptrChar += strlen(YUMA_ECC_TAG);
				if(sscanf(ptrChar,"%lf",&tmpDouble)==1)
				{
					alm->eccentricity = tmpDouble;
					fieldsFound++;
				}
				continue;
			}
			//
			ptrChar = strstr(row,YUMA_I0_TAG);
			if(ptrChar!=NULL)
			{
				ptrChar += strlen(YUMA_I0_TAG);
				if(sscanf(ptrChar,"%lf",&tmpDouble)==1)
				{
					alm->i0 = tmpDouble;
					fieldsFound++;
				}
				continue;
			}
			//
			ptrChar = strstr(row,YUMA_OMEGA0_TAG);
			if(ptrChar!=NULL)
			{
				ptrChar += strlen(YUMA_OMEGA0_TAG);
				if(sscanf(ptrChar,"%lf",&tmpDouble)==1)
				{
					alm->capitalOmega0 = tmpDouble;
					fieldsFound++;
				}
				continue;
			}
			//
			ptrChar = strstr(row,YUMA_OMEGADOT_TAG);
			if(ptrChar!=NULL)
			{
				ptrChar += strlen(YUMA_OMEGADOT_TAG);
				if(sscanf(ptrChar,"%lf",&tmpDouble)==1)
				{
					alm->capitalOmegaDot = tmpDouble;
					fieldsFound++;
				}
				continue;
			}
			//
			ptrChar = strstr(row,YUMA_W_TAG);
			if(ptrChar!=NULL)
			{
				ptrChar += strlen(YUMA_W_TAG);
				if(sscanf(ptrChar,"%lf",&tmpDouble)==1)
				{
					alm->omega = tmpDouble;
					fieldsFound++;
				}
				continue;
			}
			//
			ptrChar = strstr(row,YUMA_M0_TAG);
			if(ptrChar!=NULL)
			{
				ptrChar += strlen(YUMA_M0_TAG);
				if(sscanf(ptrChar,"%lf",&tmpDouble)==1)
				{
					alm->meanAnomaly0 = tmpDouble;
					fieldsFound++;
				}
				continue;
			}
			//
			ptrChar = strstr(row,YUMA_AF0_TAG);
			if(ptrChar!=NULL)
			{
				ptrChar += strlen(YUMA_AF0_TAG);
				if(sscanf(ptrChar,"%lf",&tmpDouble)==1)
				{
					alm->af[0] = tmpDouble;
					fieldsFound++;
				}
				continue;
			}
			//
			ptrChar = strstr(row,YUMA_AF1_TAG);
			if(ptrChar!=NULL)
			{
				ptrChar += strlen(YUMA_AF1_TAG);
				if(sscanf(ptrChar,"%lf",&tmpDouble)==1)
				{
					alm->af[1] = tmpDouble;
					fieldsFound++;
				}
				continue;
			}
			//
			ptrChar = strstr(row,YUMA_T0A_TAG);
			if(ptrChar!=NULL)
			{
				ptrChar += strlen(YUMA_T0A_TAG);
				if(sscanf(ptrChar,"%lf",&tmpDouble)==1)
				{
					alm->toa = tmpDouble;
					fieldsFound++;
				}
				continue;
			}
			//
			ptrChar = strstr(row,YUMA_WNA_TAG);
			if(ptrChar!=NULL)
			{
				ptrChar += strlen(YUMA_WNA_TAG);
				if(sscanf(ptrChar,"%d",&tmpInt)==1)
				{
					alm->wna = tmpInt;
					fieldsFound++;
				}
				continue;
			}
			//
			ptrChar = strstr(row,YUMA_HEALTH_TAG);
			if(ptrChar!=NULL)
			{
				ptrChar += strlen(YUMA_HEALTH_TAG);
				if(sscanf(ptrChar,"%d",&tmpInt)==1)
				{
					alm->health = tmpInt;
					fieldsFound++;
				}
				continue;
			}
			// loop until all the fields have been found or a new almanac is starting
		} while((fieldsFound<NUMBER_OF_FIELDS_IN_YUMA_ALMANAC) && strstr(row,YUMA_HEADER_LINE)==NULL);
		// check if the almanac is complete and then validate it
		if(fieldsFound==NUMBER_OF_FIELDS_IN_YUMA_ALMANAC)
		{
			navMesg[prn-BASE_PRN].almanac1Received = TRUE_FLAG;
			navMesg[prn-BASE_PRN].almanac2Received = TRUE_FLAG;
			prnsFound++;
		}
	}

	/* deinit */
	fclose(filePtr);
	return (prnsFound>=MINIMUM_PRNS_IN_ALMANAC) ? SUCCESS_RETURN : FAILURE_RETURN;
}




/* Parser of Galileo Almanac in XML format */
/* Constants */
#define NUMBER_OF_FIELDS_IN_XML_ALMANAC	13

#define XML_PRN_TAG	        "<SVID>"
#define XML_ASQROOT_TAG     "<aSqRoot>"
#define XML_ECC_TAG         "<ecc>"
#define XML_DELTAI_TAG      "<deltai>"
#define XML_OMEGA0_TAG      "<omega0>"
#define XML_OMEGADOT_TAG    "<omegaDot>"
#define XML_W_TAG           "<w>"
#define XML_M0_TAG          "<m0>"
#define XML_AF0_TAG         "<af0>"
#define XML_AF1_TAG         "<af1>"
#define XML_IOD_TAG         "<iod>"
#define XML_T0A_TAG         "<t0a>"
#define XML_WNA_TAG         "<wna>"
#define XML_STATUSE1B_TAG   "<statusE1B>"
#define XML_END_ALMANAC_TAG "</svAlmanac>"

/* Parser */
static int LoadGalileoAlmanacFromXmlFile(TWarmStartConfiguration *warmCfg,TNavigationMessage *navMesg)
{
	FILE *filePtr;
	char row[MAX_ROW_LENGTH];
	char *ptrChar;
	int prn;
	TGnssAlmanac *alm;
	int fieldsFound;
	int tmpInt;
	double tmpDouble;
	int prnsFound = 0;


	/* open the file*/
	filePtr = fopen(warmCfg->almanacPathGal,"r");
	if(filePtr==NULL)
	{
		return FAILURE_RETURN;
	}

	/* parse until the file is not finished */
	while(fgets(row,MAX_ROW_LENGTH,filePtr)!=NULL)
	{
		// look for PRN
		ptrChar = strstr(row,XML_PRN_TAG);
		if(ptrChar==NULL)
		{
			// not found, continue to the next row
			continue;
		}
		// else an almanac for a new PRN is found, parse it!
		ptrChar += strlen(XML_PRN_TAG);
		if(sscanf(ptrChar,"%d",&prn)!=1)
		{
			// PRN not found, discard, continue to the next row
			continue;
		}
		alm = &(navMesg[prn-BASE_PRN].almanac);
		fieldsFound = 0;
		do
		{
			if(fgets(row,MAX_ROW_LENGTH,filePtr)==NULL)
			{
				// file finished? stop the parse
				break;
			}
			// Check the presence of a field and parse it accordingly
			ptrChar = strstr(row,XML_ASQROOT_TAG);
			if(ptrChar!=NULL)
			{
				ptrChar += strlen(XML_ASQROOT_TAG);
				if(sscanf(ptrChar,"%lf",&tmpDouble)==1)
				{
					alm->sqrtA = tmpDouble;
					alm->sqrtA += GAL_NOMINAL_SQRTA;
					fieldsFound++;
				}
				continue;
			}
			//
			ptrChar = strstr(row,XML_ECC_TAG);
			if(ptrChar!=NULL)
			{
				ptrChar += strlen(XML_ECC_TAG);
				if(sscanf(ptrChar,"%lf",&tmpDouble)==1)
				{
					alm->eccentricity = tmpDouble;
					fieldsFound++;
				}
				continue;
			}
			//
			ptrChar = strstr(row,XML_DELTAI_TAG);
			if(ptrChar!=NULL)
			{
				ptrChar += strlen(XML_DELTAI_TAG);
				if(sscanf(ptrChar,"%lf",&tmpDouble)==1)
				{
					alm->i0 = M_PI*tmpDouble;
					alm->i0 += GAL_NOMINAL_I0;
					fieldsFound++;
				}
				continue;
			}
			//
			ptrChar = strstr(row,XML_OMEGA0_TAG);
			if(ptrChar!=NULL)
			{
				ptrChar += strlen(XML_OMEGA0_TAG);
				if(sscanf(ptrChar,"%lf",&tmpDouble)==1)
				{
					alm->capitalOmega0 = M_PI*tmpDouble;
					fieldsFound++;
				}
				continue;
			}
			//
			ptrChar = strstr(row,XML_OMEGADOT_TAG);
			if(ptrChar!=NULL)
			{
				ptrChar += strlen(XML_OMEGADOT_TAG);
				if(sscanf(ptrChar,"%lf",&tmpDouble)==1)
				{
					alm->capitalOmegaDot = M_PI*tmpDouble;
					fieldsFound++;
				}
				continue;
			}
			//
			ptrChar = strstr(row,XML_W_TAG);
			if(ptrChar!=NULL)
			{
				ptrChar += strlen(XML_W_TAG);
				if(sscanf(ptrChar,"%lf",&tmpDouble)==1)
				{
					alm->omega = M_PI*tmpDouble;
					fieldsFound++;
				}
				continue;
			}
			//
			ptrChar = strstr(row,XML_M0_TAG);
			if(ptrChar!=NULL)
			{
				ptrChar += strlen(XML_M0_TAG);
				if(sscanf(ptrChar,"%lf",&tmpDouble)==1)
				{
					alm->meanAnomaly0 = M_PI*tmpDouble;
					fieldsFound++;
				}
				continue;
			}
			//
			ptrChar = strstr(row,XML_AF0_TAG);
			if(ptrChar!=NULL)
			{
				ptrChar += strlen(XML_AF0_TAG);
				if(sscanf(ptrChar,"%lf",&tmpDouble)==1)
				{
					alm->af[0] = tmpDouble;
					fieldsFound++;
				}
				continue;
			}
			//
			ptrChar = strstr(row,XML_AF1_TAG);
			if(ptrChar!=NULL)
			{
				ptrChar += strlen(XML_AF1_TAG);
				if(sscanf(ptrChar,"%lf",&tmpDouble)==1)
				{
					alm->af[1] = tmpDouble;
					fieldsFound++;
				}
				continue;
			}
			//
			ptrChar = strstr(row,XML_IOD_TAG);
			if(ptrChar!=NULL)
			{
				ptrChar += strlen(XML_IOD_TAG);
				if(sscanf(ptrChar,"%d",&tmpInt)==1)
				{
					alm->ioda = tmpInt;
					fieldsFound++;
				}
				continue;
			}
			//
			ptrChar = strstr(row,XML_T0A_TAG);
			if(ptrChar!=NULL)
			{
				ptrChar += strlen(XML_T0A_TAG);
				if(sscanf(ptrChar,"%lf",&tmpDouble)==1)
				{
					alm->toa = tmpDouble;
					fieldsFound++;
				}
				continue;
			}
			//
			ptrChar = strstr(row,XML_WNA_TAG);
			if(ptrChar!=NULL)
			{
				ptrChar += strlen(XML_WNA_TAG);
				if(sscanf(ptrChar,"%d",&tmpInt)==1)
				{
					alm->wna = tmpInt;
					fieldsFound++;
				}
				continue;
			}
			//
			ptrChar = strstr(row,XML_STATUSE1B_TAG);
			if(ptrChar!=NULL)
			{
				ptrChar += strlen(XML_STATUSE1B_TAG);
				if(sscanf(ptrChar,"%d",&tmpInt)==1)
				{
					alm->health = tmpInt;
					fieldsFound++;
				}
				continue;
			}
		} while(strstr(row,XML_END_ALMANAC_TAG)==NULL);
		// check if the almanac is complete and then validate it
		if(fieldsFound==NUMBER_OF_FIELDS_IN_XML_ALMANAC)
		{
			navMesg[prn-BASE_PRN].almanac1Received = TRUE_FLAG;
			navMesg[prn-BASE_PRN].almanac2Received = TRUE_FLAG;
			prnsFound++;
		}
	}

	/* deinit */
	fclose(filePtr);

	return (prnsFound>=MINIMUM_PRNS_IN_ALMANAC) ? SUCCESS_RETURN : FAILURE_RETURN;
}


static int LoadAlmanacsFromFiles(TWarmStartConfiguration *warmCfg,TNavigationMessage *gpsNavMesg,TNavigationMessage *galNavMesg)
{
	// GPS from YUMA
	if(LoadGpsAlmanacFromYumaFile(warmCfg,gpsNavMesg)==FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_WARNING, "The parse of the GPS Almanac <%s> failed. Warm start disabled.",warmCfg->almanacPathGps);
		warmCfg->enableWarmStart = FALSE_FLAG;
		return FAILURE_RETURN;
	}
	// Galileo from xml
	if(LoadGalileoAlmanacFromXmlFile(warmCfg,galNavMesg)==FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_WARNING, "The parse of the Galileo Almanac <%s> failed. Warm start disabled.",warmCfg->almanacPathGal);
		warmCfg->enableWarmStart = FALSE_FLAG;
		return FAILURE_RETURN;
	}
	//
	return SUCCESS_RETURN;
}


int InitWarmStartData(TNGeneConfiguration *cfg,TNGeneData *rx)
{
	TWarmStartConfiguration *warmCfg = &(cfg->warmConfiguration);
	TWarmStartData *warmData = &(rx->warmStartData);
	int idx;

	/* default values */
	warmData->predictionValid = FALSE_FLAG;
	warmData->gpsNumVisible = 0;
	warmData->gpsNumAvailable = 0;
	warmData->gpsPrediction = NULL;
	warmData->galNumVisible = 0;
	warmData->galNumAvailable = 0;
	warmData->galPrediction = NULL;
	warmData->updateInterval = (unsigned long long int)(warmCfg->updateInterval*SECONDS_PER_MINUTE*cfg->fs);
	warmData->nextUpdate = 0;

	/* check if warm is enabled */
	if(warmCfg->enableWarmStart==FALSE_FLAG)
	{
		return SUCCESS_RETURN;
	}

	/* check that manual configuration is not enabled */
	if(cfg->automaticConfigurationFlag==FALSE_FLAG)
	{
		SwrxLog(NGENE_LOG_SEVERITY_WARNING,"Manual Configuration is selected, so the Warm Start is disabled");
		warmCfg->enableWarmStart = FALSE_FLAG;
		return SUCCESS_RETURN;
	}

	/* load the almanac files */
	if(LoadAlmanacsFromFiles(warmCfg,rx->gpsNavigationMessage,rx->galNavigationMessage)==FAILURE_RETURN)
	{
		// something went awry, warm start has been disabled by LoadAlmanacsFromFiles, do nothing
		return FAILURE_RETURN;
	}

	/* alloc and init */
	warmData->gpsPrediction = (TWarmStartPrediction *)malloc(GPS_NUMBER_OF_PRNS*sizeof(TWarmStartPrediction));
	warmData->galPrediction = (TWarmStartPrediction *)malloc(GALILEO_NUMBER_OF_PRNS*sizeof(TWarmStartPrediction));
	if((warmData->gpsPrediction==NULL) || (warmData->galPrediction==NULL))
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Malloc error in InitWarmStartData");
		CloseNGene(cfg,EXIT_FAILURE);
	}
	for(idx=0;idx<GPS_NUMBER_OF_PRNS;idx++)
	{
		warmData->gpsPrediction[idx].valid = FALSE_FLAG;
		warmData->gpsPrediction[idx].visible = FALSE_FLAG;
		warmData->gpsPrediction[idx].allocated = FALSE_FLAG;
	}
	for(idx=0;idx<GALILEO_NUMBER_OF_PRNS;idx++)
	{
		warmData->galPrediction[idx].valid = FALSE_FLAG;
		warmData->galPrediction[idx].visible = FALSE_FLAG;
		warmData->galPrediction[idx].allocated = FALSE_FLAG;
	}

	/* perform the first visibility prediction */
	UpdateVisibilityList(cfg,rx);

	return SUCCESS_RETURN;
}


void DeinitWarmStartData(TWarmStartData *warmData)
{
	if(warmData->gpsPrediction!=NULL)
	{
		free(warmData->gpsPrediction);
		warmData->gpsPrediction = NULL;
	}
	if(warmData->galPrediction!=NULL)
	{
		free(warmData->galPrediction);
		warmData->galPrediction = NULL;
	}
}


void UpdateVisibilityList(TNGeneConfiguration *cfg,TNGeneData *rx)
{
	TWarmStartConfiguration *warmCfg = &(cfg->warmConfiguration);
	TWarmStartData *warmData = &(rx->warmStartData);
	TPvtResults *pvtRes = &(rx->pvtResults);
	unsigned long long int discardedSamplesCurr = rx->buf.discardedSamples;
	double *rxEcefPosition;
	int gpsWn;
	double gpsTow;
	int galWn;
	double galTow;
	int idx;
	TNavigationMessage *navMesg;
	TWarmStartPrediction *prediction;
	int foundInPrnList;
	double elDeg;
	double azDeg;

	/* Early return if the visibility update is not required */
	if(warmCfg->enableWarmStart==FALSE_FLAG)
	{
		return;
	}
	if(discardedSamplesCurr < warmData->nextUpdate)
	{
		return;
	}

	/* Init receiver reference position */
	rxEcefPosition = (pvtRes->pvtValid==TRUE_FLAG) ? pvtRes->ecefPosition : warmCfg->warmEcef;
	if(pvtRes->rxGpsTime.referenceValid==TRUE_FLAG)
	{
		gpsWn = pvtRes->rxGpsTime.referenceWn;
		gpsTow = pvtRes->rxGpsTime.referenceTow;
	}
	else
	{
		gpsTow = warmCfg->gpsTow + ((double)discardedSamplesCurr/cfg->fs);
		gpsWn = warmCfg->gpsWn;
		while(gpsTow>=SECONDS_PER_WEEK)
		{
			gpsTow -= SECONDS_PER_WEEK;
			gpsWn++;
		}
	}
	if(pvtRes->rxGalTime.referenceValid==TRUE_FLAG)
	{
		galWn = pvtRes->rxGalTime.referenceWn;
		galTow = pvtRes->rxGalTime.referenceTow;
	}
	else
	{
		galTow = warmCfg->galTow + ((double)discardedSamplesCurr/cfg->fs);
		galWn = warmCfg->galWn;
		while(galTow>=SECONDS_PER_WEEK)
		{
			galTow -= SECONDS_PER_WEEK;
			galWn++;
		}
	}

	/* perform the visibility prediction */
	// GPS
	warmData->gpsNumVisible = 0;
	warmData->gpsNumAvailable = 0;
	for(idx=0;idx<GPS_NUMBER_OF_PRNS;idx++)
	{
		navMesg = &(rx->gpsNavigationMessage[idx]);
		prediction = &(warmData->gpsPrediction[idx]);
		if(navMesg->almanac1Received==FALSE_FLAG)
		{
			prediction->valid = FALSE_FLAG;
			prediction->visible = FALSE_FLAG;
			continue;
		}
		ComputeSatPositionFromAlmanac(GPS_SAT,navMesg,gpsTow,rxEcefPosition,prediction->ecefSatPos,prediction->ecefSatVel,&(prediction->azimuth),&(prediction->elevation));
		foundInPrnList = FindInPrnList(GPS_SAT,idx+BASE_PRN, &(rx->channelScheduler.prnList));
		prediction->valid = TRUE_FLAG;
		if((prediction->elevation>=warmCfg->minElevation) && (foundInPrnList==SUCCESS_RETURN))
		{
			/* set the visibility list to TRUE_FLAG only when
			 * 1) elevation is greater than minimum
			 * 2) the PRN is in the PRN list (the scheduler freezes is a visible PRN is not in the PRN list!) */
			prediction->visible = TRUE_FLAG;
			warmData->gpsNumVisible++;
			warmData->gpsNumAvailable = (prediction->allocated==TRUE_FLAG) ? (warmData->gpsNumAvailable) : (warmData->gpsNumAvailable+1);
		}
		else
		{
			prediction->visible = FALSE_FLAG;
		}
	}
	// Galileo
	warmData->galNumVisible = 0;
	warmData->galNumAvailable = 0;
	for(idx=0;idx<GALILEO_NUMBER_OF_PRNS;idx++)
	{
		navMesg = &(rx->galNavigationMessage[idx]);
		prediction = &(warmData->galPrediction[idx]);
		if((navMesg->almanac1Received==FALSE_FLAG) || (navMesg->almanac2Received==FALSE_FLAG))
		{
			prediction->valid = FALSE_FLAG;
			prediction->visible = FALSE_FLAG;
			continue;
		}
		ComputeSatPositionFromAlmanac(GALILEO_SAT,navMesg,galTow,rxEcefPosition,prediction->ecefSatPos,prediction->ecefSatVel,&(prediction->azimuth),&(prediction->elevation));
		foundInPrnList = FindInPrnList(GALILEO_SAT,idx+BASE_PRN, &(rx->channelScheduler.prnList));
		prediction->valid = TRUE_FLAG;
		if((prediction->elevation>=warmCfg->minElevation) && (foundInPrnList==SUCCESS_RETURN))
		{
			/* set the visibility list to TRUE_FLAG only when
			 * 1) elevation is greater than minimum
			 * 2) the PRN is in the PRN list (the scheduler freezes is a visible PRN is not in the PRN list!) */
			prediction->visible = TRUE_FLAG;
			warmData->galNumVisible++;
			warmData->galNumAvailable = (prediction->allocated==TRUE_FLAG) ? (warmData->galNumAvailable) : (warmData->galNumAvailable+1);
		}
		else
		{
			prediction->visible = FALSE_FLAG;
		}
	}

	/* feedback to the user */
	SwrxLog(NGENE_LOG_SEVERITY_INFO, "Performed the satellites' visibility prediction");
	// GPS
	for(idx=0;idx<GPS_NUMBER_OF_PRNS;idx++)
	{
		prediction = &(warmData->gpsPrediction[idx]);
		if((prediction->valid==FALSE_FLAG) || (prediction->visible==FALSE_FLAG))
		{
			continue;
		}
		elDeg = prediction->elevation * RADIAN_TO_DEGREE;
		azDeg = prediction->azimuth * RADIAN_TO_DEGREE;
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"  GPS     %2d ; el. %4.1f ; az. %5.1f",idx+BASE_PRN,elDeg,azDeg);
		SwrxLog(NGENE_LOG_SEVERITY_NOTE,"         ecef pos  m    %.1f %.1f %.1f",prediction->ecefSatPos[0],prediction->ecefSatPos[1],prediction->ecefSatPos[2]);
		SwrxLog(NGENE_LOG_SEVERITY_NOTE,"         ecef vel  m/s  %.1f %.1f %.1f",prediction->ecefSatVel[0],prediction->ecefSatVel[1],prediction->ecefSatVel[2]);
	}
	// Galileo
	for(idx=0;idx<GALILEO_NUMBER_OF_PRNS;idx++)
	{
		prediction = &(warmData->galPrediction[idx]);
		if((prediction->valid==FALSE_FLAG) || (prediction->visible==FALSE_FLAG))
		{
			continue;
		}
		elDeg = prediction->elevation * RADIAN_TO_DEGREE;
		azDeg = prediction->azimuth * RADIAN_TO_DEGREE;
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"  Galileo %2d ; el. %4.1f ; az. %5.1f",idx+BASE_PRN,elDeg,azDeg);
		SwrxLog(NGENE_LOG_SEVERITY_NOTE,"         ecef pos  m    %.1f %.1f %.1f",prediction->ecefSatPos[0],prediction->ecefSatPos[1],prediction->ecefSatPos[2]);
		SwrxLog(NGENE_LOG_SEVERITY_NOTE,"         ecef vel  m/s  %.1f %.1f %.1f",prediction->ecefSatVel[0],prediction->ecefSatVel[1],prediction->ecefSatVel[2]);
	}

	#ifdef DEBUG_WARM_START
	FILE *dbg = fopen("warmStartResults.txt","a");
	fprintf(dbg,"\n********\n");
	fprintf(dbg,"Samples %llu (current) vs %llu (update required)\n",discardedSamplesCurr,warmData->nextUpdate);
	fprintf(dbg,"GPS     WN %d  TOW %f\n",gpsWn,gpsTow);
	fprintf(dbg,"Galileo WN %d  TOW %f\n",galWn,galTow);
	fprintf(dbg,"RX pos ECEF %.1f %.1f %.1f\n",rxEcefPosition[0],rxEcefPosition[1],rxEcefPosition[2]);
	fprintf(dbg,"\n\n");
	//
	for(idx=0;idx<GPS_NUMBER_OF_PRNS;idx++)
	{
		prediction = &(warmData->gpsPrediction[idx]);
		if(prediction->valid==FALSE_FLAG)
		{
			continue;
		}
		fprintf(dbg,"  GPS %2d\n",idx+BASE_PRN);
		fprintf(dbg,"         ecef pos  m    %.1f %.1f %.1f\n",prediction->ecefSatPos[0],prediction->ecefSatPos[1],prediction->ecefSatPos[2]);
		fprintf(dbg,"         ecef vel  m/s  %.1f %.1f %.1f\n",prediction->ecefSatVel[0],prediction->ecefSatVel[1],prediction->ecefSatVel[2]);
		fprintf(dbg,"         elevation rad  %.5f\n",prediction->elevation);
		fprintf(dbg,"         azimuth   rad  %.5f\n",prediction->azimuth);
	}
	//
	for(idx=0;idx<GALILEO_NUMBER_OF_PRNS;idx++)
	{
		prediction = &(warmData->galPrediction[idx]);
		if(prediction->valid==FALSE_FLAG)
		{
			continue;
		}
		fprintf(dbg,"  Galileo %2d\n",idx+BASE_PRN);
		fprintf(dbg,"         ecef pos  m    %.1f %.1f %.1f\n",prediction->ecefSatPos[0],prediction->ecefSatPos[1],prediction->ecefSatPos[2]);
		fprintf(dbg,"         ecef vel  m/s  %.1f %.1f %.1f\n",prediction->ecefSatVel[0],prediction->ecefSatVel[1],prediction->ecefSatVel[2]);
		fprintf(dbg,"         elevation rad  %.5f\n",prediction->elevation);
		fprintf(dbg,"         azimuth   rad  %.5f\n",prediction->azimuth);
	}
	//
	fprintf(dbg,"\n\n");
	fclose(dbg);
	#endif

	warmData->nextUpdate += warmData->updateInterval;
}


int CheckSatVisibility(TWarmStartConfiguration *warmCfg,TWarmStartData *warmData,TSatConstellation satType,int satId)
{
	TWarmStartPrediction *prediction;

	/* Check 0: if warm start is disabled, always good */
	if(warmCfg->enableWarmStart==FALSE_FLAG)
		return TRUE_FLAG;

	/* Check 1: is satId is INVALID, then return TRUE_FLAG
	 *          (channel manager must not look for another one) */
	if(satId==INVALID_PRN)
		return TRUE_FLAG;

	/* Other checks depend on the constellation */
	if(satType==GPS_SAT)
	{
		prediction = &(warmData->gpsPrediction[satId-BASE_PRN]);

		/* Check 2: if allocated, no good (it should not happen, though) */
		if(prediction->allocated==TRUE_FLAG)
			return FALSE_FLAG;

		/* Check 3: if the prediction is not valid, good when no visible satellites are available */
		if((prediction->valid==FALSE_FLAG) && (warmData->gpsNumAvailable==0))
		{
			SwrxLog(NGENE_LOG_SEVERITY_NOTE, "Allocated GPS %2d not visible, not predicted",satId);
			prediction->allocated = TRUE_FLAG;
			return TRUE_FLAG;
		}
		else if(prediction->valid==FALSE_FLAG)
			return FALSE_FLAG;

		/* Check 4: if visible, is good */
		if(prediction->visible==TRUE_FLAG)
		{
			SwrxLog(NGENE_LOG_SEVERITY_NOTE, "Allocated GPS %2d visible",satId);
			prediction->allocated = TRUE_FLAG;
			warmData->gpsNumAvailable--;
			return TRUE_FLAG;
		}

		/* Check 5: if not visible, good only if all the visible ones are allocated */
		if((prediction->visible==FALSE_FLAG) && (warmData->gpsNumAvailable==0))
		{
			SwrxLog(NGENE_LOG_SEVERITY_NOTE, "Allocated GPS %2d not visible",satId);
			prediction->allocated = TRUE_FLAG;
			return TRUE_FLAG;
		}
		else
		{
			return FALSE_FLAG;
		}
	}
	else if(satType==GALILEO_SAT)
	{
		prediction = &(warmData->galPrediction[satId-BASE_PRN]);

		/* Check 2: if allocated, no good (it should not happen, though) */
		if(prediction->allocated==TRUE_FLAG)
			return FALSE_FLAG;

		/* Check 3: if the prediction is not valid, good when no visible satellites are available */
		if((prediction->valid==FALSE_FLAG) && (warmData->galNumAvailable==0))
		{
			SwrxLog(NGENE_LOG_SEVERITY_NOTE, "Allocated Gal %2d not visible, not predicted",satId);
			prediction->allocated = TRUE_FLAG;
			return TRUE_FLAG;
		}
		else if(prediction->valid==FALSE_FLAG)
			return FALSE_FLAG;

		/* Check 4: if visible, is good */
		if(prediction->visible==TRUE_FLAG)
		{
			SwrxLog(NGENE_LOG_SEVERITY_NOTE, "Allocated Gal %2d visible",satId);
			prediction->allocated = TRUE_FLAG;
			warmData->galNumAvailable--;
			return TRUE_FLAG;
		}

		/* Check 5: if not visible, good only if all the visible ones are allocated */
		if((prediction->visible==FALSE_FLAG) && (warmData->galNumAvailable==0))
		{
			SwrxLog(NGENE_LOG_SEVERITY_NOTE, "Allocated Gal %2d not visible",satId);
			prediction->allocated = TRUE_FLAG;
			return TRUE_FLAG;
		}
		else
		{
			return FALSE_FLAG;
		}
	}

	/* EGNOS or other conditions (if any) */
	return TRUE_FLAG;
}


void FreeInTheVisibilityList(TWarmStartConfiguration *warmCfg,TWarmStartData *warmData,TSatConstellation satType,int satId)
{
	TWarmStartPrediction *prediction;

	/* If warm is disabled, do nothing  */
	if(warmCfg->enableWarmStart==FALSE_FLAG)
		return;

	/* if INVALID_PRN, do nothing */
	if(satId==INVALID_PRN)
		return;

	if(satType==GPS_SAT)
	{
		prediction = &(warmData->gpsPrediction[satId-BASE_PRN]);
		prediction->allocated = FALSE_FLAG;
		if(prediction->visible==TRUE_FLAG)
			warmData->gpsNumAvailable++;
	}
	else if(satType==GALILEO_SAT)
	{
		prediction = &(warmData->galPrediction[satId-BASE_PRN]);
		prediction->allocated = FALSE_FLAG;
		if(prediction->visible==TRUE_FLAG)
			warmData->galNumAvailable++;
	}
}
