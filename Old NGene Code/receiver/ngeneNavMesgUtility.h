/**
 * @file ngene2NavMesgUtility.h
 *
 * @author Mario Nicola
 *
 * @date 01/02/2021
 *
 * @version 1.0.0
 *
 * @brief Data structures and functions used by ngene2NavMesg.c
 */

#ifndef NGENE2_NAV_MESG_UTILITY_H_INCLUDED
#define NGENE2_NAV_MESG_UTILITY_H_INCLUDED

#include <stdio.h>
#include "ngeneConstants.h"
#include "ngeneConfiguration.h"
#include "spreadingCodes.h"
#include "ngeneDataStructure.h"



/**
 * @def DEFAULT_NCURSES_DELAY
 * @brief Default value for the NCurses execution delay [ns]
 */
#define DEFAULT_NCURSES_DELAY    1000000

/**
 * @def MAXIMUM_NCURSES_DELAY
 * @brief Maximum value for the NCurses execution delay [ns]
 */
#define MAXIMUM_NCURSES_DELAY    999000000



/**
 * @struct NGeneNavMesgParsStruct
 *
 * @brief Contains all the parameters for the NGene2NavMesg receiver
 */
typedef struct NGeneNavMesgParsStruct
{
	char galE1bNavMesgFilename[MAX_ROW_LENGTH];  /**< name of the file containing E1b I/NAV in IFEN format */
	char galE5aEnabled;                          /**< TRUE_FLAG if the F/NAV is elaborated, FALSE_FLAG elsewhere */
	char galE5aNavMesgFilename[MAX_ROW_LENGTH];  /**< name of the file containing E5a F/NAV in IFEN format */
	char gpsL1caEnabled;                         /**< TRUE_FLAG if GPS L1 C/A is elaborated, FALSE_FLAG elsewhere */
	char gpsL1caNavMesgFilename[MAX_ROW_LENGTH]; /**< name of the file containing L1 C/A navigation message in IFEN format */
	//
	int revertGpsParityInversion;    /**< If TRUE_FLAG the GPS bits are saved after the parity inversion and must be corrected;
	                                      otherwise, the GPS bits are used as they are read from the input file */
	int numberOfGpsRevertedBits;     /**< How many bits in the GPS word are affected by the parity inversion
	                                      (data+parity bits, data bits only, no bits) */
	//
	long long int startGalTime;      /**< Start time of elaboration. Complete GST in [ms].
	                                      From the begin of the input: -1 */
	long long int endGalTime;        /**< End time of elaboration. Complete GST in [ms].
	                                      Till the end of the input: -1 */
	struct timespec delayForNCurses; /**< delay added to the elaboration to improve the readability of NCurses output [ns]  */
} TNGeneNavMesgPars;


/**
 * @struct NGeneNavMesgDataStruct
 *
 * @brief Contains all the basic data for the NGene2NavMesg receiver
 */
typedef struct NGeneNavMesgDataStruct
{
	// identify the target navigation message
	TSatConstellation satType; /**< Constellation */
	TSatSignal satSignal;      /**< Signal */
	// current navigation message parameters
	int numPrns;               /**< How many PRNS are supported */
	int symbolsPerRow;         /**< Number of bits/symbols contained in a row of the input file */
	int symbolDuration;        /**< Duration of a bit/symbol [ms] */
	// input
	FILE *navMesgFile;         /**< input file in IFEN format */
	long long int startTime;   /**< time indicated in the IFEN format header (converted from s to ms) */
	// arrays with one element per PRN
	int *chIndex;              /**< index of the receiver channel to which current PRN is allocated, -1 if not allocated. */
	char **symbolsPtr;         /**< each element points to an array with the symbols read for the current PRN (symbols' buffer) */
	int *symbolsIndex;         /**< index of the next valid symbol, status of the symbols' buffer
	                                -1 if not used, symbolsPerRow if empty, 0 is full */
	long long int *t;          /**< current time [ms] */
	char *gpsInvertedPreamble; /**< to avoid phase shift in the GPS NavMesg file (seemingly they occur in the JRC file
	                                FALSE_FLAG, the first preamble found is right
	                                TRUE_FLAG, the first preamble found is inverted
	                                -1 is not valid */
} TNGeneNavMesgData;


