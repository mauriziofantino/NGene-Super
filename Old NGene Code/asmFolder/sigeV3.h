/**
 * @file sigeV3.h
 *
 * @author Mario Nicola
 *
 * @date 15/07/2015
 *
 * @version 1.0.0
 *
 * @brief This function performs in a fast way the data conditioning.
 *        Used when a SIGE V3 front-end is connected to receiver.
 *
 * @note It is currently based on SIMD instructions
 */

#ifndef SIGE_V3_H_INCLUDED
#define SIGE_V3_H_INCLUDED

/**
 * @brief Data are mapped from the SIGE notation {0,1} to the receiver one {1, -1}
 *
 * @param inputArray contains the data to convert
 * @param outputArray will contain the data after the conversion
 * @param arrayLength is the length of both input and output array
 */
void SigeV3SamplesElab(char *inputArray, char *outputArray, int arrayLength);

#endif
