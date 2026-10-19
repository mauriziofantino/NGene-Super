/**
 * @file osnmaValidations.h
 *
 * @author Mario Nicola
 *
 * @date 03/11/2020
 *
 * @version 1.0.0
 *
 * @brief Management of MAC validations
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "osnmaValidations.h"
#include "ngeneConstants.h"
#include "osnmaUtilities.h"
#include "ioFunctions.h"




/* Manage MAC validations list */

static int GetIndexInMacValidationMatrix(int nCh,int chIdxA,int chIdxN)
{
	return ((chIdxN*nCh)+chIdxA);
}

void InsertMacValidation(TOsnmaAuthentication *auth,int chIdxA,int chIdxN,int prnN,int wn,int tow,TAdkd adkd,unsigned int iod,TValidationStatus validityFlag)
{
	TMacValidations *newPtr;
	TMacValidations *ptr;
	int index;

	/* save the PRN of the current authenticated channel */
	auth->prnN[chIdxN] = prnN;

	/* create new element */
	newPtr = (TMacValidations *)malloc(sizeof(TMacValidations));
	newPtr->wnSubframeStart = wn;
	newPtr->towSubframeStart = tow;
	newPtr->adkd = adkd;
	newPtr->iod = iod;
	newPtr->validityFlag = validityFlag;
	newPtr->nextMac = NULL;

	/* insert in the list */
	index = GetIndexInMacValidationMatrix(auth->nCh,chIdxA,chIdxN);
	if(auth->macs[index]==NULL)
	{
		// empty list, newPtr is the new head of the list
		auth->macs[index] = newPtr;
	}
	else
	{
		// insert in the tail
		ptr = auth->macs[index];
		while(ptr->nextMac != NULL)
		{
			ptr = ptr->nextMac;
		}
		ptr->nextMac = newPtr;
	}
}

TMacValidations * ExtractMacValidation(TOsnmaAuthentication *auth,int chIdxA,int chIdxN)
{
	TMacValidations *ptr;
	int index;

	/* the return value is the pointer to the former head of the list */
	index = GetIndexInMacValidationMatrix(auth->nCh,chIdxA,chIdxN);
	ptr = auth->macs[index];
	/* get the new head (may empty) */
	if(ptr!=NULL)
	{
		auth->macs[index] = ptr->nextMac;
	}

	return ptr;
}


void DeleteMacValidationsOneList(TOsnmaAuthentication *auth,int chIdxA,int chIdxN)
{
	TMacValidations *ptr;
	TMacValidations *nextPtr;
	int index;

	index = GetIndexInMacValidationMatrix(auth->nCh,chIdxA,chIdxN);
	ptr = auth->macs[index];
	while(ptr!=NULL)
	{
		nextPtr = ptr->nextMac;
		free(ptr);
		ptr = nextPtr;
	}
	auth->macs[index] = NULL;
}

void DeleteMacValidationsAuthenticatedChannel(TOsnmaAuthentication *auth,int chIdxN)
{
	int chIdxA;

	for(chIdxA=0;chIdxA<auth->nCh;chIdxA++)
	{
		DeleteMacValidationsOneList(auth,chIdxA,chIdxN);
	}
}

void DeleteMacValidationsAuthenticatingChannel(TOsnmaAuthentication *auth,int chIdxA)
{
	int chIdxN;

	for(chIdxN=0;chIdxN<auth->nCh;chIdxN++)
	{
		DeleteMacValidationsOneList(auth,chIdxA,chIdxN);
	}
}



/* Manage the overall validation structure */

