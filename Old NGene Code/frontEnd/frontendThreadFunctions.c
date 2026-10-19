/**
 * @file frontendThreadFunctions.c
 *
 * @author Micaela Troglia Gamba
 *
 * @date 09/12/2014
 *
 * @version 1.0.1
 *
 * @brief Function performed in a separate thread that manages USB Front End
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <sched.h>
#include <string.h>
#include <unistd.h>
#include <sys/msg.h>
#include <errno.h>
#include "frontendThreadFunctions.h"
#include "usbInterface.h"
#include "usbDevicesConstants.h"
#include "channelManager.h"
#include "ngeneConfiguration.h"
#include "ioFunctions.h"


extern int stopFrontEndFlag;
extern int stopReceiverFlag;


/**
 * @def DEFAULT_INDEX_CPU
 * @brief Index of the CPU that will host the thread managing the access to the USB
 */
#define DEFAULT_INDEX_CPU  2


#ifndef MONITOR_INTERMEDIATE_BUFFER
void InitUsbFrontEndThreadFunction(TRxFe *ngeneFe, TFrontEndThreadFunctionParameters *frontEndThreadFunctionParameters)
#else
void InitUsbFrontEndThreadFunction(TRxFe *ngeneFe, TFrontEndThreadFunctionParameters *frontEndThreadFunctionParameters, TNGeneConfiguration *ngeneCfg)
#endif
{
    stopFrontEndFlag = FALSE_FLAG;
    stopReceiverFlag = FALSE_FLAG;
    pthread_mutex_init(&(ngeneFe->usbFe.frontEndMutex), NULL);
    pthread_cond_init(&(ngeneFe->usbFe.intermediateBufferFull), NULL);
    pthread_cond_init(&(ngeneFe->usbFe.intermediateBufferEmpty), NULL);
    pthread_mutex_init(&(ngeneFe->usbFe.threadsMutexStopReceiver), NULL);
    pthread_mutex_init(&(ngeneFe->usbFe.threadsMutexStopFrontEnd), NULL);
    ngeneFe->usbFe.flagBufferFull = FALSE_FLAG;
    /* Initialize USB front-end */
    InitializeRxFrontEnd(frontEndThreadFunctionParameters->ngeneConfiguration, frontEndThreadFunctionParameters->ngeneFrontEnd);
    StartRxFrontEnd(frontEndThreadFunctionParameters->ngeneConfiguration, frontEndThreadFunctionParameters->ngeneFrontEnd);
#ifdef MONITOR_INTERMEDIATE_BUFFER
    char filename[MAX_ROW_LENGTH];
    filename[0] = '\0';
    strcat(filename, ngeneCfg->dumpPath);
    strcat(filename, "/percentageIntermediateBufferFull.bin");
    ngeneFe->usbFe.percentageIntBufFullFile = fopen(filename, "wb");
    if (ngeneFe->usbFe.percentageIntBufFullFile == NULL) {
	SwrxLog(NGENE_LOG_SEVERITY_NOTE, "The file percentageIntermediateBufferFullFile.bin has not been correctly created.");

    }
#endif
}

void DeinitUsbFrontEndThreadFunction(TRxFe *ngeneFe, TFrontEndThreadFunctionParameters *frontEndThreadFunctionParameters)
{
    pthread_cond_destroy(&(ngeneFe->usbFe.intermediateBufferFull));
    pthread_cond_destroy(&(ngeneFe->usbFe.intermediateBufferEmpty));
    pthread_mutex_destroy(&(ngeneFe->usbFe.frontEndMutex));
    pthread_mutex_destroy(&(ngeneFe->usbFe.threadsMutexStopFrontEnd));
    pthread_mutex_destroy(&(ngeneFe->usbFe.threadsMutexStopReceiver));
    /* Deinit USB front end */
    StopRxFrontEnd(frontEndThreadFunctionParameters->ngeneFrontEnd);
    DeinitializeRxFrontEnd(frontEndThreadFunctionParameters->ngeneConfiguration, frontEndThreadFunctionParameters->ngeneFrontEnd);
#ifdef MONITOR_INTERMEDIATE_BUFFER
    fclose(ngeneFe->usbFe.percentageIntBufFullFile);
#endif
}

