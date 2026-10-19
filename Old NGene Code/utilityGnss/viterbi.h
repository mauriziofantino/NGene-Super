/**
 * @file viterbi.h
 *
 * @author Mario Nicola
 *
 * @date 13/10/2014
 *
 * @version 1.0.0
 *
 * @brief Functions implementing the Viterbi decoder used in Galileo and EGNOS.
 */

#ifndef VITERBI_H_INCLUDED
#define VITERBI_H_INCLUDED

/**
 * Init the metricTab table used during Viterbi decoding
 */
void InitViterbi(void);

/**
 * Executes Viterbi for the Galileo navigation message decoding
 *
 * @param finalPathMetric state metrics at the last trellis stage
 * @param decData buffer containing the decoded bits
 * @param symbols input symbols {0,1}
 * @param numOutBits number of decoded bits (tail included)
 * @param startState initial state for decoding (0 for Galileo)
 * @param endState final state for decoding (0 for Galileo)
 *
 * @return FAILURE_RETURN if something went wrong, SUCCESS_RETURN elsewhere
 */
int GalileoViterbi(long *finalPathMetric, unsigned char *decData, unsigned char *symbols, unsigned int numOutBits, unsigned int startState, unsigned int endState);


int EgnosViterbi(long *stateMetric,unsigned char *decData,unsigned char *addData,unsigned int blocksize,unsigned char *symbols,unsigned int numSymbols,unsigned char polarityCorrection, unsigned long *paths1,unsigned long *paths2);


#endif /* VITERBI_H_INCLUDED */
