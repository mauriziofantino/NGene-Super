/**
 * @file amungoNT1065.h
 *
 * @author Micaela Troglia Gamba
 *
 * @date 11/04/2017
 *
 * @version 1.0.0
 *
 * @brief Function used to manipulate in a fast way the data conditioning.
 *        Used when Amungo NT1065 front-end is connected to receiver.
 *
 * @note It is currently based on SIMD instructions
 */

#ifndef AMUNGONT1065_H_INCLUDED
#define AMUNGONT1065_H_INCLUDED


/**
 * @brief Data are translated from the Amungo NT1065 format to the receiver one
 * @details Amungo NT1065  outputs samples with this format
 * 			 s7    s6     s5     s4     s3     s2     s1     s0
 * 			L4[1] L4[0]   L3[1] L3[0]   L2[1] L2[0]   L1[1] L1[0]
 * Where
 * - s is the sample (8 bit, index 7 is the most significant)
 * - L1 is related to the first channel signal at intermediate frequency (2 bits signed magnitude)
 * - L2 is related to the second channel signal at intermediate frequency (2 bits signed magnitude)
 * - L3 is related to the third channel signal at intermediate frequency (2 bits signed magnitude)
 * - L4 is related to the forth channel signal at intermediate frequency (2 bits signed magnitude)
 *   The function selects L1 bits and translates them from {00,01,10,11} into {1,3,-1,-3}.
 *
 * @param inputArray contains the data to convert
 * @param outputArray will contain the data after the conversion
 * @param arrayLength is the length of both input and output array
 */
void AmungoNT1065SamplesElab(char *inputArray, char *outputArray, int arrayLength);


#endif /* AMUNGONT1065_H_INCLUDED */
