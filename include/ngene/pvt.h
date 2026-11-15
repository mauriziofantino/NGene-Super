/** @file pvt.h @brief Common-epoch broadcast position/velocity/time, double precision C API. */
#ifndef NGENE_PVT_H
#define NGENE_PVT_H
#include "ngene/tracking.h"

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
} TNgPvtSnapshot;

/** Compute broadcast satellite ECEF, velocity, clock and drift at transmit time. */
int NgPvtSatellite(const TNgBroadcast *broadcast, unsigned constellation, double transmitTime,
                   double position[3], double velocity[3], double *clock, double *drift);
/** Solve common-epoch code/Doppler equations. No allocations, globals or source ownership. */
void NgPvtCompute(const TNgTrackingSnapshot channels[64], uint64_t epoch, uint64_t sample,
                  double sampleRate, TNgPvtSnapshot *solution);
#endif
