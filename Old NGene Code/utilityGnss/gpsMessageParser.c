/**
 * @file gpsMessageParser.c
 *
 * @author Mario Nicola
 *
 * @date 06/10/2014
 *
 * @version 1.0.0
 *
 * @brief Functions used to parse the GPS navigation message.
 */

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "ngeneConfiguration.h"
#include "ngeneDataStructure.h"
#include "channelDataStructure.h"
#include "navigationMessage.h"
#include "navigationMessageParser.h"
#include "ioFunctions.h"
#include "closeManager.h"
#include "gpsMessageParser.h"
#include "timeConversions.h"
#include "rinexLog.h"
#include "ifenLog.h"
#include "mackList.h"
#include "osnmaUtilities.h"



static unsigned int gpsParityMatrix[6] = {
    0xbb1f3480,
    0x5d8f9a40,
    0xaec7cd00,
    0x5763e680,
    0x6bb1f340,
    0x8b7a89c0
};


int InitGpsParser(TNGeneConfiguration *cfg,TChannel *ch)
{
	if(ch->satType!=GPS_SAT)
	{
		ch->gpsParser = NULL;
		return SUCCESS_RETURN;
	}
	ch->gpsParser = (TGpsParser *)malloc(sizeof(TGpsParser));
	if(ch->gpsParser==NULL)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Malloc returned a NULL pointer in InitGpsParser");
		CloseNGene(cfg,EXIT_FAILURE);
	}
	ch->gpsParser->navMsg=NULL;
	ch->gpsParser->wnCorrection = cfg->navMsgConfiguration.gpsWnCorrection;
	ch->gpsParser->gpsParseLogFile = NULL;
	return SUCCESS_RETURN;
}


void DeinitGpsParser(TChannel *ch)
{
	if(ch->gpsParser!=NULL)
	{
		if(ch->gpsParser->gpsParseLogFile!=NULL)
		{
			fclose(ch->gpsParser->gpsParseLogFile);
		}
		free(ch->gpsParser);
		ch->gpsParser=NULL;
	}
}

void StartGpsParser(TNGeneConfiguration *cfg,TNGeneData *rx,TChannel *ch)
{
	TGpsParser *parser = ch->gpsParser;
	char filename[2*MAX_ROW_LENGTH];

	if(ch->satType!=GPS_SAT)
		return;

	if(ch->satId==INVALID_PRN)
		return;

	parser->bitIndexTotal = 0;
	parser->lastWord = 0;
	parser->bitIndex = -2; /* in order to take into account the last two parity bits of the previous word */
	parser->wordIndex = 0;
	parser->crcPassed = 0;
	parser->status = SEARCH_SYNCHRONIZATION;
	parser->navMsg = &(rx->gpsNavigationMessage[ch->satId-BASE_PRN]);
	parser->navMsg->towValid=FALSE_FLAG;
	parser->navMsg->wnValid=FALSE_FLAG;
	// osnma
	parser->lastSavedSubframe = 0; // not valid
	/* TODO qui si può aggiungere la predizione dei contatori */
	// log
	if(cfg->logConfiguration.enableNavMesgLog==TRUE_FLAG)
	{
		if(parser->gpsParseLogFile!=NULL)
		{
			fclose(parser->gpsParseLogFile);
		}
		sprintf(filename,"%s/navigationMessage_GPS%02d.txt",cfg->dumpPathCurrent,ch->satId);
		parser->gpsParseLogFile = fopen(filename,"a");
	}
}


TParseResult GpsSynchronization(TGpsParser *parser,unsigned int symbol)
{
	parser->lastWord = ((parser->lastWord<<1) | symbol) & (unsigned int)0x03ff;
	if(parser->bitIndex<GPS_PREAMBLE_LENGTH)
	{
		parser->bitIndex++;
	}
	if(parser->bitIndex==GPS_PREAMBLE_LENGTH)
	{
		if((parser->lastWord & 0x0ff)==GPS_PREAMBLE_BITS)
		{
			parser->wordIndex = 0;
			parser->bitIndexTotal=GPS_PREAMBLE_LENGTH;
			return PREAMBLE_FOUND;
		}
		if((parser->lastWord & 0x0ff)==GPS_INV_PREAMBLE_BITS)
		{
			parser->wordIndex = 0;
			parser->bitIndexTotal=GPS_PREAMBLE_LENGTH;
			return PREAMBLE_FOUND;
		}
	}
	return PREAMBLE_NOT_FOUND;
}


unsigned int OneCount32(unsigned int num)
{
	unsigned int mask = 0x80000000;
	unsigned int cnt = 0;

	while(mask>0)
	{
		if((num&mask)!=0)
			cnt++;
		mask=(mask>>1);
	}
	return cnt;
}


int GpsParityVerify(unsigned int *wordPtr)
{
    unsigned int tmp, cnt;
    int i;
    unsigned int computedParity;

    cnt = 0;
    computedParity = 0;
    if((*wordPtr) & 0x40000000)
    {
    	/* Correct word polarity */
		(*wordPtr) = (*wordPtr) ^ 0x3fffffc0;
    }
    for(i=0;i<6;i++)
    {
		tmp = (*wordPtr) & gpsParityMatrix[i];
		cnt = OneCount32(tmp); /* Count the number of bits equal to one */
		(computedParity) += ((cnt & 0x01) << (5 - i));
    }
    /* Compare the computed parity and the received one (last 6 bits) */
    if((computedParity)==((*wordPtr) & 0x0000003f))
    {
		return SUCCESS_RETURN;
    }
    return FAILURE_RETURN;
}


int ParseGpsSubframe1(TGpsParser *parser)
{
	TNavigationMessage *navMsg = parser->navMsg;
	int health;
	TGnssEphemerides *eph = &(navMsg->ephTmp);
	TGnssClk *clk = &(navMsg->clkTmp);

	navMsg->wn = ExtractField(parser->subframe,GPS_SUBF1_WN_FIRST_BYTE_IDX,GPS_SUBF1_WN_LAST_BYTE_IDX,GPS_SUBF1_WN_BITS_OF_FIRST_BYTE,GPS_SUBF1_WN_BITS_OF_LAST_BYTE);
	navMsg->wn += parser->wnCorrection;
	navMsg->wnValid=TRUE_FLAG;
	health = ExtractField(parser->subframe,GPS_SUBF1_HEALTH_FIRST_BYTE_IDX,GPS_SUBF1_HEALTH_LAST_BYTE_IDX,GPS_SUBF1_HEALTH_BITS_OF_FIRST_BYTE,GPS_SUBF1_HEALTH_BITS_OF_LAST_BYTE);
	if((health & 0x020)==0)
	{
		eph->health = SIGNAL_OK;
	}
	else
	{
		eph->health = SIGNAL_OUT_OF_SERVICE;
	}
	eph->sisa = ExtractField(parser->subframe,GPS_SUBF1_URA_FIRST_BYTE_IDX,GPS_SUBF1_URA_LAST_BYTE_IDX,GPS_SUBF1_URA_BITS_OF_FIRST_BYTE,GPS_SUBF1_URA_BITS_OF_LAST_BYTE);
	clk->iodc = 256.0*ExtractField(parser->subframe,GPS_SUBF1_IODC_MS_FIRST_BYTE_IDX,GPS_SUBF1_IODC_MS_LAST_BYTE_IDX,GPS_SUBF1_IODC_MS_BITS_OF_FIRST_BYTE,GPS_SUBF1_IODC_MS_BITS_OF_LAST_BYTE);
	clk->iodc += ExtractField(parser->subframe,GPS_SUBF1_IODC_LS_FIRST_BYTE_IDX,GPS_SUBF1_IODC_LS_LAST_BYTE_IDX,GPS_SUBF1_IODC_LS_BITS_OF_FIRST_BYTE,GPS_SUBF1_IODC_LS_BITS_OF_LAST_BYTE);
	clk->toc = 16.0*ExtractField(parser->subframe,GPS_SUBF1_TOC_FIRST_BYTE_IDX,GPS_SUBF1_TOC_LAST_BYTE_IDX,GPS_SUBF1_TOC_BITS_OF_FIRST_BYTE,GPS_SUBF1_TOC_BITS_OF_LAST_BYTE);
	clk->af[0] = POW_TWO_M31*ExtractTwoCompField(parser->subframe,GPS_SUBF1_AF0_FIRST_BYTE_IDX,GPS_SUBF1_AF0_LAST_BYTE_IDX,GPS_SUBF1_AF0_BITS_OF_FIRST_BYTE,GPS_SUBF1_AF0_BITS_OF_LAST_BYTE,GPS_SUBF1_AF0_FIELD_DIM);
	clk->af[1] = POW_TWO_M43*ExtractTwoCompField(parser->subframe,GPS_SUBF1_AF1_FIRST_BYTE_IDX,GPS_SUBF1_AF1_LAST_BYTE_IDX,GPS_SUBF1_AF1_BITS_OF_FIRST_BYTE,GPS_SUBF1_AF1_BITS_OF_LAST_BYTE,GPS_SUBF1_AF1_FIELD_DIM);
	clk->af[2] = POW_TWO_M55*ExtractTwoCompField(parser->subframe,GPS_SUBF1_AF2_FIRST_BYTE_IDX,GPS_SUBF1_AF2_LAST_BYTE_IDX,GPS_SUBF1_AF2_BITS_OF_FIRST_BYTE,GPS_SUBF1_AF2_BITS_OF_LAST_BYTE,GPS_SUBF1_AF2_FIELD_DIM);
	clk->tGroupDelay = POW_TWO_M31*ExtractTwoCompField(parser->subframe,GPS_SUBF1_TGD_FIRST_BYTE_IDX,GPS_SUBF1_TGD_LAST_BYTE_IDX,GPS_SUBF1_TGD_BITS_OF_FIRST_BYTE,GPS_SUBF1_TGD_BITS_OF_LAST_BYTE,GPS_SUBF1_TGD_FIELD_DIM);

	if(clk->toc>GPS_MAXIMUM_TOC)
	{
		return FAILURE_RETURN;
	}

	navMsg->subframe1Received=TRUE_FLAG;

	if(parser->gpsParseLogFile!=NULL)
	{
		fprintf(parser->gpsParseLogFile,"Subframe 1\n");
		fprintf(parser->gpsParseLogFile,"    TOW %u\n", navMsg->tow);
		fprintf(parser->gpsParseLogFile,"    gpsWeek %d\n", navMsg->wn);
		fprintf(parser->gpsParseLogFile,"    ephemSVhealth %d\n", eph->health);
		fprintf(parser->gpsParseLogFile,"    ephemURA %d\n", eph->sisa);
		fprintf(parser->gpsParseLogFile,"    ephemIODC %d (%d)\n", clk->iodc, clk->iodc & 0x0ff);
		fprintf(parser->gpsParseLogFile,"    ephemToc %.20g\n", clk->toc);
		fprintf(parser->gpsParseLogFile,"    ephemAF0 %.20g\n", clk->af[0]);
		fprintf(parser->gpsParseLogFile,"    ephemAF1 %.20g\n", clk->af[1]);
		fprintf(parser->gpsParseLogFile,"    ephemAF2 %.20g\n", clk->af[2]);
		fprintf(parser->gpsParseLogFile,"    ephemTgd %.20g\n", clk->tGroupDelay);
		fprintf(parser->gpsParseLogFile,"\n");
	}

	return SUCCESS_RETURN;
}


