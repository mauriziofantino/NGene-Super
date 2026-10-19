/**
 * @file stereo.h
 *
 * @author Mario Nicola
 *
 * @date 23/09/2014
 *
 * @version 1.0.0
 *
 * @brief This function performs in a fast way the data conditioning.
 *        Used when an NVS Stereo front-end is connected to receiver.
 *
 * @note It is currently based on SIMD instructions
 */

#ifndef STEREO_H_INCLUDED
#define STEREO_H_INCLUDED


/**
 * @brief Data are translated from the Stereo format to the receiver one
 * @details Stereo in Intermediate Frequency configuration outputs samples with this format
 * 			 s7    s6     s5     s4     s3     s2     s1     s0
 * 			L1[1] L1[0] L2i[2] L2i[1] L2i[0] L2q[2] L2q[1] L2q[0]
 * Where
 * - s is the sample (8 bit, index 7 is the most significant)
 * - L1 is related to L1/E1 signal at intermediate frequency (2 bits unsigned)
 * - L2i is related to the L2 signal, in-phase at base band (3 bits unsigned)
 * - L2q is related to the L2 signal, quadrature at base band (3 bits unsigned)
 * The function selects L1 bits and translates them from unsigned to signed.
 *
 * @param inputArray contains the data to convert
 * @param outputArray will contain the data after the conversion
 * @param arrayLength is the length of both input and output array
 */
void StereoSamplesElab(char *inputArray, char *outputArray, int arrayLength);


#endif /* STEREO_H_INCLUDED */
