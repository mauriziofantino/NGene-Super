#include "pcps_batch.h"
#include "platform/platform.h"

#include <stdio.h>
#include <string.h>

#define MAX_WORKERS 4

typedef struct {
    void *workspace;
    unsigned index;
    int status;
} BatchLane;

static struct {
    const TLabStrategy *ops;
    unsigned count, prn;
    int square;
    TNgSearchFftMode mode;
    F4BatchFrequency frequency;
    size_t size;
    double *maps;
    size_t bytes;
    double fill_seconds;
    BatchLane lanes[MAX_WORKERS];
} batch;

void F4BatchConfigure(const TLabStrategy *ops, void *const *workspaces, unsigned count)
{
    const char *mode = getenv("NGENE_F4_PCPS_BATCH");
    batch.ops = ops;
    batch.count = mode && strcmp(mode, "direct") == 0 ? 0 : count;
    if (!batch.count)
        return;
    (void)ops->power(workspaces[0], &batch.size);
    batch.bytes = (ops->rows + 1) * batch.size * sizeof(*batch.maps);
    batch.maps = malloc(batch.bytes);
    if (!batch.maps)
        return;
    for (unsigned k = 0; k < batch.count; ++k) {
        batch.lanes[k].workspace = workspaces[k];
        batch.lanes[k].index = k;
    }
}

int F4BatchPrepare(const TNgAcquisitionCapture *capture, TNgAcquisitionCancel cancel, void *context)
{
    if (batch.count && !batch.maps)
        return NG_ERROR;
    for (unsigned k = 1; k < batch.count; ++k)
        if (batch.ops->prepare(batch.lanes[k].workspace, capture, cancel, context) != NG_OK)
            return NG_ERROR;
    return NG_OK;
}

int F4BatchEnabled(void)
{
    return batch.count != 0;
}

static void ProcessRows(void *context)
{
    BatchLane *lane = context;
    lane->status = NG_ERROR;
    for (unsigned bin = lane->index; bin < batch.ops->rows; bin += batch.count) {
        if (batch.ops->row(lane->workspace, batch.prn, batch.frequency(bin),
                           batch.square, batch.mode) != NG_OK)
            return;
        size_t size;
        const double *row = batch.ops->power(lane->workspace, &size);
        if (size != batch.size)
            return;
        memcpy(batch.maps + bin * size, row, size * sizeof(*row));
    }
    lane->status = NG_OK;
}

int F4BatchFill(F4BatchFrequency frequency, unsigned prn, int square,
                TNgSearchFftMode mode, double *power, size_t size)
{
    if (!batch.maps || size != batch.size)
        return NG_ERROR;
    const double start = NgMonotonicSeconds();
    batch.frequency = frequency;
    batch.prn = prn;
    batch.square = square;
    batch.mode = mode;
    double *noise = batch.maps + batch.ops->rows * size;
    memcpy(noise, power, size * sizeof(*power));
    TNgThread threads[MAX_WORKERS - 1];
    unsigned started = 0;
    for (unsigned k = 1; k < batch.count; ++k) {
        if (NgThreadStart(&threads[started], ProcessRows, &batch.lanes[k]) != 0)
            break;
        ++started;
    }
    int ok = started + 1 == batch.count;
    if (ok)
        ProcessRows(&batch.lanes[0]);
    for (unsigned k = 0; k < started; ++k)
        NgThreadJoin(threads[k]);
    memcpy(power, noise, size * sizeof(*power));
    for (unsigned k = 0; ok && k < batch.count; ++k)
        ok = batch.lanes[k].status == NG_OK;
    batch.fill_seconds = NgMonotonicSeconds() - start;
    return ok ? NG_OK : NG_ERROR;
}

int F4BatchRead(unsigned bin, double *power, size_t size)
{
    if (bin >= batch.ops->rows || size != batch.size)
        return NG_ERROR;
    memcpy(power, batch.maps + bin * size, size * sizeof(*power));
    return NG_OK;
}

void F4BatchReport(double prepare, double search, double cpu)
{
    printf("batch,%u,%.9f,%.9f,%.9f,%.9f,%zu\n", batch.count,
           prepare, search, cpu, batch.fill_seconds, batch.bytes);
}

void F4BatchFree(void)
{
    free(batch.maps);
}

