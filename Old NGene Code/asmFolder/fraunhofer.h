/**
 * @file fraunhofer.h
 *
 * @author Mario Nicola
 *
 * @date 04/07/2008
 *
 * @version 1.0.0
 *
 * @brief This function performs in a fast way the data conditioning.
 *        Used when a Fraunhofer front-end is connected to receiver.
 *
 * @note It is currently based on SIMD instructions
 *
 */

#ifndef FRAUNHOFER_H_INCLUDED
#define FRAUNHOFER_H_INCLUDED

/**
 * @brief Data are converted from the Fraunhofer notation (1 byte: 4 bit L2 & 4 bit L1)
 * to the NGene one (1 byte: L1 only)
 *
 * @param inputArray contains the data to convert
 * @param outputArray will contain the data after the conversion
 * @param arrayLength is the length of both input and output array
 */
void FraunhoferSamplesElab(char *inputArray, char *outputArray, int arrayLength);

#endif
