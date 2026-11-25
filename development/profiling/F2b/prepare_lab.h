#ifndef NGENE_PREPARE_LAB_H
#define NGENE_PREPARE_LAB_H
#include "ngene/acquisition.h"
#include <stdlib.h>

typedef struct {
    int (*create)(const TNgStreamInfo *, unsigned, TNgSearchFftMode, void **, TNgError *);
    void (*destroy)(void *);
    int (*prepare)(void *, const TNgAcquisitionCapture *, TNgAcquisitionCancel, void *);
    int (*coarse)(void *, unsigned, TNgSearchFftMode);
    const TNgAcquisitionStrategy *strategy;
    const double *(*power)(void *, size_t *);
    const TNgAcquisitionSearchSpace *(*space)(void *);
    unsigned rows;
    int (*row)(void *, unsigned, double, int, TNgSearchFftMode);
} TLabStrategy;

extern const TLabStrategy labGps, labGalileo;
void *LabCalloc(size_t count, size_t size);
void *LabMalloc(size_t size);
#endif
