/**
 * @file sigeV3.c
 *
 * @author Mario Nicola 
 *
 * @date 15/07/2015
 *
 * @version 1.0.0
 *
 * @brief Function sets to manipulate in a fast way (based on SIMD instructions) the data
 *        conditioning. Used when SIGE V3 front-end is connected to receiver.
 */

#include "ngeneConstants.h"
#include "simd.h"

/* global variable for the backup of XMM registers */
extern char *ngeneXmmsBackup;

static char ansiTable[4] = {1,-1,3,-3};
static char mask1[16] = {0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03};
static char mask2[16] = {0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01};


void SigeV3SamplesElab(char *inputArray, char *outputArray, int arrayLength)
{
	unsigned int inputOffset, outputOffset;
	int index = 0;

	/* BACKUP */
	movdqa_r2m(xmm0,ngeneXmmsBackup[XMM_REGISTER_SIZE*0]);
	movdqa_r2m(xmm1,ngeneXmmsBackup[XMM_REGISTER_SIZE*1]);
	movdqa_r2m(xmm2,ngeneXmmsBackup[XMM_REGISTER_SIZE*2]);
	movdqa_r2m(xmm6,ngeneXmmsBackup[XMM_REGISTER_SIZE*6]);
	movdqa_r2m(xmm7,ngeneXmmsBackup[XMM_REGISTER_SIZE*7]);
	/**/

	movdqu_m2r(mask1[0],xmm7);
	movdqu_m2r(mask2[0],xmm6);

	/* starts elaboration and aligns output array to a 16 bytes boundary */
	#ifdef COMPILE_64_BIT
   	outputOffset = (unsigned long long int)outputArray & 0x0f;
	#else
	outputOffset = (unsigned int)outputArray & 0x0f;
	#endif
	if(outputOffset!=0)
	{
		for(index=0;(index<((int)16-(int)outputOffset)) && (index<arrayLength);index++)
		{
			outputArray[index] = ansiTable[inputArray[index] & 0x03];
		}
	}

	#ifdef COMPILE_64_BIT
	inputOffset = (unsigned long long int)(&(inputArray[index])) & 0x0f;
	#else
	inputOffset = (unsigned int)(&(inputArray[index])) & 0x0f;
	#endif
	/**/
	if(inputOffset==0)
	{
		while((index+16)<=arrayLength) /* if input array is aligned to a 16 bytes boundary, movdqa_m2r is used */
		{
			movdqa_m2r(inputArray[index],xmm0);
			pand_r2r(xmm7,xmm0);
			punpckhbw_r2r(xmm0,xmm1);
			punpcklbw_r2r(xmm0,xmm2);
			psllw_i2r(7,xmm1);
			psllw_i2r(7,xmm2);
			psraw_i2r(15,xmm1);
			psraw_i2r(15,xmm2);
			packsswb_r2r(xmm1,xmm2);
			pxor_r2r(xmm2,xmm0);
			paddb_r2r(xmm6,xmm0);
			movdqa_r2m(xmm0,outputArray[index]);
			index+=16;
		}
	}
	else /* if input array is not aligned to a 16 bytes boundary, movdqu_m2r is used */
	{
		while((index+16)<=arrayLength)
		{
			movdqu_m2r(inputArray[index],xmm0);
			pand_r2r(xmm7,xmm0);
			punpckhbw_r2r(xmm0,xmm1);
			punpcklbw_r2r(xmm0,xmm2);
			psllw_i2r(7,xmm1);
			psllw_i2r(7,xmm2);
			psraw_i2r(15,xmm1);
			psraw_i2r(15,xmm2);
			packsswb_r2r(xmm1,xmm2);
			pxor_r2r(xmm2,xmm0);
			paddb_r2r(xmm6,xmm0);
			movdqa_r2m(xmm0,outputArray[index]);
			index+=16;
		}
	}
	/**/
	while(index<arrayLength)
	{
		outputArray[index] = ansiTable[inputArray[index] & 0x03];
		index++;
	}
	
	/* RESTORE */
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*0],xmm0);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*1],xmm1);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*2],xmm2);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*6],xmm6);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*7],xmm7);
}

