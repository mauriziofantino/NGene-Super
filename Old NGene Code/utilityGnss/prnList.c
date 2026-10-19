/**
 * @file prnList.c
 *
 * @author Mario Nicola
 *
 * @date 11/08/2014
 *
 * @version 1.0.0
 *
 * @brief Functions that manage a list of PRNs.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "spreadingCodes.h"
#include "prnList.h"
#include "ngeneConstants.h"


/**
 * @def MAXIMUM_SUPPORTED_GALILEO_PRN
 * @brief In ICD, GALILEO_NUMBER_OF_PRNS (50) PRNs are reported,
 *        but almanac carries data only for 36 (MAXIMUM_SUPPORTED_GALILEO_PRN).
 */
#define MAXIMUM_SUPPORTED_GALILEO_PRN	36


int CreatePrnList(TPrnList *ptr,int gpsEnabled,int galileoEnabled,int egnosEnabled,int numOfRealGalileoPrns,int *realGalileoPrns)
{
    unsigned int index;

    ptr->head = NULL;           /* the list is empty */

    /* insert in the list all possible GPS satellites */
    if(gpsEnabled==TRUE_FLAG)
    {
    	for(index=BASE_PRN;index<=GPS_NUMBER_OF_PRNS;index++)
    	{
    		if(InsertElementIntoPrnList(GPS_SAT,index,ptr)!=SUCCESS_RETURN)
    		{
    			return FAILURE_RETURN;
    		}
    	}
    }
    /* insert in the list all possible EGNOS satellites */
    if(egnosEnabled==TRUE_FLAG)
    {
    	if(InsertElementIntoPrnList(EGNOS_SAT,EGNOS_FIRST_PRN,ptr)!=SUCCESS_RETURN)
    	{
    		return FAILURE_RETURN;
    	}
    	if(InsertElementIntoPrnList(EGNOS_SAT,EGNOS_SECOND_PRN,ptr)!=SUCCESS_RETURN)
    	{
    		return FAILURE_RETURN;
    	}
    	if(InsertElementIntoPrnList(EGNOS_SAT,EGNOS_THIRD_PRN,ptr)!=SUCCESS_RETURN)
    	{
    		return FAILURE_RETURN;
    	}
    	if(InsertElementIntoPrnList(EGNOS_SAT,EGNOS_FOURTH_PRN,ptr)!=SUCCESS_RETURN)
    	{
    		return FAILURE_RETURN;
    	}
    }
    /* insert in the list all possible Galileo satellites */
    if(galileoEnabled==TRUE_FLAG)
    {
    	if((numOfRealGalileoPrns>0))
    	{
    		/* Only PRNs of real satellites must be used */
    		for(index=0;index<(unsigned int)numOfRealGalileoPrns;index++)
    		{
    			if(InsertElementIntoPrnList(GALILEO_SAT,realGalileoPrns[index],ptr)!=SUCCESS_RETURN)
    			{
    				return FAILURE_RETURN;
    			}
    		}
    	}
    	else
    	{
			#ifdef MAXIMUM_SUPPORTED_GALILEO_PRN
    		for(index=BASE_PRN;index<=MAXIMUM_SUPPORTED_GALILEO_PRN;index++)
			#else
    		for(index=BASE_PRN;index<=GALILEO_NUMBER_OF_PRNS;index++)
			#endif
    		{
    			if(InsertElementIntoPrnList(GALILEO_SAT,index,ptr)!=SUCCESS_RETURN)
    			{
    				return FAILURE_RETURN;
    			}
    		}
    	}
    }
    return SUCCESS_RETURN;
}


int DestroyPrnList(TPrnList *ptr)
{
    TPrnListElement *currentElement;
    TPrnListElement *toBeDeleted;

    currentElement = ptr->head;
    while(currentElement!=NULL)
    {
        toBeDeleted = currentElement;
        currentElement = currentElement->nextPrn;
        free(toBeDeleted);
    }
    ptr->head = NULL;           /* the list is empty */
    return SUCCESS_RETURN;
}


int GetListLength(TPrnList *ptr)
{
	int n = 0;
	TPrnListElement *currentElement;

	currentElement = ptr->head;
	while(currentElement!=NULL)
	{
		n++;
		currentElement = currentElement->nextPrn;
	}
	return n;
}


