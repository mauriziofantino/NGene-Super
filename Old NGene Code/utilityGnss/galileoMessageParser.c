/**
 * @file galileoMessageParser.c
 *
 * @author Mario Nicola
 *
 * @date 13/10/2014
 *
 * @version 1.0.0
 *
 * @brief Functions used to parse the Galileo navigation message.
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "ngeneDataStructure.h"
#include "ngeneConfiguration.h"
#include "channelDataStructure.h"
#include "galileoMessageParser.h"
#include "navigationMessage.h"
#include "navigationMessageParser.h"
#include "ngeneConstants.h"
#include "timeConstants.h"
#include "ioFunctions.h"
#include "closeManager.h"
#include "viterbi.h"
#include "arrays.h"
#include "reedSolomon.h"
#include "rinexLog.h"
#include "osnmaManagement.h"
#include "ifenLog.h"
#include "navigationMessage.h"


static unsigned int galCrc24Table[256] = {
	      0x00000000, 0x00864CFB, 0x008AD50D, 0x000C99F6, 0x0093E6E1, 0x0015AA1A,
	      0x001933EC, 0x009F7F17, 0x00A18139, 0x0027CDC2, 0x002B5434, 0x00AD18CF,
	      0x003267D8, 0x00B42B23, 0x00B8B2D5, 0x003EFE2E, 0x00C54E89, 0x00430272,
	      0x004F9B84, 0x00C9D77F, 0x0056A868, 0x00D0E493, 0x00DC7D65, 0x005A319E,
	      0x0064CFB0, 0x00E2834B, 0x00EE1ABD, 0x00685646, 0x00F72951, 0x007165AA,
	      0x007DFC5C, 0x00FBB0A7, 0x000CD1E9, 0x008A9D12, 0x008604E4, 0x0000481F,
	      0x009F3708, 0x00197BF3, 0x0015E205, 0x0093AEFE, 0x00AD50D0, 0x002B1C2B,
	      0x002785DD, 0x00A1C926, 0x003EB631, 0x00B8FACA, 0x00B4633C, 0x00322FC7,
	      0x00C99F60, 0x004FD39B, 0x00434A6D, 0x00C50696, 0x005A7981, 0x00DC357A,
	      0x00D0AC8C, 0x0056E077, 0x00681E59, 0x00EE52A2, 0x00E2CB54, 0x006487AF,
	      0x00FBF8B8, 0x007DB443, 0x00712DB5, 0x00F7614E, 0x0019A3D2, 0x009FEF29,
	      0x009376DF, 0x00153A24, 0x008A4533, 0x000C09C8, 0x0000903E, 0x0086DCC5,
	      0x00B822EB, 0x003E6E10, 0x0032F7E6, 0x00B4BB1D, 0x002BC40A, 0x00AD88F1,
	      0x00A11107, 0x00275DFC, 0x00DCED5B, 0x005AA1A0, 0x00563856, 0x00D074AD,
	      0x004F0BBA, 0x00C94741, 0x00C5DEB7, 0x0043924C, 0x007D6C62, 0x00FB2099,
	      0x00F7B96F, 0x0071F594, 0x00EE8A83, 0x0068C678, 0x00645F8E, 0x00E21375,
	      0x0015723B, 0x00933EC0, 0x009FA736, 0x0019EBCD, 0x008694DA, 0x0000D821,
	      0x000C41D7, 0x008A0D2C, 0x00B4F302, 0x0032BFF9, 0x003E260F, 0x00B86AF4,
	      0x002715E3, 0x00A15918, 0x00ADC0EE, 0x002B8C15, 0x00D03CB2, 0x00567049,
	      0x005AE9BF, 0x00DCA544, 0x0043DA53, 0x00C596A8, 0x00C90F5E, 0x004F43A5,
	      0x0071BD8B, 0x00F7F170, 0x00FB6886, 0x007D247D, 0x00E25B6A, 0x00641791,
	      0x00688E67, 0x00EEC29C, 0x003347A4, 0x00B50B5F, 0x00B992A9, 0x003FDE52,
	      0x00A0A145, 0x0026EDBE, 0x002A7448, 0x00AC38B3, 0x0092C69D, 0x00148A66,
	      0x00181390, 0x009E5F6B, 0x0001207C, 0x00876C87, 0x008BF571, 0x000DB98A,
	      0x00F6092D, 0x007045D6, 0x007CDC20, 0x00FA90DB, 0x0065EFCC, 0x00E3A337,
	      0x00EF3AC1, 0x0069763A, 0x00578814, 0x00D1C4EF, 0x00DD5D19, 0x005B11E2,
	      0x00C46EF5, 0x0042220E, 0x004EBBF8, 0x00C8F703, 0x003F964D, 0x00B9DAB6,
	      0x00B54340, 0x00330FBB, 0x00AC70AC, 0x002A3C57, 0x0026A5A1, 0x00A0E95A,
	      0x009E1774, 0x00185B8F, 0x0014C279, 0x00928E82, 0x000DF195, 0x008BBD6E,
	      0x00872498, 0x00016863, 0x00FAD8C4, 0x007C943F, 0x00700DC9, 0x00F64132,
	      0x00693E25, 0x00EF72DE, 0x00E3EB28, 0x0065A7D3, 0x005B59FD, 0x00DD1506,
	      0x00D18CF0, 0x0057C00B, 0x00C8BF1C, 0x004EF3E7, 0x00426A11, 0x00C426EA,
	      0x002AE476, 0x00ACA88D, 0x00A0317B, 0x00267D80, 0x00B90297, 0x003F4E6C,
	      0x0033D79A, 0x00B59B61, 0x008B654F, 0x000D29B4, 0x0001B042, 0x0087FCB9,
	      0x001883AE, 0x009ECF55, 0x009256A3, 0x00141A58, 0x00EFAAFF, 0x0069E604,
	      0x00657FF2, 0x00E33309, 0x007C4C1E, 0x00FA00E5, 0x00F69913, 0x0070D5E8,
	      0x004E2BC6, 0x00C8673D, 0x00C4FECB, 0x0042B230, 0x00DDCD27, 0x005B81DC,
	      0x0057182A, 0x00D154D1, 0x0026359F, 0x00A07964, 0x00ACE092, 0x002AAC69,
	      0x00B5D37E, 0x00339F85, 0x003F0673, 0x00B94A88, 0x0087B4A6, 0x0001F85D,
	      0x000D61AB, 0x008B2D50, 0x00145247, 0x00921EBC, 0x009E874A, 0x0018CBB1,
	      0x00E37B16, 0x006537ED, 0x0069AE1B, 0x00EFE2E0, 0x00709DF7, 0x00F6D10C,
	      0x00FA48FA, 0x007C0401, 0x0042FA2F, 0x00C4B6D4, 0x00C82F22, 0x004E63D9,
	      0x00D11CCE, 0x00575035, 0x005BC9C3, 0x00DD8538 };


int InitGalParser(TNGeneConfiguration *cfg,TChannel *ch)
{
	if((ch->satType!=GALILEO_SAT) || (ch->satSignal!=E1_BC))
	{
		ch->galParser = NULL;
		return SUCCESS_RETURN;
	}
	ch->galParser = (TGalParser *)malloc(sizeof(TGalParser));
	if(ch->galParser==NULL)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Malloc returned a NULL pointer in InitGalParser");
		CloseNGene(cfg,EXIT_FAILURE);
	}
	ch->galParser->navMsg=NULL;
	ch->galParser->redCedInPvt = cfg->pvtConfiguration.redCedInPvt;
	ch->galParser->wnCorrection = cfg->navMsgConfiguration.galileoWnCorrection;
	ch->galParser->galParseLogFile = NULL;
	return SUCCESS_RETURN;
}


void DeinitGalParser(TChannel *ch)
{
	if(ch->galParser!=NULL)
	{
		if(ch->galParser->galParseLogFile!=NULL)
		{
			fclose(ch->galParser->galParseLogFile);
		}
		free(ch->galParser);
		ch->galParser=NULL;
	}
}


void ResetReedSolomonData(TGalParser *parser)
{
	int idx;

	parser->rsIod = INVALID_VALUE;
	for(idx=0;idx<GAL_RS_NUM_OF_USED_WORDS;idx++)
	{
		parser->rsInformationReceived[idx] = FALSE_FLAG;
		parser->rsInformationValid[idx] = FALSE_FLAG;
		parser->rsParityReceived[idx] = FALSE_FLAG;
		parser->rsParityValid[idx] = FALSE_FLAG;
	}
	ArrayClean((char *)parser->rsInformationVector,GAL_RS_INFORMATION_VECTOR_LENGTH_BYTES);
	ArrayClean((char *)parser->rsParityVector,GAL_RS_PARITY_VECTOR_LENGTH_BYTES);
}


void StartGalParser(TNGeneConfiguration *cfg,TNGeneData *rx,TChannel *ch)
{
	TGalParser *parser = ch->galParser;
	char filename[2*MAX_ROW_LENGTH];

	if((ch->satType!=GALILEO_SAT) || (ch->satSignal!=E1_BC))
		return;

	if(ch->satId==INVALID_PRN)
		return;

	parser->satId = ch->satId;

	parser->galPreamble[1] = (unsigned char)(GAL_PREAMBLE_BITS >> 8) & 0x03;
	parser->galPreamble[0] = (unsigned char)(GAL_PREAMBLE_BITS & 0x0ff);
	parser->galPreambleInv[1] = (unsigned char)(GAL_INV_PREAMBLE_BITS >> 8) & 0x03;
	parser->galPreambleInv[0] = (unsigned char)(GAL_INV_PREAMBLE_BITS & 0x0ff);
	parser->polarityCorrection = 0x00;
	parser->bitIndexTotal = 0;
	parser->byteIndex = 0;
	parser->bitIndex = 0;
	parser->almSvIdPrev = INVALID_PRN;
	parser->almSvIdPrevTime = INVALID_VALUE;
	parser->synchronizationSuccess = FALSE_FLAG;
	parser->dummyMsg = FALSE_FLAG;
	parser->status = SEARCH_SYNCHRONIZATION;
	parser->navMsg = &(rx->galNavigationMessage[ch->satId-BASE_PRN]);
	parser->navMsg->towValid=FALSE_FLAG;
	parser->navMsg->wnValid=FALSE_FLAG;
	/* OSNMA */
	InitChannelOsnmaVariables(ch,&(parser->chOsnma));
	/* SSP */
	parser->sspIndex = 0;
	parser->ssp = 0x00;
	/* Reed-Solomon */
	ResetReedSolomonData(parser);
	/* TODO qui si può aggiungere la predizione dei contatori */
	if(cfg->logConfiguration.enableNavMesgLog==TRUE_FLAG)
	{
		if(parser->galParseLogFile!=NULL)
		{
			fclose(parser->galParseLogFile);
		}
		sprintf(filename,"%s/navigationMessage_GAL%02d.txt",cfg->dumpPathCurrent,ch->satId);
		parser->galParseLogFile = fopen(filename,"a");
	}
}


