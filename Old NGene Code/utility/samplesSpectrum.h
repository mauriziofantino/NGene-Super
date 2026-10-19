/**
 * @file samplesSpectrum.h
 *
 * @author Mario Nicola
 *
 * @date 20/11/2014
 *
 * @version 1.0.0
 *
 * @brief Implementations of basic functions for the computation of the spectrum
 *
 * @note From a previous release by Andrea Molino
 */

#ifndef SAMPLES_SPECTRUM_H_INCLUDED
#define SAMPLES_SPECTRUM_H_INCLUDED

#include "ngeneConfiguration.h"
#include "fft.h"


/**
 * @def SPECTRUM_WEIGHT_FUNC_RECTANGULAR
 * @brief Symbol used to indicate actual weighting function used in spectrum computation
 */
#define SPECTRUM_WEIGHT_FUNC_RECTANGULAR	0

/**
 * @def SPECTRUM_WEIGHT_FUNC_TRIANGULAR
 * @brief Symbol used to indicate actual weighting function used in spectrum computation
 */
#define SPECTRUM_WEIGHT_FUNC_TRIANGULAR		1

/**
 * @def SPECTRUM_WEIGHT_FUNC_HANNING
 * @brief Symbol used to indicate actual weighting function used in spectrum computation
 */
#define SPECTRUM_WEIGHT_FUNC_HANNING		2

/**
 * @def SPECTRUM_WEIGHT_FUNC_HAMMING
 * @brief Symbol used to indicate actual weighting function used in spectrum computation
 */
#define SPECTRUM_WEIGHT_FUNC_HAMMING		3


/**
 * @struct SamplesSpectrumStruct
 * @brief Stores all the variables used to evaluate the spectrum
 *        of the input samples
 */
typedef struct SamplesSpectrumStruct
{
	int spectrumSent;					/**< This flag is FALSE when a spectrum must be sent */
	unsigned int windowLen;				/**< window length (number of FFT points) */
	unsigned int windowLenLog2; 		/**< log2(window length) */
	unsigned int windowNum;				/**< number of windows (used to average the output of FFT) */
	unsigned int windowOverlap;			/**< overlapping samples between two consecutive windows */
	unsigned int windowType;			/**< indicates the window type, i.e. a function used to weight the input samples */
	unsigned int windowTableLen;		/**< length of the table containing the windowing function */
	unsigned int dataWindowLen;			/**< number of samples in each window (excluding zero padding) */
	float spectrumNormalizationFactor;	/**< normalization factor for the spectrum results */
	unsigned int neededSamples;			/**< total number of samples needed for the computation of the spectrum (averaging included) */
	int inputAverageValid;				/**< TRUE_FLAG when the inputAverage variable is updated */
	float inputAverage;					/**< mean value of the input samples */
	char *inputSamples;  				/**< Points to the array containing the input samples */
	float *windowTable;					/**< samples of windowing function */
	short int *fftInput;				/**< input of FFT (after overlapping, windowing and zero padding) */
	short int *fftOutput;				/**< output of FFT */
	unsigned int spectrumSize;			/**< number of output samples (only half of the FFT outputs are considered) */
	float *spectrumData;				/**< output samples */
} TSamplesSpectrum;


/**
 * @brief Initialize variables and allocate memory used for the spectrum computation
 *
 * @param cfg points to the receiver configuration
 * @param spectrum points to the structure used to store data for the spectrum computation
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int InitSamplesSpectrum(TNGeneConfiguration *cfg,TSamplesSpectrum *spectrum);

/**
 * @brief Deinit the structure used for the spectrum computation
 *
 * @param spectrum points to the structure used to store data for the spectrum computation
 */
void DeinitSamplesSpectrum(TSamplesSpectrum *spectrum);

/**
 * @brief Compute the spectrum
 *
 * @param cfg points to the receiver configuration
 * @param fftTable points to the structure storing variables used during FFT computation
 * @param spectrum points to the structure used to store data for the spectrum computation
 * @param samples points to the data to be analyzed. If NULL, the input array in the spectrum structure
 *        is elaborated.
 * @param availableSamples number of samples available in the buffer pointed by inputData
 *
 * @return FAILURE_RETURN if I don't have enough samples to compute spectrum, else SUCCESS_RETURN.
 */
int EvaluateSamplesPowerSpectrum(TNGeneConfiguration *cfg,TFftTable *fftTable,TSamplesSpectrum *spectrum,char *samples,unsigned int availableSamples);

#endif /* SAMPLES_SPECTRUM_H_INCLUDED */
