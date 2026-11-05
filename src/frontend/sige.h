#ifndef NGENE_SIGE_H
#define NGENE_SIGE_H
#include "ngene/frontend.h"
int NgSigeOpen(const TNgConfig *config, TNgFrontend *frontend, TNgError *error);
int NgSigeList(FILE *output, TNgError *error);

/** Decode marker-tagged native IQ. A transfer may start/end on either component.
 * Unexpected markers after synchronization fail instead of fabricating IQ pairs. */
typedef struct {
    int pending, synchronized, gap;
    unsigned char inPhase;
} TNgSigeDecoder;

int NgSigeDecode(TNgSigeDecoder *state, const unsigned char *raw, size_t bytes, TNgIq8 *output,
                 size_t capacity, size_t *pairs);
#endif
