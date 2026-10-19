/**
 * @file reedSolomon.c
 *
 * @author Mario Nicola
 *
 * @date 08/07/2021
 *
 * @version 1.0.0
 *
 * @brief Functions to perform Reed-Solomon decoder for Galileo I/NAV navigation message.
 */

#include <fec.h>
#include <stdlib.h>
#include <string.h>
#include "reedSolomon.h"


/* parameters of the Galileo Reed-Solomon code */
static void *rs = NULL;
static int symsize = 8;
static int gfpoly = 0x11d;
static int fcr = 1;
static int prim = 1;
static int nroots = 60;
static int pad = 137;

/* local variables used to manage the Reed-Solomon decoder */
// rsData contains all the information and parity symbols
static unsigned char rsData[GAL_RS_INFORMATION_VECTOR_LENGTH_BYTES+GAL_RS_PARITY_VECTOR_LENGTH_BYTES];
// erasures and erasuresIn contain the indexes of all the missing symbols
static int erasures[GAL_RS_INFORMATION_VECTOR_LENGTH_BYTES+GAL_RS_PARITY_VECTOR_LENGTH_BYTES];
static int erasuresIn[GAL_RS_INFORMATION_VECTOR_LENGTH_BYTES+GAL_RS_PARITY_VECTOR_LENGTH_BYTES];



void InitReedSolomon()
{
	rs = init_rs_char(symsize,gfpoly,fcr,prim,nroots,pad);
}

void DeinitReedSolomon()
{
	if(rs==NULL)
		return;

	free_rs_char(rs);
	rs = NULL;
}


int GetErasureIndexes(TGalParser *parser)
{
	int numErasures = 0;
	int idx;
	int idx2;

	/*

	 The index of the erasures must take into account that the order of the symbols is reverted

	 word  4 => indexes   0- 13
	 word  3 => indexes  14- 27
	 word  2 => indexes  28- 41
	 word  1 => indexes  42- 57
	 word 20 => indexes  58- 72
	 word 19 => indexes  73- 87
	 word 18 => indexes  88-102
	 word 17 => indexes 103-117

	Then the number of padding symbols must be added to make the decoder working
	Because the padding is added before the actual symbols
	(from man: The padding will consist of the specified number of zeroes at the front of the full codeword)
	*/

	// word 4,3,2
	for(idx2=0;idx2<3;idx2++)
	{
		if(parser->rsInformationReceived[3-idx2]==FALSE_FLAG)
		{
			for(idx=0;idx<14;idx++)
			{
				erasures[numErasures] = (14*idx2) + idx;
				numErasures++;
			}
		}
	}
	// word 1
	if(parser->rsInformationReceived[3-idx2]==FALSE_FLAG)
	{
		for(idx=0;idx<16;idx++)
		{
			erasures[numErasures] = (14*idx2) + idx;
			numErasures++;
		}
	}
	// word 20,19,18,17
	for(idx2=0;idx2<4;idx2++)
	{
		if(parser->rsParityReceived[3-idx2]==FALSE_FLAG)
		{
			for(idx=0;idx<15;idx++)
			{
				erasures[numErasures] = 58 + (15*idx2) + idx;
				numErasures++;
			}
		}
	}
	// add num padding to the erasure indexes
	for(idx=0;idx<numErasures;idx++)
	{
		erasures[idx] += pad;
		erasuresIn[idx] = erasures[idx];
	}

	return numErasures;
}


void CopySymbolsForRsDecoder(TGalParser *parser)
{
	int idx;

	/* copy in the local vector, reverting the order of the symbols */
	for(idx=0;idx<GAL_RS_INFORMATION_VECTOR_LENGTH_BYTES;idx++)
	{
		rsData[idx] = parser->rsInformationVector[GAL_RS_INFORMATION_VECTOR_LENGTH_BYTES-1-idx];
	}
	for(;idx<GAL_RS_INFORMATION_VECTOR_LENGTH_BYTES+GAL_RS_PARITY_VECTOR_LENGTH_BYTES;idx++)
	{
		rsData[idx] = parser->rsParityVector[GAL_RS_INFORMATION_VECTOR_LENGTH_BYTES+GAL_RS_PARITY_VECTOR_LENGTH_BYTES-1-idx];
	}
}


