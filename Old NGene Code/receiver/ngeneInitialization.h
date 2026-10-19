/**
 * @file ngeneInitialization.h
 *
 * @author Mario Nicola
 *
 * @date 12/08/2014
 *
 * @version 1.0.0
 *
 * @brief Functions used to initialize/de-initialize the receiver.
 */
#ifndef NGENE_INITIALIZATION_H_INCLUDED
#define NGENE_INITIALIZATION_H_INCLUDED

#include "ngeneConfiguration.h"
#include "ngeneDataStructure.h"

/**
 * @brief Initializes the paths for the receiver execution
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
void PreparePaths(TNGeneConfiguration *cfg,TNGeneData *rx);

/**
 * @brief Performs some preliminary initializations of the receiver
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int PreliminaryInitialization(TNGeneConfiguration *cfg,TNGeneData *rx);

/**
 * @brief Initializes all the receiver structure (excluding configuration)
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int InitReceiver(TNGeneConfiguration *cfg,TNGeneData *rx);

/**
 * @brief Frees the memory allocated for the receiver data structure
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver
 */
void DeinitReceiver(TNGeneConfiguration *cfg,TNGeneData *rx);

#endif /* NGENE_INITIALIZATION_H_INCLUDED */
