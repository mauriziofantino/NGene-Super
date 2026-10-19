/**
 * @file dsmData.h
 *
 * @author Mario Nicola
 *
 * @date 19/04/2019
 *
 * @version 1.0.0
 *
 * @brief Functions used to create and manage a structure with all the DSM bits
 */
#ifndef DSM_DATA_H_INCLUDED
#define DSM_DATA_H_INCLUDED

#include "osnmaDataStructures.h"


void InitDsmData(TDsmData *dsm,int nCh);

void ClearDsmValidityAll(TDsmData *dsm);

int CheckDsmValidity(unsigned char *validity,int dsmNb);

void ClearDsmDataValidity(TDsmData *dsm);

void ClearDsmValidityOneChannel(TDsmData *dsm,int chId);

void DeinitDsmData(TDsmData *dsm);

#endif /* DSM_DATA_H_INCLUDED */
