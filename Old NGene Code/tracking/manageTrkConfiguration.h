/**
 * @file manageTrkConfiguration.h
 *
 * @author Mario Nicola
 *
 * @date 07/08/2014
 *
 * @version 1.0.0
 *
 * @brief Functions that manage tracking parameters.
 */

#ifndef MANAGE_TRK_CONFIGURATION_H_INCLUDED
#define MANAGE_TRK_CONFIGURATION_H_INCLUDED

#include "ngeneConfiguration.h"

/**
 * @brief Read the tracking parameters from the configuration file
 *
 * @param cfg points to the receiver configuration
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int ManageTrkConfiguration(TNGeneConfiguration *cfg);

#endif /* MANAGE_TRK_CONFIGURATION_H_INCLUDED */