void RetrieveSymbolsFromRsDecoder(TGalParser *parser)
{
	int idx;

	/* copy from the local vector, reverting the order of the symbols */
	for(idx=0;idx<GAL_RS_INFORMATION_VECTOR_LENGTH_BYTES;idx++)
	{
		parser->rsInformationVector[idx] = rsData[GAL_RS_INFORMATION_VECTOR_LENGTH_BYTES-1-idx];
	}
	for(idx=0;idx<GAL_RS_PARITY_VECTOR_LENGTH_BYTES;idx++)
	{
		parser->rsParityVector[idx] = rsData[GAL_RS_INFORMATION_VECTOR_LENGTH_BYTES+GAL_RS_PARITY_VECTOR_LENGTH_BYTES-1-idx];
	}
}


TRsCheckResult CheckReedSolomon(TGalParser *parser,int *numCorrections)
{
	int idx;
	int received;
	int informationValid;
	int valid;
	int corrections;
	int numErasures;

	if(rs==NULL)
	{
		return RS_CHECK_NOT_PERFORMED;
	}

	/* count received/valid symbols */
	received = 0;
	informationValid = 0;
	valid = 0;
	for(idx=0;idx<GAL_RS_NUM_OF_USED_WORDS;idx++)
	{
		// received (maybe with CRC errors)
		if(parser->rsInformationReceived[idx]==TRUE_FLAG)
		{
			received++;
		}
		if(parser->rsParityReceived[idx]==TRUE_FLAG)
		{
			received++;
		}
		// valid (no CRC errors)
		if(parser->rsInformationValid[idx]==TRUE_FLAG)
		{
			informationValid++;
			valid++;
		}
		if(parser->rsParityValid[idx]==TRUE_FLAG)
		{
			valid++;
		}
	}

	/*
	 * check the received (seemingly good) symbols
	 */
	if(valid==(2*GAL_RS_NUM_OF_USED_WORDS)) // 4 information + 4 parity
	{
		// copy in the local vector, reverting the order of the symbols
		CopySymbolsForRsDecoder(parser);
		// just check if everything is ok
		corrections = decode_rs_char(rs,rsData,NULL,0);
		*numCorrections = corrections;
		// return
		if(corrections==0)
			return RS_CHECK_NO_ERRORS;
		else
			return RS_CHECK_ERRORS;
	}
	/*
	 * try to find the missing symbols (the others are received AND valid)
	 */
	else if((informationValid<GAL_RS_NUM_OF_USED_WORDS) &&
			((valid>=GAL_RS_NUM_OF_USED_WORDS) || (received>GAL_RS_NUM_OF_USED_WORDS)))
	{
		// do not perform if all the information symbols are available
		// perform when we received at least
		//   4 valid pages or
		//   5 pages with CRC errors

		// copy in the local vector, reverting the order of the symbols
		CopySymbolsForRsDecoder(parser);
		// manage erasures
		numErasures = GetErasureIndexes(parser);
		if(numErasures>GAL_RS_PARITY_VECTOR_LENGTH_BYTES)
		{
			// RS cannot correct more than to GAL_RS_PARITY_VECTOR_LENGTH_BYTES errors
			// even if the position is known
			return RS_FAILED_RESTORE;
		}
		// try to restore missing symbols
		corrections = decode_rs_char(rs,rsData,erasures,numErasures);
		*numCorrections = corrections;
		if(corrections<numErasures)
		{
			return RS_FAILED_RESTORE;
		}
		// copy from the local vector, reverting the order of the symbols
		RetrieveSymbolsFromRsDecoder(parser);
		//
		return RS_ERASURE_RESTORED;
	}

	return RS_CHECK_NOT_PERFORMED;
}
