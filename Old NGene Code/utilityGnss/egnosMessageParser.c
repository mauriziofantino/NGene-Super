/**
 * @file egnosMessageParser.c
 *
 * @author Mario Nicola
 *
 * @date 21/04/2015
 *
 * @version 1.0.0
 *
 * @brief Functions used to parse the EGNOS navigation message.
 */

#include <stdlib.h>
#include <math.h>
#include "ngeneDataStructure.h"
#include "ngeneConfiguration.h"
#include "channelDataStructure.h"
#include "navigationMessageParser.h"
#include "ngeneConstants.h"
#include "timeConstants.h"
#include "ioFunctions.h"
#include "closeManager.h"
#include "arrays.h"
#include "viterbi.h"
#include "egnosCorrections.h"
#include "egnosIonoCorrections.h"

static unsigned int egnosCrc24qTable[256] = {
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



int InitEgnosParser(TNGeneConfiguration *cfg,TChannel *ch)
{
	if(ch->satType!=EGNOS_SAT)
	{
		ch->egnosParser = NULL;
		return SUCCESS_RETURN;
	}
	ch->egnosParser = (TEgnosParser *)malloc(sizeof(TEgnosParser));
	if(ch->egnosParser==NULL)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Malloc returned a NULL pointer in InitEgnosParser");
		CloseNGene(cfg,EXIT_FAILURE);
	}
	//ch->egnosParser->navMsg=NULL;
	ch->egnosParser->egnosParseLogFile = NULL;
	ch->egnosParser->egnosBitsLogFile = NULL;
	return SUCCESS_RETURN;
}


void DeinitEgnosParser(TChannel *ch)
{
	if(ch->egnosParser!=NULL)
	{
		if(ch->egnosParser->egnosParseLogFile!=NULL)
		{
			fclose(ch->egnosParser->egnosParseLogFile);
		}
		if(ch->egnosParser->egnosBitsLogFile!=NULL)
		{
			fclose(ch->egnosParser->egnosBitsLogFile);
		}
		free(ch->egnosParser);
		ch->egnosParser=NULL;
	}
}


void CleanEgnosParserArrays(TEgnosParser *parser,int cleanInputs)
{
	if(cleanInputs==TRUE_FLAG)
	{
		ArrayClean((char *)(parser->symbols),EGNOS_ENCODED_BLOCK_LENGTH_SYMBOLS+EGNOS_ADDITIONAL_BLOCK_LENGTH_SYMBOLS+1);
	}
	ArrayClean((char *)(parser->path1),2*EGNOS_DECODED_BLOCK_LENGTH_BITS*sizeof(unsigned long));
	ArrayClean((char *)(parser->path2),2*EGNOS_DECODED_BLOCK_LENGTH_BITS*sizeof(unsigned long));
	ArrayClean((char *)(parser->stateMetric),64*sizeof(long));
	ArrayClean((char *)(parser->decodedBits),EGNOS_ARRAY_FOR_PARSE_LENGTH_BYTES*sizeof(unsigned char));
	ArrayClean((char *)(parser->additionalBits),EGNOS_ARRAY_FOR_PARSE_LENGTH_BYTES*sizeof(unsigned char));
}

void StartEgnosParser(TNGeneConfiguration *cfg,TChannel *ch)
{
	TEgnosParser *parser = ch->egnosParser;
	char filename[2*MAX_ROW_LENGTH];

	if(ch->satType!=EGNOS_SAT)
		return;

	if(ch->satId==INVALID_PRN)
		return;

	CleanEgnosParserArrays(parser,TRUE_FLAG);
	parser->symbolIndex=0;
	parser->polarityCorrection=0x00;
	parser->status=SEARCH_SYNCHRONIZATION;
	parser->expectedPreamble=EGNOS_PREAMBLE_INVALID_INDEX;

	if(cfg->logConfiguration.enableNavMesgLog==TRUE_FLAG)
	{
		if(parser->egnosParseLogFile!=NULL)
		{
			fclose(parser->egnosParseLogFile);
		}
		sprintf(filename,"%s/corrections_EGNOS%03d.txt",cfg->dumpPathCurrent,ch->satId);
		parser->egnosParseLogFile = fopen(filename,"w");
		if(parser->egnosBitsLogFile!=NULL)
		{
			fclose(parser->egnosBitsLogFile);
		}
		sprintf(filename,"%s/viterbiOutputs_EGNOS%03d.txt",cfg->dumpPathCurrent,ch->satId);
		parser->egnosBitsLogFile = fopen(filename,"a");
	}
}


TEgnosPreambleIndex ResearchEgnosPreamble(unsigned char *bits,unsigned int blocksize,unsigned int *startOfPreamble,unsigned char *polarityCorrection)
{
	TEgnosPreambleIndex preambleIndex = EGNOS_PREAMBLE_INVALID_INDEX;
	unsigned char mask = 0x00;
	int idx = 7;
	unsigned char tmpByte;

    tmpByte = bits[0];
    while((idx < (int)blocksize) && (preambleIndex == EGNOS_PREAMBLE_INVALID_INDEX))
    {
    	switch(tmpByte)
    	{
    		case EGNOS_PREAMBLE_1:
    			preambleIndex = EGNOS_PREAMBLE_1_INDEX;
    			*polarityCorrection = 0x00;
    			break;
    		case EGNOS_PREAMBLE_2:
    			preambleIndex = EGNOS_PREAMBLE_2_INDEX;
    			*polarityCorrection = 0x00;
    			break;
    		case EGNOS_PREAMBLE_3:
    			preambleIndex = EGNOS_PREAMBLE_3_INDEX;
    			*polarityCorrection = 0x00;
    			break;
    		case EGNOS_PREAMBLE_INV_1:
				preambleIndex = EGNOS_PREAMBLE_1_INDEX;
    			*polarityCorrection = 0xff;
				break;
    		case EGNOS_PREAMBLE_INV_2:
				preambleIndex = EGNOS_PREAMBLE_2_INDEX;
    			*polarityCorrection = 0xff;
				break;
    		case EGNOS_PREAMBLE_INV_3:
				preambleIndex = EGNOS_PREAMBLE_3_INDEX;
    			*polarityCorrection = 0xff;
				break;
    		default:
    			mask = mask >> 1;
    			idx++;
    			if(mask==0x00)
    			{
    				mask=0x80;
    			}
    			tmpByte = tmpByte << 1;
    			if((bits[idx>>3] & mask)!=0)
    			{
    				tmpByte |= 0x01;
    			}
    			break;
    	}
    }

    *startOfPreamble = idx - 7;
    return preambleIndex;
}


TEgnosPreambleIndex CheckAndUpdateEgnosPreamble(unsigned char *bits,TEgnosPreambleIndex preambleIndex)
{
	if((bits[0]==EGNOS_PREAMBLE_1) && (preambleIndex==EGNOS_PREAMBLE_1_INDEX))
		return EGNOS_PREAMBLE_2_INDEX;

	if((bits[0]==EGNOS_PREAMBLE_2) && (preambleIndex==EGNOS_PREAMBLE_2_INDEX))
		return EGNOS_PREAMBLE_3_INDEX;

	if((bits[0]==EGNOS_PREAMBLE_3) && (preambleIndex==EGNOS_PREAMBLE_3_INDEX))
		return EGNOS_PREAMBLE_1_INDEX;

	return EGNOS_PREAMBLE_INVALID_INDEX;
}


int EgnosCrc24qVerified(unsigned char *vec, int vecLen)
{
    unsigned int r;
    unsigned int tabIdx;

    r = 0;
    tabIdx = 0;

    while(vecLen--)
    {
		tabIdx = (r >> 16) & 0xFF;
		r = (r << 8) | *vec++;
		r ^= egnosCrc24qTable[tabIdx];
    }
    r &= 0xFFFFFF;
    if(0 == r)
	{
    	return SUCCESS_RETURN;
	}
    return FAILURE_RETURN;
}


static void PlotEgnosBits(TEgnosParser *parser)
{
	int idx;
	unsigned char tmpChar;

	for(idx=0;idx<EGNOS_ARRAY_FOR_PARSE_LENGTH_BYTES;idx++)
	{
		tmpChar = parser->decodedBits[idx];
		fprintf(parser->egnosBitsLogFile,"%02X",tmpChar);
	}
	fprintf(parser->egnosBitsLogFile,"\n");
	fflush(parser->egnosBitsLogFile);
}

static void PrintPrnFromTheMask(FILE *f,int numOfPrnsInTheMask,TEgnosMaskElement *prn)
{
	if(numOfPrnsInTheMask>0)
	{
		if(prn->satType==GPS_PRN)
		{
			fprintf(f,"        GPS %3u",prn->satId);
		}
		else if(prn->satType==GLONASS_SLOT)
		{
			fprintf(f,"        GLO %3u",prn->satId);
		}
		else if(prn->satType!=INVALID_EGNOS_CONSTELLATION)
		{
			fprintf(f,"            %3u",prn->satId);
		}
	}
	else
	{
		fprintf(f,"             NA");
	}
}


