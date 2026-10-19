/**
 * @file additionalLogs.h
 *
 * @author Mario Nicola
 *
 * @date 19/11/2014
 *
 * @version 1.0.0
 *
 * @brief Functions used to calculate and send to the GUI additional logs.
 *
 * @note From a previous release by Andrea Molino
 */

#ifndef ADDITIONAL_LOGS_H_INCLUDED
#define ADDITIONAL_LOGS_H_INCLUDED

#include "ngeneDataStructure.h"


/**
 * @def GUI_EPHEMERIDES_PACKET_SIZE
 * @brief Ephemerides are sent to the GUI through a packet made of GUI_EPHEMERIDES_PACKET_SIZE bytes
 */
#define GUI_EPHEMERIDES_PACKET_SIZE	200


/**
 * @brief EvaluateAndSendSamplesHistogram evaluates the histogram of the input samples
 *        and sends the results to the GUI
 *
 * @param rx points to the receiver base data structure
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int EvaluateAndSendSamplesHistogram(TNGeneData *rx);


/**
 * @brief EvaluateAndSendSamplesSpectrum evaluates the spectrum of the input samples
 *        and sends the results to the GUI
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int EvaluateAndSendSamplesSpectrum(TNGeneConfiguration *cfg,TNGeneData *rx);

/**
 * @brief SendNavigationMessageInformation sends the content of navigation message
 *        decoded by the receiver.
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 */
void SendNavigationMessageInformation(TNGeneConfiguration *cfg,TNGeneData *rx);

/**
 * @brief SendLogFiles sends the correlation file to the GUI.
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 *
 * @return SUCCESS_RETURN when the file is sent, FAILURE_RETURN otherwise
 */
// int SendLogFiles(TNGeneConfiguration *cfg,TNGeneData *rx);

#endif /* ADDITIONAL_LOGS_H_INCLUDED */
