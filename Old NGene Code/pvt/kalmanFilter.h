/**
 * @file kalmanFilter.h
 *
 * @author Mario Nicola
 *
 * @date 3/11/2014
 *
 * @version 1.0.0
 *
 * @brief Kalman filter functions.
 */

#ifndef KALMAN_FILTER_H_INCLUDED
#define KALMAN_FILTER_H_INCLUDED

/**
 * @brief Initializes Q matrices for Kalman filter with default values
 *
 * @param q points to the Q matrix for the single constellation PVT
 * @param qMulti points to the Q matrix for the multiple contellations PVT
 * @param dt is the time interval between two consecutive PVTs
 */
void InitQStateNoiseCovarianceMatrices(double *q,double *qMulti, double dt);

/**
 * @brief Initializes the variables used for the PVT computation through the Kalman Filter.
 *        Allocates the matrices and the array.
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int InitKalmanFilterData(TNGeneConfiguration *cfg,TNGeneData *rx);

/**
 * @brief Frees the memory allocated to matrices and the array used for the Kalman Filter.
 *
 * @param rx points to the receiver base data structure
 */
void DeinitKalmanFilterData(TNGeneData *rx);

/**
 * @brief Performs the Kalman Filter. Called by PvtManager.
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int PerformKalmanFilter(TNGeneConfiguration *cfg,TNGeneData *rx);

#endif /* KALMAN_FILTER_H_INCLUDED */
