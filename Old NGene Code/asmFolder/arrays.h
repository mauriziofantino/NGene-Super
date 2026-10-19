/**
 * @file arrays.h
 *
 * @author Alessandro Nordio
 *
 * @date 18/05/2007
 *
 * @version 1.0.0
 *
 * @brief Functions that elaborate arrays.
 *        Implementation is optimized through the use of assembly.
 *        Some minor changes introduced by Mario Nicola (20/08/2014)
 */
#ifndef ARRAYS_H_INCLUDED
#define ARRAYS_H_INCLUDED

#include "simd.h"
#include "ngeneConfiguration.h"

/**
   @brief ArrayXor is element-wise product of vectors of bytes using xor (8-bit)
   @details Approximates the product using XOR

   Computes : c[i] = a[i] xor b[i];

   where a[i] or b[i] has values in the set {0x00, 0xff} (0x00 for +1 and 0xff for -1)

   if b[i] = 0x00 then  a[i] xor b[i] = a[i]      (as the product a[i] *(+1) )
   if b[i] = 0xff then  a[i] xor b[i] = -a[i]-1   (approximates the product a[i] *(-1) = -a[i] )

 * @param a  pointer to the 1st vector of inputs
 * @param b  pointer to the 2nd vector of inputs
 * @param c  pointer to the output vector
 * @param n  number of elements
 */
void ArrayXor(char *a, char *b, char *c, unsigned int n);

/**
     @brief ArrayMac (Array Multiply And Cumulate) performs dot product of vectors of byte integers  (8-bit)

     @details Computes the inner product of the two integer byte sequences using bitwise xor
     and SIMD SSE2 instruction set

     y = sum_i (a[i] bitwise_xor b[i]), i=0, ..., n-1

     a:  pointer to the first byte sequence. The sequence is n bytes long.

     b:  pointer to the second byte sequence. The sequence is n bytes long.

         The elements of one sequence should be signed integers with values either 0 (hex 0x00) or -1 (hex 0xff);
         The other sequence has signed integer elements 'x' with 'd' BITS OF DYNAMIC, i.e.  -2^(d-1)  <= x <= 2^(d-1)-1

     n:  sequence length


     WARNINGS
         1) XOR and MULTIPLY:
                 if b=0  then (a xor b) = a
                 if b=-1 then (a xor b) = -a-1

                 Usually XOR operator is used to replace standard product.
                 Let c[i] = +1 if b[i]=0 and c[i] = -1 if b[i]=-1,
                 and let sb = sum_i b[i].

                 Then y1 = sum_i (a[i]*c[i]) = sum_i (a[i] bitwise_xor b[i]) + (n-sb)/2


         2) OVERFLOW:
                 The result of the dot product is given as a 32-bit signed integer.
                 To avoid overflow the input dynamic 'd' should be d<=4
                 If d>4 overflow may happen


 * @param a  pointer to the 1st vector of inputs
 * @param b  pointer to the 2nd vector of inputs
 * @param n  number of elements
 * @return   \f$ y = \sum (a[i] xor b[i]) \f$
*/
int ArrayMac(char *a, char *b, unsigned int n);

/**
 * @brief elementwise product of complex vectors
   @details Elementwise complex product of 2 vectors of length n

   C[i] = A[i]*B[i]; A[i], B[i], C[i] are complex

   A[i] = a[2*i]+j*a[2i+1];    (vector a contains 2*n short integers; format (Re Im), (Re Im), ....
   B[i] = b[2*i]+j*b[2i+2];    (vector b contains 4*n short integers; format (Re -Im Im Re), (Re -Im Im Re), ....
   C[i] = c[2*i]+j*c[2i+1];    (vector a contains 2*n short integers; format (Re Im), (Re Im), ....

   Each Re or Im is a 16-bit signed integer
   a, b, c should be aligned to 16 byte boundary

 * @param a  pointer to the 1st input  in the format [Re Im] for each of the n samples.
 * @param b  pointer to the 2nd input  in the format [Re -Im Im Re] for each of the n samples
 * @param c  pointer to the output     in the format [Re Im] for each of the n samples
 * @param n  number of complex elements to multiply
 */
void ArrayProductComplex(TNGeneConfiguration *cfg,short *a,short *b,short *c,unsigned int n);

