/*
 * @file fft.h
 *
 * @author Alessandro Nordio
 *
 * @date 25-07-2007
 *
 * @brief Header file for Fast Fourier Transform library
 *        Some minor changes introduced by Mario Nicola (20/08/2014)
*/
#ifndef __FFT__INCLUDED
#define __FFT__INCLUDED

#include "simd.h"
#include "ngeneConfiguration.h"

/**
 * @def DEBUG_AND_VERIFY_FFT
 * @brief It enables some pieces of code for debug and verification purposes of the PerformFft function
 */
//#define  DEBUG_AND_VERIFY_FFT

typedef struct FftTableStruct
{
  unsigned int  maxLog2FftSize;  /**< log2 of the maximum supported size for FFT */
  unsigned int  maxFftSize;      /**< maximum supported size for FFT */
  unsigned int  *reverse;        /**< pointer to reverse bit indices. Has size 'max_fft_size' */
  s128          *twiddles;       /**< pointer to twiddle factors in the format [Re -Im Im Re] for each twiddle
                                      Re and Im are represented on 16 bits. Therefore each twiddle need 4x16 bits
                                      'twiddles' has size (max_fft_size-4)/2  s128 objects */
} TFftTable;


/**
 * @brief Initializes the FFT table structure, by computing twiddles and the reverse bit indices
 *
 * @param cfg points to the receiver configuration
 * @param t  Structure filled by the function
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int InitFftTable(TNGeneConfiguration *cfg,TFftTable *t);

/**
 * @brief Frees the memory allocated in the structure t
 *
 * @param t  Structure to free
 */
void DeinitFftTable(TFftTable *t);


/**
 * @brief Fast Fourier Transform using fixed point arithmetic
 *        Computes b = fft(a) where a and b are complex vectors with N complex elements each.
 *
 * @param cfg points to the receiver configuration
 * @param table is a table containing some precomputed parameters (such as twiddles, reverse bit indices, ecc)
 * @param a is the pointer to the input samples. It should be aligned to a 16 byte boundary and contains 2^(log2_size) complex elements in the format (Re Im)
 *        where both Re and Im are signed short integers
 * @param b is the pointer to the output samples.It should be aligned to a 16 byte boundary and contains 2^(log2_size) complex elements in the format (Re Im)
 *        where Re and Im are signed short integers
 *
 * @param log2Size is the base-2 logarithm of the fft size (i.e. the number of complex elements in a and b)
 * @param gain sets the gain of the fft routine.   -log2_size <= gain <= 0. It amplifies the output by 2^gain (To be used when the output overflows)
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int PerformFft(TNGeneConfiguration *cfg,TFftTable *table,short *a,short *b,unsigned int log2Size,int gain);


#endif
