/**
 * @file mackList.c
 *
 * @author Mario Nicola
 *
 * @date 17/04/2019
 *
 * @version 1.0.0
 *
 * @brief Functions used to create and manage a list with all the MACK information
 */

#include <stdlib.h>
#include "mackList.h"
#include "navigationMessageParser.h"


TMackList * FindInMackList(TMackList *head,TSatConstellation satType,TSatSignal satSignal,int satId,int chId)
{
	TMackList *ptr = head;

	while(ptr!=NULL)
	{
		if((ptr->satType==satType) && (ptr->satSignal==satSignal) && (ptr->satId==satId) && (ptr->chId==chId))
		{
			return ptr;
		}
		else
		{
			ptr = ptr->nextMack;
		}
	}
	return ptr;
}


TMackList * PushInMackList(TMackList *head,TSatConstellation satType,TSatSignal satSignal,int satId,int chId)
{
	TMackList *ptr = head;
	TMackList *prev = NULL;

	while(ptr!=NULL)
	{
		if((ptr->satType==satType) && (ptr->satSignal==satSignal) && (ptr->satId==satId) && (ptr->chId==chId))
		{
			return ptr;
		}
		else
		{
			prev = ptr;
			ptr = ptr->nextMack;
		}
	}
	/* not found, create one */
	ptr = (TMackList *)malloc(sizeof(TMackList));
	if(prev!=NULL)
	{
		prev->nextMack = ptr;
	}
	if(ptr!=NULL)
	{
		ptr->satType = satType;
		ptr->satSignal = satSignal;
		ptr->satId = satId;
		ptr->chId = chId;
		ptr->prnA = -1;
		ptr->prnN = -1;
		ptr->macList = NULL;
		ptr->keyList = NULL;
		ptr->navDataList = NULL;
		ptr->prevMack = prev;
		ptr->nextMack = NULL;
	}
	return ptr;
}


void DeleteMackList(TMackList **head)
{
	TMackList *ptr = *head;
	TMackList *next;

	if(ptr==NULL)
		return;

	while(ptr!=NULL)
	{
		next = ptr->nextMack;
		DeleteMacList(ptr);
		DeleteKeyList(ptr);
		DeleteNavDataList(ptr);
		free(ptr);
		ptr = next;
	}
	
	// the list now is empty
	*head = NULL;
}


void DeleteAllKeysAndMacsFromMackList(TMackList *head)
{
	TMackList *ptr = head;
	TMackList *next;

	while(ptr!=NULL)
	{
		next = ptr->nextMack;
		DeleteMacList(ptr);
		DeleteKeyList(ptr);
		ptr = next;
	}
}


TMackList * DeleteChannelFromMackList(TMackList *head,int chId)
{
	TMackList *ptr = head;
	TMackList *next = NULL;
	TMackList *newHead = head;

	while(ptr!=NULL)
	{
		next = ptr->nextMack;
		if(ptr->chId==chId)
		{
			/* found: delete it */
			DeleteMacList(ptr);
			DeleteKeyList(ptr);
			DeleteNavDataList(ptr);
			if(ptr->prevMack==NULL)
			{
				/* first in the list */
				newHead = next;
			}
			else
			{
				(ptr->prevMack)->nextMack = next;
			}
			if(next!=NULL)
			{
				/* not the last in the list */
				next->prevMack = ptr->prevMack;
			}
			free(ptr);
		}
		ptr = next;
	}
	return newHead;
}



TMacList * PushNewMac(TOsnmaParameters *osnmaPars,TMackList * mackList)
{
	TMacList *ptr = mackList->macList;
	TMacList *prev = NULL;

	while(ptr!=NULL)
	{
		prev = ptr;
		ptr = ptr->nextMac;
	}
	ptr = (TMacList *)malloc(sizeof(TMacList));
	if(prev==NULL)
	{
		/* list was empty  */
		mackList->macList = ptr;
	}
	else
	{
		/* add at the end of the list */
		prev->nextMac = ptr;
	}
	if(ptr!=NULL)
	{
		ptr->mac = (unsigned char *)malloc(osnmaPars->macSizeBytes*sizeof(unsigned char));
		ptr->macseq = (unsigned char *)malloc(MACSEQ_SIZE_BYTES*sizeof(unsigned char));
		ptr->macInfoFlex = (unsigned char *)malloc(MAX_NUMBER_MACINFO_FLEX*MACINFO_SIZE_BYTES*sizeof(unsigned char));
		ptr->prevMac = prev;
		ptr->nextMac = NULL;
	}
	return ptr;
}


