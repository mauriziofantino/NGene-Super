/**
 * @file tracking.h
 *
 * @author Mario Nicola
 *
 * @date 03/09/2014
 *
 * @version 1.0.0
 *
 * @brief Functions used to perform tracking.
 */

#ifndef TRACKING_H_INCLUDED
#define TRACKING_H_INCLUDED


/**
 * @def TRACKING_LOG_SET_SIZE
 * @brief Every integration period TRACKING_LOG_SET_SIZE data are saved in the tracking correlation log file
 * @details Each set of data includes:
 * 			- channel index
 * 			- satType (GPS_SAT, GALILEO_SAT or EGNOS_SAT)
 * 			- satId
 * 			- time from the start of the receiver [s]
 * 			- pI, pQ, eI, eQ, lI, lQ
 * 			- CNo [dBHz]
 * 			- Doppler shift [Hz]
 * 			- code rate shift w.r.t. nominal value [Hz]
 * 			- initial carrier frequency estimate at the tracking start [Hz]
 * 		    - carrier phase error [Hz]
 * 			Each value is saved as a float number.
 */
#define TRACKING_LOG_SET_SIZE	16


/**
 * @brief Initializes the tracking structure (during receiver initialization)
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 * @param ch points to the current channel
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int InitTracking(TNGeneConfiguration *cfg,TNGeneData *rx,TChannel *ch);

/**
 * @brief Frees the memory allocated in the current tracking structure
 *
 * @param trk points to the tracking structure
 */
void DeinitTracking(TTracking *trk);

/**
 * @brief Initializes tracking data structure during receiver elaboration
 *
 * @param cfg points to the receiver configuration
 * @param ch points to the current channel
 * @param trk points to the tracking structure
 */
void StartTracking(TNGeneConfiguration *cfg,TChannel *ch,TTracking *trk);


/**
 * @brief Performs all the despreading operations required by ttracking
 *
 * @param ch points to the current channel
 * @param trk points to the tracking structure
 * @param cosArray points to the samples demodulated using cosine
 * @param sinArray points to the samples demodulated using sine
 */
void PerformAllTrackingCorrelations(TChannel *ch, TTracking *trk, char *cosArray, char *sinArray);

/**
 * @brief this function writes tracking logs in the high rate correlation file
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 * @param ch points to the current channel
 * @param trk points to the tracking structure
 */
void DumpCorrelationsToFile(TNGeneConfiguration *cfg,TNGeneData *rx,TChannel *ch,TTracking *trk);

/**
 * @brief Clear computed correlations. Updates "old" variables.
 *
 * @param ch points to the current channel
 * @param trk points to the tracking structure
 */
void UpdateCorrelations(TChannel *ch,TTracking *trk);

/**
 * @brief Allocates the variables used to manage the status of the tracking
 *
 * @param cfg points to the receiver configuration
 * @param ch points to the current channel
 * @param trk points to the tracking structure
 */
void InitTrackingStatusChecker(TNGeneConfiguration *cfg,TChannel *ch,TTracking *trk);

/**
 * @brief Check if the Galileo signal is locked to a side peak after the Fast Re-acquisition and correct it
 *
 * @param cfg points to the receiver configuration
 * @param ch points to the current channel
 * @param trk points to the tracking structure
 */
void CheckAndCorrectSidePeakLock(TNGeneConfiguration *cfg,TChannel *ch,TTracking *trk);

/**
 * @brief Initializes the variables used to manage the status of the tracking
 *
 * @param cfg points to the receiver configuration
 * @param trk points to the tracking structure
 */
void StartTrackingStatusChecker(TNGeneConfiguration *cfg,TTracking *trk);

/**
 * @brief Deinitializes the variables used to manage the status of the tracking
 *
 * @param trk points to the tracking structure
 */
void DeinitTrackingStatusChecker(TTracking *trk);

/**
 * @brief Evaluates the state of the tracking
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 * @param ch points to the current channel
 * @param trk points to the tracking structure
 */
void CheckTrackingLock(TNGeneConfiguration *cfg,TNGeneData *rx,TChannel *ch,TTracking *trk);

#endif /* TRACKING_H_INCLUDED */
