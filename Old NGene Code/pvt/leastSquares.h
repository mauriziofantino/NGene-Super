/**
 * @file leastSquares.h
 *
 * @author Mario Nicola
 *
 * @date 24/10/2014
 *
 * @version 1.0.0
 *
 * @brief Least squares functions.
 */

#ifndef LEAST_SQUARES_H_INCLUDED
#define LEAST_SQUARES_H_INCLUDED

/**
 * @brief Initializes the variables used for the PVT computation through the Least Squares method.
 *        Allocates the matrices and the array.
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int InitLeastSquaresData(TNGeneConfiguration *cfg,TNGeneData *rx);

/**
 * @brief Frees the memory allocated to matrices and the array used for the Least Squares method.
 *
 * @param rx points to the receiver base data structure
 */
void DeinitLeastSquaresData(TNGeneData *rx);

/**
 * @brief Performs the Least Squares method. Called by PvtManager.
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int PerformLeastSquares(TNGeneConfiguration *cfg,TNGeneData *rx);

#endif /* LEAST_SQUARES_H_INCLUDED */
