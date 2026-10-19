/**
 * @file samplesHistogram.h
 *
 * @author Mario Nicola
 *
 * @date 19/11/2014
 *
 * @version 1.0.0
 *
 * @brief Implementations of basic functions for the computation of the histogram
 *
 * @note From a previous release by Andrea Molino
 */

#ifndef SAMPLES_HISTOGRAM_H_INCLUDED
#define SAMPLES_HISTOGRAM_H_INCLUDED


/**
 * @def HISTOGRAM_MAX_BITS
 * @brief The maximum number for bits considered for samples histogram
 *        is equal to the maximum number of bits in a sample from USB front end (8)
 */
#define HISTOGRAM_MAX_BITS 	8


/**
 * @struct SamplesHistogramStruct
 * @brief Stores all the variables used to evaluate the histogram
 *        of the input samples
 */
typedef struct SamplesHistogramStruct
{
	int firstExecution;					/**< Flag that indicates when the histogram results are sent to the GUI for the first time
	                                         (in that case, values on the X axis are sent) */
	int histogramSent;					/**< This flag is FALSE when a histogram must be sent */
	char *inputSamples; 				/**< Points to the array containing the input samples */
	unsigned int intervals;				/**< Number of possible values for the input samples */
	float dynamicMin;					/**< Minimum value of the input samples */
	unsigned int guiIntervals;			/**< Number of possible values considered by the GUI */
	float guiDynamicMin;				/**< Minimum value considered by the GUI */
	unsigned int neededSamples;			/**< Number of samples used to evaluate the histogram */
	float data[1<<HISTOGRAM_MAX_BITS];  /**< Histogram evaluated by the receiver */
	float *guiData;						/**< Histogram sent to the GUI */
	unsigned int guiPacketLength;  		/**< Size of histogramGuiPacket in bytes */
	char *guiPacket;					/**< Whole data packet sent to the GUI, containing a header and the histogram */
} TSamplesHistogram;


/**
 * @brief Initialize variables and allocate memory used for the histogram computation
 *
 * @param hist points to the structure used to store data for the histogram computation
 * @param bits number of bits set for the histogram computation
 * @param samples number of samples to analyze for the histogram computation
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int InitHistogram(TSamplesHistogram *hist,unsigned int bits,unsigned int samples);

/**
 * @brief Deinit the structure used for the histogram computation
 *
 * @param hist points to the structure used to store data for the histogram computation
 */
void DeinitHistogram(TSamplesHistogram *hist);

/**
 * @brief Get the data labels for the histogram plot (x axis)
 *
 * @param hist points to the structure used to store data for the histogram computation
 */
void GetHistogramIntervals(TSamplesHistogram *hist);

/**
 * @brief Compute the histogram
 *
 * @param hist points to the structure used to store data for the histogram computation
 * @param samples points to the data to be analyzed. If NULL, the input array in the histogram
 * 		  structure is elaborated.
 * @param availableSamples number of samples available in the buffer pointed by inputData
 *
 * @return FAILURE_RETURN if I don't have enough samples to compute histogram, else SUCCESS_RETURN.
 */
int EvaluateSampleHistogram(TSamplesHistogram *hist,char *samples,unsigned int availableSamples);

/**
 * @brief Compute the average value of a sequence of samples
 *
 * @param samples points to the samples to be averaged
 * @param avgLength is the number of samples to be used in average computation
 *
 * @return the average value
 */
float GetSignalAverage(char *samples,unsigned int avgLength);

#endif /* SAMPLES_HISTOGRAM_H_INCLUDED */
