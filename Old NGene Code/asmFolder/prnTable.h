/**
 * @file prnTable.h
 *
 * @author Mario Nicola
 *
 * @date 18/08/2014
 *
 * @version 1.0.0
 *
 * @brief Functions that manage the sampling of the spreading codes.
 */

#ifndef PRN_TABLE_H_INCLUDED
#define PRN_TABLE_H_INCLUDED

#include "simd.h"
#include "ngeneConfiguration.h"
#include "spreadingCodes.h"

/**
 * @struct PrnTableStruct
 *
 * @brief Contains the data for the PRN sampling.
 */
typedef struct PrnTableStruct
{
	s128    *gpsChipsOversampled[GPS_NUMBER_OF_PRNS];				/**< PRN chips ( 0/-1) for each GPS satellite (each chip repeated 16 times) */
	s128    *egnosChipsOversampled[EGNOS_NUMBER_OF_PRNS];			/**< PRN chips ( 0/-1) for each EGNOS satellite (each chip repeated 16 times) */
	s128    *galileoChipsOversampled[GALILEO_NUMBER_OF_PRNS];		/**< PRN chips ( 0/-1) for each EGNOS satellite (each chip repeated 16 times) */
	s128    *gpsChipsOversampledBpsk[GPS_NUMBER_OF_PRNS];			/**< PRN chips (+1/-1) for each GPS satellite (each chip repeated 16 times) */
	s128    *egnosChipsOversampledBpsk[EGNOS_NUMBER_OF_PRNS];		/**< PRN chips (+1/-1) for each EGNOS satellite (each chip repeated 16 times) */
	s128    *galileoChipsOversampledBpsk[GALILEO_NUMBER_OF_PRNS];	/**< PRN chips (+1/-1) for each EGNOS satellite (each chip repeated 16 times) */
	/**/
	double scaling; 				/**< Use this factor to multiply NCO increment and initial value */
	unsigned int ncoThreshold;	/**< Maximum value for the NCO: when the value surpasses the limit, wraps to 0 */
	unsigned int chipShortLen;	/**< Minimum number of samples in a chip for the current sampling frequency */
	unsigned int chipLongLen;	/**< Maximum number of samples in a chip for the current sampling frequency */
	/**/
	unsigned char egnos136Valid;/**< If TRUE_FLAG then the PRN 136 has been read from configuration file,
									 otherwise PRN 136 is not supported */
	/**/
	int numberOfRealGalileoSat;	/**< Number of existing Galileo satellites as reported in Galileo_real_PRNs.txt */
	int *prnOfRealGalileoSat;	/**< Existing Galileo satellites as reported in Galileo_real_PRNs.txt */
} TPrnTable;


/**
 * @enum EmployedModulationEnum
 * @brief The representation used for the chips of the local code
 */
typedef enum EmployedModulationEnum
{
	BPSK_MODULATION,	/**< When chips are represented as +1/-1 */
	NGENE_MODULATION	/**< When chips are represented as  0/-1 */
} TEmployedModulation;


/**
 * @brief InitPrnTable initializes the table used to generate despreading sequences
 * @details Allocates memory to store codes, and read them from files
 *
 * @param cfg points to the receiver configuration
 * @param t points to the PRN table
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int InitPrnTable(TNGeneConfiguration *cfg,TPrnTable *t);

/**
 * @brief Frees the memory allocated for the PRN table
 *
 * @param t points to the PRN table
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
void DeinitPrnTable(TPrnTable *t);

/**
 * @brief Generate a sequence of samples at fs of the desired despreading code
 * @param Updates code phase
 *
 * @details Output array MUST BE LONGER than n, since the algorithm can write up to
 *          16 samples more than n
 *
 * @param cfg points to the receiver configuration
 * @param t points to the PRN table
 * @param prnConstellation is the desired signal (GPS, Galileo or EGNOS)
 * @param prn is the desired PRN index
 * @param codeRate is the current value of code rate [Hz]
 * @param codePhase points to the distance between the start of the code and the first sample to be generated [chips]
 * @param codePhaseFrac points to the fractional part of *codePhase
 * @param employedModulation states if chips are represented using +1/-1 or 0/-1
 * @param n is the number of samples that will be generated
 * @param codeOut points to the output array
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int GenerateCode(TNGeneConfiguration *cfg,TPrnTable *t,TSatConstellation prnConstellation,int prn,double codeRate,double *codePhase,double *codePhaseFrac,TEmployedModulation employedModulation,int n,char *codeOut);

/**
 * @brief Updates the code phase variable (after nSamples have been generated)
 *
 * @param cfg points to the receiver configuration
 * @param satType is the desired signal (GPS, Galileo or EGNOS)
 * @param codePhase points to the value of the code phase before sample generation [chip]
 * @param codePhaseFrac points to fractional part of *codePhase [chip]
 * @param nSamples is the number of generated samples
 * @param codeRate is the rate of the code used for generation [Hz]
 */
void UpdateCodePhase(TNGeneConfiguration *cfg,TSatConstellation satType, double *codePhase, double *codePhaseFrac, unsigned int nSamples, double codeRate);

/**
 * @brief Add two code phases
 *
 * @param satType is the desired signal (GPS, Galileo or EGNOS)
 * @param codePhase1 is the first addend [chip]
 * @param codePhase2 is the second addend [chip]
 * @param codePhase points to the value of the sum [chip]
 * @param codePhaseFrac points to fractional part of *codePhase [chip]
 */
void AddCodePhases(TSatConstellation satType, double codePhase1, double codePhase2, double *codePhase, double *codePhaseFrac);

#endif /* PRN_TABLE_H_INCLUDED */
