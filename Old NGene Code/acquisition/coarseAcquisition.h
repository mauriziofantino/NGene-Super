/**
 * @file coarseAcquisition.h
 *
 * @author Mario Nicola
 *
 * @date 24/08/2014
 *
 * @version 1.0.0
 *
 * @brief Functions performing coarse acquisition.
 */

#ifndef COARSE_ACQUISITION_H_INCLUDED
#define COARSE_ACQUISITION_H_INCLUDED

/**
 * @brief Initializes the Coarse acquisition data structure in the channel when the receiver starts elaboration
 * @param cfg points to the receiver configuration
 * @param acqCommon is the structure storing the coarse acquisition variables valid for all the channels
 * @param ch points to the current channel
 */
void InitCoarseAcquisition(TNGeneConfiguration * cfg, TAcqCommon * acqCommon, TChannel * ch);

/**
 * @brief De-initializes the Coarse acquisition data structure in the channel
 * @param acq is the structure storing the coarse acquisition variables
 */
void DeinitCoarseAcquisition(TCoarseAcquisition * acq);

/**
 * @brief Initializes the Coarse acquisition data structure in the channel when the coarse acquisition for a new PRN is started
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 * @param acqCommon is the structure storing the coarse acquisition variables valid for all the channels
 * @param ch points to the current channel
 */
void StartCoarseAcquisition(TNGeneConfiguration * cfg, TNGeneData * rx, TAcqCommon * acqCommon, TChannel * ch);

/**
 * @brief Performs Coarse acquisition
 * @details Elaborates one millisecond of samples. Tests one combination of code offset and Doppler shift Maximum of the evaluated CAF is evaluated and compared with a threshold.
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 * @param ch points to the current channel
 * @param firstBunch is the address of the first sample to be elaborated
 * @param firstBunchSize is the number of samples starting from firstBunch
 * @param secondBunch is the address of the first sample in the second bunch of samples (if any)
 * @param secondBunchSize is the number of samples starting from secondBunch (if any)
 * @return The current state of the coarse acquisition
 */
TAcquisitionStatus PerformCoarseAcquisition(TNGeneConfiguration * cfg, TNGeneData * rx, TChannel * ch, char *firstBunch, unsigned int firstBunchSize, char *secondBunch, unsigned int secondBunchSize);

#endif                          /* COARSE_ACQUISITION_H_INCLUDED */