void ParseEgnosType01(TNGeneConfiguration *cfg,TNGeneData *rx,TEgnosParser *parser)
{
	char oldMaskIodp;
	int bitIndex;
	int byteIndex = 1;
	unsigned char mask = 0x02;
	TEgnosCorrections *egnosCorr = &(rx->egnosCorrections);
	TEgnosMaskElement *prnMaskEntry;
	int idx;

	oldMaskIodp = egnosCorr->maskIodp;
	egnosCorr->maskIodp = (parser->decodedBits[28] >> 6);
	egnosCorr->numOfPrnsInTheMask = 0;
	/* PRNs */
	for(bitIndex=1;bitIndex<=210;bitIndex++)
	{
		if((parser->decodedBits[byteIndex]&mask)!=0)
		{
			prnMaskEntry = &(egnosCorr->prnMask[egnosCorr->numOfPrnsInTheMask]);
			if((bitIndex>=1) && (bitIndex<=37))
			{
				prnMaskEntry->satId = bitIndex;
				prnMaskEntry->satType = GPS_PRN;
			}
			else if((bitIndex>=38) && (bitIndex<=61))
			{
				prnMaskEntry->satId = bitIndex-37;
				prnMaskEntry->satType = GLONASS_SLOT;
			}
			else if((bitIndex>=62) && (bitIndex<=119))
			{
				prnMaskEntry->satId = bitIndex;
				prnMaskEntry->satType = FUTURE_GNSS;
			}
			else if((bitIndex>=120) && (bitIndex<=138))
			{
				prnMaskEntry->satId = bitIndex;
				prnMaskEntry->satType = SBAS_PRN;
			}
			else
			{
				prnMaskEntry->satId = bitIndex;
				prnMaskEntry->satType = FUTURE_OTHERS;
			}
			egnosCorr->numOfPrnsInTheMask++;
		}
		mask = mask>>1;
		if(mask==0)
		{
			mask = 0x80;
			byteIndex++;
		}
	}
	/* If mask is changed, then clean up invalid information */
	if(oldMaskIodp!=egnosCorr->maskIodp)
	{
		CleanEgnosCorrections(egnosCorr);
		SetEgnosPrnMaskIndex(cfg->numberOfChannels,rx->channelArray,&(rx->egnosCorrections));
	}

	if(parser->egnosParseLogFile!=NULL)
	{
		fprintf(parser->egnosParseLogFile,"BLOCK  1: PRN Mask assignments\n");
		fprintf(parser->egnosParseLogFile,"  IODP: %u\n", egnosCorr->maskIodp);
		fprintf(parser->egnosParseLogFile,"  PRNs: %d\n", egnosCorr->numOfPrnsInTheMask);
		if(egnosCorr->numOfPrnsInTheMask>0)
		{
			fprintf(parser->egnosParseLogFile,"        GPS ");

			for(idx=0;idx<egnosCorr->numOfPrnsInTheMask;idx++)
			{
				prnMaskEntry = &(egnosCorr->prnMask[idx]);
				if(prnMaskEntry->satType==GPS_PRN)
				{
					fprintf(parser->egnosParseLogFile," %3u",prnMaskEntry->satId);
				}
			}
			fprintf(parser->egnosParseLogFile,"\n");
			fprintf(parser->egnosParseLogFile,"        GLO ");
			for(idx=0;idx<egnosCorr->numOfPrnsInTheMask;idx++)
			{
				prnMaskEntry = &(egnosCorr->prnMask[idx]);
				if(prnMaskEntry->satType==GLONASS_SLOT)
				{
					fprintf(parser->egnosParseLogFile," %3u",prnMaskEntry->satId);
				}
			}
			fprintf(parser->egnosParseLogFile,"\n");
			fprintf(parser->egnosParseLogFile,"     OTHERS ");
			for(idx=0;idx<egnosCorr->numOfPrnsInTheMask;idx++)
			{
				prnMaskEntry = &(egnosCorr->prnMask[idx]);
				if((prnMaskEntry->satType!=GPS_PRN) && (prnMaskEntry->satType!=GLONASS_SLOT) && (prnMaskEntry->satType!=INVALID_EGNOS_CONSTELLATION))
				{
					fprintf(parser->egnosParseLogFile," %3u",prnMaskEntry->satId);
				}
			}
			fprintf(parser->egnosParseLogFile,"\n");
		}
		fprintf(parser->egnosParseLogFile,"\n");
	}
}


void ParseEgnosType0205(TNGeneData *rx,TEgnosParser *parser,double tow)
{
	TEgnosCorrections *egnosCorr = &(rx->egnosCorrections);
	TEgnosFastCorrection *fastCorr;
	unsigned char iodf;
	unsigned char iodp;
    unsigned char msgNum;
    unsigned char idx1; /* index of the first element (w.r.t. the PRN mask array) */
    unsigned char idx2; /* index of the last element (w.r.t. the PRN mask array) */
    unsigned char numSat;
    int idx;
    double prcOld, towOld;
    double prc[13];
    unsigned char udrei[13];
    int l;

    /* parse */
    msgNum = (parser->decodedBits[1] >> 2);
    iodf = (parser->decodedBits[1]) & 0x03;
    iodp = (parser->decodedBits[2] >> 6);
    prc[0]  = ExtractTwoCompField(parser->decodedBits,2,3,6,6,12)*0.125;
    prc[1]  = ExtractTwoCompField(parser->decodedBits,3,5,2,2,12)*0.125;
    prc[2]  = ExtractTwoCompField(parser->decodedBits,5,6,6,6,12)*0.125;
    prc[3]  = ExtractTwoCompField(parser->decodedBits,6,8,2,2,12)*0.125;
    prc[4]  = ExtractTwoCompField(parser->decodedBits,8,9,6,6,12)*0.125;
    prc[5]  = ExtractTwoCompField(parser->decodedBits,9,11,2,2,12)*0.125;
    prc[6]  = ExtractTwoCompField(parser->decodedBits,11,12,6,6,12)*0.125;
    prc[7]  = ExtractTwoCompField(parser->decodedBits,12,14,2,2,12)*0.125;
    prc[8]  = ExtractTwoCompField(parser->decodedBits,14,15,6,6,12)*0.125;
    prc[9]  = ExtractTwoCompField(parser->decodedBits,15,17,2,2,12)*0.125;
    prc[10] = ExtractTwoCompField(parser->decodedBits,17,18,6,6,12)*0.125;
    prc[11] = ExtractTwoCompField(parser->decodedBits,18,20,2,2,12)*0.125;
    prc[12] = ExtractTwoCompField(parser->decodedBits,20,21,6,6,12)*0.125;
    udrei[0]  = (unsigned char)ExtractField(parser->decodedBits,21,22,2,2);
    udrei[1]  = (unsigned char)ExtractField(parser->decodedBits,22,22,6,6);
    udrei[2]  = (unsigned char)ExtractField(parser->decodedBits,22,23,2,2);
    udrei[3]  = (unsigned char)ExtractField(parser->decodedBits,23,23,6,6);
    udrei[4]  = (unsigned char)ExtractField(parser->decodedBits,23,24,2,2);
    udrei[5]  = (unsigned char)ExtractField(parser->decodedBits,24,24,6,6);
    udrei[6]  = (unsigned char)ExtractField(parser->decodedBits,24,25,2,2);
    udrei[7]  = (unsigned char)ExtractField(parser->decodedBits,25,25,6,6);
    udrei[8]  = (unsigned char)ExtractField(parser->decodedBits,25,26,2,2);
    udrei[9]  = (unsigned char)ExtractField(parser->decodedBits,26,26,6,6);
    udrei[10] = (unsigned char)ExtractField(parser->decodedBits,26,27,2,2);
    udrei[11] = (unsigned char)ExtractField(parser->decodedBits,27,27,6,6);
    udrei[12] = (unsigned char)ExtractField(parser->decodedBits,27,28,2,2);

    numSat = egnosCorr->numOfPrnsInTheMask;
    if((numSat==0) || (iodp!=egnosCorr->maskIodp))
    {
    	/* if the mask is not available or the IODP is new w.r.t. the current mask,
    	 * save all the information.
    	 * Information not valid will be deleted when the new mask is available */
    	numSat = EGNOS_ENTRIES;
    }

    /* Get the indexes of the parsed information */
    idx1 = (msgNum - 2) * 13;
    if(idx1>=numSat)
    {
    	return;
    }
    idx2 = idx1 + 13;
    if(idx2>numSat)
    {
    	idx2=numSat;
    }
    l=idx2-idx1;

    /* save information in the data structure */
    for(idx=0;idx<l;idx++)
    {
    	fastCorr = &(egnosCorr->fastCorrections[idx+idx1]);
    	towOld = fastCorr->towOld;
    	prcOld = fastCorr->prc;
    	fastCorr->iodf = iodf;
    	fastCorr->iodp = iodp;
    	fastCorr->prc = prc[idx];
    	fastCorr->udrei = udrei[idx];
    	fastCorr->towOld = tow;
    	if(fastCorr->towOldValid==TRUE_FLAG)
    	{
    		if(towOld>tow)
    		{
    			/* beyond the week margin! */
    			fastCorr->rrc = (prc[idx] - prcOld) / (tow - towOld + SECONDS_IN_A_WEEK);
    		}
    		else
    		{
    			fastCorr->rrc = (prc[idx] - prcOld) / (tow - towOld);
    		}
    		fastCorr->valid = TRUE_FLAG;
    	}
    	else
    	{
    		fastCorr->rrc = 0.0;
    		fastCorr->valid = FALSE_FLAG;
    	}
    	fastCorr->towOldValid = TRUE_FLAG;
    	CompleteUdreiInformation(fastCorr->udrei,&(fastCorr->udreiMeters),&(fastCorr->sigma2Udre),&(fastCorr->monitored),&(fastCorr->toBeUsed));
    }

    if(parser->egnosParseLogFile!=NULL)
	{
		fprintf(parser->egnosParseLogFile,"BLOCK %2u: Fast corrections\n",msgNum);
		for(idx=idx1;idx<idx1+13;idx++)
		{
			fastCorr = &(egnosCorr->fastCorrections[idx]);
			if((fastCorr->valid==TRUE_FLAG) || (fastCorr->towOldValid==TRUE_FLAG))
			{
				PrintPrnFromTheMask(parser->egnosParseLogFile,egnosCorr->numOfPrnsInTheMask,&(egnosCorr->prnMask[idx]));
				fprintf(parser->egnosParseLogFile,"  IODP: %2u",fastCorr->iodp);
				fprintf(parser->egnosParseLogFile,"  IODF: %2u",fastCorr->iodf);
				fprintf(parser->egnosParseLogFile,"   TOF: %8.1f",fastCorr->towOld);
				fprintf(parser->egnosParseLogFile," UDREI: %2u",fastCorr->udrei);
				fprintf(parser->egnosParseLogFile,"   PRC: %8.3f",fastCorr->prc);
				if(fastCorr->valid==TRUE_FLAG)
				{
					fprintf(parser->egnosParseLogFile,"   RRC: %8.3f", fastCorr->rrc);
				}
				fprintf(parser->egnosParseLogFile,"\n");
			}
		}
		fprintf(parser->egnosParseLogFile,"\n");
	}
}


