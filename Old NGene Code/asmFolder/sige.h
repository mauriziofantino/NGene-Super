/**
 * @file sige.h
 *
 * @author Mario Nicola
 *
 * @date 04/07/2008
 *
 * @version 1.0.0
 *
 * @brief This function performs in a fast way the data conditioning.
 *        Used when a SIGE front-end is connected to receiver.
 *
 * @note It is currently based on SIMD instructions
 */

#ifndef SIGE_H_INCLUDED
#define SIGE_H_INCLUDED

/**
 * @brief Data are mapped from the SIGE notation {0,1} to the receiver one {1, -1}
 *
 * @param inputArray contains the data to convert
 * @param outputArray will contain the data after the conversion
 * @param arrayLength is the length of both input and output array
 * @param maskOffset is the number of read samples mod 4 (used as an offset in order to choose the demodulation law)
 */
void SigeSamplesElab(char *inputArray, char *outputArray, int arrayLength,int maskOffset);

#endif
