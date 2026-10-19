/**
 * @file circularBuffer.c
 *
 * @author Mario Nicola
 *
 * @date 14/08/2014
 *
 * @version 1.0.0
 *
 * @brief Managing of the pointers in the circular buffer.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include "circularBuffer.h"
#include "ngeneConstants.h"
#include "ioFunctions.h"
#include "timeConstants.h"
#include "usbFrontEndConfiguration.h"
#include "stereo.h"
#include "sige.h"
#include "fraunhofer.h"
#include "data_shift.h"
#include "sigeV3.h"
#include "frontend.h"
#include "closeManager.h"
#include "arrays.h"
#include "usrp.h"
#include "stereoTriton.h"
#include "amungoNT1065.h"
#include "jrc.h"
#include "short2char.h"


/* Perform the Circular buffer initialization */
int InitCircularBuffer(TNGeneConfiguration *cfg, TCircularBuffer *buf)
{
    int index;

    buf->dim = (unsigned int) ((cfg->fs * INPUT_BUFFER_SIZE) / SECONDS_TO_MILLISECONDS);
    buf->buffer = (char *) malloc(buf->dim * sizeof(char));
    if (buf->buffer == NULL) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "malloc returned NULL pointer in InitCircularBuffer function");
	return FAILURE_RETURN;
    }
    /* after the creation, the buffer is empty */
    buf->fill1 = 0;
    buf->fill2 = 0;
    buf->tail = 0;		/* tail is at the beginning of the buffer */
    buf->head = 0;		/* head is at the beginning of the buffer */
    buf->free1 = buf->dim;	/* only one free space with size dim */
    buf->free2 = 0;
    buf->discardedSamples = 0;	/* no samples have been elaborated yet */
     /**/ buf->sigeExpected = 0x02;
    buf->sigeMaskOffset = 0;
    posix_memalign((void **) &(buf->usrpSamples), POSIX_ALIGNMENT, buf->dim * sizeof(short int));
    if (buf->usrpSamples == NULL) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "malloc returned NULL pointer in InitCircularBuffer function");
	return FAILURE_RETURN;
    }
    buf->usrpMaskOffset = 0;
    posix_memalign((void **) &(buf->stereoTritonSamples), POSIX_ALIGNMENT, buf->dim * sizeof(char));
    if (buf->stereoTritonSamples == NULL) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "malloc returned NULL pointer in InitCircularBuffer function");
	return FAILURE_RETURN;
    }
    buf->stereoTritonMaskOffset = 0;
    buf->jrcMaskOffset = 0;
    if (cfg->fePars->resolution > 4) {
	buf->shiftAmount = cfg->fePars->resolution - 4;
	buf->unsignedToSignedCorrection[0] = (unsigned char) cfg->fePars->meanValue >> buf->shiftAmount;
    } else {
	buf->shiftAmount = 0;
	buf->unsignedToSignedCorrection[0] = (unsigned char) cfg->fePars->meanValue;
    }
    for (index = 1; index < XMM_REGISTER_SIZE; index++) {
	buf->unsignedToSignedCorrection[index] = buf->unsignedToSignedCorrection[index - 1];
    }
    return SUCCESS_RETURN;
}


/* It is the opposite of the Initialize function. It destroy and clean everything */
void DeinitCircularBuffer(TCircularBuffer *buf)
{
    if (buf->buffer != NULL) {
	free(buf->buffer);
	buf->buffer = NULL;
    }
    if (buf->usrpSamples != NULL) {
	free(buf->usrpSamples);
	buf->usrpSamples = NULL;
    }
    if (buf->stereoTritonSamples != NULL) {
	free(buf->stereoTritonSamples);
	buf->stereoTritonSamples = NULL;
    }
    buf->dim = 0;
    buf->fill1 = 0;
    buf->fill2 = 0;
    buf->tail = 0;
    buf->head = 0;
    buf->free1 = 0;
    buf->free2 = 0;
    buf->discardedSamples = 0;
}


