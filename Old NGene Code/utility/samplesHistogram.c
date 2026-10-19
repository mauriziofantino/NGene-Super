/**
 * @file samplesHistogram.c
 *
 * @author Mario Nicola
 *
 * @date 19/11/2014
 *
 * @version 1.0.0
 *
 * @brief histogram.c contains functions for the computation of the histogram of input raw data
 *
 * @note  From a previous release by Andrea Molino
 */

#include <stdlib.h>
#include "samplesHistogram.h"
#include "ngeneConstants.h"
#include "arrays.h"


int InitHistogram(TSamplesHistogram *hist,unsigned int bits,unsigned int samples)
{
	hist->firstExecution = TRUE_FLAG;
	hist->histogramSent = FALSE_FLAG;
	hist->intervals = 1<<HISTOGRAM_MAX_BITS;
	hist->dynamicMin = (float)(-(1<<(HISTOGRAM_MAX_BITS-1)));
	hist->guiIntervals = 1<<bits;
	hist->guiDynamicMin = (float)(-(1<<(bits-1)));
	hist->neededSamples = samples;
	hist->guiPacketLength = 1+(hist->guiIntervals*sizeof(float));
	hist->inputSamples = (char *)malloc(hist->neededSamples*sizeof(char));
	if(hist->inputSamples==NULL)
	{
		return FAILURE_RETURN;
	}
	hist->guiPacket = (char *)malloc(hist->guiPacketLength);
	if(hist->guiPacket==NULL)
	{
		hist->guiData = NULL;
		return FAILURE_RETURN;
	}
	hist->guiData = (float *)&(hist->guiPacket[1]);
	return SUCCESS_RETURN;
}


void DeinitHistogram(TSamplesHistogram *hist)
{
	if(hist->guiPacket!=NULL)
	{
		free(hist->inputSamples);
		hist->inputSamples = NULL;
		free(hist->guiPacket);
		hist->guiPacket = NULL;
		hist->guiData = NULL;
	}
}

void GetHistogramIntervals(TSamplesHistogram *hist)
{
	unsigned int idx;

	hist->guiData[0] = hist->guiDynamicMin;
	for(idx=1;idx<hist->guiIntervals;idx++)
	{
		hist->guiData[idx] = hist->guiData[idx-1] + (float)1.0;
	}
}


int EvaluateSampleHistogram(TSamplesHistogram *hist,char *samples, unsigned int availableSamples)
{
	unsigned int pos;
	unsigned int histPos;

	if(availableSamples < hist->neededSamples)
	{
		return FAILURE_RETURN;
	}
	/* if samples is NULL, use the array in the spectrum structure */
	if(samples==NULL)
	{
		samples = hist->inputSamples;
	}
	/* initialization */
	ArrayClean((char *)hist->data,hist->intervals*sizeof(float));
	/* calculate the histogram */
	for(pos=0;pos<hist->neededSamples;pos++)
	{
		histPos = ((unsigned int)(samples[pos]-hist->dynamicMin));
		hist->data[histPos]++;
	}
	/* normalize */
	for(pos=0;pos<hist->intervals;pos++)
	{
		hist->data[pos]/=hist->neededSamples;
	}
	/* cut the intervals for the GUI */
	for(pos=0;pos<hist->guiIntervals;pos++)
	{
		/* The two histograms (the one calculated by receiver and the one sent to the GUI)
		 * are centered around the same values */
		hist->guiData[pos] = hist->data[(hist->intervals/2) - (hist->guiIntervals/2) + pos];
	}

	return SUCCESS_RETURN;
}


float GetSignalAverage(char *samples,unsigned int avgLength)
{
	unsigned int pos;
	float avg = 0;

	for(pos=0;pos<avgLength; pos++)
	{
		avg += samples[pos];
	}
	return avg/avgLength;
}