int ParseGpsSubframe2(TGpsParser *parser)
{
	TNavigationMessage *navMsg = parser->navMsg;
	TGnssEphemerides *eph = &(navMsg->ephTmp);

	eph->iode2 = ExtractField(parser->subframe,GPS_SUBF2_IODE_FIRST_BYTE_IDX,GPS_SUBF2_IODE_LAST_BYTE_IDX,GPS_SUBF2_IODE_BITS_OF_FIRST_BYTE,GPS_SUBF2_IODE_BITS_OF_LAST_BYTE);
	eph->capitalCrs = POW_TWO_M05 * ExtractTwoCompField(parser->subframe,GPS_SUBF2_CRS_FIRST_BYTE_IDX,GPS_SUBF2_CRS_LAST_BYTE_IDX,GPS_SUBF2_CRS_BITS_OF_FIRST_BYTE,GPS_SUBF2_CRS_BITS_OF_LAST_BYTE,GPS_SUBF2_CRS_FIELD_DIM);
	eph->deltaN = POW_TWO_M43 * M_PI * ExtractTwoCompField(parser->subframe,GPS_SUBF2_DN_FIRST_BYTE_IDX,GPS_SUBF2_DN_LAST_BYTE_IDX,GPS_SUBF2_DN_BITS_OF_FIRST_BYTE,GPS_SUBF2_DN_BITS_OF_LAST_BYTE,GPS_SUBF2_DN_FIELD_DIM);
	eph->meanAnomaly0 =  POW_TWO_P24 * ExtractTwoCompField(parser->subframe,GPS_SUBF2_M0_MS_FIRST_BYTE_IDX,GPS_SUBF2_M0_MS_LAST_BYTE_IDX,GPS_SUBF2_M0_MS_BITS_OF_FIRST_BYTE,GPS_SUBF2_M0_MS_BITS_OF_LAST_BYTE,GPS_SUBF2_M0_MS_FIELD_DIM);
	eph->meanAnomaly0 += ExtractField(parser->subframe,GPS_SUBF2_M0_LS_FIRST_BYTE_IDX,GPS_SUBF2_M0_LS_LAST_BYTE_IDX,GPS_SUBF2_M0_LS_BITS_OF_FIRST_BYTE,GPS_SUBF2_M0_LS_BITS_OF_LAST_BYTE);
	eph->meanAnomaly0 *= POW_TWO_M31 * M_PI;
	eph->capitalCuc = POW_TWO_M29 * ExtractTwoCompField(parser->subframe,GPS_SUBF2_CUC_FIRST_BYTE_IDX,GPS_SUBF2_CUC_LAST_BYTE_IDX,GPS_SUBF2_CUC_BITS_OF_FIRST_BYTE,GPS_SUBF2_CUC_BITS_OF_LAST_BYTE,GPS_SUBF2_CUC_FIELD_DIM);
	eph->eccentricity = POW_TWO_P24 * ExtractField(parser->subframe,GPS_SUBF2_E_MS_FIRST_BYTE_IDX,GPS_SUBF2_E_MS_LAST_BYTE_IDX,GPS_SUBF2_E_MS_BITS_OF_FIRST_BYTE,GPS_SUBF2_E_MS_BITS_OF_LAST_BYTE);
	eph->eccentricity += ExtractField(parser->subframe,GPS_SUBF2_E_LS_FIRST_BYTE_IDX,GPS_SUBF2_E_LS_LAST_BYTE_IDX,GPS_SUBF2_E_LS_BITS_OF_FIRST_BYTE,GPS_SUBF2_E_LS_BITS_OF_LAST_BYTE);
	eph->eccentricity *= POW_TWO_M33;
	eph->capitalCus = POW_TWO_M29 * ExtractTwoCompField(parser->subframe,GPS_SUBF2_CUS_FIRST_BYTE_IDX,GPS_SUBF2_CUS_LAST_BYTE_IDX,GPS_SUBF2_CUS_BITS_OF_FIRST_BYTE,GPS_SUBF2_CUS_BITS_OF_LAST_BYTE,GPS_SUBF2_CUS_FIELD_DIM);
	eph->sqrtA = POW_TWO_P24 * ExtractField(parser->subframe,GPS_SUBF2_SQRTA_MS_FIRST_BYTE_IDX,GPS_SUBF2_SQRTA_MS_LAST_BYTE_IDX,GPS_SUBF2_SQRTA_MS_BITS_OF_FIRST_BYTE,GPS_SUBF2_SQRTA_MS_BITS_OF_LAST_BYTE);
	eph->sqrtA += ExtractField(parser->subframe,GPS_SUBF2_SQRTA_LS_FIRST_BYTE_IDX,GPS_SUBF2_SQRTA_LS_LAST_BYTE_IDX,GPS_SUBF2_SQRTA_LS_BITS_OF_FIRST_BYTE,GPS_SUBF2_SQRTA_LS_BITS_OF_LAST_BYTE);
	eph->sqrtA *= POW_TWO_M19;
	eph->toe = 16.0 * ExtractField(parser->subframe,GPS_SUBF2_TOE_FIRST_BYTE_IDX,GPS_SUBF2_TOE_LAST_BYTE_IDX,GPS_SUBF2_TOE_BITS_OF_FIRST_BYTE,GPS_SUBF2_TOE_BITS_OF_LAST_BYTE);

	if(eph->eccentricity>GPS_MAXIMUM_ECCENTRICITY)
		return FAILURE_RETURN;

	if(eph->toe>GPS_MAXIMUM_TOC)
		return FAILURE_RETURN;

	navMsg->subframe2Received = TRUE_FLAG;

	if(parser->gpsParseLogFile!=NULL)
	{
		fprintf(parser->gpsParseLogFile,"Subframe 2\n");
		fprintf(parser->gpsParseLogFile,"    TOW %u\n", navMsg->tow);
		fprintf(parser->gpsParseLogFile,"    IODE2 %d\n", eph->iode2);
		fprintf(parser->gpsParseLogFile,"    ephemCrs %.20g\n", eph->capitalCrs);
		fprintf(parser->gpsParseLogFile,"    ephemDeltaN %.20g\n", eph->deltaN);
		fprintf(parser->gpsParseLogFile,"    ephemM0 %.20g\n", eph->meanAnomaly0);
		fprintf(parser->gpsParseLogFile,"    ephemCuc %.20g\n", eph->capitalCuc);
		fprintf(parser->gpsParseLogFile,"    ephemE %.20g\n", eph->eccentricity);
		fprintf(parser->gpsParseLogFile,"    ephemCus %.20g\n", eph->capitalCus);
		fprintf(parser->gpsParseLogFile,"    ephemAPowerHalf %.20g\n", eph->sqrtA);
		fprintf(parser->gpsParseLogFile,"    ephemToe %.20g\n", eph->toe);
		fprintf(parser->gpsParseLogFile,"\n");
	}

	return SUCCESS_RETURN;
}


int ParseGpsSubframe3(TGpsParser *parser)
{
	TNavigationMessage *navMsg = parser->navMsg;
	TGnssEphemerides *eph = &(navMsg->ephTmp);

    eph->capitalCic = POW_TWO_M29 * ExtractTwoCompField(parser->subframe,GPS_SUBF3_CIC_FIRST_BYTE_IDX,GPS_SUBF3_CIC_LAST_BYTE_IDX,GPS_SUBF3_CIC_BITS_OF_FIRST_BYTE,GPS_SUBF3_CIC_BITS_OF_LAST_BYTE,GPS_SUBF3_CIC_FIELD_DIM);
    eph->capitalOmega0 =  POW_TWO_P24 * ExtractTwoCompField(parser->subframe,GPS_SUBF3_OMEGA0_MS_FIRST_BYTE_IDX,GPS_SUBF3_OMEGA0_MS_LAST_BYTE_IDX,GPS_SUBF3_OMEGA0_MS_BITS_OF_FIRST_BYTE,GPS_SUBF3_OMEGA0_MS_BITS_OF_LAST_BYTE,GPS_SUBF3_OMEGA0_MS_FIELD_DIM);
    eph->capitalOmega0 += ExtractField(parser->subframe,GPS_SUBF3_OMEGA0_LS_FIRST_BYTE_IDX,GPS_SUBF3_OMEGA0_LS_LAST_BYTE_IDX,GPS_SUBF3_OMEGA0_LS_BITS_OF_FIRST_BYTE,GPS_SUBF3_OMEGA0_LS_BITS_OF_LAST_BYTE);
    eph->capitalOmega0 *= POW_TWO_M31 * M_PI;
    eph->capitalCis = POW_TWO_M29 * ExtractTwoCompField(parser->subframe,GPS_SUBF3_CIS_FIRST_BYTE_IDX,GPS_SUBF3_CIS_LAST_BYTE_IDX,GPS_SUBF3_CIS_BITS_OF_FIRST_BYTE,GPS_SUBF3_CIS_BITS_OF_LAST_BYTE,GPS_SUBF3_CIS_FIELD_DIM);
    eph->i0 = POW_TWO_P24 * ExtractTwoCompField(parser->subframe,GPS_SUBF3_I0_MS_FIRST_BYTE_IDX,GPS_SUBF3_I0_MS_LAST_BYTE_IDX,GPS_SUBF3_I0_MS_BITS_OF_FIRST_BYTE,GPS_SUBF3_I0_MS_BITS_OF_LAST_BYTE,GPS_SUBF3_I0_MS_FIELD_DIM);
    eph->i0 += ExtractField(parser->subframe,GPS_SUBF3_I0_LS_FIRST_BYTE_IDX,GPS_SUBF3_I0_LS_LAST_BYTE_IDX,GPS_SUBF3_I0_LS_BITS_OF_FIRST_BYTE,GPS_SUBF3_I0_LS_BITS_OF_LAST_BYTE);
    eph->i0 *= POW_TWO_M31 * M_PI;
    eph->capitalCrc = POW_TWO_M05 * ExtractTwoCompField(parser->subframe,GPS_SUBF3_CRC_FIRST_BYTE_IDX,GPS_SUBF3_CRC_LAST_BYTE_IDX,GPS_SUBF3_CRC_BITS_OF_FIRST_BYTE,GPS_SUBF3_CRC_BITS_OF_LAST_BYTE,GPS_SUBF3_CRC_FIELD_DIM);
    eph->omega = POW_TWO_P24 * ExtractTwoCompField(parser->subframe,GPS_SUBF3_OMEGA_MS_FIRST_BYTE_IDX,GPS_SUBF3_OMEGA_MS_LAST_BYTE_IDX,GPS_SUBF3_OMEGA_MS_BITS_OF_FIRST_BYTE,GPS_SUBF3_OMEGA_MS_BITS_OF_LAST_BYTE,GPS_SUBF3_OMEGA_MS_FIELD_DIM);
    eph->omega += ExtractField(parser->subframe,GPS_SUBF3_OMEGA_LS_FIRST_BYTE_IDX,GPS_SUBF3_OMEGA_LS_LAST_BYTE_IDX,GPS_SUBF3_OMEGA_LS_BITS_OF_FIRST_BYTE,GPS_SUBF3_OMEGA_LS_BITS_OF_LAST_BYTE);
    eph->omega *= POW_TWO_M31 * M_PI;
    eph->capitalOmegaDot = POW_TWO_M43 * M_PI * ExtractTwoCompField(parser->subframe,GPS_SUBF3_OMEGADOT_FIRST_BYTE_IDX,GPS_SUBF3_OMEGADOT_LAST_BYTE_IDX,GPS_SUBF3_OMEGADOT_BITS_OF_FIRST_BYTE,GPS_SUBF3_OMEGADOT_BITS_OF_LAST_BYTE,GPS_SUBF3_OMEGADOT_FIELD_DIM);
    eph->iode3 = ExtractField(parser->subframe,GPS_SUBF3_IODE_FIRST_BYTE_IDX,GPS_SUBF3_IODE_LAST_BYTE_IDX,GPS_SUBF3_IODE_BITS_OF_FIRST_BYTE,GPS_SUBF3_IODE_BITS_OF_LAST_BYTE);
    eph->iDot = POW_TWO_M43 * M_PI * ExtractTwoCompField(parser->subframe,GPS_SUBF3_IDOT_FIRST_BYTE_IDX,GPS_SUBF3_IDOT_LAST_BYTE_IDX,GPS_SUBF3_IDOT_BITS_OF_FIRST_BYTE,GPS_SUBF3_IDOT_BITS_OF_LAST_BYTE,GPS_SUBF3_IDOT_FIELD_DIM);

    navMsg->subframe3Received = TRUE_FLAG;

	if(parser->gpsParseLogFile!=NULL)
	{
		fprintf(parser->gpsParseLogFile,"Subframe 3\n");
		fprintf(parser->gpsParseLogFile,"    TOW %u\n", navMsg->tow);
		fprintf(parser->gpsParseLogFile,"    ephemCic %.20g\n", eph->capitalCic);
		fprintf(parser->gpsParseLogFile,"    ephemOmega0 %.20g\n", eph->capitalOmega0);
		fprintf(parser->gpsParseLogFile,"    ephemCis %.20g\n", eph->capitalCis);
		fprintf(parser->gpsParseLogFile,"    ephemI0 %.20g\n", eph->i0);
		fprintf(parser->gpsParseLogFile,"    ephemCrc %.20g\n", eph->capitalCrc);
		fprintf(parser->gpsParseLogFile,"    ephemOmega %.20g\n", eph->omega);
		fprintf(parser->gpsParseLogFile,"    ephemOmegaDot %.20g\n", eph->capitalOmegaDot);
		fprintf(parser->gpsParseLogFile,"    IODE3 %d\n", eph->iode3);
		fprintf(parser->gpsParseLogFile,"    ephemIDot %.20g\n", eph->iDot);
		fprintf(parser->gpsParseLogFile,"\n");
	}

	return SUCCESS_RETURN;
}


