/** Windows lab. Scenario 4 profiles one lane, scenario 5 times uninstrumented searches. */
#include "pcps_lab.h"
#include "platform/platform.h"
#include <stdio.h>
#include <string.h>

#define PRN 7u
#define KERNEL_TRIALS 12u
#define CANCEL_TRIALS 24u

static int NeverCancel(void *context)
{
    (void)context;
    return 0;
}

#ifdef NGENE_PCPS_PROFILE
#include "profile_generated.h"
static double partSeconds[PCPS_PART_COUNT];

void NgPcpsProfilePart(TPcpsPart part, double *mark)
{
    double now = NgMonotonicSeconds();
    partSeconds[part] += now - *mark;
    *mark = now;
}
#else
typedef struct {
    const TLabStrategy *ops;
    TNgAcquisitionCapture capture;
    void *workspace;
    TNgAcquisitionConfig config;
    TNgAtomicInt ready;
    TNgAtomicInt *gate;
    double row[KERNEL_TRIALS];
    double lag[CANCEL_TRIALS];
    double deadline;
    double searchTime;
    int status;
} TPcpsLane;

static int CancelDuringCoarse(void *context)
{
    TPcpsLane *lane = context;
    double now = NgMonotonicSeconds();
    if (lane->deadline == 0)
        lane->deadline = now + 0.000010; /* First poll is after wrong-Doppler, before row 1. */
    return now >= lane->deadline;
}

static void Measure(void *context)
{
    TPcpsLane *lane = context;
    TNgAcquisitionResult result;
    TNgError error = {{0}};
    const TLabStrategy *ops = lane->ops;
    NgAtomicSet(&lane->ready, 1);
    while (!NgAtomicGet(lane->gate))
        SwitchToThread();
    for (unsigned k = 0; k < KERNEL_TRIALS; ++k) {
        double start = NgMonotonicSeconds();
        if (ops->coarse(lane->workspace, PRN, lane->config.fftMode) != NG_OK)
            return;
        lane->row[k] = NgMonotonicSeconds() - start;
    }
    for (unsigned k = 0; k < CANCEL_TRIALS; ++k) {
        lane->deadline = 0;
        if (ops->strategy->search(lane->workspace, &lane->capture, PRN, &lane->config,
                                  CancelDuringCoarse, lane, &result, &error) != NG_END)
            return;
        lane->lag[k] = NgMonotonicSeconds() - lane->deadline;
    }
    ++lane->capture.origin.epoch;
    double start = NgMonotonicSeconds();
    if (ops->strategy->search(lane->workspace, &lane->capture, PRN, &lane->config, NeverCancel,
                              NULL, &result, &error) != NG_OK ||
        !result.confirmed || result.successes != 3)
        return;
    lane->searchTime = NgMonotonicSeconds() - start;
    lane->status = NG_OK;
}
#endif

int NgPcpsLab(const TLabStrategy *ops, const TNgAcquisitionCapture *capture,
              void *const *workspaces, unsigned count, TNgSearchFftMode mode, int scenario)
{
#ifdef NGENE_PCPS_PROFILE
    if (scenario != 4 || count != 1)
        return NG_ERROR;
    double prepare = 0, rows = 0;
    for (unsigned k = 0; k < KERNEL_TRIALS + 3; ++k) {
        if (k == 3)
            memset(partSeconds, 0, sizeof(partSeconds));
        double start = NgMonotonicSeconds();
        if (ops->prepare(workspaces[0], capture, NeverCancel, NULL) != NG_OK)
            return NG_ERROR;
        if (k >= 3)
            prepare += NgMonotonicSeconds() - start;
        start = NgMonotonicSeconds();
        if (ops->coarse(workspaces[0], PRN, mode) != NG_OK)
            return NG_ERROR;
        if (k >= 3)
            rows += NgMonotonicSeconds() - start;
    }
    printf("profile,%.9f,%.9f", prepare / KERNEL_TRIALS, rows / KERNEL_TRIALS);
    for (unsigned k = 0; k < PCPS_PART_COUNT; ++k)
        printf(",%.9f", partSeconds[k] / KERNEL_TRIALS);
    putchar('\n');
    return NG_OK;
#else
    if (scenario != 5)
        return NG_ERROR;
    TPcpsLane lanes[4] = {0};
    TNgThread threads[4];
    TNgAtomicInt gate = {0};
    unsigned started = 0;
    int status = NG_OK;
    for (unsigned k = 0; k < count; ++k) {
        TPcpsLane *lane = &lanes[k];
        lane->status = NG_ERROR;
        lane->ops = ops;
        lane->workspace = workspaces[k];
        lane->capture = *capture;
        lane->capture.displayPrn = 0;
        lane->gate = &gate;
        NgAcquisitionDefaults(&lane->config);
        lane->config.fftMode = mode;
        if (ops->prepare(lane->workspace, &lane->capture, NeverCancel, NULL) != NG_OK ||
            ops->coarse(lane->workspace, PRN, mode) != NG_OK ||
            NgThreadStart(&threads[k], Measure, lane) != 0) {
            status = NG_ERROR;
            break;
        }
        ++started;
    }
    for (unsigned k = 0; k < started; ++k)
        while (!NgAtomicGet(&lanes[k].ready))
            SwitchToThread();
    NgAtomicSet(&gate, 1);
    for (unsigned k = 0; k < started; ++k) {
        NgThreadJoin(threads[k]);
        if (lanes[k].status != NG_OK) {
            status = NG_ERROR;
            continue;
        }
        for (unsigned j = 0; j < KERNEL_TRIALS; ++j)
            printf("row,%u,%u,%.9f\n", k, j, lanes[k].row[j]);
        for (unsigned j = 0; j < CANCEL_TRIALS; ++j)
            printf("cancel,%u,%u,%.9f\n", k, j, lanes[k].lag[j]);
        printf("search,%u,%.9f\n", k, lanes[k].searchTime);
    }
    return status;
#endif
}
