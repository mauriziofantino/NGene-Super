/**
 * @file viterbi.c
 *
 * @author Mario Nicola
 *
 * @date 13/10/2014
 *
 * @version 1.0.0
 *
 * @brief Functions implementing the Viterbi decoder used in Galileo and EGNOS.
 */
#include <string.h>
#include <limits.h>

/*
 *
 */
#define BFLY1(inputStateIndex, outputParityBit)                                                    \
    {                                                                                              \
        long met0, met1;                                                                           \
        /* output state for input 0 */                                                             \
        met0 = cMet[inputStateIndex] + branchMetrics[outputParityBit];                             \
        met1 = cMet[inputStateIndex + 32] + branchMetrics[3 ^ outputParityBit];                    \
        nMet[2 * inputStateIndex] = met0;                                                          \
        if (met1 > met0) {                                                                         \
            nMet[2 * inputStateIndex] = met1;                                                      \
            dec |= 1UL << ((2 * inputStateIndex) & 31);                                            \
        }                                                                                          \
        /* output state for input 1 */                                                             \
        met0 = cMet[inputStateIndex] + branchMetrics[3 ^ outputParityBit];                         \
        met1 = cMet[inputStateIndex + 32] + branchMetrics[outputParityBit];                        \
        nMet[2 * inputStateIndex + 1] = met0;                                                      \
        if (met1 > met0) {                                                                         \
            nMet[2 * inputStateIndex + 1] = met1;                                                  \
            dec |= 1UL << ((2 * inputStateIndex + 1) & 31);                                        \
        }                                                                                          \
    }
/*
 *
 */
#define BFLY2(inputStateIndex, outputParityBit)                                                    \
    {                                                                                              \
        long met0, met1;                                                                           \
        /* ACS for 0 branch */                                                                     \
        met0 = nMet[inputStateIndex] + branchMetrics[outputParityBit];                             \
        met1 = nMet[inputStateIndex + 32] + branchMetrics[3 ^ outputParityBit];                    \
        cMet[2 * inputStateIndex] = met0;                                                          \
        if (met1 > met0) {                                                                         \
            cMet[2 * inputStateIndex] = met1;                                                      \
            dec |= 1UL << ((2 * inputStateIndex) & 31);                                            \
        }                                                                                          \
        /* ACS for 1 branch */                                                                     \
        met0 = nMet[inputStateIndex] + branchMetrics[3 ^ outputParityBit];                         \
        met1 = nMet[inputStateIndex + 32] + branchMetrics[outputParityBit];                        \
        cMet[2 * inputStateIndex + 1] = met0;                                                      \
        if (met1 > met0) {                                                                         \
            cMet[2 * inputStateIndex + 1] = met1;                                                  \
            dec |= 1UL << ((2 * inputStateIndex + 1) & 31);                                        \
        }                                                                                          \
    }

