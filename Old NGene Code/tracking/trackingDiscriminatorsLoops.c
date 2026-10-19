/**
 * @file trackingDiscriminatorsLoops.c
 *
 * @author Mario Nicola
 *
 * @date 04/09/2014
 *
 * @version 1.0.0
 *
 * @brief Functions used to perform tracking discriminators and loop filters.
 */

#include <math.h>
#include "ngeneConfiguration.h"
#include "channelDataStructure.h"
#include "trackingDataStructure.h"
#include "trackingDiscriminatorsLoops.h"
#include "ngeneConstants.h"
#include "timeConstants.h"
#include "mathFunctions.h"
#include "ioFunctions.h"
#include "ngeneDataStructure.h"
#include "arrays.h"
#include "cno.h"

void CalculateLoopCoefficients(TNGeneConfiguration *cfg,TChannel *ch,TTracking *trk)
{
	TTrkConfiguration *trkCfg = &(cfg->trkConfiguration);
	double gainDll;
	double blDll;
	double gainFll;
	double blFll;
	double gainPll;
	double blPll;
	double zeta = 1 / sqrt(2);
	double delta;
	double wn;
	double t1;
	double t2;

	/* variables from configuration */
	if(ch->satType==GPS_SAT)
	{
		gainDll = trkCfg->gpsDllGain;
		blDll = trkCfg->gpsDllBw;
		trk->codeLoop.dllDiscriminatorFactor = 0.5;
		gainFll = trkCfg->gpsFllGain;
		blFll = trkCfg->gpsFllBw;
		gainPll = trkCfg->gpsPllGain * (float)trk->integrationPeriod;
		blPll = trkCfg->gpsPllBw;
	}
	else if(ch->satType==GALILEO_SAT)
	{
		gainDll = trkCfg->galileoDllGain;
		blDll = trkCfg->galileoDllBw;
		trk->codeLoop.dllDiscriminatorFactor = 1.0/6.0;
		gainFll = trkCfg->galileoFllGain;
		blFll = trkCfg->galileoFllBw;
		gainPll = trkCfg->galileoPllGain * (float)trk->integrationPeriod;
		blPll = trkCfg->galileoPllBw;
	}
	else /* EGNOS_SAT */
	{
		gainDll = trkCfg->egnosDllGain;
		blDll = trkCfg->egnosDllBw;
		trk->codeLoop.dllDiscriminatorFactor = 0.5;
		gainFll = trkCfg->egnosFllGain;
		blFll = trkCfg->egnosFllBw;
		gainPll = trkCfg->egnosPllGain * (float)trk->integrationPeriod;
		blPll = trkCfg->egnosPllBw;
	}
	if(cfg->automaticConfigurationFlag==FALSE_FLAG)
	{
		blDll = cfg->manuallyConfiguredChannels[ch->chIndex].dllBw;
		blPll = cfg->manuallyConfiguredChannels[ch->chIndex].pllBw;
	}
	delta = (double)trk->integrationPeriod/SECONDS_TO_MILLISECONDS;
	/* filter coefficients for DLL */
	wn = blDll / 0.53;
	t1 = gainDll / (wn * wn);
	t2 = 2 * zeta / wn;
	(trk->codeLoop).dllT2T1 = t2 / t1;
	(trk->codeLoop).dllDeltaT1 = delta / t1;
	/* filter coefficients for PLL */
	wn = blPll / 0.53;
	t1 = gainPll / (wn * wn);
	t2 = 2 * zeta / wn;
	(trk->carrierLoop).pllT2T1 = t2 / t1;
	(trk->carrierLoop).pllDeltaT1 = delta / t1;
	/* filter coefficients for FLL */
	wn = blFll / 0.53;
	t1 = gainFll / (wn * wn);
	t2 = 2 * zeta / wn;
	(trk->carrierLoop).fllT2T1 = t2 / t1;
	(trk->carrierLoop).fllDeltaT1 = delta / t1;
	(trk->carrierLoop).delta = delta;
	/* CNo */
	trk->trkStatusChecker.cnoEstimator.dllBandwidth = blDll;
}

