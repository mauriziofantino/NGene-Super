static int PrepareDirect(void *workspace, const TNgAcquisitionCapture *capture,
                         TNgAcquisitionCancel cancel, void *context)
{
    return Prepare(workspace, capture, cancel, context);
}

static int NoiseMap(void *workspace, unsigned prn, TNgSearchFftMode mode)
{
    return Coarse(workspace, prn, AcquisitionCoarseNoiseDoppler(), 0, mode);
}

static const double *Power(void *workspace, size_t *n)
{
    LAB_NATIVE *search = workspace;
    *n = search->size;
    return search->power;
}

static const TNgAcquisitionSearchSpace *Space(void *workspace)
{
    LAB_NATIVE *search = workspace;
    return &search->space;
}

const TLabStrategy LAB_TABLE = {
    Create, LAB_FREE, PrepareDirect, NoiseMap, &LAB_STRATEGY, Power, Space, LAB_ROWS
};
