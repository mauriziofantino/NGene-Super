/**
 * @file refinement.h
 *
 * @author Mario Nicola
 *
 * @date 01/09/2014
 *
 * @version 1.0.0
 *
 * @brief Functions performing code delay refinement and Doppler shift refinement.
 */

#ifndef REFINEMENT_H_INCLUDED
#define REFINEMENT_H_INCLUDED

#include "acquisitionDataStructure.h"
#include "channelDataStructure.h"
#include "ngeneConfiguration.h"
#include "ngeneDataStructure.h"

/**
 * @brief Initializes the refinement data structure for each channel
 * @param cfg points to the receiver configuration
 * @param ref points to the refinement data structure
 * @param ch points to the current channel
 */
void InitRefinement(TNGeneConfiguration * cfg, TRefinement * ref, TChannel * ch);

/**
 * @brief Initializes the refinement data structure for the Doppler refinement execution
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 * @param ch points to the current channel
 */
void StartDopplerRefinement(TNGeneConfiguration * cfg, TNGeneData * rx, TChannel * ch);

/**
 * @brief Refines the current Doppler shift estimation
 * @details Used for all supported signals. Elaborates one millisecond of samples at each call.
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 * @param ch points to the current channel
 * @param firstBunch is the address of the first sample to be elaborated
 * @param firstBunchSize is the number of samples starting from firstBunch
 * @param secondBunch is the address of the first sample in the second bunch of samples (if any)
 * @param secondBunchSize is the number of samples starting from secondBunch (if any)
 *
 * @return The current state of the refinement (ACQUISITION_IN_EXECUTION, ACQUISITION_TERMINATED)
 */
TAcquisitionStatus PerformDopplerRefinement(TNGeneConfiguration * cfg, TNGeneData * rx, TChannel * ch, char *firstBunch, unsigned int firstBunchSize, char *secondBunch, unsigned int secondBunchSize);

/**
 * @brief Initializes the refinement data structure for the code delay refinement execution
 * @details It takes into account the initial displacement in the input buffer introduced in order to test the negative code delay displacement. It manages conditions when blocksize is less than the number of samples to be rewinded.
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 * @param ch points to the current channel
 */
void StartCodeRefinement(TNGeneConfiguration * cfg, TNGeneData * rx, TChannel * ch);

/**
 * @brief Refines the current code delay estimation
 * @details Used for all supported signals. Elaborates one millisecond of samples at each call.
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 * @param ch points to the current channel
 * @param firstBunch is the address of the first sample to be elaborated
 * @param firstBunchSize is the number of samples starting from firstBunch
 * @param secondBunch is the address of the first sample in the second bunch of samples (if any)
 * @param secondBunchSize is the number of samples starting from secondBunch (if any)
 * @return The current state of the refinement (ACQUISITION_IN_EXECUTION, ACQUISITION_SKIPPED, ACQUISITION_TERMINATED)
 */
TAcquisitionStatus PerformCodeRefinement(TNGeneConfiguration * cfg, TNGeneData * rx, TChannel * ch, char *firstBunch, unsigned int firstBunchSize, char *secondBunch, unsigned int secondBunchSize);

#endif                          /* REFINEMENT_H_INCLUDED */
