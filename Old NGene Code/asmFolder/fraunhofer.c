/**
 * @file fraunhofer.c
 *
 * @author Mario Nicola
 *
 * @date 04/07/2008
 *
 * @version 1.0.0
 *
 * @brief Function sets to manipulate in a fast way (based on SIMD instructions) the data
 *        conditioning. Used when Fraunhofer front-end is connected to receiver.
 */
#include "ngeneConstants.h"
#include "simd.h"
#include "fraunhofer.h"

#include <stdio.h>


/* global variable for the backup of XMM registers */
extern char *ngeneXmmsBackup;


void FraunhoferSamplesElab(char *inputArray, char *outputArray, int arrayLength)
{
	unsigned int inputOffset, outputOffset;
	int index = 0;

	/* BACKUP */
	movdqa_r2m(xmm0,ngeneXmmsBackup[XMM_REGISTER_SIZE*0]);
	movdqa_r2m(xmm1,ngeneXmmsBackup[XMM_REGISTER_SIZE*1]);
	movdqa_r2m(xmm2,ngeneXmmsBackup[XMM_REGISTER_SIZE*2]);
	movdqa_r2m(xmm3,ngeneXmmsBackup[XMM_REGISTER_SIZE*3]);
	movdqa_r2m(xmm4,ngeneXmmsBackup[XMM_REGISTER_SIZE*4]);
	movdqa_r2m(xmm5,ngeneXmmsBackup[XMM_REGISTER_SIZE*5]);
	/**/

	/*
		for the algorithm see comments in data_shift.function file. Here a left shift of
		4 bits has been added in order to delete L2 bits. Following right shift is of
		12 bits (fixed amount).
	*/

	#ifdef COMPILE_64_BIT
	outputOffset = (unsigned long long int)outputArray & 0x0f;
	#else
	outputOffset = (unsigned int)outputArray & 0x0f;
	#endif
	if(outputOffset!=0)
	{
		for(index=0;index<((int)16-(int)outputOffset) && index<arrayLength;index++)
		{
			outputArray[index] = inputArray[index] << 4;
			outputArray[index] = outputArray[index] >> 4;
		}
	}
	/**/
	#ifdef COMPILE_64_BIT
	inputOffset = (unsigned long long int)(&(inputArray[index])) & 0x0f;
	#else
	inputOffset = (unsigned int)(&(inputArray[index])) & 0x0f;
	#endif
	if(inputOffset == 0)
	{
		while((index+48)<=arrayLength)
		{
			punpckhbw_m2r(inputArray[index],xmm0);
			punpcklbw_m2r(inputArray[index],xmm1);
			punpckhbw_m2r(inputArray[index+16],xmm2);
			punpcklbw_m2r(inputArray[index+16],xmm3);
			punpckhbw_m2r(inputArray[index+32],xmm4);
			punpcklbw_m2r(inputArray[index+32],xmm5);
			psllw_i2r(4,xmm0);
			psllw_i2r(4,xmm1);
			psllw_i2r(4,xmm2);
			psllw_i2r(4,xmm3);
			psllw_i2r(4,xmm4);
			psllw_i2r(4,xmm5);
			psraw_i2r(12,xmm0);
			psraw_i2r(12,xmm1);
			psraw_i2r(12,xmm2);
			psraw_i2r(12,xmm3);
			psraw_i2r(12,xmm4);
			psraw_i2r(12,xmm5);
			packsswb_r2r(xmm0, xmm1);
			packsswb_r2r(xmm2, xmm3);
			packsswb_r2r(xmm4, xmm5);
			movdqa_r2m(xmm1,outputArray[index]);
			movdqa_r2m(xmm3,outputArray[index+16]);
			movdqa_r2m(xmm5,outputArray[index+32]);
			index += 48;
		}
		if((index+32)<=arrayLength)
		{
			punpckhbw_m2r(inputArray[index],xmm0);
			punpcklbw_m2r(inputArray[index],xmm1);
			punpckhbw_m2r(inputArray[index+16],xmm2);
			punpcklbw_m2r(inputArray[index+16],xmm3);
			psllw_i2r(4,xmm0);
			psllw_i2r(4,xmm1);
			psllw_i2r(4,xmm2);
			psllw_i2r(4,xmm3);
			psraw_i2r(12,xmm0);
			psraw_i2r(12,xmm1);
			psraw_i2r(12,xmm2);
			psraw_i2r(12,xmm3);
			packsswb_r2r(xmm0, xmm1);
			packsswb_r2r(xmm2, xmm3);
			movdqa_r2m(xmm1,outputArray[index]);
			movdqa_r2m(xmm3,outputArray[index+16]);
			index += 32;
		}
		if((index+16)<=arrayLength)
		{
			punpckhbw_m2r(inputArray[index],xmm0);
			punpcklbw_m2r(inputArray[index],xmm1);
			psllw_i2r(4,xmm0);
			psllw_i2r(4,xmm1);
			psraw_i2r(12,xmm0);
			psraw_i2r(12,xmm1);
			packsswb_r2r(xmm0, xmm1);
			movdqa_r2m(xmm1,outputArray[index]);
			index += 16;
		}
	}
	else
	{
		while((index+32)<=arrayLength)
		{
			movdqu_m2r(inputArray[index],xmm2);
			movdqu_m2r(inputArray[index+16],xmm5);
			punpckhbw_r2r(xmm2,xmm0);
			punpcklbw_r2r(xmm2,xmm1);
			punpckhbw_r2r(xmm5,xmm3);
			punpcklbw_r2r(xmm5,xmm4);
			psllw_i2r(4,xmm0);
			psllw_i2r(4,xmm1);
			psllw_i2r(4,xmm3);
			psllw_i2r(4,xmm4);
			psraw_i2r(12,xmm0);
			psraw_i2r(12,xmm1);
			psraw_i2r(12,xmm3);
			psraw_i2r(12,xmm4);
			packsswb_r2r(xmm0, xmm1);
			packsswb_r2r(xmm3, xmm4);
			movdqa_r2m(xmm1,outputArray[index]);
			movdqa_r2m(xmm4,outputArray[index+16]);
			index += 32;
		}
		if((index+16)<=arrayLength)
		{
			movdqu_m2r(inputArray[index],xmm2);
			punpckhbw_r2r(xmm2,xmm0);
			punpcklbw_r2r(xmm2,xmm1);
			psllw_i2r(4,xmm0);
			psllw_i2r(4,xmm1);
			psraw_i2r(12,xmm0);
			psraw_i2r(12,xmm1);
			packsswb_r2r(xmm0, xmm1);
			movdqa_r2m(xmm1,outputArray[index]);
			index += 16;
		}
	}

	for(;index<arrayLength;index++)
	{
		outputArray[index] = inputArray[index] << 4;
		outputArray[index] = outputArray[index] >> 4;
	}
	
	/* RESTORE */
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*0],xmm0);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*1],xmm1);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*2],xmm2);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*3],xmm3);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*4],xmm4);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*5],xmm5);
}

