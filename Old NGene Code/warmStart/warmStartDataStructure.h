/**
 * @file warmStartDataStructure.h
 *
 * @author Mario Nicola
 *
 * @date 13/04/2021
 *
 * @version 1.0.0
 *
 * @brief This header defines the structure that stores variables used to perform the Warm Start
 */

#ifndef WARM_START_DATA_STRUCTURE_H_INCLUDED
#define WARM_START_DATA_STRUCTURE_H_INCLUDED


/**
 * @struct WarmStartPredictionStruct
 *
 * @brief Contains the variable used to store the prediction performed for the warm start.
 */
typedef struct WarmStartPredictionStruct
{
	int valid;            /**< TRUE_FLAG if the information are valid, FALSE_FLAG otherwise */
	double ecefSatPos[3]; /**< x [m], y [m], z [m] in ECEF reference system */
	double ecefSatVel[3]; /**< vx [m/s], vy [m/s], vz [m/s] in ECEF reference system */
	double azimuth;       /**< Azimuth of the satellite with respect the last receiver position [radian] */
	double elevation;     /**< Elevation of the satellite with respect the last receiver position [radian] */
	int visible;          /**< TRUE_FLAG if the satellite is deemed visible, FALSE_FLAG otherwise */
	int allocated;        /**< TRUE_FLAG if the satellite is allocated to a receiver channel, FALSE_FLAG otherwise */
} TWarmStartPrediction;


/**
 * @struct WarmStartDataStruct
 *
 * @brief Contains the variable used to manage the Warm Start in NGene.
 */
typedef struct WarmStartDataStruct
{
	int predictionValid;                   /**< TRUE_FLAG if a prediction has been performed, FALSE_FLAG otherwise */
	int gpsNumVisible;                     /**< Number of GPS satellites deemed as visible in the last prediction */
	int gpsNumAvailable;                   /**< Number of visible GPS satellites not allocated to a channel */
	TWarmStartPrediction *gpsPrediction;   /**< For the prediction of visibility of GPS satellites */
	int galNumVisible;                     /**< Number of Galileo satellites deemed as visible in the last prediction */
	int galNumAvailable;                   /**< Number of visible Galileo satellites not allocated to a channel */
	TWarmStartPrediction *galPrediction;   /**< For the prediction of visibility of Galileo satellites */
	unsigned long long int updateInterval; /**< Interval between two update (number of samples) */
	unsigned long long int nextUpdate;     /**< Time of next update (number of samples) */
} TWarmStartData;

#endif /* WARM_START_DATA_STRUCTURE_H_INCLUDED */
