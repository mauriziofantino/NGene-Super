/** @file replay.c @brief Isolated CI8 tracking trace from a saved acquisition seed. */
#include "ngene/tracking.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
int main(int argc,char **argv)
{
    TNgTrackingConfig config;TNgStreamInfo stream={0};TNgTracking *tracking=NULL;
    TNgAcquisitionResult seed={0};TNgTrackingSnapshot s;TNgError error={{0}};
    TNgIq8 *samples;TNgIqView view={0};FILE *input;uint64_t first=0,last=0;size_t count,block;
    if(argc!=8&&argc!=9){
        fprintf(stderr,"replay file rate system prn doppler phase start_sample [seed_sample]\n");
        return 2;
    }
    stream.sampleRateHz=(uint32_t)strtoul(argv[2],NULL,10);block=stream.sampleRateHz/1000u;
    NgTrackingDefaults(&config);seed.confirmed=1;seed.prn=(unsigned)strtoul(argv[4],NULL,10);
    seed.dopplerHz=strtod(argv[5],NULL);seed.codePhaseChips=strtod(argv[6],NULL);
    first=strtoull(argv[7],NULL,10);
    seed.firstSample=argc==9?strtoull(argv[8],NULL,10):first;
    if(seed.firstSample>first)return 2;
    if(NgTrackingCreate(&config,&stream,(unsigned)strtoul(argv[3],NULL,10),&tracking,&error)!=NG_OK)return 3;
    if(NgTrackingSeed(tracking,&seed,&error)!=NG_OK)return 4;
    NgTrackingObserve(tracking,0);samples=calloc(block,sizeof(*samples));input=fopen(argv[1],"rb");
    if(!samples||!input||first>LONG_MAX/2||fseek(input,(long)(2*first),SEEK_SET))return 5;
    puts("sample,integrations,doppler,code_rate,dll,carrier_error,prompt,bit_sync,subframes,tow,active,pll,navigation_boundary_sample,end_sample,transmit_time_seconds");
    while((count=fread(samples,sizeof(*samples),block,input))!=0){
        view.samples=samples;view.meta.firstSample=first;view.meta.count=count;
        if(NgTrackingProcess(tracking,&view,&error)!=NG_OK){fprintf(stderr,"%s\n",error.message);return 6;}
        first+=count;NgTrackingRead(tracking,&s);
        if(first-last>=stream.sampleRateHz/10u){last=first;
            printf("%llu,%llu,%.9f,%.9f,%.9f,%.9f,%.9f,%d,%llu,%d,%d,%d,%llu,%llu,%.12f\n",
                (unsigned long long)first,(unsigned long long)s.integrations,s.dopplerHz,s.codeRateHz,
                s.dllError,s.carrierError,hypot(s.i[1],s.q[1]),s.navigation.bitSynchronized,
                (unsigned long long)s.navigation.validSubframes,s.navigation.towValid,s.active,s.pll,
                (unsigned long long)s.navigation.boundarySample,(unsigned long long)s.endSample,
                s.navigation.transmitTimeSeconds);}
    }
    fclose(input);free(samples);NgTrackingDestroy(tracking);return 0;
}
