/**
 * @file acqConfiguration.h
 *
 * @author Mario Nicola
 *
 * @date 07/08/2014
 *
 * @version 1.0.0
 *
 * @brief This header defines the structure that stores NGene configuration for ACQ
 *        from both GUI and configuration files.
 */

#ifndef ACQ_CONFIGURATION_H_INCLUDED
#define ACQ_CONFIGURATION_H_INCLUDED

/**
 * @struct AcqConfigurationStruct
 *
 * @brief Contains the NGene receiver configuration for different acquisition steps.
 */
typedef struct AcqConfigurationStruct {
    /* PFA */
    float gpsFalseDetectionProb;     /**< false detection probability for GPS */
    float galileoFalseDetectionProb;     /**< false detection probability for Galileo */
    float egnosFalseDetectionProb;     /**< false detection probability for EGNOS */
    /* coarse acquisition */
    /* FFT */
    int gpsFftSize;     /**< size of FFT/IFFT used in the coarse acquisition of GPS/EGNOS (depends on fs) */
    int galileoFftSize;     /**< size of FFT/IFFT used in the coarse acquisition of Galileo (depends on fs) */
    int gpsLog2FftSize;     /**< log2 of the size of FFT/IFFT used in the coarse acquisition of GPS/EGNOS (depends on fs) */
    int galileoLog2FftSize;     /**< log2 of the size of FFT/IFFT used in the coarse acquisition of Galileo (depends on fs) */
    int gpsLocalCodeFftGain;     /**< gain used in the FFT of the signal in the coarse acquisition of GPS/EGNOS (depends on fs) */
    int galileoLocalCodeFftGain;     /**< gain used in the FFT of the signal in the coarse acquisition of Galileo (depends on fs) */
    int gpsFftGain;     /**< gain used in the FFT of the signal in the coarse acquisition of GPS/EGNOS (depends on fs) */
    int galileoFftGain;     /**< gain used in the FFT of the signal in the coarse acquisition of Galileo (depends on fs) */
    int gpsIfftGain;     /**< gain used in the IFFT in the coarse acquisition of GPS/EGNOS (depends on fs) */
    int galileoIfftGain;     /**< gain used in the IFFT in the coarse acquisition of Galileo (depends on fs) */
    /* others */
    float gpsMaximumDoppler;     /**< maximum Doppler value for GPS [Hz] */
    float galileoMaximumDoppler;     /**< maximum Doppler value for Galileo [Hz] */
    float egnosMaximumDoppler;     /**< maximum Doppler value for EGNOS [Hz] */
    float gpsDopplerStep;     /**< frequency bin step for GPS [Hz] */
    float galileoDopplerStep;     /**< frequency bin step for Galileo [Hz] */
    float egnosDopplerStep;     /**< frequency bin step for EGNOS [Hz] */
    float gpsCodeDelayStep;     /**< code delay step for GPS [chips] */
    float galileoCodeDelayStep;     /**< code delay step for Galileo [chips] */
    float egnosCodeDelayStep;     /**< code delay step for EGNOS [chips] */
    /* Non coherent sums */
    int gpsNumOfNonCohSums;     /**< number of non-coherent sums performed during acquisition of the GPS signal */
    int galileoNumOfNonCohSums;     /**< number of non-coherent sums performed during acquisition of the Galileo signal */
    int egnosNumOfNonCohSums;     /**< number of non-coherent sums performed during acquisition of the EGNOS signal */
    /* M on N confirmation */
    int gpsConfirmationM;     /**< M value for GPS acquisition confirmation */
    int gpsConfirmationN;     /**< N value for GPS acquisition confirmation */
    int galileoConfirmationM;     /**< M value for GPS acquisition confirmation */
    int galileoConfirmationN;     /**< N value for GPS acquisition confirmation */
    int egnosConfirmationM;     /**< M value for GPS acquisition confirmation */
    int egnosConfirmationN;     /**< N value for GPS acquisition confirmation */
    /* refinement */
    float dopplerDeltaRefinement;     /**< Delta of the values tested by Doppler refinement [Hz] */
    float dopplerStepRefinement;     /**< Step used by Doppler refinement [Hz] */
    float codeDelayDeltaRefinement;     /**< Delta of the values tested by code delay refinement [chip] */
    int codeDelayStepRefinement;     /**< Step used by code delay refinement [sample] */
} TAcqConfiguration;

#endif                          /* ACQ_CONFIGURATION_H_INCLUDED */