int InsertIntoCircularBuffer(TCircularBuffer *buf, unsigned int displacement)
{
    /* No new samples */
    if (displacement == 0) {
	return SUCCESS_RETURN;
    }
    /* Too many new samples! */
    if (displacement > buf->free1 + buf->free2) {
	return FAILURE_RETURN;
    }
    /* update the position of head */
    buf->head += displacement;
    if (buf->head >= buf->dim) {
	buf->head -= buf->dim;
    }
    /* update fill1, fill2, free1 and free2 */
    if (buf->head > buf->tail) {
	/* one bunch of samples */
	buf->fill1 = buf->head - buf->tail;
	buf->fill2 = 0;
	buf->free1 = buf->dim - buf->head;
	buf->free2 = buf->tail;
    } else {
	/* two bunches of samples or buffer full */
	buf->fill1 = buf->dim - buf->tail;
	buf->fill2 = buf->head;
	buf->free1 = buf->tail - buf->head;
	buf->free2 = 0;
    }
    return SUCCESS_RETURN;
}


/* This function is used to update the pointer for the buffer data handling */
unsigned int DeleteFromCircularBuffer(TCircularBuffer *buf, unsigned int displacement)
{
    /* if no samples have to be discarded, then returns immediately */
    if (displacement == 0) {
	return 0;
    }
    /* Number of samples to be freed exceeds buffer content */
    if (displacement > buf->fill1 + buf->fill2) {
	return 0;
    }
    /* Set tail to new position and wrap around (if needed) */
    buf->tail += displacement;
    if (buf->tail >= buf->dim) {
	buf->tail -= buf->dim;
    }
    if (buf->head >= buf->tail) {	/* Two bunches of free locations */
	buf->fill1 = buf->head - buf->tail;
	buf->fill2 = 0;
	buf->free1 = buf->dim - buf->head;
	buf->free2 = buf->tail;
    } else {			/* one bunch of free locations */
	buf->fill1 = buf->dim - buf->tail;
	buf->fill2 = buf->head;
	buf->free1 = buf->tail - buf->head;
	buf->free2 = 0;
    }
    /* some samples have been elaborated and discarded! */
    buf->discardedSamples += displacement;
    return displacement;
}


void ConvertSamplesIntoNGeneFormat(TNGeneConfiguration *cfg, TCircularBuffer *buf, char *ptr, int n)
{
    if (n <= 0)
	return;

    switch (cfg->usedFrontEnd) {
    case NGENE_FE_STEREO:
	StereoSamplesElab(ptr, ptr, n);
	break;
    case NGENE_FE_SIGE:
	if ((ptr[0] & 0x02) != buf->sigeExpected) {
	    buf->sigeExpected = buf->sigeExpected ^ 0x02;
	    buf->sigeMaskOffset = (buf->sigeMaskOffset + 1) & 0x03;
	}
	if ((n % 2) != 0) {
	    buf->sigeExpected = buf->sigeExpected ^ 0x02;
	}
	SigeSamplesElab(ptr, ptr, n, buf->sigeMaskOffset);
	buf->sigeMaskOffset = (buf->sigeMaskOffset + n) & 0x03;
	break;
    case NGENE_FE_FRAUNHOFER:
	FraunhoferSamplesElab(ptr, ptr, n);
	break;
    case NGENE_FE_SIGE_V3:
	SigeV3SamplesElab(ptr, ptr, n);
	break;
    case NGENE_FE_AMUNGO_NT1065:
	AmungoNT1065SamplesElab(ptr, ptr, n);
	break;
    case NGENE_FE_JRC:
	JrcSamplesElab(ptr, ptr, n, &(buf->jrcMaskOffset), buf->shiftAmount);
	break;
    default:
	if (cfg->fePars->meanValue > 0) {
	    RightShiftU2sByte(ptr, ptr, n, buf->shiftAmount, buf->unsignedToSignedCorrection);
	} else if (buf->shiftAmount > 0) {
	    RightShiftByte(ptr, ptr, n, buf->shiftAmount);
	}
	break;
    }
}


