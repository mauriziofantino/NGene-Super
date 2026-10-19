/**
 * @file nonCoherentSumsManager.h
 *
 * @author Mario Nicola
 *
 * @date 17/12/2014
 *
 * @version 1.0.0
 *
 * @brief Functions that manage the current number of non coherent sums.
 */

#ifndef ACQUISITION_NON_COHERENT_SUMS_MANAGER_H_INCLUDED
#define ACQUISITION_NON_COHERENT_SUMS_MANAGER_H_INCLUDED

#include "acquisitionDataStructure.h"
#include "ngeneConfiguration.h"
#include "ngeneDataStructure.h"

/**
 * @brief Initializes the variables used for the management of the number of non coherent sums
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver
 */
int InitNonCoherentSumsManager(TNGeneConfiguration * cfg, TNGeneData * rx);

/**
 * @brief Deinitializes the variables used for the management of the number of non coherent sums
 * @param nonCohManager points to the structure with variables used to manage the number of non coherent sums in acquisition
 */
void DeinitNonCoherentSumsManager(TNonCoherentSumsManager * nonCohManager);

/**
 * @brief Analyzes the condition of the performed acquisitions and chooses the number of the coherent sums for the next acquisitions.
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver
 * @param constellation is the type of satellite
 * @param prn is the index of the satellite
 * @param nonCohManager points to the structure with variables used to manage the number of non coherent sums in acquisition
 */
void PerformNonCoherentSumsManager(TNGeneConfiguration * cfg, TNGeneData * rx, TSatConstellation constellation, int prn, TNonCoherentSumsManager * nonCohManager);

#endif                          /* ACQUISITION_NON_COHERENT_SUMS_MANAGER_H_INCLUDED */
