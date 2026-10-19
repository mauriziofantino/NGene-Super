/**
 * @file data_shift.c 
 *
 * @author Mario Nicola 
 *
 * @date 03/08/2007
 *
 * @version 1.0.0
 *
 * @brief Functions used to manipulate in a fast way (based on SIMD instructions) the data
 *        conditioning, e.g. the conversion between unsigned to signed or shift.
 */

#include "ngeneConstants.h"
#include "simd.h"
#include "data_shift.h"
#include "arrays.h"


/* global variable for the backup of XMM registers */
extern char *ngeneXmmsBackup;

// Table for the data manipulation
static char sampleShiftMasks[][16] = { {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
{0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f},
{0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f},
{0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f},
{0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f}
};


/* Function which converts from unsigned to signed and apply a shift to the data */
void RightShiftU2sByte(char *inputArray, char *outputArray, int arrayLength, int shiftedBits,unsigned char *correction)
{
    unsigned int offsetIn, offsetOut;
    int index = 0;
    char *sampleShiftMask;

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

    sampleShiftMask = &(sampleShiftMasks[shiftedBits][0]);
    switch (shiftedBits)
    {
    	case 0:
			#undef SHIFTED_BITS
			#include "data_shift_u2s.function"
    		break;
    	case 1:
			#define SHIFTED_BITS 	1
			#include "data_shift_u2s.function"
			#undef SHIFTED_BITS
    		break;
    	case 2:
			#define SHIFTED_BITS 	2
			#include "data_shift_u2s.function"
			#undef SHIFTED_BITS
    		break;
    	case 3:
			#define SHIFTED_BITS 	3
			#include "data_shift_u2s.function"
			#undef SHIFTED_BITS
    		break;
    	default:
			#define SHIFTED_BITS 	4
			#include "data_shift_u2s.function"
			#undef SHIFTED_BITS
    		break;
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


/* Function which shifts the data to reduce their dynamic */
void RightShiftByte(char *inputArray, char *outputArray, int arrayLength, int shiftedBits)
{
    unsigned int offsetIn, offsetOut;
    int index = 0;

	/* BACKUP */
	movdqa_r2m(xmm0,ngeneXmmsBackup[XMM_REGISTER_SIZE*0]);
	movdqa_r2m(xmm1,ngeneXmmsBackup[XMM_REGISTER_SIZE*1]);
	movdqa_r2m(xmm2,ngeneXmmsBackup[XMM_REGISTER_SIZE*2]);
	movdqa_r2m(xmm3,ngeneXmmsBackup[XMM_REGISTER_SIZE*3]);
	movdqa_r2m(xmm4,ngeneXmmsBackup[XMM_REGISTER_SIZE*4]);
	movdqa_r2m(xmm5,ngeneXmmsBackup[XMM_REGISTER_SIZE*5]);
	/**/

    /* N.B. 
     * amount of shift is incremented by 8!!! it's due to used algorithm!!! 
     * */
    
    switch (shiftedBits)
    {
    	case 0:
    		if(inputArray!=outputArray)
    		{
    			ArrayCopy(inputArray,outputArray,arrayLength*sizeof(char));
    		}
    		break;
    	case 1:
			#define SHIFTED_BITS 	9
			#include "data_shift.function"
			#undef SHIFTED_BITS
    		break;
    	case 2:
			#define SHIFTED_BITS 	10
			#include "data_shift.function"
			#undef SHIFTED_BITS
    		break;
    	case 3:
			#define SHIFTED_BITS 	11
			#include "data_shift.function"
			#undef SHIFTED_BITS
    		break;
    	default:
			#define SHIFTED_BITS 	12
			#include "data_shift.function"
			#undef SHIFTED_BITS
    		break;
    }
	
	/* RESTORE */
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*0],xmm0);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*1],xmm1);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*2],xmm2);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*3],xmm3);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*4],xmm4);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*5],xmm5);
}


