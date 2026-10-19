/**
 * @file jrc.c
 *
 * @author Mario Nicola
 *
 * @date 29/04/2015
 *
 * @version 1.0.0
 *
 * @brief This function performs in a fast way the data conditioning.
 *        Used when a file saved from an unknown FE used by JRC front-end is elaborated
 *        (seemingly a USRP with one sample I or Q per byte).
 *
 * @note It is currently based on SIMD instructions
 */

#include "jrc.h"

#include "ngeneConstants.h"
#include "simd.h"
#include "data_shift.h"
#include <stdio.h>

/* global variable for the backup of XMM registers */
extern char *ngeneXmmsBackup;

static char  jrcMask[20]  = {0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff};

void JrcSamplesElab(char *inputArray, char *outputArray, int arrayLength, int *maskOffsetPtr, int shift)
{
	int maskOffset = *maskOffsetPtr;
	unsigned int inputOffset, outputOffset;
	int index = 0;
	char *jrcMaskLocal = &(jrcMask[maskOffset]);

	/* BACKUP */
	movdqa_r2m(xmm0,ngeneXmmsBackup[XMM_REGISTER_SIZE*0]);
	movdqa_r2m(xmm7,ngeneXmmsBackup[XMM_REGISTER_SIZE*7]);
	/**/

	/* perform the shift for all the samples, both I and Q part */
	if(shift>0)
		RightShiftByte(inputArray,inputArray,arrayLength,shift);

	/* insert IF modulation */
	/* starts elaboration and aligns output array to a 16 bytes boundary */
	#ifdef COMPILE_64_BIT
	outputOffset = (unsigned long long int)outputArray & 0x0f;
	#else
	outputOffset = (unsigned int)outputArray & 0x0f;
	#endif
	if(outputOffset!=0)
	{
		for(index=0;index<((int)16-(int)outputOffset) && index<arrayLength;index++)
		{
			outputArray[index] = ((inputArray[index]^jrcMask[(index+maskOffset)%4])-jrcMask[(index+maskOffset)%4]);
		}
	}

	jrcMaskLocal = &(jrcMask[(maskOffset+index) & 0x03]);
	movdqu_m2r(jrcMaskLocal[0],xmm7);

	#ifdef COMPILE_64_BIT
	inputOffset = (unsigned long long int)(&(inputArray[index])) & 0x0f;
	#else
	inputOffset = (unsigned int)(&(inputArray[index])) & 0x0f;
	#endif
	/**/
	if(inputOffset == 0) /* if input array is aligned to a 16 bytes boundary, movdqa_m2r is used */
	{
		while((index+16)<=arrayLength)
		{
			movdqa_m2r(inputArray[index],  xmm0);
			pxor_r2r(xmm7,xmm0);
			psubb_r2r(xmm7,xmm0);
			movdqa_r2m(xmm0,outputArray[index]);
			index += 16;
		}
	}
	else /* if input array is not aligned to a 16 bytes boundary, movdqu_m2r is used */
	{
		while((index+16)<=arrayLength)
		{
			movdqu_m2r(inputArray[index],  xmm0);
			pxor_r2r(xmm7,xmm0);
			psubb_r2r(xmm7,xmm0);
			movdqa_r2m(xmm0,outputArray[index]);
			index += 16;
		}
	}
	/**/
	for(;index<arrayLength;index++)
	{
		outputArray[index] = ((inputArray[index]^jrcMask[(index+maskOffset)%4])-jrcMask[(index+maskOffset)%4]);
	}

	/* update the offset index for the mask */
	*maskOffsetPtr = (maskOffset + arrayLength) & 0x03;
	
	/* RESTORE */
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*0],xmm0);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*7],xmm7);
}

