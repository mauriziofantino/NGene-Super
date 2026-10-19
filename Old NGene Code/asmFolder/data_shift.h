/**
 * @file data_shift.h 
 *
 * @author Mario Nicola
 *
 * @date 03/08/2007
 *
 * @version 1.0.0
 *
 * @brief Function sets to manipulate in a fast way the data conditioning. As an example
 *        the conversion between unsigned to signed or the reduction of the dynamic.
 *
 * @note It is currently based on SIMD instructions
 *
 */

#ifndef DATA_SHIFT_H_INCLUDED
#define DATA_SHIFT_H_INCLUDED

/**
 * @brief Function which converts from unsigned to signed and apply a shift to the data
 *
 * @details Amount of the shift is an input parameter to the function:
 * the shift is needed because a too high dynamic in the input samples
 * could lead to overflows in the elaboration of the signal. 
 * Receiver elaborates samples with an 8 bit signed format, but some
 * front-ends support unsigned output. Conversion from unsigned to signed 
 * is performed subtracting the main value to the signal.
 * For example if raw samples are \f$ \in [0,255] \f$ with average value 128,
 * they lead to an elaborated signal with samples \f$ \in [-128,127]\f$ 
 * after a 128 subtraction. This correction value is 
 * imposed through configuration file that describes the supported front-ends.
 * Then it's worth noticing that, in order to reduce computation complexity,
 * function performs first the shift, then the unsigned-signed conversion
 * (then correction constant must be initialized accordingly).
 * 
 * @param inputArray contains the data to convert
 * @param outputArray will contain the data after the conversion
 * @param arrayLength is the length of both input and output array
 * @param shiftedBits is the right shift to apply to the data
 * @param *correction is an array containing the corrections to be subtracted in order to
 *        perform the unsigned to Signed conversion
 */
void RightShiftU2sByte(char *inputArray, char *outputArray, int arrayLength, int shiftedBits,unsigned char *correction);

/**
 * @brief Function which shifts the data to reduce their dynamic
 *
 * @details Amount of the shift is an input parameter to the function:
 * the shift is needed because a too high dynamic in the input samples
 * could lead to overflows in the elaboration of the signal. 
 * 
 * @param inputArray contains the data to convert
 * @param outputArray will contain the data after the conversion
 * @param arrayLength is the length of both input and output array
 * @param shiftedBits is the right shift to apply to the data
 */
void RightShiftByte(char *inputArray, char *outputArray, int arrayLength, int shiftedBits);

/**
 * @brief Used to perform left shift of values in a short int array
 *
 * @param inputArray contains the data to shift
 * @param outputArray  will contain the data after the shift
 * @param arrayLength is the length of both input and output array
 * @param shiftedBits is the right shift to apply to the data
 */
void LeftShiftWord(short int *inputArray,short int *outputArray,int arrayLength,int shiftedBits);

/**
 * @brief Used to perform right shift of values in a unsigned int array
 *
 * @param inputArray contains the data to shift
 * @param outputArray  will contain the data after the shift
 * @param arrayLength is the length of both input and output array
 * @param shiftedBits is the right shift to apply to the data
 */
void RightShiftUnsignedInt(unsigned int *inputArray,unsigned int *outputArray,int arrayLength,int shiftedBits);
#endif