void ParseEgnosType06(TNGeneData *rx,TEgnosParser *parser)
{
	TEgnosCorrections *egnosCorr = &(rx->egnosCorrections);
	TEgnosIntegrityInformation *integrity;
	unsigned char iodf[4];
	int idx;

	/* Only when a mask is available! No IODP in message 6... */
	if(egnosCorr->numOfPrnsInTheMask==0)
	{
		if(parser->egnosParseLogFile!=NULL)
		{
			fprintf(parser->egnosParseLogFile,"BLOCK  6: Integrity Information\n");
			fprintf(parser->egnosParseLogFile,"\n");
		}
		return;
	}
	/* read IODF */
	iodf[0] = parser->decodedBits[1] & 0x03;
	iodf[1] = (parser->decodedBits[2] >> 6) & 0x03;
	iodf[2] = (parser->decodedBits[2] >> 4) & 0x03;
	iodf[3] = (parser->decodedBits[2] >> 2) & 0x03;
	/* read UDREIs */
	for(idx=0;(idx<EGNOS_ENTRIES)&&(idx<egnosCorr->numOfPrnsInTheMask);idx++)
	{
		integrity = &(egnosCorr->integrityInformation[idx]);
		integrity->iodf = iodf[idx/13];
		integrity->valid = TRUE_FLAG;
		integrity->udrei = (unsigned char)ExtractField(parser->decodedBits,2+((idx+1)/2),3+(idx/2),2+(4*(idx%2)),2+(4*(idx%2)));
		CompleteUdreiInformation(integrity->udrei,&(integrity->udreiMeters),&(integrity->sigma2Udre),&(integrity->monitored),&(integrity->toBeUsed));
	}

	if(parser->egnosParseLogFile!=NULL)
	{
		fprintf(parser->egnosParseLogFile,"BLOCK  6: Integrity Information\n");
		for(idx=0;idx<EGNOS_ENTRIES;idx++)
		{
			integrity = &(egnosCorr->integrityInformation[idx]);
			if(integrity->valid==TRUE_FLAG)
			{
				PrintPrnFromTheMask(parser->egnosParseLogFile,egnosCorr->numOfPrnsInTheMask,&(egnosCorr->prnMask[idx]));
				fprintf(parser->egnosParseLogFile,"  IODF: %2u",integrity->iodf);
				fprintf(parser->egnosParseLogFile," UDREI: %2u",integrity->udrei);
				fprintf(parser->egnosParseLogFile,"\n");
			}
		}
		fprintf(parser->egnosParseLogFile,"\n");
	}
}


void ParseEgnosType07(TNGeneData *rx,TEgnosParser *parser)
{
	TEgnosCorrections *egnosCorr = &(rx->egnosCorrections);
	TEgnosFastCorrectionDegradationFactor *fastDegFact;
	unsigned char numSat;
	int idx;
	char iodp;
	unsigned char tLat;

	tLat = (unsigned char)ExtractField(parser->decodedBits,1,2,2,2);
	iodp = (char)ExtractField(parser->decodedBits,2,2,4,6);

	numSat = egnosCorr->numOfPrnsInTheMask;
	if((numSat==0) || (iodp!=egnosCorr->maskIodp))
	{
		/* if the mask is not available or the IODP is new w.r.t. the current mask,
		 * save all the information.
		 * Information not valid will be deleted when the new mask is available */
		numSat = EGNOS_ENTRIES;
	}

	/* read degradation factors (ai) */
	for(idx=0;(idx<EGNOS_ENTRIES)&&(idx<numSat);idx++)
	{
		fastDegFact = &(egnosCorr->fastDegradationFactors[idx]);
		fastDegFact->iodp = iodp;
		fastDegFact->valid = TRUE_FLAG;
		fastDegFact->tLat = tLat;
		fastDegFact->ai = (unsigned char)ExtractField(parser->decodedBits,2+((idx+1)/2),3+(idx/2),2+(4*(idx%2)),2+(4*(idx%2)));
		CompleteFastDegradationFactorInformation(fastDegFact);
	}

	if(parser->egnosParseLogFile!=NULL)
	{
		fprintf(parser->egnosParseLogFile,"BLOCK  7: Fast Correction Degradation Factors\n");
		for(idx=0;idx<EGNOS_ENTRIES;idx++)
		{
			fastDegFact = &(egnosCorr->fastDegradationFactors[idx]);
			if(fastDegFact->valid==TRUE_FLAG)
			{
				PrintPrnFromTheMask(parser->egnosParseLogFile,egnosCorr->numOfPrnsInTheMask,&(egnosCorr->prnMask[idx]));
				fprintf(parser->egnosParseLogFile,"  IODP: %2d",fastDegFact->iodp);
				fprintf(parser->egnosParseLogFile,"  tLat: %2u",fastDegFact->tLat);
				fprintf(parser->egnosParseLogFile,"    ai: %2u",fastDegFact->ai);
				fprintf(parser->egnosParseLogFile,"\n");
			}
		}
		fprintf(parser->egnosParseLogFile,"\n");
	}
}


void ParseEgnosType10(TNGeneData *rx,TEgnosParser *parser)
{
	TEgnosCorrections *egnosCorr = &(rx->egnosCorrections);
	TEgnosDegradationFactors *degFact = &(egnosCorr->degradationFactors);

	degFact->valid = TRUE_FLAG;
	degFact->bRrc = (float)ExtractField(parser->decodedBits,1,2,2,8)*0.002;
	degFact->cLtcLsb = (float)ExtractField(parser->decodedBits,3,4,8,2)*0.002;
	degFact->cLtcV1 = (float)ExtractField(parser->decodedBits,4,5,6,4)*0.00005;
	degFact->iLtcV1 = (unsigned int)ExtractField(parser->decodedBits,5,6,4,5);
	degFact->cLtcV0 = (float)ExtractField(parser->decodedBits,6,7,3,7)*0.002;
	degFact->iLtcV0 = (unsigned int)ExtractField(parser->decodedBits,7,8,1,8);
	degFact->cGeoLsb = (float)ExtractField(parser->decodedBits,9,10,8,2)*0.0005;
	degFact->cGeoV = (float)ExtractField(parser->decodedBits,10,11,6,4)*0.00005;
	degFact->iGeo = (unsigned int)ExtractField(parser->decodedBits,11,12,4,5);
	degFact->cEr = (float)ExtractField(parser->decodedBits,12,13,3,3)*0.5;
	degFact->cIonoStep = (float)ExtractField(parser->decodedBits,13,14,5,5)*0.001;
	degFact->iIono = (unsigned int)ExtractField(parser->decodedBits,14,15,3,6);
	degFact->cIonoRamp = (float)ExtractField(parser->decodedBits,15,16,2,8)*0.000005;
	degFact->rssUdre = (unsigned int)ExtractField(parser->decodedBits,17,17,1,0);
	degFact->rssIono = (unsigned int)ExtractField(parser->decodedBits,17,17,2,7);
	degFact->cCovariance = (float)ExtractField(parser->decodedBits,17,18,6,1)*0.1;

	if(parser->egnosParseLogFile!=NULL)
	{
		fprintf(parser->egnosParseLogFile,"BLOCK 10: Degradation Factors\n");
		fprintf(parser->egnosParseLogFile,"         B_rrc: %f\n",degFact->bRrc);
		fprintf(parser->egnosParseLogFile,"     C_ltc_lsb: %f\n",degFact->cLtcLsb);
		fprintf(parser->egnosParseLogFile,"      C_ltc_v1: %f\n",degFact->cLtcV1);
		fprintf(parser->egnosParseLogFile,"      I_ltc_v1: %d\n",degFact->iLtcV1);
		fprintf(parser->egnosParseLogFile,"      C_ltc_v0: %f\n",degFact->cLtcV0);
		fprintf(parser->egnosParseLogFile,"      I_ltc_v0: %d\n",degFact->iLtcV0);
		fprintf(parser->egnosParseLogFile,"     C_geo_lsb: %f\n",degFact->cGeoLsb);
		fprintf(parser->egnosParseLogFile,"       C_geo_v: %f\n",degFact->cGeoV);
		fprintf(parser->egnosParseLogFile,"         I_geo: %d\n",degFact->iGeo);
		fprintf(parser->egnosParseLogFile,"          C_er: %f\n",degFact->cEr);
		fprintf(parser->egnosParseLogFile,"   C_iono_step: %f\n",degFact->cIonoStep);
		fprintf(parser->egnosParseLogFile,"        I_iono: %d\n",degFact->iIono);
		fprintf(parser->egnosParseLogFile,"   C_iono_ramp: %f\n",degFact->cIonoRamp);
		fprintf(parser->egnosParseLogFile,"      RSS_UDRE: %d\n",degFact->rssUdre);
		fprintf(parser->egnosParseLogFile,"      RSS_iono: %d\n",degFact->rssIono);
		fprintf(parser->egnosParseLogFile,"  C_covariance: %f\n",degFact->cCovariance);
		fprintf(parser->egnosParseLogFile,"\n");
	}
}


