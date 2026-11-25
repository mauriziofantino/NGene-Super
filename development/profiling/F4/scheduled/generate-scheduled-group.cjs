/* Build-local scheduler experiment. Anchor checks fail if an operative contract moves. */
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const output = process.argv[2];
assert(output);
fs.mkdirSync(output, {recursive: true});

const read = file => fs.readFileSync(file, 'utf8').replace(/\r\n/g, '\n');
const write = (file, text) => fs.writeFileSync(path.join(output, file), text);
function replace(text, from, to) {
  assert.equal(text.split(from).length, 2, from);
  return text.replace(from, () => to);
}

for (const signal of ['gps', 'galileo']) {
  const gps = signal === 'gps';
  const rows = gps ? 16 : 116;
  const type = gps ? 'TNgGpsSearch' : 'TNgGalileoSearch';
  const prefix = gps ? 'NgAcquisitionGps' : 'NgAcquisitionGalileo';
  let text = read('src/acquisition/' + signal + '.c')
    .replace(/^#include "(noise_policy|galileo_codes)\.h"$/gm, '#include "acquisition/$1.h"');
  text = replace(text, '} ' + type + ';', '    void *pairWorkspace;\n    F4GroupWorker *pairWorker;\n    double *pairPower;\n} ' + type + ';');
  text = replace(text, '    NgSearchFftDestroy(search->fft);',
    '    F4GroupWorkerDestroy(search->pairWorker);\n    ' + prefix +
    'Free(search->pairWorkspace);\n    free(search->pairPower);\n    NgSearchFftDestroy(search->fft);');
  const coarse = text.indexOf('static int Coarse(');
  const coarseEnd = text.indexOf('/** Mix ', coarse);
  assert(coarse > 0 && coarseEnd > coarse);
  const originalCoarse = text.slice(coarse, coarseEnd);
  const borrowed = originalCoarse.replace('static int Coarse(' + type + ' *search,',
    'static int F4CoarseFrom(' + type + ' *search, const ' + type + ' *samples,')
    .replace(/search->(baseband|integerBaseband|integerScale)\b/g, 'samples->$1');
  assert(borrowed.startsWith('static int F4CoarseFrom('));
  text = text.slice(0, coarse) + borrowed +
    'static int Coarse(' + type + ' *search, unsigned prn, double doppler, int square,\n' +
    '                  TNgSearchFftMode mode)\n{\n' +
    '    return F4CoarseFrom(search, search, prn, doppler, square, mode);\n}\n\n' +
    text.slice(coarseEnd);
  const search = text.indexOf('static int Search(');
  assert(search > 0);
  const original = text.slice(search);
  const frequency = /double frequency = ([^;]+);/.exec(original);
  assert(frequency && frequency[1].includes('bin'));
  const row = '        if (Coarse(search, prn, frequency, config->squareCarrier, config->fftMode) != NG_OK)';
  const loop = `    for (bin = 0; bin <= ${rows - 1}; ++bin) {`;
  const body = replace(replace(replace(original, loop, `    unsigned ready = ${rows};\n` + loop),
    frequency[0], 'double frequency = F4CoarseFrequency(bin);'), row,
    '        int coarse = F4CoarseRow(search, capture, config, cancel, cancelContext, prn, bin, &ready);\n' +
    '        if (coarse == NG_END)\n            return NG_END;\n        if (coarse != NG_OK)');
  const create = prefix + 'Workspace(&capture->stream, ' + (gps ? '' : '(unsigned)search->size, ') +
    'config->fftMode, &search->pairWorkspace, NULL)';
  write(signal + '.c', '#include "scheduled_group.h"\n' + text.slice(0, search) +
    'static double F4CoarseFrequency(unsigned bin)\n{\n    return ' + frequency[1] + ';\n}\n' +
    '#define F4_SEARCH ' + type + '\n#define F4_ROWS ' + rows + 'u\n#define F4_CREATE ' + create +
    '\n#include "scheduled_group_strategy.inc"\n\n' + body);
}

let pool = read('src/acquisition/acquisition.c');
pool = replace(pool, '    double cpuMark;', '    int pairMode;\n    unsigned pairStarted, pairCompleted;\n    double pairCpu;\n    double cpuMark;');
pool = replace(pool, '        job->cpuMark = NgThreadCpuSeconds();',
  '        const char *pair = getenv("NGENE_F4_PCPS_GROUP");\n' +
  '        job->pairMode = pair && strcmp(pair, "reuse") == 0 ? 2 : pair && strcmp(pair, "pair") == 0;\n' +
  '        job->pairStarted = job->pairCompleted = 0;\n        job->pairCpu = 0;\n' +
  '        job->cpuMark = NgThreadCpuSeconds();');
const hooks = `/* Spare permits only. Existing active jobs keep first claim on stage concurrency. */
int F4GroupBorrow(TNgAcquisitionCancel cancel, void *context)
{
    if (cancel != WorkCancelled)
        return 0;
    TNgAcquisitionJob *job = context;
    if (job->pairMode != 1 || !job->permit || Cancelled(job))
        return 0;
    TNgAcquisitionPool *pool = job->acquisition->pool;
    NgMutexLock(&pool->mutex);
    unsigned limit = (unsigned)NgAtomicGet(&pool->executionLimit);
    int borrowed = !pool->stopping && !Cancelled(job) &&
        pool->executing < limit && pool->active < limit &&
        (!pool->budgetActive || pool->cpuCredit > 0);
    if (borrowed) {
        ++pool->executing;
        ++job->pairStarted;
    }
    NgMutexUnlock(&pool->mutex);
    return borrowed;
}

int F4GroupCancelled(void *context)
{
    return Cancelled(context);
}

void F4GroupRelease(void *context, double cpu, int completed)
{
    TNgAcquisitionJob *job = context;
    TNgAcquisitionPool *pool = job->acquisition->pool;
    NgMutexLock(&pool->mutex);
    if (pool->budgetActive)
        pool->cpuCredit -= cpu;
    --pool->executing;
    NgConditionSignal(&pool->work);
    NgMutexUnlock(&pool->mutex);
    job->pairCpu += cpu;
    job->pairCompleted += (unsigned)(completed != 0);
}

`;
pool = replace(pool, '    unsigned executing;', '    TNgCondition reuseDone;\n' +
  '    F4ReuseRequest *reuseHead, *reuseTail;\n    unsigned reuseActive;\n    unsigned executing;');
pool = replace(pool, '/** Update the compatibility PRN',
  hooks + '#include "scheduled_group_reuse.inc"\n\n/** Update the compatibility PRN');
pool = replace(pool, '    while (!pool->stopping) {',
  '    while (!pool->stopping) {\n        if (F4RunQueuedReuse(pool))\n            continue;');
pool = replace(pool, '        prn = a->nextPrn++;', '        prn = a->nextPrn;');
pool = replace(pool, '        a->busyContexts |= UINT32_C(1) << lane;',
  '        if (lane == a->contextCount) {\n            NgMutexUnlock(&a->mutex);\n' +
  '            continue;\n        }\n        ++a->nextPrn;\n        a->busyContexts |= UINT32_C(1) << lane;');
pool = replace(pool, '    pool->subscriptionCount = count;',
  '    if (NgConditionInit(&pool->reuseDone)) {\n        NgConditionDestroy(&pool->idle);\n' +
  '        NgConditionDestroy(&pool->work);\n        NgMutexDestroy(&pool->mutex);\n' +
  '        free(pool);\n        NgFail(error, "Cannot initialize F4 row completion condition");\n' +
  '        return NULL;\n    }\n    pool->subscriptionCount = count;');
pool = replace(pool, '    NgConditionDestroy(&pool->idle);\n    NgConditionDestroy(&pool->work);',
  '    NgConditionDestroy(&pool->reuseDone);\n    NgConditionDestroy(&pool->idle);\n' +
  '    NgConditionDestroy(&pool->work);');
pool = replace(pool, '    if (status == NG_OK && native && result.confirmed',
  '    fprintf(stderr, "f4_pair,%s,%" PRIu64 ",%" PRIu64 ",%u,%u,%u,%.9f\\n",\n' +
  '            a->strategy == &ngGalileoAcquisitionStrategy ? "galileo" : "gps",\n' +
  '            job->capture.origin.epoch, job->capture.origin.firstSample, job->prn,\n' +
  '            job->pairStarted, job->pairCompleted, job->pairCpu);\n' +
  '    if (status == NG_OK && native && result.confirmed');
pool = replace(pool, '    NgAtomicSet(&a->stop, 1);', '    double stopStart = NgMonotonicSeconds();\n    NgAtomicSet(&a->stop, 1);');
const drain = '        while (a->activeJobs)\n            NgConditionWait(&pool->idle, &pool->mutex, UINT_MAX);';
pool = replace(pool, drain, '        unsigned pending = a->activeJobs;\n' + drain +
  '\n        fprintf(stderr, "f4_cancel,%s,%u,%.9f\\n",\n' +
  '                a->strategy == &ngGalileoAcquisitionStrategy ? "galileo" : "gps",\n' +
  '                pending, NgMonotonicSeconds() - stopStart);');
write('acquisition.c', '#include "scheduled_group.h"\n' + pool);

let dispatch = read('src/core/channel_dispatch.c');
const measure = `static unsigned trackHist[20001], trackCount, trackOverMs;
static double trackMax;
static uint64_t trackJobs, trackWorkHash = UINT64_C(14695981039346656037);

static void TrackWorkValue(uint64_t value)
{
    trackWorkHash = (trackWorkHash ^ value) * UINT64_C(1099511628211);
}

static void TrackWork(const TNgChannelDispatch *dispatch, const TNgIqView *window, size_t jobs)
{
    trackJobs += jobs;
    TrackWorkValue(window->meta.epoch);
    TrackWorkValue(window->meta.firstSample);
    TrackWorkValue(window->meta.count);
    TrackWorkValue(jobs);
    for (size_t k = 0; k < dispatch->channelCount; ++k) {
        const TNgChannel *channel = &dispatch->channels[k];
        if (channel->state != NG_CHANNEL_ACQUISITION && !Skipped(channel)) {
            TrackWorkValue(channel->id.instanceId);
            TrackWorkValue(channel->id.constellation);
            TrackWorkValue(channel->id.satellite);
            TrackWorkValue((uint64_t)channel->state);
        }
    }
}

static void TrackTime(double seconds)
{
    double usec = seconds * 1e6;
    unsigned bucket = usec > 20000 ? 20000u : (unsigned)usec;
    ++trackHist[bucket];
    ++trackCount;
    trackOverMs += usec > 1000;
    if (usec > trackMax)
        trackMax = usec;
}

void F4GroupTrackingReport(void)
{
    unsigned rank = (trackCount * 95u + 99u) / 100u, total = 0, k = 0;
    while (k < 20000 && (total += trackHist[k]) < rank)
        ++k;
    fprintf(stderr, "f4_tracking,%u,%u,%u,%.3f\\n", trackCount, trackOverMs, k, trackMax);
    fprintf(stderr, "f4_work,%" PRIu64 ",%016" PRIx64 "\\n", trackJobs, trackWorkHash);
}

`;
dispatch = replace(dispatch, '    /* Finish all tracking/holdover work',
  '    /* Finish all tracking/holdover work');
const boundary = '        if (!jobs)\n            continue;';
dispatch = replace(dispatch, boundary, boundary + '\n        double trackStart = phase == 0 ? NgMonotonicSeconds() : 0;');
const end = '    }\n    for (k = 0; k < dispatch->channelCount; ++k) {\n        TNgChannelResult *channelResult';
dispatch = replace(dispatch, end,
  '        if (phase == 0) {\n            TrackTime(NgMonotonicSeconds() - trackStart);\n' +
  '            TrackWork(dispatch, window, jobs);\n        }\n' + end);
const call = dispatch.indexOf('int NgChannelDispatchProcess(');
assert(call > 0);
dispatch = dispatch.slice(0, call) + measure + dispatch.slice(call);
write('channel_dispatch.c', '#include "scheduled_group.h"\n#include <inttypes.h>\n#include <stdio.h>\n' + dispatch);

let main = read('apps/main.c');
main = replace(main, '    NgTrackingBankDestroy(trackingBanks[0]);\n    NgTrackingBankDestroy(trackingBanks[1]);',
  '    F4GroupTrackingReport();\n    NgTrackingBankDestroy(trackingBanks[0]);\n    NgTrackingBankDestroy(trackingBanks[1]);');
write('main.c', '#include "scheduled_group.h"\n' + main);


let bank = read('src/tracking/bank.c');
bank = replace(bank, '    int haveWindow;', '    int f4FixedTracking;\n    int haveWindow;');
bank = replace(bank, 'static int Frame(TNgChannelPool *pool,', '#include "fixed_tracking.inc"\n\n' +
  'static int Frame(TNgChannelPool *pool,');
const activate = '    for (k = 0; k < pool->config.channelCount; ++k)\n' +
  '        if (pool->slots[k].active)\n            ++active;\n' +
  '    for (k = 0; k < pool->config.channelCount; ++k)\n        NgTrackingObserve';
bank = replace(bank, activate,
  '    if (!pool->haveWindow) {\n        const char *fixed = getenv("NGENE_F4_FIXED_TRACKING");\n' +
  '        pool->f4FixedTracking = fixed && strcmp(fixed, "1") == 0;\n' +
  '        if (pool->f4FixedTracking && F4SeedTracking(pool, window, error) != NG_OK)\n' +
  '            return NG_ERROR;\n    }\n' + activate);
bank = replace(bank, '    for (;;) {\n        const TNgAcquisitionResult *best',
  '    while (!pool->f4FixedTracking) {\n        const TNgAcquisitionResult *best');
write('bank.c', bank);