/**
 * @brief Parse the NGene2NavMesg parameters
 *
 * @param cfg points to the NGene2NavMesg configuration
 * @param pars points to the NGene2NavMesg parameters structure
 *
 * @return SUCCESS_RETURN when the configuration is valid, FAILURE_RETURN elsewhere
 */
int ParseNGene2NavMesgParameters(TNGeneConfiguration *cfg,TNGeneNavMesgPars *pars);

/**
 * @brief Adapts the number of channel allocated in the receiver
 *        to the parameters specific to NGene2NavMesg
 *
 * @param cfg points to the NGene2NavMesg configuration
 * @param pars points to the NGene2NavMesg parameters structure
 *
 * @return SUCCESS_RETURN when the configuration is valid, FAILURE_RETURN elsewhere
 */
int AdaptTheNumberOReceiverChannels(TNGeneConfiguration *cfg,TNGeneNavMesgPars *pars);

/**
 * @brief Initializes the data structure for a NavMesg,
 *        then reads the header of the file in the IFEN format
 *
 * @param pars points to the NGene2NavMesg parameters structure
 * @param satType is the current constellation
 * @param satSignal is the current signal
 * @param data points to the NGene2NavMesg data for the current NavMesg
 *
 * @return SUCCESS_RETURN when the initialization is successful, FAILURE_RETURN elsewhere
 */
int InitNGene2NavMesgData(TNGeneNavMesgPars *pars,TSatConstellation satType,TSatSignal satSignal,TNGeneNavMesgData *data);

/**
 * @brief Deinitializes the NGene2NavMesg data structure.
 *        If needed, it frees allocated channels.
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 * @param data points to the NGene2NavMesg data for the current NavMesg
 */
void DeinitNGene2NavMesgData(TNGeneConfiguration *cfg,TNGeneData *rx,TNGeneNavMesgData *data);

/**
 * @brief Checks whenever symbols are available for the current NavMesg
 *
 * @param data points to the NGene2NavMesg data for the current NavMesg
 *
 * @return TRUE_FLAG if symbols are available, FALSE_FLAG elsewhere
 */
int AreSymbolsAvailable(TNGeneNavMesgData *data);

/**
 * @brief Fills the current NavMesg reading symbols from the IFEN file
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 * @param pars points to the NGene2NavMesg parameters structure
 * @param data points to the NGene2NavMesg data for the current NavMesg
 * @param lastTime is the time related to the last read entry
 *        (used to check the validity of the first time stamp read at each execution)
 *
 * @return Number of satellites for which new symbols are available
 */
int ReadTheNextNavMesgRows(TNGeneConfiguration *cfg,TNGeneData *rx,TNGeneNavMesgPars *pars,TNGeneNavMesgData *data,long long int lastTime);

/**
 * @brief Checks if a new symbol is available with the current time.
 *        If available, it is fed to the Navigation Message parser and discarded
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 * @param data points to the NGene2NavMesg data for the current NavMesg
 * @param t is the current GNSS time (ms from the start of the time)
 */
void ElaborateNextSymbol(TNGeneConfiguration *cfg,TNGeneData *rx,TNGeneNavMesgData *data,long long int t);


/**
 * @brief Skip input lines till the Elaboration start time
 *        configured by the user through the configuration file.
 *        If the delayed start is not configured, does nothing.
 *
 * @param pars points to the NGene2NavMesg parameters structure
 * @param data points to the NGene2NavMesg data for the current NavMesg
 *
 * @return SUCCESS_RETURN when the initialization is successful, FAILURE_RETURN elsewhere
 */
void FastForwardNavMesgData(TNGeneNavMesgPars *pars,TNGeneNavMesgData *data);

#endif /* NGENE2_NAV_MESG_UTILITY_H_INCLUDED */
