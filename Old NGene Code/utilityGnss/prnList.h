/**
 * @file prnList.h
 *
 * @author Mario Nicola
 *
 * @date 11/08/2014
 *
 * @version 1.0.0
 *
 * @brief Functions that manage a list of PRNs.
 */

#ifndef PRN_LIST_H_INCLUDED
#define PRN_LIST_H_INCLUDED

#include "prnListDataStructure.h"
#include "ngeneConfiguration.h"
#include "pvtDataStructure.h"
#include "ngeneDataStructure.h"



/**
 * @brief Create a PRN list
 *
 * @details In the PRN list information about PRN that are used in order to
 *          manage channel scheduling are stored. At the start all PRNs are included in the list.
 *
 * @param ptr points to the PRN list.
 * @param gpsEnabled is FALSE_FLAG when GPS PRNs are not inserted in the PRN list
 * @param galileoEnabled is FALSE_FLAG when Galileo PRNs are not inserted in the PRN list
 * @param egnosEnabled is FALSE_FLAG when EGNOS PRNs are not inserted in the PRN list
 * @param numOfRealGalileoPrns is the number of Galileo PRNs reported in the Galileo_real_PRNs.txt configuration file
 * @param realGalileoPrns points to the list of real Galileo PRNs
 *
 * @return SUCCESS_RETURN if successful, FAILURE_RETURN otherwise
 */
int CreatePrnList(TPrnList *ptr,int gpsEnabled,int galileoEnabled,int egnosEnabled,int numOfRealGalileoPrns,int *realGalileoPrns);

/**
 * @brief Delete a PRN list
 *
 * @details In the prn list information about PRN that are used in order to
 *          manage channel scheduling are stored. At the start all PRNs are included in the list.
 *
 * @param ptr points to the PRN list.
 *
 * @return SUCCESS_RETURN if successful, FAILURE_RETURN otherwise
 */
int DestroyPrnList(TPrnList *ptr);

/**
 * @brief Get the number of all the elements in the list without regard of the availability
 *
 * @param ptr points to the PRN list.
 *
 * @return Return the number of elements in the list
 */
int GetListLength(TPrnList *ptr);

/**
 * @brief Delete an element from a PRN list
 *
 * @details The element in the list with specified PRN and constellation is deleted,
 *          then is no more available until it's inserted again in the list.
 *
 * @param constellation is the type of satellite
 * @param prn is the index of the satellite
 * @param ptr points to the PRN list.
 *
 * @return SUCCESS_RETURN if successful, FAILURE_RETURN otherwise
 */
int DeleteElementFromPrnList(TSatConstellation constellation, int prn, TPrnList *ptr);

/**
 * @brief Insert an element into a PRN list
 *
 * @details Insert a new element in the list with specified PRN and constellation.
 *          The new element is inserted in the tail of the list.
 *
 * @param constellation is the type of satellite
 * @param prn is the index of the satellite
 * @param ptr points to the PRN list.
 *
 * @return SUCCESS_RETURN if successful, FAILURE_RETURN otherwise
 */
int InsertElementIntoPrnList(TSatConstellation constellation, int prn, TPrnList *ptr);

/**
 * @brief Copies a list of PRN into a new list (elements in the new list will be available)
 *
 * @param srcPtr points to the source PRN list
 * @param dstPtr points to the destination PRN list
 *
 * @return SUCCESS_RETURN if successful, FAILURE_RETURN otherwise
 */
int CopyPrnList(TPrnList *srcPtr,TPrnList *dstPtr);

/**
 * @brief Counts how many PRN in the list are tagged available
 *
 * @param ptr points to the PRN list
 *
 * @return Number of available PRNs
 */
int GetNumberOfAvailableElementsInTheList(TPrnList *ptr);

/**
 * @brief Finds and returns the first available PRN in the list.
 *
 * @details The function searches for the first PRN of the the type specified with the input.
 *          The search starts from the head of the list. The found PRN is marked as not available
 *          and it is put in the tail of the list, then its index is returned.
 *          If a suitable PRN is not found, then INVALID_PRN value is returned.
 *
 * @param constellation is the type of satellite
 * @param ptr points to the PRN list.
 *
 * @return selected PRN, INVALID_PRN if no suitable PRN is found in the list
 */
int GetNextAvailableFromPrnList(TSatConstellation constellation, TPrnList *ptr);

/**
 * @brief Checks if the desired PRN is in the list.
 *
 *
 * @param constellation is the type of satellite
 * @param prn is the index of the satellite
 * @param ptr points to the PRN list.
 *
 * @return FAILURE_RETURN if the required PRN does not exist, SUCCESS_RETURN elsewhere.
 */
int FindInPrnList(TSatConstellation constellation, int prn, TPrnList *ptr);

/**
 * @brief Finds the desired PRN in the list and makes it not available.
 *
 * @details The function searches for the PRN specified with the input.
 *          The search starts from the head of the list. The found PRN is marked as not available,
 *          it is put in the tail of the list.
 *
 * @param constellation is the type of satellite
 * @param prn is the index of the satellite
 * @param ptr points to the PRN list.
 *
 * @return FAILURE_RETURN if the required PRN does not exist or is is already busy, SUCCESS_RETURN elsewhere.
 */
int FindAndOccupyInPrnList(TSatConstellation constellation, int prn, TPrnList *ptr);

/**
 * @brief Finds the desired PRN in the list and makes it again available.
 *
 * @details The function searches for the PRN specified with the input.
 *          The search starts from the head of the list. The found PRN is marked as available,
 *          it is put in the tail of the list.
 *
 * @param constellation is the type of satellite
 * @param prn is the index of the satellite
 * @param ptr points to the PRN list.
 *
 * @return selected PRN, INVALID_PRN if no suitable PRN is found in the list
 */
int FindAndFreeInPrnList(TSatConstellation constellation, int prn, TPrnList *ptr);

/**
 * @brief Change the tag of all elements in the list to the PRN_AVAILABLE value
 *
 * @param ptr points to the PRN list.
 * @param freeGps when FALSE_FLAG, elements related to GPS are not reset to PRN_AVAILABLE
 * @param freeGal when FALSE_FLAG, elements related to Galileo are not reset to PRN_AVAILABLE
 * @param freeEgnos when FALSE_FLAG, elements related to EGNOS are not reset to PRN_AVAILABLE
 */
void FreeAllElementsInTheList(TPrnList *ptr,int freeGps,int freeGal,int freeEgnos);

/**
 * @brief It translates a string describing a PRN into constellation and prn index.
 *
 * @details The format of the string is: GPSX, EGNOSX or GALILEOX where X is the PRN index.
 *
 * @param prnDescription is a string describing the PRN
 * @param constellation points to the type of the PRN
 * @param prn points to the PRN index.
 */
void FromStringToPrn(char *prnDescription, TSatConstellation *constellation, int *prn);

/**
 * @brief It translates a couple constellation and prn index into a string describing the PRN.
 *
 * @details The format of the string is: GPSX, EGNOSX or GALILEOX where X is the PRN index.
 *
 * @param it is the type of the PRN
 * @param it is the PRN index
 * @param prnDescription is a string describing the PRN.
 */
void FromPrnToString(TSatConstellation constellation, int prn, char *prnDescription);


#endif /* PRN_LIST_H_ INCLUDED*/
