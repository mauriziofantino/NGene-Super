/**
 * @file manageUsbFrontEndConfiguration.h
 *
 * @author Mario Nicola
 *
 * @date 08/08/2014
 *
 * @version 1.0.0
 *
 * @brief Functions that manage USB FE parameters.
 */

#ifndef MANAGE_USB_FRONT_END_CONFIGURATION_H_INCLUDED
#define MANAGE_USB_FRONT_END_CONFIGURATION_H_INCLUDED

#include "ngeneConfiguration.h"

/**
 * @brief Read the USB FE parameters from the configuration file
 *
 * @param cfg points to the receiver configuration
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int ManageUsbFeConfiguration(TNGeneConfiguration * cfg);

/**
 * @brief Frees memory allocated for the USB FE configuration structure.
 *
 * @param cfg points to the receiver configuration
 */
void DeleteUsbFeConfiguration(TNGeneConfiguration * cfg);



#endif				/* MANAGE_USB_FRONT_END_CONFIGURATION_H_INCLUDED */
