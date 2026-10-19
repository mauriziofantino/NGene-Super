/**
 * @file additionalLogs.c
 *
 * @author Mario Nicola
 *
 * @date 19/11/2014
 *
 * @version 1.0.0
 *
 * @brief Functions used to calculate and send to the GUI additional logs.
 *
 * @note From a previous release by Andrea Molino
 */

#include <string.h>
#include "ngeneDataStructure.h"
#include "ngeneConstants.h"
#include "commModule.h"
#include "arrays.h"
#include "additionalLogs.h"
#include "ioFunctions.h"

/**
 * @def GUI_HISTOGRAM_X_AXIS_FLAG
 * @brief In the histogram packet for the GUI, the element with index 0 equal to 1 stands for X axis
 */
#define GUI_HISTOGRAM_X_AXIS_FLAG   1

/**
 * @def GUI_HISTOGRAM_Y_AXIS_FLAG
 * @brief In the histogram packet for the GUI, the element with index 0 equal to 0 stands for Y axis
 */
#define GUI_HISTOGRAM_Y_AXIS_FLAG   0



int EvaluateAndSendSamplesHistogram(TNGeneData *rx)
{
	TSamplesHistogram *hist = &(rx->samplesHistogram);
	TCircularBuffer *buf = &(rx->buf);

	/* samples histogram data structure not initialized */
	if(hist->guiPacket==NULL)
	{
		return FAILURE_RETURN;
	}
	/* GUI does not require histogram */
	if(ComHistogramOn()==COMM_F)
	{
		return FAILURE_RETURN;
	}

	/* Prepare input samples */
	if(CopyABunchOfContiguousSamples(buf,hist->neededSamples,hist->inputSamples)==FAILURE_RETURN)
	{
		return FAILURE_RETURN;
	}
	/* Evaluate and send */
	ComLockHistogramArea();
	if(hist->firstExecution==TRUE_FLAG)
	{
		/* The first time send the intervals (displayed in the X axis) */
		hist->guiPacket[0] = GUI_HISTOGRAM_X_AXIS_FLAG;
		GetHistogramIntervals(hist);
		hist->firstExecution = FALSE_FLAG;
	}
	else
	{
		hist->guiPacket[0] = GUI_HISTOGRAM_Y_AXIS_FLAG;
		EvaluateSampleHistogram(hist,NULL,hist->neededSamples);
	}
	ComUnlockHistogramArea();
	ComSendHistogram((void *)(hist->guiPacket),hist->guiPacketLength);
	hist->histogramSent = TRUE_FLAG;

	return SUCCESS_RETURN;
}


int EvaluateAndSendSamplesSpectrum(TNGeneConfiguration *cfg,TNGeneData *rx)
{
	TSamplesSpectrum *spectrum = &(rx->samplesSpectrum);
	TCircularBuffer *buf = &(rx->buf);

	/* GUI does not require spectrum */
	if(ComSpectrumOn()==COMM_F)
	{
		return SUCCESS_RETURN;
	}

	/* Prepare input samples */
	if(CopyABunchOfContiguousSamples(buf,spectrum->neededSamples,spectrum->inputSamples)==FAILURE_RETURN)
	{
		return FAILURE_RETURN;
	}
	/* Evaluate and send */
	ComLockSpectrumArea();
	EvaluateSamplesPowerSpectrum(cfg,&(rx->fftTable),spectrum,NULL,spectrum->neededSamples);
	ComUnlockSpectrumArea();
	ComSendSpectrum((void *)(spectrum->spectrumData),spectrum->spectrumSize*sizeof(float));
	spectrum->spectrumSent = TRUE_FLAG;

	return SUCCESS_RETURN;
}


/**
 * @def GUI_EPHEMERIS_CONSTELLATION_TYPE_GPS
 * @brief Constant used to send GPS ephemerides to the Java GUI
 */
#define GUI_EPHEMERIS_CONSTELLATION_TYPE_GPS		0

/**
 * @def GUI_EPHEMERIS_CONSTELLATION_TYPE_EGNOS
 * @brief Constant used to send EGNOS ephemerides to the Java GUI
 */
#define GUI_EPHEMERIS_CONSTELLATION_TYPE_EGNOS   	1

/**
 * @def GUI_EPHEMERIS_CONSTELLATION_TYPE_GALILEO
 * @brief Constant used to send Galileo ephemerides to the Java GUI
 */
#define GUI_EPHEMERIS_CONSTELLATION_TYPE_GALILEO	2

/**
 * @def GUI_EPHEMERIS_FIRST_WAIT
 * @brief Before sending ephemerides, waits GUI_EPHEMERIS_FIRST_WAIT PVT computations
 *        in order to be sure that GUI does not write a wrong ephemerides time in the RINEX file.
 */
#define GUI_EPHEMERIS_FIRST_WAIT	10


