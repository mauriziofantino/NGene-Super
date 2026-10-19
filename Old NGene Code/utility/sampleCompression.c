/**
 * @file sampleCompression.c
 *
 * @author Mario Nicola
 *
 * @date 09/06/2012
 *
 * @version 1.0.0
 *
 * @brief Function used to perform sample compression.
 */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include "ioFunctions.h"
#include "closeManager.h"
#include "ngeneConfiguration.h"

/* Following macros perform accumulation operation in function of the number of samples to added together */
#define ACCUMULATE_02(v) (v[0]+v[1])
#define ACCUMULATE_03(v) (v[0]+v[1]+v[2])
#define ACCUMULATE_04(v) (v[0]+v[1]+v[2]+v[3])
#define ACCUMULATE_05(v) (v[0]+v[1]+v[2]+v[3]+v[4])
#define ACCUMULATE_06(v) (v[0]+v[1]+v[2]+v[3]+v[4]+v[5])
#define ACCUMULATE_07(v) (v[0]+v[1]+v[2]+v[3]+v[4]+v[5]+v[6])
#define ACCUMULATE_08(v) (v[0]+v[1]+v[2]+v[3]+v[4]+v[5]+v[6]+v[7])
#define ACCUMULATE_09(v) (v[0]+v[1]+v[2]+v[3]+v[4]+v[5]+v[6]+v[7]+v[8])
#define ACCUMULATE_10(v) (v[0]+v[1]+v[2]+v[3]+v[4]+v[5]+v[6]+v[7]+v[8]+v[9])
#define ACCUMULATE_11(v) (v[0]+v[1]+v[2]+v[3]+v[4]+v[5]+v[6]+v[7]+v[8]+v[9]+v[10])
#define ACCUMULATE_12(v) (v[0]+v[1]+v[2]+v[3]+v[4]+v[5]+v[6]+v[7]+v[8]+v[9]+v[10]+v[11])
#define ACCUMULATE_13(v) (v[0]+v[1]+v[2]+v[3]+v[4]+v[5]+v[6]+v[7]+v[8]+v[9]+v[10]+v[11]+v[12])
#define ACCUMULATE_14(v) (v[0]+v[1]+v[2]+v[3]+v[4]+v[5]+v[6]+v[7]+v[8]+v[9]+v[10]+v[11]+v[12]+v[13])
#define ACCUMULATE_15(v) (v[0]+v[1]+v[2]+v[3]+v[4]+v[5]+v[6]+v[7]+v[8]+v[9]+v[10]+v[11]+v[12]+v[13]+v[14])
#define ACCUMULATE_16(v) (v[0]+v[1]+v[2]+v[3]+v[4]+v[5]+v[6]+v[7]+v[8]+v[9]+v[10]+v[11]+v[12]+v[13]+v[14]+v[15])
#define ACCUMULATE_17(v) (v[0]+v[1]+v[2]+v[3]+v[4]+v[5]+v[6]+v[7]+v[8]+v[9]+v[10]+v[11]+v[12]+v[13]+v[14]+v[15]+v[16])
#define ACCUMULATE_18(v) (v[0]+v[1]+v[2]+v[3]+v[4]+v[5]+v[6]+v[7]+v[8]+v[9]+v[10]+v[11]+v[12]+v[13]+v[14]+v[15]+v[16]+v[17])
#define ACCUMULATE_19(v) (v[0]+v[1]+v[2]+v[3]+v[4]+v[5]+v[6]+v[7]+v[8]+v[9]+v[10]+v[11]+v[12]+v[13]+v[14]+v[15]+v[16]+v[17]+v[18])
#define ACCUMULATE_20(v) (v[0]+v[1]+v[2]+v[3]+v[4]+v[5]+v[6]+v[7]+v[8]+v[9]+v[10]+v[11]+v[12]+v[13]+v[14]+v[15]+v[16]+v[17]+v[18]+v[19])
#define ACCUMULATE_52(v) (v[0]+v[1]+v[2]+v[3]+v[4]+v[5]+v[6]+v[7]+v[8]+v[9]+v[10]+v[11]+v[12]+v[13]+v[14]+v[15]+v[16]+v[17]+v[18]+v[19]+v[20]+v[21]+v[22]+v[23]+v[24]+v[25]+v[26]+v[27]+v[28]+v[29]+v[30]+v[31]+v[32]+v[33]+v[34]+v[35]+v[36]+v[37]+v[38]+v[39]+v[40]+v[41]+v[42]+v[43]+v[44]+v[45]+v[46]+v[47]+v[48]+v[49]+v[50]+v[51])


/**
 * @def COMPRESSION_NCO_SCALING
 * @brief In order to manage NCO as a integer number,
 *        initial value and increment are left shifted of
 *        COMPRESSION_NCO_SCALING bits
 */
#define COMPRESSION_NCO_SCALING 	16

/**
 * @def  COMPRESSION_SAMPLE_SCALING
 * @brief Each output samples is right shifted of COMPRESSION_SAMPLE_SCALING bits
 */
#define COMPRESSION_SAMPLE_SCALING  8

