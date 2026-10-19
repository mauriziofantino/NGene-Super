/**
 * @file sampleCompression.h
 *
 * @author Mario Nicola
 *
 * @date 09/06/2012
 *
 * @version 1.0.0
 *
 * @brief Function used to perform sample compression.
 */

#ifndef SAMPLE_COMPRESSION_H_INCLUDED
#define SAMPLE_COMPRESSION_H_INCLUDED

/**
 * @brief SampleCompression reduces the size of an input array, accumulating consecutive samples
 *
 * @details The input samples are contained in two separate arrays of char (one for real part and one for imaginary part).
 *          The output samples are contained in one array of short with real and imaginary part interleaved.
 *          The length of each input array is numberOfInputs char.
 *          The length of the output array is twice numberOfOutputs short (both real and imaginary part).
 *
 * @param initialNco is the initial value of the NCO used to perform compression
 * @param offset is a displacement w.r.t. the input pointers
 * @param real is the input array (real part)
 * @param imag is the input array (imaginary part)
 * @param numberOfInputs is the number of input samples to be compressed
 * @param s is the output array
 * @param numberOfOutputs is the number of output samples compressed
 *
 * @return the final value of the NCO
 */
unsigned int SampleCompression(unsigned int initialNco, unsigned int offset, char *real, char *imag, unsigned int numberOfInputs, short *s, unsigned int numberOfOutputs);

#endif /* SAMPLE_COMPRESSION_H_INCLUDED */
