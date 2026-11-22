/** Direct Windows Release benchmark of the native acquisition workspaces. */
#include "prepare_lab.h"
#include "ngene/dsp/gps_ca.h"
#include "platform/platform.h"
#include <inttypes.h>
#include <math.h>
#include <psapi.h>
#include <stdio.h>
#include <string.h>
#ifdef NGENE_PCPS_LAB
#include "pcps_lab.h"
#endif

#define PRN 7u
#define LANES 4u
#define WARMUPS 3u
#define TRIALS 12u

static size_t requestedBytes;

void *LabCalloc(size_t count, size_t size)
{
    requestedBytes += count * size;
    return calloc(count, size);
}

void *LabMalloc(size_t size)
{
    requestedBytes += size;
    return malloc(size);
}

static void Check(int ok, const char *message)
{
    if (!ok) {
        fprintf(stderr, "%s\n", message);
        exit(EXIT_FAILURE);
    }
}

static size_t Resident(void)
{
    PROCESS_MEMORY_COUNTERS counters;
    Check(GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)) != 0,
          "Cannot measure process working set");
    return counters.WorkingSetSize;
}

static int Never(void *context)
{
    (void)context;
    return 0;
}

static uint64_t Hash(uint64_t hash, const void *data, size_t size)
{
    const unsigned char *p = data;
    for (size_t k = 0; k < size; ++k)
        hash = (hash ^ p[k]) * UINT64_C(1099511628211);
    return hash;
}

static double Noise(uint32_t *seed)
{
    *seed = *seed * UINT32_C(1664525) + UINT32_C(1013904223);
    return (double)(*seed >> 8) / 16777216.0 - 0.5;
}

static uint64_t Fixture(TNgComplex *samples, const TNgAcquisitionCapture *capture,
                         int galileo, int scenario)
{
    int8_t chips[4092];
    uint32_t seed = 20261121;
    const unsigned length = galileo ? 4092u : 1023u;
    const double rate = capture->stream.sampleRateHz;
    const int inverted = capture->stream.spectrumInverted;
    const double doppler = 1234;
    FILE *code = NULL;
    if (galileo) {
        code = fopen(NGENE_TEST_SOURCE_DIR "/tests/data/galileo-e1b-prn7.bin", "rb");
        Check(code && fread(chips, 1, length, code) == length, "Cannot read Galileo fixture");
        fclose(code);
    } else {
        Check(NgGpsCaGenerate(PRN, chips, NULL) == NG_OK, "Cannot generate GPS fixture");
    }
    for (size_t k = 0; k < capture->count; ++k) {
        double t = (double)k / rate;
        double phase = fmod(123.42 + t * 1023000.0 * (1 + doppler / 1575420000.0), length);
        double carrier = 6.283185307179586 * ((double)capture->stream.digitalIfHz + doppler) * t +
                         0.71;
        double amplitude = 12 * chips[(unsigned)phase];
        if (galileo && phase - floor(phase) >= 0.5)
            amplitude = -amplitude;
        if (scenario == 2 || (scenario == 3 && t >= (galileo ? 0.096 : 0.024)))
            amplitude = 0;
        double i = (double)lround(amplitude * cos(carrier) + 28 * Noise(&seed) + 2);
        double q = (double)lround(amplitude * sin(carrier) + 28 * Noise(&seed) - 2);
        samples[k].i = (float)(inverted ? 101 * i : i);
        samples[k].q = (float)(inverted ? -101 * q : q);
    }
    return Hash(UINT64_C(14695981039346656037), samples, capture->count * sizeof(*samples));
}

typedef struct {
    const TLabStrategy *ops;
    void *workspace;
    TNgAcquisitionCapture capture;
    TNgAtomicInt ready;
    TNgAtomicInt *start;
    double elapsed, lastPoll, longestGap, deadline, cancelLag, cancelAfter;
    unsigned polls;
    int status;
} TLane;

static int Poll(void *context)
{
    TLane *lane = context;
    double now = NgMonotonicSeconds();
    lane->longestGap = fmax(lane->longestGap, now - lane->lastPoll);
    lane->lastPoll = now;
    ++lane->polls;
    return now >= lane->deadline;
}

