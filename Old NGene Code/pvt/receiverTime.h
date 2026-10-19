/**
 * @file receiverTime.h
 *
 * @author Mario Nicola
 *
 * @date 27/10/2014
 *
 * @version 1.0.0
 *
 * @brief Functions used to manage the receiver time
 */

#ifndef RECEIVER_TIME_H_INCLUDED
#define RECEIVER_TIME_H_INCLUDED


/**
 * @brief ResetReferenceRxTime cleans up the receiver time estimations e.g., after a PVT error
 * @param rx points to the receiver base data structure
 */
void ResetReferenceRxTime(TNGeneData *rx);

/**
 * @brief SetReferenceRxTime evaluates the receiver time starting from the output of the PVT
 * @param rx points to the receiver base data structure
 * @param defaultLeapSeconds default value of leap seconds, read from a configuration file
 */
void SetReferenceRxTime(TNGeneData *rx,int defaultLeapSeconds);

/**
 * @brief PredictRxTime predicts the receiver time considering the result of the last valid PVT
 *        and the number of elaborated samples
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 */
void PredictRxTime(TNGeneConfiguration *cfg,TNGeneData *rx);

#endif /* RECEIVERTIME_H_INCLUDED */