void ParseEgnosType18(TNGeneData *rx,TEgnosParser *parser)
{
	char oldMaskIodi;
	int bandNumber;
	int maxNumOfPoints;
	int bitIndex;
	int byteIndex = 3;
	unsigned char mask = 0x80;
	TEgnosCorrections *egnosCorr = &(rx->egnosCorrections);
	TEgnosIonoMask *ionoMask;
	int n;

	egnosCorr->numOfIonoBands = (char)ExtractField(parser->decodedBits,1,2,2,2);
	if(egnosCorr->numOfIonoBands==0)
	{
		/* No valid iono information are available */
		if(parser->egnosParseLogFile!=NULL)
		{
			fprintf(parser->egnosParseLogFile,"BLOCK 18: IGP mask not valid\n");
			fprintf(parser->egnosParseLogFile,"\n");
		}
		return;
	}
	bandNumber = (int)ExtractField(parser->decodedBits,2,2,6,6); /* range of values: 0-11 */
	ionoMask = &(egnosCorr->ionoMask[bandNumber]);
	oldMaskIodi = ionoMask->maskIodi;
	ionoMask->maskIodi = parser->decodedBits[2] & 0x03;
	if(bandNumber<=7)
	{
		maxNumOfPoints = 201;
	}
	else if(bandNumber==8)
	{
		maxNumOfPoints = 200;
	}
	else /* if((bandIdx==9) || (bandIdx==10)) */
	{
		maxNumOfPoints = 192;
	}
	/* parse the mask */
	ionoMask->numOfPointInTheMask = 0;
	for(bitIndex=0;bitIndex<maxNumOfPoints;bitIndex++)
	{
		if((parser->decodedBits[byteIndex]&mask)!=0)
		{
			n = ionoMask->numOfPointInTheMask;
			ionoMask->pointIndexes[n] = bitIndex;
			GetIgpLatLon(bandNumber,bitIndex,&(ionoMask->pointLatitudes[n]),&(ionoMask->pointLongitudes[n]));
			ionoMask->numOfPointInTheMask++;
		}
		mask = mask>>1;
		if(mask==0)
		{
			mask = 0x80;
			byteIndex++;
		}
	}
	/* If mask is changed, then clean up invalid information */
	if(oldMaskIodi!=ionoMask->maskIodi)
	{
		CleanEgnosIonoCorrections(egnosCorr,bandNumber);
	}

	if(parser->egnosParseLogFile!=NULL)
	{
		fprintf(parser->egnosParseLogFile,"BLOCK 18: IGP mask\n");
		fprintf(parser->egnosParseLogFile,"  IODI: %2d\n", ionoMask->maskIodi);
		fprintf(parser->egnosParseLogFile,"  band: %2d\n", bandNumber);
		fprintf(parser->egnosParseLogFile,"points: %3u\n", ionoMask->numOfPointInTheMask);
		fprintf(parser->egnosParseLogFile,"        ");
		for(bitIndex=0;bitIndex<(int)ionoMask->numOfPointInTheMask;bitIndex++)
		{
			fprintf(parser->egnosParseLogFile," %4u",ionoMask->pointIndexes[bitIndex]+1);
		}
		fprintf(parser->egnosParseLogFile,"\n");
		fprintf(parser->egnosParseLogFile,"   idx: ");
		for(bitIndex=0;bitIndex<(int)ionoMask->numOfPointInTheMask;bitIndex++)
		{
			fprintf(parser->egnosParseLogFile," %4d",bitIndex);
		}
		fprintf(parser->egnosParseLogFile,"\n");
		fprintf(parser->egnosParseLogFile,"   lat: ");
		for(bitIndex=0;bitIndex<(int)ionoMask->numOfPointInTheMask;bitIndex++)
		{
			fprintf(parser->egnosParseLogFile," %4d",ionoMask->pointLatitudes[bitIndex]);
		}
		fprintf(parser->egnosParseLogFile,"\n");
		fprintf(parser->egnosParseLogFile,"   lon: ");
		for(bitIndex=0;bitIndex<(int)ionoMask->numOfPointInTheMask;bitIndex++)
		{
			fprintf(parser->egnosParseLogFile," %4d",ionoMask->pointLongitudes[bitIndex]);
		}
		fprintf(parser->egnosParseLogFile,"\n");
		fprintf(parser->egnosParseLogFile,"\n");
	}
}


