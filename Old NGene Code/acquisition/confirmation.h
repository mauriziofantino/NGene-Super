/**
 * @file confirmation.h
 *
 * @author Mario Nicola
 *
 * @date 03/09/2014
 *
 * @version 1.0.0
 *
 * @brief Functions performing M-on-N confirmation.
 */

#ifndef CONFIRMATION_H_INCLUDED
#define CONFIRMATION_H_INCLUDED

#include "channelDataStructure.h"
#include "ngeneConfiguration.h"
#include "ngeneDataStructure.h"

#ifndef DISABLE_CONFIRMATION_THRESHOLD
/**
 * @brief Initializes the confirmation data structure for each channel
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 * @param ch points to the current channel
 */
void InitConfirmation(TNGeneConfiguration * cfg, TNGeneData * rx, TChannel * ch);
#else
/**
 * @brief Initializes the confirmation data structure for each channel
 * @param cfg points to the receiver configuration
 * @param ch points to the current channel
 */
void InitConfirmation(TNGeneConfiguration * cfg, TChannel * ch);
#endif

#ifndef DISABLE_CONFIRMATION_THRESHOLD
/**
 * @brief Initializes the confirmation data structure for the next execution
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 * @param ch points to the current channel
 */
void StartConfirmation(TNGeneConfiguration * cfg, TNGeneData * rx, TChannel * ch);
#else
/**
 * @brief Initializes the confirmation data structure for the next execution
 *
 * @param ch points to the current channel
 */
void StartConfirmation(TChannel * ch);
#endif

/**
 * @brief Performs a M-on-N confirmation (the signal is present if the confirmation obtains at least M correlations over the threshold out of N trials correlations.
 * @details Used for all supported signals. Elaborates one millisecond of samples at each call.
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 * @param ch points to the current channel
 * @param firstBunch is the address of the first sample to be elaborated
 * @param firstBunchSize is the number of samples starting from firstBunch
 * @param secondBunch is the address of the first sample in the second bunch of samples (if any)
 * @param secondBunchSize is the number of samples starting from secondBunch (if any)
 * @return The current state of the refinement (ACQUISITION_IN_EXECUTION, ACQUISITION_TERMINATED)
 */
TAcquisitionStatus PerformConfirmation(TNGeneConfiguration * cfg, TNGeneData * rx, TChannel * ch, char *firstBunch, unsigned int firstBunchSize, char *secondBunch, unsigned int secondBunchSize);

#endif                          /* CONFIRMATION_H_INCLUDED */
