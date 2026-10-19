/**
 * @file stereoTriton.h
 *
 * @author Micaela Troglia Gamba
 *
 * @date 21/12/2015
 *
 * @version 1.0.0
 *
 * @brief This function performs in a fast way the data conditioning.
 *        Used when a file saved from a Stereo Triton front-end is elaborated.
 *
 * @note It is currently based on SIMD instructions
 */

#ifndef STEREO_TRITON_H_INCLUDED
#define STEREO_TRITON_H_INCLUDED

/**
 * @brief Data are translated from the Stereo Triton format to the receiver one
 *
 * @details Stereo Triton outputs 16 bit samples with 8 bit real part, 8 bit imaginary part, fs=8.192MHz, at baseband.
 *          This functions translates them into 8 bit samples (real), fs=16.384MHz, at fif=4.096MHz
 *
 * @param inputArray contains the data to convert
 * @param outputArray will contain the data after the conversion
 * @param arrayLength is the length of both input and output array
 * @param maskOffset points to the index for the mask used to translate the samples at IF
 *        (the values is updated at each execution)
 * @param shift is the amount of the right shift to be performed in order to have a suitable range of input values
 */
void StereoTritonSamplesElab(char *inputArray, char *outputArray, int arrayLength, int *maskOffsetPtr, int shift);

#endif
