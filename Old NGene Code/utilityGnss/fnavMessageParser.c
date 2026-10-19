/**
 * @file fnavMessageParser.c
 *
 * @author Mario Nicola
 *
 * @date 08/02/2021
 *
 * @version 1.0.0
 *
 * @brief Functions used to parse the Galileo F/NAV navigation message.
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "ngeneDataStructure.h"
#include "ngeneConfiguration.h"
#include "channelDataStructure.h"
#include "fnavMessageParser.h"
#include "galileoMessageParser.h"
#include "navigationMessage.h"
#include "navigationMessageParser.h"
#include "ngeneConstants.h"
#include "timeConstants.h"
#include "ioFunctions.h"
#include "closeManager.h"
#include "viterbi.h"
#include "arrays.h"
#include "rinexLog.h"
#include "osnmaManagement.h"
#include "osnmaUtilities.h"
#include "mackList.h"
#include "ifenLog.h"


int InitFNavParser(TNGeneConfiguration *cfg,TChannel *ch)
{
	if((ch->satType!=GALILEO_SAT) || (ch->satSignal!=E5_A))
	{
		ch->fnavParser = NULL;
		return SUCCESS_RETURN;
	}
	ch->fnavParser = (TFNavParser *)malloc(sizeof(TFNavParser));
	if(ch->fnavParser==NULL)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Malloc returned a NULL pointer in InitFNavParser");
		CloseNGene(cfg,EXIT_FAILURE);
	}
	ch->fnavParser->wnCorrection = cfg->navMsgConfiguration.galileoWnCorrection;
	ch->fnavParser->fnavParseLogFile = NULL;
	return SUCCESS_RETURN;
}


void DeinitFNavParser(TChannel *ch)
{
	if(ch->fnavParser!=NULL)
	{
		if(ch->fnavParser->fnavParseLogFile!=NULL)
		{
			fclose(ch->fnavParser->fnavParseLogFile);
		}
		free(ch->fnavParser);
		ch->fnavParser=NULL;
	}
}

void StartFNavParser(TNGeneConfiguration *cfg,TNGeneData *rx,TChannel *ch)
{
	TFNavParser *parser = ch->fnavParser;
	char filename[2*MAX_ROW_LENGTH];

	if((ch->satType!=GALILEO_SAT) || (ch->satSignal!=E5_A))
		return;

	if(ch->satId==INVALID_PRN)
		return;

	parser->satId = ch->satId;

	parser->fnavPreamble[1] = (unsigned char)(FNAV_PREAMBLE_BITS >> 8) & 0x0f;
	parser->fnavPreamble[0] = (unsigned char)(FNAV_PREAMBLE_BITS & 0x0ff);
	parser->fnavPreambleInv[1] = (unsigned char)(FNAV_INV_PREAMBLE_BITS >> 8) & 0x0f;
	parser->fnavPreambleInv[0] = (unsigned char)(FNAV_INV_PREAMBLE_BITS & 0x0ff);
	parser->polarityCorrection = 0x00;
	parser->bitIndexTotal = 0;
	parser->byteIndex = 0;
	parser->bitIndex = 0;
	parser->synchronizationSuccess = FALSE_FLAG;
	parser->dummyMsg = FALSE_FLAG;
	parser->status = SEARCH_SYNCHRONIZATION;
	parser->navMsg = &(rx->fnavNavigationMessage[ch->satId-BASE_PRN]);
	parser->navMsg->towValid=FALSE_FLAG;
	parser->navMsg->wnValid=FALSE_FLAG;
	// osnma
	parser->lastSavedPage = 0; // not valid
	// log
	if(cfg->logConfiguration.enableNavMesgLog==TRUE_FLAG)
	{
		if(parser->fnavParseLogFile!=NULL)
		{
			fclose(parser->fnavParseLogFile);
		}
		sprintf(filename,"%s/navigationMessage_FNAV%02d.txt",cfg->dumpPathCurrent,ch->satId);
		parser->fnavParseLogFile = fopen(filename,"a");
	}
}


TParseResult FNavSynchronization(TFNavParser *parser,unsigned char symbol)
{
	TParseResult res = PREAMBLE_NOT_FOUND;

	parser->page[1] = (((parser->page[1]<<1) & 0x0f) | (parser->page[0] >> 7));
	parser->page[0] = ((parser->page[0]<<1) | symbol);
	if(parser->bitIndex<FNAV_PREAMBLE_LENGTH)
	{
		parser->bitIndex++;
	}
	if(parser->bitIndex==FNAV_PREAMBLE_LENGTH)
	{
		if((parser->page[1]==parser->fnavPreamble[1]) && (parser->page[0]==parser->fnavPreamble[0]))
		{
			//parser->polarityCorrection = parser->polarityCorrection;
			res = PREAMBLE_FOUND;
		}
		if((parser->page[1]==parser->fnavPreambleInv[1]) && (parser->page[0]==parser->fnavPreambleInv[0]))
		{
			parser->polarityCorrection = parser->polarityCorrection ^ 0x01;
			res = PREAMBLE_FOUND;
		}
	}
	if(res==PREAMBLE_FOUND)
	{
		parser->byteIndex=0;
		parser->bitIndex=0;
	}
	return res;
}


void FNavDeinterleaver(TFNavParser *parser)
{
	unsigned short i, bitIdx, byteIdx;
	unsigned short idx;
	unsigned short byteStartVect[8] = { 0, 7, 15, 22, 30, 38, 45, 53 };
	unsigned short bitStartVect[8] = { 0, 5, 2, 7, 4, 1, 6, 3 };

	for(i=0;i<FNAV_ENCODED_PAGE_LENGTH_SYMBOLS;i++)
	{
		idx = i & 0x00000007;
		byteIdx = byteStartVect[idx];
		bitIdx = bitStartVect[idx];
		parser->deinterleavedPage[i] = ( (parser->page[byteIdx] >> (8 - (bitIdx + 1))) & 0x1);
		// Set new indexes for bitStartVect and byteStartVect (Compact version)
		// If bitStartVect[idx]+1 == 8 then increment byteStartVect[idx]
		byteStartVect[idx] = byteStartVect[idx] + ((bitStartVect[idx] + 1) >> 3);
		// set bitStartVect[idx] to (bitStartVect[idx] + 1) mod 7
		bitStartVect[idx] = (bitStartVect[idx] + 1) & 0x00000007;
	}
}




int ParseFNavPage01(TFNavParser *parser)
{
	unsigned char *dataBytes = parser->decodedPage;
	TNavigationMessage *navMsg = parser->navMsg;
	int svid;
	TGnssEphemerides *eph = &(navMsg->ephTmp);

	svid = ExtractField(dataBytes,FNAV_TYPE1_SVID_FIRST_BYTE_IDX,FNAV_TYPE1_SVID_LAST_BYTE_IDX,FNAV_TYPE1_SVID_BITS_OF_FIRST_BYTE,FNAV_TYPE1_SVID_BITS_OF_LAST_BYTE);
	eph->iode1 = ExtractField(dataBytes,FNAV_TYPE1_IOD_FIRST_BYTE_IDX,FNAV_TYPE1_IOD_LAST_BYTE_IDX,FNAV_TYPE1_IOD_BITS_OF_FIRST_BYTE,FNAV_TYPE1_IOD_BITS_OF_LAST_BYTE);
	navMsg->wn = ExtractField(dataBytes,FNAV_TYPE1_WN_FIRST_BYTE_IDX,FNAV_TYPE1_WN_LAST_BYTE_IDX,FNAV_TYPE1_WN_BITS_OF_FIRST_BYTE,FNAV_TYPE1_WN_BITS_OF_LAST_BYTE);
	navMsg->wn += parser->wnCorrection;
	navMsg->wnValid = TRUE_FLAG;
	navMsg->tow = ExtractField(dataBytes,FNAV_TYPE1_TOW_FIRST_BYTE_IDX,FNAV_TYPE1_TOW_LAST_BYTE_IDX,FNAV_TYPE1_TOW_BITS_OF_FIRST_BYTE,FNAV_TYPE1_TOW_BITS_OF_LAST_BYTE);
	parser->bitIndexTotal = FNAV_PAGE_LENGTH;
	navMsg->towValid = TRUE_FLAG;

	if(parser->fnavParseLogFile!=NULL)
	{
		fprintf(parser->fnavParseLogFile,"PAGE 1: SVID, Clock correction, etc.\n");
		fprintf(parser->fnavParseLogFile,"    SVID: %d\n", svid);
		fprintf(parser->fnavParseLogFile,"    IOD: %d\n", eph->iode1);
		fprintf(parser->fnavParseLogFile,"    WN: %d\n", navMsg->wn);
		fprintf(parser->fnavParseLogFile,"    TOW: %d\n",navMsg->tow);
		fprintf(parser->fnavParseLogFile,"\n");
	}

	return SUCCESS_RETURN;
}


int ParseFNavPage02(TFNavParser *parser)
{
	unsigned char *dataBytes = parser->decodedPage;
	TNavigationMessage *navMsg = parser->navMsg;
	TGnssEphemerides *eph = &(navMsg->ephTmp);

	eph->iode2 = ExtractField(dataBytes,FNAV_TYPE2_IOD_FIRST_BYTE_IDX,FNAV_TYPE2_IOD_LAST_BYTE_IDX,FNAV_TYPE2_IOD_BITS_OF_FIRST_BYTE,FNAV_TYPE2_IOD_BITS_OF_LAST_BYTE);
	navMsg->wn = ExtractField(dataBytes,FNAV_TYPE2_WN_FIRST_BYTE_IDX,FNAV_TYPE2_WN_LAST_BYTE_IDX,FNAV_TYPE2_WN_BITS_OF_FIRST_BYTE,FNAV_TYPE2_WN_BITS_OF_LAST_BYTE);
	navMsg->wn += parser->wnCorrection;
	navMsg->wnValid = TRUE_FLAG;
	navMsg->tow = ExtractField(dataBytes,FNAV_TYPE2_TOW_FIRST_BYTE_IDX,FNAV_TYPE2_TOW_LAST_BYTE_IDX,FNAV_TYPE2_TOW_BITS_OF_FIRST_BYTE,FNAV_TYPE2_TOW_BITS_OF_LAST_BYTE);
	parser->bitIndexTotal = FNAV_PAGE_LENGTH;
	navMsg->towValid = TRUE_FLAG;

	if(parser->fnavParseLogFile!=NULL)
	{
		fprintf(parser->fnavParseLogFile,"PAGE 2: EPHEMERIS (1/3) and GST\n");
		fprintf(parser->fnavParseLogFile,"    IOD: %d\n", eph->iode2);
		fprintf(parser->fnavParseLogFile,"    WN: %d\n", navMsg->wn);
		fprintf(parser->fnavParseLogFile,"    TOW: %d\n",navMsg->tow);
		fprintf(parser->fnavParseLogFile,"\n");
	}

	return SUCCESS_RETURN;
}


int ParseFNavPage03(TFNavParser *parser)
{
	unsigned char *dataBytes = parser->decodedPage;
	TNavigationMessage *navMsg = parser->navMsg;
	TGnssEphemerides *eph = &(navMsg->ephTmp);

	eph->iode3 = ExtractField(dataBytes,FNAV_TYPE3_IOD_FIRST_BYTE_IDX,FNAV_TYPE3_IOD_LAST_BYTE_IDX,FNAV_TYPE3_IOD_BITS_OF_FIRST_BYTE,FNAV_TYPE3_IOD_BITS_OF_LAST_BYTE);
	navMsg->wn = ExtractField(dataBytes,FNAV_TYPE3_WN_FIRST_BYTE_IDX,FNAV_TYPE3_WN_LAST_BYTE_IDX,FNAV_TYPE3_WN_BITS_OF_FIRST_BYTE,FNAV_TYPE3_WN_BITS_OF_LAST_BYTE);
	navMsg->wn += parser->wnCorrection;
	navMsg->wnValid = TRUE_FLAG;
	navMsg->tow = ExtractField(dataBytes,FNAV_TYPE3_TOW_FIRST_BYTE_IDX,FNAV_TYPE3_TOW_LAST_BYTE_IDX,FNAV_TYPE3_TOW_BITS_OF_FIRST_BYTE,FNAV_TYPE3_TOW_BITS_OF_LAST_BYTE);
	parser->bitIndexTotal = FNAV_PAGE_LENGTH;
	navMsg->towValid = TRUE_FLAG;

	navMsg->subframe3Received = TRUE_FLAG;

	if(parser->fnavParseLogFile!=NULL)
	{
		fprintf(parser->fnavParseLogFile,"PAGE 3: EPHEMERIS (2/3) and GST\n");
		fprintf(parser->fnavParseLogFile,"    IOD: %d\n", eph->iode3);
		fprintf(parser->fnavParseLogFile,"    WN: %d\n", navMsg->wn);
		fprintf(parser->fnavParseLogFile,"    TOW: %d\n",navMsg->tow);
		fprintf(parser->fnavParseLogFile,"\n");
	}

	return SUCCESS_RETURN;
}


int ParseFNavPage04(TFNavParser *parser)
{
	unsigned char *dataBytes = parser->decodedPage;
	TNavigationMessage *navMsg = parser->navMsg;
	TGnssEphemerides *eph = &(navMsg->ephTmp);
	int tow;

	eph->iode4 = ExtractField(dataBytes,FNAV_TYPE4_IOD_FIRST_BYTE_IDX,FNAV_TYPE4_IOD_LAST_BYTE_IDX,FNAV_TYPE4_IOD_BITS_OF_FIRST_BYTE,FNAV_TYPE4_IOD_BITS_OF_LAST_BYTE);
	tow = ExtractField(dataBytes,FNAV_TYPE4_TOW_FIRST_BYTE_IDX,FNAV_TYPE4_TOW_LAST_BYTE_IDX,FNAV_TYPE4_TOW_BITS_OF_FIRST_BYTE,FNAV_TYPE4_TOW_BITS_OF_LAST_BYTE);

	navMsg->subframe4Received = TRUE_FLAG;

	if(parser->fnavParseLogFile!=NULL)
	{
		fprintf(parser->fnavParseLogFile,"PAGE 4: EPHEMERIS (3/3), GST-UTC, GST-GPS, and TOW\n");
		fprintf(parser->fnavParseLogFile,"    IOD: %d\n", eph->iode4);
		fprintf(parser->fnavParseLogFile,"    TOW: %d\n", tow);
		fprintf(parser->fnavParseLogFile,"\n");
	}

	return SUCCESS_RETURN;
}


int ParseFNavPage05(TFNavParser *parser)
{
	unsigned char *dataBytes = parser->decodedPage;
	int ioda;
	int svid1;
	int svid2;

	ioda = ExtractField(dataBytes,FNAV_TYPE5_IODA_FIRST_BYTE_IDX,FNAV_TYPE5_IODA_LAST_BYTE_IDX,FNAV_TYPE5_IODA_BITS_OF_FIRST_BYTE,FNAV_TYPE5_IODA_BITS_OF_LAST_BYTE);

	if(parser->fnavParseLogFile!=NULL)
	{
		fprintf(parser->fnavParseLogFile,"PAGE 5:\n");
		fprintf(parser->fnavParseLogFile,"    IODa %d\n", ioda);
	}

	/* parse the almanac */
	// SVID1
	svid1 = ExtractField(dataBytes,FNAV_TYPE5_SVID1_FIRST_BYTE_IDX,FNAV_TYPE5_SVID1_LAST_BYTE_IDX,FNAV_TYPE5_SVID1_BITS_OF_FIRST_BYTE,FNAV_TYPE5_SVID1_BITS_OF_LAST_BYTE);
	if(parser->fnavParseLogFile!=NULL)
	{
		if((svid1<BASE_PRN) || (svid1>GALILEO_NUMBER_OF_PRNS))
		{
			fprintf(parser->fnavParseLogFile,"PAGE 5: DUMMY ALMANAC SVID1\n");
		}
		else
		{
			fprintf(parser->fnavParseLogFile,"PAGE 5: ALMANAC FOR SVID %2d (full)\n",svid1);
		}
	}
	//SVID2
	svid2 = ExtractField(dataBytes,FNAV_TYPE5_SVID2_FIRST_BYTE_IDX,FNAV_TYPE5_SVID2_LAST_BYTE_IDX,FNAV_TYPE5_SVID2_BITS_OF_FIRST_BYTE,FNAV_TYPE5_SVID2_BITS_OF_LAST_BYTE);
	if(parser->fnavParseLogFile!=NULL)
	{
		if((svid2<BASE_PRN) || (svid2>GALILEO_NUMBER_OF_PRNS))
		{
			fprintf(parser->fnavParseLogFile,"PAGE 5: DUMMY ALMANAC SVID2\n");
		}
		else
		{
			fprintf(parser->fnavParseLogFile,"PAGE 5: ALMANAC FOR SVID %2d (1/2)\n",svid2);
		}
		fprintf(parser->fnavParseLogFile,"\n");
	}

	return SUCCESS_RETURN;
}


