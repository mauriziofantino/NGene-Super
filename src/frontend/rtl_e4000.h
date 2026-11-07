/** @file rtl_e4000.h @brief E4000 IF allocation using the native discrete stage gains. */
#ifndef NGENE_RTL_E4000_H
#define NGENE_RTL_E4000_H

/* librtlsdr v2.0.2 tuner_e4k.c: aliases of the same gain are omitted.
 * Prefer gain earlier in the cascade for each requested total. This is an
 * allocation policy, not a measurement of the board's noise figure. */
static inline int E4000AllocateStage(unsigned stage, int remaining, int gains[6])
{
    static const int choices[6][5] = {{6, -3},   {9, 6, 3, 0},      {9, 6, 3, 0},
                                      {2, 1, 0}, {15, 12, 9, 6, 3}, {15, 12, 9, 6, 3}};
    static const unsigned counts[6] = {2, 4, 4, 3, 5, 5};
    unsigned k;
    if (stage == 6)
        return remaining == 0;
    for (k = 0; k < counts[stage]; ++k) {
        gains[stage] = choices[stage][k];
        if (E4000AllocateStage(stage + 1, remaining - gains[stage], gains))
            return 1;
    }
    return 0;
}

static inline int E4000Allocate(int totalDb, int gains[6])
{
    return totalDb >= 3 && totalDb <= 56 && E4000AllocateStage(0, totalDb, gains);
}
#endif
