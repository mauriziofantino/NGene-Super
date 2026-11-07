/* Deterministic frontend: no hardware access. Exercise the real calibration sweep. */
#include "ngene/calibration.h"
#include "ngene/dsp/statistics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x);                                        \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
static int active, opens, reads, reuses;
static unsigned fixtureVga, fixtureLna;
static int fixtureAmp, selectionSignal;
static TNgIq8 fixtureSamples[32768];
static TNgIq8 noiseSamples[32768];
static TNgGainPolicy fixturePolicy = NG_GAIN_POLICY_HACKRF_MAX2837;
static int uniformSignal;
static double e4000Scale;

static int CheckedCenterBlock(const TNgIqView *view, int32_t *output, int dc[2], TNgError *error)
{
    CHECK(!active); /* No queue publication while analyzing copied windows. */
    return NgIqCenterBlock(view, output, dc, error);
}

#define NgIqCenterBlock CheckedCenterBlock
#include "../src/frontend/calibration.c"
#undef NgIqCenterBlock
static TNgAtomicInt captureReady, cancelRequested;
static int slowCapture;

static int Start(void *ctx, TNgError *error)
{
    (void)ctx;
    (void)error;
    CHECK(!active);
    active = 1;
    reads = 0;
    return NG_OK;
}

static int Acquire(void *ctx, TNgIqView *view, unsigned timeout, TNgError *error)
{
    size_t k;
    (void)ctx;
    (void)timeout;
    (void)error;
    CHECK(active);
    memcpy(fixtureSamples, noiseSamples, sizeof(fixtureSamples));
    if (fixturePolicy == NG_GAIN_POLICY_RTL_E4000)
        for (k = 0; k < 32768; k++) {
            fixtureSamples[k].i = (int8_t)round(noiseSamples[k].i * e4000Scale);
            fixtureSamples[k].q = (int8_t)round(noiseSamples[k].q * e4000Scale);
        }
    if (selectionSignal) {
        int preferred = fixtureLna == 32 && fixtureVga == 8;
        if (fixtureAmp || (!preferred && !(fixtureLna == 8 && fixtureVga == 32)))
            memset(fixtureSamples, 0, sizeof(fixtureSamples));
        else if (preferred)
            for (k = 0; k < 32768; k++) {
                fixtureSamples[k].i = (int8_t)(fixtureSamples[k].i + 3);
                fixtureSamples[k].q = (int8_t)(fixtureSamples[k].q + 3);
            }
    }
    if (!uniformSignal && fixtureVga && reads >= 4)
        for (k = 0; k < 32768; k++) {
            fixtureSamples[k].i = (int8_t)(k % 2 ? 127 : -128);
            fixtureSamples[k].q = (int8_t)(k % 2 ? -128 : 127);
        }
    memset(view, 0, sizeof(*view));
    view->samples = fixtureSamples;
    view->format = NG_FORMAT_CI8;
    view->meta.count = 32768;
    ++reads;
    NgAtomicSet(&captureReady, reads);
    if (slowCapture)
        NgSleepMs(5);
    return NG_OK;
}

static void Release(void *ctx)
{
    (void)ctx;
}

static int Stop(void *ctx, TNgError *error)
{
    (void)ctx;
    (void)error;
    CHECK(active);
    active = 0;
    return NG_OK;
}

static const TNgFrontendOps ops = {Start, Acquire, Release, Stop, NULL, NULL};

int NgFrontendAvailable(TNgFrontendKind kind)
{
    (void)kind;
    return 1;
}

int NgFrontendCapabilities(const TNgConfig *config, TNgFrontendCaps *caps, TNgError *error)
{
    (void)config;
    (void)error;
    memset(caps, 0, sizeof(*caps));
    caps->gainPolicy = fixturePolicy;
    strcpy(caps->deviceSerial, "fixture-device");
    strcpy(caps->deviceModel, "fixture");
    caps->tunerGainCount = 4;
    caps->tunerGains[0] = 0;
    caps->tunerGains[1] = 77;
    caps->tunerGains[2] = 77;
    caps->tunerGains[3] = 144;
    caps->gainMinDb = -12;
    caps->gainMaxDb = -8;
    caps->gainStepDb = 2;
    if (fixturePolicy == NG_GAIN_POLICY_RTL_E4000) {
        static const int e4000Gains[] = {-10, 15,  40,  65,  90,  115, 140,
                                         165, 190, 215, 240, 290, 340, 420};
        caps->tunerGainCount = sizeof(e4000Gains) / sizeof(e4000Gains[0]);
        memcpy(caps->tunerGains, e4000Gains, sizeof(e4000Gains));
    }
    return NG_OK;
}

