/**
 * @file carrierTable.h
 *
 * @author Mario Nicola
 *
 * @date 19/08/2014
 *
 * @version 1.0.0
 *
 * @brief Functions that manage the generation of a carrier at sampling frequency.
 *  	  Derives from the implementation by Nordio (2007)
 */

#ifndef CARRIER_TABLE_H_INCLUDED
#define CARRIER_TABLE_H_INCLUDED

#include "simd.h"
#include "ngeneConfiguration.h"

/**
 * @def LOG2_ROWS
 * @brief The carrier table is made of 2^8=256 rows
 */
#define LOG2_ROWS				8

/**
 * @def LOG2_SAMPLES_PER_ROW
 * @brief The carrier table is made of 2^5=32 columns
 */
#define LOG2_SAMPLES_PER_ROW	5

/**
 * @struct SinCosTablStruct
 * @brief Contains all the information needed to generate a carrier at sampling frequency
 */
typedef struct SinCosTablStruct
{
    double f; 						/**< Nominal carrier frequency (normalized w.r.t. the sampling frequency) */
    unsigned int tableSize;			/**< Size of the carrier table */
    unsigned int log2Rows;			/**< log2(number of rows) */
    unsigned int rows;				/**< number of rows */
    unsigned int log2SamplesPerRow;	/**< samples per row */
    unsigned int samplesPerRow;		/**< samples per row */
    s128 *table;					/**< needs to be aligned to 16 bytes boundary */
} TSinCosTable;

/**
 * @brief Initializes the table used to generate the carrier
 *
 * @param cfg points to the receiver configuration
 * @param t points to the carrier table
 * @param f is the nominal frequency [Hz]. Doppler shift zero!
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int InitCarrierTable(TNGeneConfiguration *cfg, TSinCosTable *t, double f);

/**
 * @brief Frees memory allocated for the carrier table
 *
 * @param t points to the carrier table
 */
void DeinitCarrierTable(TSinCosTable *t);

/**
 * @brief Generate samples of a carrier (both sine and cosine)
 *
 * @details Uses the original function by Nordio
 *
 * @param cfg points to the receiver configuration
 * @param t points to the carrier table
 * @param f is the current carrier frequency [Hz]. Doppler shift included!
 * @param phaseIn is the input phase [rad]. The value is in [0,2pi)
 * @param samples is the number of samples to be generated
 * @param sinOut points to the output sin array (output). Must be aligned (address multiple of 16)!
 * @param cosOut points to the output cos array (output). Must be aligned (address multiple of 16)!
 *
 * @return The update value of carrier phase [rad]. The value is in [0,2pi)
 */
double GenerateCarrier(TNGeneConfiguration *cfg, TSinCosTable *t, double f, double phaseIn, unsigned int samples, char *sinOut, char *cosOut);

/**
 * @brief Generate samples of a sine
 *
 * @details Uses the original function by Nordio
 *
 * @param cfg points to the receiver configuration
 * @param t points to the carrier table
 * @param f is the current carrier frequency [Hz]. Doppler shift included!
 * @param phaseIn is the input phase [rad]. The value is in [0,2pi)
 * @param samples is the number of samples to be generated
 * @param sinOut points to the output sin array (output). Must be aligned (address multiple of 16)!
 *
 * @return The update value of carrier phase [rad]. The value is in [0,2pi)
 */
double GenerateSine(TNGeneConfiguration *cfg, TSinCosTable *t, double f, double phaseIn, unsigned int samples, char *sinOut);

#endif /* CARRIER_TABLE_H_INCLUDED */
