/**
 * @file megaBuffer.h
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
#include "ngeneConstants.h"
#include "timeConstants.h"

#ifndef MEGA_BUFFER_H_INCLUDED
#define MEGA_BUFFER_H_INCLUDED

/* when OSNMA data is found, the saved samples starts
 * ADVANCE_SAVE_SECONDS before  */
#define ADVANCE_SAVE_SECONDS 60.0

#define LENGTH_SAVE_MINUTES 5.0

typedef struct MegaBufferStruct {
    double fs;
    FILE *fOut;
    unsigned long long int bufferSize;
    char *buffer;
    unsigned long long int bufferIndex;
    int flushEnabled;
    int bufferFull;
    double flushedSeconds;
} TMegaBuffer;

void InitMegaBuffer(TMegaBuffer * buf, double fs);
void DeinitMegaBuffer(TMegaBuffer * buf);
void StartMegaBuffer(TMegaBuffer * buf);
void UpdateMegaBuffer(TMegaBuffer * buf, char *v, unsigned int n);

#endif				/* MEGA_BUFFER_H_INCLUDED */
