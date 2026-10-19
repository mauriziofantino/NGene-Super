/**
 * @file usrp.c
 *
 * @author Mario Nicola
 *
 * @date 29/04/2015
 *
 * @version 1.0.0
 *
 * @brief This function performs in a fast way the data conditioning.
 *        Used when a file saved from a USRP front-end is elaborated.
 *
 * @note It is currently based on SIMD instructions
 */

#include "usrp.h"

#include "ngeneConstants.h"
#include "simd.h"
#include <stdio.h>
#include <stdlib.h>

/* global variable for the backup of XMM registers */
extern char *ngeneXmmsBackup;

//static short int  stereoTritonMask[XMM_REGISTER_SIZE+4]  = {0x0000, 0x0000, 0xffff, 0xffff, 0x00, 0x00, 0xffff, 0xffff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff};
static short int  stereoTritonMask[12]  = {0x0000, 0x0000, 0xffff, 0xffff, 0x0000, 0x0000, 0xffff, 0xffff, 0x0000, 0x0000, 0xffff, 0xffff};

void StereoTritonSamplesElab(char *inputArray, char *outputArray, int arrayLength, int *maskOffsetPtr, int shift)
{
	int maskOffset = *maskOffsetPtr;
	unsigned int inputOffset, outputOffset;
	int index = 0;
	short int *stereoTritonMaskLocal = &(stereoTritonMask[maskOffset]);

	/* BACKUP */
	movdqa_r2m(xmm0,ngeneXmmsBackup[XMM_REGISTER_SIZE*0]);
	movdqa_r2m(xmm1,ngeneXmmsBackup[XMM_REGISTER_SIZE*1]);
	movdqa_r2m(xmm2,ngeneXmmsBackup[XMM_REGISTER_SIZE*2]);
	movdqa_r2m(xmm7,ngeneXmmsBackup[XMM_REGISTER_SIZE*7]);
	/**/

	/**/
	/*FILE *inputSamples;
	char filename_inputSamples[128];
	sprintf(filename_inputSamples,"InputSamples.txt");
	FILE *outputSamples;
	char filename_outputSamples[128];
	sprintf(filename_outputSamples,"OutputSamples.txt");
	int indexSave = 0;*/
	/**/


	switch(shift)
	{
		case 0:
			#undef SHIFTED_BITS
			#include "stereoTriton.function"
    		break;
    	case 1:
    		/**/
			/*inputSamples = fopen(filename_inputSamples,"a");
			for(indexSave=0;indexSave<arrayLength;indexSave++)
			{
			fprintf(inputSamples,"%d\n",inputArray[indexSave]);
			}
			fclose(inputSamples);*/
			/**/
			#define SHIFTED_BITS 	1
    		#include "stereoTriton.function"
			/**/
			/*outputSamples = fopen(filename_outputSamples,"a");
			for(indexSave=0;indexSave<arrayLength;indexSave++)
			{
			fprintf(outputSamples,"%d\n",outputArray[indexSave]);
			}
			fclose(outputSamples);*/
			/**/
			#undef SHIFTED_BITS
    		break;
    	default:
			#define SHIFTED_BITS 	1
			#include "stereoTriton.function"
			#undef SHIFTED_BITS
    		break;
	}
	*maskOffsetPtr = (maskOffset + arrayLength) & 0x03;
	
	/* RESTORE */
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*0],xmm0);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*1],xmm1);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*2],xmm2);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*7],xmm7);
}

