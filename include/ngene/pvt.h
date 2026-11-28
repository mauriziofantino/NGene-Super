/** @file pvt.h @brief Common-epoch broadcast position/velocity/time, double precision C API. */
#ifndef NGENE_PVT_H
#define NGENE_PVT_H
#include "ngene/tracking.h"
#include "ngene/sbas.h"

/** Real line of sight at the solution epoch; all entries participated in the solve. */
typedef struct {
    unsigned prn, constellation; /**< Broadcast satellite identity. */
    double azimuth, elevation;   /**< Local north-clockwise azimuth and elevation [rad]. */
} TNgPvtSatelliteView;

/** Copied solution; available denotes converged broadcast least squares, not integrity. */
typedef struct {
    int available;                      /**< Position/clock iteration converged with full rank. */
    int velocityAvailable;              /**< Doppler velocity/drift solve has full rank. */
    unsigned satellites, gps, galileo;  /**< Measurements admitted at the common epoch. */
    uint64_t epoch, sample;             /**< Input continuity generation and solution sample. */
    double towSeconds;                  /**< Nominal receiver TOW before constellation bias. */
    double ecef[3], velocity[3];        /**< ECEF metres and metres/second. */
    double latitude, longitude, height; /**< WGS84 radians and ellipsoidal metres. */
    double clockGps, clockGalileo, clockDrift; /**< Clock biases [m], common drift [m/s]. */
    double residualRms;                        /**< Post-fit code residual RMS [m]. */
    int dopAvailable;                          /**< Geometry has full rank. */
    double gdop, hdop, vdop;                   /**< Unweighted geometry, separate code clocks. */
    unsigned skyCount;                         /**< Number of real solved lines of sight. */
    TNgPvtSatelliteView sky[64];               /**< Solution-epoch satellite sky geometry. */
    unsigned egnosPrn, egnosFrames, egnosCorrected; /**< GEO and EGNOS data used at this epoch. */
    unsigned egnosIonosphereCorrected; /**< GPS observations using the SBAS ionosphere grid. */
    unsigned ionosphereCorrected[3];   /**< Applied delays for GPS, Galileo and BeiDou. */
    unsigned beidou;                   /**< Admitted B1C measurements. */
    double clockBeidou;                /**< Independent B1C receiver code bias [m]. */
} TNgPvtSnapshot;

/** Raw common-epoch code and Doppler observables, before broadcast corrections. */
typedef struct {
    uint64_t epoch, sample;      /**< Receiver continuity/time anchor. */
    unsigned prn, constellation; /**< Satellite identity. */
    double nativeTransmitTow;    /**< Native code-time TOW [s]. */
    double commonTransmitTow;    /**< Nominal GPST-equivalent code-time TOW [s]. */
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
 * @return One when the observation is usable, zero for mismatched epochs,
 * unhealthy or stale data, or unsupported signals.
 */
int NgPvtObserve(const TNgTrackingSnapshot *channel, uint64_t epoch, uint64_t sample,
                 double sampleRate, double receiverTow, TNgPvtObservation *observation);

/** @brief Propagate broadcast ephemeris at the native transmit time.
 * @param[in] broadcast Validated clock and ephemeris data.
 * @param[in] constellation GNSS signal identifier.
 * @param[in] transmitTime Satellite transmit time in native week seconds.
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
