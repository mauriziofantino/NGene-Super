/**
 * @file viterbi.c
 *
 * @author Mario Nicola
 *
 * @date 13/10/2014
 *
 * @version 1.0.0
 *
 * @brief Functions implementing the Viterbi decoder used in Galileo and EGNOS.
 */

#include <string.h>
#include <limits.h>
#include "ngeneConstants.h"
#include "arrays.h"


static int metricTab[2][256]; /**< Used to quickly evaluate the branch metric */


/*
 *
 */
#define	BFLY1(inputStateIndex,outputParityBit) { \
	long met0,met1;\
	/* output state for input 0 */\
	met0 = cMet[inputStateIndex] + branchMetrics[outputParityBit];\
	met1 = cMet[inputStateIndex+32] + branchMetrics[3^outputParityBit];\
	nMet[2*inputStateIndex] = met0;\
	if(met1 > met0){\
		nMet[2*inputStateIndex] = met1;\
		dec |= 1 << ((2*inputStateIndex) & 31);\
	}\
	/* output state for input 1 */\
	met0 = cMet[inputStateIndex] + branchMetrics[3^outputParityBit];\
	met1 = cMet[inputStateIndex+32] + branchMetrics[outputParityBit];\
	nMet[2*inputStateIndex+1] = met0;\
	if(met1 > met0){\
		nMet[2*inputStateIndex+1] = met1;\
		dec |= 1 << ((2*inputStateIndex+1) & 31);\
	}\
}


/*
 *
 */
#define	BFLY2(inputStateIndex,outputParityBit) { \
	long met0,met1;\
	/* ACS for 0 branch */\
	met0 = nMet[inputStateIndex] + branchMetrics[outputParityBit];\
	met1 = nMet[inputStateIndex+32] + branchMetrics[3^outputParityBit];\
	cMet[2*inputStateIndex] = met0;\
	if(met1 > met0){\
		cMet[2*inputStateIndex] = met1;\
		dec |= 1 << ((2*inputStateIndex) & 31);\
	}\
	/* ACS for 1 branch */\
	met0 = nMet[inputStateIndex] + branchMetrics[3^outputParityBit];\
	met1 = nMet[inputStateIndex+32] + branchMetrics[outputParityBit];\
	cMet[2*inputStateIndex+1] = met0;\
	if(met1 > met0){\
		cMet[2*inputStateIndex+1] = met1;\
		dec |= 1 << ((2*inputStateIndex+1) & 31);\
	}\
}


void InitViterbi(void)
{
	int i;

	for(i=0;i<256;i++)
	{
		metricTab[0][i] = 128 - i;
		metricTab[1][255-i] = 127 - i;
	}
}


