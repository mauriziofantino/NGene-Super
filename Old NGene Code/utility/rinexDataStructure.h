/**
 * @file rinexDataStructure.h
 *
 * @author Mario Nicola
 *
 * @date 05/02/2018
 *
 * @version 1.0.0
 *
 * @brief Functions used to save a log in the RINEX format.
 */

#ifndef UTILITY_RINEXDATASTRUCTURE_H_INCLUDED
#define UTILITY_RINEXDATASTRUCTURE_H_INCLUDED

#include "ngeneConstants.h"

/**
 * @struct RinexLogStruct
 * @brief Stores all the variables used for the RINEX files used for the RINEX file creation
 */
typedef struct RinexLogStruct
{
	/* Navigation message */
	int currDayNav;      /**< Day number for the RINEX file currently open (nav) */
	char filenameNav[2*MAX_ROW_LENGTH];/**< Complete name of the RINEX file (nav) */
	FILE *navFile;       /**< Pointer to the RINEX file (nav) */
	int ionoSaved;       /**< if TRUE_FLAG, then iono parameters have been already saved */
	fpos_t ionoPosition; /**< this points to the position in the file where the iono parameters will be saved */
	int gautSaved;	     /**< if TRUE_FLAG, then the Galileo-UTC parameters have been already saved */
	fpos_t gautPosition; /**< this points to the position in the file where the Galileo-UTC parameters will be saved */
	int gputSaved;	     /**< if TRUE_FLAG, then GPS-UTC parameters have been already saved */
	fpos_t gputPosition; /**< this points to the position in the file where the GPS-UTC parameters will be saved */
	int gpgaSaved;	     /**< if TRUE_FLAG, then GPS-Galileo parameters have been already saved */
	fpos_t gpgaPosition; /**< this points to the position in the file where the GPS-Galileo parameters will be saved */
	/* Observation data */
	int currDayObs;      /**< Day number for the RINEX file currently open (obs) */
	char filenameObs[2*MAX_ROW_LENGTH];/**< Complete name of the RINEX file (obs) */
	FILE *obsFile;       /**< Pointer to the RINEX file (obs) */
} TRinexLog;

#endif /* UTILITY_RINEXDATASTRUCTURE_H_INCLUDED */