int ParseFNavPage06(TFNavParser *parser)
{
	unsigned char *dataBytes = parser->decodedPage;
	int ioda;
	int svid3;

	ioda = ExtractField(dataBytes,FNAV_TYPE6_IODA_FIRST_BYTE_IDX,FNAV_TYPE6_IODA_LAST_BYTE_IDX,FNAV_TYPE6_IODA_BITS_OF_FIRST_BYTE,FNAV_TYPE6_IODA_BITS_OF_LAST_BYTE);

	if(parser->fnavParseLogFile!=NULL)
	{
		fprintf(parser->fnavParseLogFile,"PAGE 6:\n");
		fprintf(parser->fnavParseLogFile,"    IODa %d\n", ioda);
	}

	/* parse the almanac */
	// SVID2
	// nothing to do now
	//SVID3
	svid3 = ExtractField(dataBytes,FNAV_TYPE6_SVID3_FIRST_BYTE_IDX,FNAV_TYPE6_SVID3_LAST_BYTE_IDX,FNAV_TYPE6_SVID3_BITS_OF_FIRST_BYTE,FNAV_TYPE6_SVID3_BITS_OF_LAST_BYTE);
	if(parser->fnavParseLogFile!=NULL)
	{
		if((svid3<BASE_PRN) || (svid3>GALILEO_NUMBER_OF_PRNS))
		{
			if(svid3==GAL_SVID_FOR_DUMMY_ALMANAC)
			{
				fprintf(parser->fnavParseLogFile,"PAGE 6: DUMMY ALMANAC SVID3\n");
			}
		}
		else
		{
			fprintf(parser->fnavParseLogFile,"PAGE 6: ALMANAC FOR SVID %2d (full)\n",svid3);
		}
		fprintf(parser->fnavParseLogFile,"\n");
	}

	return SUCCESS_RETURN;
}