int ParseGpsSubframe4(TNGeneData *rx,TGpsParser *parser)
{
	TNavigationMessage *navMsg = parser->navMsg;
	TGnssIono *iono = &(navMsg->iono);
	TGnssToUtcTimeConversion *gnss2Utc = &(navMsg->gnss2Utc);
	TGnssAlmanac *alm;
	int pageId;
	int health;
	int dataId;
	int index;
	
	dataId = ExtractField(parser->subframe,GPS_SUBF45_DATAID_FIRST_BYTE_IDX,GPS_SUBF45_DATAID_LAST_BYTE_IDX,GPS_SUBF45_DATAID_BITS_OF_FIRST_BYTE,GPS_SUBF45_DATAID_BITS_OF_LAST_BYTE);
	pageId = ExtractField(parser->subframe,GPS_SUBF45_PAGEID_FIRST_BYTE_IDX,GPS_SUBF45_PAGEID_LAST_BYTE_IDX,GPS_SUBF45_PAGEID_BITS_OF_FIRST_BYTE,GPS_SUBF45_PAGEID_BITS_OF_LAST_BYTE);

	if((pageId>=25) && (pageId<=32))
	{
		alm=&(rx->gpsNavigationMessage[pageId-BASE_PRN].almanac);
		alm->eccentricity = POW_TWO_M21 * ExtractField(parser->subframe,GPS_SUBF45_E_FIRST_BYTE_IDX,GPS_SUBF45_E_LAST_BYTE_IDX,GPS_SUBF45_E_BITS_OF_FIRST_BYTE,GPS_SUBF45_E_BITS_OF_LAST_BYTE);
		alm->toa = POW_TWO_P12 * ExtractField(parser->subframe,GPS_SUBF45_TOA_FIRST_BYTE_IDX,GPS_SUBF45_TOA_LAST_BYTE_IDX,GPS_SUBF45_TOA_BITS_OF_FIRST_BYTE,GPS_SUBF45_TOA_BITS_OF_LAST_BYTE);
		alm->i0 = POW_TWO_M19 * M_PI * ExtractTwoCompField(parser->subframe,GPS_SUBF45_I0_FIRST_BYTE_IDX,GPS_SUBF45_I0_LAST_BYTE_IDX,GPS_SUBF45_I0_BITS_OF_FIRST_BYTE,GPS_SUBF45_I0_BITS_OF_LAST_BYTE,GPS_SUBF45_I0_FIELD_DIM);
		alm->i0 += (GPS_NOMINAL_I0*M_PI);
		alm->capitalOmegaDot = POW_TWO_M38 * M_PI * ExtractTwoCompField(parser->subframe,GPS_SUBF45_OMEGADOT_FIRST_BYTE_IDX,GPS_SUBF45_OMEGADOT_LAST_BYTE_IDX,GPS_SUBF45_OMEGADOT_BITS_OF_FIRST_BYTE,GPS_SUBF45_OMEGADOT_BITS_OF_LAST_BYTE,GPS_SUBF45_OMEGADOT_FIELD_DIM);
		health = ExtractField(parser->subframe,GPS_SUBF45_HEALTH_FIRST_BYTE_IDX,GPS_SUBF45_HEALTH_LAST_BYTE_IDX,GPS_SUBF45_HEALTH_BITS_OF_FIRST_BYTE,GPS_SUBF45_HEALTH_BITS_OF_LAST_BYTE);
		if((health & 0x0e0)==0)
		{
			alm->health = SIGNAL_OK;
		}
		else
		{
			alm->health = SIGNAL_OUT_OF_SERVICE;
		}
		alm->sqrtA = POW_TWO_M11 * ExtractField(parser->subframe,GPS_SUBF45_SQRTA_FIRST_BYTE_IDX,GPS_SUBF45_SQRTA_LAST_BYTE_IDX,GPS_SUBF45_SQRTA_BITS_OF_FIRST_BYTE,GPS_SUBF45_SQRTA_BITS_OF_LAST_BYTE);
		alm->capitalOmega0 = POW_TWO_M23 * M_PI * ExtractTwoCompField(parser->subframe,GPS_SUBF45_OMEGA0_FIRST_BYTE_IDX,GPS_SUBF45_OMEGA0_LAST_BYTE_IDX,GPS_SUBF45_OMEGA0_BITS_OF_FIRST_BYTE,GPS_SUBF45_OMEGA0_BITS_OF_LAST_BYTE,GPS_SUBF45_OMEGA0_FIELD_DIM);
		alm->omega = POW_TWO_M23 * M_PI * ExtractTwoCompField(parser->subframe,GPS_SUBF45_OMEGA_FIRST_BYTE_IDX,GPS_SUBF45_OMEGA_LAST_BYTE_IDX,GPS_SUBF45_OMEGA_BITS_OF_FIRST_BYTE,GPS_SUBF45_OMEGA_BITS_OF_LAST_BYTE,GPS_SUBF45_OMEGA_FIELD_DIM);
		/* TODO indagare sul perchè M0 dell'almanacco differisce dal valore nel subframe 2:
		 *      non ci sono errori evidenti nei parametri. M0 è uguale quale che sia il satellite che la trasmette.
		 *      Problema dell'Ifen? Idem per M0 nel subframe 5. */
		alm->meanAnomaly0 = POW_TWO_M23 * M_PI * ExtractTwoCompField(parser->subframe,GPS_SUBF45_M0_FIRST_BYTE_IDX,GPS_SUBF45_M0_LAST_BYTE_IDX,GPS_SUBF45_M0_BITS_OF_FIRST_BYTE,GPS_SUBF45_M0_BITS_OF_LAST_BYTE,GPS_SUBF45_M0_FIELD_DIM);
		alm->af[0] = POW_TWO_P03 * ExtractTwoCompField(parser->subframe,GPS_SUBF45_AF0_MS_FIRST_BYTE_IDX,GPS_SUBF45_AF0_MS_LAST_BYTE_IDX,GPS_SUBF45_AF0_MS_BITS_OF_FIRST_BYTE,GPS_SUBF45_AF0_MS_BITS_OF_LAST_BYTE,GPS_SUBF45_AF0_MS_FIELD_DIM);
		alm->af[0] += ExtractField(parser->subframe,GPS_SUBF45_AF0_LS_FIRST_BYTE_IDX,GPS_SUBF45_AF0_LS_LAST_BYTE_IDX,GPS_SUBF45_AF0_LS_BITS_OF_FIRST_BYTE,GPS_SUBF45_AF0_LS_BITS_OF_LAST_BYTE);
		alm->af[0] *= POW_TWO_M20;
		alm->af[1] = POW_TWO_M38 * ExtractTwoCompField(parser->subframe,GPS_SUBF45_AF1_FIRST_BYTE_IDX,GPS_SUBF45_AF1_LAST_BYTE_IDX,GPS_SUBF45_AF1_BITS_OF_FIRST_BYTE,GPS_SUBF45_AF1_BITS_OF_LAST_BYTE,GPS_SUBF45_AF1_FIELD_DIM);

		rx->gpsNavigationMessage[pageId-BASE_PRN].almanac1Received = TRUE_FLAG;
		rx->gpsNavigationMessage[pageId-BASE_PRN].almanac2Received = TRUE_FLAG;

		if(parser->gpsParseLogFile!=NULL)
		{
			fprintf(parser->gpsParseLogFile,"Subframe 4 (pageSvId %d, dataId %d)\n",pageId,dataId);
			fprintf(parser->gpsParseLogFile,"    TOW %u\n", navMsg->tow);
			fprintf(parser->gpsParseLogFile,"    almE %.20g\n", alm->eccentricity);
			fprintf(parser->gpsParseLogFile,"    almToa %.20g\n", alm->toa);
			fprintf(parser->gpsParseLogFile,"    almI0 %.20g\n", alm->i0);
			fprintf(parser->gpsParseLogFile,"    almOmegaDot %.20g\n", alm->capitalOmegaDot);
			fprintf(parser->gpsParseLogFile,"    almHealth %d\n", alm->health);
			fprintf(parser->gpsParseLogFile,"    almAPowerHalf %.20g\n", alm->sqrtA);
			fprintf(parser->gpsParseLogFile,"    almOmega0 %.20g\n", alm->capitalOmega0);
			fprintf(parser->gpsParseLogFile,"    almOmega %.20g\n", alm->omega);
			fprintf(parser->gpsParseLogFile,"    almM0 %.20g\n", alm->meanAnomaly0);
			fprintf(parser->gpsParseLogFile,"    almAF0 %.20g\n", alm->af[0]);
			fprintf(parser->gpsParseLogFile,"    almAF1 %.20g\n", alm->af[1]);
			fprintf(parser->gpsParseLogFile,"\n");
		}
	}
	else if(pageId==56)
	{
		iono->alphaGps[0] = POW_TWO_M30 * ExtractTwoCompField(parser->subframe,GPS_SUBF4_ALFA0_FIRST_BYTE_IDX,GPS_SUBF4_ALFA0_LAST_BYTE_IDX,GPS_SUBF4_ALFA0_BITS_OF_FIRST_BYTE,GPS_SUBF4_ALFA0_BITS_OF_LAST_BYTE,GPS_SUBF4_ALFA0_FIELD_DIM);
		iono->alphaGps[1] = POW_TWO_M27 * ExtractTwoCompField(parser->subframe,GPS_SUBF4_ALFA1_FIRST_BYTE_IDX,GPS_SUBF4_ALFA1_LAST_BYTE_IDX,GPS_SUBF4_ALFA1_BITS_OF_FIRST_BYTE,GPS_SUBF4_ALFA1_BITS_OF_LAST_BYTE,GPS_SUBF4_ALFA1_FIELD_DIM);
		iono->alphaGps[2] = POW_TWO_M24 * ExtractTwoCompField(parser->subframe,GPS_SUBF4_ALFA2_FIRST_BYTE_IDX,GPS_SUBF4_ALFA2_LAST_BYTE_IDX,GPS_SUBF4_ALFA2_BITS_OF_FIRST_BYTE,GPS_SUBF4_ALFA2_BITS_OF_LAST_BYTE,GPS_SUBF4_ALFA2_FIELD_DIM);
		iono->alphaGps[3] = POW_TWO_M24 * ExtractTwoCompField(parser->subframe,GPS_SUBF4_ALFA3_FIRST_BYTE_IDX,GPS_SUBF4_ALFA3_LAST_BYTE_IDX,GPS_SUBF4_ALFA3_BITS_OF_FIRST_BYTE,GPS_SUBF4_ALFA3_BITS_OF_LAST_BYTE,GPS_SUBF4_ALFA3_FIELD_DIM);
		iono->beta[0] = POW_TWO_P11 * ExtractTwoCompField(parser->subframe,GPS_SUBF4_BETA0_FIRST_BYTE_IDX,GPS_SUBF4_BETA0_LAST_BYTE_IDX,GPS_SUBF4_BETA0_BITS_OF_FIRST_BYTE,GPS_SUBF4_BETA0_BITS_OF_LAST_BYTE,GPS_SUBF4_BETA0_FIELD_DIM);
		iono->beta[1] = POW_TWO_P14 * ExtractTwoCompField(parser->subframe,GPS_SUBF4_BETA1_FIRST_BYTE_IDX,GPS_SUBF4_BETA1_LAST_BYTE_IDX,GPS_SUBF4_BETA1_BITS_OF_FIRST_BYTE,GPS_SUBF4_BETA1_BITS_OF_LAST_BYTE,GPS_SUBF4_BETA1_FIELD_DIM);
		iono->beta[2] = POW_TWO_P16 * ExtractTwoCompField(parser->subframe,GPS_SUBF4_BETA2_FIRST_BYTE_IDX,GPS_SUBF4_BETA2_LAST_BYTE_IDX,GPS_SUBF4_BETA2_BITS_OF_FIRST_BYTE,GPS_SUBF4_BETA2_BITS_OF_LAST_BYTE,GPS_SUBF4_BETA2_FIELD_DIM);
		iono->beta[3] = POW_TWO_P16 * ExtractTwoCompField(parser->subframe,GPS_SUBF4_BETA3_FIRST_BYTE_IDX,GPS_SUBF4_BETA3_LAST_BYTE_IDX,GPS_SUBF4_BETA3_BITS_OF_FIRST_BYTE,GPS_SUBF4_BETA3_BITS_OF_LAST_BYTE,GPS_SUBF4_BETA3_FIELD_DIM);
		gnss2Utc->a0 = POW_TWO_P08 * ExtractTwoCompField(parser->subframe,GPS_SUBF4_A0_MS_FIRST_BYTE_IDX,GPS_SUBF4_A0_MS_LAST_BYTE_IDX,GPS_SUBF4_A0_MS_BITS_OF_FIRST_BYTE,GPS_SUBF4_A0_MS_BITS_OF_LAST_BYTE,GPS_SUBF4_A0_MS_FIELD_DIM);
		gnss2Utc->a0 += ExtractField(parser->subframe,GPS_SUBF4_A0_LS_FIRST_BYTE_IDX,GPS_SUBF4_A0_LS_LAST_BYTE_IDX,GPS_SUBF4_A0_LS_BITS_OF_FIRST_BYTE,GPS_SUBF4_A0_LS_BITS_OF_LAST_BYTE);
		gnss2Utc->a0 *= POW_TWO_M30;
		gnss2Utc->a1 = POW_TWO_M50 * ExtractTwoCompField(parser->subframe,GPS_SUBF4_A1_FIRST_BYTE_IDX,GPS_SUBF4_A1_LAST_BYTE_IDX,GPS_SUBF4_A1_BITS_OF_FIRST_BYTE,GPS_SUBF4_A1_BITS_OF_LAST_BYTE,GPS_SUBF4_A1_FIELD_DIM);
		gnss2Utc->dtLs = ExtractField(parser->subframe,GPS_SUBF4_DTLS_FIRST_BYTE_IDX,GPS_SUBF4_DTLS_LAST_BYTE_IDX,GPS_SUBF4_DTLS_BITS_OF_FIRST_BYTE,GPS_SUBF4_DTLS_BITS_OF_LAST_BYTE);
		gnss2Utc->t0T = POW_TWO_P12 * ExtractField(parser->subframe,GPS_SUBF4_T0T_FIRST_BYTE_IDX,GPS_SUBF4_T0T_LAST_BYTE_IDX,GPS_SUBF4_T0T_BITS_OF_FIRST_BYTE,GPS_SUBF4_T0T_BITS_OF_LAST_BYTE);
		gnss2Utc->wn0T = ExtractField(parser->subframe,GPS_SUBF4_WN0T_FIRST_BYTE_IDX,GPS_SUBF4_WN0T_LAST_BYTE_IDX,GPS_SUBF4_WN0T_BITS_OF_FIRST_BYTE,GPS_SUBF4_WN0T_BITS_OF_LAST_BYTE);
		gnss2Utc->wnLsf = ExtractField(parser->subframe,GPS_SUBF4_WNLSF_FIRST_BYTE_IDX,GPS_SUBF4_WNLSF_LAST_BYTE_IDX,GPS_SUBF4_WNLSF_BITS_OF_FIRST_BYTE,GPS_SUBF4_WNLSF_BITS_OF_LAST_BYTE);
		gnss2Utc->dn = ExtractField(parser->subframe,GPS_SUBF4_DN_FIRST_BYTE_IDX,GPS_SUBF4_DN_LAST_BYTE_IDX,GPS_SUBF4_DN_BITS_OF_FIRST_BYTE,GPS_SUBF4_DN_BITS_OF_LAST_BYTE);
		gnss2Utc->dtLsf = ExtractTwoCompField(parser->subframe,GPS_SUBF4_DTLSF_FIRST_BYTE_IDX,GPS_SUBF4_DTLSF_LAST_BYTE_IDX,GPS_SUBF4_DTLSF_BITS_OF_FIRST_BYTE,GPS_SUBF4_DTLSF_BITS_OF_LAST_BYTE,GPS_SUBF4_DTLSF_FIELD_DIM);

		if(gnss2Utc->t0T > GPS_MAXIMUM_T0T)
			return FAILURE_RETURN;
		if(gnss2Utc->dn > GPS_MAXIMUM_DN)
			return FAILURE_RETURN;

		navMsg->ionoReceived = TRUE_FLAG;
		navMsg->gnss2UtcReceived = TRUE_FLAG;

		// if possible, adjust WN taking into account truncation (8 bits!)
		if(navMsg->wnValid==TRUE_FLAG)
		{
			gnss2Utc->wn0T = (navMsg->wn & 0xffffff00) | (gnss2Utc->wn0T & 0x000000ff);
			gnss2Utc->wnLsf = (navMsg->wn & 0xffffff00) | (gnss2Utc->wnLsf & 0x000000ff);
			navMsg->gnss2UtcCompleted = TRUE_FLAG;
		}

		rx->gpsIono.alphaGps[0] = iono->alphaGps[0];
		rx->gpsIono.alphaGps[1] = iono->alphaGps[1];
		rx->gpsIono.alphaGps[2] = iono->alphaGps[2];
		rx->gpsIono.alphaGps[3] = iono->alphaGps[3];
		rx->gpsIono.beta[0] = iono->beta[0];
		rx->gpsIono.beta[1] = iono->beta[1];
		rx->gpsIono.beta[2] = iono->beta[2];
		rx->gpsIono.beta[3] = iono->beta[3];
		rx->gpsIonoValid = TRUE_FLAG;

		if(parser->gpsParseLogFile!=NULL)
		{
			fprintf(parser->gpsParseLogFile,"Subframe 4 (pageSvId %d, dataId %d)\n",pageId,dataId);
			fprintf(parser->gpsParseLogFile,"    TOW %u\n", navMsg->tow);
			fprintf(parser->gpsParseLogFile,"    alfa0 %.20g\n", iono->alphaGps[0]);
			fprintf(parser->gpsParseLogFile,"    alfa1 %.20g\n", iono->alphaGps[1]);
			fprintf(parser->gpsParseLogFile,"    alfa2 %.20g\n", iono->alphaGps[2]);
			fprintf(parser->gpsParseLogFile,"    alfa3 %.20g\n", iono->alphaGps[3]);
			fprintf(parser->gpsParseLogFile,"    beta0 %.20g\n", iono->beta[0]);
			fprintf(parser->gpsParseLogFile,"    beta1 %.20g\n", iono->beta[1]);
			fprintf(parser->gpsParseLogFile,"    beta2 %.20g\n", iono->beta[2]);
			fprintf(parser->gpsParseLogFile,"    beta3 %.20g\n", iono->beta[3]);
			fprintf(parser->gpsParseLogFile,"    utcA0 %.20g\n", gnss2Utc->a0);
			fprintf(parser->gpsParseLogFile,"    utcA1 %.20g\n", gnss2Utc->a1);
			fprintf(parser->gpsParseLogFile,"    utcDeltaTls %d\n", gnss2Utc->dtLs);
			fprintf(parser->gpsParseLogFile,"    utcTot %d\n", gnss2Utc->t0T);
			if(navMsg->gnss2UtcCompleted==TRUE_FLAG)
			{
				fprintf(parser->gpsParseLogFile,"    utcWNt %d\n", gnss2Utc->wn0T);
				fprintf(parser->gpsParseLogFile,"    utcWNlsf %d\n", gnss2Utc->wnLsf);
			}
			else
			{
				fprintf(parser->gpsParseLogFile,"    utcWNt %d (truncated)\n", gnss2Utc->wn0T);
				fprintf(parser->gpsParseLogFile,"    utcWNlsf %d (truncated)\n", gnss2Utc->wnLsf);
			}
			fprintf(parser->gpsParseLogFile,"    utcDN %d\n", gnss2Utc->dn);
			fprintf(parser->gpsParseLogFile,"    utcDeltaTlsf %d\n", gnss2Utc->dtLsf);
			fprintf(parser->gpsParseLogFile,"\n");
		}
	}
	else if(pageId==63)
	{
		alm=&(rx->gpsNavigationMessage[25-BASE_PRN].almanac);
		health = ExtractField(parser->subframe,GPS_SUBF4_HEALTH25_FIRST_BYTE_IDX,GPS_SUBF4_HEALTH25_LAST_BYTE_IDX,GPS_SUBF4_HEALTH25_BITS_OF_FIRST_BYTE,GPS_SUBF4_HEALTH25_BITS_OF_LAST_BYTE);
		if((health & 0x020)==0)
			alm->health = SIGNAL_OK;
		else
			alm->health = SIGNAL_OUT_OF_SERVICE;
		alm=&(rx->gpsNavigationMessage[26-BASE_PRN].almanac);
		health = ExtractField(parser->subframe,GPS_SUBF4_HEALTH26_FIRST_BYTE_IDX,GPS_SUBF4_HEALTH26_LAST_BYTE_IDX,GPS_SUBF4_HEALTH26_BITS_OF_FIRST_BYTE,GPS_SUBF4_HEALTH26_BITS_OF_LAST_BYTE);
		if((health & 0x020)==0)
			alm->health = SIGNAL_OK;
		else
			alm->health = SIGNAL_OUT_OF_SERVICE;
		alm=&(rx->gpsNavigationMessage[27-BASE_PRN].almanac);
		health = ExtractField(parser->subframe,GPS_SUBF4_HEALTH27_FIRST_BYTE_IDX,GPS_SUBF4_HEALTH27_LAST_BYTE_IDX,GPS_SUBF4_HEALTH27_BITS_OF_FIRST_BYTE,GPS_SUBF4_HEALTH27_BITS_OF_LAST_BYTE);
		if((health & 0x020)==0)
			alm->health = SIGNAL_OK;
		else
			alm->health = SIGNAL_OUT_OF_SERVICE;
		alm=&(rx->gpsNavigationMessage[28-BASE_PRN].almanac);
		health = ExtractField(parser->subframe,GPS_SUBF4_HEALTH28_FIRST_BYTE_IDX,GPS_SUBF4_HEALTH28_LAST_BYTE_IDX,GPS_SUBF4_HEALTH28_BITS_OF_FIRST_BYTE,GPS_SUBF4_HEALTH28_BITS_OF_LAST_BYTE);
		if((health & 0x020)==0)
			alm->health = SIGNAL_OK;
		else
			alm->health = SIGNAL_OUT_OF_SERVICE;
		alm=&(rx->gpsNavigationMessage[29-BASE_PRN].almanac);
		health = ExtractField(parser->subframe,GPS_SUBF4_HEALTH29_FIRST_BYTE_IDX,GPS_SUBF4_HEALTH29_LAST_BYTE_IDX,GPS_SUBF4_HEALTH29_BITS_OF_FIRST_BYTE,GPS_SUBF4_HEALTH29_BITS_OF_LAST_BYTE);
		if((health & 0x020)==0)
			alm->health = SIGNAL_OK;
		else
			alm->health = SIGNAL_OUT_OF_SERVICE;
		alm=&(rx->gpsNavigationMessage[30-BASE_PRN].almanac);
		health = ExtractField(parser->subframe,GPS_SUBF4_HEALTH30_FIRST_BYTE_IDX,GPS_SUBF4_HEALTH30_LAST_BYTE_IDX,GPS_SUBF4_HEALTH30_BITS_OF_FIRST_BYTE,GPS_SUBF4_HEALTH30_BITS_OF_LAST_BYTE);
		if((health & 0x020)==0)
			alm->health = SIGNAL_OK;
		else
			alm->health = SIGNAL_OUT_OF_SERVICE;
		alm=&(rx->gpsNavigationMessage[31-BASE_PRN].almanac);
		health = ExtractField(parser->subframe,GPS_SUBF4_HEALTH31_FIRST_BYTE_IDX,GPS_SUBF4_HEALTH31_LAST_BYTE_IDX,GPS_SUBF4_HEALTH31_BITS_OF_FIRST_BYTE,GPS_SUBF4_HEALTH31_BITS_OF_LAST_BYTE);
		if((health & 0x020)==0)
			alm->health = SIGNAL_OK;
		else
			alm->health = SIGNAL_OUT_OF_SERVICE;
		alm=&(rx->gpsNavigationMessage[32-BASE_PRN].almanac);
		health = ExtractField(parser->subframe,GPS_SUBF4_HEALTH32_FIRST_BYTE_IDX,GPS_SUBF4_HEALTH32_LAST_BYTE_IDX,GPS_SUBF4_HEALTH32_BITS_OF_FIRST_BYTE,GPS_SUBF4_HEALTH32_BITS_OF_LAST_BYTE);
		if((health & 0x020)==0)
			alm->health = SIGNAL_OK;
		else
			alm->health = SIGNAL_OUT_OF_SERVICE;

		if(parser->gpsParseLogFile!=NULL)
		{
			fprintf(parser->gpsParseLogFile,"Subframe 4 (pageSvId %d, dataId %d)\n",pageId,dataId);
			fprintf(parser->gpsParseLogFile,"    TOW %u\n", navMsg->tow);
			for(index=25;index<=32;index++)
			{
				fprintf(parser->gpsParseLogFile,"    Health GPS%02d %d\n",index,rx->gpsNavigationMessage[index-BASE_PRN].almanac.health);
			}
			fprintf(parser->gpsParseLogFile,"\n");
		}
	}
	else
	{
		if(parser->gpsParseLogFile!=NULL)
		{
			fprintf(parser->gpsParseLogFile,"Subframe 4 (pageSvId %d, dataId %d)\n",pageId,dataId);
			fprintf(parser->gpsParseLogFile,"    TOW %u\n", navMsg->tow);
			fprintf(parser->gpsParseLogFile,"\n");
		}
	}

    return SUCCESS_RETURN;
}