void DeleteAMacFromMacList(TMackList * mackPtr,TMacList *macPtr)
{
	if(macPtr==NULL)
	{
		/* nothing to delete */
		return;
	}
	/* adjust links */
	if((macPtr->prevMac)!=NULL)
	{
		(macPtr->prevMac)->nextMac = macPtr->nextMac;
	}
	else
	{
		/* new head */
		mackPtr->macList = macPtr->nextMac;
	}
	if((macPtr->nextMac)!=NULL)
	{
		(macPtr->nextMac)->prevMac = macPtr->prevMac;
	}
	free(macPtr->mac);
	free(macPtr->macseq);
	free(macPtr->macInfoFlex);
	free(macPtr);
}


void DeleteMacsOlderThan(TMackList * mackPtr,int wn,int tow)
{
	/* only in the current MAC list i.e. for one channel */
	TMacList *macPtr = mackPtr->macList;
	TMacList *nextPtr;

	while(macPtr!=NULL)
	{
		nextPtr = macPtr->nextMac;
		if((macPtr->wnMackStart<wn) || ((macPtr->wnMackStart==wn) && (macPtr->towMackStart<=tow)))
		{
			/* the current element is deleted */
			DeleteAMacFromMacList(mackPtr,macPtr);
		}
		macPtr = nextPtr;
	}
}

void DeleteMacIfKeyOlderThan(TMackList * mackPtr,TMacList *macPtr, int wn,int tow)
{
	if((macPtr->wnKey<wn) || ((macPtr->wnKey==wn) && (macPtr->towKey<tow)))
	{
		/* the current element is deleted */
		DeleteAMacFromMacList(mackPtr,macPtr);
	}
}


void DeleteMacList(TMackList * mackList)
{
	TMacList *ptr = mackList->macList;
	TMacList *next;

	while(ptr!=NULL)
	{
		next = ptr->nextMac;
		free(ptr->mac);
		free(ptr->macseq);
		free(ptr->macInfoFlex);
		free(ptr);
		ptr = next;
	}
	mackList->macList = NULL;
}


TKeyList * PushNewKey(TOsnmaParameters *osnmaPars,TMackList * mackList)
{
	TKeyList *ptr = mackList->keyList;
	TKeyList *prev = NULL;

	while(ptr!=NULL)
	{
		prev = ptr;
		ptr = ptr->nextKey;
	}
	ptr = (TKeyList *)malloc(sizeof(TKeyList));
	if(prev==NULL)
	{
		/* list was empty  */
		mackList->keyList = ptr;
	}
	else
	{
		/* add at the end of the list */
		prev->nextKey = ptr;
	}
	if(ptr!=NULL)
	{
		ptr->key = (unsigned char *)malloc((osnmaPars->keySize/8)*sizeof(unsigned char));
		ptr->prevKey = prev;
		ptr->nextKey = NULL;
	}
	return ptr;
}


void DeleteAKeyFromKeyList(TMackList * mackList,TKeyList *keyPtr)
{
	if(keyPtr==NULL)
	{
		/* nothing to delete */
		return;
	}
	/* adjust links */
	if((keyPtr->prevKey)!=NULL)
	{
		(keyPtr->prevKey)->nextKey = keyPtr->nextKey;
	}
	else
	{
		/* new head */
		mackList->keyList = keyPtr->nextKey;
	}
	if((keyPtr->nextKey)!=NULL)
	{
		(keyPtr->nextKey)->prevKey = keyPtr->prevKey;
	}
	free(keyPtr->key);
	free(keyPtr);
}


void DeleteKeysOlderThan(TMackList * mackPtr,int wn,int tow)
{
	/* only in the current key list i.e. for one channel */
	TKeyList *keyPtr = mackPtr->keyList;
	TKeyList *nextPtr;

	while(keyPtr!=NULL)
	{
		nextPtr = keyPtr->nextKey;
		if((keyPtr->wnKey<wn) || ((keyPtr->wnKey==wn) && (keyPtr->towKey<=tow)))
		{
			/* the current element is deleted */
			DeleteAKeyFromKeyList(mackPtr,keyPtr);
		}
		keyPtr = nextPtr;
	}
}


