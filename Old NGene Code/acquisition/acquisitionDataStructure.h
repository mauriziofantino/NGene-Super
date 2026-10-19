/**
 * @file acquisitionDataStructure.h
 *
 * @author Mario Nicola
 *
 * @date 21/08/2014
 *
 * @version 1.0.0
 *
 * @brief Structures used to store variables used for acquisition.
 */

#ifndef ACQUISITION_DATA_STRUCTURE_H_INCLUDED
#define ACQUISITION_DATA_STRUCTURE_H_INCLUDED

#include <stdio.h>
#include "spreadingCodes.h"
#include "mathFunctions.h"
#include "prnListDataStructure.h"

/**
 * @struct ThresholdManagerStruct
 * @brief Stores the thresholds and the variables used to manage and update them
 */
typedef struct ThresholdManagerStruct {
    /* variables used to calculate and updates coarse thresholds */
    double coarseVarianceGps;     /**< Last estimation of variance for GPS coarse */
    double coarseVarianceGal;     /**< Last estimation of variance for Galileo coarse */
    /* variables used to calculate and updates confirmation thresholds */
    unsigned int blocksize;     /**< How many samples are considered */
    char *code;     /**< Code used for Confirmation variance estimation */
    char *carrierCos;     /**< Carrier used for Confirmation variance estimation */
    char *carrierSin;     /**< Carrier used for Confirmation variance estimation */
    char *demodulatedCos;     /**< Input samples after demodulation with cosine */
    char *demodulatedSin;     /**< Input samples after demodulation with sin */
    TAverageWindow averageCorrGps;      /**< Mean value of the GPS correlation */
    TAverageWindow averageCorr2Gps;     /**< Mean value of the square of the GPS correlation */
    TAverageWindow averageCorrGal;      /**< Mean value of the Galileo correlation */
    TAverageWindow averageCorr2Gal;     /**< Mean value of the square of the Galileo correlation */
    unsigned int numberOfEstimationsForConfirmation;     /**< How many samples have been used to estimates variance */
} TThresholdManager;

/**
 * @struct NonCoherentSumsManagerStruct
 * @brief Contains variables used to manage the runtime change of the number of non coherent sums. Initial value is 1. The maximum number depends on the maximum number of non coherent sums configured by the user (through GUI or configuration file)
 */
typedef struct NonCoherentSumsManagerStruct {
    TPrnList testedPrns;     /**< List of PRN the receiver tried to acquire using the current number of non-coherent sums. The tag AVAILABLE means that the PRN has not been tested yet */
    int listLength;     /**< Number of elements in acqAttempts */
    int currentNumberOfNonCoherentSums;     /**< Current number of non coherent sums */
    int maximumNumberOfNonCoherentSums;     /**< maximum number of non coherent sums as required by the user */
} TNonCoherentSumsManager;

/*
 * AcqCommonStruct contains arrays used for coarse acquisition.
 *
 * Coarse acquisition performs FFT and IFFT over arrays of complex samples.
 * An array of complex samples is represented using this convention Re[0] Im[0] Re[1] Im[1] ... Re[n] Im[n]
 * where Re[i] and Im[i] are real and imaginary part of the i-th sample
 * (i.e. real and imaginary part are interleaved in the same array: if n
 * is the number of complex samples, then the array is 2n elements long).
 * This stands for signalCompressed, signalFft and ifftOut arrays.
 *
 * Conjugate FFT of locals codes uses a different notation to ease
 * the performing of the complex multiplication. The algorithm employed for
 * the complex multiplication requires that one of the two arrays is
 * stored in the following format: Re[0] -Im[0] Im[0] Re[0] Re[1] -Im[1] Im[1] Re[1] ... Re[n] -Im[n] Im[n] Re[n]
 * where Re[i] and Im[i] are real and imaginary part of the i-th sample
 * (i.e. real and imaginary part are interleaved and repeated in the same array: if n
 * is the number of complex samples, then the array is 4n elements long).
 * Since the conjugate of the codes are used for acquisition algorithm,
 * in the arrays the format used in InitLocalCodesFft will be Re[0] Im[0] -Im[0] Re[0] Re[1] Im[1] -Im[1] Re[1] ... Re[n] Im[n] -Im[n] Re[n]
 * (note the sign change in the imaginary part!).
 * This stands for gpsCodesFft, egnosCodesFft and galileoCodesFft arrays.
 */

