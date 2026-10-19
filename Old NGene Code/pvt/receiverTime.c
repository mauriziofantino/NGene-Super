/**
 * @file receiverTime.c
 *
 * @author Mario Nicola
 *
 * @date 27/10/2014
 *
 * @version 1.0.0
 *
 * @brief Functions used to manage the receiver time
 */

#include "ngeneDataStructure.h"
#include "receiverTime.h"
#include "timeConstants.h"
#include "timeConversions.h"


void ResetReferenceRxTime(TNGeneData *rx)
{
	TPvtData *pvtData = &(rx->pvtData);
	TPvtResults *pvtRes = &(rx->pvtResults);

	// clear time
	pvtData->rxTimeForNextPvtFlag=INVALID_TIME;
	pvtRes->rxGpsTime.referenceValid=FALSE_FLAG;
	pvtRes->rxGpsTime.referenceUpdated=FALSE_FLAG;
	pvtRes->rxGalTime.referenceValid=FALSE_FLAG;
	pvtRes->rxGalTime.referenceUpdated=FALSE_FLAG;
	pvtRes->utcFromGps=FALSE_FLAG;
	pvtRes->utcFromGal=FALSE_FLAG;
	pvtRes->utcFromDefault=FALSE_FLAG;
	pvtRes->rxUtcTime.referenceValid=FALSE_FLAG;
	pvtRes->rxUtcTime.referenceUpdated=FALSE_FLAG;
}


