/**
 * @file manageWarmStartConfiguration.h
 *
 * @author Mario Nicola
 *
 * @date 08/08/2014
 *
 * @version 1.0.0
 *
 * @brief Functions that manage Warm Start configuration.
 */

#ifndef MANAGE_WARM_CONFIGURATION_H_INCLUDED
#define MANAGE_WARM_CONFIGURATION_H_INCLUDED

#include "ngeneConfiguration.h"

/**
 * @brief Read the warm start parameters from the configuration file
 *
 * @param cfg points to the receiver configuration
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int ManageWarmStartConfiguration(TNGeneConfiguration *cfg);

/**
 * @brief Frees memory allocated for the warm start configuration structure.
 *
 * @param cfg points to the receiver configuration
 */
void DeleteWarmStartConfiguration(TNGeneConfiguration *cfg);

#endif /* MANAGE_WARM_CONFIGURATION_H_INCLUDED */