/* TODO Va debuggata trovando un dataset che contenga il messaggio di tipo 24!!! */
void ParseEgnosType24(TNGeneData *rx,TEgnosParser *parser,double tow)
{
	TEgnosCorrections *egnosCorr = &(rx->egnosCorrections);
	TEgnosFastCorrection *fastCorr;
	unsigned char iodf;
	unsigned char iodp;
	unsigned char blockId;
	unsigned char idx1; /* index of the first element (w.r.t. the PRN mask array) */
	unsigned char idx2; /* index of the last element (w.r.t. the PRN mask array) */
	unsigned char numSat;
	int idx;
	double prcOld, towOld;
	double prc[13];
	unsigned char udrei[13];
	int l;
	TEgnosLongTermCorrections *longTermCorr;
	int indexes[2];	/* indexes in the PRN mask for the (up to two) set of long term corrections. They start from 1!!! not 0!!! */
	unsigned char velocityCode;

	/* parse fast corrections */
	blockId = (parser->decodedBits[14] >> 6);
	iodf = (parser->decodedBits[14]>>4) & 0x03;
	iodp = parser->decodedBits[13] & 0x03;
	prc[0]  = ExtractTwoCompField(parser->decodedBits,1,3,2,2,12)*0.125;
	prc[1]  = ExtractTwoCompField(parser->decodedBits,3,4,6,6,12)*0.125;
	prc[2]  = ExtractTwoCompField(parser->decodedBits,4,6,2,2,12)*0.125;
	prc[3]  = ExtractTwoCompField(parser->decodedBits,6,7,6,6,12)*0.125;
	prc[4]  = ExtractTwoCompField(parser->decodedBits,7,9,2,2,12)*0.125;
	prc[5]  = ExtractTwoCompField(parser->decodedBits,9,10,6,6,12)*0.125;
	udrei[0]  = (unsigned char)ExtractField(parser->decodedBits,10,11,2,2);
	udrei[1]  = (unsigned char)ExtractField(parser->decodedBits,11,11,6,6);
	udrei[2]  = (unsigned char)ExtractField(parser->decodedBits,11,12,2,2);
	udrei[3]  = (unsigned char)ExtractField(parser->decodedBits,12,12,6,6);
	udrei[4]  = (unsigned char)ExtractField(parser->decodedBits,12,13,2,2);
	udrei[5]  = (unsigned char)ExtractField(parser->decodedBits,13,13,6,6);

	numSat = egnosCorr->numOfPrnsInTheMask;
	if((numSat==0) || (iodp!=egnosCorr->maskIodp))
	{
		/* if the mask is not available or the IODP is new w.r.t. the current mask,
		 * save all the information.
		 * Information not valid will be deleted when the new mask is available */
		numSat = EGNOS_ENTRIES;
	}

	/* Get the indexes of the parsed information */
	idx1 = blockId * 13;
	if(idx1>=numSat)
	{
		return;
	}
	idx2 = idx1 + 6;
	if(idx2>numSat)
	{
		idx2=numSat;
	}
	l=idx2-idx1;

	/* save information in the data structure */
	for(idx=0;idx<l;idx++)
	{
		fastCorr = &(egnosCorr->fastCorrections[idx+idx1]);
		towOld = fastCorr->towOld;
		prcOld = fastCorr->prc;
		fastCorr->iodf = iodf;
		fastCorr->iodp = iodp;
		fastCorr->prc = prc[idx];
		fastCorr->udrei = udrei[idx];
		fastCorr->towOld = tow;
		if(fastCorr->towOldValid==TRUE_FLAG)
		{
			if(towOld>tow)
			{
				/* beyond the week margin! */
				fastCorr->rrc = (prc[idx] - prcOld) / (tow - towOld + SECONDS_IN_A_WEEK);
			}
			else
			{
				fastCorr->rrc = (prc[idx] - prcOld) / (tow - towOld);
			}
			fastCorr->valid = TRUE_FLAG;
		}
		else
		{
			fastCorr->rrc = 0.0;
			fastCorr->valid = FALSE_FLAG;
		}
		fastCorr->towOldValid = TRUE_FLAG;
		CompleteUdreiInformation(fastCorr->udrei,&(fastCorr->udreiMeters),&(fastCorr->sigma2Udre),&(fastCorr->monitored),&(fastCorr->toBeUsed));
	}

	/* Long term satellite error corrections (it is equivalent to the second half message of block 25) */
	velocityCode = (parser->decodedBits[15]>>7) & 0x01;
	if(velocityCode==0)
	{
		iodp = (char)ExtractField(parser->decodedBits,27,27,1,1);
		indexes[0] = (unsigned char)ExtractField(parser->decodedBits,15,15,7,7);
		if(indexes[0]>0)
		{
			longTermCorr = &(egnosCorr->longTermCorrections[indexes[0]-1]);
			longTermCorr->iodp = iodp;
			longTermCorr->validVelocityCode0 = TRUE_FLAG;
			longTermCorr->validVelocityCode1 = FALSE_FLAG;
			longTermCorr->iod = (int)ExtractField(parser->decodedBits,15,16,1,7);
			longTermCorr->deltaX = ExtractTwoCompField(parser->decodedBits,16,17,1,8,9)*0.125;
			longTermCorr->deltaY = ExtractTwoCompField(parser->decodedBits,18,19,8,1,9)*0.125;
			longTermCorr->deltaZ = ExtractTwoCompField(parser->decodedBits,19,20,7,2,9)*0.125;
			longTermCorr->deltaAf0 = ExtractTwoCompField(parser->decodedBits,20,21,6,4,10)*POW_TWO_M31;
		}
		indexes[1] = (unsigned char)ExtractField(parser->decodedBits,21,22,4,2);
		if(indexes[1]>0)
		{
			longTermCorr = &(egnosCorr->longTermCorrections[indexes[1]-1]);
			longTermCorr->iodp = iodp;
			longTermCorr->validVelocityCode0 = TRUE_FLAG;
			longTermCorr->validVelocityCode1 = FALSE_FLAG;
			longTermCorr->iod = (int)ExtractField(parser->decodedBits,22,23,6,2);
			longTermCorr->deltaX = ExtractTwoCompField(parser->decodedBits,23,24,6,3,9)*0.125;
			longTermCorr->deltaY = ExtractTwoCompField(parser->decodedBits,24,25,5,4,9)*0.125;
			longTermCorr->deltaZ = ExtractTwoCompField(parser->decodedBits,25,26,4,5,9)*0.125;
			longTermCorr->deltaAf0 = ExtractTwoCompField(parser->decodedBits,26,27,3,7,10)*POW_TWO_M31;
		}
	}
	else /* velocityCode==1 */
	{
		indexes[0] = (unsigned char)ExtractField(parser->decodedBits,15,15,7,7);
		indexes[1] = 0;
		if(indexes[0]>0)
		{
			longTermCorr = &(egnosCorr->longTermCorrections[indexes[0]-1]);
			longTermCorr->iodp = (char)ExtractField(parser->decodedBits,28,28,2,0);
			longTermCorr->validVelocityCode0 = FALSE_FLAG;
			longTermCorr->validVelocityCode1 = TRUE_FLAG;
			longTermCorr->iod = (int)ExtractField(parser->decodedBits,15,16,1,7);
			longTermCorr->deltaX = ExtractTwoCompField(parser->decodedBits,16,18,1,2,11)*0.125;
			longTermCorr->deltaY = ExtractTwoCompField(parser->decodedBits,18,19,6,5,11)*0.125;
			longTermCorr->deltaZ = ExtractTwoCompField(parser->decodedBits,19,20,3,8,11)*0.125;
			longTermCorr->deltaAf0 = ExtractTwoCompField(parser->decodedBits,21,22,8,3,11)*POW_TWO_M31;
			longTermCorr->rateDeltaX = ExtractTwoCompField(parser->decodedBits,22,23,5,3,8)*POW_TWO_M11;
			longTermCorr->rateDeltaY = ExtractTwoCompField(parser->decodedBits,23,24,5,3,8)*POW_TWO_M11;
			longTermCorr->rateDeltaZ = ExtractTwoCompField(parser->decodedBits,24,25,5,3,8)*POW_TWO_M11;
			longTermCorr->deltaAf1 = ExtractTwoCompField(parser->decodedBits,25,26,5,3,8)*POW_TWO_M39;
			longTermCorr->to = ExtractField(parser->decodedBits,26,27,5,8)*16.0;
		}
	}

	if(parser->egnosParseLogFile!=NULL)
	{
		fprintf(parser->egnosParseLogFile,"BLOCK 24: Mixed Fast Corrections/Long Term Satellite Error Corrections\n");
		fprintf(parser->egnosParseLogFile,"          Fast Corrections (block index %u)\n",blockId);
		for(idx=idx1;idx<idx1+6;idx++)
		{
			fastCorr = &(egnosCorr->fastCorrections[idx]);
			if((fastCorr->valid==TRUE_FLAG) || (fastCorr->towOldValid==TRUE_FLAG))
			{
				PrintPrnFromTheMask(parser->egnosParseLogFile,egnosCorr->numOfPrnsInTheMask,&(egnosCorr->prnMask[idx]));
				fprintf(parser->egnosParseLogFile,"  IODP: %2u",fastCorr->iodp);
				fprintf(parser->egnosParseLogFile,"  IODF: %2u",fastCorr->iodf);
				fprintf(parser->egnosParseLogFile,"   TOF: %8.1f",fastCorr->towOld);
				fprintf(parser->egnosParseLogFile," UDREI: %2u",fastCorr->udrei);
				fprintf(parser->egnosParseLogFile,"   PRC: %8.3f",fastCorr->prc);
				if(fastCorr->valid==TRUE_FLAG)
				{
					fprintf(parser->egnosParseLogFile,"   RRC: %8.3f", fastCorr->rrc);
				}
				fprintf(parser->egnosParseLogFile,"\n");
			}
		}
		fprintf(parser->egnosParseLogFile,"\n");
		fprintf(parser->egnosParseLogFile,"          Long Term Satellite Error Corrections\n");
		for(idx=0;idx<2;idx++)
		{
			if(indexes[idx]>0)
			{
				longTermCorr = &(egnosCorr->longTermCorrections[indexes[idx]-1]);
				if(longTermCorr->validVelocityCode0==TRUE_FLAG)
				{
					PrintPrnFromTheMask(parser->egnosParseLogFile,egnosCorr->numOfPrnsInTheMask,&(egnosCorr->prnMask[indexes[idx]-1]));
					fprintf(parser->egnosParseLogFile," (idx %2d)",indexes[idx]-1);
					fprintf(parser->egnosParseLogFile,"  IODP: %2d",longTermCorr->iodp);
					fprintf(parser->egnosParseLogFile,"   IOD: %d",longTermCorr->iod);
					fprintf(parser->egnosParseLogFile,"  dX: %f",longTermCorr->deltaX);
					fprintf(parser->egnosParseLogFile,"  dY: %f",longTermCorr->deltaY);
					fprintf(parser->egnosParseLogFile,"  dZ: %f",longTermCorr->deltaZ);
					fprintf(parser->egnosParseLogFile,"  dAf0: %g",longTermCorr->deltaAf0);
					fprintf(parser->egnosParseLogFile,"\n");
				}
				if(longTermCorr->validVelocityCode1==TRUE_FLAG)
				{
					PrintPrnFromTheMask(parser->egnosParseLogFile,egnosCorr->numOfPrnsInTheMask,&(egnosCorr->prnMask[indexes[idx]-1]));
					fprintf(parser->egnosParseLogFile," (idx %2d)",indexes[idx]-1);
					fprintf(parser->egnosParseLogFile,"  IODP: %2d",longTermCorr->iodp);
					fprintf(parser->egnosParseLogFile,"   IOD: %d",longTermCorr->iod);
					fprintf(parser->egnosParseLogFile,"  dX: %f",longTermCorr->deltaX);
					fprintf(parser->egnosParseLogFile,"  dY: %f",longTermCorr->deltaY);
					fprintf(parser->egnosParseLogFile,"  dZ: %f",longTermCorr->deltaZ);
					fprintf(parser->egnosParseLogFile,"  dAf0: %g",longTermCorr->deltaAf0);
					fprintf(parser->egnosParseLogFile,"  rdX: %f",longTermCorr->rateDeltaX);
					fprintf(parser->egnosParseLogFile,"  rdY: %f",longTermCorr->rateDeltaY);
					fprintf(parser->egnosParseLogFile,"  rdZ: %f",longTermCorr->rateDeltaZ);
					fprintf(parser->egnosParseLogFile,"  dAf1: %g",longTermCorr->deltaAf1);
					fprintf(parser->egnosParseLogFile,"  t0: %f",longTermCorr->to);
					fprintf(parser->egnosParseLogFile,"\n");
				}
			}
		}
		fprintf(parser->egnosParseLogFile,"\n");
	}
}


