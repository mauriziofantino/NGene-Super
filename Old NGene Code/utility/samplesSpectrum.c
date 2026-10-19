/**
 * @file samplesSpectrum.c
 *
 * @author Mario Nicola
 *
 * @date 20/11/2014
 *
 * @version 1.0.0
 *
 * @brief Implementations of basic functions for the computation of the spectrum
 *
 * @note From a previous release by Andrea Molino
 */

#include <stdlib.h>
#include <math.h>
#include "ngeneConfiguration.h"
#include "samplesSpectrum.h"
#include "samplesHistogram.h"
#include "ioFunctions.h"
#include "closeManager.h"
#include "arrays.h"
#include "fft.h"


/**
 * @def SPECTRUM_FFT_NORM_FACTOR
 * @brief normalization factor imposed to the FFT engine inside the spectrum computation
 */
#define SPECTRUM_FFT_NORM_FACTOR   0


unsigned int roundToLog2up(unsigned int value)
{
	unsigned int tmp, result;

	result = 0;
	tmp = value;
	while(tmp)
	{
		tmp >>= 1;
		result ++;
	}
	if((unsigned int)1<<((result)-1) == value)
	{
		/* value was a power of two */
		result --;
	}
	if((int)result == -1)
	{
		/* value was zero */
		result = 0;
	}
	return result;
}


int InitSamplesSpectrum(TNGeneConfiguration *cfg,TSamplesSpectrum *spectrum)
{
	TGuiConfiguration *guiCfg = &(cfg->guiParameters);
	unsigned int pos;
	static float wAccValue;
	unsigned int spectrumFixedGain;

	if(cfg->usedFrontEnd==NGENE_FE_SIGE)
	{
		spectrumFixedGain = 128;
	}
	else
	{
		spectrumFixedGain = 8;
	}
	spectrum->spectrumSent = FALSE_FLAG;
	spectrum->windowLen = guiCfg->spectrumSamples;
	spectrum->windowLenLog2 = roundToLog2up(guiCfg->spectrumSamples);
	posix_memalign((void **)&(spectrum->fftInput),POSIX_ALIGNMENT,2*spectrum->windowLen*sizeof(unsigned int));
	posix_memalign((void **)&(spectrum->fftOutput),POSIX_ALIGNMENT,2*spectrum->windowLen*sizeof(unsigned int));
	if((spectrum->fftOutput==NULL) || (spectrum->fftInput==NULL))
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Malloc returned a NULL pointer in InitSamplesSpectrum");
		CloseNGene(cfg,EXIT_FAILURE);
	}
	spectrum->windowType = guiCfg->spectrumWeightFunction;
	spectrum->windowNum = guiCfg->spectrumWindows;
	spectrum->windowOverlap = guiCfg->spectrumOverlap;
	if((guiCfg->spectrumZeroPadding>0) && ((unsigned int)guiCfg->spectrumZeroPadding<spectrum->windowLen))
	{
	  /* There is some zero padding */
	  spectrum->dataWindowLen = spectrum->windowLen - guiCfg->spectrumZeroPadding;
	}
	else
	{
		spectrum->dataWindowLen = spectrum->windowLen;
	}
	spectrum->windowTableLen =  (spectrum->dataWindowLen/2) + 1;
	spectrum->windowTable = (float *)malloc((spectrum->windowTableLen+1)*sizeof(float));
	if(spectrum->windowTable==NULL)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Malloc returned a NULL pointer in InitSamplesSpectrum");
		CloseNGene(cfg,EXIT_FAILURE);
	}
	/* Generate the window function */
	wAccValue = 0;
	switch(guiCfg->spectrumWeightFunction)
	{
		case SPECTRUM_WEIGHT_FUNC_RECTANGULAR:
			for(pos=0;pos<spectrum->windowTableLen;pos++)
			{
				spectrum->windowTable[pos] = spectrumFixedGain * 1.0;
				wAccValue += spectrum->windowTable[pos];
			}
			break;
		case SPECTRUM_WEIGHT_FUNC_TRIANGULAR:
			for(pos=0;pos<spectrum->windowTableLen;pos++)
			{
				spectrum->windowTable[pos] = spectrumFixedGain * (2.0/spectrum->dataWindowLen) * ((spectrum->dataWindowLen/2.0) - fabs(pos-(spectrum->dataWindowLen-1)/(2.0)));
				wAccValue += spectrum->windowTable[pos];
			}
			break;
		case SPECTRUM_WEIGHT_FUNC_HAMMING:
			for(pos=0;pos<spectrum->windowTableLen;pos++)
			{
				spectrum->windowTable[pos] = spectrumFixedGain * (0.53836 - 0.46164*(cos((2*M_PI*(pos-1))/(spectrum->dataWindowLen))));
				wAccValue += spectrum->windowTable[pos];
			}
			break;
		case SPECTRUM_WEIGHT_FUNC_HANNING:
		default:
			for(pos=0;pos<spectrum->windowTableLen;pos++)
			{
				spectrum->windowTable[pos] = spectrumFixedGain *  0.5 * (1-cos(2*M_PI*(pos-1)/spectrum->dataWindowLen));
				wAccValue += spectrum->windowTable[pos];
			}
			break;
	}
	spectrum->spectrumNormalizationFactor = (spectrum->windowNum*spectrum->dataWindowLen*wAccValue)/(1<<(-SPECTRUM_FFT_NORM_FACTOR));
	/* Needed samples for the computation of the spectrum (over-estimated: does not take into account zero padding) */
	spectrum->neededSamples = spectrum->windowLen + ((spectrum->windowNum -1) * (spectrum->windowLen - spectrum->windowOverlap));
	spectrum->inputSamples = (char *)malloc(spectrum->neededSamples*sizeof(char));
	if(spectrum->inputSamples==NULL)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Malloc returned a NULL pointer in InitSamplesSpectrum");
		CloseNGene(cfg,EXIT_FAILURE);
	}
	spectrum->inputAverageValid = FALSE_FLAG;
	spectrum->inputAverage = 0.0;
	/* output array */
	spectrum->spectrumSize =  (spectrum->windowLen/2) + 1;
	spectrum->spectrumData = (float *)malloc(spectrum->spectrumSize*sizeof(float));
	if(spectrum->spectrumData==NULL)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Malloc returned a NULL pointer in InitSamplesSpectrum");
		CloseNGene(cfg,EXIT_FAILURE);
	}
	return SUCCESS_RETURN;
}


