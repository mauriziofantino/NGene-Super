/**
 * @file short2char.h
 *
 * @author Mario Nicola
 *
 * @date 23/09/2020
 *
 * @version 1.0.0
 *
 * @brief Function that translates an array of short int (16 bits)
 *        into an array of char (8 bits).
 */
#ifndef SHORT2CHAR_H_INCLUDED
#define SHORT2CHAR_H_INCLUDED


/**
 * @brief FromShortArrayToCharArray translates an array of short int (16 bits)
 *        into an array of char (8 bits)
 * @details uses simd in order to elaborate 16 bytes per time.
 *          The compression uses the low 8 bits of the word: a shift
 *          is used to select the most significant bits from the input.
 *
 * @param inputArray contains the data to convert (short int)
 * @param outputArray pointer to the destination memory location (char)
 * @param arrayLength number of bytes to be copied
 * @param shift is the number of right shift performed to avoid saturation
 */
void FromShortArrayToCharArray(short int *inputArray,char *outputArray,int arrayLength,int shift);


#endif /* SHORT2CHAR_H_INCLUDED */