int ParseGpsSubframe5(TNGeneData *rx,TGpsParser *parser)
{
	TNavigationMessage *navMsg = parser->navMsg;
	TGnssAlmanac *alm;
	int pageId;
	int health;
	int dataId;
	int index;

	dataId = ExtractField(parser->subframe,GPS_SUBF45_DATAID_FIRST_BYTE_IDX,GPS_SUBF45_DATAID_LAST_BYTE_IDX,GPS_SUBF45_DATAID_BITS_OF_FIRST_BYTE,GPS_SUBF45_DATAID_BITS_OF_LAST_BYTE);
	pageId = ExtractField(parser->subframe,GPS_SUBF45_PAGEID_FIRST_BYTE_IDX,GPS_SUBF45_PAGEID_LAST_BYTE_IDX,GPS_SUBF45_PAGEID_BITS_OF_FIRST_BYTE,GPS_SUBF45_PAGEID_BITS_OF_LAST_BYTE);

	if((pageId>=1) && (pageId<=24))
	{
		alm=&(rx->gpsNavigationMessage[pageId-BASE_PRN].almanac);
		alm->eccentricity = POW_TWO_M21 * ExtractField(parser->subframe,GPS_SUBF45_E_FIRST_BYTE_IDX,GPS_SUBF45_E_LAST_BYTE_IDX,GPS_SUBF45_E_BITS_OF_FIRST_BYTE,GPS_SUBF45_E_BITS_OF_LAST_BYTE);
		alm->toa = POW_TWO_P12 * ExtractField(parser->subframe,GPS_SUBF45_TOA_FIRST_BYTE_IDX,GPS_SUBF45_TOA_LAST_BYTE_IDX,GPS_SUBF45_TOA_BITS_OF_FIRST_BYTE,GPS_SUBF45_TOA_BITS_OF_LAST_BYTE);
		alm->i0 = POW_TWO_M19 * M_PI * ExtractTwoCompField(parser->subframe,GPS_SUBF45_I0_FIRST_BYTE_IDX,GPS_SUBF45_I0_LAST_BYTE_IDX,GPS_SUBF45_I0_BITS_OF_FIRST_BYTE,GPS_SUBF45_I0_BITS_OF_LAST_BYTE,GPS_SUBF45_I0_FIELD_DIM);
		alm->i0 += (GPS_NOMINAL_I0*M_PI);
		alm->capitalOmegaDot = POW_TWO_M38 * M_PI * ExtractTwoCompField(parser->subframe,GPS_SUBF45_OMEGADOT_FIRST_BYTE_IDX,GPS_SUBF45_OMEGADOT_LAST_BYTE_IDX,GPS_SUBF45_OMEGADOT_BITS_OF_FIRST_BYTE,GPS_SUBF45_OMEGADOT_BITS_OF_LAST_BYTE,GPS_SUBF45_OMEGADOT_FIELD_DIM);
		health = ExtractField(parser->subframe,GPS_SUBF45_HEALTH_FIRST_BYTE_IDX,GPS_SUBF45_HEALTH_LAST_BYTE_IDX,GPS_SUBF45_HEALTH_BITS_OF_FIRST_BYTE,GPS_SUBF45_HEALTH_BITS_OF_LAST_BYTE);
		if((health & 0x0e0)==0)
		{
			alm->health = SIGNAL_OK;
		}
		else
		{
			alm->health = SIGNAL_OUT_OF_SERVICE;
		}
		alm->sqrtA = POW_TWO_M11 * ExtractField(parser->subframe,GPS_SUBF45_SQRTA_FIRST_BYTE_IDX,GPS_SUBF45_SQRTA_LAST_BYTE_IDX,GPS_SUBF45_SQRTA_BITS_OF_FIRST_BYTE,GPS_SUBF45_SQRTA_BITS_OF_LAST_BYTE);
		alm->capitalOmega0 = POW_TWO_M23 * M_PI * ExtractTwoCompField(parser->subframe,GPS_SUBF45_OMEGA0_FIRST_BYTE_IDX,GPS_SUBF45_OMEGA0_LAST_BYTE_IDX,GPS_SUBF45_OMEGA0_BITS_OF_FIRST_BYTE,GPS_SUBF45_OMEGA0_BITS_OF_LAST_BYTE,GPS_SUBF45_OMEGA0_FIELD_DIM);
		alm->omega = POW_TWO_M23 * M_PI * ExtractTwoCompField(parser->subframe,GPS_SUBF45_OMEGA_FIRST_BYTE_IDX,GPS_SUBF45_OMEGA_LAST_BYTE_IDX,GPS_SUBF45_OMEGA_BITS_OF_FIRST_BYTE,GPS_SUBF45_OMEGA_BITS_OF_LAST_BYTE,GPS_SUBF45_OMEGA_FIELD_DIM);
		alm->meanAnomaly0 = POW_TWO_M23 * M_PI * ExtractTwoCompField(parser->subframe,GPS_SUBF45_M0_FIRST_BYTE_IDX,GPS_SUBF45_M0_LAST_BYTE_IDX,GPS_SUBF45_M0_BITS_OF_FIRST_BYTE,GPS_SUBF45_M0_BITS_OF_LAST_BYTE,GPS_SUBF45_M0_FIELD_DIM);
		alm->af[0] = POW_TWO_P03 * ExtractTwoCompField(parser->subframe,GPS_SUBF45_AF0_MS_FIRST_BYTE_IDX,GPS_SUBF45_AF0_MS_LAST_BYTE_IDX,GPS_SUBF45_AF0_MS_BITS_OF_FIRST_BYTE,GPS_SUBF45_AF0_MS_BITS_OF_LAST_BYTE,GPS_SUBF45_AF0_MS_FIELD_DIM);
		alm->af[0] += ExtractField(parser->subframe,GPS_SUBF45_AF0_LS_FIRST_BYTE_IDX,GPS_SUBF45_AF0_LS_LAST_BYTE_IDX,GPS_SUBF45_AF0_LS_BITS_OF_FIRST_BYTE,GPS_SUBF45_AF0_LS_BITS_OF_LAST_BYTE);
		alm->af[0] *= POW_TWO_M20;
		alm->af[1] = POW_TWO_M38 * ExtractTwoCompField(parser->subframe,GPS_SUBF45_AF1_FIRST_BYTE_IDX,GPS_SUBF45_AF1_LAST_BYTE_IDX,GPS_SUBF45_AF1_BITS_OF_FIRST_BYTE,GPS_SUBF45_AF1_BITS_OF_LAST_BYTE,GPS_SUBF45_AF1_FIELD_DIM);

		rx->gpsNavigationMessage[pageId-BASE_PRN].almanac1Received = TRUE_FLAG;
		rx->gpsNavigationMessage[pageId-BASE_PRN].almanac2Received = TRUE_FLAG;

		if(parser->gpsParseLogFile!=NULL)
		{
			fprintf(parser->gpsParseLogFile,"Subframe 5 (pageSvId %d, dataId %d)\n",pageId,dataId);
			fprintf(parser->gpsParseLogFile,"    TOW %u\n", navMsg->tow);
			fprintf(parser->gpsParseLogFile,"    almE %.20g\n", alm->eccentricity);
			fprintf(parser->gpsParseLogFile,"    almToa %.20g\n", alm->toa);
			fprintf(parser->gpsParseLogFile,"    almI0 %.20g\n", alm->i0);
			fprintf(parser->gpsParseLogFile,"    almOmegaDot %.20g\n", alm->capitalOmegaDot);
			fprintf(parser->gpsParseLogFile,"    almHealth %d\n", alm->health);
			fprintf(parser->gpsParseLogFile,"    almAPowerHalf %.20g\n", alm->sqrtA);
			fprintf(parser->gpsParseLogFile,"    almOmega0 %.20g\n", alm->capitalOmega0);
			fprintf(parser->gpsParseLogFile,"    almOmega %.20g\n", alm->omega);
			fprintf(parser->gpsParseLogFile,"    almM0 %.20g\n", alm->meanAnomaly0);
			fprintf(parser->gpsParseLogFile,"    almAF0 %.20g\n", alm->af[0]);
			fprintf(parser->gpsParseLogFile,"    almAF1 %.20g\n", alm->af[1]);
			fprintf(parser->gpsParseLogFile,"\n");
		}
	}
	else if(pageId==51)
	{
		/* ATTENZIONE! Non bisogna salvare toa e wna da questo subframe.
		 * Se diverse versioni dell'almanacco vengono trasmesse in parallelo (trovato su un dataset 11 dic 2020)
		 * puo' succedere che sovrascrivere toa porti l'almanacco a non essere coerente (toa da una versione, dati rimanenti dall'altra versione) */
		//alm=&(navMsg->almanac);
		//alm->toa = POW_TWO_P12 * ExtractField(parser->subframe,GPS_SUBF5_TOA_FIRST_BYTE_IDX,GPS_SUBF5_TOA_LAST_BYTE_IDX,GPS_SUBF5_TOA_BITS_OF_FIRST_BYTE,GPS_SUBF5_TOA_BITS_OF_LAST_BYTE);
		//alm->wna = ExtractField(parser->subframe,GPS_SUBF5_WN_FIRST_BYTE_IDX,GPS_SUBF5_WN_LAST_BYTE_IDX,GPS_SUBF5_WN_BITS_OF_FIRST_BYTE,GPS_SUBF5_WN_BITS_OF_LAST_BYTE);
		alm=&(rx->gpsNavigationMessage[1-BASE_PRN].almanac);
		health = ExtractField(parser->subframe,GPS_SUBF5_HEALTH01_FIRST_BYTE_IDX,GPS_SUBF5_HEALTH01_LAST_BYTE_IDX,GPS_SUBF5_HEALTH01_BITS_OF_FIRST_BYTE,GPS_SUBF5_HEALTH01_BITS_OF_LAST_BYTE);
		if((health & 0x020)==0)
			alm->health = SIGNAL_OK;
		else
			alm->health = SIGNAL_OUT_OF_SERVICE;
		alm=&(rx->gpsNavigationMessage[2-BASE_PRN].almanac);
		health = ExtractField(parser->subframe,GPS_SUBF5_HEALTH02_FIRST_BYTE_IDX,GPS_SUBF5_HEALTH02_LAST_BYTE_IDX,GPS_SUBF5_HEALTH02_BITS_OF_FIRST_BYTE,GPS_SUBF5_HEALTH02_BITS_OF_LAST_BYTE);
		if((health & 0x020)==0)
			alm->health = SIGNAL_OK;
		else
			alm->health = SIGNAL_OUT_OF_SERVICE;
		alm=&(rx->gpsNavigationMessage[3-BASE_PRN].almanac);
		health = ExtractField(parser->subframe,GPS_SUBF5_HEALTH03_FIRST_BYTE_IDX,GPS_SUBF5_HEALTH03_LAST_BYTE_IDX,GPS_SUBF5_HEALTH03_BITS_OF_FIRST_BYTE,GPS_SUBF5_HEALTH03_BITS_OF_LAST_BYTE);
		if((health & 0x020)==0)
			alm->health = SIGNAL_OK;
		else
			alm->health = SIGNAL_OUT_OF_SERVICE;
		alm=&(rx->gpsNavigationMessage[4-BASE_PRN].almanac);
		health = ExtractField(parser->subframe,GPS_SUBF5_HEALTH04_FIRST_BYTE_IDX,GPS_SUBF5_HEALTH04_LAST_BYTE_IDX,GPS_SUBF5_HEALTH04_BITS_OF_FIRST_BYTE,GPS_SUBF5_HEALTH04_BITS_OF_LAST_BYTE);
		if((health & 0x020)==0)
			alm->health = SIGNAL_OK;
		else
			alm->health = SIGNAL_OUT_OF_SERVICE;
		alm=&(rx->gpsNavigationMessage[5-BASE_PRN].almanac);
		health = ExtractField(parser->subframe,GPS_SUBF5_HEALTH05_FIRST_BYTE_IDX,GPS_SUBF5_HEALTH05_LAST_BYTE_IDX,GPS_SUBF5_HEALTH05_BITS_OF_FIRST_BYTE,GPS_SUBF5_HEALTH05_BITS_OF_LAST_BYTE);
		if((health & 0x020)==0)
			alm->health = SIGNAL_OK;
		else
			alm->health = SIGNAL_OUT_OF_SERVICE;
		alm=&(rx->gpsNavigationMessage[6-BASE_PRN].almanac);
		health = ExtractField(parser->subframe,GPS_SUBF5_HEALTH06_FIRST_BYTE_IDX,GPS_SUBF5_HEALTH06_LAST_BYTE_IDX,GPS_SUBF5_HEALTH06_BITS_OF_FIRST_BYTE,GPS_SUBF5_HEALTH06_BITS_OF_LAST_BYTE);
		if((health & 0x020)==0)
			alm->health = SIGNAL_OK;
		else
			alm->health = SIGNAL_OUT_OF_SERVICE;
		alm=&(rx->gpsNavigationMessage[7-BASE_PRN].almanac);
		health = ExtractField(parser->subframe,GPS_SUBF5_HEALTH07_FIRST_BYTE_IDX,GPS_SUBF5_HEALTH07_LAST_BYTE_IDX,GPS_SUBF5_HEALTH07_BITS_OF_FIRST_BYTE,GPS_SUBF5_HEALTH07_BITS_OF_LAST_BYTE);
		if((health & 0x020)==0)
			alm->health = SIGNAL_OK;
		else
			alm->health = SIGNAL_OUT_OF_SERVICE;
		alm=&(rx->gpsNavigationMessage[8-BASE_PRN].almanac);
		health = ExtractField(parser->subframe,GPS_SUBF5_HEALTH08_FIRST_BYTE_IDX,GPS_SUBF5_HEALTH08_LAST_BYTE_IDX,GPS_SUBF5_HEALTH08_BITS_OF_FIRST_BYTE,GPS_SUBF5_HEALTH08_BITS_OF_LAST_BYTE);
		if((health & 0x020)==0)
			alm->health = SIGNAL_OK;
		else
			alm->health = SIGNAL_OUT_OF_SERVICE;
		alm=&(rx->gpsNavigationMessage[9-BASE_PRN].almanac);
		health = ExtractField(parser->subframe,GPS_SUBF5_HEALTH09_FIRST_BYTE_IDX,GPS_SUBF5_HEALTH09_LAST_BYTE_IDX,GPS_SUBF5_HEALTH09_BITS_OF_FIRST_BYTE,GPS_SUBF5_HEALTH09_BITS_OF_LAST_BYTE);
		if((health & 0x020)==0)
			alm->health = SIGNAL_OK;
		else
			alm->health = SIGNAL_OUT_OF_SERVICE;
		alm=&(rx->gpsNavigationMessage[10-BASE_PRN].almanac);
		health = ExtractField(parser->subframe,GPS_SUBF5_HEALTH10_FIRST_BYTE_IDX,GPS_SUBF5_HEALTH10_LAST_BYTE_IDX,GPS_SUBF5_HEALTH10_BITS_OF_FIRST_BYTE,GPS_SUBF5_HEALTH10_BITS_OF_LAST_BYTE);
		if((health & 0x020)==0)
			alm->health = SIGNAL_OK;
		else
			alm->health = SIGNAL_OUT_OF_SERVICE;
		alm=&(rx->gpsNavigationMessage[11-BASE_PRN].almanac);
		health = ExtractField(parser->subframe,GPS_SUBF5_HEALTH11_FIRST_BYTE_IDX,GPS_SUBF5_HEALTH11_LAST_BYTE_IDX,GPS_SUBF5_HEALTH11_BITS_OF_FIRST_BYTE,GPS_SUBF5_HEALTH11_BITS_OF_LAST_BYTE);
		if((health & 0x020)==0)
			alm->health = SIGNAL_OK;
		else
			alm->health = SIGNAL_OUT_OF_SERVICE;
		alm=&(rx->gpsNavigationMessage[12-BASE_PRN].almanac);
		health = ExtractField(parser->subframe,GPS_SUBF5_HEALTH12_FIRST_BYTE_IDX,GPS_SUBF5_HEALTH12_LAST_BYTE_IDX,GPS_SUBF5_HEALTH12_BITS_OF_FIRST_BYTE,GPS_SUBF5_HEALTH12_BITS_OF_LAST_BYTE);
		if((health & 0x020)==0)
			alm->health = SIGNAL_OK;
		else
			alm->health = SIGNAL_OUT_OF_SERVICE;
		alm=&(rx->gpsNavigationMessage[13-BASE_PRN].almanac);
		health = ExtractField(parser->subframe,GPS_SUBF5_HEALTH13_FIRST_BYTE_IDX,GPS_SUBF5_HEALTH13_LAST_BYTE_IDX,GPS_SUBF5_HEALTH13_BITS_OF_FIRST_BYTE,GPS_SUBF5_HEALTH13_BITS_OF_LAST_BYTE);
		if((health & 0x020)==0)
			alm->health = SIGNAL_OK;
		else
			alm->health = SIGNAL_OUT_OF_SERVICE;
		alm=&(rx->gpsNavigationMessage[14-BASE_PRN].almanac);
		health = ExtractField(parser->subframe,GPS_SUBF5_HEALTH14_FIRST_BYTE_IDX,GPS_SUBF5_HEALTH14_LAST_BYTE_IDX,GPS_SUBF5_HEALTH14_BITS_OF_FIRST_BYTE,GPS_SUBF5_HEALTH14_BITS_OF_LAST_BYTE);
		if((health & 0x020)==0)
			alm->health = SIGNAL_OK;
		else
			alm->health = SIGNAL_OUT_OF_SERVICE;
		alm=&(rx->gpsNavigationMessage[15-BASE_PRN].almanac);
		health = ExtractField(parser->subframe,GPS_SUBF5_HEALTH15_FIRST_BYTE_IDX,GPS_SUBF5_HEALTH15_LAST_BYTE_IDX,GPS_SUBF5_HEALTH15_BITS_OF_FIRST_BYTE,GPS_SUBF5_HEALTH15_BITS_OF_LAST_BYTE);
		if((health & 0x020)==0)
			alm->health = SIGNAL_OK;
		else
			alm->health = SIGNAL_OUT_OF_SERVICE;
		alm=&(rx->gpsNavigationMessage[16-BASE_PRN].almanac);
		health = ExtractField(parser->subframe,GPS_SUBF5_HEALTH16_FIRST_BYTE_IDX,GPS_SUBF5_HEALTH16_LAST_BYTE_IDX,GPS_SUBF5_HEALTH16_BITS_OF_FIRST_BYTE,GPS_SUBF5_HEALTH16_BITS_OF_LAST_BYTE);
		if((health & 0x020)==0)
			alm->health = SIGNAL_OK;
		else
			alm->health = SIGNAL_OUT_OF_SERVICE;
		alm=&(rx->gpsNavigationMessage[17-BASE_PRN].almanac);
		health = ExtractField(parser->subframe,GPS_SUBF5_HEALTH17_FIRST_BYTE_IDX,GPS_SUBF5_HEALTH17_LAST_BYTE_IDX,GPS_SUBF5_HEALTH17_BITS_OF_FIRST_BYTE,GPS_SUBF5_HEALTH17_BITS_OF_LAST_BYTE);
		if((health & 0x020)==0)
			alm->health = SIGNAL_OK;
		else
			alm->health = SIGNAL_OUT_OF_SERVICE;
		alm=&(rx->gpsNavigationMessage[18-BASE_PRN].almanac);
		health = ExtractField(parser->subframe,GPS_SUBF5_HEALTH18_FIRST_BYTE_IDX,GPS_SUBF5_HEALTH18_LAST_BYTE_IDX,GPS_SUBF5_HEALTH18_BITS_OF_FIRST_BYTE,GPS_SUBF5_HEALTH18_BITS_OF_LAST_BYTE);
		if((health & 0x020)==0)
			alm->health = SIGNAL_OK;
		else
			alm->health = SIGNAL_OUT_OF_SERVICE;
		alm=&(rx->gpsNavigationMessage[19-BASE_PRN].almanac);
		health = ExtractField(parser->subframe,GPS_SUBF5_HEALTH19_FIRST_BYTE_IDX,GPS_SUBF5_HEALTH19_LAST_BYTE_IDX,GPS_SUBF5_HEALTH19_BITS_OF_FIRST_BYTE,GPS_SUBF5_HEALTH19_BITS_OF_LAST_BYTE);
		if((health & 0x020)==0)
			alm->health = SIGNAL_OK;
		else
			alm->health = SIGNAL_OUT_OF_SERVICE;
		alm=&(rx->gpsNavigationMessage[20-BASE_PRN].almanac);
		health = ExtractField(parser->subframe,GPS_SUBF5_HEALTH20_FIRST_BYTE_IDX,GPS_SUBF5_HEALTH20_LAST_BYTE_IDX,GPS_SUBF5_HEALTH20_BITS_OF_FIRST_BYTE,GPS_SUBF5_HEALTH20_BITS_OF_LAST_BYTE);
		if((health & 0x020)==0)
			alm->health = SIGNAL_OK;
		else
			alm->health = SIGNAL_OUT_OF_SERVICE;
		alm=&(rx->gpsNavigationMessage[21-BASE_PRN].almanac);
		health = ExtractField(parser->subframe,GPS_SUBF5_HEALTH21_FIRST_BYTE_IDX,GPS_SUBF5_HEALTH21_LAST_BYTE_IDX,GPS_SUBF5_HEALTH21_BITS_OF_FIRST_BYTE,GPS_SUBF5_HEALTH21_BITS_OF_LAST_BYTE);
		if((health & 0x020)==0)
			alm->health = SIGNAL_OK;
		else
			alm->health = SIGNAL_OUT_OF_SERVICE;
		alm=&(rx->gpsNavigationMessage[22-BASE_PRN].almanac);
		health = ExtractField(parser->subframe,GPS_SUBF5_HEALTH22_FIRST_BYTE_IDX,GPS_SUBF5_HEALTH22_LAST_BYTE_IDX,GPS_SUBF5_HEALTH22_BITS_OF_FIRST_BYTE,GPS_SUBF5_HEALTH22_BITS_OF_LAST_BYTE);
		if((health & 0x020)==0)
			alm->health = SIGNAL_OK;
		else
			alm->health = SIGNAL_OUT_OF_SERVICE;
		alm=&(rx->gpsNavigationMessage[23-BASE_PRN].almanac);
		health = ExtractField(parser->subframe,GPS_SUBF5_HEALTH23_FIRST_BYTE_IDX,GPS_SUBF5_HEALTH23_LAST_BYTE_IDX,GPS_SUBF5_HEALTH23_BITS_OF_FIRST_BYTE,GPS_SUBF5_HEALTH23_BITS_OF_LAST_BYTE);
		if((health & 0x020)==0)
			alm->health = SIGNAL_OK;
		else
			alm->health = SIGNAL_OUT_OF_SERVICE;
		alm=&(rx->gpsNavigationMessage[24-BASE_PRN].almanac);
		health = ExtractField(parser->subframe,GPS_SUBF5_HEALTH24_FIRST_BYTE_IDX,GPS_SUBF5_HEALTH24_LAST_BYTE_IDX,GPS_SUBF5_HEALTH24_BITS_OF_FIRST_BYTE,GPS_SUBF5_HEALTH24_BITS_OF_LAST_BYTE);
		if((health & 0x020)==0)
			alm->health = SIGNAL_OK;
		else
			alm->health = SIGNAL_OUT_OF_SERVICE;

		if(parser->gpsParseLogFile!=NULL)
		{
			fprintf(parser->gpsParseLogFile,"Subframe 5 (pageSvId %d, dataId %d)\n",pageId,dataId);
			fprintf(parser->gpsParseLogFile,"    TOW %u\n", navMsg->tow);
			fprintf(parser->gpsParseLogFile,"    TOA %.20g\n", navMsg->almanac.toa);
			fprintf(parser->gpsParseLogFile,"    Almanac reference Week Number %d\n", navMsg->almanac.wna);
			for(index=1;index<=24;index++)
			{
				fprintf(parser->gpsParseLogFile,"    Health GPS%02d %d\n",index,rx->gpsNavigationMessage[index-BASE_PRN].almanac.health);
			}
			fprintf(parser->gpsParseLogFile,"\n");
		}
	}
	else
	{
		if(parser->gpsParseLogFile!=NULL)
		{
			fprintf(parser->gpsParseLogFile,"Subframe 5 (pageSvId %d, dataId %d)\n",pageId,dataId);
			fprintf(parser->gpsParseLogFile,"    TOW %u\n", navMsg->tow);
			fprintf(parser->gpsParseLogFile,"\n");
		}
	}
    return SUCCESS_RETURN;
}