int DeleteElementFromPrnList(TSatConstellation constellation, int prn, TPrnList *ptr)
{
    TPrnListElement *currentElement; /* PRN in the list that precedes the one that will be deleted */
    TPrnListElement *toBeDeleted;    /* PRN to be deleted */

    if(prn==INVALID_PRN)
    {
    	return SUCCESS_RETURN;
    }
    toBeDeleted = ptr->head;
    currentElement = NULL;
    /* search the element to be deleted */
    while((toBeDeleted!=NULL) && ((toBeDeleted->prn!=prn) || (toBeDeleted->prnConstellation!=constellation)))
    {
        currentElement = toBeDeleted;
        toBeDeleted = toBeDeleted->nextPrn;
    }
    if(toBeDeleted==NULL)     /* not found */
    {
        return FAILURE_RETURN;
    }
    if(currentElement==NULL)  /* the element to be deleted is the first one in the list */
    {
        ptr->head = toBeDeleted->nextPrn;
    }
    else
    {
        currentElement->nextPrn = toBeDeleted->nextPrn;
    }
    free(toBeDeleted);
    return SUCCESS_RETURN;
}


int InsertElementIntoPrnList(TSatConstellation constellation, int prn, TPrnList *ptr)
{
    TPrnListElement *currentElement;    /* last PRN in the list */
    TPrnListElement *toBeInserted;  /* new PRN to be inserted in the list */

    toBeInserted = (TPrnListElement *) malloc(sizeof(TPrnListElement));
    if(toBeInserted==NULL)
    {
    	return FAILURE_RETURN;
    }
    toBeInserted->prnConstellation = constellation;
    toBeInserted->prn = prn;
    toBeInserted->nextPrn = NULL;
    toBeInserted->available = PRN_AVAILABLE;

    if(ptr->head==NULL)       /* this is the first element inserted in the list */
    {
        ptr->head = toBeInserted;
        return SUCCESS_RETURN;
    }
    currentElement = ptr->head;
    while(currentElement->nextPrn!=NULL)  /* at the end of the loop, currentElement is the last element of the list */
    {
        currentElement = currentElement->nextPrn;
    }
    currentElement->nextPrn = toBeInserted;
    return SUCCESS_RETURN;
}


int CopyPrnList(TPrnList *srcPtr,TPrnList *dstPtr)
{
	TPrnListElement *prnPtr;

	if(srcPtr==NULL)
		return FAILURE_RETURN;

	DestroyPrnList(dstPtr);
	prnPtr = srcPtr->head;
	while(prnPtr!=NULL)
	{
		if(InsertElementIntoPrnList(prnPtr->prnConstellation,prnPtr->prn,dstPtr)!=SUCCESS_RETURN)
			return FAILURE_RETURN;
		prnPtr = prnPtr->nextPrn;
	}

	return SUCCESS_RETURN;
}


/**
 * @brief MoveToTheTail moves an element of the list to the last position.
 *
 * @param constellation is the type of satellite
 * @param prn is the index of the satellite
 * @param ptr points to the PRN list.
 *
 * @return SUCCESS_RETURN if successful, FAILURE_RETURN otherwise
 */
int MoveToTheTail(TSatConstellation constellation, int prn, TPrnList *ptr)
{
    TPrnListElement *toBeMoved;     /* points to the element that will be moved to the last position */
    TPrnListElement *oldPrevious;   /* element that precedes the one that will be moved */
    TPrnListElement *newPrevious;   /* last PRN in the list */

    /* search element to be moved */
    toBeMoved = ptr->head;
    oldPrevious = NULL;
    while((toBeMoved!=NULL) && ((toBeMoved->prn!=prn) || (toBeMoved->prnConstellation!=constellation)))
    {
        oldPrevious = toBeMoved;
        toBeMoved = toBeMoved->nextPrn;
    }
    if(toBeMoved==NULL)       /* not found */
    {
        return FAILURE_RETURN;
    }

    /* search tail of the list */
    newPrevious = toBeMoved->nextPrn;
    while((newPrevious != NULL) && (newPrevious->nextPrn != NULL))
    {
        newPrevious = newPrevious->nextPrn;
    }
    if(newPrevious==NULL)     /* it's the last one already */
    {
        return SUCCESS_RETURN;
    }

    /* delete from the list */
    if(oldPrevious==NULL)     /* the moved one is the first in the list */
    {
        ptr->head = toBeMoved->nextPrn;
    }
    else
    {
        oldPrevious->nextPrn = toBeMoved->nextPrn;
    }
    /* insert at the tail */
    newPrevious->nextPrn = toBeMoved;
    toBeMoved->nextPrn = NULL;
    return SUCCESS_RETURN;
}


int GetNumberOfAvailableElementsInTheList(TPrnList *ptr)
{
	TPrnListElement *currentElement;
	int available = 0;

	currentElement = ptr->head;
	while(currentElement!=NULL)
	{
		if(currentElement->available == PRN_AVAILABLE)
			available++;
		currentElement = currentElement->nextPrn;
	}
	return available;
}


