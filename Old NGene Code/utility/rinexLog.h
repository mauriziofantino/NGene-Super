/**
 * @file rinexLog.h
 *
 * @author Mario Nicola
 *
 * @date 05/02/2018
 *
 * @version 1.0.0
 *
 * @brief Functions used to save a log in the RINEX format.
 */

#ifndef UTILITY_RINEX_H_INCLUDED
#define UTILITY_RINEX_H_INCLUDED

#include "ngeneDataStructure.h"
#include "ngeneConfiguration.h"
#include "rinexDataStructure.h"
#include "channelDataStructure.h"
#include "navigationMessageDataStructure.h"


#define CHAR_FOR_EXPONENTIAL	'D'

#define DEFAULT_PROGRAM_NAME "NGene2 RX           "
#define DEFAULT_STATION_NAME "name"
#define DEFAULT_AGENCY_NAME "AgencyName          "
#define DEFAULT_MARKER_TYPE "NON_GEODETIC        "
#define DEFAULT_RINEX_STRING "Unknown                                                     "



/**
 * @brief Read the configuration parameters for the RINEX logs
 *
 * @param cfg points to the receiver configuration
 * @param cfgFilename is the name of the configuration file
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int ManageRinexConfiguration(TNGeneConfiguration *cfg,char *cfgFilename);

/**
 * @brief Initializes all the structure used to save RINEX logs
 *
 * @param rinex points to the data structure used to save RINEX logs
 */
void InitRinexLog(TRinexLog *rinex);


/**
 * @brief Deinitializes all the structure used to save RINEX logs
 *
 * @param rinex points to the data structure used to save RINEX logs
 */
void DeinitRinexLog(TRinexLog *rinex);

/**
 * @brief Save in the RINEX format the navigation message for one GPS satellite
 *
 * @param cfg points to the receiver configuration
 * @param rinex points to the data structure used to save RINEX logs
 * @param ch points to channel that elaborate the current PRN
 * @param navMsg points to the navigation message to be saved
 */
void RinexSaveGpsNav(TNGeneConfiguration *cfg,TRinexLog *rinex,TChannel *ch,TNavigationMessage *navMsg);

/**
 * @brief Add the GPS iono parameter to a RINEX file (if not present)
 *
 * @param cfg points to the receiver configuration
 * @param rinex points to the data structure used to save RINEX logs
 * @param navMsg points to the navigation message to be saved
 */
void RinexAddGpsIono(TNGeneConfiguration *cfg,TRinexLog *rinex,TNavigationMessage *navMsg);

/**
 * @brief Add the GPS-UTC parameters to a RINEX file (if not present)
 *
 * @param cfg points to the receiver configuration
 * @param rinex points to the data structure used to save RINEX logs
 * @param navMsg points to the navigation message to be saved
 */
void RinexAddGput(TNGeneConfiguration *cfg,TRinexLog *rinex,TNavigationMessage *navMsg);


/**
 * @brief Save in the RINEX format the navigation message for one Galileo satellite
 *
 * @param cfg points to the receiver configuration
 * @param rinex points to the data structure used to save RINEX logs
 * @param ch points to channel that elaborate the current PRN
 * @param navMsg points to the navigation message to be saved
 */
void RinexSaveGalNav(TNGeneConfiguration *cfg,TRinexLog *rinex,TChannel *ch,TNavigationMessage *navMsg);

/**
 * @brief Add the Galileo iono parameter to a RINEX file (if not present)
 *
 * @param cfg points to the receiver configuration
 * @param rinex points to the data structure used to save RINEX logs
 * @param navMsg points to the navigation message to be saved
 */
void RinexAddGalIono(TNGeneConfiguration *cfg,TRinexLog *rinex,TNavigationMessage *navMsg);

/**
 * @brief Add the Galileo-UTC parameters to a RINEX file (if not present)
 *
 * @param cfg points to the receiver configuration
 * @param rinex points to the data structure used to save RINEX logs
 * @param navMsg points to the navigation message to be saved
 */
void RinexAddGaut(TNGeneConfiguration *cfg,TRinexLog *rinex,TNavigationMessage *navMsg);

/**
 * @brief Add the Galileo-GPS parameters to a RINEX file (if not present)
 *
 * @param cfg points to the receiver configuration
 * @param rinex points to the data structure used to save RINEX logs
 * @param navMsg points to the navigation message to be saved
 */
void RinexAddGpga(TNGeneConfiguration *cfg,TRinexLog *rinex,TNavigationMessage *navMsg);


/**
 * @brief Save in the RINEX format the observables used in the PVT computation
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 */
void RinexSaveObs(TNGeneConfiguration *cfg,TNGeneData *rx);

#endif /* UTILITY_RINEX_H_INCLUDED */
