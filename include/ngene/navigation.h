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

/** @brief Reset the symbol decoder, week-time anchor and broadcast cache.
 * @param[out] navigation Channel-owned state. Call before first use or reassignment.
 */
void NgNavigationReset(TNgNavigation *navigation);

/** @brief Invalidate symbol and time synchronization after carrier loss.
 * @param[in,out] navigation Channel-owned state. Retains the satellite broadcast cache.
 */
void NgNavigationLoseTiming(TNgNavigation *navigation);

/** @brief Verify a GPS LNAV word with its preceding D29 and D30 bits.
 * @param[in,out] word Packed 32-bit value. Data polarity is corrected before checking.
 * @return One for a parity-valid word, zero otherwise.
 */
int NgNavigationGpsParity(uint32_t *word);

/** @brief Feed one already-demodulated GPS LNAV data bit.
 * @param[in,out] navigation Channel-owned word and subframe decoder.
 * @param[in] bit Binary data value.
 * @param[in] endSample Exclusive sample index of the bit boundary.
 */
void NgNavigationGpsBit(TNgNavigation *navigation, unsigned bit, uint64_t endSample);

/** @brief Feed one complete 1-ms GPS prompt while retaining the native LNAV cadence.
 * @param[in,out] navigation Channel-owned synchronization and decoder state.
 * @param[in] i Coherent in-phase prompt sum in native ADC units.
 * @param[in] q Coherent quadrature prompt sum in native ADC units.
 * @param[in] pll Nonzero when the carrier loop is in PLL.
 * @param[in] endSample Exclusive sample index of the prompt.
 * @param[in] transitionChecks Required count of consistent 20-ms transition intervals.
 * FLL input clears symbol timing. The caller must serialize all accesses to the state.
 */
void NgNavigationGpsPromptWithChecks(TNgNavigation *navigation, double i, double q, int pll,
                                     uint64_t endSample, unsigned transitionChecks);

/** @brief Feed one GPS prompt using the default five transition checks.
 * @param[in,out] navigation Channel-owned synchronization and decoder state.
 * @param[in] i Coherent in-phase prompt sum in native ADC units.
 * @param[in] q Coherent quadrature prompt sum in native ADC units.
 * @param[in] pll Nonzero when the carrier loop is in PLL.
 * @param[in] endSample Exclusive sample index of the prompt.
 */
void NgNavigationGpsPrompt(TNgNavigation *navigation, double i, double q, int pll,
                           uint64_t endSample);

/** @brief Feed one coherent 4-ms Galileo E1-B prompt into I/NAV.
 * @param[in,out] navigation Channel-owned framing and decoder state.
 * @param[in] i Coherent in-phase prompt sum in native ADC units.
 * @param[in] pll Nonzero when the carrier loop is in PLL.
 * @param[in] endSample Exclusive sample index of the symbol.
 */
void NgNavigationGalileoPrompt(TNgNavigation *navigation, double i, int pll, uint64_t endSample);

/** @brief Update the broadcast cache from ten parity-validated GPS words.
 * @param[in,out] broadcast Satellite cache owned by the caller.
 * @param[in] words Ten validated LNAV words in transmission order.
 * @param[in] id LNAV subframe number.
 * @return Decoder status.
 */
int NgBroadcastGps(TNgBroadcast *broadcast, const uint32_t words[10], unsigned id);

/** @brief Update the broadcast cache from a CRC-validated Galileo I/NAV word.
 * @param[in,out] broadcast Satellite cache owned by the caller.
 * @param[in] data Sixteen bytes of decoded word data.
 * @return One when the word supplies a GST anchor, zero otherwise.
 */
int NgBroadcastGalileo(TNgBroadcast *broadcast, const unsigned char data[16]);
#endif
