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

/* global variable for the backup of XMM registers */
extern char *ngeneXmmsBackup;

static short int  usrpMask[12]  = {0x0000, 0x0000, 0xffff, 0xffff, 0x0000, 0x0000, 0xffff, 0xffff, 0x0000, 0x0000, 0xffff, 0xffff};

void UsrpSamplesElab(short int *inputArray, char *outputArray, int arrayLength, int *maskOffsetPtr, int shift)
{
	int maskOffset = *maskOffsetPtr;
	unsigned int inputOffset, outputOffset;
	int index = 0;
	short int *usrpMaskLocal = &(usrpMask[maskOffset]);

	/* BACKUP */
	movdqa_r2m(xmm0,ngeneXmmsBackup[XMM_REGISTER_SIZE*0]);
	movdqa_r2m(xmm1,ngeneXmmsBackup[XMM_REGISTER_SIZE*1]);
	movdqa_r2m(xmm7,ngeneXmmsBackup[XMM_REGISTER_SIZE*7]);
	/**/

	switch(shift)
	{
		case 0:
			#undef SHIFTED_BITS
			#include "usrp.function"
    		break;
    	case 1:
			#define SHIFTED_BITS 	1
			#include "usrp.function"
			#undef SHIFTED_BITS
    		break;
    	case 2:
			#define SHIFTED_BITS 	2
			#include "usrp.function"
			#undef SHIFTED_BITS
    		break;
    	case 3:
			#define SHIFTED_BITS 	3
			#include "usrp.function"
			#undef SHIFTED_BITS
    		break;
    	case 4:
			#define SHIFTED_BITS 	4
			#include "usrp.function"
			#undef SHIFTED_BITS
    		break;
    	case 5:
			#define SHIFTED_BITS 	5
			#include "usrp.function"
			#undef SHIFTED_BITS
    		break;
    	case 6:
			#define SHIFTED_BITS 	6
			#include "usrp.function"
			#undef SHIFTED_BITS
    		break;
    	case 7:
			#define SHIFTED_BITS 	7
			#include "usrp.function"
			#undef SHIFTED_BITS
    		break;
    	case 8:
			#define SHIFTED_BITS 	8
    		#include "usrp.function"
			#undef SHIFTED_BITS
    		break;
    	case 9:
			#define SHIFTED_BITS 	9
			#include "usrp.function"
			#undef SHIFTED_BITS
    		break;
    	case 10:
			#define SHIFTED_BITS 	10
			#include "usrp.function"
			#undef SHIFTED_BITS
    		break;
    	case 11:
			#define SHIFTED_BITS 	11
			#include "usrp.function"
			#undef SHIFTED_BITS
    		break;
    	case 12:
			#define SHIFTED_BITS 	12
			#include "usrp.function"
			#undef SHIFTED_BITS
    		break;
    	default:
			#define SHIFTED_BITS 	12
			#include "usrp.function"
			#undef SHIFTED_BITS
    		break;
	}
	*maskOffsetPtr = (maskOffset + arrayLength) & 0x03;
	
	/* RESTORE */
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*0],xmm0);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*1],xmm1);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*7],xmm7);
}

