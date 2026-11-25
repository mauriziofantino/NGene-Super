typedef struct {
    F4_SEARCH *workspace;
    const TNgAcquisitionCapture *capture;
    unsigned prn;
    double doppler;
    const TNgAcquisitionConfig *config;
    void *cancelContext;
} F4RowWork;

static int F4CheckOnly(void *context)
{
    return F4GroupCancelled(context);
}

static int F4ComputeSecond(void *context)
{
    F4RowWork *job = context;
    F4_SEARCH *search = job->workspace;
    if (F4CheckOnly(job->cancelContext))
        return NG_END;
    if (!search->prepared || search->preparedFirst != job->capture->origin.firstSample ||
        search->preparedEpoch != job->capture->origin.epoch) {
        int status = Prepare(search, job->capture, F4CheckOnly, job->cancelContext);
        if (status != NG_OK)
            return status;
    }
    if (F4CheckOnly(job->cancelContext))
        return NG_END;
    return Coarse(search, job->prn, job->doppler, job->config->squareCarrier,
                  job->config->fftMode);
}

static int F4ComputeReused(void *workspace, void *context)
{
    const F4RowWork *job = context;
    if (F4CheckOnly(job->cancelContext))
        return NG_END;
    return F4CoarseFrom(workspace, job->workspace, job->prn, job->doppler,
                        job->config->squareCarrier, job->config->fftMode);
}

static int F4ReuseCoarseRow(F4_SEARCH *search, const TNgAcquisitionCapture *capture,
                            const TNgAcquisitionConfig *config, void *cancelContext,
                            unsigned prn, unsigned bin, unsigned *ready)
{
    F4RowWork input = {search, capture, prn, F4CoarseFrequency(bin + 1), config, cancelContext};
    F4ReuseRequest request;
    const double doppler = F4CoarseFrequency(bin);
    if (!search->pairPower)
        search->pairPower = malloc(search->size * sizeof(*search->pairPower));
    if (!search->pairPower || !F4GroupSubmitReuse(cancelContext, &request, F4ComputeReused, &input))
        return Coarse(search, prn, doppler, config->squareCarrier, config->fftMode);

    int first = Coarse(search, prn, doppler, config->squareCarrier, config->fftMode);
    int second = F4GroupWaitReuse(cancelContext, &request);
    int cancelled = F4CheckOnly(cancelContext);
    int completed = !cancelled && first == NG_OK && second == NG_OK;
    if (completed) {
        const F4_SEARCH *helper = request.workspace;
        memcpy(search->pairPower, helper->power, search->size * sizeof(*search->pairPower));
        *ready = bin + 1;
    }
    F4GroupReturnReuse(cancelContext, &request, completed);
    if (cancelled || second == NG_END)
        return NG_END;
    return completed ? NG_OK : NG_ERROR;
}

static int F4CoarseRow(F4_SEARCH *search, const TNgAcquisitionCapture *capture,
                        const TNgAcquisitionConfig *config, TNgAcquisitionCancel cancel,
                        void *cancelContext, unsigned prn, unsigned bin, unsigned *ready)
{
    const int reuse = F4GroupReuseEnabled(cancel, cancelContext);
    if (*ready == bin) {
        const double *power = reuse ? search->pairPower : ((F4_SEARCH *)search->pairWorkspace)->power;
        memcpy(search->power, power,
               search->size * sizeof(*search->power));
        *ready = F4_ROWS;
        return NG_OK;
    }
    const double doppler = F4CoarseFrequency(bin);
    if (reuse && bin + 1 < F4_ROWS)
        return F4ReuseCoarseRow(search, capture, config, cancelContext, prn, bin, ready);
    if (bin + 1 >= F4_ROWS || !F4GroupBorrow(cancel, cancelContext))
        return Coarse(search, prn, doppler, config->squareCarrier, config->fftMode);

    if (!search->pairWorkspace && F4_CREATE != NG_OK)
        search->pairWorkspace = NULL;
    if (search->pairWorkspace && !search->pairWorker)
        search->pairWorker = F4GroupWorkerCreate();
    if (!search->pairWorker) {
        F4GroupRelease(cancelContext, 0, 0);
        return Coarse(search, prn, doppler, config->squareCarrier, config->fftMode);
    }

    F4RowWork helper = {search->pairWorkspace, capture, prn, F4CoarseFrequency(bin + 1),
                        config, cancelContext};
    F4GroupWorkerStart(search->pairWorker, F4ComputeSecond, &helper);
    int first = Coarse(search, prn, doppler, config->squareCarrier, config->fftMode);
    double cpu;
    int second = F4GroupWorkerWait(search->pairWorker, &cpu);
    int cancelled = F4CheckOnly(cancelContext);
    F4GroupRelease(cancelContext, cpu, !cancelled && first == NG_OK && second == NG_OK);
    if (cancelled || second == NG_END)
        return NG_END;
    if (first != NG_OK || second != NG_OK)
        return NG_ERROR;
    *ready = bin + 1;
    return NG_OK;
}

