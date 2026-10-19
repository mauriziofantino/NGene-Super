/**
 * @file  galileoTracking.h
 *
 * @author Mario Nicola
 *
 * @date 08/09/2014
 *
 * @version 1.0.0
 *
 * @brief Functions used to perform tracking for Galileo.
 */

#ifndef GALILEO_TRACKING_H_INCLUDED
#define GALILEO_TRACKING_H_INCLUDED

#include "ngeneConfiguration.h"
#include "ngeneDataStructure.h"
#include "channelDataStructure.h"
#include "trackingDataStructure.h"

/**
 * @brief Performs tracking for a Galileo PRN
 * @details Elaborates one millisecond of samples.
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 * @param ch points to the current channel
 * @param firstBunch is the address of the first sample to be elaborated
 * @param firstBunchSize is the number of samples starting from firstBunch
 * @param secondBunch is the address of the first sample in the second bunch of samples (if any)
 * @param secondBunchSize is the number of samples starting from secondBunch (if any)
 *
 * @return The current state of the tracking
 */
TTrackingStatus PerformGalileoTracking(TNGeneConfiguration *cfg,TNGeneData *rx,TChannel *ch,char *firstBunch, unsigned int firstBunchSize,char *secondBunch, unsigned int secondBunchSize);


#endif /* GALILEO_TRACKING_H_ INCLUDED*/