int ParseGpsSubframe(TNGeneData *rx,TGpsParser *parser)
{
	unsigned int subFrameIndex;
	TNavigationMessage *navMsg = parser->navMsg;
	int towOld = navMsg->tow;
	int parseReturn;

	subFrameIndex = ExtractField(parser->subframe,GPS_SUBFRAME_FIRST_BYTE_IDX,GPS_SUBFRAME_LAST_BYTE_IDX,GPS_SUBFRAME_BITS_OF_FIRST_BYTE,GPS_SUBFRAME_BITS_OF_LAST_BYTE);
	navMsg->tow = FROM_TOW_INDEX_TO_SECOND_FACTOR*ExtractField(parser->subframe,GPS_TOW_FIRST_BYTE_IDX,GPS_TOW_LAST_BYTE_IDX,GPS_TOW_BITS_OF_FIRST_BYTE,GPS_TOW_BITS_OF_LAST_BYTE);

	switch(subFrameIndex)
	{
		case 1:
			parseReturn = ParseGpsSubframe1(parser);
			break;
		case 2:
			parseReturn = ParseGpsSubframe2(parser);
			break;
		case 3:
			parseReturn = ParseGpsSubframe3(parser);
			break;
		case 4:
			parseReturn = ParseGpsSubframe4(rx,parser);
			break;
		case 5:
			parseReturn = ParseGpsSubframe5(rx,parser);
			break;
		default:
			parseReturn = FAILURE_RETURN;
			break;
	}
	if(parseReturn==FAILURE_RETURN)
	{
		// reset OSNMA saving
		parser->lastSavedSubframe = 0;
		// update TOW
		if(towOld!=INVALID_VALUE)
		{
			navMsg->tow = towOld+GPS_SUBFRAME_DURATION;
		}
		return FAILURE_RETURN;
	}
	rx->gpsLastDecodedSubframe=subFrameIndex;
	rx->gpsLastDecodedPage=(((navMsg->tow-6)/(GPS_SUBFRAME_DURATION*5))%25)+1;

	/* Save for Galileo OSNMA cross-authentication
	 * (only when OSNMA bits are received) */
	if((rx->osnmaData.osnmaTransmitted==TRUE_FLAG) &&
	   (((parser->lastSavedSubframe==5)  && (subFrameIndex==1)) || (parser->lastSavedSubframe==((int)subFrameIndex-1))))
	{
		memcpy(&(parser->frame[(subFrameIndex-1)*GPS_SUBFRAME_BYTES]),parser->subframe,GPS_SUBFRAME_BYTES);
		parser->lastSavedSubframe = subFrameIndex;
	}

	return SUCCESS_RETURN;
}


