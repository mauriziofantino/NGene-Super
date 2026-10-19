/**
 * @file circularBuffer.h
 *
 * @author Mario Nicola
 *
 * @date 14/08/2014
 *
 * @version 1.0.0
 *
 * @brief Managing of the pointers in the circular buffer
 *
 * @details The buffer is composed of
 * - The tail of the buffer that represents the first usable sample (oldest)
 * - The head of the buffer pointing to the first free location
 * - fill1 that represents how many sample there are from the tail to the end of the buffer
 * - fill2 that represents how many samples there are from the beginning of the buffer.
 * - free1: the size of the 1st free space of the buffer
 * - free2: the size of the 2nd free space of the buffer
 *
 * fill1 + fill2 = all usable samples inside the buffer
 */

#ifndef CIRCULAR_BUFFER_H_INCLUDED
#define CIRCULAR_BUFFER_H_INCLUDED

#include "ngeneConfiguration.h"
#include "simd.h"
#include "frontend.h"

/**
 * @def INPUT_BUFFER_SIZE
 * @brief number of milliseconds of samples in the input buffer
 */
#define INPUT_BUFFER_SIZE 16	

/**
 * @struct CircularBufferStruct
 * @brief Structure to handle the circular buffer.
 */
typedef struct CircularBufferStruct {
    unsigned int dim;						/**< Dimension of the buffer */
    unsigned int head;						/**< Head of the fresh data */
    unsigned int tail;						/**< Tail of the fresh data */
    unsigned int fill1;						/**< Dimension of the first part that compose the fresh data */
    unsigned int fill2;						/**< Dimension of the second part that compose the fresh data */
    unsigned int free1;						/**< Size of the first free block */
    unsigned int free2;						/**< Size of the second free block */
    char *buffer;							/**< The buffer (samples for the E1 band) */
    unsigned long long int discardedSamples;			/**< Number of already elaborated samples */
    /* variables related to sample conversion */
    char sigeExpected;									/**< specifies if the next samples that will be read from SIGE front-end
                                                             will be an in-phase (0x02) or a quadrature one (0x00) */
    int sigeMaskOffset;									/**< samples from SIGE must be demodulated using two different laws alternatively.
    														This is done through a mask, sigeMaskOffset is the offset used to access this mask */
    short int *usrpSamples;								/**< Temporary buffer used to store samples read from the file as a sequence of short int at base-band
     	 	 	 	 	 	 	 	 	 	 	 	 	 	 Ready for the conversion in the internal format */
    char *stereoTritonSamples;						/**< Temporary buffer used to store samples read from the file as a sequence of char at base-band
     	 	 	 	 	 	 	 	 	 	 	 	 	 	 Ready for the conversion in the internal format */
    int usrpMaskOffset;									/**< samples from USRP must be demodulated using two different laws alternatively.
    														 This is done through a mask, usrpMaskOffset is the offset used to access this mask */
    int stereoTritonMaskOffset;							/**< samples from Stereo TRITON must be demodulated using two different laws alternatively.
        													 This is done through a mask, stereoTritonMaskOffset is the offset used to access this mask */
    int jrcMaskOffset;									/**< samples from JRC FE must be demodulated using two different laws alternatively.
    														 This is done through a mask, jrcMaskOffset is the offset used to access this mask */
    int shiftAmount;									/**< Amount of the right shift (in bits) to be performed in order to limit the input range */
    unsigned char unsignedToSignedCorrection[XMM_REGISTER_SIZE];/**< Array containing the correction to be used in unsigned to signed conversion */
} TCircularBuffer;


/**
 * @brief Initializes the buffer where input samples are stored
 *
 * @param cfg points to the receiver configuration
 * @param buf points to the buffer structure
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int InitCircularBuffer(TNGeneConfiguration * cfg, TCircularBuffer * buf);

/**
 * @brief De-initializes the buffer where input samples are stored
 *
 * @param buf points to the buffer structure
 */
void DeinitCircularBuffer(TCircularBuffer * buf);

/**
 * @brief Update the pointers of the Circular buffer after some new samples are inserted
 *
 * @param buf is the pointer to the buffer structure
 * @param displacement is the amount of data inserted
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int InsertIntoCircularBuffer(TCircularBuffer * buf, unsigned int displacement);

/**
 * @brief Update the pointers of the Circular buffer after some samples are discarded
 *
 * @param buf is the pointer to the buffer structure
 * @param displacement is the amount of data to be deleted
 *
 * @return The number of effectively deleted samples
 */
unsigned int DeleteFromCircularBuffer(TCircularBuffer * buf, unsigned int displacement);

/**
 * @brief Translates samples from the format employed by the used front-end
 *        to the internal format supported by NGene (schar)
 *
 * @param cfg points to the receiver configuration
 * @param buf is the pointer to the buffer structure
 * @param ptr points to the first sample to be translated
 * @param n is the number of samples to be translated
 */
void ConvertSamplesIntoNGeneFormat(TNGeneConfiguration * cfg, TCircularBuffer * buf, char *ptr, int n);

/**
 * @brief This function tries to fill the circular buffer, reading samples from the input
 *        file or USB front-end. Read samples are converted into internal NGene2 format.
 *
 * @param cfg points to the receiver configuration
 * @param buf is the pointer to the buffer structure
 * @param rxFe rxFe pointer to the Front End structure
 *
 * @return SUCCESS_RETURN when the buffer is filled, FAILURE_RETURN when buffer
 *    	   still exhibits empty space.
 */
int FillInputBuffer(TNGeneConfiguration * cfg, TCircularBuffer * buf, TRxFe * rxFe);

/**
 * @brief This function copies the most recent samples in the input buffer into an output array
 *
 * @param buf is the pointer to the buffer structure
 * @param neededSamples is the number of samples to be copied into the output buffer
 * @param outputArray points to the memory where selected samples will be copied
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int CopyABunchOfContiguousSamples(TCircularBuffer * buf, int neededSamples, char *outputArray);

#endif