/**
 * @struct AcqCommonStruct
 * @brief Stores the acquisition variables common to all the channels.
 */
typedef struct AcqCommonStruct {
    /* FFT of local codes */
    short int *gpsCodesFft[GPS_NUMBER_OF_PRNS];     /**< conj FFT of GPS codes */
    short int *egnosCodesFft[EGNOS_NUMBER_OF_PRNS];     /**< conj FFT of EGNOS codes */
    short int *galileoCodesFft[GALILEO_NUMBER_OF_PRNS];     /**< conj FFT of Galileo codes */
    /* Arrays */
    char *demodulatedCos;     /**< Array used to store the input samples demodulated using cosine */
    char *demodulatedSin;     /**< Array used to store the input samples demodulated using sin */
    /* Non coherent sums */
    TNonCoherentSumsManager nonCohManager;     /**< Structure used to manage run time change of the number of non-coherent sums */
    /* Thresholds */
    TThresholdManager thr;     /**< Includes all the thresholds and variables used to update them */
    /* Coarse acquisition */
    int gpsNumDopplers;     /**< Number of Doppler shifts to be explored by GPS coarse acquisition */
    float *gpsDopplers;     /**< Doppler shifts to be explored by GPS coarse acquisition [Hz] */
    int gpsNumDelays;     /**< Number of code delays to be explored by GPS coarse acquisition */
    int *gpsDelays;     /**< Code delays to be explored by GPS coarse acquisition [samples] */
    int egnosNumDopplers;     /**< Number of Doppler shifts to be explored by EGNOS coarse acquisition */
    float *egnosDopplers;     /**< Doppler shifts to be explored by EGNOS coarse acquisition [Hz] */
    int egnosNumDelays;     /**< Number of code delays to be explored by RGNOS coarse acquisition */
    int *egnosDelays;     /**< Code delays to be explored by EGNOS coarse acquisition [samples] */
    int galileoNumDopplers;     /**< Number of Doppler shifts to be explored by Galileo coarse acquisition */
    float *galileoDopplers;     /**< Doppler shifts to be explored by Galileo coarse acquisition [Hz] */
    int galileoNumDelays;     /**< Number of code delays to be explored by Galileo coarse acquisition */
    int *galileoDelays;     /**< Code delays to be explored by Galileo coarse acquisition [samples] */
    /* Refinement */
    int dopplerRefNumSteps;     /**< Number of Doppler corrections to be explored during the Doppler refinement */
    float *dopplerRefSteps;     /**< Doppler corrections [Hz] w.r.t. the output of coarse acquisition to be explored during the Doppler refinement */
    int codeRefDelta;     /**< Maximum displacement used for code delay refinement */
    int codeRefNumSteps;     /**< Number of code delay corrections to be explored during the code delay refinement */
    int *codeRefSteps;     /**< Code delay corrections w.r.t. the output of coarse acquisition to be explored during the code delay refinement */
} TAcqCommon;

/**
 * @enum AcquisitionStatusEnum
 * @brief Used to represent the current status of the acquisition stage
 */
typedef enum AcquisitionStatusEnum {
    ACQUISITION_IN_EXECUTION,     /**< Acquisition is in execution */
    ACQUISITION_SKIPPED,     /**< Acquisition has been skipped in the current execution cycle */
    ACQUISITION_SUCCESS,     /**< Acquisition ended with a successful comparison with a threshold */
    ACQUISITION_FAILURE,     /**< Acquisition ended with a failed comparison with a threshold */
    ACQUISITION_TERMINATED     /**< Acquisition ended without a comparison with a threshold (refinement) */
} TAcquisitionStatus;

/**
 * @struct CoarseAcquisitionStruct
 * @brief Stores the coarse acquisition variables specific to the single channel.
 */