void InitOsnmaAuthenticationStruct(TNGeneConfiguration *cfg,TOsnmaAuthentication *auth)
{
	int idx;

	auth->nCh = cfg->numberOfChannels;
	auth->numAccumulations = INVALID_VALUE;
	// channel info
	auth->prnN = (int *)malloc(auth->nCh*sizeof(int));
	for(idx=0;idx<auth->nCh;idx++)
	{
		auth->prnN[idx] = 0;
	}
	// list of MACs validations
	auth->macs = (TMacValidations **)malloc(auth->nCh*auth->nCh*sizeof(TMacValidations *));
	for(idx=0;idx<auth->nCh*auth->nCh;idx++)
	{
		auth->macs[idx] = NULL;
	}
	// accumulations
	auth->accumulations = (TMacAccumulations *)malloc(auth->nCh*NUM_OF_ADKD_VALUES*sizeof(TMacAccumulations));
	for(idx=0;idx<(auth->nCh*NUM_OF_ADKD_VALUES);idx++)
	{
		auth->accumulations[idx].iod = 0;
		auth->accumulations[idx].counter = 0;
		auth->accumulations[idx].authenticationFlag = TO_BE_VALIDATED;
	}
	// authentication flags
	auth->authenticationFlags = (TValidationStatus *)malloc(auth->nCh*sizeof(TValidationStatus));
	auth->rxCounter = (unsigned long long int *)malloc(auth->nCh*sizeof(unsigned long long int));
	for(idx=0;idx<auth->nCh;idx++)
	{
		auth->authenticationFlags[idx] = TO_BE_VALIDATED;
		auth->rxCounter[idx] = 0;
	}
}

void ResetOsnmaAuthenticationStruct(TOsnmaAuthentication *auth)
{
	int idx;

	for(idx=0;idx<auth->nCh;idx++)
	{
		auth->prnN[idx] = 0;
		DeleteMacValidationsAuthenticatedChannel(auth,idx);
	}
	for(idx=0;idx<(auth->nCh*NUM_OF_ADKD_VALUES);idx++)
	{
		auth->accumulations[idx].iod = 0;
		auth->accumulations[idx].counter = 0;
		auth->accumulations[idx].authenticationFlag = TO_BE_VALIDATED;
	}
	// TODO cambiare a seconda del valore di validazione precedente?
	for(idx=0;idx<auth->nCh;idx++)
	{
		auth->authenticationFlags[idx] = TO_BE_VALIDATED;
	}
}


void ResetChannelAuthenticationStruct(TOsnmaAuthentication *auth,int chIndex)
{
	int idx;

	auth->prnN[chIndex] = 0;
	// delete available MACs validations
	DeleteMacValidationsAuthenticatedChannel(auth,chIndex);
	DeleteMacValidationsAuthenticatingChannel(auth,chIndex);
	// reset accumulations
	for(idx=chIndex*NUM_OF_ADKD_VALUES;idx<(chIndex+1)*NUM_OF_ADKD_VALUES;idx++)
	{
		auth->accumulations[idx].iod = 0;
		auth->accumulations[idx].counter = 0;
		auth->accumulations[idx].authenticationFlag = TO_BE_VALIDATED;
	}
	// reset channel authentication flag
	auth->rxCounter[chIndex] = 0;
	auth->authenticationFlags[chIndex] = TO_BE_VALIDATED;
}


void DeinitOsnmaAuthenticationStruct(TOsnmaAuthentication *auth)
{
	int idx;

	free(auth->prnN);
	for(idx=0;idx<auth->nCh;idx++)
	{
		DeleteMacValidationsAuthenticatedChannel(auth,idx);
	}
	free(auth->macs);
	free(auth->accumulations);
	free(auth->authenticationFlags);
	free(auth->rxCounter);
}


void CheckOsnmaDsmTeslaMacseq(TNGeneConfiguration *cfg,TNGeneData *rx)
{
	int chIdx;
	TChannel *ch;
	TOsnmaAuthentication *auth = &(rx->osnmaAuth);
	int adkdIdx;
	int accumIdx;
	TMacAccumulations *accPtr;

	for(chIdx=0;chIdx<cfg->numberOfChannels;chIdx++)
	{
		ch = &(rx->channelArray[chIdx]);

		// perform this check only for Galileo
		if(ch->satType!=GALILEO_SAT)
			continue;

		// perform this check only if parser data are available
		if(ch->galParser==NULL)
			continue;

		// check the validation flags for this channel
		if((ch->dsmStatus==NOT_VALID) ||
		   (ch->keyStatus==NOT_VALID) ||
		   (ch->macseqStatus==NOT_VALID))
		{
			// set output flag
			auth->authenticationFlags[chIdx] = NOT_VALID;
			// reset MAC accumulations
			for(adkdIdx=0;adkdIdx<NUM_OF_ADKD_VALUES;adkdIdx++)
			{
				accumIdx = (chIdx*NUM_OF_ADKD_VALUES) + adkdIdx;
				accPtr = &(auth->accumulations[accumIdx]);
				accPtr->counter = 0;
				accPtr->authenticationFlag = TO_BE_VALIDATED;
			}
		}
	}
}