static void SetFixtureGains(const TNgConfig *config)
{
    CHECK(!active);
    fixtureVga = config->vgaGainDb;
    fixtureLna = config->lnaGainDb;
    fixtureAmp = config->rfAmp;
    CHECK(strcmp(config->serial, "fixture-device") == 0);
    if (config->frontend == NG_FRONTEND_RTLSDR)
        CHECK(!config->rtlAutoGain);
    if (fixturePolicy == NG_GAIN_POLICY_RTL_E4000) {
        CHECK(config->rtlIfGainDb >= 3 && config->rtlIfGainDb <= 56);
        /* At maximum RF and the driver-default 24 dB IF, only ~0.4% ADC RMS.
         * The formerly omitted IF gain must bring this input into range. */
        e4000Scale =
            0.032 * pow(10.0, (config->rtlGainTenthsDb / 10.0 + config->rtlIfGainDb - 66.0) / 20.0);
    }
    if (config->frontend == NG_FRONTEND_BLADERF)
        CHECK(config->bladeGainDb >= -12 && config->bladeGainDb <= -8 &&
              config->bladeGainDb % 2 == 0);
}

int NgFrontendOpenNative(const TNgConfig *config, TNgFrontend *frontend, TNgError *error)
{
    (void)error;
    ++opens;
    SetFixtureGains(config);
    frontend->ops = &ops;
    frontend->info.format = NG_FORMAT_CI8;
    frontend->info.sampleRateHz = 32768;
    return NG_OK;
}

int NgFrontendCalibrationNext(const TNgConfig *config, TNgFrontend *frontend, TNgError *error)
{
    (void)error;
    CHECK(frontend->ops == &ops && config->frontend == NG_FRONTEND_RTLSDR);
    ++reuses;
    SetFixtureGains(config);
    return NG_OK;
}

int NgFrontendCalibrationPause(TNgFrontend *frontend, TNgError *error)
{
    CHECK(frontend->ops == &ops);
    return Stop(NULL, error);
}

void NgFrontendDestroy(TNgFrontend *frontend)
{
    (void)frontend;
    CHECK(!active);
}

typedef struct {
    TNgConfig config;
    TNgGainCalibration result;
    TNgError error;
    int rc;
} CancelFixture;

static int IsCancelled(void *context)
{
    (void)context;
    return NgAtomicGet(&cancelRequested);
}

static void CancelWorker(void *context)
{
    CancelFixture *fixture = context;
    TNgGainCalibrationCallbacks callbacks = {IsCancelled, NULL, NULL};
    fixture->rc =
        NgGainCalibrateControlled(&fixture->config, &fixture->result, &callbacks, &fixture->error);
}

