/**
 * @file pvtChecks.h
 *
 * @author Mario Nicola
 *
 * @date 22/10/2014
 *
 * @version 1.0.0
 *
 * @brief Functions used to perform checks during PVT computation.
 */

#ifndef PVT_CHECKS_H_INCLUDED
#define PVT_CHECKS_H_INCLUDED


/**
 * @def PVT_MAXIMUM_TIME_ERROR
 * @brief Maximum PVT correction to the estimated receiver time [s]
 */
#define PVT_MAXIMUM_TIME_ERROR    0.01

/**
 * @def PVT_MAXIMUM_POSITION_JUMP
 * @brief Maximum difference between two consecutive positions [m]
 *        The default value (100 m) leads to a speed equal to 360 km/h.
 */
#define PVT_MAXIMUM_POSITION_JUMP   100

/**
 * @def PVT_ENABLE_HARD_RESTART
 * @brief If PVT_ENABLE_HARD_RESTART is defined, channels and navigation messages
 *        are reset after a sequence of PVT computation failures
 */
#define PVT_ENABLE_HARD_RESTART

#ifdef PVT_ENABLE_HARD_RESTART
	/**
	 * @def PVT_MAXIMUM_CONSECUTIVE_FAILURES
	 * @brief If PVT_MAXIMUM_CONSECUTIVE_FAILURES consecutive PVT computations fail,
	 *        then a restart of all the channels and related information is performed.
	 */
	#define PVT_MAXIMUM_CONSECUTIVE_FAILURES  10
#endif




/**
 * Checks if the Time Of Week from the satellite on the channel ch has been received
 *
 * @param ch is the channel structure
 *
 * @return TRUE_FLAG if currently TOW and bit counter are valid, FALSE_FLAG otherwise
 */
int CheckTow(TChannel* ch);

/**
 * Checks if all the ephemerides data from the satellite on the channel ch have been received
 *
 * @param ch is the channel structure
 *
 * @return TRUE_FLAG if ephemerides have been received, FALSE_FLAG, otherwise
 */
int CheckEphemerides(TChannel* ch);

/**
 * Checks if a channel can be used for PVT computation
 *
 * @param ch is the channel structure
 *
 * @return TRUE_FLAG if the channel can be used, FALSE_FLAG otherwise
 */
int ReadyForPvt(TChannel *ch);

/**
 * Checks the goodness of the obtained PVT results
 *
 * @param cfg points to the receiver configuration
 * @param pvtData variables used for PVT computation
 * @param pvtResNew results obtained by the PVT computation
 * @param pvtResOld results obtained by the previous PVT computation
 * @param failure points to a flag that indicates whenever the PVT is not good due to a large error
 *        Eg, it if FALSE_FLAG if the GDOP limit is exceeded.
 *
 * @return TRUE_FLAG if the PVT result is seemingly good, FALSE_FLAG otherwise
 */
int CheckPvtResultsGoodness(TNGeneConfiguration *cfg,TPvtData *pvtData,TPvtResults *pvtResNew,TPvtResults *pvtResOld,int *failure);

#endif /* PVT_CHECKS_H_INCLUDED */