void ManageOsnmaAuthentication(unsigned long long int rxCounter,TOsnmaData *osnma,TOsnmaAuthentication *auth)
{
	int chIdxN; // index of the authenticated channel
	int macsFound;
	int chIdxA; // index of the authenticating channel
	TMacValidations *macPtr;
	int accumIdx;
	TMacAccumulations *accPtr;
	int adkdIdx;
	int numPending;
	int numNotValid;
	int numValid;

	/* update the number of required authentications if the number of needed accumulations changes */
	if(auth->numAccumulations != osnma->osnmaPars.requiredMacAccumulations)
	{
		if(auth->numAccumulations != INVALID_VALUE)
		{
			// if numAccumulations already valid, discard old data
			ResetOsnmaAuthenticationStruct(auth);
		}
		auth->numAccumulations = osnma->osnmaPars.requiredMacAccumulations;
	}

	/* manage each channel for authentication */
	for(chIdxN=0;chIdxN<auth->nCh;chIdxN++)
	{
		macsFound = FALSE_FLAG;
		/* manage each authenticating channel */
		for(chIdxA=0;chIdxA<auth->nCh;chIdxA++)
		{
			// repeat until the list of validations for (chIdxN,chIdxA) is finished
			macPtr = ExtractMacValidation(auth,chIdxA,chIdxN);
			while(macPtr!=NULL)
			{
				macsFound = TRUE_FLAG;

				// get accumulation for the current ADKD
				accumIdx = (chIdxN*NUM_OF_ADKD_VALUES) + (int)(macPtr->adkd);
				accPtr = &(auth->accumulations[accumIdx]);

				/* manage a change of IOD or new data flag */
				if((macPtr->adkd==ADKD_EPH_CLK_HEALTH) || (macPtr->adkd==ADKD_SLMAC_1_EXTRA) || (macPtr->adkd==ADKD_SLMAC_10_EXTRA) || (macPtr->adkd==ADKD_OTHER_NAV_MSG))
				{
					// restart accumulation if new data bit (MS bit of IODtag) is set or 3 LS IODtag (3 LS bits of IOD) changed
					if(((macPtr->iod & 0x08)>0) || ((macPtr->iod & 0x07)!=accPtr->iod))
					{
						accPtr->iod = macPtr->iod & 0x07;
						accPtr->counter = 0;
						accPtr->authenticationFlag = TO_BE_VALIDATED;
					}
				}
				else if(macPtr->adkd==ADKD_ALMANAC)
				{
					// restart accumulation if the four bits of IOD tag changed
					if(macPtr->iod != accPtr->iod)
					{
						accPtr->iod = macPtr->iod;
						accPtr->counter = 0;
						accPtr->authenticationFlag = TO_BE_VALIDATED;
					}
				}
				// other ADKDs do not have IOD or are not supported

				/* manage accumulation */
				if(macPtr->validityFlag==NOT_VALID)
				{
					// when a failure occurs, everything is discarded!
					accPtr->counter = 0;
					accPtr->authenticationFlag = NOT_VALID;
				}
				else
				{
					// update the time of the last authentication found (only when the MAC is valid)
					auth->rxCounter[chIdxN] = rxCounter;

					// when MAC is validated, accumulate
					switch(accPtr->authenticationFlag)
					{
					case TO_BE_VALIDATED:
						accPtr->counter++;
						if(accPtr->counter>=auth->numAccumulations)
						{
							accPtr->counter = 0;
							accPtr->authenticationFlag = VALIDATED;
						}
						else
						{
							accPtr->authenticationFlag = VALIDATION_PENDING;
						}
						break;
					case VALIDATION_PENDING:
					case NOT_VALID:
						// TODO gestire separatamente il caso NOT_VALID?
						// ad esempio farlo muovere da li' solo se c'e' stata una qualche mitigation?
						accPtr->counter++;
						if(accPtr->counter>=auth->numAccumulations)
						{
							accPtr->authenticationFlag = VALIDATED;
						}
						break;
					case VALIDATED:
						accPtr->counter++;
						if(accPtr->counter>auth->numAccumulations)
						{
							accPtr->counter = 1;
						}
						break;
					default:
						break;
					}
				}

				// free the used validation (already extracted from the list!)
				free(macPtr);
				// get the next MAC
				macPtr = ExtractMacValidation(auth,chIdxA,chIdxN);
			} // for each MAC
		} // for each authenticating channel

		/* manage authentication flag on a channel basis */
		if(macsFound==TRUE_FLAG)
		{
			numPending = 0;
			numNotValid = 0;
			numValid = 0;
			for(adkdIdx=0;adkdIdx<NUM_OF_ADKD_VALUES;adkdIdx++)
			{
				accumIdx = (chIdxN*NUM_OF_ADKD_VALUES) + adkdIdx;
				accPtr = &(auth->accumulations[accumIdx]);
				if(accPtr->authenticationFlag==VALIDATION_PENDING)
					numPending++;
				else if(accPtr->authenticationFlag==NOT_VALID)
					numNotValid++;
				else if(accPtr->authenticationFlag==VALIDATED)
					numValid++;
			}
			// TODO tenere conto del valore precedente?
			if(numNotValid>0)
				auth->authenticationFlags[chIdxN] = NOT_VALID;
			else if(numValid>0)
				auth->authenticationFlags[chIdxN] = VALIDATED;
			else if(numPending>0)
				auth->authenticationFlags[chIdxN] = VALIDATION_PENDING;
			else
				auth->authenticationFlags[chIdxN] = TO_BE_VALIDATED;
		}
	} // for each authenticated channel
}


