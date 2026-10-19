/**
 * @file logSplitterDataStructure.h
 *
 * @author Mario Nicola
 *
 * @date 12/11/2020
 *
 * @version 1.0.0
 *
 * @brief Data structures used to manage the receiver logs and their splitting.
 */

#ifndef LOG_SPLITTER_DATA_STRUCTURE_H_INCLUDED
#define LOG_SPLITTER_DATA_STRUCTURE_H_INCLUDED

#include "ngeneConstants.h"
#include "spreadingCodes.h"

/**
 * @struct LogSplitterStruct
 *
 * @brief Contains the data used to split logs
 */
typedef struct LogSplitterStruct
{
	// from the configuration
	int splitPeriod;                   /**< The same as the configuration parameter, but in seconds
	                                        (useful to force a split period in minutes instead of hours for debug purposes) */
	// split time
	int splitTimeValid;               /**< TRUE_FLAG when the GNSS time for the next split is valid */
	TSatConstellation splitTimeConst; /**< indicates whether the time is GPS or Galileo */
	int splitWn;                      /**< rx time value for the next split, WN */
	int splitTow;                     /**< rx time value for the next split, TOW (s) */
} TLogSplitter;

#endif /* LOG_SPLITTER_DATA_STRUCTURE_H_INCLUDED */