void DeleteKeyList(TMackList * mackList)
{
	TKeyList *ptr = mackList->keyList;
	TKeyList *next;

	while(ptr!=NULL)
	{
		next = ptr->nextKey;
		free(ptr->key);
		free(ptr);
		ptr = next;
	}
	mackList->keyList = NULL;
}


TNavDataList * FindNavData(TNavDataList *navData,int wn,int tow)
{
	while(navData!=NULL)
	{
		if((navData->wnSubframeStart==wn) && (navData->towSubframeStart==tow))
		{
			return navData;
		}
		navData = navData->nextNavData;
	}
	return navData;
}


TNavDataList * PushNewNavData(TMackList * mackList)
{
	TNavDataList *ptr = mackList->navDataList;
	TNavDataList *prev = NULL;

	while(ptr!=NULL)
	{
		prev = ptr;
		ptr = ptr->nextNavData;
	}
	ptr = (TNavDataList *)malloc(sizeof(TNavDataList));
	if(prev==NULL)
	{
		/* list was empty  */
		mackList->navDataList = ptr;
	}
	else
	{
		/* add at the end of the list */
		prev->nextNavData = ptr;
	}
	if(ptr!=NULL)
	{
		if(mackList->satType==GPS_SAT)
		{
			/* GPS navigation message */
			ptr->wordTypes = (int *)malloc(GPS_SUBFRAMES_PER_FRAME*sizeof(int));
			ptr->navData = (unsigned char *)malloc(GPS_FRAME_BYTES*sizeof(unsigned char));
		}
		else //	if(mackList->satType==GALILEO_SAT)
		{
			/* Galileo navigation message (and others) */
			ptr->wordTypes = (int *)malloc(GAL_PAGES_PER_SUBFRAME*sizeof(int));
			ptr->navData = (unsigned char *)malloc(GAL_BYTES_PER_SUBFRAME*sizeof(unsigned char));
		}
		ptr->prevNavData = prev;
		ptr->nextNavData = NULL;
	}
	return ptr;
}


void DeleteANavDataFromNavDataList(TMackList * mackList,TNavDataList *navPtr)
{
	if(navPtr==NULL)
	{
		/* nothing to delete */
		return;
	}
	/* adjust links */
	if((navPtr->prevNavData)!=NULL)
	{
		(navPtr->prevNavData)->nextNavData = navPtr->nextNavData;
	}
	else
	{
		/* new head */
		mackList->navDataList = navPtr->nextNavData;
	}
	if((navPtr->nextNavData)!=NULL)
	{
		(navPtr->nextNavData)->prevNavData = navPtr->prevNavData;
	}
	free(navPtr->wordTypes);
	free(navPtr->navData);
	free(navPtr);
}


void DeleteNavDataOlderThan(TMackList * mackPtr,int wn,int tow)
{
	/* only in the current key list i.e. for one channel */
	TNavDataList *navPtr = mackPtr->navDataList;
	TNavDataList *nextPtr;

	while(navPtr!=NULL)
	{
		nextPtr = navPtr->nextNavData;
		if((navPtr->wnSubframeStart<wn) || ((navPtr->wnSubframeStart==wn) && (navPtr->towSubframeStart<tow)))
		{
			/* the current element is deleted */
			DeleteANavDataFromNavDataList(mackPtr,navPtr);
		}
		navPtr = nextPtr;
	}
}


void DeleteNavDataList(TMackList * mackList)
{
	TNavDataList *ptr = mackList->navDataList;
	TNavDataList *next;

	while(ptr!=NULL)
	{
		next = ptr->nextNavData;
		free(ptr->wordTypes);
		free(ptr->navData);
		free(ptr);
		ptr = next;
	}
	mackList->navDataList = NULL;
}


int GetOldestTowInNavDataList(TMackList *head,TSatConstellation satType,TSatSignal satSignal,int satId,int chId,int *wn,int *tow)
{
	TMackList * mackPtr;
	mackPtr = FindInMackList(head,satType,satSignal,satId,chId);

	if(mackPtr==NULL)
	{
		return FAILURE_RETURN;
	}
	if(mackPtr->navDataList==NULL)
	{
		return FAILURE_RETURN;
	}
	*wn = mackPtr->navDataList->wnSubframeStart;
	*tow = mackPtr->navDataList->towSubframeStart;
	return SUCCESS_RETURN;
}
