/** @file egnos.h @brief Receiver-owned GEO acquisition, tracking and SBAS corrections. */
#ifndef NG_TRACKING_EGNOS_H
#define NG_TRACKING_EGNOS_H
#include "ngene/tracking.h"
#include "ngene/sbas.h"

typedef struct TNgEgnos TNgEgnos;

/** Prepare one optional GEO tracker and one private search worker. */
int NgEgnosCreate(const TNgStreamInfo *stream, TNgEgnos **out, TNgError *error);
/** Process one unique common input interval, before computing PVT. */
int NgEgnosProcess(TNgEgnos *egnos, const TNgIqView *view, int searchAllowed, TNgError *error);
/** Free the tracker after joining the private search worker. */
void NgEgnosDestroy(TNgEgnos *egnos);
/** Borrow the receiver-thread correction bank for use during the solver call. */
const TNgSbasCorrections *NgEgnosCorrections(const TNgEgnos *egnos);
/** Return current GEO PRN, or zero while searching. */
unsigned NgEgnosPrn(const TNgEgnos *egnos);

/** Start a private C/A tracker and route its 1-ms prompts to the GEO decoder. */
int NgTrackingSeedEgnos(TNgTracking *tracking, const TNgAcquisitionResult *result,
                        void (*prompt)(void *context, double value, uint64_t sample),
                        void *context, TNgError *error);
#endif