void DeinitSamplesSpectrum(TSamplesSpectrum *spectrum)
{
	free(spectrum->inputSamples);
	free(spectrum->fftInput);
	free(spectrum->fftOutput);
	free(spectrum->windowTable);
	free(spectrum->spectrumData);
}


int EvaluateSamplesPowerSpectrum(TNGeneConfiguration *cfg,TFftTable *fftTable,TSamplesSpectrum *spectrum,char *samples,unsigned int availableSamples)
{
	char *samplesPtr;
	unsigned int wCnt;
	unsigned int pos;

	if(availableSamples < spectrum->neededSamples)
	{
	    return FAILURE_RETURN;
	}

	/* if samples is NULL, use the array in the spectrum structure */
	if(samples==NULL)
	{
		samples = spectrum->inputSamples;
	}
	samplesPtr = samples;
	/* clear the spectrum buffer */
	ArrayClean((char *)(spectrum->spectrumData),spectrum->spectrumSize*sizeof(float));
	/* if needed, evaluate average value of the input samples */
	if(spectrum->inputAverageValid==FALSE_FLAG)
	{
		spectrum->inputAverage = GetSignalAverage(samples,spectrum->neededSamples);
		spectrum->inputAverageValid = TRUE_FLAG;
	}
	/* accumulate different windows */
	for(wCnt=0;wCnt<spectrum->windowNum;wCnt++)
	{
		/* Clean the whole input array:
		 * - initializes imaginary part of samples (always zero)
		 * - performs the zero padding */
		ArrayClean((char *)(spectrum->fftInput),2*spectrum->windowLen*sizeof(short int));
		/* load the input samples (interleaved R and I) into the input vector of the FFT core */
		for(pos=0;pos<spectrum->dataWindowLen;pos++)
		{
			if(pos<spectrum->windowTableLen)
			{
				/* first half of the window */
				spectrum->fftInput[2*pos] = (short int)(((float)(samplesPtr[pos])-spectrum->inputAverage) * spectrum->windowTable[pos]);
			}
			else
			{
				/* second half of the window */
				spectrum->fftInput[2*pos] = (short int)(((float)(samplesPtr[pos])-spectrum->inputAverage) * spectrum->windowTable[(2*spectrum->windowTableLen)-pos-1]);
			}
		}
		/* perform FFT */
		PerformFft(cfg,fftTable,spectrum->fftInput,spectrum->fftOutput,spectrum->windowLenLog2,SPECTRUM_FFT_NORM_FACTOR);
		for(pos=0;pos<spectrum->spectrumSize;pos++)
		{
			spectrum->spectrumData[pos] += (spectrum->fftOutput[2*pos]*spectrum->fftOutput[2*pos]) + (spectrum->fftOutput[(2*pos)+1]*spectrum->fftOutput[(2*pos)+1]);
		}
		samplesPtr += (spectrum->dataWindowLen - spectrum->windowOverlap);
	}
	/* divide and normalize */
	for(pos=0;pos<spectrum->spectrumSize;pos++)
	{
		spectrum->spectrumData[pos] = (float) 20*log10((spectrum->spectrumData[pos]/spectrum->spectrumNormalizationFactor));
	}

	return SUCCESS_RETURN;
}