void ManageOsnmaTimeout(TNGeneConfiguration *cfg,TNGeneData *rx)
{
	int chIdx;
	TOsnmaAuthentication *auth = &(rx->osnmaAuth);
	TChannel *ch;
	int adkdIdx;
	int accumIdx;
	TMacAccumulations *accPtr;

	/* if disabled, nothing to do */
	if(rx->osnmaData.osnmaPars.osnmaTimeout==OSNMA_TIMEOUT_DISABLED)
		return;

	/* for each channel, check the time of the last MAC validation */
	for(chIdx=0;chIdx<cfg->numberOfChannels;chIdx++)
	{
		ch = &(rx->channelArray[chIdx]);
		if(ch->satId==INVALID_PRN)
		{
			// no valid PRN associated to the channel, do nothing
			continue;
		}
		if(auth->rxCounter[chIdx]==0)
		{
			// no validations found for this channel, do nothing
			continue;
		}
		// check the time of the last MAC validation
		if((int)((rx->rxCounter - auth->rxCounter[chIdx])/SECONDS_TO_MILLISECONDS) > rx->osnmaData.osnmaPars.osnmaTimeout)
		{
			// set output flag
			if(auth->authenticationFlags[chIdx]!=NOT_VALID)
			{
				if(ch->satType==GALILEO_SAT)
					SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO, "Authentication of the navigation message of Galileo %d removed due to OSNMA timeout",ch->satId);
				else
					SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO, "Authentication of the navigation message of GPS %d removed due to OSNMA timeout",ch->satId);
				//
				ch->msgStatus = TO_BE_VALIDATED;
				auth->rxCounter[chIdx] = 0;
				auth->authenticationFlags[chIdx] = TO_BE_VALIDATED;
			}
			// reset MAC accumulations
			for(adkdIdx=0;adkdIdx<NUM_OF_ADKD_VALUES;adkdIdx++)
			{
				accumIdx = (chIdx*NUM_OF_ADKD_VALUES) + adkdIdx;
				accPtr = &(auth->accumulations[accumIdx]);
				accPtr->counter = 0;
				if(accPtr->authenticationFlag!=NOT_VALID)
				{
					accPtr->authenticationFlag = TO_BE_VALIDATED;
				}
			}
		}
	}
}


