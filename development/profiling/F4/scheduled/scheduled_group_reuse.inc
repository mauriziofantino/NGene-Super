/* Included in the build-local scheduler after the operative cancellation hook. */
int F4GroupReuseEnabled(TNgAcquisitionCancel cancel, void *context)
{
    if (cancel != WorkCancelled)
        return 0;
    const TNgAcquisitionJob *job = context;
    return job->pairMode == 2;
}

int F4GroupSubmitReuse(void *context, F4ReuseRequest *request, F4ReuseRow run, void *parameter)
{
    TNgAcquisitionJob *job = context;
    TNgAcquisition *a = job->acquisition;
    TNgAcquisitionPool *pool = a->pool;
    NgMutexLock(&pool->mutex);
    unsigned limit = (unsigned)NgAtomicGet(&pool->executionLimit);
    if (pool->stopping || !job->permit || Cancelled(job) || pool->executing >= limit ||
        pool->active + pool->reuseActive >= pool->started ||
        (pool->budgetActive && pool->cpuCredit <= 0)) {
        NgMutexUnlock(&pool->mutex);
        return 0;
    }

    NgMutexLock(&a->mutex);
    unsigned lane;
    for (lane = 0; lane < a->contextCount; ++lane)
        if (!(a->busyContexts & (UINT32_C(1) << lane)))
            break;
    if (lane == a->contextCount) {
        NgMutexUnlock(&a->mutex);
        NgMutexUnlock(&pool->mutex);
        return 0;
    }
    a->busyContexts |= UINT32_C(1) << lane;
    NgMutexUnlock(&a->mutex);

    *request = (F4ReuseRequest){.workspace = a->contexts[lane], .context = parameter,
        .run = run, .lane = lane};
    if (pool->reuseTail)
        pool->reuseTail->next = request;
    else
        pool->reuseHead = request;
    pool->reuseTail = request;
    ++pool->reuseActive;
    ++pool->executing;
    ++job->pairStarted;
    NgConditionSignal(&pool->work);
    NgMutexUnlock(&pool->mutex);
    return 1;
}

/* Called with pool mutex held. Dispatch before admitting a new PRN, because
 * the queued row already owns both a worker slot and a stage permit. */
static int F4RunQueuedReuse(TNgAcquisitionPool *pool)
{
    F4ReuseRequest *request = pool->reuseHead;
    if (!request)
        return 0;
    pool->reuseHead = request->next;
    if (!pool->reuseHead)
        pool->reuseTail = NULL;
    NgMutexUnlock(&pool->mutex);

    double start = NgThreadCpuSeconds();
    int status = request->run(request->workspace, request->context);
    double cpu = NgThreadCpuSeconds() - start;

    NgMutexLock(&pool->mutex);
    if (pool->budgetActive)
        pool->cpuCredit -= cpu;
    --pool->reuseActive;
    --pool->executing;
    request->status = status;
    request->cpu = cpu;
    request->done = 1;
    NgConditionSignal(&pool->reuseDone);
    NgConditionSignal(&pool->work);
    return 1;
}

int F4GroupWaitReuse(void *context, F4ReuseRequest *request)
{
    const TNgAcquisitionJob *job = context;
    TNgAcquisitionPool *pool = job->acquisition->pool;
    NgMutexLock(&pool->mutex);
    while (!request->done)
        NgConditionWait(&pool->reuseDone, &pool->mutex, UINT_MAX);
    int status = request->status;
    NgMutexUnlock(&pool->mutex);
    return status;
}

void F4GroupReturnReuse(void *context, F4ReuseRequest *request, int completed)
{
    TNgAcquisitionJob *job = context;
    TNgAcquisition *a = job->acquisition;
    TNgAcquisitionPool *pool = a->pool;
    NgMutexLock(&pool->mutex);
    NgMutexLock(&a->mutex);
    a->busyContexts &= ~(UINT32_C(1) << request->lane);
    NgMutexUnlock(&a->mutex);
    NgConditionSignal(&pool->work);
    NgMutexUnlock(&pool->mutex);
    job->pairCpu += request->cpu;
    job->pairCompleted += (unsigned)(completed != 0);
}

