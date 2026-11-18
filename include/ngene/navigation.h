/** @file navigation.h @brief Per-channel GPS LNAV demodulation on coherent prompt epochs. */
#ifndef NGENE_NAVIGATION_H
#define NGENE_NAVIGATION_H
#include <stdint.h>
#include "ngene/broadcast.h"

/** Value telemetry; TOW denotes the next subframe at the last validated boundary. */
typedef struct {
    uint8_t hkroot;          /**< First eight OSNMA bits on the validated odd I/NAV page. */
    uint8_t mack[4];         /**< Following 32 OSNMA bits, MSB first. */
    uint64_t boundarySample; /**< End sample of this CRC-validated page. */
    uint64_t pages;          /**< Number of CRC-validated nominal OSNMA pages. */
    int valid;               /**< Raw data present; never implies authentication. */
} TNgOsnmaPage;

typedef struct {
    TNgBroadcast broadcast;  /**< Per-satellite pending and committed broadcast values. */
    TNgOsnmaPage osnma;      /**< Latest raw Galileo OSNMA fragment. */
    int bitSynchronized;     /**< Five verified 20-ms transition intervals. */
    int towValid;            /**< All ten words of a subframe passed parity. */
    unsigned towSeconds;     /**< GPS HOW, seconds modulo one week. */
    unsigned subframeId;     /**< Validated subframe 1..5. */
    uint64_t symbols;        /**< Completed 20-ms data bits. */
    uint64_t validSubframes; /**< Full parity-validated subframes. */
    uint64_t parityErrors;   /**< Rejected word candidates. */
    uint64_t boundarySample; /**< Exclusive sample boundary of validated subframe. */
    double
        transmitTimeSeconds; /**< Transmit time at tracking endSample, including code fraction. */
} TNgNavigationSnapshot;

/** Bounded private state; owner serializes reset/feed/read through its channel job. */
typedef struct {
    TNgNavigationSnapshot snapshot; /**< Copied observation state. */
    uint64_t epochs, transition;    /**< One-ms prompt counters. */
    int transitions;                /**< -1 before first transition, then valid intervals. */
    double previous, sum;           /**< Prompt sign history and data-bit accumulator. */
    double inPhasePower, quadPower; /**< Classic length-100 exponential PLL power means. */
    int quality;                    /**< Classic low PLL threshold passed. */
    unsigned bitMs;                 /**< Prompt epochs accumulated in one bit. */
    uint32_t shift, words[10];      /**< Two previous parity bits plus current 30-bit word. */
    unsigned wordBits, wordCount;   /**< Word assembly cursors. */
    unsigned sinceValidBits;        /**< Age in symbols of the last validated subframe. */
    int collecting;                 /**< A normal or inverted TLM preamble was found. */

    struct {
        unsigned preamble, count, polarity, age; /**< I/NAV framing and freshness. */
        unsigned char symbols[240], even[15];    /**< Private coded and even-half storage. */
        int collecting, haveEven;                /**< Page assembly state. */
        uint64_t evenSymbol;                     /**< Last even-half ending symbol. */
    } galileo;                                   /**< Four-ms Galileo symbol decoder. */
} TNgNavigation;

/** Reset all timing and fragments on assignment, RF gap or loss of qualification. */
void NgNavigationReset(TNgNavigation *navigation);
/** Discard demodulation/timing after carrier loss, retaining the per-satellite broadcast cache. */
void NgNavigationLoseTiming(TNgNavigation *navigation);
/** Verify and polarity-correct one GPS word, including preceding D29 and D30. */
int NgNavigationGpsParity(uint32_t *word);
/** Feed one GPS data bit; used by the prompt demodulator and deterministic fixtures. */
void NgNavigationGpsBit(TNgNavigation *navigation, unsigned bit, uint64_t endSample);
/** Feed each complete 1-ms GPS prompt exactly once; FLL epochs never contribute bits. */
void NgNavigationGpsPrompt(TNgNavigation *navigation, double i, double q, int pll,
                           uint64_t endSample);
/** Feed each coherent 4-ms E1-B prompt once; page parity/CRC establishes data validity. */
void NgNavigationGalileoPrompt(TNgNavigation *navigation, double i, int pll, uint64_t endSample);
/** Parse already parity-validated GPS words into the per-satellite broadcast cache. */
int NgBroadcastGps(TNgBroadcast *broadcast, const uint32_t words[10], unsigned id);
/** Parse a CRC-validated I/NAV data word; returns one when it supplies GST. */
int NgBroadcastGalileo(TNgBroadcast *broadcast, const unsigned char data[16]);
#endif
