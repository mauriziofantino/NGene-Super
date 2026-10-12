/** @file types.h @brief Units, sample representation and block ownership. */
#ifndef NGENE_TYPES_H
#define NGENE_TYPES_H
#include "ngene/limits.h"
#include <stddef.h>
#include <stdint.h>

/** @brief Result codes for bounded stream and lifecycle operations. */
typedef enum { NG_OK = 0, NG_TIMEOUT = 1, NG_END = 2, NG_FULL = 3, NG_ERROR = -1 } TNgStatus;

/** @brief Caller-owned bounded diagnostic message. */
typedef struct {
    /** Bounded NUL-terminated diagnostic text; owned by this object. */
    char message[256];
} TNgError;

/** One simultaneous complex pair; signed components, no IF reconstruction. */
typedef struct {
    /** Signed in-phase ADC level in CI8 representation. */
    int8_t i;
    /** Signed quadrature ADC level in CI8 representation. */
    int8_t q;
} TNgIq8;

_Static_assert(sizeof(TNgIq8) == 2, "CI8 must occupy exactly two bytes");

/** @brief Wire representation of complex frontend samples. */
typedef enum { NG_FORMAT_CI8 = 1, NG_FORMAT_CU8 = 2, NG_FORMAT_SC16_Q11 = 3 } TNgSampleFormat;

/** @brief Rate, RF reference and borrowed frontend identity for one stream. */
typedef struct {
    /** Complex pairs per second, not component rate. */
    uint32_t sampleRateHz;
    /** Hardware frequency, never signal target. */
    uint64_t centerFrequencyHz;
    /** Configured analogue baseband filter. */
    uint32_t bandwidthHz;
    /** Representation of each simultaneous complex pair. */
    TNgSampleFormat format;
    /** Borrowed for the frontend lifetime. */
    const char *sourceName;
    /** Device-relative; never inferred GNSS time. */
    const char *clockDomain;
    /** Borrowed serial/identifier; may be NULL. */
    const char *deviceIdentifier;
    /** Requested signal centre in hertz; independent of SDR tuning. */
    uint64_t signalFrequencyHz;
    /** signal_frequency - hardware_frequency. */
    int64_t digitalIfHz;
} TNgStreamInfo;

enum { NG_BLOCK_GAP = 1u, NG_BLOCK_PARTIAL = 2u, NG_BLOCK_HARDWARE_TIME = 4u };

/** @brief Sample-index and discontinuity information for an immutable block. */
typedef struct {
    /** Continuity generation; changes when the stream time reference is reset. */
    uint64_t epoch;
    /** Absolute within epoch, including known losses. */
    uint64_t firstSample;
    /** Exact only when missingKnown is nonzero. */
    uint64_t missingBefore;
    /** Nonzero only when missingBefore is an exact sample-loss count. */
    int missingKnown;
    /** Combination of NG_BLOCK_GAP and NG_BLOCK_PARTIAL. */
    unsigned flags;
    /** Complex pairs. */
    size_t count;
    /** Device sample-clock ticks; valid only with NG_BLOCK_HARDWARE_TIME. */
    uint64_t hardwareTimestamp;
} TNgBlockMeta;

/** Read-only view. Valid until release/return from the receiving callback. */
typedef struct {
    /** Borrowed complex-pair array; lifetime follows the enclosing view contract. */
    const TNgIq8 *samples;
    /** Sample indices, continuity and length for this view. */
    TNgBlockMeta meta;
    /** Native representation; zero is the legacy CI8 default. For other formats use
     * NgIqComponent/NgIqStore rather than dereferencing samples as TNgIq8. The borrowed
     * storage contains exactly meta.count * NgSampleBytes(format) bytes. */
    TNgSampleFormat format;
} TNgIqView;

/** @brief Describe the native IQ pair stride; zero denotes legacy CI8.
 * @param format Native sample representation.
 * @return Two bytes for CI8/CU8, four for little-endian SC16 Q11, zero if unsupported.
 */
size_t NgSampleBytes(TNgSampleFormat format);
/** @brief Return the stable name of a native sample representation.
 * @param format Native format; zero denotes CI8 compatibility.
 * @return Borrowed static name, or "invalid".
 */
const char *NgSampleFormatName(TNgSampleFormat format);
/** @brief Read one signed ADC component without losing bladeRF precision.
 * @details CI8 reads are signed; CU8 subtracts 128; SC16 Q11 reads little-endian
 * signed 16-bit storage. Call only with a validated view and an in-range index.
 * @param view Borrowed native input.
 * @param index Complex-pair index smaller than meta.count.
 * @param quadrature Zero selects I, one selects Q.
 * @return Signed ADC level, in native ADC units.
 */
int NgIqComponent(const TNgIqView *view, size_t index, unsigned quadrature);
/** @brief Store a signed ADC component in native sample storage.
 * @details Value must lie within the representation's ADC range. SC16 Q11 is
 * written little-endian. No scaling or quantization is performed.
 * @param storage Writable native IQ pair array.
 * @param format Native representation.
 * @param index Complex-pair index within caller-owned capacity.
 * @param quadrature Zero selects I, one selects Q.
 * @param value Signed ADC level; CU8 is recentered by adding 128.
 */
void NgIqStore(void *storage, TNgSampleFormat format, size_t index, unsigned quadrature, int value);

/**
 * @brief Convert physical Doppler into a receiver carrier-NCO frequency.
 *
 * @details
 * Uses carrier = digital IF + Doppler. This metadata calculation performs no sample mixing and
 * uses the stream convention signal minus hardware tuning.
 *
 * @param info Stream metadata with digitalIfHz initialized.
 * @param dopplerHz Physical satellite Doppler estimate in hertz.
 *
 * @return Carrier frequency in the hardware-baseband reference, in hertz.
 */
double NgCarrierFromDoppler(const TNgStreamInfo *info, double dopplerHz);

/**
 * @brief Remove digital IF from an estimated carrier frequency.
 *
 * @details
 * Use this conversion before passing a physical Doppler observable to navigation/PVT. It
 * performs no sample processing.
 *
 * @param info Stream metadata with digitalIfHz initialized.
 * @param carrierHz Estimated baseband carrier-NCO frequency in hertz.
 *
 * @return Physical Doppler in hertz: carrier minus digital IF.
 */
double NgDopplerFromCarrier(const TNgStreamInfo *info, double carrierHz);

/**
 * @brief Format a bounded diagnostic message and return NG_ERROR.
 *
 * @details
 * Accepts printf-style arguments; escape a literal percent as %% in format. A NULL error
 * destination is supported and still returns failure.
 *
 * @param error Optional error structure to fill.
 * @param format printf-style format string.
 * @param ... Arguments required by the format specifiers.
 *
 * @return NG_ERROR in all cases.
 */
int NgFail(TNgError *error, const char *format, ...);
#endif
