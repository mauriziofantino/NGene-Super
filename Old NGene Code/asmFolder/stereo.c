/**
 * @file stereo.c
 *
 * @author Mario Nicola
 *
 * @date 23/09/2014
 *
 * @version 1.0.0
 *
 * @brief Function used to manipulate in a fast way the data conditioning.
 *        Used when NVS Stereo front-end is connected to receiver.
 *
 * @note It is currently based on SIMD instructions
 */

#include "ngeneConstants.h"
#include "simd.h"

/* global variable for the backup of XMM registers */
extern char *ngeneXmmsBackup;

static char stereoMask1[XMM_REGISTER_SIZE] = {0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03};
static char stereoMask2[XMM_REGISTER_SIZE] = {0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0};

void StereoSamplesElab(char *inputArray, char *outputArray, int arrayLength)
{
	unsigned int inputOffset, outputOffset;
	int index = 0;
	char mask = 0x06;

	/* BACKUP */
	movdqa_r2m(xmm0,ngeneXmmsBackup[XMM_REGISTER_SIZE*0]);
	movdqa_r2m(xmm1,ngeneXmmsBackup[XMM_REGISTER_SIZE*1]);
	movdqa_r2m(xmm2,ngeneXmmsBackup[XMM_REGISTER_SIZE*2]);
	movdqa_r2m(xmm3,ngeneXmmsBackup[XMM_REGISTER_SIZE*3]);
	movdqa_r2m(xmm4,ngeneXmmsBackup[XMM_REGISTER_SIZE*4]);
	movdqa_r2m(xmm5,ngeneXmmsBackup[XMM_REGISTER_SIZE*5]);
	movdqa_r2m(xmm6,ngeneXmmsBackup[XMM_REGISTER_SIZE*6]);
	movdqa_r2m(xmm7,ngeneXmmsBackup[XMM_REGISTER_SIZE*7]);
	/**/

	/* starts elaboration and aligns output array to a 16 bytes boundary */
	#ifdef COMPILE_64_BIT
	outputOffset = (unsigned long long int)outputArray & 0x0f;
	#else
	outputOffset = (unsigned int)outputArray & 0x0f;
	#endif
	if(outputOffset!=0)
	{
		for(index=0; index<((int)XMM_REGISTER_SIZE-(int)outputOffset) && index<arrayLength; index++)
		{
			outputArray[index] = inputArray[index] >> 5;
			outputArray[index] = outputArray[index] & mask;
			outputArray[index] = outputArray[index] - 3;
		}
	}

	movdqu_m2r(stereoMask1[0], xmm6); /* we have to subtract 3 in order to properly manage all the samples */
	movdqu_m2r(stereoMask2[0], xmm7); /* to adjust data properly --> 1100 0000 1100 0000 = C0C0 */

	#ifdef COMPILE_64_BIT
	inputOffset = (unsigned long long int)(&(inputArray[index])) & 0x0f;
	#else
	inputOffset = (unsigned int)(&(inputArray[index])) & 0x0f;
	#endif

	if(inputOffset == 0) /* if input array is aligned to a 16 bytes boundary, movdqa_m2r is used */
	{
		while((index+96) <= arrayLength)
		{
			/* copy data from memory to registers */
			movdqa_m2r(inputArray[index]   ,xmm0);
			movdqa_m2r(inputArray[index+16],xmm1);
			movdqa_m2r(inputArray[index+32],xmm2);
			movdqa_m2r(inputArray[index+48],xmm3);
			movdqa_m2r(inputArray[index+64],xmm4);
			movdqa_m2r(inputArray[index+80],xmm5);
			/* and */
			pand_r2r(xmm7,xmm0);
			pand_r2r(xmm7,xmm1);
			pand_r2r(xmm7,xmm2);
			pand_r2r(xmm7,xmm3);
			pand_r2r(xmm7,xmm4);
			pand_r2r(xmm7,xmm5);
			/* logical shift */
			psrlw_i2r(5, xmm0); /* instead shifting 6 and multiply by 2. */
			psrlw_i2r(5, xmm1);
			psrlw_i2r(5, xmm2);
			psrlw_i2r(5, xmm3);
			psrlw_i2r(5, xmm4);
			psrlw_i2r(5, xmm5);
			/* subtract */
			psubb_r2r(xmm6, xmm0);
			psubb_r2r(xmm6, xmm1);
			psubb_r2r(xmm6, xmm2);
			psubb_r2r(xmm6, xmm3);
			psubb_r2r(xmm6, xmm4);
			psubb_r2r(xmm6, xmm5);
			/* copy to memory */
			movdqa_r2m(xmm0,outputArray[index]);
			movdqa_r2m(xmm1,outputArray[index+16]);
			movdqa_r2m(xmm2,outputArray[index+32]);
			movdqa_r2m(xmm3,outputArray[index+48]);
			movdqa_r2m(xmm4,outputArray[index+64]);
			movdqa_r2m(xmm5,outputArray[index+80]);
			/* update index */
			index += 96;
		}
		if((index+80) <= arrayLength)
		{
			movdqa_m2r(inputArray[index]   ,xmm0);
			movdqa_m2r(inputArray[index+16],xmm1);
			movdqa_m2r(inputArray[index+32],xmm2);
			movdqa_m2r(inputArray[index+48],xmm3);
			movdqa_m2r(inputArray[index+64],xmm4);
			pand_r2r(xmm7,xmm0);
			pand_r2r(xmm7,xmm1);
			pand_r2r(xmm7,xmm2);
			pand_r2r(xmm7,xmm3);
			pand_r2r(xmm7,xmm4);
			psrlw_i2r(5, xmm0);
			psrlw_i2r(5, xmm1);
			psrlw_i2r(5, xmm2);
			psrlw_i2r(5, xmm3);
			psrlw_i2r(5, xmm4);
			psubb_r2r(xmm6, xmm0);
			psubb_r2r(xmm6, xmm1);
			psubb_r2r(xmm6, xmm2);
			psubb_r2r(xmm6, xmm3);
			psubb_r2r(xmm6, xmm4);
			movdqa_r2m(xmm0,outputArray[index]);
			movdqa_r2m(xmm1,outputArray[index+16]);
			movdqa_r2m(xmm2,outputArray[index+32]);
			movdqa_r2m(xmm3,outputArray[index+48]);
			movdqa_r2m(xmm4,outputArray[index+64]);
			index += 80;
		}
		if((index+64) <= arrayLength)
		{
			movdqa_m2r(inputArray[index]   ,xmm0);
			movdqa_m2r(inputArray[index+16],xmm1);
			movdqa_m2r(inputArray[index+32],xmm2);
			movdqa_m2r(inputArray[index+48],xmm3);
			pand_r2r(xmm7,xmm0);
			pand_r2r(xmm7,xmm1);
			pand_r2r(xmm7,xmm2);
			pand_r2r(xmm7,xmm3);
			psrlw_i2r(5, xmm0);
			psrlw_i2r(5, xmm1);
			psrlw_i2r(5, xmm2);
			psrlw_i2r(5, xmm3);
			psubb_r2r(xmm6, xmm0);
			psubb_r2r(xmm6, xmm1);
			psubb_r2r(xmm6, xmm2);
			psubb_r2r(xmm6, xmm3);
			movdqa_r2m(xmm0,outputArray[index]);
			movdqa_r2m(xmm1,outputArray[index+16]);
			movdqa_r2m(xmm2,outputArray[index+32]);
			movdqa_r2m(xmm3,outputArray[index+48]);
			index += 64;
		}
		if((index+48) <= arrayLength)
		{
			movdqa_m2r(inputArray[index]   ,xmm0);
			movdqa_m2r(inputArray[index+16],xmm1);
			movdqa_m2r(inputArray[index+32],xmm2);
			pand_r2r(xmm7,xmm0);
			pand_r2r(xmm7,xmm1);
			pand_r2r(xmm7,xmm2);
			psrlw_i2r(5, xmm0);
			psrlw_i2r(5, xmm1);
			psrlw_i2r(5, xmm2);
			psubb_r2r(xmm6, xmm0);
			psubb_r2r(xmm6, xmm1);
			psubb_r2r(xmm6, xmm2);
			movdqa_r2m(xmm0,outputArray[index]);
			movdqa_r2m(xmm1,outputArray[index+16]);
			movdqa_r2m(xmm2,outputArray[index+32]);
			index += 48;
		}
		if((index+32) <= arrayLength)
		{
			movdqa_m2r(inputArray[index]   ,xmm0);
			movdqa_m2r(inputArray[index+16],xmm1);
			pand_r2r(xmm7,xmm0);
			pand_r2r(xmm7,xmm1);
			psrlw_i2r(5, xmm0);
			psrlw_i2r(5, xmm1);
			psubb_r2r(xmm6, xmm0);
			psubb_r2r(xmm6, xmm1);
			movdqa_r2m(xmm0,outputArray[index]);
			movdqa_r2m(xmm1,outputArray[index+16]);
			index += 32;
		}
		if((index+16) <= arrayLength)
		{
			movdqa_m2r(inputArray[index]   ,xmm0);
			pand_r2r(xmm7,xmm0);
			psrlw_i2r(5, xmm0);
			psubb_r2r(xmm6, xmm0);
			movdqa_r2m(xmm0,outputArray[index]);
			index += 16;
		}
	}
	else
	{
		/* if input array is not aligned to a 16 bytes boundary, movdqu_m2r is used */
		while((index+96) <= arrayLength)
		{
			movdqu_m2r(inputArray[index]   ,xmm0);
			movdqu_m2r(inputArray[index+16],xmm1);
			movdqu_m2r(inputArray[index+32],xmm2);
			movdqu_m2r(inputArray[index+48],xmm3);
			movdqu_m2r(inputArray[index+64],xmm4);
			movdqu_m2r(inputArray[index+80],xmm5);
			pand_r2r(xmm7,xmm0);
			pand_r2r(xmm7,xmm1);
			pand_r2r(xmm7,xmm2);
			pand_r2r(xmm7,xmm3);
			pand_r2r(xmm7,xmm4);
			pand_r2r(xmm7,xmm5);
			psrlw_i2r(5, xmm0);
			psrlw_i2r(5, xmm1);
			psrlw_i2r(5, xmm2);
			psrlw_i2r(5, xmm3);
			psrlw_i2r(5, xmm4);
			psrlw_i2r(5, xmm5);
			psubb_r2r(xmm6, xmm0);
			psubb_r2r(xmm6, xmm1);
			psubb_r2r(xmm6, xmm2);
			psubb_r2r(xmm6, xmm3);
			psubb_r2r(xmm6, xmm4);
			psubb_r2r(xmm6, xmm5);
			movdqa_r2m(xmm0,outputArray[index]);
			movdqa_r2m(xmm1,outputArray[index+16]);
			movdqa_r2m(xmm2,outputArray[index+32]);
			movdqa_r2m(xmm3,outputArray[index+48]);
			movdqa_r2m(xmm4,outputArray[index+64]);
			movdqa_r2m(xmm5,outputArray[index+80]);
			index += 96;
		}
		if((index+80) <= arrayLength)
		{
			movdqu_m2r(inputArray[index]   ,xmm0);
			movdqu_m2r(inputArray[index+16],xmm1);
			movdqu_m2r(inputArray[index+32],xmm2);
			movdqu_m2r(inputArray[index+48],xmm3);
			movdqu_m2r(inputArray[index+64],xmm4);
			pand_r2r(xmm7,xmm0);
			pand_r2r(xmm7,xmm1);
			pand_r2r(xmm7,xmm2);
			pand_r2r(xmm7,xmm3);
			pand_r2r(xmm7,xmm4);
			psrlw_i2r(5, xmm0);
			psrlw_i2r(5, xmm1);
			psrlw_i2r(5, xmm2);
			psrlw_i2r(5, xmm3);
			psrlw_i2r(5, xmm4);
			psubb_r2r(xmm6, xmm0);
			psubb_r2r(xmm6, xmm1);
			psubb_r2r(xmm6, xmm2);
			psubb_r2r(xmm6, xmm3);
			psubb_r2r(xmm6, xmm4);
			movdqa_r2m(xmm0,outputArray[index]);
			movdqa_r2m(xmm1,outputArray[index+16]);
			movdqa_r2m(xmm2,outputArray[index+32]);
			movdqa_r2m(xmm3,outputArray[index+48]);
			movdqa_r2m(xmm4,outputArray[index+64]);
			index += 80;
		}
		if((index+64) <= arrayLength)
		{
			movdqu_m2r(inputArray[index]   ,xmm0);
			movdqu_m2r(inputArray[index+16],xmm1);
			movdqu_m2r(inputArray[index+32],xmm2);
			movdqu_m2r(inputArray[index+48],xmm3);
			pand_r2r(xmm7,xmm0);
			pand_r2r(xmm7,xmm1);
			pand_r2r(xmm7,xmm2);
			pand_r2r(xmm7,xmm3);
			psrlw_i2r(5, xmm0);
			psrlw_i2r(5, xmm1);
			psrlw_i2r(5, xmm2);
			psrlw_i2r(5, xmm3);
			psubb_r2r(xmm6, xmm0);
			psubb_r2r(xmm6, xmm1);
			psubb_r2r(xmm6, xmm2);
			psubb_r2r(xmm6, xmm3);
			movdqa_r2m(xmm0,outputArray[index]);
			movdqa_r2m(xmm1,outputArray[index+16]);
			movdqa_r2m(xmm2,outputArray[index+32]);
			movdqa_r2m(xmm3,outputArray[index+48]);
			index += 64;
		}
		if((index+48) <= arrayLength)
		{
			movdqu_m2r(inputArray[index]   ,xmm0);
			movdqu_m2r(inputArray[index+16],xmm1);
			movdqu_m2r(inputArray[index+32],xmm2);
			pand_r2r(xmm7,xmm0);
			pand_r2r(xmm7,xmm1);
			pand_r2r(xmm7,xmm2);
			psrlw_i2r(5, xmm0);
			psrlw_i2r(5, xmm1);
			psrlw_i2r(5, xmm2);
			psubb_r2r(xmm6, xmm0);
			psubb_r2r(xmm6, xmm1);
			psubb_r2r(xmm6, xmm2);
			movdqa_r2m(xmm0,outputArray[index]);
			movdqa_r2m(xmm1,outputArray[index+16]);
			movdqa_r2m(xmm2,outputArray[index+32]);
			index += 48;
		}
		if((index+32) <= arrayLength)
		{
			movdqu_m2r(inputArray[index]   ,xmm0);
			movdqu_m2r(inputArray[index+16],xmm1);
			pand_r2r(xmm7,xmm0);
			pand_r2r(xmm7,xmm1);
			psrlw_i2r(5, xmm0);
			psrlw_i2r(5, xmm1);
			psubb_r2r(xmm6, xmm0);
			psubb_r2r(xmm6, xmm1);
			movdqa_r2m(xmm0,outputArray[index]);
			movdqa_r2m(xmm1,outputArray[index+16]);
			index += 32;
		}
		if((index+16) <= arrayLength)
		{
			movdqu_m2r(inputArray[index]   ,xmm0);
			pand_r2r(xmm7,xmm0);
			psrlw_i2r(5, xmm0);
			psubb_r2r(xmm6, xmm0);
			movdqa_r2m(xmm0,outputArray[index]);
			index += 16;
		}
	}

	/* elaborates remaining samples */
	for(;index < arrayLength; index++)
	{
		outputArray[index] = inputArray[index] >> 5;
		outputArray[index] = outputArray[index] & mask;
		outputArray[index] = outputArray[index] - 3;
	}

	/* RESTORE */
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*0],xmm0);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*1],xmm1);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*2],xmm2);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*3],xmm3);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*4],xmm4);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*5],xmm5);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*6],xmm6);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*7],xmm7);
}