int GalileoViterbi(long *finalPathMetric, unsigned char *decData, unsigned char *symbols, unsigned int numOutBits, unsigned int startState, unsigned int endState)
{
	int bitCounter = 0;
	int i, branchMetrics[4];
	unsigned long dec, paths[numOutBits*2], *pathPointer;
	/* each section of the trellis consists of 64 states:
	 * in the paths variable it's represented using two words of 32 bits each */
	long cMet[64], nMet[64];

	startState &= 63;
	endState &= 63;

	// Convert all values in 0 and 255
	// the number of input symbols is 2*numOutBits because the rate is 1/2 for both F/NAV and I/NAV
	for(i=0;i<(int)(2*numOutBits);i++)
	{
		if(symbols[i] == 1) symbols[i] = 255;
	}

	/* Initialize starting metrics */
	for(i=0;i<64;i++)
	{
		cMet[i] = -999999;
	}
	cMet[startState] = 0;

	pathPointer = paths;
	for(;;)
	{
		// Read input symbol pair and compute branch metrics
		branchMetrics[0] = metricTab[0][symbols[1]^0xff] + metricTab[0][symbols[0]];
		branchMetrics[1] = metricTab[0][symbols[1]^0xff] + metricTab[1][symbols[0]];
		branchMetrics[3] = metricTab[1][symbols[1]^0xff] + metricTab[1][symbols[0]];
		branchMetrics[2] = metricTab[1][symbols[1]^0xff] + metricTab[0][symbols[0]];
		symbols += 2;

		dec = 0;
		BFLY1(0,0);
		BFLY1(6,0);
		BFLY1(8,0);
		BFLY1(14,0);
		BFLY1(2,3);
		BFLY1(4,3);
		BFLY1(10,3);
		BFLY1(12,3);
		BFLY1(1,1);
		BFLY1(7,1);
		BFLY1(9,1);
		BFLY1(15,1);
		BFLY1(3,2);
		BFLY1(5,2);
		BFLY1(11,2);
		BFLY1(13,2);
		*pathPointer++ = dec;

		dec = 0;
		BFLY1(19,0);
		BFLY1(21,0);
		BFLY1(27,0);
		BFLY1(29,0);
		BFLY1(17,3);
		BFLY1(23,3);
		BFLY1(25,3);
		BFLY1(31,3);
		BFLY1(18,1);
		BFLY1(20,1);
		BFLY1(26,1);
		BFLY1(28,1);
		BFLY1(16,2);
		BFLY1(22,2);
		BFLY1(24,2);
		BFLY1(30,2);
		*pathPointer++ = dec;

		if(++bitCounter == (int)numOutBits)
		{
			*finalPathMetric = nMet[endState];
			break;
		}

		// Read input symbol pair and compute branch metrics
		branchMetrics[0] = metricTab[0][symbols[1]^0xff] + metricTab[0][symbols[0]];
		branchMetrics[1] = metricTab[0][symbols[1]^0xff] + metricTab[1][symbols[0]];
		branchMetrics[3] = metricTab[1][symbols[1]^0xff] + metricTab[1][symbols[0]];
		branchMetrics[2] = metricTab[1][symbols[1]^0xff] + metricTab[0][symbols[0]];
		symbols += 2;

		dec = 0;
		BFLY2(0,0);
		BFLY2(6,0);
		BFLY2(8,0);
		BFLY2(14,0);
		BFLY2(2,3);
		BFLY2(4,3);
		BFLY2(10,3);
		BFLY2(12,3);
		BFLY2(1,1);
		BFLY2(7,1);
		BFLY2(9,1);
		BFLY2(15,1);
		BFLY2(3,2);
		BFLY2(5,2);
		BFLY2(11,2);
		BFLY2(13,2);
		*pathPointer++ = dec;

		dec = 0;
		BFLY2(19,0);
		BFLY2(21,0);
		BFLY2(27,0);
		BFLY2(29,0);
		BFLY2(17,3);
		BFLY2(23,3);
		BFLY2(25,3);
		BFLY2(31,3);
		BFLY2(18,1);
		BFLY2(20,1);
		BFLY2(26,1);
		BFLY2(28,1);
		BFLY2(16,2);
		BFLY2(22,2);
		BFLY2(24,2);
		BFLY2(30,2);
		*pathPointer++ = dec;

		if(++bitCounter == (int)numOutBits)
		{
			*finalPathMetric = cMet[endState];
			break;
		}

	}
	/* Chain back from terminal state to produce decoded data (tail included) */
	if(decData == NULL)
		return FAILURE_RETURN;

	memset(decData,0,(numOutBits+7)/8); /* round up in case nbits % 8 != 0 */
	for(i=numOutBits-1;i >= 0;i--)
	{
		pathPointer -= 2;
		decData[i>>3] |= (endState & 0x01)<<(7-(i&7));

		if(pathPointer[endState >> 5] & (1 << (endState & 31)))
		{
			endState |= 64;	/* 2^(K-1) */
		}
		endState >>= 1;
	}
	return SUCCESS_RETURN;
}

/**
 * @def EGNOS_VITERBI_RENORMALIZE
 * @brief Used to prevent overflows in the state metrics
 */
#define	EGNOS_VITERBI_RENORMALIZE	10000