unsigned int SampleCompression(unsigned int initialNco, unsigned int offset, char *real, char *imag, unsigned int numberOfInputs, short *s, unsigned int numberOfOutputs)
{
	unsigned int shortAccum;
	unsigned int longAccum;
	unsigned int accum;

	unsigned int remNco = initialNco;
	unsigned int limit = (unsigned int)((double)numberOfInputs*(1<<COMPRESSION_NCO_SCALING)/(double)numberOfOutputs);
	unsigned int increment1 = (1<<COMPRESSION_NCO_SCALING);
	unsigned int incrementShortAccum;

	int index = numberOfOutputs;
	char *realPtr = &(real[offset]);
	char *imagPtr = &(imag[offset]);
	short *outPtr = s;
	int sampleIndex;

	if(numberOfInputs<=numberOfOutputs)
	{
		return 0;
	}

	shortAccum = numberOfInputs/numberOfOutputs;
	longAccum = shortAccum +1;
	incrementShortAccum = shortAccum * increment1;

	while(index>0)
	{
		remNco += incrementShortAccum;
		if(remNco>=limit)
		{
			accum = shortAccum;
		}
		else
		{
			remNco += increment1;
			accum = longAccum;
		}
		remNco -= limit;

		switch (accum)
		{
			case 1:
				outPtr[0] = (short)realPtr[0];
				outPtr[1] = (short)imagPtr[0];
				break;
			case 2:
				outPtr[0] = ACCUMULATE_02(realPtr);
				outPtr[1] = ACCUMULATE_02(imagPtr);
				break;
			case 3:
				outPtr[0] = ACCUMULATE_03(realPtr);
				outPtr[1] = ACCUMULATE_03(imagPtr);
				break;
			case 4:
				outPtr[0] = ACCUMULATE_04(realPtr);
				outPtr[1] = ACCUMULATE_04(imagPtr);
				break;
			case 5:
				outPtr[0] = ACCUMULATE_05(realPtr);
				outPtr[1] = ACCUMULATE_05(imagPtr);
				break;
			case 6:
				outPtr[0] = ACCUMULATE_06(realPtr);
				outPtr[1] = ACCUMULATE_06(imagPtr);
				break;
			case 7:
				outPtr[0] = ACCUMULATE_07(realPtr);
				outPtr[1] = ACCUMULATE_07(imagPtr);
				break;
			case 8:
				outPtr[0] = ACCUMULATE_08(realPtr);
				outPtr[1] = ACCUMULATE_08(imagPtr);
				break;
			case 9:
				outPtr[0] = ACCUMULATE_09(realPtr);
				outPtr[1] = ACCUMULATE_09(imagPtr);
				break;
			case 10:
				outPtr[0] = ACCUMULATE_10(realPtr);
				outPtr[1] = ACCUMULATE_10(imagPtr);
				break;
			case 11:
				outPtr[0] = ACCUMULATE_11(realPtr);
				outPtr[1] = ACCUMULATE_11(imagPtr);
				break;
			case 12:
				outPtr[0] = ACCUMULATE_12(realPtr);
				outPtr[1] = ACCUMULATE_12(imagPtr);
				break;
			case 13:
				outPtr[0] = ACCUMULATE_13(realPtr);
				outPtr[1] = ACCUMULATE_13(imagPtr);
			break;
			case 14:
				outPtr[0] = ACCUMULATE_14(realPtr);
				outPtr[1] = ACCUMULATE_14(imagPtr);
				break;
			case 15:
				outPtr[0] = ACCUMULATE_15(realPtr);
				outPtr[1] = ACCUMULATE_15(imagPtr);
				break;
			case 16:
				outPtr[0] = ACCUMULATE_16(realPtr);
				outPtr[1] = ACCUMULATE_16(imagPtr);
				break;
			case 17:
				outPtr[0] = ACCUMULATE_17(realPtr);
				outPtr[1] = ACCUMULATE_17(imagPtr);
				break;
			case 18:
				outPtr[0] = ACCUMULATE_18(realPtr);
				outPtr[1] = ACCUMULATE_18(imagPtr);
				break;
			case 19:
				outPtr[0] = ACCUMULATE_19(realPtr);
				outPtr[1] = ACCUMULATE_19(imagPtr);
				break;
			case 20:
				outPtr[0] = ACCUMULATE_20(realPtr);
				outPtr[1] = ACCUMULATE_20(imagPtr);
				break;
			case 52:
				outPtr[0] = ACCUMULATE_52(realPtr);
				outPtr[1] = ACCUMULATE_52(imagPtr);
				break;
			default:
				outPtr[0] = 0;
				outPtr[1] = 0;
				for(sampleIndex=0;(unsigned int)sampleIndex<accum;sampleIndex++)
				{
					outPtr[0] += (short)(realPtr[sampleIndex]);
					outPtr[1] += (short)(imagPtr[sampleIndex]);
				}
				//SwrxLog(NGENE_LOG_SEVERITY_ERROR, "ERROR: unsupported sampling frequency set in downsample/accumulate unit (led to a compression factor equal to %d)",accum);
				//CloseNGene(cfg,EXIT_FAILURE);
			    break;
		}
		outPtr[0] <<= COMPRESSION_SAMPLE_SCALING;
		outPtr[1] <<= COMPRESSION_SAMPLE_SCALING;
		realPtr += accum;
		imagPtr += accum;
		outPtr += 2;
		index--;
	}

	return remNco;
}
