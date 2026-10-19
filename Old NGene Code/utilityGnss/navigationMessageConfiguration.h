/**
 * @file navigationMessageConfiguration.h
 *
 * @author Mario Nicola
 *
 * @date 15/04/2019
 *
 * @version 1.0.0
 *
 * @brief Manage configuration parameters related to navigation message parsing.
 */

#ifndef NAVIGATION_MESSAGE_CONFIGURATION_H_INCLUDED
#define NAVIGATION_MESSAGE_CONFIGURATION_H_INCLUDED


/**
 * @def GPS_WEEK_NUMBER_CORRECTION
 * @brief This number must be added to week number in order to obtain the complete
 *    	  number of weeks since the start of GPS time.
 *    	  Used when the value from the configuration file is not valid (or not found).
 */
#define GPS_WEEK_NUMBER_CORRECTION		2048


/**
 * @struct NavMsgConfigurationStruct
 *
 * @brief Contains configuration parameters related to the navigation parsing of NGene2.
 */
typedef struct NavMsgConfigurationStruct
{
	/* GPS/Galileo time */
	int gpsWnCorrection; /**< to be added to WN read from the GPS navigation message */
	int galileoWnCorrection; /**< to be added to WN read from the Galileo navigation message */

	/* Leap Seconds */
	int defaultLeapSeconds; /**< Default Number of Leap Seconds to get UTC from GNSS time
	                             when the parameters in the navigation message are not available */
} TNavMsgConfiguration;


#endif /* NAVIGATION_MESSAGE_CONFIGURATION_H_INCLUDED */