int NgGalileoViterbi(long *finalPathMetric, unsigned char *decData, unsigned char *symbols,
                     unsigned int numOutBits, unsigned int startState, unsigned int endState)
{
    int bitCounter = 0;
    int i, branchMetrics[4];
    unsigned long dec, paths[240], *pathPointer;
    int metricTab[2][256];
    if (numOutBits != 120) {
        return -1;
    }
    for (i = 0; i < 256; i++) {
        metricTab[0][i] = 128 - i;
        metricTab[1][255 - i] = 127 - i;
    }
    /* each section of the trellis consists of 64 states:
     * in the paths variable it's represented using two words of 32 bits each */
    long cMet[64], nMet[64];
    startState &= 63;
    endState &= 63;
    // Convert all values in 0 and 255
    // the number of input symbols is 2*numOutBits because the rate is 1/2 for both F/NAV and I/NAV
    for (i = 0; i < (int)(2 * numOutBits); i++) {
        if (symbols[i] == 1) {
            symbols[i] = 255;
        }
    }
    /* Initialize starting metrics */
    for (i = 0; i < 64; i++) {
        cMet[i] = -999999;
    }
    cMet[startState] = 0;
    pathPointer = paths;
    for (;;) {
        // Read input symbol pair and compute branch metrics
        branchMetrics[0] = metricTab[0][symbols[1] ^ 0xff] + metricTab[0][symbols[0]];
        branchMetrics[1] = metricTab[0][symbols[1] ^ 0xff] + metricTab[1][symbols[0]];
        branchMetrics[3] = metricTab[1][symbols[1] ^ 0xff] + metricTab[1][symbols[0]];
        branchMetrics[2] = metricTab[1][symbols[1] ^ 0xff] + metricTab[0][symbols[0]];
        symbols += 2;
        dec = 0;
        BFLY1(0, 0);
        BFLY1(6, 0);
        BFLY1(8, 0);
        BFLY1(14, 0);
        BFLY1(2, 3);
        BFLY1(4, 3);
        BFLY1(10, 3);
        BFLY1(12, 3);
        BFLY1(1, 1);
        BFLY1(7, 1);
        BFLY1(9, 1);
        BFLY1(15, 1);
        BFLY1(3, 2);
        BFLY1(5, 2);
        BFLY1(11, 2);
        BFLY1(13, 2);
        *pathPointer++ = dec;
        dec = 0;
        BFLY1(19, 0);
        BFLY1(21, 0);
        BFLY1(27, 0);
        BFLY1(29, 0);
        BFLY1(17, 3);
        BFLY1(23, 3);
        BFLY1(25, 3);
        BFLY1(31, 3);
        BFLY1(18, 1);
        BFLY1(20, 1);
        BFLY1(26, 1);
        BFLY1(28, 1);
        BFLY1(16, 2);
        BFLY1(22, 2);
        BFLY1(24, 2);
        BFLY1(30, 2);
        *pathPointer++ = dec;
        if (++bitCounter == (int)numOutBits) {
            *finalPathMetric = nMet[endState];
            break;
        }
        // Read input symbol pair and compute branch metrics
        branchMetrics[0] = metricTab[0][symbols[1] ^ 0xff] + metricTab[0][symbols[0]];
        branchMetrics[1] = metricTab[0][symbols[1] ^ 0xff] + metricTab[1][symbols[0]];
        branchMetrics[3] = metricTab[1][symbols[1] ^ 0xff] + metricTab[1][symbols[0]];
        branchMetrics[2] = metricTab[1][symbols[1] ^ 0xff] + metricTab[0][symbols[0]];
        symbols += 2;
        dec = 0;
        BFLY2(0, 0);
        BFLY2(6, 0);
        BFLY2(8, 0);
        BFLY2(14, 0);
        BFLY2(2, 3);
        BFLY2(4, 3);
        BFLY2(10, 3);
        BFLY2(12, 3);
        BFLY2(1, 1);
        BFLY2(7, 1);
        BFLY2(9, 1);
        BFLY2(15, 1);
        BFLY2(3, 2);
        BFLY2(5, 2);
        BFLY2(11, 2);
        BFLY2(13, 2);
        *pathPointer++ = dec;
        dec = 0;
        BFLY2(19, 0);
        BFLY2(21, 0);
        BFLY2(27, 0);
        BFLY2(29, 0);
        BFLY2(17, 3);
        BFLY2(23, 3);
        BFLY2(25, 3);
        BFLY2(31, 3);
        BFLY2(18, 1);
        BFLY2(20, 1);
        BFLY2(26, 1);
        BFLY2(28, 1);
        BFLY2(16, 2);
        BFLY2(22, 2);
        BFLY2(24, 2);
        BFLY2(30, 2);
        *pathPointer++ = dec;
        if (++bitCounter == (int)numOutBits) {
            *finalPathMetric = cMet[endState];
            break;
        }
    }
    /* Chain back from terminal state to produce decoded data (tail included) */
    if (decData == NULL) {
        return -1;
    }
    memset(decData, 0, (numOutBits + 7) / 8); /* round up in case nbits % 8 != 0 */
    for (i = (int)numOutBits - 1; i >= 0; i--) {
        pathPointer -= 2;
        decData[i >> 3] |= (endState & 0x01) << (7 - (i & 7));
        if (pathPointer[endState >> 5] & (1UL << (endState & 31))) {
            endState |= 64; /* 2^(K-1) */
        }
        endState >>= 1;
    }
    return 0;
}