void ParseEgnosType25(TNGeneData *rx,TEgnosParser *parser)
{
	TEgnosCorrections *egnosCorr = &(rx->egnosCorrections);
	TEgnosLongTermCorrections *longTermCorr;
	int indexes[4];	/* indexes in the PRN mask for the (up to four) set of corrections. They start from 1!!! not 0!!! */
	unsigned char velocityCode;
	char iodp;

	/* First half of the message */
	velocityCode = (parser->decodedBits[1]>>1) & 0x01;
	if(velocityCode==0)
	{
		iodp = (char)ExtractField(parser->decodedBits,14,14,7,3);
		indexes[0] = (unsigned char)ExtractField(parser->decodedBits,1,2,1,5);
		if(indexes[0]>0)
		{
			longTermCorr = &(egnosCorr->longTermCorrections[indexes[0]-1]);
			longTermCorr->iodp = iodp;
			longTermCorr->validVelocityCode0 = TRUE_FLAG;
			longTermCorr->validVelocityCode1 = FALSE_FLAG;
			longTermCorr->iod = (int)ExtractField(parser->decodedBits,2,3,3,5);
			longTermCorr->deltaX = ExtractTwoCompField(parser->decodedBits,3,4,3,6,9)*0.125;
			longTermCorr->deltaY = ExtractTwoCompField(parser->decodedBits,4,5,2,7,9)*0.125;
			longTermCorr->deltaZ = ExtractTwoCompField(parser->decodedBits,5,6,1,8,9)*0.125;
			longTermCorr->deltaAf0 = ExtractTwoCompField(parser->decodedBits,7,8,8,2,10)*POW_TWO_M31;
		}
		indexes[1] = (unsigned char)ExtractField(parser->decodedBits,8,8,0,6);
		if(indexes[1]>0)
		{
			longTermCorr = &(egnosCorr->longTermCorrections[indexes[1]-1]);
			longTermCorr->iodp = iodp;
			longTermCorr->validVelocityCode0 = TRUE_FLAG;
			longTermCorr->validVelocityCode1 = FALSE_FLAG;
			longTermCorr->iod = (int)parser->decodedBits[9];
			longTermCorr->deltaX = ExtractTwoCompField(parser->decodedBits,10,11,8,1,9)*0.125;
			longTermCorr->deltaY = ExtractTwoCompField(parser->decodedBits,11,12,7,2,9)*0.125;
			longTermCorr->deltaZ = ExtractTwoCompField(parser->decodedBits,12,13,6,3,9)*0.125;
			longTermCorr->deltaAf0 = ExtractTwoCompField(parser->decodedBits,13,14,5,5,10)*POW_TWO_M31;
		}
	}
	else /* velocityCode==1 */
	{
		indexes[0] = (unsigned char)ExtractField(parser->decodedBits,1,2,1,5);
		indexes[1] = 0;
		if(indexes[0]>0)
		{
			longTermCorr = &(egnosCorr->longTermCorrections[indexes[0]-1]);
			longTermCorr->iodp = (char)ExtractField(parser->decodedBits,14,14,0,2);
			longTermCorr->validVelocityCode0 = FALSE_FLAG;
			longTermCorr->validVelocityCode1 = TRUE_FLAG;
			longTermCorr->iod = (int)ExtractField(parser->decodedBits,2,3,3,5);
			longTermCorr->deltaX = ExtractTwoCompField(parser->decodedBits,3,4,3,8,11)*0.125;
			longTermCorr->deltaY = ExtractTwoCompField(parser->decodedBits,5,6,8,3,11)*0.125;
			longTermCorr->deltaZ = ExtractTwoCompField(parser->decodedBits,6,7,5,6,11)*0.125;
			longTermCorr->deltaAf0 = ExtractTwoCompField(parser->decodedBits,7,9,2,1,11)*POW_TWO_M31;
			longTermCorr->rateDeltaX = ExtractTwoCompField(parser->decodedBits,9,10,7,1,8)*POW_TWO_M11;
			longTermCorr->rateDeltaY = ExtractTwoCompField(parser->decodedBits,10,11,7,1,8)*POW_TWO_M11;
			longTermCorr->rateDeltaZ = ExtractTwoCompField(parser->decodedBits,11,12,7,1,8)*POW_TWO_M11;
			longTermCorr->deltaAf1 = ExtractTwoCompField(parser->decodedBits,12,13,7,1,8)*POW_TWO_M39;
			longTermCorr->to = ExtractField(parser->decodedBits,13,14,7,6)*16.0;
		}
	}
	/* Second half of the message */
	velocityCode = (parser->decodedBits[15]>>7) & 0x01;
	if(velocityCode==0)
	{
		iodp = (char)ExtractField(parser->decodedBits,27,27,1,1);
		indexes[2] = (unsigned char)ExtractField(parser->decodedBits,15,15,7,7);
		if(indexes[2]>0)
		{
			longTermCorr = &(egnosCorr->longTermCorrections[indexes[2]-1]);
			longTermCorr->iodp = iodp;
			longTermCorr->validVelocityCode0 = TRUE_FLAG;
			longTermCorr->validVelocityCode1 = FALSE_FLAG;
			longTermCorr->iod = (int)ExtractField(parser->decodedBits,15,16,1,7);
			longTermCorr->deltaX = ExtractTwoCompField(parser->decodedBits,16,17,1,8,9)*0.125;
			longTermCorr->deltaY = ExtractTwoCompField(parser->decodedBits,18,19,8,1,9)*0.125;
			longTermCorr->deltaZ = ExtractTwoCompField(parser->decodedBits,19,20,7,2,9)*0.125;
			longTermCorr->deltaAf0 = ExtractTwoCompField(parser->decodedBits,20,21,6,4,10)*POW_TWO_M31;
		}
		indexes[3] = (unsigned char)ExtractField(parser->decodedBits,21,22,4,2);
		if(indexes[3]>0)
		{
			longTermCorr = &(egnosCorr->longTermCorrections[indexes[3]-1]);
			longTermCorr->iodp = iodp;
			longTermCorr->validVelocityCode0 = TRUE_FLAG;
			longTermCorr->validVelocityCode1 = FALSE_FLAG;
			longTermCorr->iod = (int)ExtractField(parser->decodedBits,22,23,6,2);
			longTermCorr->deltaX = ExtractTwoCompField(parser->decodedBits,23,24,6,3,9)*0.125;
			longTermCorr->deltaY = ExtractTwoCompField(parser->decodedBits,24,25,5,4,9)*0.125;
			longTermCorr->deltaZ = ExtractTwoCompField(parser->decodedBits,25,26,4,5,9)*0.125;
			longTermCorr->deltaAf0 = ExtractTwoCompField(parser->decodedBits,26,27,3,7,10)*POW_TWO_M31;
		}
	}
	else /* velocityCode==1 */
	{
		indexes[2] = (unsigned char)ExtractField(parser->decodedBits,15,15,7,7);
		indexes[3] = 0;
		if(indexes[2]>0)
		{
			longTermCorr = &(egnosCorr->longTermCorrections[indexes[2]-1]);
			longTermCorr->iodp = (char)ExtractField(parser->decodedBits,28,28,2,0);
			longTermCorr->validVelocityCode0 = FALSE_FLAG;
			longTermCorr->validVelocityCode1 = TRUE_FLAG;
			longTermCorr->iod = (int)ExtractField(parser->decodedBits,15,16,1,7);
			longTermCorr->deltaX = ExtractTwoCompField(parser->decodedBits,16,18,1,2,11)*0.125;
			longTermCorr->deltaY = ExtractTwoCompField(parser->decodedBits,18,19,6,5,11)*0.125;
			longTermCorr->deltaZ = ExtractTwoCompField(parser->decodedBits,19,20,3,8,11)*0.125;
			longTermCorr->deltaAf0 = ExtractTwoCompField(parser->decodedBits,21,22,8,3,11)*POW_TWO_M31;
			longTermCorr->rateDeltaX = ExtractTwoCompField(parser->decodedBits,22,23,5,3,8)*POW_TWO_M11;
			longTermCorr->rateDeltaY = ExtractTwoCompField(parser->decodedBits,23,24,5,3,8)*POW_TWO_M11;
			longTermCorr->rateDeltaZ = ExtractTwoCompField(parser->decodedBits,24,25,5,3,8)*POW_TWO_M11;
			longTermCorr->deltaAf1 = ExtractTwoCompField(parser->decodedBits,25,26,5,3,8)*POW_TWO_M39;
			longTermCorr->to = ExtractField(parser->decodedBits,26,27,5,8)*16.0;
		}
	}

	if(parser->egnosParseLogFile!=NULL)
	{
		int idx;
		fprintf(parser->egnosParseLogFile,"BLOCK 25: Long Term Satellite Error Corrections\n");
		for(idx=0;idx<4;idx++)
		{
			if(indexes[idx]>0)
			{
				longTermCorr = &(egnosCorr->longTermCorrections[indexes[idx]-1]);
				if(longTermCorr->validVelocityCode0==TRUE_FLAG)
				{
					PrintPrnFromTheMask(parser->egnosParseLogFile,egnosCorr->numOfPrnsInTheMask,&(egnosCorr->prnMask[indexes[idx]-1]));
					fprintf(parser->egnosParseLogFile," (idx %2d)",indexes[idx]-1);
					fprintf(parser->egnosParseLogFile,"  IODP: %2d",longTermCorr->iodp);
					fprintf(parser->egnosParseLogFile,"   IOD: %d",longTermCorr->iod);
					fprintf(parser->egnosParseLogFile,"  dX: %f",longTermCorr->deltaX);
					fprintf(parser->egnosParseLogFile,"  dY: %f",longTermCorr->deltaY);
					fprintf(parser->egnosParseLogFile,"  dZ: %f",longTermCorr->deltaZ);
					fprintf(parser->egnosParseLogFile,"  dAf0: %g",longTermCorr->deltaAf0);
					fprintf(parser->egnosParseLogFile,"\n");
				}
				if(longTermCorr->validVelocityCode1==TRUE_FLAG)
				{
					PrintPrnFromTheMask(parser->egnosParseLogFile,egnosCorr->numOfPrnsInTheMask,&(egnosCorr->prnMask[indexes[idx]-1]));
					fprintf(parser->egnosParseLogFile," (idx %2d)",indexes[idx]-1);
					fprintf(parser->egnosParseLogFile,"  IODP: %2d",longTermCorr->iodp);
					fprintf(parser->egnosParseLogFile,"   IOD: %d",longTermCorr->iod);
					fprintf(parser->egnosParseLogFile,"  dX: %f",longTermCorr->deltaX);
					fprintf(parser->egnosParseLogFile,"  dY: %f",longTermCorr->deltaY);
					fprintf(parser->egnosParseLogFile,"  dZ: %f",longTermCorr->deltaZ);
					fprintf(parser->egnosParseLogFile,"  dAf0: %g",longTermCorr->deltaAf0);
					fprintf(parser->egnosParseLogFile,"  rdX: %f",longTermCorr->rateDeltaX);
					fprintf(parser->egnosParseLogFile,"  rdY: %f",longTermCorr->rateDeltaY);
					fprintf(parser->egnosParseLogFile,"  rdZ: %f",longTermCorr->rateDeltaZ);
					fprintf(parser->egnosParseLogFile,"  dAf1: %g",longTermCorr->deltaAf1);
					fprintf(parser->egnosParseLogFile,"  t0: %f",longTermCorr->to);
					fprintf(parser->egnosParseLogFile,"\n");
				}
			}
		}
		fprintf(parser->egnosParseLogFile,"\n");
	}
}


