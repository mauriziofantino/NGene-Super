/*
 * prnListDataStructure.h
 *
 *  Created on: Jun 23, 2016
 *      Author: Micaela Troglia Gamba
 */

#ifndef UTILITYGNSS_PRNLISTDATASTRUCTURE_H_
#define UTILITYGNSS_PRNLISTDATASTRUCTURE_H_

/**
 * @enum PrnAvailabilityEnum
 *
 * @brief is the type that indicates if a PRN has been assigned to one channel.
 */
typedef enum PrnAvailabilityEnum
{
    PRN_AVAILABLE,     /**< PRN is not assigned to any channel */
    PRN_NOT_AVAILABLE, /**< PRN is already assigned to one channel */
    SAT_NOT_VISIBLE    /**< PRN not assigned and not visible (in case of warm starts) */
} TPrnAvailability;


/**
 * @enum PvtInfoAvailabilityEnum
 *
 * @brief is the type that indicates if PVT info for fast re-acquisition of a a PRN assigned to a channel are available or not.
 */
typedef enum PvtInfoAvailabilityEnum
{
    PVT_INFO_AVAILABLE,     /**< PVT info for that PRN are available */
	PVT_INFO_NOT_AVAILABLE, /**< PVT info for that PRN are not available */
} TPvtInfoAvailability;

/**
 * @struct PrnListElementStruct
 *
 * @brief Describes a single element in the PRN list.
 */
typedef struct PrnListElementStruct
{
    TSatConstellation prnConstellation;   /**< Constellation of the current PRN. */
    int prn;                              /**< Index of the current PRN. */
    TPrnAvailability available;           /**< If 1 the PRN is available, else the PRN is allocated to a channel */
    struct PrnListElementStruct *nextPrn; /**< Points to the next PRN in the list.
                                               If NULL the current PRN is the last in the list. */
} TPrnListElement;


/**
 * @struct PrnListStruct
 *
 * @brief Describes a PRN list.
 */
typedef struct PrnListStruct
{
    TPrnListElement *head; /**< Points to the first element of the PRN list. If NULL the list is empty. */
} TPrnList;



#endif /* UTILITYGNSS_PRNLISTDATASTRUCTURE_H_ */
