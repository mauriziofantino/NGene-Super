/* Deterministic frontend: no hardware access. Exercise the real calibration sweep. */
#include "ngene/calibration.h"
#include "ngene/dsp/statistics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static int active, opens, reads;
static unsigned fixtureVga;
static TNgIq8 fixtureSamples[32768];
static int CheckedCenterBlock(const TNgIqView *view, int32_t *output, int dc[2], TNgError *error)
{
    CHECK(!active); /* Statistical work must happen only after RX stops. */
    return NgIqCenterBlock(view, output, dc, error);
}
#define NgIqCenterBlock CheckedCenterBlock
#include "../src/frontend/calibration.c"
#undef NgIqCenterBlock
static TNgAtomicInt captureReady, cancelRequested;
static int slowCapture;
static int Start(void *ctx, TNgError *error) { (void)ctx; (void)error; CHECK(!active); active=1; reads=0; return NG_OK; }
static int Acquire(void *ctx, TNgIqView *view, unsigned timeout, TNgError *error)
{
    uint32_t state=1729;
    size_t k;
    (void)ctx; (void)timeout; (void)error;
    CHECK(active);
    for(k=0;k<32768;k++) {
        int sum[2]={0}; unsigned j,c;
        for(j=0;j<12;j++) for(c=0;c<2;c++) { state=state*1664525u+1013904223u; sum[c]+=(int)(state>>28); }
        fixtureSamples[k].i=(int8_t)(fixtureVga && reads>=4 ? (k%2?127:-128) : sum[0]-90);
        fixtureSamples[k].q=(int8_t)(fixtureVga && reads>=4 ? (k%2?-128:127) : sum[1]-90);
    }
    memset(view,0,sizeof(*view)); view->samples=fixtureSamples; view->format=NG_FORMAT_CI8; view->meta.count=32768;
    ++reads;
    NgAtomicSet(&captureReady, reads);
    if (slowCapture) NgSleepMs(5);
    return NG_OK;
}
static void Release(void *ctx) { (void)ctx; }
static int Stop(void *ctx, TNgError *error) { (void)ctx; (void)error; CHECK(active); active=0; return NG_OK; }
static const TNgFrontendOps ops={Start,Acquire,Release,Stop,NULL,NULL};
int NgFrontendAvailable(TNgFrontendKind kind) { (void)kind; return 1; }
int NgFrontendCapabilities(const TNgConfig *config,TNgFrontendCaps *caps,TNgError *error)
{ (void)config; (void)error; memset(caps,0,sizeof(*caps)); return NG_OK; }
int NgFrontendOpenNative(const TNgConfig *config,TNgFrontend *frontend,TNgError *error)
{
    (void)error; CHECK(!active); CHECK(config->vgaGainDb<=2); ++opens; fixtureVga=config->vgaGainDb;
    frontend->ops=&ops; frontend->info.format=NG_FORMAT_CI8; frontend->info.sampleRateHz=32768; return NG_OK;
}
void NgFrontendDestroy(TNgFrontend *frontend) { (void)frontend; CHECK(!active); }
typedef struct { TNgConfig config; TNgGainCalibration result; TNgError error; int rc; } CancelFixture;
static int IsCancelled(void *context) { (void)context; return NgAtomicGet(&cancelRequested); }
static void CancelWorker(void *context)
{
    CancelFixture *fixture = context;
    TNgGainCalibrationCallbacks callbacks = {IsCancelled, NULL, NULL};
    fixture->rc = NgGainCalibrateControlled(&fixture->config, &fixture->result, &callbacks, &fixture->error);
}
int main(void)
{
    TNgConfig config={0}; TNgGainCalibration result; TNgError error={{0}};
    config.frontend=NG_FRONTEND_HACKRF;
    CHECK(NgGainCalibrate(&config,&result,&error)==NG_OK);
    CHECK(result.accepted && result.selected.vgaGainDb==0);
    CHECK(opens==24 && result.tested==72); /* One clean + one intermittently clipped gain per branch. */
    CHECK(result.rejectedOverload == 12);
    {
        CancelFixture fixture = {0};
        TNgThread worker;
        unsigned waited;
        fixture.config = config;
        slowCapture = 1;
        NgAtomicSet(&captureReady, 0); NgAtomicSet(&cancelRequested, 0);
        CHECK(NgThreadStart(&worker, CancelWorker, &fixture) == 0);
        for (waited = 0; waited < 2000 && !NgAtomicGet(&captureReady); ++waited) NgSleepMs(1);
        CHECK(NgAtomicGet(&captureReady));
        NgAtomicSet(&cancelRequested, 1);
        NgThreadJoin(worker);
        CHECK(fixture.rc == NG_END && !active && opens == 25 && !fixture.result.tested);
    }
    puts("PASS: RX stops before SIMD DC/analysis; clipped third window rejected; overload stops branches; concurrent cancellation releases the SDR.");
    return 0;
}