int EgnosViterbi(long *stateMetric,unsigned char *decData,unsigned char *addData,unsigned int blocksize,unsigned char *symbols,unsigned int numSymbols,unsigned char polarityCorrection,unsigned long *paths1,unsigned long *paths2)
{
	unsigned int numOutBits = blocksize/2;
	long cMet[64], nMet[64];
	long maxMet = 0;
	long minMet = 0;
	unsigned long dec;
	unsigned long *pathPointer;
	int branchMetrics[4];
	int i;
	unsigned int bitCounter;
	unsigned int endState;

	if((numSymbols!=(2*blocksize)) && (numSymbols!=blocksize))
	{
		return FAILURE_RETURN;
	}

	/* initializes state metrics */
	ArrayCopy((char *)stateMetric,(char *)cMet,64*sizeof(long));

	/* Renormalize metrics to prevent overflow */
	for(i=0;i<64;i++)
	{
		if(cMet[i]>maxMet)
			maxMet=cMet[i];
		if(cMet[i]<minMet)
			minMet=cMet[i];
	}
	if(maxMet > (LONG_MAX - EGNOS_VITERBI_RENORMALIZE))
	{
		for(i = 0; i < 64; i++)
		{
			cMet[i] -= LONG_MAX;
		}
	}
	if(minMet < (LONG_MIN + EGNOS_VITERBI_RENORMALIZE))
	{
		for(i = 0; i < 64; i++)
		{
			cMet[i] += LONG_MAX;
		}
	}

	/* Correct the polarity of symbols */
	for(i=0;i<(int)numSymbols;i++)
	{
		symbols[i] ^= polarityCorrection;
	}

	/* elaborate the first block (if needed) */
	if(numSymbols==2*blocksize)
	{
		pathPointer = paths1;
		for(bitCounter=0;bitCounter<blocksize;bitCounter+=4)
		{
			// Read input symbol pair and compute branch metrics
			branchMetrics[0] = metricTab[0][symbols[1]] + metricTab[0][symbols[0]];
			branchMetrics[1] = metricTab[0][symbols[1]] + metricTab[1][symbols[0]];
			branchMetrics[3] = metricTab[1][symbols[1]] + metricTab[1][symbols[0]];
			branchMetrics[2] = metricTab[1][symbols[1]] + metricTab[0][symbols[0]];
			symbols += 2;

			dec = 0;
			BFLY1(0,0);
			BFLY1(6,0);
			BFLY1(8,0);
			BFLY1(14,0);
			BFLY1(2,3);
			BFLY1(4,3);
			BFLY1(10,3);
			BFLY1(12,3);
			BFLY1(1,1);
			BFLY1(7,1);
			BFLY1(9,1);
			BFLY1(15,1);
			BFLY1(3,2);
			BFLY1(5,2);
			BFLY1(11,2);
			BFLY1(13,2);
			*pathPointer++ = dec;

			dec = 0;
			BFLY1(19,0);
			BFLY1(21,0);
			BFLY1(27,0);
			BFLY1(29,0);
			BFLY1(17,3);
			BFLY1(23,3);
			BFLY1(25,3);
			BFLY1(31,3);
			BFLY1(18,1);
			BFLY1(20,1);
			BFLY1(26,1);
			BFLY1(28,1);
			BFLY1(16,2);
			BFLY1(22,2);
			BFLY1(24,2);
			BFLY1(30,2);
			*pathPointer++ = dec;

			// Read input symbol pair and compute branch metrics
			branchMetrics[0] = metricTab[0][symbols[1]] + metricTab[0][symbols[0]];
			branchMetrics[1] = metricTab[0][symbols[1]] + metricTab[1][symbols[0]];
			branchMetrics[3] = metricTab[1][symbols[1]] + metricTab[1][symbols[0]];
			branchMetrics[2] = metricTab[1][symbols[1]] + metricTab[0][symbols[0]];
			symbols += 2;

			dec = 0;
			BFLY2(0,0);
			BFLY2(6,0);
			BFLY2(8,0);
			BFLY2(14,0);
			BFLY2(2,3);
			BFLY2(4,3);
			BFLY2(10,3);
			BFLY2(12,3);
			BFLY2(1,1);
			BFLY2(7,1);
			BFLY2(9,1);
			BFLY2(15,1);
			BFLY2(3,2);
			BFLY2(5,2);
			BFLY2(11,2);
			BFLY2(13,2);
			*pathPointer++ = dec;

			dec = 0;
			BFLY2(19,0);
			BFLY2(21,0);
			BFLY2(27,0);
			BFLY2(29,0);
			BFLY2(17,3);
			BFLY2(23,3);
			BFLY2(25,3);
			BFLY2(31,3);
			BFLY2(18,1);
			BFLY2(20,1);
			BFLY2(26,1);
			BFLY2(28,1);
			BFLY2(16,2);
			BFLY2(22,2);
			BFLY2(24,2);
			BFLY2(30,2);
			*pathPointer++ = dec;
		}
	}

	/* elaborate the second block */
	pathPointer = paths2;
	for(bitCounter=0;bitCounter<blocksize;bitCounter+=4)
	{
		// Read input symbol pair and compute branch metrics
		branchMetrics[0] = metricTab[0][symbols[1]] + metricTab[0][symbols[0]];
		branchMetrics[1] = metricTab[0][symbols[1]] + metricTab[1][symbols[0]];
		branchMetrics[3] = metricTab[1][symbols[1]] + metricTab[1][symbols[0]];
		branchMetrics[2] = metricTab[1][symbols[1]] + metricTab[0][symbols[0]];
		symbols += 2;

		dec = 0;
		BFLY1(0,0);
		BFLY1(6,0);
		BFLY1(8,0);
		BFLY1(14,0);
		BFLY1(2,3);
		BFLY1(4,3);
		BFLY1(10,3);
		BFLY1(12,3);
		BFLY1(1,1);
		BFLY1(7,1);
		BFLY1(9,1);
		BFLY1(15,1);
		BFLY1(3,2);
		BFLY1(5,2);
		BFLY1(11,2);
		BFLY1(13,2);
		*pathPointer++ = dec;

		dec = 0;
		BFLY1(19,0);
		BFLY1(21,0);
		BFLY1(27,0);
		BFLY1(29,0);
		BFLY1(17,3);
		BFLY1(23,3);
		BFLY1(25,3);
		BFLY1(31,3);
		BFLY1(18,1);
		BFLY1(20,1);
		BFLY1(26,1);
		BFLY1(28,1);
		BFLY1(16,2);
		BFLY1(22,2);
		BFLY1(24,2);
		BFLY1(30,2);
		*pathPointer++ = dec;

		// Read input symbol pair and compute branch metrics
		branchMetrics[0] = metricTab[0][symbols[1]] + metricTab[0][symbols[0]];
		branchMetrics[1] = metricTab[0][symbols[1]] + metricTab[1][symbols[0]];
		branchMetrics[3] = metricTab[1][symbols[1]] + metricTab[1][symbols[0]];
		branchMetrics[2] = metricTab[1][symbols[1]] + metricTab[0][symbols[0]];
		symbols += 2;

		dec = 0;
		BFLY2(0,0);
		BFLY2(6,0);
		BFLY2(8,0);
		BFLY2(14,0);
		BFLY2(2,3);
		BFLY2(4,3);
		BFLY2(10,3);
		BFLY2(12,3);
		BFLY2(1,1);
		BFLY2(7,1);
		BFLY2(9,1);
		BFLY2(15,1);
		BFLY2(3,2);
		BFLY2(5,2);
		BFLY2(11,2);
		BFLY2(13,2);
		*pathPointer++ = dec;

		dec = 0;
		BFLY2(19,0);
		BFLY2(21,0);
		BFLY2(27,0);
		BFLY2(29,0);
		BFLY2(17,3);
		BFLY2(23,3);
		BFLY2(25,3);
		BFLY2(31,3);
		BFLY2(18,1);
		BFLY2(20,1);
		BFLY2(26,1);
		BFLY2(28,1);
		BFLY2(16,2);
		BFLY2(22,2);
		BFLY2(24,2);
		BFLY2(30,2);
		*pathPointer++ = dec;
	}

	/* trace back the second block (the decode is saved for test purpose) */
	endState = 0;
	memset(addData,0,(numOutBits+7)/8);
	for(i=numOutBits-1;i >= 0;i--)
	{
		pathPointer -= 2;
		addData[i>>3] |= (endState & 0x01)<<(7-(i&7));

		if(pathPointer[endState >> 5] & (1 << (endState & 31)))
		{
			endState |= 64;	/* 2^(K-1) */
		}
		endState >>= 1;
	}
	/* trace back and decode the first block */
	memset(decData,0,(numOutBits+7)/8); /* round up in case nbits % 8 != 0 */
	pathPointer = &(paths1[blocksize]);
	for(i=numOutBits-1;i >= 0;i--)
	{
		pathPointer -= 2;
		decData[i>>3] |= (endState & 0x01)<<(7-(i&7));

		if(pathPointer[endState >> 5] & (1 << (endState & 31)))
		{
			endState |= 64;	/* 2^(K-1) */
		}
		endState >>= 1;
	}

	/* returns state metrics */
	ArrayCopy((char *)cMet,(char *)stateMetric,64*sizeof(long));

	return SUCCESS_RETURN;
}
