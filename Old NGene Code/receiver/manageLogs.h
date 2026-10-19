/**
 * @file manageLogs.h
 *
 * @author Mario Nicola
 *
 * @date 12/11/2020
 *
 * @version 1.0.0
 *
 * @brief Functions used to manage the receiver logs and their splitting.
 */

#ifndef MANAGE_LOGS_H_INCLUDED
#define MANAGE_LOGS_H_INCLUDED


#include "ngeneConfiguration.h"
#include "ngeneDataStructure.h"



/**
 * @brief InitLogSplitter initializes the variables used to split the receiver logs
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver
 */
void InitLogSplitter(TNGeneConfiguration *cfg,TNGeneData *rx);

/**
 * @brief ManageLogSplitter manages the split of the logs
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver
 */
void ManageLogSplitter(TNGeneConfiguration *cfg,TNGeneData *rx);



#endif /* MANAGE_LOGS_H_INCLUDED */