int ParseFNavPage(TNGeneData *rx,TFNavParser *parser)
{
	unsigned int wordType;
	int parseReturn = SUCCESS_RETURN;

	wordType = ExtractField(parser->decodedPage,FNAV_WORDTYPE_FIRST_BYTE_IDX,FNAV_WORDTYPE_LAST_BYTE_IDX,FNAV_WORDTYPE_BITS_OF_FIRST_BYTE,FNAV_WORDTYPE_BITS_OF_LAST_BYTE);

	/* parse each word */
	switch(wordType)
	{
	case 1:
		parseReturn = ParseFNavPage01(parser);
		break;
	case 2:
		parseReturn = ParseFNavPage02(parser);
		break;
	case 3:
		parseReturn = ParseFNavPage03(parser);
		break;
	case 4:
		parseReturn = ParseFNavPage04(parser);
		break;
	case 5:
		parseReturn = ParseFNavPage05(parser);
		break;
	case 6:
		parseReturn = ParseFNavPage06(parser);
		break;
	case 63:
		if(parser->fnavParseLogFile!=NULL)
		{
			fprintf(parser->fnavParseLogFile,"WORD 63: DUMMY\n");
			fprintf(parser->fnavParseLogFile,"\n");
		}
		parser->dummyMsg = TRUE_FLAG;
		parseReturn = SUCCESS_RETURN;
		break;
	default:
		if(parser->fnavParseLogFile!=NULL)
		{
			fprintf(parser->fnavParseLogFile,"WORD %2u: Unknown or reserved\n",wordType);
			fprintf(parser->fnavParseLogFile,"\n");
		}
		parseReturn = FAILURE_RETURN;
		break;
	}
	if(parseReturn==FAILURE_RETURN)
	{
		return parseReturn;
	}

	/* Save for Galileo OSNMA cross-authentication
	 * (only when OSNMA bits are received) */
	// "adjust" wordType when page 6 is received: use it like a page 5
	wordType = (wordType==6) ? 5 : wordType;
	//
	if((rx->osnmaData.osnmaTransmitted==TRUE_FLAG) && (parser->dummyMsg==FALSE_FLAG) &&
	   (((parser->lastSavedPage==5) && (wordType==1)) || (parser->lastSavedPage==((int)wordType-1))))
	{
		memcpy(&(parser->subframe[(wordType-1)*FNAV_DECODED_PAGE_LENGTH_BYTES]),parser->decodedPage,FNAV_DECODED_PAGE_LENGTH_BYTES);
		parser->lastSavedPage = wordType;
	}

	return parseReturn;
}