TParseResult DecodeGpsSubframe(TNGeneConfiguration *cfg,TNGeneData *rx,int chIndex,TGpsParser *parser,unsigned int symbol)
{
	int crcErrorFound = FALSE_FLAG;
	TSatConstellation satType;
	int wn;
	double ttxDouble;

	parser->lastWord = (parser->lastWord<<1) | symbol;
	parser->bitIndex++;
	parser->bitIndexTotal++;
	/* continue to verify synchronization */
	if((parser->wordIndex==0) && (parser->bitIndex==GPS_PREAMBLE_LENGTH) && (parser->crcPassed>=GPS_SUBFRAME_LENGTH))
	{
		/* the test on the CRC is added in order to test the preamble only when bits are reliable */
		if(((parser->lastWord & 0x0ff)!=GPS_PREAMBLE_BITS) && ((parser->lastWord & 0x0ff)!=GPS_INV_PREAMBLE_BITS))
		{
			return DECODING_ERROR;
		}
	}
	/* a word is complete, verify CRC and save */
	if(parser->bitIndex==GPS_WORD_LENGTH)
	{
		parser->bitIndex=0;
		if(GpsParityVerify(&(parser->lastWord))==FAILURE_RETURN)
		{
			crcErrorFound = TRUE_FLAG;
			parser->crcPassed=0;
		}
		else
		{
			parser->crcPassed++;
		}
		/* the word is copied into a byte array (takes into account endianness) */
		parser->subframe[(4*parser->wordIndex)]  =(unsigned char)((parser->lastWord>>24) & 0x0ff);
		parser->subframe[(4*parser->wordIndex)+1]=(unsigned char)((parser->lastWord>>16) & 0x0ff);
		parser->subframe[(4*parser->wordIndex)+2]=(unsigned char)((parser->lastWord>>8)  & 0x0ff);
		parser->subframe[(4*parser->wordIndex)+3]=(unsigned char)((parser->lastWord)     & 0x0ff);

		parser->wordIndex++;
		if(parser->wordIndex==GPS_SUBFRAME_LENGTH)
		{
			parser->wordIndex=0;
			parser->bitIndexTotal=0;
			if(parser->crcPassed>=GPS_SUBFRAME_LENGTH)
			{
				if(ParseGpsSubframe(rx,parser)==FAILURE_RETURN)
				{
					parser->crcPassed=0;
					return DECODING_ERROR;
				}
				// In GPS, TOW is present in all subframes!
				// try to recover WN if missing
				if(parser->navMsg->wnValid==FALSE_FLAG)
				{
					GetATransmissionTime(cfg,rx,chIndex,&satType,&wn,&ttxDouble);
					if((wn>0) && (parser->navMsg->wnValid==FALSE_FLAG))
					{
						parser->navMsg->wn = (satType==GPS_SAT) ? wn : (wn + GPS_WN_TO_GAL_WN_BIAS);
						parser->navMsg->wnValid = TRUE_FLAG;
						if(parser->gpsParseLogFile!=NULL)
						{
							fprintf(parser->gpsParseLogFile,"WN extrapolated from other channels %d\n\n",parser->navMsg->wn);
						}
					}
				}
				return DECODING_1_SUCCESSFUL;
			}
			else
			{
				parser->crcPassed=0;
				parser->navMsg->tow += GPS_SUBFRAME_DURATION;
				return CRC_ERROR;
			}
		}
		else if(crcErrorFound==TRUE_FLAG)
		{
			return CRC_ERROR;
		}
	}
	return DECODING_ON_GOING;
}


