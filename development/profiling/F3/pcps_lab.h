#ifndef NGENE_PCPS_LAB_H
#define NGENE_PCPS_LAB_H
#include "prepare_lab.h"

typedef enum {
    PCPS_CARRIER, PCPS_ROTATE, PCPS_FORWARD, PCPS_PRODUCT, PCPS_PACK,
    PCPS_INVERSE, PCPS_POWER, PCPS_PART_COUNT
} TPcpsPart;

void NgPcpsProfilePart(TPcpsPart part, double *mark);
int NgPcpsLab(const TLabStrategy *ops, const TNgAcquisitionCapture *capture,
              void *const *workspaces, unsigned count, TNgSearchFftMode mode, int scenario);
#endif