TParseResult GalSynchronization(TGalParser *parser,unsigned char symbol)
{
	TParseResult res = PREAMBLE_NOT_FOUND;

	parser->pagePart[1] = (((parser->pagePart[1]<<1) & 0x03) | (parser->pagePart[0] >> 7));
	parser->pagePart[0] = ((parser->pagePart[0]<<1) | symbol);
	if(parser->bitIndex<GAL_PREAMBLE_LENGTH)
	{
		parser->bitIndex++;
	}
	if(parser->bitIndex==GAL_PREAMBLE_LENGTH)
	{
		if((parser->pagePart[1]==parser->galPreamble[1]) && (parser->pagePart[0]==parser->galPreamble[0]))
		{
			//parser->polarityCorrection = parser->polarityCorrection;
			res = PREAMBLE_FOUND;
		}
		if((parser->pagePart[1]==parser->galPreambleInv[1]) && (parser->pagePart[0]==parser->galPreambleInv[0]))
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


void GalDeinterleaver(TGalParser *parser)
{
    unsigned short i, bitIdx, byteIdx;
    unsigned short idx;
    unsigned short byteStartVect[8] = { 0, 3, 7, 11, 15, 18, 22, 26 };
    unsigned short bitStartVect[8] = { 0, 6, 4, 2, 0, 6, 4, 2 };

    for(i=0;i<GAL_ENCODED_PAGE_PART_LENGTH_SYMBOLS;i++)
    {
    	idx = i & 0x00000007;
    	byteIdx = byteStartVect[idx];
    	bitIdx = bitStartVect[idx];
    	parser->deinterleavedPagePart[i] = ( (parser->pagePart[byteIdx] >> (8 - (bitIdx + 1))) & 0x1);
    	// Set new indexes for bitStartVect and byteStartVect (Compact version)
    	// If bitStartVect[idx]+1 == 8 then increment byteStartVect[idx]
    	byteStartVect[idx] = byteStartVect[idx] + ((bitStartVect[idx] + 1) >> 3);
    	// set bitStartVect[idx] to (bitStartVect[idx] + 1) mod 7
    	bitStartVect[idx] = (bitStartVect[idx] + 1) & 0x00000007;
    }
}


void PrepareGalBits(TGalParser *parser)
{
	int i;

	/* Prepare bit for CRC control */
	ArrayClean((char *)parser->bitsForCrc,GAL_ARRAY_FOR_CRC_LENGTH_BYTES);
	for(i=0;i<14;i++)
	{
		parser->bitsForCrc[i] = parser->decodedPagePart1[i];
	}
	parser->bitsForCrc[i] = parser->decodedPagePart1[i] & 0xc0; /* i==14 */
	for(;i<27;i++)
	{
		parser->bitsForCrc[i] = parser->bitsForCrc[i] | ((parser->decodedPagePart2[i-14] >> 2) & 0x3f);
		parser->bitsForCrc[i+1] = (parser->decodedPagePart2[i-14] << 6) & 0xc0;
	}
	parser->bitsForCrc[i] = parser->bitsForCrc[i] | ((parser->decodedPagePart2[i-14] >> 2) & 0x30);
	/* Prepare bit for parsing */
	ArrayClean((char *)parser->bitsForParse,GAL_ARRAY_FOR_PARSE_LENGTH_BYTES);
	for(i=0;i<14;i++)
	{
		parser->bitsForParse[i] = (parser->decodedPagePart1[i] << 2) & 0xfc;
		parser->bitsForParse[i] = parser->bitsForParse[i] | ((parser->decodedPagePart1[i+1] >> 6) & 0x03);
	}
	for(;i<GAL_ARRAY_FOR_PARSE_LENGTH_BYTES;i++) /* i == 14 */
	{
		parser->bitsForParse[i] = (parser->decodedPagePart2[i-14]  << 2) & 0xfc;
		parser->bitsForParse[i] = parser->bitsForParse[i] | ((parser->decodedPagePart2[i-14+1] >> 6) & 0x03);
	}
}


void ExtractGalreservedBits(TGalParser *parser)
{
	int i;
	int i2;
	unsigned char mask;

	/* Save reserved bits */

	/* Reserved 1 */
	// ArrayClean((char *)parser->reserved1,GAL_RESERVED1_SIZE_BYTES); /* not needed, all bytes are overwritten */
	ArrayClean((char *)parser->reserved1Unpk,GAL_RESERVED1_SIZE_BITS);
	for(i=0;i<GAL_RESERVED1_SIZE_BYTES;i++)
	{
		parser->reserved1[i] = ((parser->decodedPagePart2[2+i] << 2) & 0xfc) | ((parser->decodedPagePart2[3+i] >> 6) & 0x03);
		mask = (unsigned char)0x80;
		for(i2=0;i2<BITS_PER_BYTE;i2++)
		{
			if((parser->reserved1[i] & mask)!=0)
			{
				parser->reserved1Unpk[(i*BITS_PER_BYTE)+i2] = (unsigned char)0x01;
			}
			mask = mask >> 1;
		}
	}

	if(parser->galParseLogFile!=NULL)
	{
		fprintf(parser->galParseLogFile,"    Reserved1: ");
		for(i=0;i<GAL_RESERVED1_SIZE_BITS;i++)
		{
			fprintf(parser->galParseLogFile,"%1u",(unsigned int)parser->reserved1Unpk[i]);
		}
		fprintf(parser->galParseLogFile,"\n");
	}

	/* Reserved 2 */
	parser->ssp = ((parser->decodedPagePart2[13] & 0x3f)<<2) | ((parser->decodedPagePart2[14] & 0xc0)>>6);
	switch(parser->ssp)
	{
	case GAL_SSP1:
		parser->sspIndex = 1;
		break;
	case GAL_SSP2:
		parser->sspIndex = 2;
		break;
	case GAL_SSP3:
		parser->sspIndex = 3;
		break;
	default:
		parser->sspIndex = 0;
		break;
	}

	if((parser->galParseLogFile!=NULL) && (parser->sspIndex!=0))
	{
		fprintf(parser->galParseLogFile,"    SSP%d (0x%02x)\n",parser->sspIndex,parser->ssp);
	}
	else if(parser->galParseLogFile!=NULL)
	{
		fprintf(parser->galParseLogFile,"    NO SSP (0x%02x)\n",parser->ssp);
	}
}


int GalCrc24qVerified(unsigned char *vec, int vecLen)
{
    unsigned int r;
    unsigned int tabIdx;

    r = 0;
    tabIdx = 0;

    while(vecLen--)
    {
		tabIdx = (r >> 16) & 0xFF;
		r = (r << 8) | *vec++;
		r ^= galCrc24Table[tabIdx];
    }
    r &= 0xFFFFFF;
    if(0==r)
	{
    	return SUCCESS_RETURN;
	}
    return FAILURE_RETURN;
}


void WriteReedSolomonInformationVector(TGalParser *parser,unsigned int wordType,int crcCheckResult)
{
	unsigned char *dataBytes = parser->bitsForParse;
	TNavigationMessage *navMsg = parser->navMsg;
	TGnssEphemerides *eph = &(navMsg->ephTmp);
	int iod;
	int iodChanged; // TRUE if the parsed word has a new IOD
	int destIdx;

	/* Get the destination index, Get the current IOD */
	switch(wordType)
	{
		case 1:
			iod = eph->iode1;
			iodChanged = (iod!=eph->iode2) && (eph->iode2==eph->iode3) && (eph->iode2==eph->iode4) && ((iod & 0x03)!=parser->rsIod);
			destIdx = 0;
			break;
		case 2:
			iod = eph->iode2;
			iodChanged = (iod!=eph->iode1) && (eph->iode1==eph->iode3) && (eph->iode1==eph->iode4) && ((iod & 0x03)!=parser->rsIod);
			destIdx = 16;
			break;
		case 3:
			iod = eph->iode3;
			iodChanged = (iod!=eph->iode1) && (eph->iode1==eph->iode2) && (eph->iode1==eph->iode4) && ((iod & 0x03)!=parser->rsIod);
			destIdx = 30;
			break;
		case 4:
			iod = eph->iode4;
			iodChanged = (iod!=eph->iode1) && (eph->iode1==eph->iode2) && (eph->iode1==eph->iode3) && ((iod & 0x03)!=parser->rsIod);
			destIdx = 44;
			break;
		default:
			// do nothing
			return;
			break;
	}
	/* Check IOD (only if no CRC error occurred) */
	if(crcCheckResult==TRUE_FLAG)
	{
		if((parser->rsIod==INVALID_VALUE) || (iodChanged))
		{
			// if rsIod is not valid, initialize with IOD read from the current word or
			// a change in the transmitted IOD occurred
			ResetReedSolomonData(parser);
			parser->rsIod = iod & 0x03;
		}
		else if((parser->rsIod!=INVALID_VALUE) && (parser->rsIod!=(iod & 0x03)))
		{
			// IOD not consistent, do nothing
			return;
		}
	}

	/* now save! */
	if(wordType==1)
	{
		// first two bytes saved only for word 1 (but scrambled!)
		parser->rsInformationVector[destIdx] = (dataBytes[0] & 0xfc) | (dataBytes[1] & 0x03);
		parser->rsInformationVector[destIdx+1] = ((dataBytes[0] & 0x03)<<6) | ((dataBytes[1] & 0xfc)>>2);
		destIdx += 2;
	}
	memcpy(&(parser->rsInformationVector[destIdx]),&(dataBytes[2]),14);
	parser->rsInformationReceived[wordType-1] = TRUE_FLAG;
	parser->rsInformationValid[wordType-1] = crcCheckResult;
}


int ParseGalWord00(TGalParser *parser)
{
	unsigned char *dataBytes = parser->bitsForParse;
	TNavigationMessage *navMsg = parser->navMsg;
	int time;

	time = ExtractField(dataBytes,GAL_TYPE0_TIME_FIRST_BYTE_IDX,GAL_TYPE0_TIME_LAST_BYTE_IDX,GAL_TYPE0_TIME_BITS_OF_FIRST_BYTE,GAL_TYPE0_TIME_BITS_OF_LAST_BYTE);
	if(time==2)
	{
		/* when the field 'Time' is not set to '10', the fields WN and TOW do not contain valid data */
		navMsg->wn = ExtractField(dataBytes,GAL_TYPE0_WN_FIRST_BYTE_IDX,GAL_TYPE0_WN_LAST_BYTE_IDX,GAL_TYPE0_WN_BITS_OF_FIRST_BYTE,GAL_TYPE0_WN_BITS_OF_LAST_BYTE);
		navMsg->wn += parser->wnCorrection;
		navMsg->wnValid = TRUE_FLAG;
		navMsg->tow = ExtractField(dataBytes,GAL_TYPE0_TOW_FIRST_BYTE_IDX,GAL_TYPE0_TOW_LAST_BYTE_IDX,GAL_TYPE0_TOW_BITS_OF_FIRST_BYTE,GAL_TYPE0_TOW_BITS_OF_LAST_BYTE);
		parser->bitIndexTotal=GAL_PAGE_LENGTH;
		navMsg->towValid = TRUE_FLAG;
	}

	if(parser->galParseLogFile!=NULL)
	{
		fprintf(parser->galParseLogFile,"SPARE WORD 0\n");
		if(time==2)
		{
			fprintf(parser->galParseLogFile,"    WN: %d\n", navMsg->wn);
			fprintf(parser->galParseLogFile,"    TOW: %d\n",navMsg->tow);
		}
		else
		{
			fprintf(parser->galParseLogFile,"    Invalid time information (%d)\n", time);
		}
		fprintf(parser->galParseLogFile,"\n");
	}
	
	return SUCCESS_RETURN;
}


int ParseGalWord01(TGalParser *parser)
{
	unsigned char *dataBytes = parser->bitsForParse;
	TNavigationMessage *navMsg = parser->navMsg;
	TGnssEphemerides *eph = &(navMsg->ephTmp);

	eph->iode1 = ExtractField(dataBytes,GAL_TYPE1_IOD_FIRST_BYTE_IDX,GAL_TYPE1_IOD_LAST_BYTE_IDX,GAL_TYPE1_IOD_BITS_OF_FIRST_BYTE,GAL_TYPE1_IOD_BITS_OF_LAST_BYTE);
	eph->toe = GAL_TOE_SCALE_FACTOR*ExtractField(dataBytes,GAL_TYPE1_toe_FIRST_BYTE_IDX,GAL_TYPE1_toe_LAST_BYTE_IDX,GAL_TYPE1_toe_BITS_OF_FIRST_BYTE,GAL_TYPE1_toe_BITS_OF_LAST_BYTE);
	eph->meanAnomaly0 = POW_TWO_M31*M_PI*ExtractTwoCompField(dataBytes,GAL_TYPE1_M0_FIRST_BYTE_IDX,GAL_TYPE1_M0_LAST_BYTE_IDX,GAL_TYPE1_M0_BITS_OF_FIRST_BYTE,GAL_TYPE1_M0_BITS_OF_LAST_BYTE,GAL_TYPE1_M0_BITS_FIELD_DIM);
	eph->eccentricity = POW_TWO_M33*ExtractField(dataBytes,GAL_TYPE1_e_FIRST_BYTE_IDX,GAL_TYPE1_e_LAST_BYTE_IDX,GAL_TYPE1_e_BITS_OF_FIRST_BYTE,GAL_TYPE1_e_BITS_OF_LAST_BYTE);
	eph->sqrtA = POW_TWO_M19*ExtractField(dataBytes,GAL_TYPE1_sqrtA_FIRST_BYTE_IDX,GAL_TYPE1_sqrtA_LAST_BYTE_IDX,GAL_TYPE1_sqrtA_BITS_OF_FIRST_BYTE,GAL_TYPE1_sqrtA_BITS_OF_LAST_BYTE);

	navMsg->subframe1Received = TRUE_FLAG;

	if(parser->galParseLogFile!=NULL)
	{
		fprintf(parser->galParseLogFile,"WORD 1: EPHEMERIS (1/4)\n");
		fprintf(parser->galParseLogFile,"    IOD: %d\n", eph->iode1);
		fprintf(parser->galParseLogFile,"    t_0e: %.20g\n", eph->toe);
		fprintf(parser->galParseLogFile,"    M_0: %.20g\n", eph->meanAnomaly0);
		fprintf(parser->galParseLogFile,"    e: %.20g\n", eph->eccentricity);
		fprintf(parser->galParseLogFile,"    sqrtA: %.20e\n", eph->sqrtA);
		fprintf(parser->galParseLogFile,"\n");
	}
	
	return SUCCESS_RETURN;
}


int ParseGalWord02(TGalParser *parser)
{
	unsigned char *dataBytes = parser->bitsForParse;
	TNavigationMessage *navMsg = parser->navMsg;
	TGnssEphemerides *eph = &(navMsg->ephTmp);

	eph->iode2 = ExtractField(dataBytes,GAL_TYPE2_IOD_FIRST_BYTE_IDX,GAL_TYPE2_IOD_LAST_BYTE_IDX,GAL_TYPE2_IOD_BITS_OF_FIRST_BYTE,GAL_TYPE2_IOD_BITS_OF_LAST_BYTE);
	eph->capitalOmega0 = POW_TWO_M31*M_PI *ExtractTwoCompField(dataBytes,GAL_TYPE2_omega0_FIRST_BYTE_IDX,GAL_TYPE2_omega0_LAST_BYTE_IDX,GAL_TYPE2_omega0_BITS_OF_FIRST_BYTE,GAL_TYPE2_omega0_BITS_OF_LAST_BYTE,GAL_TYPE2_omega0_BITS_FIELD_DIM);
	eph->i0 = POW_TWO_M31*M_PI*ExtractTwoCompField(dataBytes,GAL_TYPE2_i0_FIRST_BYTE_IDX,GAL_TYPE2_i0_LAST_BYTE_IDX,GAL_TYPE2_i0_BITS_OF_FIRST_BYTE,GAL_TYPE2_i0_BITS_OF_LAST_BYTE,GAL_TYPE2_i0_BITS_FIELD_DIM);
	eph->omega = POW_TWO_M31*M_PI*ExtractTwoCompField(dataBytes,GAL_TYPE2_omega_FIRST_BYTE_IDX,GAL_TYPE2_omega_LAST_BYTE_IDX,GAL_TYPE2_omega_BITS_OF_FIRST_BYTE,GAL_TYPE2_omega_BITS_OF_LAST_BYTE,GAL_TYPE2_omega_BITS_FIELD_DIM);
	eph->iDot = POW_TWO_M43*M_PI*ExtractTwoCompField(dataBytes,GAL_TYPE2_idot_FIRST_BYTE_IDX,GAL_TYPE2_idot_LAST_BYTE_IDX,GAL_TYPE2_idot_BITS_OF_FIRST_BYTE,GAL_TYPE2_idot_BITS_OF_LAST_BYTE,GAL_TYPE2_idot_BITS_FIELD_DIM);

	navMsg->subframe2Received = TRUE_FLAG;

	if(parser->galParseLogFile!=NULL)
	{
		fprintf(parser->galParseLogFile,"WORD 2: EPHEMERIS (2/4)\n");
		fprintf(parser->galParseLogFile,"    IOD: %d\n", eph->iode2);
		fprintf(parser->galParseLogFile,"    omega_0: %.20g\n", eph->capitalOmega0);
		fprintf(parser->galParseLogFile,"    i_0: %.20g\n", eph->i0);
		fprintf(parser->galParseLogFile,"    omega: %.20g\n", eph->omega);
		fprintf(parser->galParseLogFile,"    iDot: %.20g\n", eph->iDot);
		fprintf(parser->galParseLogFile,"\n");
	}
	
	return SUCCESS_RETURN;
}


int ParseGalWord03(TGalParser *parser)
{
	unsigned char *dataBytes = parser->bitsForParse;
	TNavigationMessage *navMsg = parser->navMsg;
	TGnssEphemerides *eph = &(navMsg->ephTmp);

	eph->iode3 = ExtractField(dataBytes,GAL_TYPE3_IOD_FIRST_BYTE_IDX,GAL_TYPE3_IOD_LAST_BYTE_IDX,GAL_TYPE3_IOD_BITS_OF_FIRST_BYTE,GAL_TYPE3_IOD_BITS_OF_LAST_BYTE);
	eph->capitalOmegaDot = POW_TWO_M43*M_PI*ExtractTwoCompField(dataBytes,GAL_TYPE3_omegadot_FIRST_BYTE_IDX,GAL_TYPE3_omegadot_LAST_BYTE_IDX,GAL_TYPE3_omegadot_BITS_OF_FIRST_BYTE,GAL_TYPE3_omegadot_BITS_OF_LAST_BYTE,GAL_TYPE3_omegadot_BITS_FIELD_DIM);
	eph->deltaN = POW_TWO_M43*M_PI*ExtractTwoCompField(dataBytes,GAL_TYPE3_deltan_FIRST_BYTE_IDX,GAL_TYPE3_deltan_LAST_BYTE_IDX,GAL_TYPE3_deltan_BITS_OF_FIRST_BYTE,GAL_TYPE3_deltan_BITS_OF_LAST_BYTE,GAL_TYPE3_deltan_BITS_FIELD_DIM);
	eph->capitalCuc = POW_TWO_M29*ExtractTwoCompField(dataBytes,GAL_TYPE3_cuc_FIRST_BYTE_IDX,GAL_TYPE3_cuc_LAST_BYTE_IDX,GAL_TYPE3_cuc_BITS_OF_FIRST_BYTE,GAL_TYPE3_cuc_BITS_OF_LAST_BYTE,GAL_TYPE3_cuc_BITS_FIELD_DIM);
	eph->capitalCus = POW_TWO_M29*ExtractTwoCompField(dataBytes,GAL_TYPE3_cus_FIRST_BYTE_IDX,GAL_TYPE3_cus_LAST_BYTE_IDX,GAL_TYPE3_cus_BITS_OF_FIRST_BYTE,GAL_TYPE3_cus_BITS_OF_LAST_BYTE,GAL_TYPE3_cus_BITS_FIELD_DIM);
	eph->capitalCrc = POW_TWO_M05*ExtractTwoCompField(dataBytes,GAL_TYPE3_crc_FIRST_BYTE_IDX,GAL_TYPE3_crc_LAST_BYTE_IDX,GAL_TYPE3_crc_BITS_OF_FIRST_BYTE,GAL_TYPE3_crc_BITS_OF_LAST_BYTE,GAL_TYPE3_crc_BITS_FIELD_DIM);
	eph->capitalCrs = POW_TWO_M05*ExtractTwoCompField(dataBytes,GAL_TYPE3_crs_FIRST_BYTE_IDX,GAL_TYPE3_crs_LAST_BYTE_IDX,GAL_TYPE3_crs_BITS_OF_FIRST_BYTE,GAL_TYPE3_crs_BITS_OF_LAST_BYTE,GAL_TYPE3_crs_BITS_FIELD_DIM);
	eph->sisa = (ExtractField(dataBytes, GAL_TYPE3_SISA_FIRST_BYTE_IDX, GAL_TYPE3_SISA_LAST_BYTE_IDX, GAL_TYPE3_SISA_BITS_OF_FIRST_BYTE, GAL_TYPE3_SISA_BITS_OF_LAST_BYTE));

	navMsg->subframe3Received = TRUE_FLAG;

	if(parser->galParseLogFile!=NULL)
	{
		fprintf(parser->galParseLogFile,"WORD 3: EPHEMERIS (3/4) and SISA\n");
		fprintf(parser->galParseLogFile,"    IOD: %d\n", eph->iode3);
		fprintf(parser->galParseLogFile,"    omegaDot: %.20g\n", eph->capitalOmegaDot);
		fprintf(parser->galParseLogFile,"    deltan: %.20g\n", eph->deltaN);
		fprintf(parser->galParseLogFile,"    C_uc: %.20g\n", eph->capitalCuc);
		fprintf(parser->galParseLogFile,"    C_us: %.20g\n", eph->capitalCus);
		fprintf(parser->galParseLogFile,"    C_rc: %.20g\n", eph->capitalCrc);
		fprintf(parser->galParseLogFile,"    C_rs: %.20g\n", eph->capitalCrs);
		fprintf(parser->galParseLogFile,"    SISA: %d\n", eph->sisa);
		fprintf(parser->galParseLogFile,"\n");
	}
	
	return SUCCESS_RETURN;
}


int ParseGalWord04(TGalParser *parser)
{
	unsigned char *dataBytes = parser->bitsForParse;
	TNavigationMessage *navMsg = parser->navMsg;
	int svId;
	TGnssEphemerides *eph = &(navMsg->ephTmp);
	TGnssClk *clk = &(navMsg->clkTmp);

	eph->iode4 = ExtractField(dataBytes,GAL_TYPE4_IOD_FIRST_BYTE_IDX,GAL_TYPE4_IOD_LAST_BYTE_IDX,GAL_TYPE4_IOD_BITS_OF_FIRST_BYTE,GAL_TYPE4_IOD_BITS_OF_LAST_BYTE);
	svId = ExtractField(dataBytes,GAL_TYPE4_prn_FIRST_BYTE_IDX,GAL_TYPE4_prn_LAST_BYTE_IDX,GAL_TYPE4_prn_BITS_OF_FIRST_BYTE,GAL_TYPE4_prn_BITS_OF_LAST_BYTE);
	eph->capitalCic = POW_TWO_M29*ExtractTwoCompField(dataBytes,GAL_TYPE4_cic_FIRST_BYTE_IDX,GAL_TYPE4_cic_LAST_BYTE_IDX,GAL_TYPE4_cic_BITS_OF_FIRST_BYTE,GAL_TYPE4_cic_BITS_OF_LAST_BYTE,GAL_TYPE4_cic_BITS_FIELD_DIM);
	eph->capitalCis = POW_TWO_M29*ExtractTwoCompField(dataBytes,GAL_TYPE4_cis_FIRST_BYTE_IDX,GAL_TYPE4_cis_LAST_BYTE_IDX,GAL_TYPE4_cis_BITS_OF_FIRST_BYTE,GAL_TYPE4_cis_BITS_OF_LAST_BYTE,GAL_TYPE4_cis_BITS_FIELD_DIM);
	clk->toc = 60*ExtractField(dataBytes,GAL_TYPE4_coc_FIRST_BYTE_IDX,GAL_TYPE4_coc_LAST_BYTE_IDX,GAL_TYPE4_coc_BITS_OF_FIRST_BYTE,GAL_TYPE4_coc_BITS_OF_LAST_BYTE);
	clk->af[0] = POW_TWO_M34*ExtractTwoCompField(dataBytes,GAL_TYPE4_af0_FIRST_BYTE_IDX,GAL_TYPE4_af0_LAST_BYTE_IDX,GAL_TYPE4_af0_BITS_OF_FIRST_BYTE,GAL_TYPE4_af0_BITS_OF_LAST_BYTE,GAL_TYPE4_af0_BITS_FIELD_DIM);
	clk->af[1] = POW_TWO_M46*ExtractTwoCompField(dataBytes,GAL_TYPE4_af1_FIRST_BYTE_IDX,GAL_TYPE4_af1_LAST_BYTE_IDX,GAL_TYPE4_af1_BITS_OF_FIRST_BYTE,GAL_TYPE4_af1_BITS_OF_LAST_BYTE,GAL_TYPE4_af1_BITS_FIELD_DIM);
	clk->af[2] = POW_TWO_M59*ExtractTwoCompField(dataBytes,GAL_TYPE4_af2_FIRST_BYTE_IDX,GAL_TYPE4_af2_LAST_BYTE_IDX,GAL_TYPE4_af2_BITS_OF_FIRST_BYTE,GAL_TYPE4_af2_BITS_OF_LAST_BYTE,GAL_TYPE4_af2_BITS_FIELD_DIM);

	navMsg->subframe4Received = TRUE_FLAG;

	if(parser->galParseLogFile!=NULL)
	{
		fprintf(parser->galParseLogFile,"WORD 4: EPHEMERIS (4/4) and CLOCK CORRECTION PARAMETERS\n");
		fprintf(parser->galParseLogFile,"    IOD: %d\n", eph->iode4);
		fprintf(parser->galParseLogFile,"    PRN: %d\n", svId);
		fprintf(parser->galParseLogFile,"    C_ic: %.20g\n", eph->capitalCic);
		fprintf(parser->galParseLogFile,"    C_is: %.20g\n", eph->capitalCis);
		fprintf(parser->galParseLogFile,"    t_oc: %.20g\n", clk->toc);
		fprintf(parser->galParseLogFile,"    a_f0: %.20g\n", clk->af[0]);
		fprintf(parser->galParseLogFile,"    a_f1: %.20g\n", clk->af[1]);
		fprintf(parser->galParseLogFile,"    a_f2: %.20g\n", clk->af[2]);
		fprintf(parser->galParseLogFile,"\n");
	}
	
	return SUCCESS_RETURN;
}


int ParseGalWord05(TNGeneData *rx,TGalParser *parser)
{
	unsigned char *dataBytes = parser->bitsForParse;
	TNavigationMessage *navMsg = parser->navMsg;
	TGnssEphemerides *eph = &(navMsg->ephTmp);
	TGnssIono *iono = &(navMsg->iono);
	TGnssClk *clk = &(navMsg->clkTmp);
	int flag;

	iono->alphaGal[0] = POW_TWO_M02*ExtractField(dataBytes,GAL_TYPE5_A0I_FIRST_BYTE_IDX,GAL_TYPE5_A0I_LAST_BYTE_IDX,GAL_TYPE5_A0I_BITS_OF_FIRST_BYTE,GAL_TYPE5_A0I_BITS_OF_LAST_BYTE);
	iono->alphaGal[1] = POW_TWO_M08*ExtractTwoCompField(dataBytes,GAL_TYPE5_A1I_FIRST_BYTE_IDX,GAL_TYPE5_A1I_LAST_BYTE_IDX,GAL_TYPE5_A1I_BITS_OF_FIRST_BYTE,GAL_TYPE5_A1I_BITS_OF_LAST_BYTE,GAL_TYPE5_A1I_BITS_FIELD_DIM);
	iono->alphaGal[2] = POW_TWO_M15*ExtractTwoCompField(dataBytes,GAL_TYPE5_A2I_FIRST_BYTE_IDX,GAL_TYPE5_A2I_LAST_BYTE_IDX,GAL_TYPE5_A2I_BITS_OF_FIRST_BYTE,GAL_TYPE5_A2I_BITS_OF_LAST_BYTE,GAL_TYPE5_A2I_BITS_FIELD_DIM);
	flag = (int)ExtractField(dataBytes,GAL_TYPE5_SF1_FIRST_BYTE_IDX,GAL_TYPE5_SF1_LAST_BYTE_IDX,GAL_TYPE5_SF1_BITS_OF_FIRST_BYTE,GAL_TYPE5_SF1_BITS_OF_LAST_BYTE);
	iono->sf[0] = (flag>0) ? TRUE_FLAG : FALSE_FLAG;
	flag = (int)ExtractField(dataBytes,GAL_TYPE5_SF2_FIRST_BYTE_IDX,GAL_TYPE5_SF2_LAST_BYTE_IDX,GAL_TYPE5_SF2_BITS_OF_FIRST_BYTE,GAL_TYPE5_SF2_BITS_OF_LAST_BYTE);
	iono->sf[1] = (flag>0) ? TRUE_FLAG : FALSE_FLAG;
	flag = (int)ExtractField(dataBytes,GAL_TYPE5_SF3_FIRST_BYTE_IDX,GAL_TYPE5_SF3_LAST_BYTE_IDX,GAL_TYPE5_SF3_BITS_OF_FIRST_BYTE,GAL_TYPE5_SF3_BITS_OF_LAST_BYTE);
	iono->sf[2] = (flag>0) ? TRUE_FLAG : FALSE_FLAG;
	flag = (int)ExtractField(dataBytes,GAL_TYPE5_SF4_FIRST_BYTE_IDX,GAL_TYPE5_SF4_LAST_BYTE_IDX,GAL_TYPE5_SF4_BITS_OF_FIRST_BYTE,GAL_TYPE5_SF4_BITS_OF_LAST_BYTE);
	iono->sf[3] = (flag>0) ? TRUE_FLAG : FALSE_FLAG;
	flag = (int)ExtractField(dataBytes,GAL_TYPE5_SF5_FIRST_BYTE_IDX,GAL_TYPE5_SF5_LAST_BYTE_IDX,GAL_TYPE5_SF5_BITS_OF_FIRST_BYTE,GAL_TYPE5_SF5_BITS_OF_LAST_BYTE);
	iono->sf[4] = (flag>0) ? TRUE_FLAG : FALSE_FLAG;
	clk->bgd_E1E5a = POW_TWO_M32*ExtractTwoCompField(dataBytes,GAL_TYPE5_TGD_E1E5A_FIRST_BYTE_IDX,GAL_TYPE5_TGD_E1E5A_LAST_BYTE_IDX,GAL_TYPE5_TGD_E1E5A_BITS_OF_FIRST_BYTE,GAL_TYPE5_TGD_E1E5A_BITS_OF_LAST_BYTE,GAL_TYPE5_TGD_E1E5A_BITS_FIELD_DIM);
	clk->bgd_E1E5b = POW_TWO_M32*ExtractTwoCompField(dataBytes,GAL_TYPE5_TGD_E1E5B_FIRST_BYTE_IDX,GAL_TYPE5_TGD_E1E5B_LAST_BYTE_IDX,GAL_TYPE5_TGD_E1E5B_BITS_OF_FIRST_BYTE,GAL_TYPE5_TGD_E1E5B_BITS_OF_LAST_BYTE,GAL_TYPE5_TGD_E1E5B_BITS_FIELD_DIM);
	clk->tGroupDelay = clk->bgd_E1E5b;
	navMsg->wn = ExtractField(dataBytes,GAL_TYPE5_WN_FIRST_BYTE_IDX,GAL_TYPE5_WN_LAST_BYTE_IDX,GAL_TYPE5_WN_BITS_OF_FIRST_BYTE,GAL_TYPE5_WN_BITS_OF_LAST_BYTE);
	navMsg->wn += parser->wnCorrection;
	navMsg->wnValid = TRUE_FLAG;
	navMsg->tow = ExtractField(dataBytes,GAL_TYPE5_TOW_FIRST_BYTE_IDX,GAL_TYPE5_TOW_LAST_BYTE_IDX,GAL_TYPE5_TOW_BITS_OF_FIRST_BYTE,GAL_TYPE5_TOW_BITS_OF_LAST_BYTE);
	navMsg->towValid = TRUE_FLAG;
	parser->bitIndexTotal = GAL_PAGE_LENGTH;
	eph->health = ExtractField(dataBytes,GAL_TYPE5_E1BHS_FIRST_BYTE_IDX,GAL_TYPE5_E1BHS_LAST_BYTE_IDX,GAL_TYPE5_E1BHS_BITS_OF_FIRST_BYTE,GAL_TYPE5_E1BHS_BITS_OF_LAST_BYTE);
	eph->galileoDataValidity = ExtractField(dataBytes,GAL_TYPE5_E1BDVS_FIRST_BYTE_IDX,GAL_TYPE5_E1BDVS_LAST_BYTE_IDX,GAL_TYPE5_E1BDVS_BITS_OF_FIRST_BYTE,GAL_TYPE5_E1BDVS_BITS_OF_LAST_BYTE);

	navMsg->subframe5Received = TRUE_FLAG;
	navMsg->ionoReceived=TRUE_FLAG;

	rx->galIono.alphaGal[0] = iono->alphaGal[0];
	rx->galIono.alphaGal[1] = iono->alphaGal[1];
	rx->galIono.alphaGal[2] = iono->alphaGal[2];
	rx->galIono.sf[0] = iono->sf[0];
	rx->galIono.sf[1] = iono->sf[1];
	rx->galIono.sf[2] = iono->sf[2];
	rx->galIono.sf[3] = iono->sf[3];
	rx->galIono.sf[4] = iono->sf[4];
	rx->galIonoValid=TRUE_FLAG;

	if(parser->galParseLogFile!=NULL)
	{
		fprintf(parser->galParseLogFile,"WORD 5: IONOSPHERIC CORRECTION, BGD, SIGNAL HEALTH and DATA VALIDITY STATUS and GST\n");
		fprintf(parser->galParseLogFile,"    Ai0: %.20g\n", iono->alphaGal[0]);
		fprintf(parser->galParseLogFile,"    Ai1: %.20g\n", iono->alphaGal[1]);
		fprintf(parser->galParseLogFile,"    Ai2: %.20g\n", iono->alphaGal[2]);
		fprintf(parser->galParseLogFile,"    SF: %d%d%d%d%d\n",	(int)iono->sf[0],(int)iono->sf[1],(int)iono->sf[2],(int)iono->sf[3],(int)iono->sf[4]);
		fprintf(parser->galParseLogFile,"    BGD(E1,E5a): %.20g\n", clk->bgd_E1E5a);
		fprintf(parser->galParseLogFile,"    BGD(E1,E5b): %.20g\n", clk->bgd_E1E5b);
		fprintf(parser->galParseLogFile,"    WN: %d\n", navMsg->wn);
		fprintf(parser->galParseLogFile,"    TOW: %d\n", navMsg->tow);
		fprintf(parser->galParseLogFile,"    E1b HS: %d\n", (int)eph->health);
		fprintf(parser->galParseLogFile,"    E1b DVS: %d\n", (int)eph->galileoDataValidity);
		fprintf(parser->galParseLogFile,"\n");
	}
	
	return SUCCESS_RETURN;
}


int ParseGalWord06(TGalParser *parser)
{
	unsigned char *dataBytes = parser->bitsForParse;
	TNavigationMessage *navMsg = parser->navMsg;
	TGnssToUtcTimeConversion *gnss2Utc = &(navMsg->gnss2Utc);
	int tow;

	gnss2Utc->a0 = POW_TWO_M30*ExtractTwoCompField(dataBytes,GAL_TYPE6_A0_FIRST_BYTE_IDX,GAL_TYPE6_A0_LAST_BYTE_IDX,GAL_TYPE6_A0_BITS_OF_FIRST_BYTE,GAL_TYPE6_A0_BITS_OF_LAST_BYTE,GAL_TYPE6_A0_BITS_FIELD_DIM);
	gnss2Utc->a1 = POW_TWO_M50*ExtractTwoCompField(dataBytes,GAL_TYPE6_A1_FIRST_BYTE_IDX,GAL_TYPE6_A1_LAST_BYTE_IDX,GAL_TYPE6_A1_BITS_OF_FIRST_BYTE,GAL_TYPE6_A1_BITS_OF_LAST_BYTE,GAL_TYPE6_A1_BITS_FIELD_DIM);
	gnss2Utc->dtLs = ExtractTwoCompField(dataBytes,GAL_TYPE6_DTLS_FIRST_BYTE_IDX,GAL_TYPE6_DTLS_LAST_BYTE_IDX,GAL_TYPE6_DTLS_BITS_OF_FIRST_BYTE,GAL_TYPE6_DTLS_BITS_OF_LAST_BYTE,GAL_TYPE6_DTLS_BITS_FIELD_DIM);
	gnss2Utc->t0T = GAL_TYPE6_TOT_SCALE_FACTOR*ExtractField(dataBytes,GAL_TYPE6_TOT_FIRST_BYTE_IDX,GAL_TYPE6_TOT_LAST_BYTE_IDX,GAL_TYPE6_TOT_BITS_OF_FIRST_BYTE,GAL_TYPE6_TOT_BITS_OF_LAST_BYTE);
	gnss2Utc->wn0T = ExtractField(dataBytes,GAL_TYPE6_WNT_FIRST_BYTE_IDX,GAL_TYPE6_WNT_LAST_BYTE_IDX,GAL_TYPE6_WNT_BITS_OF_FIRST_BYTE,GAL_TYPE6_WNT_BITS_OF_LAST_BYTE);
	gnss2Utc->wnLsf = ExtractField(dataBytes,GAL_TYPE6_WNLSF_FIRST_BYTE_IDX,GAL_TYPE6_WNLSF_LAST_BYTE_IDX,GAL_TYPE6_WNLSF_BITS_OF_FIRST_BYTE,GAL_TYPE6_WNLSF_BITS_OF_LAST_BYTE);
	gnss2Utc->dn = ExtractField(dataBytes,GAL_TYPE6_DN_FIRST_BYTE_IDX,GAL_TYPE6_DN_LAST_BYTE_IDX,GAL_TYPE6_DN_BITS_OF_FIRST_BYTE,GAL_TYPE6_DN_BITS_OF_LAST_BYTE);
	gnss2Utc->dtLsf = ExtractTwoCompField(dataBytes,GAL_TYPE6_DTLSF_FIRST_BYTE_IDX,GAL_TYPE6_DTLSF_LAST_BYTE_IDX,GAL_TYPE6_DTLSF_BITS_OF_FIRST_BYTE,GAL_TYPE6_DTLSF_BITS_OF_LAST_BYTE,GAL_TYPE6_DTLSF_BITS_FIELD_DIM);
	/* WN is not available, so problems with the week change can occur 
	   Try to avoid them updating also WN if possible 
	   if tow<navMsg->tow means that the time went back, so a week change occurred */
	tow = ExtractField(dataBytes,GAL_TYPE6_TOW_FIRST_BYTE_IDX,GAL_TYPE6_TOW_LAST_BYTE_IDX,GAL_TYPE6_TOW_BITS_OF_FIRST_BYTE,GAL_TYPE6_TOW_BITS_OF_LAST_BYTE);
	if((navMsg->towValid==TRUE_FLAG) && (navMsg->wnValid==TRUE_FLAG) && (tow<navMsg->tow))
	{
		navMsg->wn++;
	}
	navMsg->tow = tow;
	navMsg->towValid = TRUE_FLAG;
	parser->bitIndexTotal = GAL_PAGE_LENGTH;

	navMsg->gnss2UtcReceived = TRUE_FLAG;

	// if possible, adjust WN taking into account truncation (8 bits!)
	if(navMsg->wnValid==TRUE_FLAG)
	{
		gnss2Utc->wn0T = (navMsg->wn & 0xffffff00) | (gnss2Utc->wn0T & 0x000000ff);
		gnss2Utc->wnLsf = (navMsg->wn & 0xffffff00) | (gnss2Utc->wnLsf & 0x000000ff);
		navMsg->gnss2UtcCompleted = TRUE_FLAG;
	}

	if(parser->galParseLogFile!=NULL)
	{
		fprintf(parser->galParseLogFile,"WORD 6: GST-UTC CONVERSION PARAMETERS\n");
		fprintf(parser->galParseLogFile,"    A0: %.20g\n", gnss2Utc->a0);
		fprintf(parser->galParseLogFile,"    A1: %.20g\n", gnss2Utc->a1);
		fprintf(parser->galParseLogFile,"    dt_LS: %d\n", gnss2Utc->dtLs);
		fprintf(parser->galParseLogFile,"    t_ot: %d\n", gnss2Utc->t0T);
		if(navMsg->gnss2UtcCompleted==TRUE_FLAG)
		{
			fprintf(parser->galParseLogFile,"    WN_ot: %d\n", gnss2Utc->wn0T);
			fprintf(parser->galParseLogFile,"    WN_LSF: %d\n", gnss2Utc->wnLsf);
		}
		else
		{
			fprintf(parser->galParseLogFile,"    WN_ot: %d (truncated)\n", gnss2Utc->wn0T);
			fprintf(parser->galParseLogFile,"    WN_LSF: %d (truncated)\n", gnss2Utc->wnLsf);
		}
		fprintf(parser->galParseLogFile,"    DN: %d\n", gnss2Utc->dn);
		fprintf(parser->galParseLogFile,"    dt_LSF: %d\n", gnss2Utc->dtLsf);
		fprintf(parser->galParseLogFile,"    TOW: %d\n", navMsg->tow);
		if(navMsg->wnValid==TRUE_FLAG)
			fprintf(parser->galParseLogFile,"    (WN: %d)\n", navMsg->wn);
		fprintf(parser->galParseLogFile, "\n");
	}
	
	return SUCCESS_RETURN;
}


int ParseGalWord07(TNGeneData *rx,TGalParser *parser,int tow)
{
	unsigned char *dataBytes = parser->bitsForParse;
	int svIdCur; /* svId whose almanac starts from the current word */
	TGnssAlmanac *almCur;
	int ioda;
	int wna;
	double toa;

	ioda = ExtractField(dataBytes,GAL_TYPE7_IODA_FIRST_BYTE_IDX,GAL_TYPE7_IODA_LAST_BYTE_IDX,GAL_TYPE7_IODA_BITS_OF_FIRST_BYTE,GAL_TYPE7_IODA_BITS_OF_LAST_BYTE);
	wna = ExtractField(dataBytes,GAL_TYPE7_WNA_FIRST_BYTE_IDX,GAL_TYPE7_WNA_LAST_BYTE_IDX,GAL_TYPE7_WNA_BITS_OF_FIRST_BYTE,GAL_TYPE7_WNA_BITS_OF_LAST_BYTE);
	toa = GAL_TOA_SCALE_FACTOR*ExtractField(dataBytes,GAL_TYPE7_T0A_FIRST_BYTE_IDX,GAL_TYPE7_T0A_LAST_BYTE_IDX,GAL_TYPE7_T0A_BITS_OF_FIRST_BYTE,GAL_TYPE7_T0A_BITS_OF_LAST_BYTE);

	if(parser->galParseLogFile!=NULL)
	{
		fprintf(parser->galParseLogFile,"WORD 7:\n");
		fprintf(parser->galParseLogFile,"    almIODa %d\n", ioda);
		fprintf(parser->galParseLogFile,"    almWNa %d\n", wna);
		fprintf(parser->galParseLogFile,"    almToa %.20g\n", toa);
	}
	
	/* parse the almanac */
	svIdCur = ExtractField(dataBytes,GAL_TYPE7_SVID1_FIRST_BYTE_IDX,GAL_TYPE7_SVID1_LAST_BYTE_IDX,GAL_TYPE7_SVID1_BITS_OF_FIRST_BYTE,GAL_TYPE7_SVID1_BITS_OF_LAST_BYTE);
	if((svIdCur<BASE_PRN) || (svIdCur>GALILEO_NUMBER_OF_PRNS))
	{
		if((parser->galParseLogFile!=NULL) && (svIdCur==GAL_SVID_FOR_DUMMY_ALMANAC))
		{
			fprintf(parser->galParseLogFile,"WORD 7: DUMMY ALMANAC (1/2)\n");
			fprintf(parser->galParseLogFile,"\n");
		}
		
		parser->almSvIdPrev = GAL_SVID_FOR_DUMMY_ALMANAC;
		parser->almSvIdPrevTime = INVALID_VALUE;
		return SUCCESS_RETURN;
	}

	almCur = &(rx->galNavigationMessage[svIdCur-BASE_PRN].almanac);
	almCur->ioda = ioda;
	almCur->wna = wna;
	almCur->toa = toa;
	almCur->sqrtA = POW_TWO_M09*ExtractTwoCompField(dataBytes,GAL_TYPE7_SQRTA_FIRST_BYTE_IDX,GAL_TYPE7_SQRTA_LAST_BYTE_IDX,GAL_TYPE7_SQRTA_BITS_OF_FIRST_BYTE,GAL_TYPE7_SQRTA_BITS_OF_LAST_BYTE,GAL_TYPE7_SQRTA_FIELD_DIM);
	almCur->sqrtA += GAL_NOMINAL_SQRTA;
	almCur->eccentricity = POW_TWO_M16*ExtractField(dataBytes,GAL_TYPE7_E_FIRST_BYTE_IDX,GAL_TYPE7_E_LAST_BYTE_IDX,GAL_TYPE7_E_BITS_OF_FIRST_BYTE,GAL_TYPE7_E_BITS_OF_LAST_BYTE);
	almCur->omega = POW_TWO_M15*M_PI*ExtractTwoCompField(dataBytes,GAL_TYPE7_OMEGA_FIRST_BYTE_IDX,GAL_TYPE7_OMEGA_LAST_BYTE_IDX,GAL_TYPE7_OMEGA_BITS_OF_FIRST_BYTE,GAL_TYPE7_OMEGA_BITS_OF_LAST_BYTE,GAL_TYPE7_OMEGA_FIELD_DIM);
	almCur->i0 = POW_TWO_M14*M_PI*ExtractTwoCompField(dataBytes,GAL_TYPE7_I0_FIRST_BYTE_IDX,GAL_TYPE7_I0_LAST_BYTE_IDX,GAL_TYPE7_I0_BITS_OF_FIRST_BYTE,GAL_TYPE7_I0_BITS_OF_LAST_BYTE,GAL_TYPE7_I0_FIELD_DIM);
	almCur->i0 += GAL_NOMINAL_I0;
	almCur->capitalOmega0 = POW_TWO_M15*M_PI*ExtractTwoCompField(dataBytes,GAL_TYPE7_OMEGA0_FIRST_BYTE_IDX,GAL_TYPE7_OMEGA0_LAST_BYTE_IDX,GAL_TYPE7_OMEGA0_BITS_OF_FIRST_BYTE,GAL_TYPE7_OMEGA0_BITS_OF_LAST_BYTE,GAL_TYPE7_OMEGA0_FIELD_DIM);
	almCur->capitalOmegaDot = POW_TWO_M33*M_PI*ExtractTwoCompField(dataBytes,GAL_TYPE7_OMEGADOT_FIRST_BYTE_IDX,GAL_TYPE7_OMEGADOT_LAST_BYTE_IDX,GAL_TYPE7_OMEGADOT_BITS_OF_FIRST_BYTE,GAL_TYPE7_OMEGADOT_BITS_OF_LAST_BYTE,GAL_TYPE7_OMEGADOT_FIELD_DIM);
	almCur->meanAnomaly0 = POW_TWO_M15*M_PI*ExtractTwoCompField(dataBytes,GAL_TYPE7_M0_FIRST_BYTE_IDX,GAL_TYPE7_M0_LAST_BYTE_IDX,GAL_TYPE7_M0_BITS_OF_FIRST_BYTE,GAL_TYPE7_M0_BITS_OF_LAST_BYTE,GAL_TYPE7_M0_FIELD_DIM);

	rx->galNavigationMessage[svIdCur-BASE_PRN].almanac1Received=TRUE_FLAG;

	parser->almSvIdPrev = svIdCur;
	parser->almSvIdPrevTime = tow;

	if(parser->galParseLogFile!=NULL)
	{
		fprintf(parser->galParseLogFile,"WORD 7: ALMANAC FOR SVID %2d (1/2)\n",svIdCur);
		fprintf(parser->galParseLogFile,"    almAPowerHalf %.20g\n", almCur->sqrtA);
		fprintf(parser->galParseLogFile,"    almE %.20g\n", almCur->eccentricity);
		fprintf(parser->galParseLogFile,"    almOmega %.20g\n", almCur->omega);
		fprintf(parser->galParseLogFile,"    almI0 %.20g\n", almCur->i0);
		fprintf(parser->galParseLogFile,"    almOmega0 %.20g\n", almCur->capitalOmega0);
		fprintf(parser->galParseLogFile,"    almOmegaDot %.20g\n", almCur->capitalOmegaDot);
		fprintf(parser->galParseLogFile,"    almM0 %.20g\n", almCur->meanAnomaly0);
		fprintf(parser->galParseLogFile,"\n");
	}
	
	return SUCCESS_RETURN;
}


int ParseGalWord08(TNGeneData *rx,TGalParser *parser,int tow)
{
	unsigned char *dataBytes = parser->bitsForParse;
	int svIdPrev; /* svId whose almanac started in the previous word (and ends in the current) */
	int svIdCur; /* svId whose almanac starts from the current word */
	TGnssAlmanac *almPrev;
	TGnssAlmanac *almCur;
	int ioda;
	
	ioda = ExtractField(dataBytes,GAL_TYPE8_IODA_FIRST_BYTE_IDX,GAL_TYPE8_IODA_LAST_BYTE_IDX,GAL_TYPE8_IODA_BITS_OF_FIRST_BYTE,GAL_TYPE8_IODA_BITS_OF_LAST_BYTE);
	if(parser->galParseLogFile!=NULL)
	{
		fprintf(parser->galParseLogFile,"WORD 8:\n");
		fprintf(parser->galParseLogFile,"    almIODa %d\n", ioda);
	}
	
	/* parse the first part of the word */
	/* check reception of word 7 */
	if((parser->almSvIdPrev!=INVALID_PRN) && (parser->almSvIdPrev!=GAL_SVID_FOR_DUMMY_ALMANAC) && (parser->almSvIdPrevTime==tow-2))
	{
		svIdPrev = parser->almSvIdPrev;
		almPrev = &(rx->galNavigationMessage[svIdPrev-BASE_PRN].almanac);
		almPrev->ioda = ioda;
		almPrev->af[0] = POW_TWO_M19*ExtractTwoCompField(dataBytes,GAL_TYPE8_AF0_FIRST_BYTE_IDX,GAL_TYPE8_AF0_LAST_BYTE_IDX,GAL_TYPE8_AF0_BITS_OF_FIRST_BYTE,GAL_TYPE8_AF0_BITS_OF_LAST_BYTE,GAL_TYPE8_AF0_FIELD_DIM);
		almPrev->af[1] = POW_TWO_M38*ExtractTwoCompField(dataBytes,GAL_TYPE8_AF1_FIRST_BYTE_IDX,GAL_TYPE8_AF1_LAST_BYTE_IDX,GAL_TYPE8_AF1_BITS_OF_FIRST_BYTE,GAL_TYPE8_AF1_BITS_OF_LAST_BYTE,GAL_TYPE8_AF1_FIELD_DIM);
		almPrev->health = ExtractField(dataBytes,GAL_TYPE8_HEALTH_FIRST_BYTE_IDX,GAL_TYPE8_HEALTH_LAST_BYTE_IDX,GAL_TYPE8_HEALTH_BITS_OF_FIRST_BYTE,GAL_TYPE8_HEALTH_BITS_OF_LAST_BYTE);

		rx->galNavigationMessage[svIdPrev-BASE_PRN].almanac2Received=TRUE_FLAG;

		if(parser->galParseLogFile!=NULL)
		{
			fprintf(parser->galParseLogFile,"WORD 8: ALMANAC FOR SVID %2d (2/2)\n",svIdPrev);
			fprintf(parser->galParseLogFile,"    almAF0 %.20g\n", almPrev->af[0]);
			fprintf(parser->galParseLogFile,"    almAF1 %.20g\n", almPrev->af[1]);
			fprintf(parser->galParseLogFile,"    almHealth %d\n", almPrev->health);
		}
	}
	else if((parser->almSvIdPrev==GAL_SVID_FOR_DUMMY_ALMANAC) && (parser->almSvIdPrevTime==tow-2))
	{
		if(parser->galParseLogFile!=NULL)
		{
			fprintf(parser->galParseLogFile,"WORD 8: DUMMY ALMANAC (2/2)\n");
		}
	}

	/* parse the second part of the word */
	svIdCur = ExtractField(dataBytes,GAL_TYPE8_SVID2_FIRST_BYTE_IDX,GAL_TYPE8_SVID2_LAST_BYTE_IDX,GAL_TYPE8_SVID2_BITS_OF_FIRST_BYTE,GAL_TYPE8_SVID2_BITS_OF_LAST_BYTE);
	if((svIdCur<BASE_PRN) || (svIdCur>GALILEO_NUMBER_OF_PRNS))
	{
		if((parser->galParseLogFile!=NULL) && (svIdCur==GAL_SVID_FOR_DUMMY_ALMANAC))
		{
			fprintf(parser->galParseLogFile,"WORD 8: DUMMY ALMANAC (1/2)\n");
			fprintf(parser->galParseLogFile,"\n");
		}
		parser->almSvIdPrev = GAL_SVID_FOR_DUMMY_ALMANAC;
		parser->almSvIdPrevTime = INVALID_VALUE;
		return SUCCESS_RETURN;
	}

	almCur = &(rx->galNavigationMessage[svIdCur-BASE_PRN].almanac);
	almCur->ioda = ioda;
	almCur->sqrtA = POW_TWO_M09*ExtractTwoCompField(dataBytes,GAL_TYPE8_SQRTA_FIRST_BYTE_IDX,GAL_TYPE8_SQRTA_LAST_BYTE_IDX,GAL_TYPE8_SQRTA_BITS_OF_FIRST_BYTE,GAL_TYPE8_SQRTA_BITS_OF_LAST_BYTE,GAL_TYPE8_SQRTA_FIELD_DIM);
	almCur->sqrtA += GAL_NOMINAL_SQRTA;
	almCur->eccentricity = POW_TWO_M16*ExtractField(dataBytes,GAL_TYPE8_E_FIRST_BYTE_IDX,GAL_TYPE8_E_LAST_BYTE_IDX,GAL_TYPE8_E_BITS_OF_FIRST_BYTE,GAL_TYPE8_E_BITS_OF_LAST_BYTE);
	almCur->omega =  POW_TWO_M15*M_PI*ExtractTwoCompField(dataBytes,GAL_TYPE8_OMEGA_FIRST_BYTE_IDX,GAL_TYPE8_OMEGA_LAST_BYTE_IDX,GAL_TYPE8_OMEGA_BITS_OF_FIRST_BYTE,GAL_TYPE8_OMEGA_BITS_OF_LAST_BYTE,GAL_TYPE8_OMEGA_FIELD_DIM);
	almCur->i0 = POW_TWO_M14*M_PI*ExtractTwoCompField(dataBytes,GAL_TYPE8_I0_FIRST_BYTE_IDX,GAL_TYPE8_I0_LAST_BYTE_IDX,GAL_TYPE8_I0_BITS_OF_FIRST_BYTE,GAL_TYPE8_I0_BITS_OF_LAST_BYTE,GAL_TYPE8_I0_FIELD_DIM);
	almCur->i0 += GAL_NOMINAL_I0;
	almCur->capitalOmega0 = POW_TWO_M15*M_PI*ExtractTwoCompField(dataBytes,GAL_TYPE8_OMEGA0_FIRST_BYTE_IDX,GAL_TYPE8_OMEGA0_LAST_BYTE_IDX,GAL_TYPE8_OMEGA0_BITS_OF_FIRST_BYTE,GAL_TYPE8_OMEGA0_BITS_OF_LAST_BYTE,GAL_TYPE8_OMEGA0_FIELD_DIM);
	almCur->capitalOmegaDot = POW_TWO_M33*M_PI*ExtractTwoCompField(dataBytes,GAL_TYPE8_OMEGADOT_FIRST_BYTE_IDX,GAL_TYPE8_OMEGADOT_LAST_BYTE_IDX,GAL_TYPE8_OMEGADOT_BITS_OF_FIRST_BYTE,GAL_TYPE8_OMEGADOT_BITS_OF_LAST_BYTE,GAL_TYPE8_OMEGADOT_FIELD_DIM);

	rx->galNavigationMessage[svIdCur-BASE_PRN].almanac1Received=TRUE_FLAG;

	parser->almSvIdPrev = svIdCur;
	parser->almSvIdPrevTime = tow;

	if(parser->galParseLogFile!=NULL)
	{
		fprintf(parser->galParseLogFile,"WORD 8: ALMANAC FOR SVID %2d (1/2)\n",svIdCur);
		fprintf(parser->galParseLogFile,"    almAPowerHalf %.20g\n", almCur->sqrtA);
		fprintf(parser->galParseLogFile,"    almE %.20g\n", almCur->eccentricity);
		fprintf(parser->galParseLogFile,"    almOmega %.20g\n", almCur->omega);
		fprintf(parser->galParseLogFile,"    almI0 %.20g\n", almCur->i0);
		fprintf(parser->galParseLogFile,"    almOmega0 %.20g\n", almCur->capitalOmega0);
		fprintf(parser->galParseLogFile,"    almOmegaDot %.20g\n", almCur->capitalOmegaDot);
		fprintf(parser->galParseLogFile,"\n");
	}
	return SUCCESS_RETURN;
}


int ParseGalWord09(TNGeneData *rx,TGalParser *parser,int tow)
{
	unsigned char *dataBytes = parser->bitsForParse;
	int svIdPrev; /* svId whose almanac started in the previous word (and ends in the current) */
	int svIdCur; /* svId whose almanac starts from the current word */
	TGnssAlmanac *almPrev;
	TGnssAlmanac *almCur;
	int ioda;
	int wna;
	double toa;

	ioda = ExtractField(dataBytes,GAL_TYPE9_IODA_FIRST_BYTE_IDX,GAL_TYPE9_IODA_LAST_BYTE_IDX,GAL_TYPE9_IODA_BITS_OF_FIRST_BYTE,GAL_TYPE9_IODA_BITS_OF_LAST_BYTE);
	wna = ExtractField(dataBytes,GAL_TYPE9_WNA_FIRST_BYTE_IDX,GAL_TYPE9_WNA_LAST_BYTE_IDX,GAL_TYPE9_WNDA_BITS_OF_FIRST_BYTE,GAL_TYPE9_WNDA_BITS_OF_LAST_BYTE);
	toa = GAL_TOA_SCALE_FACTOR*ExtractField(dataBytes,GAL_TYPE9_T0A_FIRST_BYTE_IDX,GAL_TYPE9_T0A_LAST_BYTE_IDX,GAL_TYPE9_T0A_BITS_OF_FIRST_BYTE,GAL_TYPE9_T0A_BITS_OF_LAST_BYTE);
	if(parser->galParseLogFile!=NULL)
	{
		fprintf(parser->galParseLogFile,"WORD 9:\n");
		fprintf(parser->galParseLogFile,"    almIODa %d\n", ioda);
		fprintf(parser->galParseLogFile,"    almWNa %d\n", wna);
		fprintf(parser->galParseLogFile,"    almToa %.20g\n", toa);
	}

	/* parse the first part of the word */
	/* check reception of word 8 */
	if((parser->almSvIdPrev!=INVALID_PRN) && (parser->almSvIdPrev!=GAL_SVID_FOR_DUMMY_ALMANAC) && (parser->almSvIdPrevTime==tow-28))
	{
		svIdPrev = parser->almSvIdPrev;
		almPrev = &(rx->galNavigationMessage[svIdPrev-BASE_PRN].almanac);
		almPrev->ioda = ioda;
		almPrev->wna = wna;
		almPrev->toa = toa;
		almPrev->meanAnomaly0 = POW_TWO_M15*M_PI*ExtractTwoCompField(dataBytes,GAL_TYPE9_M0_FIRST_BYTE_IDX,GAL_TYPE9_M0_LAST_BYTE_IDX,GAL_TYPE9_M0_BITS_OF_FIRST_BYTE,GAL_TYPE9_M0_BITS_OF_LAST_BYTE,GAL_TYPE9_M0_FIELD_DIM);
		almPrev->af[0] = POW_TWO_M19*ExtractTwoCompField(dataBytes,GAL_TYPE9_AF0_FIRST_BYTE_IDX,GAL_TYPE9_AF0_LAST_BYTE_IDX,GAL_TYPE9_AF0_BITS_OF_FIRST_BYTE,GAL_TYPE9_AF0_BITS_OF_LAST_BYTE,GAL_TYPE9_AF0_FIELD_DIM);
		almPrev->af[1] = POW_TWO_M38*ExtractTwoCompField(dataBytes,GAL_TYPE9_AF1_FIRST_BYTE_IDX,GAL_TYPE9_AF1_LAST_BYTE_IDX,GAL_TYPE9_AF1_BITS_OF_FIRST_BYTE,GAL_TYPE9_AF1_BITS_OF_LAST_BYTE,GAL_TYPE9_AF1_FIELD_DIM);
		almPrev->health = ExtractField(dataBytes,GAL_TYPE9_HEALTH_FIRST_BYTE_IDX,GAL_TYPE9_HEALTH_LAST_BYTE_IDX,GAL_TYPE9_HEALTH_BITS_OF_FIRST_BYTE,GAL_TYPE9_HEALTH_BITS_OF_LAST_BYTE);

		rx->galNavigationMessage[svIdPrev-BASE_PRN].almanac2Received=TRUE_FLAG;

		if(parser->galParseLogFile!=NULL)
		{
			fprintf(parser->galParseLogFile,"WORD 9: ALMANAC FOR SVID %2d (2/2)\n",svIdPrev);
			fprintf(parser->galParseLogFile,"    almM0 %.20g\n", almPrev->meanAnomaly0);
			fprintf(parser->galParseLogFile,"    almAF0 %.20g\n", almPrev->af[0]);
			fprintf(parser->galParseLogFile,"    almAF1 %.20g\n", almPrev->af[1]);
			fprintf(parser->galParseLogFile,"    almHealth %d\n", almPrev->health);
		}
	}
	else if((parser->almSvIdPrev==GAL_SVID_FOR_DUMMY_ALMANAC) && (parser->almSvIdPrevTime==tow-28))
	{
		if(parser->galParseLogFile!=NULL)
		{
			fprintf(parser->galParseLogFile,"WORD 9: DUMMY ALMANAC (2/2)\n");
			fprintf(parser->galParseLogFile,"\n");
		}
	}

	/* parse the second part of the word */
	svIdCur = ExtractField(dataBytes,GAL_TYPE9_SVID3_FIRST_BYTE_IDX,GAL_TYPE9_SVID3_LAST_BYTE_IDX,GAL_TYPE9_SVID3_BITS_OF_FIRST_BYTE,GAL_TYPE9_SVID3_BITS_OF_LAST_BYTE);

	if((svIdCur<BASE_PRN) || (svIdCur>GALILEO_NUMBER_OF_PRNS))
	{
		if((parser->galParseLogFile!=NULL) && (svIdCur==GAL_SVID_FOR_DUMMY_ALMANAC))
		{
			fprintf(parser->galParseLogFile,"WORD 9: DUMMY ALMANAC (1/2)\n");
		}
		parser->almSvIdPrev = GAL_SVID_FOR_DUMMY_ALMANAC;
		parser->almSvIdPrevTime = INVALID_VALUE;
		return SUCCESS_RETURN;
	}

	almCur = &(rx->galNavigationMessage[svIdCur-BASE_PRN].almanac);
	almCur->ioda = ioda;
	almCur->wna = wna;
	almCur->toa = toa;
	almCur->sqrtA = POW_TWO_M09*ExtractTwoCompField(dataBytes,GAL_TYPE9_SQRTA_FIRST_BYTE_IDX,GAL_TYPE9_SQRTA_LAST_BYTE_IDX,GAL_TYPE9_SQRTA_BITS_OF_FIRST_BYTE,GAL_TYPE9_SQRTA_BITS_OF_LAST_BYTE,GAL_TYPE9_SQRTA_FIELD_DIM);
	almCur->sqrtA += GAL_NOMINAL_SQRTA;
	almCur->eccentricity = POW_TWO_M16*ExtractField(dataBytes,GAL_TYPE9_E_FIRST_BYTE_IDX,GAL_TYPE9_E_LAST_BYTE_IDX,GAL_TYPE9_E_BITS_OF_FIRST_BYTE,GAL_TYPE9_E_BITS_OF_LAST_BYTE);
	almCur->omega = POW_TWO_M15*M_PI*ExtractTwoCompField(dataBytes,GAL_TYPE9_OMEGA_FIRST_BYTE_IDX,GAL_TYPE9_OMEGA_LAST_BYTE_IDX,GAL_TYPE9_OMEGA_BITS_OF_FIRST_BYTE,GAL_TYPE9_OMEGA_BITS_OF_LAST_BYTE,GAL_TYPE9_OMEGA_FIELD_DIM);
	almCur->i0 = POW_TWO_M14*M_PI*ExtractTwoCompField(dataBytes,GAL_TYPE9_I0_FIRST_BYTE_IDX,GAL_TYPE9_I0_LAST_BYTE_IDX,GAL_TYPE9_I0_BITS_OF_FIRST_BYTE,GAL_TYPE9_I0_BITS_OF_LAST_BYTE,GAL_TYPE9_I0_FIELD_DIM);
	almCur->i0 += GAL_NOMINAL_I0;

	rx->galNavigationMessage[svIdCur-BASE_PRN].almanac1Received=TRUE_FLAG;

	parser->almSvIdPrev = svIdCur;
	parser->almSvIdPrevTime = tow;

	if(parser->galParseLogFile!=NULL)
	{
		fprintf(parser->galParseLogFile,"WORD 9: ALMANAC FOR SVID %2d (1/2)\n",svIdCur);
		fprintf(parser->galParseLogFile,"    almAPowerHalf %.20g\n", almCur->sqrtA);
		fprintf(parser->galParseLogFile,"    almE %.20g\n", almCur->eccentricity);
		fprintf(parser->galParseLogFile,"    almOmega %.20g\n", almCur->omega);
		fprintf(parser->galParseLogFile,"    almI0 %.20g\n", almCur->i0);
		fprintf(parser->galParseLogFile,"\n");
	}

	return SUCCESS_RETURN;
}


int ParseGalWord10(TNGeneData *rx,TGalParser *parser,int tow)
{
	unsigned char *dataBytes = parser->bitsForParse;
	int svIdPrev; /* svId whose almanac started in the previous word (and ends in the current) */
	TGnssAlmanac *almPrev;
	TNavigationMessage *navMsg = parser->navMsg;
	TGalileoToGpsTimeConversion	*gal2Gps = &(navMsg->gal2Gps);
	int ioda;
	int checkGgtoValidity[4];

	ioda = ExtractField(dataBytes,GAL_TYPE10_IODA_FIRST_BYTE_IDX,GAL_TYPE10_IODA_LAST_BYTE_IDX,GAL_TYPE10_IODA_BITS_OF_FIRST_BYTE,GAL_TYPE10_IODA_BITS_OF_LAST_BYTE);
	if(parser->galParseLogFile!=NULL)
	{
		fprintf(parser->galParseLogFile,"WORD 10:\n");
		fprintf(parser->galParseLogFile,"    almIODa %d\n", ioda);
	}

	/* parse the almanac */
	/* check reception of word 9 */
	if((parser->almSvIdPrev!=INVALID_PRN) && (parser->almSvIdPrev!=GAL_SVID_FOR_DUMMY_ALMANAC) && (parser->almSvIdPrevTime==tow-2))
	{
		svIdPrev = parser->almSvIdPrev;

		almPrev = &(rx->galNavigationMessage[svIdPrev-BASE_PRN].almanac);
		almPrev->ioda = ioda;
		almPrev->capitalOmega0 = POW_TWO_M15*M_PI*ExtractTwoCompField(dataBytes,GAL_TYPE10_OMEGA0_FIRST_BYTE_IDX,GAL_TYPE10_OMEGA0_LAST_BYTE_IDX,GAL_TYPE10_OMEGA0_BITS_OF_FIRST_BYTE,GAL_TYPE10_OMEGA0_BITS_OF_LAST_BYTE,GAL_TYPE10_OMEGA0_FIELD_DIM);
		almPrev->capitalOmegaDot = POW_TWO_M33*M_PI*ExtractTwoCompField(dataBytes,GAL_TYPE10_OMEGADOT_FIRST_BYTE_IDX,GAL_TYPE10_OMEGADOT_LAST_BYTE_IDX,GAL_TYPE10_OMEGADOT_BITS_OF_FIRST_BYTE,GAL_TYPE10_OMEGADOT_BITS_OF_LAST_BYTE,GAL_TYPE10_OMEGADOT_FIELD_DIM);
		almPrev->meanAnomaly0 = POW_TWO_M15*M_PI*ExtractTwoCompField(dataBytes,GAL_TYPE10_M0_FIRST_BYTE_IDX,GAL_TYPE10_M0_LAST_BYTE_IDX,GAL_TYPE10_M0_BITS_OF_FIRST_BYTE,GAL_TYPE10_M0_BITS_OF_LAST_BYTE,GAL_TYPE10_M0_FIELD_DIM);
		almPrev->af[0] = POW_TWO_M19*ExtractTwoCompField(dataBytes,GAL_TYPE10_AF0_FIRST_BYTE_IDX,GAL_TYPE10_AF0_LAST_BYTE_IDX,GAL_TYPE10_AF0_BITS_OF_FIRST_BYTE,GAL_TYPE10_AF0_BITS_OF_LAST_BYTE,GAL_TYPE10_AF0_FIELD_DIM);
		almPrev->af[1] = POW_TWO_M38*ExtractTwoCompField(dataBytes,GAL_TYPE10_AF1_FIRST_BYTE_IDX,GAL_TYPE10_AF1_LAST_BYTE_IDX,GAL_TYPE10_AF1_BITS_OF_FIRST_BYTE,GAL_TYPE10_AF1_BITS_OF_LAST_BYTE,GAL_TYPE10_AF1_FIELD_DIM);
		almPrev->health = ExtractField(dataBytes,GAL_TYPE10_HEALTH_FIRST_BYTE_IDX,GAL_TYPE10_HEALTH_LAST_BYTE_IDX,GAL_TYPE10_HEALTH_BITS_OF_FIRST_BYTE,GAL_TYPE10_HEALTH_BITS_OF_LAST_BYTE);

		rx->galNavigationMessage[svIdPrev-BASE_PRN].almanac2Received=TRUE_FLAG;

		if(parser->galParseLogFile!=NULL)
		{
			fprintf(parser->galParseLogFile,"WORD 10: ALMANAC FOR SVID %2d (2/2)\n",svIdPrev);
			fprintf(parser->galParseLogFile,"    almOmega0 %.20g\n", almPrev->capitalOmega0);
			fprintf(parser->galParseLogFile,"    almOmegaDot %.20g\n", almPrev->capitalOmegaDot);
			fprintf(parser->galParseLogFile,"    almM0 %.20g\n", almPrev->meanAnomaly0);
			fprintf(parser->galParseLogFile,"    almAF0 %.20g\n", almPrev->af[0]);
			fprintf(parser->galParseLogFile,"    almAF1 %.20g\n", almPrev->af[1]);
			fprintf(parser->galParseLogFile,"    almHealth %d\n", almPrev->health);
		}
	}
	else if((parser->almSvIdPrev==GAL_SVID_FOR_DUMMY_ALMANAC) && (parser->almSvIdPrevTime==tow-2))
	{
		if(parser->galParseLogFile!=NULL)
		{
			fprintf(parser->galParseLogFile,"WORD 10: DUMMY ALMANAC (2/2)\n");
		}
	}

	parser->almSvIdPrev = INVALID_PRN;
	parser->almSvIdPrevTime = INVALID_VALUE;

	/* parse GST-GPS */
	gal2Gps->a0G = POW_TWO_M35*(ExtractTwoCompField(dataBytes,GAL_TYPE10_A0G_FIRST_BYTE_IDX,GAL_TYPE10_A0G_LAST_BYTE_IDX,GAL_TYPE10_A0G_BITS_OF_FIRST_BYTE,GAL_TYPE10_A0G_BITS_OF_LAST_BYTE,GAL_TYPE10_A0G_BITS_FIELD_DIM));
	gal2Gps->a1G = POW_TWO_M51*(ExtractTwoCompField(dataBytes,GAL_TYPE10_A1G_FIRST_BYTE_IDX,GAL_TYPE10_A1G_LAST_BYTE_IDX,GAL_TYPE10_A1G_BITS_OF_FIRST_BYTE,GAL_TYPE10_A1G_BITS_OF_LAST_BYTE,GAL_TYPE10_A1G_BITS_FIELD_DIM));
	gal2Gps->t0G = 3600*(ExtractField(dataBytes,GAL_TYPE10_t0G_FIRST_BYTE_IDX,GAL_TYPE10_t0G_LAST_BYTE_IDX,GAL_TYPE10_t0G_BITS_OF_FIRST_BYTE,GAL_TYPE10_t0G_BITS_OF_LAST_BYTE));
	gal2Gps->wn0G = (ExtractField(dataBytes,GAL_TYPE10_WN0G_FIRST_BYTE_IDX,GAL_TYPE10_WN0G_LAST_BYTE_IDX,GAL_TYPE10_WN0G_BITS_OF_FIRST_BYTE,GAL_TYPE10_WN0G_BITS_OF_LAST_BYTE));
	// check the validity of GGTO parameters (parse them as integer number, no scaling factor)
	checkGgtoValidity[0] = (int)ExtractField(dataBytes,GAL_TYPE10_A0G_FIRST_BYTE_IDX,GAL_TYPE10_A0G_LAST_BYTE_IDX,GAL_TYPE10_A0G_BITS_OF_FIRST_BYTE,GAL_TYPE10_A0G_BITS_OF_LAST_BYTE);
	checkGgtoValidity[1] = (int)ExtractField(dataBytes,GAL_TYPE10_A1G_FIRST_BYTE_IDX,GAL_TYPE10_A1G_LAST_BYTE_IDX,GAL_TYPE10_A1G_BITS_OF_FIRST_BYTE,GAL_TYPE10_A1G_BITS_OF_LAST_BYTE);
	checkGgtoValidity[2] = (int)ExtractField(dataBytes,GAL_TYPE10_t0G_FIRST_BYTE_IDX,GAL_TYPE10_t0G_LAST_BYTE_IDX,GAL_TYPE10_t0G_BITS_OF_FIRST_BYTE,GAL_TYPE10_t0G_BITS_OF_LAST_BYTE);
	checkGgtoValidity[3] = (int)ExtractField(dataBytes,GAL_TYPE10_WN0G_FIRST_BYTE_IDX,GAL_TYPE10_WN0G_LAST_BYTE_IDX,GAL_TYPE10_WN0G_BITS_OF_FIRST_BYTE,GAL_TYPE10_WN0G_BITS_OF_LAST_BYTE);
	if((checkGgtoValidity[0]==0x0ffff) && (checkGgtoValidity[1]==0x00fff) && (checkGgtoValidity[2]==0x000ff) && (checkGgtoValidity[3]==0x0003f))
	{
		// when a user receives all four parameters set to all ones the GGTO is considered not valid
		navMsg->gal2GpsReceived=FALSE_FLAG;
	}
	else
	{
		navMsg->gal2GpsReceived=TRUE_FLAG;
		// if possible, adjust WN taking into account truncation (6 bits!)
		if(navMsg->wnValid==TRUE_FLAG)
		{
			gal2Gps->wn0G = (navMsg->wn & 0xffffffc0) | (gal2Gps->wn0G & 0x0000003f);
			navMsg->gal2GpsCompleted = TRUE_FLAG;
		}
	}

	if((parser->galParseLogFile!=NULL) && (navMsg->gal2GpsReceived==TRUE_FLAG))
	{
		fprintf(parser->galParseLogFile,"WORD 10: GST-GPS CONVERSION PARAMETER\n");
		fprintf(parser->galParseLogFile,"    A_0G: %.20g\n", gal2Gps->a0G);
		fprintf(parser->galParseLogFile,"    A_1G: %.20g\n", gal2Gps->a1G);
		fprintf(parser->galParseLogFile,"    t_0G: %d\n", gal2Gps->t0G);
		if(navMsg->gal2GpsCompleted==TRUE_FLAG)
		{
			fprintf(parser->galParseLogFile,"    WN_0G: %d\n", gal2Gps->wn0G);
		}
		else
		{
			fprintf(parser->galParseLogFile,"    WN_0G: %d (truncated)\n", gal2Gps->wn0G);
		}
		fprintf(parser->galParseLogFile,"\n");
	}

	return SUCCESS_RETURN;
}


int ParseGalWord16(TGalParser *parser)
{
	unsigned char *dataBytes = parser->bitsForParse;
	TNavigationMessage *navMsg = parser->navMsg;
	TGnssRedCed *redCed = &(navMsg->redCed);
	TGnssEphemerides *eph = &(navMsg->eph);
	TGnssClk *clk = &(navMsg->clk);

	redCed->deltaARed = POW_TWO_P08*ExtractTwoCompField(dataBytes,GAL_TYPE16_DA_FIRST_BYTE_IDX,GAL_TYPE16_DA_LAST_BYTE_IDX,GAL_TYPE16_DA_BITS_OF_FIRST_BYTE,GAL_TYPE16_DA_BITS_OF_LAST_BYTE,GAL_TYPE16_DA_BITS_FIELD_DIM);
	redCed->exRed = POW_TWO_M22*ExtractTwoCompField(dataBytes,GAL_TYPE16_EX_FIRST_BYTE_IDX,GAL_TYPE16_EX_LAST_BYTE_IDX,GAL_TYPE16_EX_BITS_OF_FIRST_BYTE,GAL_TYPE16_EX_BITS_OF_LAST_BYTE,GAL_TYPE16_EX_BITS_FIELD_DIM);
	redCed->eyRed = POW_TWO_M22*ExtractTwoCompField(dataBytes,GAL_TYPE16_EY_FIRST_BYTE_IDX,GAL_TYPE16_EY_LAST_BYTE_IDX,GAL_TYPE16_EY_BITS_OF_FIRST_BYTE,GAL_TYPE16_EY_BITS_OF_LAST_BYTE,GAL_TYPE16_EY_BITS_FIELD_DIM);
	redCed->deltaI0Red = POW_TWO_M22*M_PI*ExtractTwoCompField(dataBytes,GAL_TYPE16_I0_FIRST_BYTE_IDX,GAL_TYPE16_I0_LAST_BYTE_IDX,GAL_TYPE16_I0_BITS_OF_FIRST_BYTE,GAL_TYPE16_I0_BITS_OF_LAST_BYTE,GAL_TYPE16_I0_BITS_FIELD_DIM);
	redCed->omega0Red = POW_TWO_M22*M_PI*ExtractTwoCompField(dataBytes,GAL_TYPE16_OMEGA0_FIRST_BYTE_IDX,GAL_TYPE16_OMEGA0_LAST_BYTE_IDX,GAL_TYPE16_OMEGA0_BITS_OF_FIRST_BYTE,GAL_TYPE16_OMEGA0_BITS_OF_LAST_BYTE,GAL_TYPE16_OMEGA0_BITS_FIELD_DIM);
	redCed->lambda0Red = POW_TWO_M22*M_PI*ExtractTwoCompField(dataBytes,GAL_TYPE16_LAMBDA0_FIRST_BYTE_IDX,GAL_TYPE16_LAMBDA0_LAST_BYTE_IDX,GAL_TYPE16_LAMBDA0_BITS_OF_FIRST_BYTE,GAL_TYPE16_LAMBDA0_BITS_OF_LAST_BYTE,GAL_TYPE16_LAMBDA0_BITS_FIELD_DIM);
	redCed->af0Red = POW_TWO_M26*ExtractTwoCompField(dataBytes,GAL_TYPE16_AF0_FIRST_BYTE_IDX,GAL_TYPE16_AF0_LAST_BYTE_IDX,GAL_TYPE16_AF0_BITS_OF_FIRST_BYTE,GAL_TYPE16_AF0_BITS_OF_LAST_BYTE,GAL_TYPE16_AF0_BITS_FIELD_DIM);
	redCed->af1Red = POW_TWO_M35*ExtractTwoCompField(dataBytes,GAL_TYPE16_AF1_FIRST_BYTE_IDX,GAL_TYPE16_AF1_LAST_BYTE_IDX,GAL_TYPE16_AF1_BITS_OF_FIRST_BYTE,GAL_TYPE16_AF1_BITS_OF_LAST_BYTE,GAL_TYPE16_AF1_BITS_FIELD_DIM);

	if(navMsg->towValid==TRUE_FLAG)
	{
		// We are at the end of the page, so bitIndexTotal is multiple of GAL_PAGE_LENGTH.
		// The transmission time at the start of the current page is considered (then: -1 in the count of the pages)
		redCed->tor = navMsg->tow + ((parser->bitIndexTotal/GAL_PAGE_LENGTH)-1)*GAL_PAGE_DURATION;
		// Now get the start time of the subframe
		redCed->tor = ((int)(redCed->tor/GAL_SECONDS_PER_SUBFRAME)*GAL_SECONDS_PER_SUBFRAME) +1;
		while(redCed->tor>=SECONDS_IN_A_WEEK)
		{
			redCed->tor -= SECONDS_IN_A_WEEK;
		}
		navMsg->redCedReceived = TRUE_FLAG;
		/* If CED are not available, get CED from Red CED */
		if(((navMsg->ephemeridesReceived==FALSE_FLAG) || (navMsg->ephFromRedCed==TRUE_FLAG)) &&
		   (parser->redCedInPvt!=RED_CED_DISABLED))
		{
			ResetEphemerides(eph);
			eph->sqrtA = sqrt(redCed->deltaARed + 29600000.0);
			eph->i0 = redCed->deltaI0Red + (56*DEGREE_TO_RADIAN);
			eph->eccentricity = sqrt((redCed->exRed*redCed->exRed) + (redCed->eyRed*redCed->eyRed));
			eph->omega = atan2(redCed->eyRed,redCed->exRed);
			eph->meanAnomaly0 = redCed->lambda0Red - eph->omega;
			eph->capitalOmega0 = redCed->omega0Red;
			eph->toe = redCed->tor;
			ResetClockCorrections(clk);
			clk->af[0] = redCed->af0Red;
			clk->af[1] = redCed->af1Red;
			clk->toc = redCed->tor;
			//
			navMsg->ephemeridesReceived = TRUE_FLAG;
			navMsg->ephFromRedCed = TRUE_FLAG;
		}
	}
	else
	{
		redCed->tor = INVALID_VALUE;
		navMsg->redCedReceived = FALSE_FLAG;
	}

	if(parser->galParseLogFile!=NULL)
	{
		fprintf(parser->galParseLogFile,"WORD 16: Reduced Clock and Ephemeris Data\n");
		fprintf(parser->galParseLogFile,"    t_0r: %d\n", redCed->tor);
		fprintf(parser->galParseLogFile,"    dA_red: %.20g\n", redCed->deltaARed);
		fprintf(parser->galParseLogFile,"    ex_red: %.20g\n", redCed->exRed);
		fprintf(parser->galParseLogFile,"    ey_red: %.20g\n", redCed->eyRed);
		fprintf(parser->galParseLogFile,"    di0_red: %.20g\n", redCed->deltaI0Red);
		fprintf(parser->galParseLogFile,"    omega0_red: %.20g\n", redCed->omega0Red);
		fprintf(parser->galParseLogFile,"    lambda0_red: %.20g\n", redCed->lambda0Red);
		fprintf(parser->galParseLogFile,"    af0_red: %.20g\n", redCed->af0Red);
		fprintf(parser->galParseLogFile,"    af1_red: %.20g\n", redCed->af1Red);
		fprintf(parser->galParseLogFile,"\n");
	}
	return SUCCESS_RETURN;
}


int ParseGalWord17(TGalParser *parser,unsigned int wordType,int crcCheckResult)
{
	/* this function parses Words 17, 18, 19, and 20 */

	unsigned char *dataBytes = parser->bitsForParse;
	int pvIndex = wordType - 17; // index in the parity vector (0,1,2,3)
	int destIndex = pvIndex * 15;
	int rsIod;

	/* check IOD (only if no CRC error occurred) */
	rsIod = dataBytes[1] & 0x03;
	if((rsIod!=parser->rsIod) && (crcCheckResult==TRUE_FLAG))
	{
		// a change of IOD occurred, clean up all the stored RS data
		ResetReedSolomonData(parser);
		parser->rsIod = rsIod;
	}

	/* save the parity vector */
	parser->rsParityVector[destIndex] = ((dataBytes[0] & 0x03)<<6) | ((dataBytes[1] & 0xfc)>>2);
	memcpy(&(parser->rsParityVector[destIndex+1]),&(dataBytes[2]),14);
	parser->rsParityReceived[pvIndex] = TRUE_FLAG;
	parser->rsParityValid[pvIndex] = crcCheckResult;

	if(parser->galParseLogFile!=NULL)
	{
		if(crcCheckResult)
		{
			fprintf(parser->galParseLogFile,"WORD %u: FEC2 Reed-Solomon (%d/4)\n",wordType,pvIndex+1);
			fprintf(parser->galParseLogFile,"    LSB IOD: %d\n", rsIod);
		}
		else
		{
			fprintf(parser->galParseLogFile,"WORD %u: FEC2 Reed-Solomon (%d/4)   with CRC errors\n",wordType,pvIndex+1);
		}
		fprintf(parser->galParseLogFile,"\n");
	}
	return SUCCESS_RETURN;
}


void ManageReedSolomon(TGalParser *parser,unsigned int wordType)
{
	TRsCheckResult rsCheckResult;
	int rsCorrections;
	int wordIdx;
	unsigned int wordTypeRs;

	/* Reed-Solomon check */
	if(((wordType>=1) && (wordType<=4)) || // CED
	   ((wordType>=17) && (wordType<=20))) // RS
	{
		rsCheckResult = CheckReedSolomon(parser,&rsCorrections);
		if(parser->galParseLogFile!=NULL)
		{
			if(rsCheckResult==RS_CHECK_ERRORS)
				fprintf(parser->galParseLogFile,"ERRORS found by Reed-Solomon\n\n");
			else if(rsCheckResult==RS_CHECK_NO_ERRORS)
				fprintf(parser->galParseLogFile,"Reed-Solomon did not find errors\n\n");
			else if(rsCheckResult==RS_ERASURE_RESTORED)
				fprintf(parser->galParseLogFile,"Reed-Solomon restored %d symbols\n\n",rsCorrections);
			else if(rsCheckResult==RS_FAILED_RESTORE)
				fprintf(parser->galParseLogFile,"Reed-Solomon failed to restore symbols\n\n");
		}
		/* use the symbols recovered by the RS decoder (if any) */
		if(rsCheckResult==RS_ERASURE_RESTORED)
		{
			for(wordIdx=0;wordIdx<GAL_RS_NUM_OF_USED_WORDS;wordIdx++)
			{
				wordTypeRs = wordIdx +1;
				if(parser->rsInformationValid[wordIdx]==FALSE_FLAG)
				{
					// word type is inserted from the index of the cycle
					// IODnav is read from the information vector of the word 1 (2 LSB must be moved)
					parser->bitsForParse[0] = ((unsigned char)(wordIdx+1)<<2) | ((parser->rsInformationVector[1] & 0xc0)>>6);
					parser->bitsForParse[1] = ((parser->rsInformationVector[1] & 0x3f)<<2) | (parser->rsInformationVector[0] & 0x03);
					// insert from the restored information vector
					memcpy(&(parser->bitsForParse[2]),&(parser->rsInformationVector[2+(wordIdx*14)]),14);
					// parse
					if(parser->galParseLogFile!=NULL)
					{
						fprintf(parser->galParseLogFile,"Reed-Solomon Recovered Symbols for Word %d\n",wordTypeRs);
					}
					switch(wordTypeRs)
					{
						case 1:
							ParseGalWord01(parser);
							break;
						case 2:
							ParseGalWord02(parser);
							break;
						case 3:
							ParseGalWord03(parser);
							break;
						case 4:
							ParseGalWord04(parser);
							break;
						default:
							// do nothing
							break;
					}
				}
				//
				parser->rsInformationReceived[wordIdx] = TRUE_FLAG;
				parser->rsInformationValid[wordIdx] = TRUE_FLAG;
				parser->rsParityReceived[wordIdx] = TRUE_FLAG;
				parser->rsParityValid[wordIdx] = TRUE_FLAG;
			}
		}
	}
}


int ParseGalSubframeWithCrcErrors(TGalParser *parser)
{
	unsigned int wordType;
	int crcCheckResult = FALSE_FLAG;

	wordType = ExtractField(parser->bitsForParse,GAL_WORDTYPE_FIRST_BYTE_IDX,GAL_WORDTYPE_LAST_BYTE_IDX,GAL_WORDTYPE_BITS_OF_FIRST_BYTE,GAL_WORDTYPE_BITS_OF_LAST_BYTE);

	/* parse each word */
	switch(wordType)
	{
		case 1:
		case 2:
		case 3:
		case 4:
			// if the word is already valid, do nothing
			if(parser->rsInformationValid[wordType-1]==TRUE_FLAG)
				return SUCCESS_RETURN;
			// else continue
			WriteReedSolomonInformationVector(parser,wordType,crcCheckResult);
			if(parser->galParseLogFile!=NULL)
			{
				fprintf(parser->galParseLogFile,"WORD %d   with CRC errors\n",wordType);
				fprintf(parser->galParseLogFile,"\n");
			}
			break;
		case 17:
		case 18:
		case 19:
		case 20:
			// if the word is already valid, do nothing (flag for word 17 is at index 0)
			if(parser->rsParityValid[wordType-17]==TRUE_FLAG)
				return SUCCESS_RETURN;
			// else continue
			ParseGalWord17(parser,wordType,crcCheckResult);
			break;
		default:
			// not useful for Reed-Solomon, just ignore
			return SUCCESS_RETURN;
			break;
	}

	/* Reed-Solomon check */
	ManageReedSolomon(parser,wordType);

	return SUCCESS_RETURN;
}


int ParseGalSubframe(TNGeneConfiguration *cfg,TNGeneData *rx,int chIndex,TGalParser *parser)
{
	unsigned int wordType;
	int parseReturn;
	int ttx;
	int wn;
	int crcCheckResult = TRUE_FLAG;
	TSatConstellation satType;
	double ttxDouble;

	wordType = ExtractField(parser->bitsForParse,GAL_WORDTYPE_FIRST_BYTE_IDX,GAL_WORDTYPE_LAST_BYTE_IDX,GAL_WORDTYPE_BITS_OF_FIRST_BYTE,GAL_WORDTYPE_BITS_OF_LAST_BYTE);

	/* prepare time for almanac parsing */
	if(parser->navMsg->towValid==TRUE_FLAG)
	{
		ttx = parser->navMsg->tow + (parser->bitIndexTotal*GALILEO_BIT_DURATION)/SECONDS_TO_MILLISECONDS;
	}
	else
	{
		ttx = -1;
	}

	/* parse each word */
	switch(wordType)
	{
		case 0:
			/* Spare word */
			parseReturn = ParseGalWord00(parser);
			break;
		case 1:
			parseReturn = ParseGalWord01(parser);
			WriteReedSolomonInformationVector(parser,wordType,crcCheckResult);
			break;
		case 2:
			parseReturn = ParseGalWord02(parser);
			WriteReedSolomonInformationVector(parser,wordType,crcCheckResult);
			break;
		case 3:
			parseReturn = ParseGalWord03(parser);
			WriteReedSolomonInformationVector(parser,wordType,crcCheckResult);
			break;
		case 4:
			parseReturn = ParseGalWord04(parser);
			WriteReedSolomonInformationVector(parser,wordType,crcCheckResult);
			break;
		case 5:
			parseReturn = ParseGalWord05(rx,parser);
			break;
		case 6:
			parseReturn = ParseGalWord06(parser);
			break;
		case 7:
			parseReturn = ParseGalWord07(rx,parser,ttx);
			break;
		case 8:
			parseReturn = ParseGalWord08(rx,parser,ttx);
			break;
		case 9:
			parseReturn = ParseGalWord09(rx,parser,ttx);
			break;
		case 10:
			parseReturn = ParseGalWord10(rx,parser,ttx);
			break;
		case 16:
			parseReturn = ParseGalWord16(parser);
			break;
		case 17:
		case 18:
		case 19:
		case 20:
			parseReturn = ParseGalWord17(parser,wordType,crcCheckResult);
			break;
		case 63:
			if(parser->galParseLogFile!=NULL)
			{
				fprintf(parser->galParseLogFile,"WORD 63: DUMMY\n");
				fprintf(parser->galParseLogFile,"\n");
			}
			parser->dummyMsg = TRUE_FLAG;
			parseReturn = SUCCESS_RETURN;
			break;
		default:
			if(parser->galParseLogFile!=NULL)
			{
				fprintf(parser->galParseLogFile,"WORD %2u: Unknown or reserved\n",wordType);
				fprintf(parser->galParseLogFile,"\n");
			}
			parseReturn = FAILURE_RETURN;
			break;
	}
	/* if missing, try to recover WN/TOW from other channels */
	if((parseReturn==SUCCESS_RETURN) && ((parser->navMsg->towValid==FALSE_FLAG) || (parser->navMsg->wnValid==FALSE_FLAG)))
	{
		GetATransmissionTime(cfg,rx,chIndex,&satType,&wn,&ttxDouble);
		if(ttxDouble>0)
		{
			// valid TOW, look if it is in advance or late wrt the current channel
			// NB a page has just beed received, so TOW must be an integer number of seconds!
			if(parser->navMsg->towValid==FALSE_FLAG)
			{
				if((ttxDouble - floor(ttxDouble)) > (ceil(ttxDouble)-ttxDouble))
				{
					ttx = (int)ceil(ttxDouble);
				}
				else
				{
					ttx = (int)floor(ttxDouble);
				}
				parser->navMsg->tow = ttx - GAL_SECONDS_PER_PAGE;
				parser->navMsg->towValid = TRUE_FLAG;
				parser->bitIndexTotal = GAL_PAGE_LENGTH;
				if(parser->galParseLogFile!=NULL)
				{
					fprintf(parser->galParseLogFile,"TOW extrapolated from other channels %d\n\n",parser->navMsg->tow);
				}
			}
			// WN
			if((wn>0) && (parser->navMsg->wnValid==FALSE_FLAG))
			{
				parser->navMsg->wn = (satType==GPS_SAT) ? (wn - GPS_WN_TO_GAL_WN_BIAS) : wn;
				parser->navMsg->wnValid = TRUE_FLAG;
				if(parser->galParseLogFile!=NULL)
				{
					fprintf(parser->galParseLogFile,"WN extrapolated from other channels %d\n\n",parser->navMsg->wn);
				}
			}
		}
	}

	/* Here also OSNMA is managed! */
	if(parseReturn==FAILURE_RETURN)
	{
		RestartChannelOsnmaVariables(&(rx->osnmaData),&(parser->chOsnma));
		return FAILURE_RETURN;
	}
	rx->galLastDecodedSubframe=wordType;
	if(parser->navMsg->towValid==TRUE_FLAG)
	{
		ttx = parser->navMsg->tow + (parser->bitIndexTotal*GALILEO_BIT_DURATION)/SECONDS_TO_MILLISECONDS;
		rx->galLastDecodedPage = (((ttx-3)/30)%24)+1;
	}
	else
	{
		ttx = -1;
	}
	if(parser->dummyMsg==FALSE_FLAG)
	{
		/* if the navigation message is dummy, no OSNMA available */
		/* ttx evaluated at line 1215 is the transmission time of the start of the NEXT page.
		 * Subtract 2 seconds to find the transmission time of the start of the CURRENT page */
		ttx -= GAL_PAGE_DURATION;
		wn = (parser->navMsg->wnValid==TRUE_FLAG) ? parser->navMsg->wn : -1;
		UpdateChannelOsnmaVariables(&(rx->osnmaData),&(rx->osnmaAuth),&(parser->chOsnma),wn,ttx,wordType,parser->decodedPagePart1,parser->decodedPagePart2,parser->reserved1);
	}
	else
	{
		sprintf(parser->chOsnma.chOsnmaStatusLog,"               I/NAV Dummy Msg");
	}

	/* Reed-Solomon check */
	ManageReedSolomon(parser,wordType);

	return SUCCESS_RETURN;
}


TParseResult DecodeGalEvenPagePart(TGalParser *parser,unsigned char symbol)
{
	long metric;

	parser->pagePart[parser->byteIndex] = (parser->pagePart[parser->byteIndex] << 1) | symbol;
	parser->bitIndex++;
	if(parser->bitIndex==BITS_PER_BYTE)
	{
		parser->byteIndex++;
		parser->bitIndex=0;
		if(parser->byteIndex==GAL_ENCODED_PAGE_PART_LENGTH_BYTES)
		{
			parser->byteIndex=0;
			GalDeinterleaver(parser);
			GalileoViterbi(&metric,parser->decodedPagePart1,parser->deinterleavedPagePart,GAL_DECODED_PAGE_PART_LENGTH_BITS,0,0);
			if((parser->decodedPagePart1[0] & 0x80)==GALILEO_PAGE_PART_EVEN)
			{
				return DECODING_1_SUCCESSFUL;
			}
			else
			{
				return DECODING_2_SUCCESSFUL;
			}
		}
	}
	return DECODING_ON_GOING;
}


TParseResult DecodeGalOddPagePart(TNGeneConfiguration *cfg,TNGeneData *rx,int chIndex,TGalParser *parser,unsigned char symbol)
{
	long metric;

	parser->pagePart[parser->byteIndex] = (parser->pagePart[parser->byteIndex] << 1) | symbol;
	parser->bitIndex++;
	if(parser->bitIndex==BITS_PER_BYTE)
	{
		parser->byteIndex++;
		parser->bitIndex=0;
		if(parser->byteIndex==GAL_ENCODED_PAGE_PART_LENGTH_BYTES)
		{
			parser->byteIndex=0;
			GalDeinterleaver(parser);
			GalileoViterbi(&metric,parser->decodedPagePart2,parser->deinterleavedPagePart,GAL_DECODED_PAGE_PART_LENGTH_BITS,0,0);
			if(((parser->decodedPagePart2[0] >> 7) & 0x01)==GALILEO_PAGE_PART_ODD)
			{
				PrepareGalBits(parser);
				if(GalCrc24qVerified(parser->bitsForCrc,GAL_ARRAY_FOR_CRC_LENGTH_BYTES))
				{
					ParseGalSubframeWithCrcErrors(parser);
					RestartChannelOsnmaVariables(&(rx->osnmaData),&(parser->chOsnma));
					return CRC_ERROR;
				}
				if(((parser->decodedPagePart2[0] >> 6) & 0x01)==GALILEO_ALERT_PAGE)
				{
					/* alert pages are not parsed and do not contain OSNMA */
					RestartChannelOsnmaVariables(&(rx->osnmaData),&(parser->chOsnma));
					return DECODING_2_SUCCESSFUL;
				}
				ExtractGalreservedBits(parser); // OSNMA and SSP
				if(ParseGalSubframe(cfg,rx,chIndex,parser)==DECODING_ERROR)
				{
					return DECODING_ERROR;
				}
				return DECODING_2_SUCCESSFUL;
			}
			else
			{
				RestartChannelOsnmaVariables(&(rx->osnmaData),&(parser->chOsnma));
				return DECODING_ERROR;
			}
		}
	}
	return DECODING_ON_GOING;
}


void ParseGalMessage(TNGeneConfiguration *cfg,TNGeneData *rx,TChannel *ch,float corr)
{
	int chIndex = ch->chIndex;
	TGalParser *parser = ch->galParser;
	TNavigationMessage *navMsg = parser->navMsg;
	unsigned char symbol = (corr > 0.0) ? parser->polarityCorrection : (parser->polarityCorrection^0x01);
	TParseResult parseResult;
	int wn;
	int tow;

	/* count the received symbol */
	parser->bitIndexTotal++;

	/* parse state machine */
	switch(parser->status)
	{
		case SEARCH_SYNCHRONIZATION:
			if(GalSynchronization(parser,symbol)==PREAMBLE_FOUND)
			{
				parser->status=FIRST_DECODING;
			}
			break;
		case FIRST_DECODING:
			parseResult =  DecodeGalEvenPagePart(parser,symbol);
			if(parseResult==DECODING_1_SUCCESSFUL)
			{
				parser->status=PREAMBLE_2;
			}
			else if(parseResult==DECODING_2_SUCCESSFUL)
			{
				parser->status=PREAMBLE_1;
			}
			break;
		case PREAMBLE_1:
			if(GalSynchronization(parser,symbol)==PREAMBLE_FOUND)
			{
				parser->status=DECODING_1;
			}
			break;
		case DECODING_1:
			parseResult = DecodeGalEvenPagePart(parser,symbol);
			if(parseResult!=DECODING_ON_GOING)
			{
				// IFEN format log
				if((navMsg->wnValid==TRUE_FLAG) && (navMsg->towValid==TRUE_FLAG) && (parser->synchronizationSuccess==TRUE_FLAG))
				{
					// do it when synchronization is confirmed and time is valid
					wn = navMsg->wn;
					tow = navMsg->tow + ((parser->bitIndexTotal*GALILEO_BIT_DURATION)/SECONDS_TO_MILLISECONDS) - GAL_PAGE_PART_DURATION;
					SaveGalileoINavSymbolsInIfenLog(rx->ifenLogGal,&(rx->ifenLogGalTime),wn,tow,ch->satId,parser->pagePart);
				}
				//
				parser->status=PREAMBLE_2;
			}
			break;
		case PREAMBLE_2:
			if(GalSynchronization(parser,symbol)==PREAMBLE_FOUND)
			{
				parser->status=DECODING_2;
			}
			break;
		case DECODING_2:
			parseResult = DecodeGalOddPagePart(cfg,rx,chIndex,parser,symbol);
			if(parseResult!=DECODING_ON_GOING)
			{
				// IFEN format log
				if((navMsg->wnValid==TRUE_FLAG) && (navMsg->towValid==TRUE_FLAG) && (parser->synchronizationSuccess==TRUE_FLAG))
				{
					// do it when synchronization is confirmed and time is valid
					wn = navMsg->wn;
					tow = navMsg->tow + ((parser->bitIndexTotal*GALILEO_BIT_DURATION)/SECONDS_TO_MILLISECONDS) - GAL_PAGE_PART_DURATION;
					SaveGalileoINavSymbolsInIfenLog(rx->ifenLogGal,&(rx->ifenLogGalTime),wn,tow,ch->satId,parser->pagePart);
				}
				if(parseResult==DECODING_2_SUCCESSFUL)
				{
					parser->synchronizationSuccess=TRUE_FLAG;
					parser->status=PREAMBLE_1;
				}
				else if(parser->synchronizationSuccess==FALSE_FLAG)
				{
					/* an error has been found, and no page has been decoded yet:
					 * seemingly the message synchronization is wrong */
					parser->status=SEARCH_SYNCHRONIZATION;
				}
				else
				{
					/* an error has been found, but at least one page has been decoded:
					 * seemingly the message synchronization is OK */
					parser->status=PREAMBLE_1;
				}
				/* manage coherence of the parsed ephemerides */
				if((navMsg->subframe1Received==TRUE_FLAG) && (navMsg->subframe2Received==TRUE_FLAG) && (navMsg->subframe3Received==TRUE_FLAG) && (navMsg->subframe4Received==TRUE_FLAG) && (navMsg->subframe5Received==TRUE_FLAG) &&
				   (navMsg->ephTmp.iode1==navMsg->ephTmp.iode2) && (navMsg->ephTmp.iode2==navMsg->ephTmp.iode3) && (navMsg->ephTmp.iode3==navMsg->ephTmp.iode4) &&
				   (parser->redCedInPvt!=RED_CED_ONLY))
				{
					if(navMsg->eph.iode1!=navMsg->ephTmp.iode1)
					{
						/* a new set of data from navigation message can be sent to the GUI */
						ch->newNavMsgInformation = TRUE_FLAG;
					}
					CopyFromTmpToOperative(navMsg);
					navMsg->subframe1Received=FALSE_FLAG;
					navMsg->subframe2Received=FALSE_FLAG;
					navMsg->subframe3Received=FALSE_FLAG;
					navMsg->subframe4Received=FALSE_FLAG;
					navMsg->subframe5Received=FALSE_FLAG;
					navMsg->ephemeridesReceived = TRUE_FLAG;
					navMsg->ephFromRedCed = FALSE_FLAG;
					/* optional RINEX log */
					if((navMsg->wnValid==TRUE_FLAG) && (ch->newNavMsgInformation==TRUE_FLAG))
					{
						RinexSaveGalNav(cfg,&(rx->rinexGal),ch,navMsg);
						if(cfg->userInterface!=RX_UI_GUI)
						{
							// if the GUI is not used, reset here the flag to avoid saving twice the same navigation data
							// otherwise, the flag is reset by SendNavigationMessageInformation
							ch->newNavMsgInformation = FALSE_FLAG;
						}
					}
					RinexAddGalIono(cfg,&(rx->rinexGal),navMsg);
					RinexAddGaut(cfg,&(rx->rinexGal),navMsg);
					RinexAddGpga(cfg,&(rx->rinexGal),navMsg);
				}
			}
			break;
		default:
			break;
	}
}