void SendNavigationMessageInformation(TNGeneConfiguration *cfg,TNGeneData *rx)
{
	int chIdx = 0;
	int finito = FALSE_FLAG;
	TChannel *ch;
	TNavigationMessage *navMsg;
	TGnssEphemerides *eph;
	TGnssClk *clk;
	void *pBuff;

	if(rx->ephemeridesPacketToGui==NULL)
	{
		return;
	}

	if(rx->pvtData.pvtIndex<GUI_EPHEMERIS_FIRST_WAIT)
	{
		return;
	}

	while((finito==FALSE_FLAG) && (chIdx<cfg->numberOfChannels))
	{
		navMsg = NULL;
		eph = NULL;
		clk = NULL;
		ch = &(rx->channelArray[chIdx]);
		if((ch->satType==GPS_SAT) && (rx->pvtResults.rxGpsTime.referenceValid==TRUE_FLAG))
		{
			navMsg = ch->gpsParser->navMsg;
		}
		else if((ch->satType==GALILEO_SAT) && (rx->pvtResults.rxGalTime.referenceValid==TRUE_FLAG))
		{
			navMsg = ch->galParser->navMsg;
		}
		if(navMsg!=NULL)
		{
			eph = &(navMsg->eph);
			clk = &(navMsg->clk);
		}
		if((ch->newNavMsgInformation==TRUE_FLAG) && (navMsg!=NULL) && (eph!=NULL) && (clk!=NULL))
		{
			ch->newNavMsgInformation=FALSE_FLAG;	/* send the same information once */
			finito=TRUE_FLAG;						/* send only one packet per time */
			pBuff = (void *)(rx->ephemeridesPacketToGui);
			ComLockEphemerisArea();
			if(ch->satType==GPS_SAT)
			{
				WriteAnInt(&pBuff,GUI_EPHEMERIS_CONSTELLATION_TYPE_GPS);
			}
			else /* if(ch->satType==GALILEO_SAT) */
			{
				WriteAnInt(&pBuff,GUI_EPHEMERIS_CONSTELLATION_TYPE_GALILEO);
			}
			WriteAnInt(&pBuff,ch->satId);
			WriteADouble(&pBuff,eph->meanAnomaly0);
			WriteADouble(&pBuff,eph->deltaN);
			WriteADouble(&pBuff,eph->eccentricity);
			WriteADouble(&pBuff,eph->sqrtA);
			WriteADouble(&pBuff,eph->capitalOmega0);
			WriteADouble(&pBuff,eph->i0);
			WriteADouble(&pBuff,eph->omega);
			WriteADouble(&pBuff,eph->capitalOmegaDot);
			WriteADouble(&pBuff,eph->iDot);
			WriteADouble(&pBuff,eph->capitalCuc);
			WriteADouble(&pBuff,eph->capitalCus);
			WriteADouble(&pBuff,eph->capitalCrc);
			WriteADouble(&pBuff,eph->capitalCrs);
			WriteADouble(&pBuff,eph->capitalCic);
			WriteADouble(&pBuff,eph->capitalCis);
			WriteADouble(&pBuff,eph->toe);
			WriteAnInt(&pBuff,navMsg->wn);
			WriteAnInt(&pBuff,eph->health);
			WriteAnInt(&pBuff,eph->sisa);	/* SISA name is used for both Galileo (SISA) and GPS (namely URA idx) */
			WriteAnInt(&pBuff,eph->iode2);
			WriteAnInt(&pBuff,eph->iode3);
			WriteADouble(&pBuff,clk->toc);
			WriteADouble(&pBuff,clk->af[0]);
			WriteADouble(&pBuff,clk->af[1]);
			WriteADouble(&pBuff,clk->af[2]);
			WriteADouble(&pBuff,clk->tGroupDelay);
			WriteAnInt(&pBuff,clk->iodc);
			ComUnlockEphemerisArea();
			ComSendEphemeris((void *)(rx->ephemeridesPacketToGui),GUI_EPHEMERIDES_PACKET_SIZE);
		}
		chIdx++;
	}
}


//int SendLogFiles(TNGeneConfiguration *cfg,TNGeneData *rx)
//{
//	/* TODO make the send of the correlation file reliable */
//
//	/* sending the correlation file causes the receiver to crash,
//	 * seemingly due to the large size of the file: this behaviour must be studied */
//	char filename[MAX_ROW_LENGTH];
//
//	//
//	if(cfg->userInterface!=RX_UI_GUI)
//	{
//		return FAILURE_RETURN;
//	}
//
//	//if(cfg->guiParameters.outputPathLength == 0)
//	{
//		// send high-correlators log files
//		if(cfg->guiParameters.correlatorsGenerationFlag==TRUE_FLAG)
//		{
//			filename[0] = '\0';
//			strcat(filename, cfg->dumpPath);
//			strcat(filename, "/trackingCorrelators.bin");
//			if(ComSendHighCorrFile(filename)!=0)
//			{
//				SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Problems sending High-Rate correlator file");
//			}
//		}
//		// in this release the log of raw samples is not available
//		// the code is maintained here for reference in case of future use
//		// send raw samples log files
//		//
//		//if((r->rx_cpb).rawSamplesFile != NULL)
//		//{
//		//	fclose((r->rx_cpb).rawSamplesFile);
//		//	(r->rx_cpb).rawSamplesFile = NULL;
//		//	sprintf(nomefile,"%srawSamples.bin", GNSS_RX_DUMPPATH);
//
//		//	if(user_interface == RX_UI_GUI){
//		//		if(ComSendRawSamplesFile(nomefile)!=0){
//		//			swrx_log(stdlog,NGENE_LOG_SEVERITY_ERROR,"Problems sending raw-sample file");
//		//		}
//		//	}
//		//}
//	}
//
//	return SUCCESS_RETURN;
//}
