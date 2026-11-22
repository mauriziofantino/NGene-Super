#include "prepare_lab.h"
#define calloc LabCalloc
#include "acquisition/galileo.c"
#undef calloc
#define LAB_NATIVE TNgGalileoSearch
#define LAB_FREE NgAcquisitionGalileoFree
#define LAB_STRATEGY ngGalileoAcquisitionStrategy
#define LAB_TABLE labGalileo
#define LAB_ROWS 116
static int Create(const TNgStreamInfo *stream, unsigned n, TNgSearchFftMode mode, void **out,
                  TNgError *error)
{
#ifdef NGENE_ACQUISITION_WORKSPACE_H
    return NgAcquisitionGalileoWorkspace(stream, n, mode, out, error);
#else
    (void)mode;
    return NgAcquisitionGalileoWorkspace(stream, n, out, error);
#endif
}
#include "strategy_probe.inc"
