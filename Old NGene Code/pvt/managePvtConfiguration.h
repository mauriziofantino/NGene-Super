/**
 * @file managePvtConfiguration.h
 *
 * @author Mario Nicola
 *
 * @date 07/08/2014
 *
 * @version 1.0.0
 *
 * @brief Functions that manage PVT parameters.
 */

#ifndef MANAGE_PVT_CONFIGURATION_H_INCLUDED
#define MANAGE_PVT_CONFIGURATION_H_INCLUDED

#include "ngeneConfiguration.h"

/**
 * @brief Read the PVT parameters from the configuration file
 *
 * @param cfg points to the receiver configuration
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int ManagePvtConfiguration(TNGeneConfiguration *cfg);



#endif /* MANAGE_PVT_CONFIGURATION_H_INCLUDED */