/** @brief Equal-gain early-stage priority must outrank ADC score and remain order independent. */
static void SplitSelection(void)
{
    int reverse;
    for (reverse = 0; reverse < 2; ++reverse) {
        TNgGainSweep sweep = {0};
        TNgGainCalibration result = {0};
        TNgConfig candidate = {0};
        TNgGainQuality quality = {0};
        unsigned k;
        result.capabilities.gainPolicy = NG_GAIN_POLICY_HACKRF_MAX2837;
        quality.acceptable = 1;
        for (k = 0; k < 2; ++k) {
            int early = ((k != 0) != (reverse != 0));
            candidate.lnaGainDb = early ? 32u : 8u;
            candidate.vgaGainDb = early ? 8u : 32u;
            quality.score = early ? 0.8 : 0.1;
            Consider(&candidate, &quality, &result, &sweep);
        }
        CHECK(result.selected.lnaGainDb == 32 && result.selected.vgaGainDb == 8);
        /* A genuinely better different-total group supersedes the representative. */
        candidate.lnaGainDb = 40;
        candidate.vgaGainDb = 2;
        quality.score = 0.5;
        Consider(&candidate, &quality, &result, &sweep);
        CHECK(result.selected.lnaGainDb == 40 && result.selected.vgaGainDb == 2);
        /* A rejected split cannot displace an accepted split, even at higher LNA. */
        candidate.vgaGainDb = 0;
        quality.acceptable = 0;
        quality.score = 0;
        quality.rejectionReasons = NG_GAIN_OVERLOAD;
        Consider(&candidate, &quality, &result, &sweep);
        CHECK(result.selected.vgaGainDb == 2 && result.accepted);
    }
    {
        TNgGainSweep sweep = {0};
        TNgGainCalibration result = {0};
        TNgConfig candidate = {0};
        TNgGainQuality quality = {0};
        result.capabilities.gainPolicy = NG_GAIN_POLICY_HACKRF_MAX2837;
        quality.acceptable = 1;
        candidate.vgaGainDb = 40;
        quality.score = 0.1;
        Consider(&candidate, &quality, &result, &sweep);
        candidate.vgaGainDb = 42;
        quality.score = 0.5;
        Consider(&candidate, &quality, &result, &sweep);
        candidate.lnaGainDb = 40;
        candidate.vgaGainDb = 0;
        quality.score = 0.8;
        Consider(&candidate, &quality, &result, &sweep);
        CHECK(result.selected.lnaGainDb == 40 && result.selected.vgaGainDb == 0);
    }
    {
        TNgGainSweep sweep = {0};
        TNgGainCalibration result = {0};
        TNgConfig candidate = {0};
        TNgGainQuality quality = {0};
        result.capabilities.gainPolicy = NG_GAIN_POLICY_HACKRF_MAX2839;
        quality.acceptable = 1;
        quality.score = 0.55;
        candidate.rfAmp = 1;
        candidate.lnaGainDb = 0;
        candidate.vgaGainDb = 62;
        Consider(&candidate, &quality, &result, &sweep);
        candidate.lnaGainDb = 40;
        candidate.vgaGainDb = 42;
        quality.score = 0.62;
        Consider(&candidate, &quality, &result, &sweep);
        CHECK(result.selected.lnaGainDb == 40 && result.selected.vgaGainDb == 42);
        quality.acceptable = 0;
        quality.rejectionReasons = NG_GAIN_OVERLOAD;
        candidate.vgaGainDb = 62;
        quality.score = 0;
        Consider(&candidate, &quality, &result, &sweep);
        CHECK(result.selected.vgaGainDb == 42 && result.accepted);
    }
    {
        TNgGainSweep sweep = {0};
        TNgGainCalibration result = {0};
        TNgConfig candidate = {0};
        TNgGainQuality quality = {0};
        result.capabilities.gainPolicy = NG_GAIN_POLICY_RTL_E4000;
        quality.acceptable = 1;
        quality.score = 0.1;
        candidate.rtlGainTenthsDb = 340;
        candidate.rtlIfGainDb = 32;
        Consider(&candidate, &quality, &result, &sweep);
        quality.score = 0.8;
        candidate.rtlGainTenthsDb = 420;
        candidate.rtlIfGainDb = 24;
        Consider(&candidate, &quality, &result, &sweep);
        CHECK(result.selected.rtlGainTenthsDb == 420 && result.selected.rtlIfGainDb == 24);
    }
}

/** @brief Check firmware compensation and alias boundaries with independent known states. */
static void PhysicalGains(void)
{
    static const unsigned cases[][4] = {{0, 0, 0, 0},     {0, 4, 0, 0},     {8, 62, 16, 49},
                                        {16, 0, 0, 11},   {16, 30, 16, 25}, {24, 62, 24, 57},
                                        {32, 62, 32, 57}, {40, 62, 32, 63}};
    size_t k;
    int early, late;
    for (k = 0; k < sizeof(cases) / sizeof(cases[0]); ++k) {
        HackrfPhysicalGains(NG_GAIN_POLICY_HACKRF_MAX2839, cases[k][0], cases[k][1], &early, &late);
        CHECK(early == (int)cases[k][2] && late == (int)cases[k][3]);
    }
    for (k = 0; k <= 40; k += 8) {
        HackrfPhysicalGains(NG_GAIN_POLICY_HACKRF_MAX2831, (unsigned)k, 20, &early, &late);
        CHECK(early == (k < 16 ? 0 : k < 32 ? 17 : 33) && late == 20);
    }
}

