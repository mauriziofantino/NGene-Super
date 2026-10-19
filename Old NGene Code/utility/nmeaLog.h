/**
 * @file nmeaLog.h
 *
 * @author Mario Nicola
 *
 * @date 21/08/2020
 *
 * @version 1.0.0
 *
 * @brief Functions used to save a log in the NMEA format.
 *        See https://gpsd.gitlab.io/gpsd/NMEA.html for reference.
 */

#ifndef UTILITY_NMEALOG_H_INCLUDED
#define UTILITY_NMEALOG_H_INCLUDED

#include "ngeneDataStructure.h"
#include "ngeneConfiguration.h"



#define NMEA_DEFAULT_FILENAME    "auto"
#define NMEA_DEFAULT_TALKER_ID	 "GP"
#define NMEA_SAT_PER_GSV_MSG      3


/**
 * @brief Read the configuration parameters for the NMEA log
 *
 * @param cfg points to the receiver configuration
 * @param cfgFilename is the name of the configuration file
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int ManageNmeaConfiguration(TNGeneConfiguration *cfg,char *cfgFilename);


/**
 * @brief Save in the NMEA format the results of the PVT computation
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 */
void NmeaSavePvt(TNGeneConfiguration *cfg,TNGeneData *rx);


#endif /* UTILITY_NMEALOG_H_INCLUDED */