int FillInputBuffer(TNGeneConfiguration *cfg, TCircularBuffer *buf, TRxFe *rxFe)
{
    unsigned int read1 = 0;
    unsigned int read2 = 0;
    unsigned int readTot;

    if ((cfg->usedFrontEnd != NGENE_FE_USRP) && (cfg->usedFrontEnd != NGENE_FE_STEREO_TRITON)) {
	if (buf->free1 > 0) {
	    read1 = ReadRxFrontEnd(rxFe, (void *) &(buf->buffer[buf->head]), buf->free1);
	    ConvertSamplesIntoNGeneFormat(cfg, buf, &(buf->buffer[buf->head]), read1);
	}
	if ((read1 == buf->free1) && (buf->free2 > 0)) {
	    read2 = ReadRxFrontEnd(rxFe, (void *) &(buf->buffer[0]), buf->free2);
	    ConvertSamplesIntoNGeneFormat(cfg, buf, &(buf->buffer[0]), read2);
	}
    } else if ((cfg->usedFrontEnd == NGENE_FE_USRP)) {
	/* NGENE_FE_USRP is 16 bits per sample! read and convert to 8 bits per sample */
	if (buf->free1 > 0) {
	    read1 = ReadRxFrontEnd(rxFe, (void *) buf->usrpSamples, 2 * buf->free1);	// reads bytes!
	    read1 = read1 / 2;	// from number of bytes to number of words
	    FromShortArrayToCharArray(buf->usrpSamples, &(buf->buffer[buf->head]), read1, buf->shiftAmount);
	    JrcSamplesElab(&(buf->buffer[buf->head]), &(buf->buffer[buf->head]), read1, &(buf->jrcMaskOffset), 0);
	}
	if ((read1 == buf->free1) && (buf->free2 > 0)) {
	    read2 = ReadRxFrontEnd(rxFe, (void *) buf->usrpSamples, 2 * buf->free2);	// reads bytes!
	    read2 = read2 / 2;	// from number of bytes to number of words
	    FromShortArrayToCharArray(buf->usrpSamples, &(buf->buffer[0]), read2, buf->shiftAmount);
	    JrcSamplesElab(&(buf->buffer[0]), &(buf->buffer[0]), read2, &(buf->jrcMaskOffset), 0);
	}
    } else if (cfg->usedFrontEnd == NGENE_FE_STEREO_TRITON) {
	/* NGENE_FE_STEREO_TRITON works only from file, and with a specific sample format! */
	if (buf->free1 > 0) {
	    read1 = fread(buf->stereoTritonSamples, sizeof(char), buf->free1, rxFe->incomingFile);
	    StereoTritonSamplesElab(buf->stereoTritonSamples, &(buf->buffer[buf->head]), read1, &(buf->stereoTritonMaskOffset), buf->shiftAmount);
	}
	if ((read1 == buf->free1) && (buf->free2 > 0)) {
	    read2 = fread(buf->stereoTritonSamples, sizeof(char), buf->free2, rxFe->incomingFile);
	    StereoTritonSamplesElab(buf->stereoTritonSamples, &(buf->buffer[0]), read2, &(buf->stereoTritonMaskOffset), buf->shiftAmount);
	}
    }

    readTot = read1 + read2;
    if (InsertIntoCircularBuffer(buf, readTot) == FAILURE_RETURN) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Unsupported buffer condition: investigate!");
	CloseNGene(cfg, EXIT_FAILURE);
    }
    if ((buf->free1 + buf->free2) > 0)
	return FAILURE_RETURN;
    else
	return SUCCESS_RETURN;
}


int CopyABunchOfContiguousSamples(TCircularBuffer *buf, int neededSamples, char *outputArray)
{
    /* check if the input buffer is large enough */
    if (buf->dim < (unsigned int) neededSamples) {
	return FAILURE_RETURN;
    }
    /* select the most recent samples in the input buffer */
    if (buf->head >= (unsigned int) neededSamples) {
	ArrayCopy(&(buf->buffer[buf->head - neededSamples]), outputArray, neededSamples);
    } else {
	ArrayCopy(buf->buffer, outputArray, buf->head);
	ArrayCopy(&(buf->buffer[buf->dim - (neededSamples - buf->head)]), &(outputArray[buf->head]), neededSamples - buf->head);
    }
    return SUCCESS_RETURN;
}
