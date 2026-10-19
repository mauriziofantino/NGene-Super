/**
 * @file manageAcqConfiguration.h
 *
 * @author Mario Nicola
 *
 * @date 07/08/2014
 *
 * @version 1.0.0
 *
 * @brief Functions that manage acquisition parameters.
 */

#ifndef MANAGE_ACQ_CONFIGURATION_H_INCLUDED
#define MANAGE_ACQ_CONFIGURATION_H_INCLUDED

#include "ngeneConfiguration.h"

/**
 * @brief Read the acquisition parameters from the configuration file
 * @param cfg points to the receiver configuration
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int ManageAcqConfiguration(TNGeneConfiguration * cfg);

/**
 * @brief Complete initializing the missing acquisition parameters in function of the selected FE and fs
 * @param cfg points to the receiver configuration
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int CompleteAcqConfiguration(TNGeneConfiguration * cfg);

#endif                          /* MANAGE_ACQ_CONFIGURATION_H_INCLUDED */
