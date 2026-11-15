/** @file test_acquisition_pool.c @brief Parallel native search parity, ownership and admission. */
#include "ngene/acquisition.h"
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)

/** Run the same frozen RF once serially and once at the available native concurrency. */
static TNgAcquisitionSnapshot Run(unsigned limit)
{
    TNgAcquisitionConfig config; TNgAcquisition *a=NULL;
    TNgStreamInfo stream={0}; TNgChannel channel; TNgChannelState next;
    TNgAcquisitionSnapshot snapshot; TNgError error={{0}}; TNgIqView view={0};
    size_t count=2048u*56u,k; uint32_t random=567; unsigned tries,maximum=0;
    TNgIq8 *samples=calloc(count,sizeof(*samples)); CHECK(samples);
    for(k=0;k<count;k++){random=random*1664525u+1013904223u;samples[k].i=(int8_t)(random>>24);random=random*1664525u+1013904223u;samples[k].q=(int8_t)(random>>24);}
    NgAcquisitionDefaults(&config); config.firstPrn=1;config.lastPrn=8; stream.sampleRateHz=2048000;
    CHECK(NgAcquisitionCreate(&config,&stream,&a,&channel,&error)==NG_OK);
    NgAcquisitionRead(a,&snapshot);CHECK(snapshot.parallelCapacity>=4);
    NgAcquisitionSetParallelism(a,0,8);
    view.samples=samples;view.meta.count=count;view.meta.epoch=9;view.meta.firstSample=500000;
    CHECK(channel.ops->process(channel.context,&channel.id,channel.state,&view,&next,&error)==NG_OK);
    NgSleepMs(10);NgAcquisitionRead(a,&snapshot);CHECK(!snapshot.activeSearches&&!snapshot.searches);
    NgAcquisitionSetParallelism(a,limit,8);
    for(tries=0;tries<10000;tries++) {
        NgAcquisitionRead(a,&snapshot);if(snapshot.activeSearches>maximum)maximum=snapshot.activeSearches;
        CHECK(snapshot.poolActive<=snapshot.parallelCapacity);
        if(snapshot.searches==8&&!snapshot.poolActive)break;
        NgSleepMs(1);
    }
    CHECK(snapshot.searches==8&&tries<10000);
    if(limit==1)CHECK(maximum==1);else CHECK(maximum>=4);
    printf("native acquisition limit=%u observed=%u capacity=%u searches=%llu\n",limit,maximum,snapshot.parallelCapacity,(unsigned long long)snapshot.searches);
    NgAcquisitionDestroy(a);free(samples);return snapshot;
}

/** Every per-satellite decision must remain identical under concurrent PRN scheduling. */
int main(void)
{
    TNgAcquisitionSnapshot serial=Run(1),parallel=Run(NG_ACQUISITION_MAX_PARALLEL);
    unsigned k;
    for(k=0;k<8;k++) {
        const TNgAcquisitionResult *a=&serial.results[k],*b=&parallel.results[k];
        CHECK(a->prn==b->prn&&a->epoch==b->epoch&&a->firstSample==b->firstSample);
        CHECK(a->confirmed==b->confirmed&&a->successes==b->successes);
        CHECK(a->dopplerHz==b->dopplerHz&&a->codePhaseChips==b->codePhaseChips&&a->peakRatio==b->peakRatio);
    }
    puts("native parallel pool preserves exact single-satellite results and pauses admission");return 0;
}
