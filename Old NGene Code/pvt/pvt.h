/**
 * @file pvt.h
 *
 * @author Mario Nicola
 *
 * @date 22/10/2014
 *
 * @version 1.0.0
 *
 * @brief Main functions used for PVT computation.
 */

#ifndef PVT_H_INCLUDED
#define PVT_H_INCLUDED

/**
 * @brief Initializes the structure used to store the data for PVT computation
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int InitPvtData(TNGeneConfiguration *cfg,TNGeneData *rx);

/**
 * @brief De-initializes the structure used for the PVT computation
 *
 * @param rx points to the receiver base data structure
 */
void DeinitPvtData(TNGeneData *rx);

int PvtManager(TNGeneConfiguration *cfg,TNGeneData *rx);

int TimeForANewPvt(TNGeneConfiguration *cfg,TNGeneData *rx);


#endif /* PVT_H_INCLUDED */