void *UsbFrontEndThreadFunction(TFrontEndThreadFunctionParameters *frontEndThreadFunctionParameters)
{
    TUsbFe *usbFe = &(frontEndThreadFunctionParameters->ngeneFrontEnd->usbFe);
    int stopThread = FALSE_FLAG;
    int stopFrontEndFlagLocal = FALSE_FLAG;
    int maxPriority;
    int indexCpu;
    int numCpus = DEFAULT_INDEX_CPU + 1;
    struct timespec delay;

    /* used to sleep when CopySamplesIntoUsbIntermediateBuffer terminates
     * (i.e. all the USB completed USB transfers have been processed */
    delay.tv_sec = 0;
    delay.tv_nsec = USB_WAIT_NS;

    /* Adjust the CPU index
       The maximum value for the CPU index is the number of available CPUs -1 */
    numCpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (DEFAULT_INDEX_CPU >= numCpus) {
	indexCpu = numCpus - 1;
	SwrxLog(NGENE_LOG_SEVERITY_WARNING, "CPU index %d not available; pthread_setaffinity_np will use CPU index %d instead", DEFAULT_INDEX_CPU, indexCpu);
    } else {
	indexCpu = DEFAULT_INDEX_CPU;
    }

    /* Act on the priority */
    maxPriority = sched_get_priority_max(SCHED_FIFO);
    SetCpuPriorityAndPolicy(indexCpu, SCHED_FIFO, maxPriority);

    /* Thread main loop */
    while (stopThread == FALSE_FLAG) {
	if (FrontEndCheckStatus(frontEndThreadFunctionParameters->ngeneFrontEnd) == FAILURE_RETURN) {
	    SwrxLog(NGENE_LOG_SEVERITY_INFO, "NGene2 front end thread stops due to a USB error");
	    stopThread = TRUE_FLAG;
	    pthread_mutex_lock(&usbFe->threadsMutexStopReceiver);
	    stopReceiverFlag = TRUE_FLAG;
	    pthread_mutex_unlock(&usbFe->threadsMutexStopReceiver);
	    /* If the receiver thread is blocked on a cond_wait, we need to release it */
	    pthread_mutex_lock(&usbFe->frontEndMutex);
	    pthread_cond_signal(&usbFe->intermediateBufferEmpty);
	    pthread_mutex_unlock(&usbFe->frontEndMutex);
	} else {
	    pthread_mutex_lock(&usbFe->threadsMutexStopFrontEnd);
	    stopFrontEndFlagLocal = stopFrontEndFlag;
	    pthread_mutex_unlock(&usbFe->threadsMutexStopFrontEnd);
	    if (stopFrontEndFlagLocal == TRUE_FLAG) {
		stopThread = TRUE_FLAG;
		SwrxLog(NGENE_LOG_SEVERITY_INFO, "NGene2 front end thread stops due to a user command");
	    } else {
		CopySamplesIntoUsbIntermediateBuffer(frontEndThreadFunctionParameters->ngeneFrontEnd);
		// wait some time
		nanosleep(&delay, NULL);
	    }
	}
    }

    SwrxLog(NGENE_LOG_SEVERITY_INFO, "NGene2 front end thread exiting with stopThread=%d and stopFrontEndFlagLocal=%d", stopThread, stopFrontEndFlagLocal);
    return NULL;
}

void SetCpuPriorityAndPolicy(int indexCpu, int policy, int priority)
{
    pthread_t this_thread;
    struct sched_param params;
    cpu_set_t cpuset;
    int ret = 0;

    this_thread = pthread_self();

    CPU_ZERO(&cpuset);
    CPU_SET(indexCpu, &cpuset);
    ret = pthread_setaffinity_np(this_thread, sizeof(cpu_set_t), &cpuset);
    if (ret != 0) {
	SwrxLog(NGENE_LOG_SEVERITY_INFO, "Unsuccessful in setting thread affinity");
    }

    params.sched_priority = priority;
    ret = pthread_setschedparam(this_thread, policy, &params);
    if (ret != 0) {
	SwrxLog(NGENE_LOG_SEVERITY_INFO, "Unsuccessful in setting thread priority and policy");
    }

}