int main(void)
{
    TNgConfig config = {0};
    TNgGainCalibration result;
    TNgError error = {{0}};
    uint32_t state = 1729;
    size_t k;
    int before;
    for (k = 0; k < 32768; k++) {
        int sum[2] = {0};
        unsigned j, c;
        for (j = 0; j < 12; j++)
            for (c = 0; c < 2; c++) {
                state = state * 1664525u + 1013904223u;
                sum[c] += (int)(state >> 28);
            }
        noiseSamples[k].i = (int8_t)(sum[0] - 90);
        noiseSamples[k].q = (int8_t)(sum[1] - 90);
    }
    SplitSelection();
    PhysicalGains();
    config.frontend = NG_FRONTEND_HACKRF;
    CHECK(NgGainCalibrate(&config, &result, &error) == NG_OK);
    CHECK(result.accepted && result.selected.vgaGainDb == 0);
    CHECK(opens == 384 &&
          result.tested == 1152); /* An overload must not prune higher VGA settings. */
    CHECK(result.rejectedOverload == 372);
    uniformSignal = 1;
    selectionSignal = 1;
    CHECK(NgGainCalibrate(&config, &result, &error) == NG_OK && result.accepted);
    CHECK(result.selected.lnaGainDb == 32 && result.selected.vgaGainDb == 8 &&
          !result.selected.rfAmp);
    selectionSignal = 0;
    fixturePolicy = NG_GAIN_POLICY_HACKRF_MAX2831;
    before = opens;
    CHECK(NgGainCalibrate(&config, &result, &error) == NG_OK && result.accepted);
    CHECK(opens - before == 192 && result.tested == 576);
    fixturePolicy = NG_GAIN_POLICY_HACKRF_MAX2839;
    before = opens;
    CHECK(NgGainCalibrate(&config, &result, &error) == NG_OK && result.accepted);
    CHECK(opens - before == 252 && result.tested == 756);
    config.frontend = NG_FRONTEND_RTLSDR;
    config.rtlAutoGain = 1;
    fixturePolicy = NG_GAIN_POLICY_RTL_R82XX;
    before = opens;
    CHECK(NgGainCalibrate(&config, &result, &error) == NG_OK && !result.selected.rtlAutoGain);
    CHECK(opens - before == 1 && result.tested == 9 && reuses == 3);
    fixturePolicy = NG_GAIN_POLICY_RTL_E4000;
    before = opens;
    CHECK(NgGainCalibrate(&config, &result, &error) == NG_OK && result.accepted);
    CHECK(opens - before == 1 && result.tested == 3 * 14 * 54 && reuses == 3 + 14 * 54);
    CHECK(result.selected.rtlGainTenthsDb == 420 && result.selected.rtlIfGainDb > 24);
    CHECK(result.quality.rmsFraction >= 0.06 && result.quality.rmsFraction <= 0.32);
    config.frontend = NG_FRONTEND_BLADERF;
    fixturePolicy = NG_GAIN_POLICY_BLADERF2;
    before = opens;
    CHECK(NgGainCalibrate(&config, &result, &error) == NG_OK && result.tested == 9);
    CHECK(opens - before == 3);
    fixturePolicy = NG_GAIN_POLICY_UNAVAILABLE;
    before = opens;
    CHECK(NgGainCalibrate(&config, &result, &error) == NG_ERROR && opens == before &&
          !result.tested);
    config.frontend = NG_FRONTEND_HACKRF;
    fixturePolicy = NG_GAIN_POLICY_HACKRF_MAX2837;
    for (unsigned cancelMode = 0; cancelMode < 2; ++cancelMode) {
        CancelFixture fixture = {0};
        TNgThread worker;
        unsigned waited;
        double requested;
        fixturePolicy = cancelMode ? NG_GAIN_POLICY_RTL_E4000 : NG_GAIN_POLICY_HACKRF_MAX2837;
        config.frontend = cancelMode ? NG_FRONTEND_RTLSDR : NG_FRONTEND_HACKRF;
        fixture.config = config;
        before = opens;
        slowCapture = 1;
        NgAtomicSet(&captureReady, 0);
        NgAtomicSet(&cancelRequested, 0);
        CHECK(NgThreadStart(&worker, CancelWorker, &fixture) == 0);
        for (waited = 0; waited < 2000 && !NgAtomicGet(&captureReady); ++waited)
            NgSleepMs(1);
        CHECK(NgAtomicGet(&captureReady));
        requested = NgMonotonicSeconds();
        NgAtomicSet(&cancelRequested, 1);
        NgThreadJoin(worker);
        CHECK(NgMonotonicSeconds() - requested < 1.0);
        CHECK(fixture.rc == NG_END && !active && opens == before + 1 && !fixture.result.tested);
    }
    puts("PASS: device policies, complete distinct-state sweeps, preferred equal-gain splits, "
         "third-window rejection, manual RTL/native bladeRF, device pinning and cancellation.");
    return 0;
}
