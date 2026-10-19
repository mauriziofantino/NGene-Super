/**
 * @file spreadingCodes.h
 *
 * @author Mario Nicola
 *
 * @date 21/08/2014
 *
 * @version 1.0.0
 *
 * @brief Functions that manage acquisition data structures.
 */

#ifndef ACQUISITION_H_INCLUDED
#define ACQUISITION_H_INCLUDED

#include "ngeneDataStructure.h"
#include "ngeneConfiguration.h"

/**
 * @def ACQ_DEFAULT_FFT_SIZE
 * @brief Size of the FFT/IFFT used for coarse acquisition (default value)
 */
#define ACQ_DEFAULT_FFT_SIZE 4096

/**
 * @def ACQ_DEFAULT_LOG2_FFT_SIZE
 * @brief log2 of ACQ_FFT_SIZE 8default value)
 */
#define ACQ_DEFAULT_LOG2_FFT_SIZE 12

/**
 * @def	THRESHOLD_FAKE_DOPPLER
 * @brief Doppler shift used to estimate noise variance [Hz]
 */
#define THRESHOLD_FAKE_DOPPLER 30000.0

/**
 * @def	FAKE_DOPPLER_INDEX
 * @brief Index used to indicate Doppler shift used to estimate noise variance in the search space
 */
#define FAKE_DOPPLER_INDEX -1

/**
 * @def	THRESHOLD_COARSE_TO_CONFIRMATION_FACTOR
 * @brief Factor used to obtain the noise variance for confirmation
 *        from the noise variance for coarse acquisition
 */
#define THRESHOLD_COARSE_TO_CONFIRMATION_FACTOR	60.0

/**
 * @def	THRESHOLD_NUMBER_OF_ESTIMATIONS
 * @brief How many estimations are needed to have a reliable estimation
 *        of noise variance for confirmation
 */
#define THRESHOLD_NUMBER_OF_ESTIMATIONS 10000

/**
 * @brief Initializes acquisition variable common to all the channels
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int InitAcquisitionCommon(TNGeneConfiguration * cfg, TNGeneData * rx);

/**
 * @brief Deinitializes acquisition variable common to all the channels
 *
 * @param acqCommon points to the structure storing these variables
 */
void DeinitAcquisitionCommon(TAcqCommon * acqCommon);

/**
 * @brief Calculates the threshold for coarse acquisition of one constellation.
 * @details Called by the the coarse acquisition function, uses the results of the coarse acquisition (IFFT already performed!).
 * @param cfg points to the receiver configuration
 * @param ch points to the current channel
 * @param nonCoherentSums is the number of non coherent sums
 */
void UpdateCoarseThreshold(TNGeneConfiguration * cfg, TChannel * ch, int nonCoherentSums);

/**
 * @brief Performs preliminary operations in order to enable the calculus of the threshold for confirmation.
 * @details Called by main, it performs correlation over incoming samples in the buffer. Uses fixed code and carrier sequences. It perform a complete Galileo correlation (4 ms)
 * @param rx points to the receiver
 */
void UpdateVariablesForConfirmationThresholds(TNGeneData * rx);

#ifndef DISABLE_CONFIRMATION_THRESHOLD
/**
 * @brief Calculates the threshold for confirmation.
 * @details Called by main, it performs correlation over incoming samples in the buffer. Uses fixed code and carrier sequences. It perform a complete Galileo correlation (4 ms)
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver
 * @param ch points to the current channel
 */
void UpdateConfirmationThresholds(TNGeneConfiguration * cfg, TNGeneData * rx, TChannel * ch);
#else
/**
 * @brief Calculates the threshold for confirmation.
 * @details Called by main, it performs correlation over incoming samples in the buffer. Uses fixed code and carrier sequences. It perform a complete Galileo correlation (4 ms)
 * @param ch points to the current channel
 */
void UpdateConfirmationThresholds(TChannel * ch);
#endif

#endif                          /* ACQUISITION_H_INCLUDED */
