/**
 * @file reedSolomon.h
 *
 * @author Mario Nicola
 *
 * @date 08/07/2021
 *
 * @version 1.0.0
 *
 * @brief Functions to perform Reed-Solomon decoder for Galileo I/NAV navigation message.
 */

#ifndef REEDSOLOMON_H_INCLUDED
#define REEDSOLOMON_H_INCLUDED

#include "navigationMessageParser.h"

typedef enum RsCheckResultEnum
{
	RS_CHECK_NOT_PERFORMED,
	RS_CHECK_NO_ERRORS,
	RS_CHECK_ERRORS,
	RS_FAILED_RESTORE,
	RS_ERASURE_RESTORED
} TRsCheckResult;

void InitReedSolomon();
void DeinitReedSolomon();
TRsCheckResult CheckReedSolomon(TGalParser *parser,int *numCorrections);

#endif /* REEDSOLOMON_H_INCLUDED */
