#ifndef NGENE_F4_PCPS_BATCH_H
#define NGENE_F4_PCPS_BATCH_H

#include "prepare_lab.h"

typedef double (*F4BatchFrequency)(unsigned bin);

void F4BatchConfigure(const TLabStrategy *ops, void *const *workspaces, unsigned count);
int F4BatchPrepare(const TNgAcquisitionCapture *capture, TNgAcquisitionCancel cancel,
                   void *context);
int F4BatchEnabled(void);
int F4BatchFill(F4BatchFrequency frequency, unsigned prn, int square, TNgSearchFftMode mode,
                double *power, size_t size);
int F4BatchRead(unsigned bin, double *power, size_t size);
void F4BatchReport(double prepare, double search, double cpu);
void F4BatchFree(void);

#endif