static void RunLane(void *context)
{
    TLane *lane = context;
    NgAtomicSet(&lane->ready, 1);
    while (!NgAtomicGet(lane->start))
        SwitchToThread();
    double start = NgMonotonicSeconds();
    lane->status = lane->ops->prepare(lane->workspace, &lane->capture, Never, NULL);
    lane->elapsed = NgMonotonicSeconds() - start;
    if (lane->status != NG_OK)
        return;
    ++lane->capture.origin.epoch;
    lane->longestGap = 0;
    lane->polls = 0;
    lane->deadline = HUGE_VAL;
    lane->lastPoll = NgMonotonicSeconds();
    lane->status = lane->ops->prepare(lane->workspace, &lane->capture, Poll, lane);
    lane->longestGap = fmax(lane->longestGap, NgMonotonicSeconds() - lane->lastPoll);
    if (lane->status != NG_OK)
        return;
    ++lane->capture.origin.epoch;
    lane->lastPoll = NgMonotonicSeconds();
    lane->deadline = lane->lastPoll + lane->cancelAfter;
    lane->status = lane->ops->prepare(lane->workspace, &lane->capture, Poll, lane);
    lane->cancelLag = NgMonotonicSeconds() - lane->deadline;
    if (lane->status == NG_END)
        lane->status = lane->ops->prepare(lane->workspace, &lane->capture, Never, NULL);
    else
        lane->status = NG_ERROR;
}

typedef struct {
    const TLabStrategy *ops;
    void *workspace;
    unsigned seen;
    double noise;
    uint64_t hash;
} TMapTrace;

static int Trace(void *context)
{
    TMapTrace *trace = context;
    if (trace->seen <= trace->ops->rows) {
        size_t size;
        const double *power = trace->ops->power(trace->workspace, &size);
        for (size_t k = 0; k < size; ++k) {
            Check(isfinite(power[k]) && power[k] >= 0, "Nonfinite or negative PCPS power");
            if (trace->seen == 0)
                trace->noise += power[k];
        }
        trace->hash = Hash(trace->hash, power, size * sizeof(*power));
    }
    ++trace->seen;
    return 0;
}

static void Verify(TLane *lane, TNgSearchFftMode mode, int scenario)
{
    TMapTrace trace = {lane->ops, lane->workspace, 0, 0, UINT64_C(14695981039346656037)};
    TNgAcquisitionConfig config;
    TNgAcquisitionResult result;
    TNgError error = {{0}};
    NgAcquisitionDefaults(&config);
    config.fftMode = mode;
    Check(lane->ops->prepare(lane->workspace, &lane->capture, Never, NULL) == NG_OK,
          "Prepare failed in map verification");
    Check(lane->ops->strategy->search(lane->workspace, &lane->capture, PRN, &config,
                                     Trace, &trace, &result, &error) == NG_OK, error.message);
    if (trace.seen == lane->ops->rows)
        (void)Trace(&trace); /* Below threshold: search returns after the last coarse row. */
    Check(trace.seen > lane->ops->rows, "Incomplete coarse maps");
    const TNgAcquisitionSearchSpace *space = lane->ops->space(lane->workspace);
    printf("verify,%d,%016" PRIx64 ",%a,%a,%a,%a,%a,%a,%a,%d,%u,%u\n",
           scenario, trace.hash, trace.noise, space->threshold, space->peakDopplerHz,
           space->peakCodePhaseChips, result.peakRatio, result.dopplerHz, result.codePhaseChips,
           result.confirmed, result.successes, trace.seen);
}

