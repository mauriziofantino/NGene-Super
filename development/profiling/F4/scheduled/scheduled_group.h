#ifndef NGENE_F4_SCHEDULED_GROUP_H
#define NGENE_F4_SCHEDULED_GROUP_H

#include "ngene/acquisition.h"
#include "platform/platform.h"

typedef struct F4GroupWorker F4GroupWorker;
typedef int (*F4GroupWork)(void *context);

int F4GroupBorrow(TNgAcquisitionCancel cancel, void *context);
int F4GroupCancelled(void *context);
void F4GroupRelease(void *context, double cpu, int completed);
F4GroupWorker *F4GroupWorkerCreate(void);
void F4GroupWorkerDestroy(F4GroupWorker *worker);
void F4GroupWorkerStart(F4GroupWorker *worker, F4GroupWork run, void *context);
int F4GroupWorkerWait(F4GroupWorker *worker, double *cpu);
void F4GroupTrackingReport(void);

/* Each request lives on the owning worker's stack until its wait and return. */
typedef int (*F4ReuseRow)(void *workspace, void *context);

typedef struct F4ReuseRequest {
    struct F4ReuseRequest *next;
    void *workspace, *context;
    F4ReuseRow run;
    unsigned lane;
    int done, status;
    double cpu;
} F4ReuseRequest;

int F4GroupReuseEnabled(TNgAcquisitionCancel cancel, void *context);
int F4GroupSubmitReuse(void *context, F4ReuseRequest *request, F4ReuseRow run, void *parameter);
int F4GroupWaitReuse(void *context, F4ReuseRequest *request);
void F4GroupReturnReuse(void *context, F4ReuseRequest *request, int completed);

#endif
