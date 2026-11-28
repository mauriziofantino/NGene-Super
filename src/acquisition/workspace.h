/** @file workspace.h @brief Private native acquisition strategies and worker workspaces. */
#ifndef NGENE_ACQUISITION_WORKSPACE_H
#define NGENE_ACQUISITION_WORKSPACE_H
#include "ngene/acquisition.h"

int NgAcquisitionGpsWorkspace(const TNgStreamInfo *stream, TNgSearchFftMode mode, void **out,
                              TNgError *error);
int NgAcquisitionGalileoWorkspace(const TNgStreamInfo *stream, unsigned fftSize,
                                  TNgSearchFftMode mode, void **out, TNgError *error);
void NgAcquisitionGpsFree(void *context);
void NgAcquisitionGalileoFree(void *context);
extern const TNgAcquisitionStrategy ngGpsAcquisitionStrategy;
/** Build private PRN 121/136 spectra for the unchanged PCPS search. */
int NgAcquisitionEgnosWorkspace(const TNgStreamInfo *stream, void **out, TNgError *error);
extern const TNgAcquisitionStrategy ngGalileoAcquisitionStrategy;
int NgAcquisitionBeidouWorkspace(const TNgStreamInfo *stream, void **out, TNgError *error);
extern const TNgAcquisitionStrategy ngBeidouAcquisitionStrategy;
#endif
