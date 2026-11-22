/** @file search_plan.h @brief Mode-specific search FFT for native acquisition workers. */
#ifndef NGENE_DSP_SEARCH_PLAN_H
#define NGENE_DSP_SEARCH_PLAN_H
#include "ngene/dsp/search.h"

/** Create a plan to be used only with the selected arithmetic. */
int NgSearchFftCreateForMode(size_t size, TNgSearchFftMode mode, TNgSearchFft **out,
                             TNgError *error);
#endif