void SaveAuthenticationLog(int wn,double tow,TOsnmaData *osnma,TOsnmaAuthentication *auth)
{
	int chIdxN;
	int accIdx;
	TMacAccumulations *accPtr;
	int adkdArray[] = {0,2,3,4,5,11,12};
	int adkdArrayLen = 7;
	int idx;

	/* preliminary check */
	if((wn<0) || (tow<0) || (osnma->authLogFile==NULL))
	{
		// log is performed only when enabled and the time is valid
		return;
	}

	/* save header (when needed) */
	if(ftell(osnma->authLogFile)==0)
	{
		// if the file is empty, add a header
		fprintf(osnma->authLogFile,"%4d %10.3f   %2d %3d %2d    ",-1,-1.0,-1,-1,TO_BE_VALIDATED);
		// save each ADKD
		for(idx=0;idx<adkdArrayLen;idx++)
		{
			fprintf(osnma->authLogFile,"%2d %2u %2d   ",adkdArray[idx],0,TO_BE_VALIDATED);
		}
		fprintf(osnma->authLogFile,"\n");
	}

	/* save authentication for each channel */
	for(chIdxN=0;chIdxN<auth->nCh;chIdxN++)
	{
		if(auth->prnN[chIdxN]==0)
		{
			// if prnN is not valid, this channel does not have MACs
			continue;
		}
		// info (WN, TOW, channel index, PRN, authentication flag)
		fprintf(osnma->authLogFile,"%4d %10.3f   %2d %3d %2d    ",wn,tow,chIdxN,auth->prnN[chIdxN],auth->authenticationFlags[chIdxN]);
		// save each ADKD
		for(idx=0;idx<adkdArrayLen;idx++)
		{
			accIdx  = (chIdxN*NUM_OF_ADKD_VALUES) + adkdArray[idx];
			accPtr = &(auth->accumulations[accIdx]);
			fprintf(osnma->authLogFile,"%2d %2u %2d   ",accPtr->counter,accPtr->iod,accPtr->authenticationFlag);
		}
		fprintf(osnma->authLogFile,"\n");
	}
}


void SaveOsnmaSimpleLog(TNGeneConfiguration *cfg,TNGeneData *rx)
{
	int wn = rx->pvtData.rxGalWn;
	double tow = rx->pvtData.rxGalTow;
	long int tCurr = (long long int)(wn*SECONDS_IN_A_WEEK) + (long int)floor(tow);
	TOsnmaData *osnma = &(rx->osnmaData);
	TOsnmaAuthentication *auth = &(rx->osnmaAuth);
	int chIdx;
	TValidationStatus currStatus;
	int prnN;

	/* preliminary check */
	if((wn<0) || (tow<0) || (osnma->simpleLogFile==NULL) || (osnma->simpleLogBinFile==NULL))
	{
		// log is performed only when enabled and the time is valid
		return;
	}

	/* check if the time for an update is now */
	if((tCurr-OSNMA_SIMPLE_LOG_UPDATE_INTERVAL) < osnma->lastSimpleLogSaving)
	{
		// not yet
		return;
	}

	/* the time is now */
	osnma->lastSimpleLogSaving = tCurr;

	/* save on a channel basis */
	for(chIdx=0;chIdx<cfg->numberOfChannels;chIdx++)
	{
		// save only satellites in tracking
		if(rx->channelArray[chIdx].chState!=TRACKING_STATE)
		{
			continue;
		}
		// prepare
		if(auth->prnN[chIdx]==0)
		{
			currStatus = TO_BE_VALIDATED;
			prnN = GetOsnmaPrnFromSatTypePrn(rx->channelArray[chIdx].satType,rx->channelArray[chIdx].satId);
		}
		else
		{
			// do not take into account MAC accumulation (put it to VALID instead of VALIDATION_PENDING)
			currStatus = (auth->authenticationFlags[chIdx]==VALIDATION_PENDING) ? VALIDATED : auth->authenticationFlags[chIdx];
			prnN = auth->prnN[chIdx];
		}
		// text file
		fprintf(osnma->simpleLogFile,"%12ld,",tCurr);
		fprintf(osnma->simpleLogFile,"%3d,",prnN);
		fprintf(osnma->simpleLogFile,"%d\n",(int)currStatus);
		// bin file
		fwrite(&(tCurr),sizeof(long int),1,osnma->simpleLogBinFile);
		fwrite(&(prnN),sizeof(int),1,osnma->simpleLogBinFile);
		fwrite(&(currStatus),sizeof(int),1,osnma->simpleLogBinFile);
	}
}
