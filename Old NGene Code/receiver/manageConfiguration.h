/**
 * @file manageConfiguration.h
 *
 * @author Mario Nicola
 *
 * @date 06/08/2014
 *
 * @version 1.0.0
 *
 * @brief Functions used to to read receiver configuration.
 */

#ifndef MANAGE_CONFIGURATION_H_INCLUDED
#define MANAGE_CONFIGURATION_H_INCLUDED

#include "ngeneConfiguration.h"
#include "ngeneDataStructure.h"

/**
 * @def DISABLE_CONFIGURATION_CHECK
 * @brief This symbol disables the check of the configuration files, then the
 *        configuration folder is always valid.
 *        This symbol should be defined only for debug purposes.
 */
//#define DISABLE_CONFIGURATION_CHECK

/**
 * @brief Copies the configuration folder from the installation directory to the current home folder.
 *
 * @param cfg points to the receiver configuration
 */
void CreateConfigurationFiles(TNGeneConfiguration *cfg);


#ifndef DISABLE_CONFIGURATION_CHECK
/**
 * @brief Checks whether the configuration folder is updated or not.
 *
 * @param cfg points to the receiver configuration
 *
 * @return TRUE_FLAG when the configuration folder is valid, FALSE_FLAG when it must be updated
 */
int ConfigurationFilesValid(TNGeneConfiguration *cfg);
#endif

/**
 * @brief Parses configuration files in order to initialize receiver parameters.
 *
 * @param cfg points to the receiver configuration
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int ParseConfigurationFiles(TNGeneConfiguration *cfg);


/**
 * @brief If the receiver is executed with verbose option,
 *        this function plots the current configuration!
 *
 * @param cfg points to the receiver configuration
 */
void PlotConfiguration(TNGeneConfiguration *cfg);


/**
 * @brief Complete the configuration initializing variables
 *        whose values depend on the parameters chosen by the user,
 *        but are not chosen directly by the user.
 *
 * @param cfg points to the receiver configuration
 */
void CompleteConfiguration(TNGeneConfiguration *cfg);


/**
 * @brief Frees memory allocated for configuration structure.
 *
 * @param cfg points to the receiver configuration
 */
void DeleteConfiguration(TNGeneConfiguration *cfg);

/**
 * @brief Frees memory allocated for parameters from the GUI.
 *
 * @param cfg points to the receiver configuration
 */
void DeleteGuiConfiguration(TNGeneConfiguration *cfg);

/**
 * @brief Read the configuration parameters for the NGene2 logs
 *
 * @param cfg points to the receiver configuration
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int ManageLogsConfiguration(TNGeneConfiguration *cfg);

#endif /* MANAGE_CONFIGURATION_H_INCLUDED */
