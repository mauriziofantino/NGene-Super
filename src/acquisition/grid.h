/** @file grid.h @brief Compatibility of a complete code period with the coarse FFT grid. */
#ifndef NGENE_ACQUISITION_GRID_H
#define NGENE_ACQUISITION_GRID_H
#include "ngene/types.h"

/** Validate FFT support and at least one native complex pair in every compression cell. */
int NgAcquisitionValidateGrid(const char *signal, uint32_t rateHz, unsigned periodMs,
                              size_t size, TNgError *error);

#endif