void RestartCarrierLoop(TChannel *ch,TTracking *trk)
{
	trk->carrierLoop.initialCarrierFrequency = ch->carrierFrequency;
	trk->carrierLoop.errort = 0.0;
	trk->carrierLoop.errortOld = 0.0;
	trk->carrierLoop.carrierNcoOld = 0.0;
	trk->carrierLoop.meanCarrierFreq.filterValue = ch->carrierFrequency;
	trk->carrierLoop.meanCarrierFreqOld = ch->carrierFrequency;
}

void RestartCodeLoop(TChannel *ch,TTracking *trk)
{
	trk->codeLoop.initialCodeRate = ch->codeRate;
	trk->codeLoop.dllDiscriminator = 0.0;
	trk->codeLoop.dllDiscriminatorOld = 0.0;
	trk->codeLoop.codeNcoOld = 0.0;
}

void InitTrackingLoops(TNGeneConfiguration *cfg,TChannel *ch,TTracking *trk)
{
	CalculateLoopCoefficients(cfg,ch,trk);
	RestartCarrierLoop(ch,trk);
	RestartCodeLoop(ch,trk);
	InitAverageWindow(&(trk->carrierLoop.meanCarrierFreq),cfg->trkConfiguration.fllPllChoiceFilterLen,ch->carrierFrequency);
	trk->carrierLoop.meanCarrierFreqOld = 0.0;
}

TCarrierLoopChoice ChooseCarrierLoop(TNGeneConfiguration *cfg,TNGeneData *rx,TChannel *ch,TTracking *trk)
{
	TFllPll *fllPll = &(trk->carrierLoop);
	TTrkConfiguration *trkCfg = &(cfg->trkConfiguration);
	double meanCarrierFreq;
	/* CN0 */
	int N;
	int M;
	double firstCN0;

	if(trk->fllLocked==TRUE_FLAG)
	{
		return CARRIER_PLL;
	}
	else
	{
		if((trk->trkCounter+1)%trkCfg->fllPllChoiceFilterUpdateTime==0)
		{
			meanCarrierFreq = GetAverageWindowValue(&(fllPll->meanCarrierFreq));
			if(fabs(meanCarrierFreq-fllPll->meanCarrierFreqOld)<(double)trkCfg->fllPllThreshold)
			{
				SwrxLog(NGENE_LOG_SEVERITY_INFO,"Channel %2d %s tracking FLL locked at sample %llu, trk counter %llu, with Doppler %f Hz!",ch->chIndex,ch->prnName,rx->buf.discardedSamples+ch->bufferOffset+ch->blocksize,trk->trkCounter,ch->dopplerShift);
				trk->fllLocked = TRUE_FLAG;
				trk->trkStatusChecker.pllStartCounter = trk->trkCounter;
				RestartCarrierLoop(ch,trk);
	            /* Initialize CN0 estimation algorithm */
	            N = 100;
	            M = 100;
	            firstCN0 = 50.0;
	            SetCnoMeasurement(&(trk->trkStatusChecker.cnoEstimator),trk->pIForCno,trk->pQForCno,N,M,firstCN0);
				return CARRIER_PLL;
			}
			else
			{
				fllPll->meanCarrierFreqOld = meanCarrierFreq;
				return CARRIER_FLL;
			}
		}
	}
	return CARRIER_FLL;
}

void FllDiscriminator(TTracking *trk)
{
	double dot;
	double cross;
    double phase;

    /* when trk->time == 1, pIOld and pQOld are not valid */
    if(trk->trkCounter>0)
    {
    	cross = ((trk->pIOld*trk->pQ) - (trk->pI*trk->pQOld));
    	dot = ((trk->pIOld*trk->pI) + (trk->pQOld*trk->pQ));

        if(dot != 0)
        {
        	phase = -atan(cross/dot);
        }
        else
        {
            phase = 0;
        }
        trk->carrierLoop.errort = phase / TWICE_M_PI;
        /* the old version follows */
        /* trk->carrierLoop.errort = phase / (trk->carrierLoop.delta * 360.0); */
    }
}

void PllDiscriminator(TTracking *trk)
{
	if(trk->pI != 0.0)
    {
		/* if it happens, then nan value is returned!!! */
		trk->carrierLoop.errort = -atan(trk->pQ/trk->pI)/TWICE_M_PI;
		/* the old version follows */
		/* trk->carrierLoop.errort = -atan(trk->pQ/trk->pI); */
    }
    else
    {
    	trk->carrierLoop.errort = 0.0;
    }
}