typedef struct CoarseAcquisitionStruct {
    /* search parameters (same values stored in TAcqCommon, but taking into account current constellation) */
    int numDopplers;     /**< Number of Doppler shifts to be explored */
    float *dopplers;     /**< Doppler shifts to be explored (points to the AcqCommon structure) */
    int numDelays;     /**< Number of code delays to be explored */
    int *delays;     /**< Code delays to be explored (points to the AcqCommon structure) */
    unsigned int threshold;     /**< Threshold used to test CAF (points to the AcqCommon structure) */
    short int *localCode;     /**< Pointer to the complex conjugate FFT of despreading code (points to the AcqCommon structure) */
    int compressedLength;     /**< The length of a compressed output from 1 ms input */
    /* algorithm parameters */
    int fftSize;     /**< Size of the FFT */
    int log2FftSize;     /**< Log2 of fftSize */
    int fftGain;     /**< Gain to be applied to the FFT function */
    int ifftGain;     /**< Gain to be applied to the IFFT function */
    int shiftsForNonCohSums;     /**< Amount of left shift used to limit the range of the values out of IFFT */
    int maxNumOfNonCoherentSums;     /**< Maximum number of non coherent sums for this channel */
    int numOfNonCoherentSums;     /**< Number of non coherent sums */
    /* coarse acquisition indexes */
    int dopplerIndex;     /**< Index of the Doppler shift to be explored */
    int delayIndex;     /**< Index of the code delay to be explored */
    int msCounter;     /**< Counts the number of milliseconds in the current try */
    int nonCoherentSumsIndex;     /**< Counts the current non coherent sums */
    /* samples */
    short int *signalCompressed;     /**< Array used to store the result of a compression */
    short int *signalFft;     /**< Array used to store the result of an FFT */
    short int *ifftOut;     /**< Array used to store the output of the IFFT */
    unsigned int *ifftAbs;     /**< Array used to store absolute values of the samples out of the IFFT */
    unsigned int *nonCoherentSums;     /**< Array used to store the output of the non coherent sums */
    /* maximum */
    unsigned int maximum;     /**< Current maximum of the CAF */
    float dopplerMax;     /**< Doppler used to obtain the maximum */
    int delayMax;     /**< Index of the sample corresponding to the maximum */
    unsigned long long int discardedSamples;     /**< Number of sample discarded from the input buffer when maximum is found */
    /* optional log */
    FILE *acqBinLog;     /**< Optional binary log file for acquisition */
} TCoarseAcquisition;

/**
 * @struct RefinementStruct
 * @brief Stores the refinement variables specific to the single channel.
 */
typedef struct RefinementStruct {
    int numOfNonCoherentSums;     /**< Number of non coherent sums */
    int indexBin;     /**< Index of the bin explored by the refinement */
    int msCounter;     /**< Index of the ms in the current correlation */
    int nonCoherentSumsIndex;     /**< Counts the current non coherent sums */
    int pI;     /**< In-phase correlation */
    int pQ;     /**< Quadrature correlation */
    unsigned int p;     /**< Modulus of the correlation */
    unsigned int maximum;     /**< Maximum correlation obtained (modulus) */
    int indexBinMax;     /**< Index of the bin for which the maximum has been found */
    unsigned long long int discardedSamples;     /**< Number of sample discarded from the input buffer when refinement starts */
} TRefinement;

/**
 * @struct ConfirmationStruct
 * @brief Stores the confirmation variables specific to the single channel. It uses M-on-N strategy (at least M correlations over the threshold on N tries).
 */
typedef struct ConfirmationStruct {
    int numOfNonCoherentSums;     /**< Number of non coherent sums */
    int successesRequired;     /**< M parameter for the current constellation */
    int maxNumOfTries;     /**< N parameter for the current constellation */
    unsigned int threshold;     /**< Confirmation threshold for the current constellation */
    int msCounter;     /**< Index of the ms in the current correlation */
    int nonCoherentSumsIndex;     /**< Counts the current non coherent sums */
    int pI;     /**< In-phase correlation */
    int pQ;     /**< Quadrature correlation */
    unsigned int p;     /**< Modulus of the correlation */
    int successCounter;     /**< Number of successes obtained up-to-now */
    int tryCounter;     /**< Number of attempts made up to now */
} TConfirmation;

#endif                          /* ACQUISITION_DATA_STRUCTURE_H_INCLUDED */
