#include "prepare_lab.h"
#define calloc LabCalloc
#include "acquisition/gps.c"
#undef calloc
#define LAB_NATIVE TNgGpsSearch
#define LAB_FREE NgAcquisitionGpsFree
#define LAB_STRATEGY ngGpsAcquisitionStrategy
#define LAB_TABLE labGps
#define LAB_ROWS 16

static int Create(const TNgStreamInfo *stream, unsigned n, TNgSearchFftMode mode, void **out,
                  TNgError *error)
{
    (void)n;
#ifdef NGENE_ACQUISITION_WORKSPACE_H
    return NgAcquisitionGpsWorkspace(stream, mode, out, error);
#else
    (void)mode;
    return NgAcquisitionGpsWorkspace(stream, out, error);
#endif
}

#include "strategy_probe.inc"
