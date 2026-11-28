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
    unsigned ionosphereCorrected[3]; /**< Applied delays for GPS, Galileo and BeiDou. */
    unsigned beidou; /**< Admitted B1C measurements. */
    double clockBeidou; /**< Independent B1C receiver code bias [m]. */
} TNgPvtSnapshot;

/** Raw common-epoch code and Doppler observables, before broadcast corrections. */
typedef struct {
    uint64_t epoch, sample; /**< Receiver continuity/time anchor. */
    unsigned prn, constellation; /**< Satellite identity. */
    double nativeTransmitTow; /**< Native code-time TOW [s]. */
    double commonTransmitTow; /**< Nominal GPST-equivalent code-time TOW [s]. */
    double pseudorange; /**< Code range from supplied receiver TOW, including clock biases [m]. */
    double dopplerHz; /**< Measured carrier Doppler with IF removed [Hz]. */
    double rangeRate; /**< Doppler-derived range rate, positive away [m/s]. */
} TNgPvtObservation;

/** Form an observation at sample using a nominal GPST receiver TOW.
 * Reject mismatched epochs, stale/unhealthy data and unsupported constellations. */
int NgPvtObserve(const TNgTrackingSnapshot *channel, uint64_t epoch, uint64_t sample,
                 double sampleRate, double receiverTow, TNgPvtObservation *observation);

/** Compute broadcast satellite ECEF, velocity, clock and drift at transmit time. */
int NgPvtSatellite(const TNgBroadcast *broadcast, unsigned constellation, double transmitTime,
                   double position[3], double velocity[3], double *clock, double *drift);
/** Solve common-epoch code/Doppler equations. No allocations, globals or source ownership. */
void NgPvtCompute(const TNgTrackingSnapshot channels[64], uint64_t epoch, uint64_t sample,
                  double sampleRate, TNgPvtSnapshot *solution);
/** Solve with explicit control of the broadcast ionospheric corrections. */
void NgPvtComputeWithIonosphere(const TNgTrackingSnapshot channels[64], uint64_t epoch,
                                uint64_t sample, double sampleRate, int broadcastIonosphere,
                                TNgPvtSnapshot *solution);
/** Solve with one optional GEO bank. Ages are tied to the input sample timeline. */
void NgPvtComputeAugmented(const TNgTrackingSnapshot channels[64], uint64_t epoch,
                            uint64_t sample, double sampleRate, int broadcastIonosphere,
                            const TNgSbasCorrections *sbas, TNgPvtSnapshot *solution);
#endif
