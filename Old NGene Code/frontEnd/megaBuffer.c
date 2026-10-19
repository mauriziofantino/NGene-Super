/**
 * @file megaBuffer.c
 *
 * @author Mario Nicola
 *
 * @date 08/08/2019
 *
 * @version 1.0.0
 *
 * @brief Variables and functions used to save samples on a file after a condition is verified
 */

#include <stdio.h>
#include <stdlib.h>
#include "megaBuffer.h"
#include "osnmaConstants.h"
#include "arrays.h"

void InitMegaBuffer(TMegaBuffer *buf, double fs)
{
    buf->fs = fs;
    buf->fOut = NULL;
    buf->bufferSize = (unsigned long long int) (fs * ADVANCE_SAVE_SECONDS);
    buf->buffer = (char *) malloc(buf->bufferSize * sizeof(char));
    buf->bufferIndex = 0;
    buf->flushEnabled = FALSE_FLAG;
    buf->bufferFull = FALSE_FLAG;
    buf->flushedSeconds = 0.0;
}

void DeinitMegaBuffer(TMegaBuffer *buf)
{
    if (buf->fOut != NULL) {
	fclose(buf->fOut);
	buf->fOut = NULL;
    }
    if (buf->buffer != NULL) {
	free(buf->buffer);
	buf->buffer = NULL;
    }
}

void StartMegaBuffer(TMegaBuffer *buf)
{
    if (buf->buffer == NULL) {
	return;
    }
    buf->fOut = fopen("RawSamplesForOsnma.bin", "wb");
    buf->flushEnabled = TRUE_FLAG;
}

void UpdateMegaBuffer(TMegaBuffer *buf, char *v, unsigned int n)
{
    int n1;
    int n2;

    /* Initial checks */
    if (buf->buffer == NULL) {
	return;
    }
    if (buf->flushedSeconds >= (LENGTH_SAVE_MINUTES * SECONDS_PER_MINUTE)) {
	DeinitMegaBuffer(buf);
	return;
    }

    /* Write to buffer/flush to file depending on the status */
    if (buf->bufferFull == FALSE_FLAG) {
	/* start only when the buffer has been filled */
	if ((buf->bufferIndex + n) <= buf->bufferSize) {
	    ArrayCopy(v, &(buf->buffer[buf->bufferIndex]), n);
	    buf->bufferIndex += n;
	    if (buf->bufferIndex == buf->bufferSize) {
		buf->bufferIndex = 0;
		buf->bufferFull = TRUE_FLAG;
	    }
	} else {
	    n1 = buf->bufferSize - buf->bufferIndex;
	    n2 = n - n1;
	    ArrayCopy(v, &(buf->buffer[buf->bufferIndex]), n1);
	    if (buf->flushEnabled == TRUE_FLAG) {
		fwrite(&(buf->buffer[0]), sizeof(char), n2, buf->fOut);
		buf->flushedSeconds += ((double) n2 / buf->fs);
	    }
	    ArrayCopy(&(v[n1]), &(buf->buffer[0]), n2);
	    buf->bufferIndex = n2;
	    buf->bufferFull = TRUE_FLAG;
	}
    } else if (buf->flushEnabled == TRUE_FLAG)	// && (buf->bufferFull==TRUE_FLAG)
    {
	/* save old samples and then overwrite them */
	if ((buf->bufferIndex + n) <= buf->bufferSize) {
	    fwrite(&(buf->buffer[buf->bufferIndex]), sizeof(char), n, buf->fOut);
	    ArrayCopy(v, &(buf->buffer[buf->bufferIndex]), n);
	    buf->bufferIndex += n;
	    if (buf->bufferIndex == buf->bufferSize) {
		buf->bufferIndex = 0;
	    }
	} else {
	    n1 = buf->bufferSize - buf->bufferIndex;
	    n2 = n - n1;
	    fwrite(&(buf->buffer[buf->bufferIndex]), sizeof(char), n1, buf->fOut);
	    ArrayCopy(v, &(buf->buffer[buf->bufferIndex]), n1);
	    fwrite(&(buf->buffer[0]), sizeof(char), n2, buf->fOut);
	    ArrayCopy(&(v[n1]), &(buf->buffer[0]), n2);
	    buf->bufferIndex = n2;
	}
	buf->flushedSeconds += ((double) n / buf->fs);
    } else			// if (buf->flushEnabled==FALSE_FLAG) && (buf->bufferFull==TRUE_FLAG)
    {
	/* just overwrite old samples */
	if ((buf->bufferIndex + n) < buf->bufferSize) {
	    ArrayCopy(v, &(buf->buffer[buf->bufferIndex]), n);
	    buf->bufferIndex += n;
	} else {
	    n1 = buf->bufferSize - buf->bufferIndex;
	    n2 = n - n1;
	    ArrayCopy(v, &(buf->buffer[buf->bufferIndex]), n1);
	    ArrayCopy(&(v[n1]), &(buf->buffer[0]), n2);
	    buf->bufferIndex = n2;
	}
    }
}