/**
 * @details Computes the square of the absolute value of complex numbers
 * - The input is a vector of n complex numbers in the format [Re Im], pointed by 'c'
 *   where Re and Im are short integers.
 * - The output is a vector of  32-bit unsigned integers.
 * - The size 'n' of the vectors should be a multiple of 4
 *
 *   s[i] = c[2*i]^2 + c[2*i+1]^2
 *
 * @param cfg points to the receiver configuration
 * @param c pointer to complex vector. Each complex element is in the format [Re Im], where Re and Im are short integers
 * @param s pointer to square absolute values vector. Each element is a 32-bit unsigned integer.
 * @param n number of complex samples in the input array (should be a multiple of 4)
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int ArrayAbs2(TNGeneConfiguration *cfg, short *c, unsigned int *s, unsigned int n);

/**
 * @details Find the maximum in the vector pointed by 'v'. Vector 'v' has size n
 * The function returns the maximum and the index of the maximum
 *
 * @param v pointer to the input vector
 * @param max pointer to the maximum
 * @param index pointer to the index of the maximum ( 0 <= *index < n )
 * @param n input vector length
 */
void ArrayMax(unsigned int *v, unsigned int *max, unsigned int *index, unsigned int n);

/**
 * @brief ArrayAdd performs the sum of two arrays
 *
 * @details c = a + b. The length of each array is arrayLength.
 *
 * @param a			pointer to the first input array
 * @param b			pointer to the second input array
 * @param c			pointer to the sum array
 * @param arrayLength	number of bytes to be added
 */
void ArrayAdd(char *a, char *b, char *c, int arrayLength);

/**
 * @brief ArrayAddUnsignedInt performs the sum of two arrays of unsigned integers (32 bits each)
 *
 * @details c = a + b. The length of each array is arrayLength.
 *
 * @param a			pointer to the first input array
 * @param b			pointer to the second input array
 * @param c			pointer to the sum array
 * @param arrayLength	number of bytes to be added
 */
void ArrayAddUnsignedInt(unsigned int *a, unsigned int *b, unsigned int *c, int arrayLength);

/**
 * @brief ArraySub performs the subtraction of two arrays
 *
 * @details c = a - b. The length of each array is arrayLength.
 *
 * @param a			pointer to the first input array
 * @param b			pointer to the second input array
 * @param c			pointer to the sum array
 * @param arrayLength	number of bytes to be subtracted
 */
void ArraySub(char *a, char *b, char *c, int arrayLength);


/**
 * @brief ArrayClean resets all sample of the output array
 *
 * @details uses simd in order to initialize 16 bytes per time.
 *
 * @param outputArray pointer to the destination memory location
 * @param arrayLength number of bytes to be reset
 */
void ArrayClean(char *outputArray, int arrayLength);

/**
 * @brief ArrayCopy performs a copy of an array of char (samples)
 * @details uses simd in order to initialize 16 bytes per time.
 *
 * @param inputArray contains the data to convert
 * @param outputArray pointer to the destination memory location
 * @param arrayLength number of bytes to be copied
 */
void ArrayCopy(char *inputArray,char *outputArray,int arrayLength);

/**
 * @brief Performs carrier wipe-off on input samples using cos+i*sin
 *
 * @details Manages up to two bunches of input samples
 *
 * @param input1 points to the first bunch of samples
 * @param size1 number of samples in the first bunch
 * @param input2 points to the number of samples in the second bunch (maybe NULL)
 * @param size2 number of samples in the second bunch (maybe 0)
 * @param cosArray array with cosine samples
 * @param sinArray array with sine samples
 * @param carrierSize number of samples in the carrier array (same for sine and cosine)
 * @param cosDemod array containing samples demodulated using cosine
 * @param sinDemod array containing samples demodulated using sine
 */
void CarrierComplexWipeOff(char *input1, unsigned int size1, char *input2, unsigned int size2, char *cosArray, char *sinArray, unsigned int carrierSize, char *cosDemod, char *sinDemod);

/**
 * @brief Performs carrier wipe-off on input samples using only cos (or sin). For subcarrier removal.
 *
 * @details Manages up to two bunches of input samples
 *
 * @param input1 points to the first bunch of samples
 * @param size1 number of samples in the first bunch
 * @param input2 points to the number of samples in the second bunch (maybe NULL)
 * @param size2 number of samples in the second bunch (maybe 0)
 * @param carrierArray array with sinusoidal samples
 * @param carrierSize number of samples in the carrier array (same for sine and cosine)
 * @param demod array containing samples demodulated
 */
void CarrierRealWipeOff(char *input1, unsigned int size1, char *input2, unsigned int size2, char *carrierArray, unsigned int carrierSize, char *demod);
#endif /* ARRAYS_H_INCLUDED */