int main(int argc, char **argv)
{
    Check(argc == 8, "Usage: ngene_prepare_lab gps|galileo Fs N IF mode lanes scenario");
    int galileo = strcmp(argv[1], "galileo") == 0;
    uint32_t rate = (uint32_t)strtoul(argv[2], NULL, 10);
    unsigned grid = (unsigned)strtoul(argv[3], NULL, 10);
    int ifHz = atoi(argv[4]);
    TNgSearchFftMode mode = (TNgSearchFftMode)atoi(argv[5]);
    unsigned count = (unsigned)atoi(argv[6]);
    int scenario = atoi(argv[7]);
    const TLabStrategy *ops = galileo ? &labGalileo : &labGps;
    TNgAcquisitionCapture capture = {0};
    TNgError error = {{0}};
    TLane lanes[LANES] = {0};
    Check((galileo || strcmp(argv[1], "gps") == 0) && (count == 1 || count == LANES) &&
          scenario >= 0 &&
#ifdef NGENE_PCPS_LAB
          scenario <= 5 &&
#else
          scenario <= 3 &&
#endif
          mode >= 0 && mode <= NG_SEARCH_FFT_INTEGER_SCALED,
          "Invalid profile arguments");
    capture.stream.sampleRateHz = rate;
    capture.stream.digitalIfHz = ifHz;
    capture.stream.format = ifHz ? NG_FORMAT_CI16 : NG_FORMAT_CI8;
    capture.stream.spectrumInverted = ifHz != 0;
    capture.count = (size_t)(((uint64_t)rate * (galileo ? 224u : 56u) + 999u) / 1000u);
    capture.origin.epoch = 1;
    capture.origin.count = capture.count;
    capture.displayPrn = PRN;
    TNgComplex *samples = calloc(capture.count, sizeof(*samples));
    Check(samples != NULL, "Cannot allocate capture");
    capture.samples = samples;
    uint64_t hash = Fixture(samples, &capture, galileo, scenario);
    printf("fixture,%s,%u,%u,%d,%d,%u,%zu,%016" PRIx64 "\n", argv[1], rate, grid, ifHz, mode,
           count, capture.count, hash);
    size_t residentBefore = Resident();
    double start = NgMonotonicSeconds();
    for (unsigned k = 0; k < count; ++k) {
        lanes[k].ops = ops;
        lanes[k].capture = capture;
        lanes[k].cancelAfter = galileo ? 0.0004 : 0.0001;
        Check(ops->create(&capture.stream, grid, mode, &lanes[k].workspace, &error) == NG_OK,
              error.message);
    }
    double createTime = NgMonotonicSeconds() - start;
    size_t residentCreate = Resident();
#ifdef NGENE_PCPS_LAB
    if (scenario == 4 || scenario == 5) {
        void *workspaces[LANES];
        for (unsigned k = 0; k < count; ++k)
            workspaces[k] = lanes[k].workspace;
        printf("allocated,%zu\n", requestedBytes);
        Check(NgPcpsLab(ops, &capture, workspaces, count, mode, scenario) == NG_OK, "PCPS lab failed");
    } else
#endif
    if (scenario) {
        Verify(&lanes[0], mode, scenario);
    } else {
        for (unsigned k = 0; k < count; ++k) {
            Check(ops->prepare(lanes[k].workspace, &capture, Never, NULL) == NG_OK,
                  "Warm Prepare failed");
            Check(ops->coarse(lanes[k].workspace, PRN, mode) == NG_OK, "Warm PCPS failed");
        }
        printf("memory,%.9f,%zu,%zu,%zu,%zu\n", createTime, requestedBytes, residentBefore,
               residentCreate, Resident());
        for (unsigned trial = 0; trial < WARMUPS + TRIALS; ++trial) {
            TNgThread threads[LANES];
            TNgAtomicInt gate = {0};
            for (unsigned k = 0; k < count; ++k) {
                lanes[k].start = &gate;
                NgAtomicSet(&lanes[k].ready, 0);
                Check(NgThreadStart(&threads[k], RunLane, &lanes[k]) == 0, "Thread start failed");
            }
            for (unsigned k = 0; k < count; ++k)
                while (!NgAtomicGet(&lanes[k].ready))
                    SwitchToThread();
            NgAtomicSet(&gate, 1);
            for (unsigned k = 0; k < count; ++k) {
                NgThreadJoin(threads[k]);
                Check(lanes[k].status == NG_OK, "Prepare cancellation or retry failed");
                if (trial >= WARMUPS)
                    printf("time,%u,%u,%.9f,%.9f,%.9f\n", trial - WARMUPS, k,
                           lanes[k].elapsed, lanes[k].cancelLag, lanes[k].longestGap);
            }
        }
    }
    for (unsigned k = 0; k < count; ++k)
        ops->destroy(lanes[k].workspace);
    free(samples);
    return EXIT_SUCCESS;
}