void ParseEgnosType26(TNGeneData *rx,TEgnosParser *parser)
{
	TEgnosCorrections *egnosCorr = &(rx->egnosCorrections);
	TEgnosIonoIgpDelayCorrections *ionoCorr;
	int bandNumber;
	int blockId;
	int iodi;
	double vertDelay[15];
	int givei[15];
	int numPoints;
	unsigned char idx1; /* index of the first element (w.r.t. the IGP mask array) */
	unsigned char idx2; /* index of the last element (w.r.t. the IGP mask array) */
	int l;
	int idx;

	bandNumber = (int)ExtractField(parser->decodedBits,1,2,2,2);
	blockId = (int)ExtractField(parser->decodedBits,2,2,6,6);
	iodi = (int)ExtractField(parser->decodedBits,27,27,3,7);
	vertDelay[0] = ExtractField(parser->decodedBits,2,3,2,7)*0.125;
	vertDelay[1] = ExtractField(parser->decodedBits,4,5,5,4)*0.125;
	vertDelay[2] = ExtractField(parser->decodedBits,6,7,8,1)*0.125;
	vertDelay[3] = ExtractField(parser->decodedBits,7,8,3,6)*0.125;
	vertDelay[4] = ExtractField(parser->decodedBits,9,10,6,3)*0.125;
	vertDelay[5] = ExtractField(parser->decodedBits,10,11,1,8)*0.125;
	vertDelay[6] = ExtractField(parser->decodedBits,12,13,4,5)*0.125;
	vertDelay[7] = ExtractField(parser->decodedBits,14,15,7,2)*0.125;
	vertDelay[8] = ExtractField(parser->decodedBits,15,16,2,7)*0.125;
	vertDelay[9] = ExtractField(parser->decodedBits,17,18,5,4)*0.125;
	vertDelay[10] = ExtractField(parser->decodedBits,19,20,8,1)*0.125;
	vertDelay[11] = ExtractField(parser->decodedBits,20,21,3,6)*0.125;
	vertDelay[12] = ExtractField(parser->decodedBits,22,23,6,3)*0.125;
	vertDelay[13] = ExtractField(parser->decodedBits,23,24,1,8)*0.125;
	vertDelay[14] = ExtractField(parser->decodedBits,25,26,4,5)*0.125;
	givei[0] = (int)ExtractField(parser->decodedBits,3,4,1,3);
	givei[1] = (int)ExtractField(parser->decodedBits,5,5,0,4);
	givei[2] = (int)ExtractField(parser->decodedBits,7,7,5,7);
	givei[3] = (int)ExtractField(parser->decodedBits,8,9,2,2);
	givei[4] = (int)ExtractField(parser->decodedBits,10,10,7,5);
	givei[5] = (int)ExtractField(parser->decodedBits,12,12,4,0);
	givei[6] = (int)ExtractField(parser->decodedBits,13,14,3,1);
	givei[7] = (int)ExtractField(parser->decodedBits,15,15,6,6);
	givei[8] = (int)ExtractField(parser->decodedBits,16,17,1,3);
	givei[9] = (int)ExtractField(parser->decodedBits,18,18,0,4);
	givei[10] = (int)ExtractField(parser->decodedBits,20,20,5,7);
	givei[11] = (int)ExtractField(parser->decodedBits,21,22,2,2);
	givei[12] = (int)ExtractField(parser->decodedBits,23,23,7,5);
	givei[13] = (int)ExtractField(parser->decodedBits,25,25,4,0);
	givei[14] = (int)ExtractField(parser->decodedBits,26,27,3,1);

	/* Get the indexes of the parsed information */
	numPoints = egnosCorr->ionoMask[bandNumber].numOfPointInTheMask;
	if((numPoints==0) || (iodi!=egnosCorr->ionoMask[bandNumber].maskIodi))
	{
		/* if the mask is not available or the IODI is new w.r.t. the current mask,
		 * save all the information.
		 * Information not valid will be deleted when the new mask is available */
		numPoints = EGNOS_IONO_POINTS;
	}
	idx1 = blockId*15;
	idx2 = idx1+15;
	if(idx2>numPoints)
	{
		idx2=numPoints;
	}
	l=idx2-idx1;
	/* save information in the data structure */
	for(idx=0;idx<l;idx++)
	{
		ionoCorr = &(egnosCorr->bandIonoCorrections[bandNumber].igpIonoCorrections[idx+idx1]);
		ionoCorr->iodi = iodi;
		ionoCorr->valid = TRUE_FLAG;
		ionoCorr->igpVerticalDelay = vertDelay[idx];
		ionoCorr->igpGivei = givei[idx];
		/* A vertical delay of 63.875 meters (111111111) will indicate don't use */
		ionoCorr->usable = (ionoCorr->igpVerticalDelay>VERTICAL_DELAY_LIMIT) ? FALSE_FLAG : TRUE_FLAG;
		CompleteGiveInformation(ionoCorr);
	}

    if(parser->egnosParseLogFile!=NULL)
	{
		fprintf(parser->egnosParseLogFile,"BLOCK 26: Ionospheric Delay Corrections\n");
		fprintf(parser->egnosParseLogFile,"  band:  %2d\n", bandNumber);
		fprintf(parser->egnosParseLogFile," block:  %2d\n", blockId);
		fprintf(parser->egnosParseLogFile,"points: %3u\n", egnosCorr->ionoMask[bandNumber].numOfPointInTheMask);

		fprintf(parser->egnosParseLogFile,"   idx:");
		for(idx=0;idx<numPoints;idx++)
		{
			fprintf(parser->egnosParseLogFile,"   %3d",idx);
		}
		fprintf(parser->egnosParseLogFile,"\n");
		fprintf(parser->egnosParseLogFile,"  IODI:");
		for(idx=0;idx<numPoints;idx++)
		{
			ionoCorr = &(egnosCorr->bandIonoCorrections[bandNumber].igpIonoCorrections[idx]);
			if(ionoCorr->valid==TRUE_FLAG)
				fprintf(parser->egnosParseLogFile,"    %2d",ionoCorr->iodi);
			else
				fprintf(parser->egnosParseLogFile,"    NA");
		}
		fprintf(parser->egnosParseLogFile,"\n");
		fprintf(parser->egnosParseLogFile," delay:");
		for(idx=0;idx<numPoints;idx++)
		{
			ionoCorr = &(egnosCorr->bandIonoCorrections[bandNumber].igpIonoCorrections[idx]);
			if(ionoCorr->valid==TRUE_FLAG)
				fprintf(parser->egnosParseLogFile," %5.2f",ionoCorr->igpVerticalDelay);
			else
				fprintf(parser->egnosParseLogFile,"    NA");
		}
		fprintf(parser->egnosParseLogFile,"\n");
		fprintf(parser->egnosParseLogFile," GIVEI:");
		for(idx=0;idx<numPoints;idx++)
		{
			ionoCorr = &(egnosCorr->bandIonoCorrections[bandNumber].igpIonoCorrections[idx]);
			if(ionoCorr->valid==TRUE_FLAG)
				fprintf(parser->egnosParseLogFile,"    %2d",ionoCorr->igpGivei);
			else
				fprintf(parser->egnosParseLogFile,"    NA");
		}
		fprintf(parser->egnosParseLogFile,"\n");
		fprintf(parser->egnosParseLogFile,"\n");
	}
}