void SetReferenceRxTime(TNGeneData *rx,int defaultLeapSeconds)
{
	int index;
	TPvtResults *pvtRes = &(rx->pvtTemporaryResults);
	TPvtData *pvtData = &(rx->pvtData);
	unsigned long long referenceSample = rx->buf.discardedSamples;
	TPvtChannel *pvtCh;
	TChannel *ch;
	TReceiverTime *rxGpsTime = &(pvtRes->rxGpsTime);
	TReceiverTime *rxGalTime = &(pvtRes->rxGalTime);
	TReceiverTime *rxUtcTime = &(pvtRes->rxUtcTime);
	TNavigationMessage *navMsg;

	/* in case of complete outage, reference time is not updated */
	if(pvtData->numOfOperativeChannels==0)
	{
		return;
	}

	/* GPS */
	if((pvtData->numOfGpsOpChannels>0) && (pvtRes->clockBiasGpsValid==TRUE_FLAG))
	{
		index=0;
		while((index<pvtData->numOfOperativeChannels) && (pvtData->pvtChArray[index].ch->satType!=GPS_SAT))
		{
			index++;
		}
		if(index<pvtData->numOfOperativeChannels)
		{
			pvtCh = &(pvtData->pvtChArray[index]);
			ch = pvtCh->ch;
			rxGpsTime->referenceValid = TRUE_FLAG;
			rxGpsTime->referenceUpdated = TRUE_FLAG;
			rxGpsTime->referenceSample = referenceSample;
			rxGpsTime->referenceWn = ch->gpsParser->navMsg->wn;
			rxGpsTime->referenceTow = pvtCh->transmissionTime + (pvtCh->pseudorangeRaw/SPEED_OF_LIGHT) - (pvtRes->clockBiasGps/SPEED_OF_LIGHT);
			if(rxGpsTime->referenceTow > SECONDS_IN_A_WEEK)
			{
				rxGpsTime->referenceWn++;
				rxGpsTime->referenceTow -= SECONDS_IN_A_WEEK;
			}
		}
	}
	/* Galileo */
	if((pvtData->numOfGalileoOpChannels>0) && (pvtRes->clockBiasGalileoValid==TRUE_FLAG))
	{
		index=0;
		while((index<pvtData->numOfOperativeChannels) && (pvtData->pvtChArray[index].ch->satType!=GALILEO_SAT))
		{
			index++;
		}
		if(index<pvtData->numOfOperativeChannels)
		{
			pvtCh = &(pvtData->pvtChArray[index]);
			ch = pvtCh->ch;
			rxGalTime->referenceValid = TRUE_FLAG;
			rxGalTime->referenceUpdated = TRUE_FLAG;
			rxGalTime->referenceSample = referenceSample;
			rxGalTime->referenceWn = ch->galParser->navMsg->wn;
			rxGalTime->referenceTow = pvtCh->transmissionTime + (pvtCh->pseudorangeRaw/SPEED_OF_LIGHT) - (pvtRes->clockBiasGalileo/SPEED_OF_LIGHT);
			if(rxGalTime->referenceTow > SECONDS_IN_A_WEEK)
			{
				rxGalTime->referenceWn++;
				rxGalTime->referenceTow -= SECONDS_IN_A_WEEK;
			}
		}
	}
	/* UTC */
	/* In order to estimate UTC time, GPS or Galileo time is converted.
	 * The choice of the constellation depends on the availability of its time and its conversion parameters.
	 * The precedence is given to Galileo.
	 * UTC is calculated starting from the same constellation for the whole NGene2 execution. */
	if((rxGalTime->referenceValid==TRUE_FLAG) && (rxGalTime->referenceUpdated==TRUE_FLAG) && ((pvtRes->utcFromGps==FALSE_FLAG) || (pvtRes->utcFromDefault==TRUE_FLAG)))
	{
		index = 0;
		while((index<GALILEO_NUMBER_OF_PRNS) && (rx->galNavigationMessage[index].gnss2UtcReceived==FALSE_FLAG))
		{
			index++;
		}
		if(index<GALILEO_NUMBER_OF_PRNS)
		{
			navMsg = &(rx->galNavigationMessage[index]);
			rxUtcTime->referenceSample = referenceSample;
			FromGnssTimeToUtcTime(&(navMsg->gnss2Utc),navMsg->gnss2UtcReceived,defaultLeapSeconds,rxGalTime->referenceWn,rxGalTime->referenceTow,&(rxUtcTime->referenceWn),&(rxUtcTime->referenceTow));
			rxUtcTime->referenceValid = TRUE_FLAG;
			rxUtcTime->referenceUpdated = TRUE_FLAG;
			pvtRes->utcFromGps = FALSE_FLAG;
			pvtRes->utcFromGal = TRUE_FLAG;
			pvtRes->utcFromDefault = FALSE_FLAG;
			return;
		}
	}
	if((rxGpsTime->referenceValid==TRUE_FLAG) && (rxGpsTime->referenceUpdated==TRUE_FLAG) && ((pvtRes->utcFromGal==FALSE_FLAG) || (pvtRes->utcFromDefault==TRUE_FLAG)))
	{
		index = 0;
		while((index<GPS_NUMBER_OF_PRNS) && (rx->gpsNavigationMessage[index].gnss2UtcReceived==FALSE_FLAG))
		{
			index++;
		}
		if(index<GPS_NUMBER_OF_PRNS)
		{
			navMsg = &(rx->gpsNavigationMessage[index]);
			rxUtcTime->referenceSample = referenceSample;
			FromGnssTimeToUtcTime(&(navMsg->gnss2Utc),navMsg->gnss2UtcReceived,defaultLeapSeconds,rxGpsTime->referenceWn,rxGpsTime->referenceTow,&(rxUtcTime->referenceWn),&(rxUtcTime->referenceTow));
			rxUtcTime->referenceValid = TRUE_FLAG;
			rxUtcTime->referenceUpdated = TRUE_FLAG;
			pvtRes->utcFromGps = TRUE_FLAG;
			pvtRes->utcFromGal = FALSE_FLAG;
			pvtRes->utcFromDefault = FALSE_FLAG;
			return;
		}
	}
	// if here, the conversion parameters are not available, try to use default value of leap seconds
	if((rxGalTime->referenceValid==TRUE_FLAG) && (rxGalTime->referenceUpdated==TRUE_FLAG) && (pvtRes->utcFromGps==FALSE_FLAG))
	{
		rxUtcTime->referenceSample = referenceSample;
		FromGnssTimeToUtcTime(NULL,FALSE_FLAG,defaultLeapSeconds,rxGalTime->referenceWn,rxGalTime->referenceTow,&(rxUtcTime->referenceWn),&(rxUtcTime->referenceTow));
		rxUtcTime->referenceValid = TRUE_FLAG;
		rxUtcTime->referenceUpdated = TRUE_FLAG;
		pvtRes->utcFromGps = FALSE_FLAG;
		pvtRes->utcFromGal = TRUE_FLAG;
		pvtRes->utcFromDefault =TRUE_FLAG;
	}
	else if((rxGpsTime->referenceValid==TRUE_FLAG) && (rxGpsTime->referenceUpdated==TRUE_FLAG) && (pvtRes->utcFromGal==FALSE_FLAG))
	{
		rxUtcTime->referenceSample = referenceSample;
		FromGnssTimeToUtcTime(NULL,FALSE_FLAG,defaultLeapSeconds,rxGpsTime->referenceWn,rxGpsTime->referenceTow,&(rxUtcTime->referenceWn),&(rxUtcTime->referenceTow));
		rxUtcTime->referenceValid = TRUE_FLAG;
		rxUtcTime->referenceUpdated = TRUE_FLAG;
		pvtRes->utcFromGps = TRUE_FLAG;
		pvtRes->utcFromGal = FALSE_FLAG;
		pvtRes->utcFromDefault = TRUE_FLAG;
	}
}


