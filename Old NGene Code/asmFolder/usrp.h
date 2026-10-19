/**
 * @file usrp.h
 *
 * @author Mario Nicola
 *
 * @date 29/04/2015
 *
 * @version 1.0.0
 *
 * @brief This function performs in a fast way the data conditioning.
 *        Used when a file saved from a USRP front-end is elaborated.
 *
 * @note It is currently based on SIMD instructions
 */

#ifndef USRP_H_INCLUDED
#define USRP_H_INCLUDED

/**
 * @brief Data are translated from the USRP format to the receiver one
 *
 * @details USRP outputs 32 bit samples with 16 bit real part, 16 bit imaginary part, fs=5MHz, at baseband.
 *          This functions translates them into 8 bit samples (real), fs=10MHz, at fif=2.5MHz
 *
 * @param inputArray contains the data to convert
 * @param outputArray will contain the data after the conversion
 * @param arrayLength is the length of both input and output array
 * @param maskOffset points to the index for the mask used to translate the samples at IF
 *        (the values is updated at each execution)
 * @param shift is the amount of the right shift to be performed in order to have a suitable range of input values
 */
void UsrpSamplesElab(short int *inputArray, char *outputArray, int arrayLength, int *maskOffsetPtr, int shift);

#endif