int ParseEgnosBlock(TNGeneConfiguration *cfg,TNGeneData *rx,TEgnosParser *parser,double tow)
{
	unsigned char msgType;

	if(parser->egnosParseLogFile!=NULL)
	{
		fprintf(parser->egnosParseLogFile,"TOW     : %f\n",tow);
	}

	msgType = parser->decodedBits[1] >> 2;
	if(msgType==0)
	{
		if(parser->egnosParseLogFile!=NULL)
		{
			fprintf(parser->egnosParseLogFile,"BLOCK  0: Don't use for safety applications (for SBAS testing)\n");
			fprintf(parser->egnosParseLogFile,"\n");
		}
	}
	else if(msgType==1)
	{
		ParseEgnosType01(cfg,rx,parser);
	}
	else if((msgType>=2) && (msgType<=5))
	{
		ParseEgnosType0205(rx,parser,tow);
	}
	else if(msgType==6)
	{
		ParseEgnosType06(rx,parser);
	}
	else if(msgType==7)
	{
		ParseEgnosType07(rx,parser);
	}
	else if(msgType==10)
	{
		ParseEgnosType10(rx,parser);
	}
	else if(msgType==18)
	{
		ParseEgnosType18(rx,parser);
	}
	else if(msgType==24)
	{
		ParseEgnosType24(rx,parser,tow);
	}
	else if(msgType==25)
	{
		ParseEgnosType25(rx,parser);
	}
	else if(msgType==26)
	{
		ParseEgnosType26(rx,parser);
	}
	else if((msgType==8) || (msgType==11) || ((msgType>=13) && (msgType<=16)) || ((msgType>=19) && (msgType<=23)) || ((msgType>=29) && (msgType<=61)))
	{
		if(parser->egnosParseLogFile!=NULL)
		{
			fprintf(parser->egnosParseLogFile,"BLOCK %2u: Reserved for future messages\n",msgType);
			fprintf(parser->egnosParseLogFile,"\n");
		}
	}
	else if(msgType==62)
	{
		if(parser->egnosParseLogFile!=NULL)
		{
			fprintf(parser->egnosParseLogFile,"BLOCK 62: Internal Test Message\n");
			fprintf(parser->egnosParseLogFile,"\n");
		}
	}
	else if(msgType==63)
	{
		if(parser->egnosParseLogFile!=NULL)
		{
			fprintf(parser->egnosParseLogFile,"BLOCK 63: Null Message\n");
			fprintf(parser->egnosParseLogFile,"\n");
		}
	}
	else
	{
		if(parser->egnosParseLogFile!=NULL)
		{
			fprintf(parser->egnosParseLogFile,"BLOCK %2u\n",msgType);
			fprintf(parser->egnosParseLogFile,"\n");
		}
	}

	return SUCCESS_RETURN;
}


void ParseEgnosMessage(TNGeneConfiguration *cfg,TNGeneData *rx,TChannel *ch,float corr,double tow)
{
	TEgnosParser *parser = ch->egnosParser;
	unsigned int startOfPreamble = 0;
	unsigned int blocksize = EGNOS_ENCODED_BLOCK_LENGTH_SYMBOLS;
	unsigned int blocksizeTot = EGNOS_ENCODED_BLOCK_LENGTH_SYMBOLS+EGNOS_ADDITIONAL_BLOCK_LENGTH_SYMBOLS;
	unsigned int blocksizeDecoded = EGNOS_DECODED_BLOCK_LENGTH_BITS;
	int readyForParse = FALSE_FLAG;
	int crcResult = FAILURE_RETURN;

	/* store  and count the received symbol */
	parser->symbols[parser->symbolIndex] = (corr > 0.0) ? 0x00 : 0xff;
	parser->symbolIndex++;

	/* parse state machine */
	switch(parser->status)
	{
		case SEARCH_SYNCHRONIZATION:
		case SEARCH_SYNCHRONIZATION_2:
			if(parser->symbolIndex==(blocksizeTot+1))
			{
				/* in this case (SEARCH_SYNCHRONIZATION_2) I have blocksizeTot + 1, input symbols, I discard the first */
				ArrayCopy((char *)&(parser->symbols[1]),(char *)&(parser->symbols[0]),blocksizeTot);
				parser->symbolIndex = blocksizeTot;
			}
			if(parser->symbolIndex==blocksizeTot)
			{
				CleanEgnosParserArrays(parser,FALSE_FLAG);
				EgnosViterbi(parser->stateMetric,parser->decodedBits,parser->additionalBits,blocksize,parser->symbols,blocksizeTot,0x00,parser->path1,parser->path2);
				parser->expectedPreamble = ResearchEgnosPreamble(parser->decodedBits,blocksizeDecoded,&startOfPreamble,&(parser->polarityCorrection));
				if(parser->expectedPreamble==EGNOS_PREAMBLE_INVALID_INDEX)
				{
					if(parser->status==SEARCH_SYNCHRONIZATION)
					{
						parser->status = SEARCH_SYNCHRONIZATION_2;
					}
					else /* if(parser->status==SEARCH_SYNCHRONIZATION_2) */
					{
						parser->symbolIndex = 0;
						parser->status = SEARCH_SYNCHRONIZATION;
					}
				}
				else if(startOfPreamble>0)
				{
					/* align to the block, not yet ready for the parse */
					ArrayCopy((char *)&(parser->symbols[2*startOfPreamble]),(char *)&(parser->symbols[0]),blocksizeTot-(2*startOfPreamble));
					parser->symbolIndex = blocksizeTot - (2*startOfPreamble);
					parser->status = FIRST_DECODING;
				}
				else
				{
					/* the bits are ready for the parse */
					if(parser->polarityCorrection==0xff)
					{
						/* revert if needed */
						CleanEgnosParserArrays(parser,FALSE_FLAG);
						EgnosViterbi(parser->stateMetric,parser->decodedBits,parser->additionalBits,blocksize,parser->symbols,blocksizeTot,parser->polarityCorrection,parser->path1,parser->path2);
					}
					readyForParse = TRUE_FLAG;
					parser->status = DECODING_2;
				}
			}
			break;
		case FIRST_DECODING:
			if(parser->symbolIndex==blocksizeTot)
			{
				CleanEgnosParserArrays(parser,FALSE_FLAG);
				EgnosViterbi(parser->stateMetric,parser->decodedBits,parser->additionalBits,blocksize,parser->symbols,blocksizeTot,parser->polarityCorrection,parser->path1,parser->path2);
				readyForParse = TRUE_FLAG;
				parser->status = DECODING_2;
			}
			break;
		case DECODING_1:
			if(parser->symbolIndex==blocksize)
			{
				EgnosViterbi(parser->stateMetric,parser->decodedBits,parser->additionalBits,blocksize,parser->symbols,blocksize,parser->polarityCorrection,parser->path1,parser->path2);
				readyForParse = TRUE_FLAG;
				parser->status = DECODING_2;
			}
			break;
		case DECODING_2:
			if(parser->symbolIndex==blocksize)
			{
				EgnosViterbi(parser->stateMetric,parser->decodedBits,parser->additionalBits,blocksize,parser->symbols,blocksize,parser->polarityCorrection,parser->path2,parser->path1);
				readyForParse = TRUE_FLAG;
				parser->status = DECODING_1;
			}
			break;
		default:
			break;
	}

	if(readyForParse==TRUE_FLAG)
	{
		parser->expectedPreamble = CheckAndUpdateEgnosPreamble(parser->decodedBits,parser->expectedPreamble);
		crcResult = EgnosCrc24qVerified(parser->decodedBits,EGNOS_ARRAY_FOR_PARSE_LENGTH_BYTES);
		if((parser->expectedPreamble == EGNOS_PREAMBLE_INVALID_INDEX) || (crcResult==FAILURE_RETURN))
		{
			if(parser->symbolIndex==blocksizeTot)
			{
				/* It came from the first decoding */
				ArrayCopy((char *)&(parser->symbols[1]),(char *)&(parser->symbols[0]),blocksizeTot-1);
				parser->symbolIndex = blocksizeTot -1;
			}
			parser->status = SEARCH_SYNCHRONIZATION;
		}
		else
		{
			parser->symbolIndex = 0;
			tow = floor(tow) - 2.0;	/* The time is referred to the first bit of the message block:
			 	 	 	 	 	 	 	- floor to take into account the flight time
			 	 	 	 	 	 	 	- -1.0 to take into account the latency of the Viterbi
			 	 	 	 	 	 	 	- -1.0 to take into account the size of the message */
			ParseEgnosBlock(cfg,rx,parser,tow);
			if(parser->egnosBitsLogFile!=NULL)
			{
				PlotEgnosBits(parser);
				fflush(parser->egnosBitsLogFile);
			}
		}
	}
}