void PredictRxTime(TNGeneConfiguration *cfg,TNGeneData *rx)
{
	unsigned long long int currentSample = rx->buf.discardedSamples;
	long long int deltaSample;
	double deltaTime;
	TPvtResults *pvtRes = &(rx->pvtResults);
	TPvtData *pvtData = &(rx->pvtData);
	TReceiverTime *rxTime;

	pvtData->rxUtcTimeValid=FALSE_FLAG;
	pvtData->rxGpsTimeValid=FALSE_FLAG;
	pvtData->rxGalTimeValid=FALSE_FLAG;

	rxTime = &(pvtRes->rxUtcTime);
	if(rxTime->referenceValid==TRUE_FLAG)
	{
		deltaSample = currentSample - rxTime->referenceSample;
		deltaTime = (double)deltaSample/cfg->fs;
		pvtData->rxUtcWn = rxTime->referenceWn;
		pvtData->rxUtcTow = rxTime->referenceTow + deltaTime + (deltaTime*(pvtRes->clockDrift/SPEED_OF_LIGHT));
		if(pvtData->rxUtcTow>SECONDS_IN_A_WEEK)
		{
			pvtData->rxUtcWn++;
			pvtData->rxUtcTow -= SECONDS_IN_A_WEEK;
		}
		pvtData->rxUtcTimeValid = TRUE_FLAG;
	}
	rxTime = &(pvtRes->rxGpsTime);
	if(rxTime->referenceValid==TRUE_FLAG)
	{
		deltaSample = currentSample - rxTime->referenceSample;
		deltaTime = (double)deltaSample/cfg->fs;
		pvtData->rxGpsWn = rxTime->referenceWn;
		pvtData->rxGpsTow = rxTime->referenceTow + deltaTime + (deltaTime*(pvtRes->clockDrift/SPEED_OF_LIGHT));
		if(pvtData->rxGpsTow>SECONDS_IN_A_WEEK)
		{
			pvtData->rxGpsWn++;
			pvtData->rxGpsTow -= SECONDS_IN_A_WEEK;
		}
		pvtData->rxGpsTimeValid = TRUE_FLAG;
	}
	rxTime = &(pvtRes->rxGalTime);
	if(rxTime->referenceValid==TRUE_FLAG)
	{
		deltaSample = currentSample - rxTime->referenceSample;
		deltaTime = (double)deltaSample/cfg->fs;
		pvtData->rxGalWn = rxTime->referenceWn;
		pvtData->rxGalTow = rxTime->referenceTow + deltaTime + (deltaTime*(pvtRes->clockDrift/SPEED_OF_LIGHT));
		if(pvtData->rxGalTow>SECONDS_IN_A_WEEK)
		{
			pvtData->rxGalWn++;
			pvtData->rxGalTow -= SECONDS_IN_A_WEEK;
		}
		pvtData->rxGalTimeValid = TRUE_FLAG;
	}
}
