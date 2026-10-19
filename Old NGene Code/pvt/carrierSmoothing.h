/**
 * @file carrierSmoothing.h
 *
 * @author Mario Nicola
 *
 * @date 07/11/2014
 *
 * @version 1.0.0
 *
 * @brief Functions used to perform carrier smoothing.
 */

#ifndef CARRIER_SMOOTHING_H_INCLUDED
#define CARRIER_SMOOTHING_H_INCLUDED

#include "ngeneConfiguration.h"
#include "pvtDataStructure.h"

/**
 * @def CSM_TRANSIENT_LENGTH
 * @brief The CSM supports an initial transient: after 5 executions,
 *        the CSM can begin standard operations
 */
#define CSM_TRANSIENT_LENGTH	5


/**
 * @brief Clear CSM variables for one channel
 *
 * @param csmDataCh points to the CSM variables for one channel
 */
void ClearCsmDataChannel(TCarrierSmoothingData *csmDataCh);


/**
 * @brief Clear CSM variables for all the channels
 *
 * @param cfg points to the receiver configuration
 * @param pvtData points to the structure containing variables used for PVT
 * @param pvtRes points to the structure containing results of the PVT
 */
void ClearCsmData(TNGeneConfiguration *cfg,TPvtData *pvtData,TPvtResults *pvtRes);


/**
 * @brief Initialize carrier smoothing variables.
 *        Allocates structure that store CSM variables.
 *
 * @param cfg points to the receiver configuration
 * @param pvtData points to the structure containing variables used for PVT
 * @param pvtRes points to the structure containing results of the PVT
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int InitCarrierSmoothing(TNGeneConfiguration *cfg,TPvtData *pvtData,TPvtResults *pvtRes);


/**
 * @brief Frees the memory allocated for the carrier smoothing variables
 *
 * @param pvtData points to the structure containing variables used for PVT
 */
void DeinitCarrierSmoothing(TPvtData *pvtData);


/**
 * @brief Called by channel manager after a tracking execution:
 *        it updates the cumulated Doppler variable.
 *
 * @param cfg points to the receiver configuration
 * @param ch points to the channel to be initialized
 * @param pvtData points to the structure containing variables used for PVT
 * @param pvtRes points to the structure containing results of the PVT
 */
void UpdateCarrierSmoothingMeasure(TNGeneConfiguration *cfg,TChannel *ch,TPvtData *pvtData,TPvtResults *pvtRes);


/**
 * @brief Performs the smoothing of the pseudoranges: the result is saved in the pseudorangeCsm variable.
 *
 * @param cfg points to the receiver configuration
 * @param pvtData points to the structure containing variables used for PVT
 * @param pvtRes points to the structure containing results of the PVT
 */
void PerformCarrierSmoothing(TNGeneConfiguration *cfg,TPvtData *pvtData,TPvtResults *pvtRes);


/**
 * @brief Updates the variable representing the cumulated biases for GPS and Galileo
 *
 * @param pvtRes points to the structure containing results of the PVT
 */
void UpdateCumulatedBiases(TPvtResults *pvtRes);

#endif /* CARRIER_SMOOTHING_H_INCLUDED */
