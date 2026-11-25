/** @file beidou_observation.h @brief CRC-validated B1C orbit/clock cache and BDT timing. */
#ifndef NGENE_BEIDOU_OBSERVATION_H
#define NGENE_BEIDOU_OBSERVATION_H
#include "ngene/beidou_bcnv1.h"
#include "ngene/dsp/beidou_b1c.h"
#include "ngene/navigation.h"

#define NG_BDT_WEEK_SECONDS 604800.0
#define NG_BDT_TO_GPST_SECONDS 14.0
#define NG_BDT_GPS_WEEK_ORIGIN 1356u
#define NG_BCNV1_MAX_BROADCAST_AGE 14400.0
#define NG_BCNV1_MAX_ANCHOR_AGE 36.0

/** Per-PRN state, zero-initialized before use. The receiver owns one cache per satellite. */
typedef struct {
    TNgBroadcast broadcast; /**< Last matched orbit/clock pair, including latest health. */
    uint64_t epoch, firstSample; /**< Continuity and SF1 first-chip sample. */
    unsigned prn, week, tow; /**< Latest timing anchor in native BDT. */
} TNgBcnv1Cache;

/** Consume a frame returned by the B-CNAV1 decoder. Retains the last matched pair while
 * IODE and the low eight IODC bits disagree. An invalid/backward frame leaves state intact.
 * A newer continuity epoch starts a fresh cache for the same PRN. */
int NgBcnv1Accept(TNgBcnv1Cache *cache, const TNgBcnv1Frame *frame, uint64_t epoch,
                  uint64_t firstSample);

/** Write an eligible navigation snapshot at sample in the same epoch.
 * advanceChips is the tracked code integral from the supplied SF1 first-chip anchor.
 * transmitTimeSeconds and broadcast references remain in BDT. The selected component
 * determines the group-delay correction. Returns NG_END for stale/unhealthy data. */
int NgBcnv1Navigation(const TNgBcnv1Cache *cache, uint64_t epoch, uint64_t sample,
                      double advanceChips, TNgBeidouB1cComponent component,
                      TNgNavigationSnapshot *navigation);

/** Convert nominal BDT to GPST, resolving ordinary week rollover. bdtWeek is an
 * already era-expanded week counted from 2006-01-01. Input TOW must be in [0,604800). */
int NgBdtToGps(unsigned bdtWeek, double bdtTow, unsigned *gpsWeek, double *gpsTow);
#endif