int GetNextAvailableFromPrnList(TSatConstellation constellation, TPrnList *ptr)
{
    TPrnListElement *currentElement;

    currentElement = ptr->head;
    while((currentElement!=NULL) && ((currentElement->available!=PRN_AVAILABLE) || (currentElement->prnConstellation!=constellation)))
    {
        currentElement = currentElement->nextPrn;
    }

    if(currentElement == NULL)  /* not found */
    {
        return INVALID_PRN;
    }
    MoveToTheTail(constellation, currentElement->prn, ptr);
    currentElement->available = PRN_NOT_AVAILABLE;
    return currentElement->prn;
}


int FindInPrnList(TSatConstellation constellation, int prn, TPrnList *ptr)
{
	TPrnListElement *currentElement;

	currentElement = ptr->head;
	while((currentElement!=NULL) && ((currentElement->prn!=prn) || (currentElement->prnConstellation!=constellation)))
	{
		currentElement = currentElement->nextPrn;
	}

	if(currentElement == NULL)  /* not found */
	{
		return FAILURE_RETURN;
	}
	return SUCCESS_RETURN;
}


int FindAndOccupyInPrnList(TSatConstellation constellation, int prn, TPrnList *ptr)
{
    TPrnListElement *currentElement;

    currentElement = ptr->head;
    while((currentElement!=NULL) && ((currentElement->prn!=prn) || (currentElement->prnConstellation!=constellation)))
    {
        currentElement = currentElement->nextPrn;
    }

    if(currentElement == NULL)  /* not found */
    {
        return FAILURE_RETURN;
    }
    if(currentElement->available==PRN_NOT_AVAILABLE) /* already busy */
    {
    	return FAILURE_RETURN;
    }
    MoveToTheTail(constellation, currentElement->prn, ptr);
    currentElement->available = PRN_NOT_AVAILABLE;
    return SUCCESS_RETURN;
}


int FindAndFreeInPrnList(TSatConstellation constellation, int prn, TPrnList *ptr)
{
    TPrnListElement *currentElement;

    currentElement = ptr->head;
    while((currentElement!=NULL) && ((currentElement->prn!=prn) || (currentElement->prnConstellation!=constellation)))
    {
        currentElement = currentElement->nextPrn;
    }

    if(currentElement==NULL)  /* not found */
    {
        return FAILURE_RETURN;
    }
    currentElement->available = PRN_AVAILABLE;
    return SUCCESS_RETURN;
}


void FreeAllElementsInTheList(TPrnList *ptr,int freeGps,int freeGal,int freeEgnos)
{
	TPrnListElement *currentElement;

	currentElement = ptr->head;
	while(currentElement!=NULL)
	{
		currentElement->available = PRN_NOT_AVAILABLE;
		if((currentElement->prnConstellation==GPS_SAT) && (freeGps==TRUE_FLAG))
			currentElement->available = PRN_AVAILABLE;
		if((currentElement->prnConstellation==GALILEO_SAT) && (freeGal==TRUE_FLAG))
			currentElement->available = PRN_AVAILABLE;
		if((currentElement->prnConstellation==EGNOS_SAT) && (freeEgnos==TRUE_FLAG))
			currentElement->available = PRN_AVAILABLE;

		currentElement = currentElement->nextPrn;
	}
}


void FromStringToPrn(char *prnDescription, TSatConstellation *constellation, int *prn)
{
	*constellation = INVALID_SAT;
	*prn = INVALID_PRN;

	if(strncmp(prnDescription,"GPS",3)==0)
	{
		*constellation = GPS_SAT;
		sscanf(&(prnDescription[3]),"%d", prn);
	}
	else if(strncmp(prnDescription,"EGNOS",5)==0)
	{
		*constellation = EGNOS_SAT;
		sscanf(&(prnDescription[5]),"%d",prn);
	}
	else /* if(strncmp(prnDescription,"Galileo",7)==0) */
	{
	    	*constellation = GALILEO_SAT;
	    	sscanf(&(prnDescription[7]),"%d",prn);
	}
}


void FromPrnToString(TSatConstellation constellation, int prn, char *prnDescription)
{
	if((prn==INVALID_PRN) && (constellation==EGNOS_SAT))
		prn=EGNOS_FIRST_PRN;
	else if(prn==INVALID_PRN)
		prn=BASE_PRN;

	if(constellation==GPS_SAT)
	{
		sprintf(prnDescription,"GPS%d",prn);
	}
	else if(constellation==EGNOS_SAT)
	{
		sprintf(prnDescription,"EGNOS%d",prn);
	}
	else if(constellation==GALILEO_SAT)
	{
		sprintf(prnDescription,"Galileo%d",prn);
	}
	else
	{
		sprintf(prnDescription,"INVALID");
	}
}


