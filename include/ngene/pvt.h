/** @file pvt.h @brief Common-epoch broadcast position/velocity/time, double precision C API. */
#ifndef NGENE_PVT_H
#define NGENE_PVT_H
#include "ngene/tracking.h"
#include "ngene/sbas.h"

/** Real line of sight at the solution epoch; all entries participated in the solve. */
typedef struct {
    unsigned prn;           /**< Broadcast satellite PRN. */
    unsigned constellation; /**< Broadcast satellite identity. */
    double azimuth;         /**< Azimuth clockwise from north in radians. */
    double elevation;       /**< Elevation above the local horizon in radians. */
} TNgPvtSatelliteView;

/** Copied solution; available denotes converged broadcast least squares, not integrity. */
typedef struct {
    int available;                     /**< Position/clock iteration converged with full rank. */
    int velocityAvailable;             /**< Doppler velocity/drift solve has full rank. */
    unsigned satellites;               /**< Total admitted observations. */
    unsigned gps;                      /**< Admitted GPS observations. */
    unsigned galileo;                  /**< Admitted Galileo observations. */
    uint64_t epoch;                    /**< Receiver continuity generation. */
    uint64_t sample;                   /**< Receiver observation index within the stream. */
    double towSeconds;                 /**< Nominal receiver TOW before constellation bias. */
    double ecef[3];                    /**< Receiver ECEF position in metres. */
    double velocity[3];                /**< Receiver ECEF velocity in metres per second. */
    double latitude;                   /**< WGS84 geodetic latitude in radians. */
    double longitude;                  /**< WGS84 longitude in radians. */
    double height;                     /**< WGS84 ellipsoidal height in metres. */
    double clockGps;                   /**< GPS code clock bias in metres. */
    double clockGalileo;               /**< Galileo code clock bias in metres. */
    double clockDrift;                 /**< Common receiver drift in metres per second. */
    double residualRms;                /**< Post-fit code residual RMS [m]. */
    int dopAvailable;                  /**< Geometry has full rank. */
    double gdop;                       /**< Geometric dilution of precision. */
    double hdop;                       /**< Horizontal dilution of precision. */
    double vdop;                       /**< Vertical dilution of precision. */
    unsigned skyCount;                 /**< Number of real solved lines of sight. */
    TNgPvtSatelliteView sky[64];       /**< Solution-epoch satellite sky geometry. */
    unsigned egnosPrn;                 /**< Current auxiliary GEO PRN. */
    unsigned egnosFrames;              /**< CRC-valid frames in the current GEO assignment. */
    unsigned egnosCorrected;           /**< GPS observations using differential corrections. */
    unsigned egnosIonosphereCorrected; /**< GPS observations using the SBAS ionosphere grid. */
    unsigned ionosphereCorrected[3];   /**< Applied delays for GPS, Galileo and BeiDou. */
    unsigned beidou;                   /**< Admitted B1C measurements. */
    double clockBeidou;                /**< Independent B1C receiver code bias [m]. */
} TNgPvtSnapshot;

/** Raw common-epoch code and Doppler observables, before broadcast corrections. */
typedef struct {
    uint64_t epoch;           /**< Receiver continuity generation. */
    uint64_t sample;          /**< Receiver observation index within the stream. */
    unsigned prn;             /**< Broadcast satellite PRN. */
    unsigned constellation;   /**< Satellite identity. */
    double nativeTransmitTow; /**< Native code-time TOW [s]. */
    double commonTransmitTow; /**< Nominal GPST-equivalent code-time TOW [s]. */
    double pseudorange; /**< Code range from supplied receiver TOW, including clock biases [m]. */
    double dopplerHz;   /**< Measured carrier Doppler with IF removed [Hz]. */
    double rangeRate;   /**< Doppler-derived range rate, positive away [m/s]. */
} TNgPvtObservation;

/** @brief Form a broadcast observation at a receiver sample.
 * @param[in] channel Completed tracking and navigation snapshot.
 * @param[in] epoch Receiver continuity generation.
 * @param[in] sample Observation sample index in that generation.
 * @param[in] sampleRate Native sample rate in hertz.
 * @param[in] receiverTow Nominal receiver time in GPS week seconds.
 * @param[out] observation Caller-provided result.
 * @retval NG_OK The observation was written.
 * @retval NG_END The channel lacks usable data or belongs to another epoch.
 * @retval NG_ERROR The caller supplied invalid input.
 */
int NgPvtObserve(const TNgTrackingSnapshot *channel, uint64_t epoch, uint64_t sample,
                 double sampleRate, double receiverTow, TNgPvtObservation *observation);

/** @brief Propagate broadcast ephemeris at the native transmit time.
 * @param[in] broadcast Validated clock and ephemeris data.
 * @param[in] constellation GNSS signal identifier.
 * @param[in] transmitTime Satellite transmit time in the constellation time scale, in seconds of week.
 * @param[out] position Satellite ECEF coordinates in metres.
 * @param[out] velocity Satellite ECEF velocity in metres per second.
 * @param[out] clock Satellite clock correction in seconds.
 * @param[out] drift Satellite clock drift in seconds per second.
 * @return One for a valid propagated state, zero otherwise.
 */
int NgPvtSatellite(const TNgBroadcast *broadcast, unsigned constellation, double transmitTime,
                   double position[3], double velocity[3], double *clock, double *drift);
/** @brief Solve common-epoch code/Doppler with broadcast ionosphere enabled and no SBAS.
 * @param[in] channels Array of 64 tracking snapshots.
 * @param[in] epoch Receiver continuity generation.
 * @param[in] sample Solution sample index.
 * @param[in] sampleRate Native sample rate in hertz.
 * @param[out] solution Caller-owned result with availability flags.
 */
void NgPvtCompute(const TNgTrackingSnapshot channels[64], uint64_t epoch, uint64_t sample,
                  double sampleRate, TNgPvtSnapshot *solution);
/** @brief Solve common-epoch code/Doppler without SBAS.
 * @param[in] channels Array of 64 tracking snapshots.
 * @param[in] epoch Receiver continuity generation.
 * @param[in] sample Solution sample index.
 * @param[in] sampleRate Native sample rate in hertz.
 * @param[in] broadcastIonosphere Nonzero to use eligible broadcast delays.
 * @param[out] solution Caller-owned result with availability flags.
 */
void NgPvtComputeWithIonosphere(const TNgTrackingSnapshot channels[64], uint64_t epoch,
                                uint64_t sample, double sampleRate, int broadcastIonosphere,
                                TNgPvtSnapshot *solution);
/** @brief Solve common-epoch position, velocity and clock with optional SBAS.
 * @param[in] channels Array of 64 snapshots, including unused receiver slots.
 * @param[in] epoch Receiver continuity generation.
 * @param[in] sample Epoch sample index in the receiver timeline.
 * @param[in] sampleRate Native sample rate in hertz.
 * @param[in] broadcastIonosphere Nonzero to apply eligible broadcast delays.
 * @param[in] sbas Optional GEO correction bank, borrowed for this call. A valid
 * SBAS grid delay takes precedence over the GPS broadcast delay.
 * @param[out] solution Caller-owned result. Check availability flags before use.
 * The caller must keep all inputs stable for the duration of the call.
 */
void NgPvtComputeAugmented(const TNgTrackingSnapshot channels[64], uint64_t epoch, uint64_t sample,
                           double sampleRate, int broadcastIonosphere,
                           const TNgSbasCorrections *sbas, TNgPvtSnapshot *solution);
#endif