void ParseGpsMessage(TNGeneConfiguration *cfg,TNGeneData *rx,TChannel *ch,float corr)
{
	int chIndex = ch->chIndex;
	TGpsParser *parser = ch->gpsParser;
	TNavigationMessage *navMsg = ch->gpsParser->navMsg;
	unsigned int symbol = (corr > 0.0) ? 0x00 : 0x01;
	TParseResult parseResult;
	int iodc;
	int wn;
	int tow;

	switch(parser->status)
	{
		case SEARCH_SYNCHRONIZATION:
			if(GpsSynchronization(parser,symbol)==PREAMBLE_FOUND)
			{
				parser->status=FIRST_DECODING;
			}
			break;
		case FIRST_DECODING:
			parseResult = DecodeGpsSubframe(cfg,rx,chIndex,parser,symbol);
			if(parseResult==CRC_ERROR)
			{
				parser->bitIndex = 0;
				parser->status=SEARCH_SYNCHRONIZATION;
			}
			else if(parseResult==DECODING_ERROR)
			{
				parser->bitIndex = 0;
				parser->status=SEARCH_SYNCHRONIZATION;
			}
			else if(parseResult==DECODING_1_SUCCESSFUL)
			{
				parser->status=DECODING_1;
			}
			break;
		case DECODING_1:
		default:
			parseResult = DecodeGpsSubframe(cfg,rx,chIndex,parser,symbol);
			if(parseResult==DECODING_1_SUCCESSFUL)
			{
				navMsg->towValid=TRUE_FLAG;
				/* manage coherence of the parsed ephemerides
				 * for IODC the two most significant bits must be discarded before the comparison with IODE */
				iodc = navMsg->clkTmp.iodc & 0x0ff;
				if((navMsg->subframe1Received==TRUE_FLAG) && (navMsg->subframe2Received==TRUE_FLAG) && (navMsg->subframe3Received==TRUE_FLAG) &&
						(iodc==navMsg->ephTmp.iode2) && (navMsg->ephTmp.iode2==navMsg->ephTmp.iode3))
				{
					if(navMsg->eph.iode2!=navMsg->ephTmp.iode2)
					{
						/* a new set of data from navigation message can be sent to the GUI */
						ch->newNavMsgInformation = TRUE_FLAG;
					}
					CopyFromTmpToOperative(navMsg);
					navMsg->subframe1Received=FALSE_FLAG;
					navMsg->subframe2Received=FALSE_FLAG;
					navMsg->subframe3Received=FALSE_FLAG;
					navMsg->ephemeridesReceived = TRUE_FLAG;
					/* optional RINEX log */
					if((navMsg->wnValid==TRUE_FLAG) && (ch->newNavMsgInformation==TRUE_FLAG))
					{
						RinexSaveGpsNav(cfg,&(rx->rinexGps),ch,navMsg);
						if(cfg->userInterface!=RX_UI_GUI)
						{
							// if the GUI is not used, reset here the flag to avoid saving twice the same navigation data
							// otherwise, the flag is reset by SendNavigationMessageInformation
							ch->newNavMsgInformation = FALSE_FLAG;
						}
					}
					RinexAddGpsIono(cfg,&(rx->rinexGps),navMsg);
					RinexAddGput(cfg,&(rx->rinexGps),navMsg);
				}
				/* optional IFEN format log */
				if((navMsg->towValid==TRUE_FLAG) && (navMsg->wnValid==TRUE_FLAG))
				{
					SaveGpsL1CaBitsInIfenLog(rx->ifenLogGps,&(rx->ifenLogGpsTime),navMsg->wn,navMsg->tow-GPS_SUBFRAME_DURATION,ch->satId,parser->subframe);
				}
				/* save a frame for Galileo OSNMA cross-authentication
				   save in the MACK list when an entire frame is available,
				   time is valid (both WN and TOW), IODc and IODE are coherent */
				if(parser->lastSavedSubframe==5)
				{
					parser->lastSavedSubframe=0;
					iodc = navMsg->clkTmp.iodc & 0x0ff;
					if((navMsg->towValid==TRUE_FLAG) && (navMsg->wnValid==TRUE_FLAG) &&
					   (iodc==navMsg->ephTmp.iode2) && (navMsg->ephTmp.iode2==navMsg->ephTmp.iode3))
					{
						TMackList *mackPtr;
						TNavDataList *navDataPtr;

						// transmission time for the begin of frame
						// 1024 is subtracted to the week number to align it
						// with the WN of Galileo (used to store MACs and keys)
						wn = navMsg->wn - GPS_WN_TO_GAL_WN_BIAS;
						tow = navMsg->tow - GPS_FRAME_DURATION;
						if(tow<0)
						{
							// previous week: correct WN and TOW
							wn--;
							tow += (60*60*24*7);
						}
						// find/insert in the MACK list
						mackPtr = PushInMackList(rx->osnmaData.mackList, ch->satType, ch->satSignal, ch->satId, ch->chIndex);
						if(rx->osnmaData.mackList==NULL)
						{
							/* list was empty */
							rx->osnmaData.mackList = mackPtr;
						}
						mackPtr->prnA = GetOsnmaPrnFromSatTypePrn(ch->satType,ch->satId);
						mackPtr->prnN = mackPtr->prnA;
						// insert in the navigationData list
						navDataPtr = PushNewNavData(mackPtr);
						navDataPtr->wnSubframeStart = wn;
						navDataPtr->towSubframeStart = tow;
						memcpy(navDataPtr->navData,parser->frame,GPS_FRAME_BYTES);
					}
				}
			}
			else if(parseResult==DECODING_ERROR)
			{
				parser->bitIndex = 0;
				parser->crcPassed = 0;
				parser->status=SEARCH_SYNCHRONIZATION;
				navMsg->towValid=TRUE_FLAG;
				navMsg->subframe1Received=FALSE_FLAG;
				navMsg->subframe2Received=FALSE_FLAG;
				navMsg->subframe3Received=FALSE_FLAG;
				// decoding failed, reset OSNMA saving
				parser->lastSavedSubframe = 0;
			}
			break;
	}

	if(parser->gpsParseLogFile!=NULL)
	{
		fflush(parser->gpsParseLogFile);
	}
}
