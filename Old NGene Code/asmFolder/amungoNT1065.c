/**
 * @file amungoNT1065.c
 *
 * @author Micaela Troglia Gamba
 *
 * @date 11/04/2017
 *
 * @version 1.0.0
 *
 * @brief Function used to manipulate in a fast way the data conditioning.
 *        Used when Amungo NT1065 front-end is connected to receiver.
 *
 * @note It is currently based on SIMD instructions
 */

#include "ngeneConstants.h"
#include "simd.h"
#include "stdio.h"
#include "stdlib.h"

/* global variable for the backup of XMM registers */
extern char *ngeneXmmsBackup;

static char amungoNT1065Mask1[XMM_REGISTER_SIZE] = {0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02};
static char amungoNT1065Mask2[XMM_REGISTER_SIZE] = {0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01};

/*The following lines are used only for the Intel assembly and not for the ARM assembly*/
static char amungoNT1065Mask3[XMM_REGISTER_SIZE] = {0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF};
static char amungoNT1065Mask4[XMM_REGISTER_SIZE] = {0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00};


static char amungoNT1065_LUT[4]={0x01,0x03,0xFF,0xFD};

void AmungoNT1065SamplesElab(char *inputArray, char *outputArray, int arrayLength)
{
	unsigned int inputOffset, outputOffset;
	int index = 0;
	char mask = 0x03;
	//char sign, module, tmp;

	/* BACKUP */
	movdqa_r2m(xmm0,ngeneXmmsBackup[XMM_REGISTER_SIZE*0]);
	movdqa_r2m(xmm1,ngeneXmmsBackup[XMM_REGISTER_SIZE*1]);
	movdqa_r2m(xmm2,ngeneXmmsBackup[XMM_REGISTER_SIZE*2]);
	movdqa_r2m(xmm6,ngeneXmmsBackup[XMM_REGISTER_SIZE*6]);
	movdqa_r2m(xmm7,ngeneXmmsBackup[XMM_REGISTER_SIZE*7]);
	/**/

	#ifdef DEBUG_AND_VERIFY
	char *cDebug;
	cDebug= malloc(arrayLength*sizeof(char));
	if(cDebug==NULL)
		printf("Error: cDebug has not been correctly allocated\n");
	int index2;
	for(index2=0;index2<arrayLength;index2++)
	{
		cDebug[index2] = amungoNT1065_LUT[inputArray[index2] & 0x03];
	}
	#endif


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
			outputArray[index] = inputArray[index] & mask;
			outputArray[index] = amungoNT1065_LUT[(int)outputArray[index]];
		}
	}

	movdqu_m2r(amungoNT1065Mask1[0], xmm6); /*  */
	movdqu_m2r(amungoNT1065Mask2[0], xmm7); /*  */

	#ifdef COMPILE_64_BIT
	inputOffset = (unsigned long long int)(&(inputArray[index])) & 0x0f;
	#else
	inputOffset = (unsigned int)(&(inputArray[index])) & 0x0f;
	#endif

	if(inputOffset == 0) /* if input array is aligned to a 16 bytes boundary, movdqa_m2r is used */
	{
		while((index+16) <= arrayLength)
		{
			movdqa_m2r(inputArray[index],xmm0);
			movdqa_r2r(xmm0, xmm1);
			movdqa_r2r(xmm0, xmm2);
			/**/
			pand_r2r(xmm6,xmm1);
			pand_r2r(xmm7,xmm2);
			/**/
			movdqa_r2r(xmm1, xmm0);
			/**/
			psrlw_i2r(1, xmm0);
			/**/
			pxor_r2r(xmm2, xmm0);
			por_r2r(xmm1, xmm0);
			/**/
			psllw_i2r(1,xmm0);
			paddb_r2r(xmm7,xmm0);
			psllw_i2r(5,xmm0);
			movdqa_r2r(xmm0, xmm1);
			psraw_i2r(5,xmm0);
			/*The following assembly lines are necessary only for Intel. This is
			* beacuse ARM assembly work at byte level (8 bit), while Intel assembly at word level (16 bit)*/
			movdqu_m2r(amungoNT1065Mask3[0], xmm2); /*  */
			pand_r2r(xmm2,xmm0);
			psllw_i2r(8,xmm1);
			psraw_i2r(13,xmm1);
			movdqu_m2r(amungoNT1065Mask4[0], xmm2); /*  */
			pand_r2r(xmm2,xmm1);
			por_r2r(xmm1, xmm0);
			/**********************************/
			movdqa_r2m(xmm0,outputArray[index]);
			index += 16;
		}
	}
	else
	{
		/* if input array is not aligned to a 16 bytes boundary, movdqu_m2r is used */
		while((index+16) <= arrayLength)
		{
			movdqu_m2r(inputArray[index],xmm0);
			movdqa_r2r(xmm0, xmm1);
			movdqa_r2r(xmm0, xmm2);
			/**/
			pand_r2r(xmm6,xmm1);
			pand_r2r(xmm7,xmm2);
			/**/
			movdqa_r2r(xmm1, xmm0);
			/**/
			psrlw_i2r(1, xmm0);
			/**/
			pxor_r2r(xmm2, xmm0);
			por_r2r(xmm1, xmm0);
			/**/
			psllw_i2r(1,xmm0);
			paddb_r2r(xmm7,xmm0);
			psllw_i2r(5,xmm0);
			movdqa_r2r(xmm0, xmm1);
			psraw_i2r(5,xmm0);
			/*The following assembly lines are necessary only for Intel. This is
			* beacuse ARM assembly work at byte level (8 bit), while Intel assembly at word level (16 bit)*/
			movdqu_m2r(amungoNT1065Mask3[0], xmm2); /*  */
			pand_r2r(xmm2,xmm0);
			psllw_i2r(8,xmm1);
			psraw_i2r(13,xmm1);
			movdqu_m2r(amungoNT1065Mask4[0], xmm2); /*  */
			pand_r2r(xmm2,xmm1);
			por_r2r(xmm1, xmm0);
			movdqa_r2m(xmm0,outputArray[index]);
			/**********************************/
			index += 16;
		}
	}

	/* elaborates remaining samples */
	for(;index < arrayLength; index++)
	{
		outputArray[index] = inputArray[index] & mask;
		outputArray[index] = amungoNT1065_LUT[(int)outputArray[index]];
        /*The following commented lines report the equivalent code in elementary operations and it is the reference C code, to compare assembly against*/
		/*tmp=(inputArray[index] & 0x02);
		sign=tmp>>1;
		module=(inputArray[index] & 0x01);
		outputArray[index] = sign^module;
		outputArray[index] = tmp | outputArray[index];
		outputArray[index] = (outputArray[index]<<1);
		outputArray[index] = outputArray[index]+1;
		outputArray[index] = outputArray[index]<<5;
		outputArray[index] = outputArray[index]>>5;*/
	}


	#ifdef DEBUG_AND_VERIFY
	for(index2=0;index2<arrayLength;index2++)
	{
		if(outputArray[index2]!=cDebug[index2])
		{
			printf("Error: AmungoNT1065SamplesElab has not been correctly computed\n");
			printf("outputArray[index2]=%d cDebug[index2]=%d\n",outputArray[index2],cDebug[index2]);
		}
	}
	free(cDebug);
	#endif
	
	/* RESTORE */
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*0],xmm0);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*1],xmm1);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*2],xmm2);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*6],xmm6);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*7],xmm7);
}


