/**
 * @file manageNavMsgConfiguration.h
 *
 * @author Mario Nicola
 *
 * @author Mario Nicola
 *
 * @date 15/04/2019
 *
 * @version 1.0.0
 *
 * @brief Manage configuration parameters related to navigation message parsing.
 */
#ifndef MANAGE_NAV_MSG_CONFIGURATION_H_INCLUDED
#define MANAGE_NAV_MSG_CONFIGURATION_H_INCLUDED

#include "ngeneConfiguration.h"

/**
 * @brief Read the Navigation Message parameters from the configuration file
 *
 * @param cfg points to the receiver configuration
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int ManageNavigationMessageConfiguration(TNGeneConfiguration *cfg);

#endif  /* MANAGE_NAV_MSG_CONFIGURATION_H_INCLUDED */

