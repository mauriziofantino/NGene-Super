/** @file sbas.h
 * @brief EGNOS L1 differential and ionospheric correction storage.
 *
 * A receiver owns one bank per tracked GEO assignment. Zero-initialize the bank
 * before its first frame and discard it after a GEO change or sample discontinuity.
 * Serialize decoder writes with PVT reads. Frame ages use the receiver sample
 * timeline, independently of GPS time of week.
 */
#ifndef NG_SBAS_H
#define NG_SBAS_H
#include <stdint.h>

#define NG_SBAS_FRAME_BYTES 32u /**< Storage for 250 MSB-first bits, padded to a byte. */
#define NG_SBAS_MASK_SIZE 51u   /**< Maximum number of satellites in one MT1 mask. */

/** One non-polar MT18 mask and its mask-relative MT26 values. */
typedef struct {
    uint8_t mask[201];   /**< Physical grid indices in transmission order, zero-based. */
    uint16_t delay[201]; /**< Encoded delay plus one, zero for unavailable, UINT16_MAX for veto. */
    double time[201];    /**< MT26 reception time per entry, in signal seconds. */
    double maskTime;     /**< MT18 reception time in signal seconds. */
    unsigned count;      /**< Number of enabled points in the mask. */
    unsigned issue;      /**< IODI shared by the mask and accepted delay messages. */
} TNgSbasIonoBand;

/** One GEO's correction bank, reset on a source or continuity change. */
typedef struct {
    unsigned count;                  /**< Number of active slots in the MT1 mask. */
    unsigned iodp;                   /**< Issue of data for the active PRN mask. */
    unsigned prn[NG_SBAS_MASK_SIZE]; /**< SBAS PRN identifiers in mask order. */
    double maskTime;                 /**< Last accepted MT1 reception time in signal seconds. */
    double lastFrame;                /**< Last accepted CRC-valid frame time in signal seconds. */
    double blockedUntil;             /**< End of MT0 exclusion in signal seconds. */
    unsigned frameCount;             /**< Accepted frames since the assignment began. */
    TNgSbasIonoBand ionosphere[9];   /**< MT18/MT26 bands used by the supported grid. */

    struct {
        double range;          /**< Additive pseudorange correction in metres. */
        double time;           /**< Reception time in signal seconds. */
        unsigned iodf;         /**< Issue of data for the fast correction. */
        unsigned valid;        /**< Eligibility after the UDRE veto. */
    } fast[NG_SBAS_MASK_SIZE]; /**< Fast corrections indexed by MT1 mask position. */

    struct {
        double xyz[3];          /**< ECEF position correction in metres. */
        double velocity[3];     /**< ECEF velocity correction in metres per second. */
        double clock;           /**< Additive satellite clock correction in seconds. */
        double drift;           /**< Satellite clock correction rate in seconds per second. */
        double tod;             /**< Reference GPS time of day in seconds. */
        double time;            /**< Reception time in signal seconds. */
        unsigned iode;          /**< Matching broadcast ephemeris issue. */
        unsigned valid;         /**< A long-term correction is present. */
        unsigned dynamic;       /**< Message includes velocity, clock drift and time. */
    } orbit[NG_SBAS_MASK_SIZE]; /**< Long-term corrections indexed by MT1 mask position. */
} TNgSbasCorrections;

/** Additive GPS L1 corrections expressed at transmit time. */
typedef struct {
    double position[3]; /**< ECEF position correction in metres. */
    double velocity[3]; /**< ECEF velocity correction in metres per second. */
    double clock;       /**< Satellite clock correction in seconds. */
    double drift;       /**< Satellite clock correction rate in seconds per second. */
    double range;       /**< Pseudorange correction in metres. */
} TNgSbasCorrection;

/** @brief Accept one CRC-valid SBAS L1 frame in chronological order.
 * @param[in,out] bank Receiver-owned GEO state.
 * @param[in] data Packed MSB-first frame, with the low six padding bits ignored.
 * @param[in] signalTime Nonnegative reception time in signal seconds.
 * @return One if the frame is accepted, including unsupported message types, or zero
 *         on invalid input, CRC or timing. The frame buffer is not retained.
 */
int NgSbasPushFrame(TNgSbasCorrections *bank, const uint8_t data[NG_SBAS_FRAME_BYTES],
                    double signalTime);

/** @brief Read matched fast and long-term corrections for a GPS satellite.
 * @param[in] bank Correction state, or NULL for unavailable.
 * @param[in] prn GPS PRN in the range 1 through 32.
 * @param[in] iode Issue of the broadcast ephemeris used by the solver.
 * @param[in] signalTime Observation time in signal seconds.
 * @param[in] gpsTransmitTow Satellite transmit time in GPS week seconds.
 * @param[out] correction Copy of the correction, zeroed if unavailable.
 * @return One if both corrections are eligible, or zero otherwise.
 */
int NgSbasRead(const TNgSbasCorrections *bank, unsigned prn, unsigned iode, double signalTime,
               double gpsTransmitTow, TNgSbasCorrection *correction);

/** @brief Interpolate the SBAS grid at the GPS L1 ionospheric pierce point.
 * @param[in] bank Correction state, or NULL for unavailable.
 * @param[in] signalTime Observation time in signal seconds.
 * @param[in] latitude Receiver geodetic latitude in radians.
 * @param[in] longitude Receiver longitude in radians.
 * @param[in] azimuth Line-of-sight azimuth clockwise from north in radians.
 * @param[in] elevation Line-of-sight elevation above the horizon in radians.
 * @param[out] delay Slant delay in metres, written only on success.
 * @return One when eligible grid points cover the pierce point, or zero otherwise.
 */
int NgSbasIonosphere(const TNgSbasCorrections *bank, double signalTime, double latitude,
                     double longitude, double azimuth, double elevation, double *delay);
#endif
