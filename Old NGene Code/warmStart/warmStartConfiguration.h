/**
 * @file warmStartConfiguration.h
 *
 * @author Mario Nicola
 *
 * @date 08/08/2014
 *
 * @version 1.0.0
 *
 * @brief This header defines the structure that stores NGene configuration for Warm start
 *        from both GUI and configuration files.
 */

#ifndef WARM_CONFIGURATION_H_INCLUDED
#define WARM_CONFIGURATION_H_INCLUDED


/**
 * @struct WarmStartConfigurationStruct
 *
 * @brief Contains the NGene receiver configuration for warm start.
 *        The almanac is saved in the yumaAlmanac.txt file in the dumpPath folder (see manageConfiguration.c)
 */
typedef struct WarmStartConfigurationStruct
{
	// read from file
	int enableWarmStart;  /**< if TRUE_FLAG, the warm start is enabled */
	char *almanacPathGps; /**< is the complete path of the yuma almanac for GPS */
	char *almanacPathGal; /**< is the complete path of the xml almanac for Galileo */
	double warmLatitude;  /**< latitude to be used for warm start */
	double warmLongitude; /**< longitude to be used for warm start */
	int warmYear;		  /**< year to be used for warm start */
	int warmMonth;		  /**< month to be used for warm start */
	int warmDay;		  /**< day to be used for warm start */
	int warmHour;		  /**< hour (from 0 to 23) to be used for warm start */
	int warmMinute;		  /**< minute to be used for warm start */
	int warmSecond;		  /**< second to be used for warm start */
	double minElevation;  /**< minimum elevation for a satellite to be deemed visible (rad) */
	int updateInterval;   /**< update interval between two visibility prediction (minutes) */
	// derived from the previous
	double warmEcef[3];   /**< receiver position to be used for warm start (ECEF) */
	int gpsWn;            /**< GPS Week NUmber complete */
	double gpsTow;        /**< GPS TOW (s) */
	int galWn;            /**< Galileo Week NUmber complete */
	double galTow;        /**< Galileo TOW (s) */
} TWarmStartConfiguration;


#endif /* WARM_CONFIGURATION_H_INCLUDED */