TParseResult DecodeFNavPage(TNGeneData *rx,TFNavParser *parser,unsigned char symbol)
{
	long metric;

	parser->page[parser->byteIndex] = (parser->page[parser->byteIndex] << 1) | symbol;
	parser->bitIndex++;
	if(parser->bitIndex==BITS_PER_BYTE)
	{
		parser->byteIndex++;
		parser->bitIndex=0;
		if(parser->byteIndex==FNAV_ENCODED_PAGE_LENGTH_BYTES)
		{
			parser->byteIndex=0;
			FNavDeinterleaver(parser);
			GalileoViterbi(&metric,parser->decodedPage,parser->deinterleavedPage,FNAV_DECODED_PAGE_LENGTH_BITS,0,0);
			if(GalCrc24qVerified(parser->decodedPage,FNAV_ARRAY_FOR_CRC_LENGTH_BYTES))
			{
				return CRC_ERROR;
			}
			if(ParseFNavPage(rx,parser)==FAILURE_RETURN)
			{
				return DECODING_ERROR;
			}
			return DECODING_1_SUCCESSFUL;
		}
	}
	return DECODING_ON_GOING;
}


void ParseFNavMessage(TNGeneData *rx,TChannel *ch,float corr)
{
	TFNavParser *parser = ch->fnavParser;
	TNavigationMessage *navMsg = parser->navMsg;
	unsigned char symbol = (corr > 0.0) ? parser->polarityCorrection : (parser->polarityCorrection^0x01);
	TParseResult parseResult = DECODING_ON_GOING;
	int wn;
	int tow;

	/* count the received symbol */
	parser->bitIndexTotal++;

	/* parse state machine */
	switch(parser->status)
	{
	case SEARCH_SYNCHRONIZATION:
		if(FNavSynchronization(parser,symbol)==PREAMBLE_FOUND)
		{
			parser->status=FIRST_DECODING;
		}
		break;
	case FIRST_DECODING:
		parseResult =  DecodeFNavPage(rx,parser,symbol);
		if(parseResult==DECODING_1_SUCCESSFUL)
		{
			parser->status=PREAMBLE_1;
			parser->synchronizationSuccess=TRUE_FLAG;
		}

		break;
	case PREAMBLE_1:
		if(FNavSynchronization(parser,symbol)==PREAMBLE_FOUND)
		{
			parser->status=DECODING_1;
		}
		break;
	case DECODING_1:
		parseResult = DecodeFNavPage(rx,parser,symbol);
		if(parseResult==DECODING_1_SUCCESSFUL)
		{
			// IFEN format log
			if((navMsg->wnValid==TRUE_FLAG) && (navMsg->towValid==TRUE_FLAG) && (parser->synchronizationSuccess==TRUE_FLAG))
			{
				// do it when synchronization is confirmed and time is valid
				wn = navMsg->wn;
				tow = navMsg->tow + ((parser->bitIndexTotal*GALILEO_E5A_BIT_DURATION)/SECONDS_TO_MILLISECONDS) - FNAV_PAGE_DURATION;
				SaveGalileoFNavSymbolsInIfenLog(rx->ifenLogFNav,&(rx->ifenLogFNavTime),wn,tow,ch->satId,parser->page);
			}
		}
		if(parseResult!=DECODING_ON_GOING)
		{
			parser->status=PREAMBLE_1;
		}
		break;
	default:
		break;
	}

	/* osnma */
	if(parseResult==DECODING_1_SUCCESSFUL)
	{
		/* save a frame for Galileo OSNMA cross-authentication
	       save in the MACK list when an entire frame is available,
	       time is valid (both WN and TOW), IODE (IODnav in the ICD) are coherent */
		if(parser->lastSavedPage==5)
		{
			parser->lastSavedPage=0;
			if((navMsg->towValid==TRUE_FLAG) && (navMsg->wnValid==TRUE_FLAG) &&
			   (navMsg->ephTmp.iode1==navMsg->ephTmp.iode2) && (navMsg->ephTmp.iode2==navMsg->ephTmp.iode3) &&  (navMsg->ephTmp.iode3==navMsg->ephTmp.iode4))
			{
				TMackList *mackPtr;
				TNavDataList *navDataPtr;

				// transmission time for the begin of the subframe
				wn = navMsg->wn;
				tow = navMsg->tow + ((parser->bitIndexTotal*GALILEO_E5A_BIT_DURATION)/SECONDS_TO_MILLISECONDS) - FNAV_SUBFRAME_DURATION;
				if(tow<0)
				{
					// previous week: correct WN and TOW
					wn--;
					tow += SECONDS_PER_WEEK;
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
				memcpy(navDataPtr->navData,parser->subframe,FNAV_SUBFRAME_BYTES);
			}
		}
	}
	else if(parseResult==CRC_ERROR)
	{
		// discard the current subframe
		parser->lastSavedPage = 0;
	}

}
