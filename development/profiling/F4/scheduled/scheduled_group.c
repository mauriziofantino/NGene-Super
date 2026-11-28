#include "scheduled_group.h"

#include <limits.h>
#include <stdlib.h>

struct F4GroupWorker {
    TNgMutex mutex;
    TNgCondition changed;
    TNgThread thread;
    F4GroupWork run;
    void *context;
    int stop, pending, status;
    double cpu;
};

static void Worker(void *context)
{
    F4GroupWorker *worker = context;
    NgThreadSchedulingPriority(-1);
    double charged = 0;
    NgMutexLock(&worker->mutex);
    for (;;) {
        while (!worker->pending && !worker->stop)
            NgConditionWait(&worker->changed, &worker->mutex, UINT_MAX);
        if (worker->stop)
            break;
        NgMutexUnlock(&worker->mutex);
        int status = worker->run(worker->context);
        NgMutexLock(&worker->mutex);
        double used = NgThreadCpuSeconds();
        worker->cpu = used - charged;
        charged = used;
        worker->status = status;
        worker->pending = 0;
        NgConditionSignal(&worker->changed);
    }
    NgMutexUnlock(&worker->mutex);
}

F4GroupWorker *F4GroupWorkerCreate(void)
{
    F4GroupWorker *worker = calloc(1, sizeof(*worker));
    if (!worker)
        return NULL;
    if (NgMutexInit(&worker->mutex) == 0) {
        if (NgConditionInit(&worker->changed) == 0) {
            if (NgThreadStart(&worker->thread, Worker, worker) == 0)
                return worker;
            NgConditionDestroy(&worker->changed);
        }
        NgMutexDestroy(&worker->mutex);
    }
    free(worker);
    return NULL;
}

void F4GroupWorkerDestroy(F4GroupWorker *worker)
{
    if (!worker)
        return;
    NgMutexLock(&worker->mutex);
    worker->stop = 1;
    NgConditionSignal(&worker->changed);
    NgMutexUnlock(&worker->mutex);
    NgThreadJoin(worker->thread);
    NgConditionDestroy(&worker->changed);
    NgMutexDestroy(&worker->mutex);
    free(worker);
}

void F4GroupWorkerStart(F4GroupWorker *worker, F4GroupWork run, void *context)
{
    NgMutexLock(&worker->mutex);
    worker->run = run;
    worker->context = context;
    worker->pending = 1;
    NgConditionSignal(&worker->changed);
    NgMutexUnlock(&worker->mutex);
}

int F4GroupWorkerWait(F4GroupWorker *worker, double *cpu)
{
    NgMutexLock(&worker->mutex);
    while (worker->pending)
        NgConditionWait(&worker->changed, &worker->mutex, UINT_MAX);
    int status = worker->status;
    *cpu = worker->cpu;
    NgMutexUnlock(&worker->mutex);
    return status;
}