void LeftShiftWord(short int *inputArray,short int *outputArray,int arrayLength,int shiftedBits)
{
	int index=arrayLength>>3;
	short int *ptrIn = inputArray;
	short int *ptrOut = outputArray;
	int xmmSize = XMM_REGISTER_SIZE/sizeof(short int);

	/* BACKUP */
	movdqa_r2m(xmm0,ngeneXmmsBackup[XMM_REGISTER_SIZE*0]);
	/**/

	switch(shiftedBits)
	{
		case 0:
			if(inputArray!=outputArray)
			{
				ArrayCopy((char *)inputArray,(char *)outputArray,arrayLength*sizeof(short int));
			}
			break;
		case 1:
			#define SHIFTED_BITS 1
			#include "data_shift_left.function"
			#undef SHIFTED_BITS
			break;
		case 2:
			#define SHIFTED_BITS 2
			#include "data_shift_left.function"
			#undef SHIFTED_BITS
			break;
		case 3:
			#define SHIFTED_BITS 3
			#include "data_shift_left.function"
			#undef SHIFTED_BITS
			break;
		case 4:
			#define SHIFTED_BITS 4
			#include "data_shift_left.function"
			#undef SHIFTED_BITS
			break;
		case 5:
			#define SHIFTED_BITS 5
			#include "data_shift_left.function"
			#undef SHIFTED_BITS
			break;
		case 6:
			#define SHIFTED_BITS 6
			#include "data_shift_left.function"
			#undef SHIFTED_BITS
			break;
		case 7:
			#define SHIFTED_BITS 7
			#include "data_shift_left.function"
			#undef SHIFTED_BITS
			break;
		case 8:
			#define SHIFTED_BITS 8
			#include "data_shift_left.function"
			#undef SHIFTED_BITS
			break;
		case 9:
			#define SHIFTED_BITS 9
			#include "data_shift_left.function"
			#undef SHIFTED_BITS
			break;
		case 10:
			#define SHIFTED_BITS 10
			#include "data_shift_left.function"
			#undef SHIFTED_BITS
			break;
		case 11:
			#define SHIFTED_BITS 11
			#include "data_shift_left.function"
			#undef SHIFTED_BITS
			break;
		case 12:
			#define SHIFTED_BITS 12
			#include "data_shift_left.function"
			#undef SHIFTED_BITS
			break;
		case 13:
			#define SHIFTED_BITS 13
			#include "data_shift_left.function"
			#undef SHIFTED_BITS
			break;
		case 14:
			#define SHIFTED_BITS 14
			#include "data_shift_left.function"
			#undef SHIFTED_BITS
			break;
		default:
			#define SHIFTED_BITS 15
			#include "data_shift_left.function"
			#undef SHIFTED_BITS
			break;
	}
	if(shiftedBits>0)
	{
		index = arrayLength & 0x07;
		while(index>0)
		{
			ptrOut[0] = ptrIn[0]<<shiftedBits;
			index--;
			ptrIn++;
			ptrOut++;
		}
	}
	
	/* RESTORE */
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*0],xmm0);
}


void RightShiftUnsignedInt(unsigned int *inputArray,unsigned int *outputArray,int arrayLength,int shiftedBits)
{
	int index=arrayLength>>2;
	unsigned int *ptrIn = inputArray;
	unsigned int *ptrOut = outputArray;
	int xmmSize = XMM_REGISTER_SIZE/sizeof(unsigned int);

	/* BACKUP */
	movdqa_r2m(xmm0,ngeneXmmsBackup[XMM_REGISTER_SIZE*0]);
	/**/

	switch(shiftedBits)
	{
		case 0:
			if(inputArray!=outputArray)
			{
				ArrayCopy((char *)inputArray,(char *)outputArray,arrayLength*sizeof(unsigned int));
			}
			break;
		case 1:
			#define SHIFTED_BITS 1
			#include "data_shift_right_uint32.function"
			#undef SHIFTED_BITS
			break;
		case 2:
			#define SHIFTED_BITS 2
			#include "data_shift_right_uint32.function"
			#undef SHIFTED_BITS
			break;
		case 3:
			#define SHIFTED_BITS 3
			#include "data_shift_right_uint32.function"
			#undef SHIFTED_BITS
			break;
		case 4:
			#define SHIFTED_BITS 4
			#include "data_shift_right_uint32.function"
			#undef SHIFTED_BITS
			break;
		case 5:
			#define SHIFTED_BITS 5
			#include "data_shift_right_uint32.function"
			#undef SHIFTED_BITS
			break;
		case 6:
			#define SHIFTED_BITS 6
			#include "data_shift_right_uint32.function"
			#undef SHIFTED_BITS
			break;
		case 7:
			#define SHIFTED_BITS 7
			#include "data_shift_right_uint32.function"
			#undef SHIFTED_BITS
			break;
		case 8:
			#define SHIFTED_BITS 8
			#include "data_shift_right_uint32.function"
			#undef SHIFTED_BITS
			break;
		case 9:
			#define SHIFTED_BITS 9
			#include "data_shift_right_uint32.function"
			#undef SHIFTED_BITS
			break;
		case 10:
			#define SHIFTED_BITS 10
			#include "data_shift_right_uint32.function"
			#undef SHIFTED_BITS
			break;
		case 11:
			#define SHIFTED_BITS 11
			#include "data_shift_right_uint32.function"
			#undef SHIFTED_BITS
			break;
		case 12:
			#define SHIFTED_BITS 12
			#include "data_shift_right_uint32.function"
			#undef SHIFTED_BITS
			break;
		case 13:
			#define SHIFTED_BITS 13
			#include "data_shift_right_uint32.function"
			#undef SHIFTED_BITS
			break;
		case 14:
			#define SHIFTED_BITS 14
			#include "data_shift_right_uint32.function"
			#undef SHIFTED_BITS
			break;
		default:
			#define SHIFTED_BITS 15
			#include "data_shift_right_uint32.function"
			#undef SHIFTED_BITS
			break;
	}
	if(shiftedBits>0)
	{
		index = arrayLength & 0x03;

		while(index>0)
		{
			ptrOut[0] = ptrIn[0]>>shiftedBits;
			index--;
			ptrIn++;
			ptrOut++;
		}
	}
	
	/* RESTORE */
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*0],xmm0);
}
