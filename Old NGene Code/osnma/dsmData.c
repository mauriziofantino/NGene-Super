/**
 * @file dsmData.c
 *
 * @author Mario Nicola
 *
 * @date 19/04/2019
 *
 * @version 1.0.0
 *
 * @brief Functions used to create and manage a structure with all the DSM bits
 */

#include <stddef.h>
#include <stdlib.h>
#include "dsmData.h"
#include "ngeneConstants.h"




void InitDsmData(TDsmData *dsm,int nCh)
{
	int idx;

	dsm->dsmType = DSM_INVALID_TYPE;
	dsm->currDsmId = -1;
	dsm->currNb = 0;
	dsm->wnDsmStart = -1;
	dsm->towDsmStart = -1;
	dsm->validity = (unsigned char *)malloc(OSNMA_DSM_MAX_NB*sizeof(unsigned char));
	dsm->dsmBlocks = (unsigned char *)malloc(OSNMA_DSM_MAX_NB*(DSM_BLOCK_SIZE_BITS/8)*sizeof(unsigned char));
	dsm->dsmArrayLen = nCh;
	dsm->dsmArray = (TDsmArray *)malloc(dsm->dsmArrayLen*sizeof(TDsmArray));
	for(idx=0;idx<dsm->dsmArrayLen;idx++)
	{
		dsm->dsmArray[idx].satType = INVALID_SAT;
		dsm->dsmArray[idx].satId = INVALID_PRN;
		dsm->dsmArray[idx].validity = (unsigned char *)malloc(OSNMA_DSM_MAX_NB*sizeof(unsigned char));
		dsm->dsmArray[idx].dsmBlocks = (unsigned char *)malloc(OSNMA_DSM_MAX_NB*(DSM_BLOCK_SIZE_BITS/8)*sizeof(unsigned char));
		dsm->dsmArray[idx].nmaHeaders = (unsigned char *)malloc(OSNMA_DSM_MAX_NB*sizeof(unsigned char));
		dsm->dsmArray[idx].dsmHeaders = (unsigned char *)malloc(OSNMA_DSM_MAX_NB*sizeof(unsigned char));
	}
	ClearDsmValidityAll(dsm);
}


void ClearDsmValidityAll(TDsmData *dsm)
{
	int idx;
	int idx2;

	/* clear common variable */
	for(idx=0;idx<OSNMA_DSM_MAX_NB;idx++)
	{
		dsm->validity[idx] = FALSE_FLAG;
	}
	/* clear PRN variables */
	for(idx=0;idx<dsm->dsmArrayLen;idx++)
	{
		for(idx2=0;idx2<OSNMA_DSM_MAX_NB;idx2++)
		{
			dsm->dsmArray[idx].validity[idx2] = FALSE_FLAG;
		}
	}
}

//int CheckDsmValidity(TDsmData *dsm,TOsnmaParameters *osnmaPars)
int CheckDsmValidity(unsigned char *validity,int dsmNb)
{
	int idx;
	int numerOfValidDsmBlocks=0;


	/* clear common variable */
	for(idx=0;idx<dsmNb;idx++)
	{
		if(validity[idx] == TRUE_FLAG)
		{
			numerOfValidDsmBlocks++;
		}
	}
	return numerOfValidDsmBlocks;
}

void ClearDsmDataValidity(TDsmData *dsm)
{
	int idx;

	/* clear common variable */
	for(idx=0;idx<OSNMA_DSM_MAX_NB;idx++)
	{
		dsm->validity[idx] = FALSE_FLAG;
	}
}


void ClearDsmValidityOneChannel(TDsmData *dsm,int chId)
{
	int idx;

	for(idx=0;idx<OSNMA_DSM_MAX_NB;idx++)
	{
		dsm->dsmArray[chId].validity[idx] = FALSE_FLAG;
	}
}


void DeinitDsmData(TDsmData *dsm)
{
	int idx;

	dsm->currDsmId = -1;
	dsm->currNb = 0;
	if(dsm->validity!=NULL)
	{
		/* these arrays are allocated at the same time */
		free(dsm->validity);
		dsm->validity = NULL;
		free(dsm->dsmBlocks);
		dsm->dsmBlocks = NULL;
	}
	for(idx=0;idx<dsm->dsmArrayLen;idx++)
	{
		if(dsm->dsmArray[idx].validity!=NULL)
		{
			/* these arrays are allocated at the same time */
			free(dsm->dsmArray[idx].validity);
			dsm->dsmArray[idx].validity = NULL;
			free(dsm->dsmArray[idx].dsmBlocks);
			dsm->dsmArray[idx].dsmBlocks= NULL;
			free(dsm->dsmArray[idx].nmaHeaders);
			dsm->dsmArray[idx].nmaHeaders = NULL;
			free(dsm->dsmArray[idx].dsmHeaders);
			dsm->dsmArray[idx].dsmHeaders = NULL;
		}
	}
	if(dsm->dsmArray!=NULL)
	{
		free(dsm->dsmArray);
		dsm->dsmArray = NULL;
	}
}


