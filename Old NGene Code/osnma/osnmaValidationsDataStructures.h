/**
 * @file osnmaValidationsDataStructures.h
 *
 * @author Mario Nicola
 *
 * @date 03/11/2020
 *
 * @version 1.0.0
 *
 * @brief Data structures for the management of  the MAC validations
 */

#ifndef OSNMA_VALIDATIONS_DATA_STRUCTURES_H_INCLUDED
#define OSNMA_VALIDATIONS_DATA_STRUCTURES_H_INCLUDED


#include "osnmaDataStructures.h"



/**
 * @struct MacValidationsStruct
 *
 * @brief Used to store the results of the performed MAC validations.
 */
typedef struct MacValidationsStruct
{
	/* time values */
	int wnSubframeStart; /**< Starting time of the subframe containing the MAC, WN */
	int towSubframeStart; /**< Starting time of the subframe containing the MAC, TOW [s] */
	/* MAC infos */
	TAdkd adkd;
	unsigned int iod;
	/* validation results */
	TValidationStatus validityFlag;
	/* next element in the list */
	struct MacValidationsStruct *nextMac;
} TMacValidations;


/**
 * @struct MacAccumulationsStruct
 *
 * @brief Used to store the variables needed for the MAC accumulations.
 */
typedef struct MacAccumulationsStruct
{
	unsigned int iod;
	int counter;
	TValidationStatus authenticationFlag;
} TMacAccumulations;



/**
 * @struct  OsnmaAuthenticationStruct
 * @brief Contains all the variables used to manage the authentication of the signals tracked by the receiver
 *        using the MAC validations of Galileo OSNMA.
 */
typedef struct OsnmaAuthenticationStruct
{
	/* parameters */
	int nCh; /**< number of channels in the receiver (from the receive configuration) */
	int numAccumulations; /**< number of MACs to be accumulated (current value, could change with chain renewal/revocation) */
	/* channel info */
	int *prnN; /**< PRN (in OSNMA convention) allocated to each channel */
	/* MAC validations are saved in a matrix
	 * - one row for each authenticated channel
	 * - one column for each authenticating channel
	 * They are stored row-wise, i.e.
	 *   0 -> ch0 authenticated, ch0 authenticating
	 *   1 -> ch0 authenticated, ch1 authenticating
	 *   2 -> ch0 authenticated, ch2 authenticating
	 * ...
	 * nCh -> ch1 authenticated, ch0 authenticating
	 * ...
	 * */
	TMacValidations **macs; /**< MAC validations matrix */
	/* support for MAC accumulation. Variables are saved in a matrix
	 * - one row for each authenticated channel
	 * - one column for each ADKD value (included not supported and reserved values)
	 * They are stored row-wise, i.e.
	 *   0 -> ch0 authenticated, ADKD 0
	 *   1 -> ch0 authenticated, ADKD 1
	 *   2 -> ch0 authenticated, ADKD 2
	 * ...
	 *  16 -> ch1 authenticated, ADKD 0
	 * ...
	 * */
	TMacAccumulations *accumulations;
	/* final authentication flags (one flag for each channel) */
	TValidationStatus *authenticationFlags;
	/* time (rxCounter) for the last update of the authenticationFlags (one value for each channel) */
	unsigned long long int *rxCounter;
} TOsnmaAuthentication;


#endif /* OSNMA_VALIDATIONS_DATA_STRUCTURES_H_INCLUDED */
