/**
 * @file short2char.c
 *
 * @author Mario Nicola
 *
 * @date 23/09/2020
 *
 * @version 1.0.0
 *
 * @brief Function that translates an array of short int (16 bits)
 *        into an array of char (8 bits).
 */

#include "simd.h"
#include "ngeneConstants.h"


/* global variable for the backup of XMM registers */
extern char *ngeneXmmsBackup;


void FromShortArrayToCharArray(short int *inputArray,char *outputArray,int arrayLength,int shift)
{
	unsigned int inputOffset;
	unsigned int outputOffset;
	int index = 0;

	if(arrayLength==0)
		return;

	// avoid a negative number of shifts
	if(shift<0)
		shift=0;

	/* BACKUP */
	movdqa_r2m(xmm0,ngeneXmmsBackup[XMM_REGISTER_SIZE*0]);
	movdqa_r2m(xmm1,ngeneXmmsBackup[XMM_REGISTER_SIZE*1]);
	/**/

	#ifdef COMPILE_64_BIT
	outputOffset = (unsigned long long int)outputArray & 0x0f;
	#else
	outputOffset = (unsigned int)outputArray & 0x0f;
	#endif
	if(outputOffset!=0)
	{
		for(index=0;index<((int)XMM_REGISTER_SIZE-(int)outputOffset) && index<arrayLength;index++)
			outputArray[index] = (char)(inputArray[index]>>shift);
	}
	/**/
	switch(shift)
	{
	case 0:
		#undef SHIFTED_BITS
		#include "short2char.function"
		break;
	case 1:
		#define SHIFTED_BITS 	1
		#include "short2char.function"
		#undef SHIFTED_BITS
		break;
	case 2:
		#define SHIFTED_BITS 	2
		#include "short2char.function"
		#undef SHIFTED_BITS
		break;
	case 3:
		#define SHIFTED_BITS 	3
		#include "short2char.function"
		#undef SHIFTED_BITS
		break;
	case 4:
		#define SHIFTED_BITS 	4
		#include "short2char.function"
		#undef SHIFTED_BITS
		break;
	case 5:
		#define SHIFTED_BITS 	5
		#include "short2char.function"
		#undef SHIFTED_BITS
		break;
	case 6:
		#define SHIFTED_BITS 	6
		#include "short2char.function"
		#undef SHIFTED_BITS
		break;
	case 7:
		#define SHIFTED_BITS 	7
		#include "short2char.function"
		#undef SHIFTED_BITS
		break;
	case 8:
		#define SHIFTED_BITS 	8
	    #include "short2char.function"
		#undef SHIFTED_BITS
	    break;
	case 9:
		#define SHIFTED_BITS 	9
		#include "short2char.function"
		#undef SHIFTED_BITS
		break;
	case 10:
		#define SHIFTED_BITS 	10
		#include "short2char.function"
		#undef SHIFTED_BITS
		break;
	case 11:
		#define SHIFTED_BITS 	11
		#include "short2char.function"
		#undef SHIFTED_BITS
		break;
	case 12:
		#define SHIFTED_BITS 	12
		#include "short2char.function"
		#undef SHIFTED_BITS
		break;
	default:
		#define SHIFTED_BITS 	12
		#include "short2char.function"
		#undef SHIFTED_BITS
		break;
	}
	/**/
	for(;index<arrayLength;index++)
	{
		outputArray[index] = (char)(inputArray[index]>>shift);
	}

	/* RESTORE */
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*0],xmm0);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*1],xmm1);
}