void DllDiscriminator(TTracking *trk)
{
	double num;
	double denum;
	#ifdef CSC_DISCRIMINATOR_FUNCTION
	double e,l, vE,vL,eN,lN,vEN,vLN;
	#endif


	#ifdef CSC_DISCRIMINATOR_FUNCTION

	if((trk->trkCounter-trk->lockCounter)>=(MAX_NUMBER_DATA_TRANSITION*ch->bitDuration) && (trk->lockCounter!=INT_MAX))
	{

		e=(trk->eQ*trk->eQ + trk->eI*trk->eI);
		l=(trk->lQ*trk->lQ + trk->lI*trk->lI);
		eN=e/(e+l);
		lN=l/(e+l);
		vE=(trk->vEQ*trk->vEQ + trk->vEI*trk->vEI);
		vL=(trk->vLQ*trk->vLQ + trk->vLI*trk->vLI);
		vEN=vE/(e+l);
		vLN=vL/(e+l);

		trk->codeLoop.dllDiscriminator = 2*(eN*eN)-(vEN*vEN)-4*(lN*lN)-(vLN*vLN)+4*(lN*vLN);
	}
	else
	{
		num = (trk->eQ*trk->eQ + trk->eI*trk->eI) - (trk->lQ*trk->lQ + trk->lI*trk->lI);
		denum = (trk->eQ*trk->eQ + trk->eI*trk->eI) + (trk->lQ*trk->lQ + trk->lI*trk->lI);
		if(denum != 0.0)
		{
			trk->codeLoop.dllDiscriminator =  0.5*trk->codeLoop.dllDiscriminatorFactor*(num / denum);
		}
		else
		{
			trk->codeLoop.dllDiscriminator = 0.0;
		}
	}




	#else

	num = (trk->eQ*trk->eQ + trk->eI*trk->eI) - (trk->lQ*trk->lQ + trk->lI*trk->lI);
	denum = (trk->eQ*trk->eQ + trk->eI*trk->eI) + (trk->lQ*trk->lQ + trk->lI*trk->lI);
	if(denum != 0.0)
	{
		trk->codeLoop.dllDiscriminator =  trk->codeLoop.dllDiscriminatorFactor*(num / denum);
	}
	else
	{
		trk->codeLoop.dllDiscriminator = 0.0;
	}

	#endif

}

void CarrierLoop(TNGeneConfiguration *cfg,TChannel *ch, TTracking *trk)
{
    double carrierNco = 0.0;
    TFllPll *filter = &(trk->carrierLoop);

    if(trk->fllLocked == FALSE_FLAG)
    {
    	carrierNco = filter->carrierNcoOld + (filter->fllT2T1 * (filter->errort - filter->errortOld)) + (filter->errort * filter->fllDeltaT1);
    }
    else /* if PLL */
    {
    	carrierNco = filter->carrierNcoOld + (filter->pllT2T1 * (filter->errort - filter->errortOld)) + (filter->errort * filter->pllDeltaT1);
    }
    ch->carrierFrequency = filter->initialCarrierFrequency + carrierNco;
    ch->dopplerShift = ch->carrierFrequency - cfg->fif;
    PerformAverageWindow(&(filter->meanCarrierFreq),ch->carrierFrequency);
    filter->carrierNcoOld = carrierNco;
    filter->errortOld = filter->errort;
}

void CodeLoop(TChannel *ch, TTracking *trk)
{
    double codeNco;
    double tChip, codeDelayErrorPerTChip;
    TDll *filter = &(trk->codeLoop);

    codeNco= filter->codeNcoOld + (filter->dllT2T1 * (filter->dllDiscriminator - filter->dllDiscriminatorOld)) + filter->dllDiscriminator * filter->dllDeltaT1;
    ch->codeRate = filter->initialCodeRate - codeNco;
    PerformAverageWindow(&(ch->codeRateMean),ch->codeRate);
    filter->codeNcoOld = codeNco;
    filter->dllDiscriminatorOld = filter->dllDiscriminator;
    /* Computation of the code delay tracking error variance */
    tChip=1.0/ch->codeRate; /* chip time */
    codeDelayErrorPerTChip=(ch->codeRate-(double)GPS_CODE_RATE)*tChip;
    ComputeSigmaJ(&(trk->trkStatusChecker.cnoEstimator), codeDelayErrorPerTChip);
    //**
}

