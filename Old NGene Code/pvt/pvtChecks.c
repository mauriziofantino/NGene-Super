/**
 * @file pvtChecks.c
 *
 * @author Mario Nicola
 *
 * @date 22/10/2014
 *
 * @version 1.0.0
 *
 * @brief Functions used to perform checks during PVT computation.
 */

#include <math.h>
#include "ngeneConfiguration.h"
#include "channelDataStructure.h"
#include "ngeneConstants.h"
#include "pvtDataStructure.h"
#include "pvtConfiguration.h"
#include "dop.h"
#include "ioFunctions.h"
#include "timeConstants.h"
#include "pvtChecks.h"


int CheckTow(TChannel* ch)
{
	/* Preliminary check */
	if(ch->satType==EGNOS_SAT)
	{
		return FALSE_FLAG;
	}
	if(ch->satId==INVALID_PRN)
	{
		return FALSE_FLAG;
	}
	/* TOW check */
	if(ch->satType==GPS_SAT)
	{
		if(ch->gpsParser->navMsg==NULL)
		{
			return FALSE_FLAG;
		}
		if(ch->gpsParser->navMsg->towValid==TRUE_FLAG)
		{
			return TRUE_FLAG;
		}
	}
	if(ch->satType==GALILEO_SAT)
	{
		if(ch->galParser->navMsg==NULL)
		{
			return FALSE_FLAG;
		}
		if(ch->galParser->navMsg->towValid==TRUE_FLAG)
		{
			return TRUE_FLAG;
		}
	}
	return FALSE_FLAG;
}


int CheckEphemerides(TChannel* ch)
{
	TNavigationMessage *navMsg;

	/* Preliminary check */
	if(ch->satType==EGNOS_SAT)
	{
		return FALSE_FLAG;
	}
	if(ch->satId==INVALID_PRN)
	{
		return FALSE_FLAG;
	}
	/* Consider availability of ephemerides, health and data validity (Galileo only)
	 * Accuracy information are ignored */
	if(ch->satType==GPS_SAT)
	{
		navMsg = ch->gpsParser->navMsg;
		if(navMsg==NULL)
		{
			return FALSE_FLAG;
		}
		if(navMsg->ephemeridesReceived==FALSE_FLAG)
		{
			return FALSE_FLAG;
		}
		if(navMsg->eph.health!=SIGNAL_OK)
		{
			return FALSE_FLAG;
		}
	}
	if(ch->satType==GALILEO_SAT)
	{
		navMsg = ch->galParser->navMsg;
		if(navMsg==NULL)
		{
			return FALSE_FLAG;
		}
		if(navMsg->ephemeridesReceived==FALSE_FLAG)
		{
			return FALSE_FLAG;
		}
		if(navMsg->eph.health!=SIGNAL_OK)
		{
			return FALSE_FLAG;
		}
		if(navMsg->eph.galileoDataValidity==WORKING_WITHOUT_GUARANTEE)
		{
			return FALSE_FLAG;
		}
	}
	return TRUE_FLAG;
}


int ReadyForPvt(TChannel *ch)
{
	if(ch->satType==EGNOS_SAT)
	{
		return FALSE_FLAG;
	}

	if((ch->chState==TRACKING_STATE) && (ch->tracking.trkStatus==TRACKING_LOCKED) && (CheckTow(ch)==TRUE_FLAG) && (CheckEphemerides(ch)==TRUE_FLAG))
	{
		return TRUE_FLAG;
	}

	return FALSE_FLAG;
}


/**
 * @def MAXIMUM_ANGLE_DEGREE
 * @brief PVT results where latitude or longitude is beyond 360 deg are obvious outlier
 */
#define MAXIMUM_ANGLE_DEGREE	360.0

/**
 * @def MAXIMUM_ALTITUDE_METERS
 * @brief PVT results where altitude is beyond 1000 km are suspicious
 */
#define MAXIMUM_ALTITUDE_METERS 1.0e6

/**
 * @def MAXIMUM_GDOP_JUMP_FOR_CHECKS
 * #brief If the GDOP changes, the check on the position jumps is not meaningful: avoid it
 */
#define MAXIMUM_GDOP_JUMP_FOR_CHECKS	5.0

int CheckPvtResultsGoodness(TNGeneConfiguration *cfg,TPvtData *pvtData,TPvtResults *pvtResNew,TPvtResults *pvtResOld,int *failure)
{
	TPvtConfiguration *pvtCfg = &(cfg->pvtConfiguration);
	double maxGdop = (double)(cfg->pvtConfiguration.maxGdop);
	int idx;
	double deltaTime;
	double deltaTimeFromSamples;
	double deltaPos;
	double vel;
	double deltaPosFromVel;

	// init
	*failure = TRUE_FLAG;

	/* first gross test */
	// position
	if((isnan(pvtResNew->geodPosition[0])>0) || (isinf(pvtResNew->geodPosition[0])>0) || (fabs(pvtResNew->geodPosition[0])>MAXIMUM_ANGLE_DEGREE))
	{
		SwrxLog(NGENE_LOG_SEVERITY_WARNING,"Large error in the estimated longitude (%f deg)",pvtResNew->geodPosition[0]);
		return FALSE_FLAG;
	}
	if((isnan(pvtResNew->geodPosition[1])>0) || (isinf(pvtResNew->geodPosition[1])>0) || (fabs(pvtResNew->geodPosition[1])>MAXIMUM_ANGLE_DEGREE))
	{
		SwrxLog(NGENE_LOG_SEVERITY_WARNING,"Large error in the estimated latitude (%f deg)",pvtResNew->geodPosition[1]);
		return FALSE_FLAG;
	}
	if((isnan(pvtResNew->geodPosition[2])>0) || (isinf(pvtResNew->geodPosition[2])>0) || (fabs(pvtResNew->geodPosition[2])>MAXIMUM_ALTITUDE_METERS))
	{
		SwrxLog(NGENE_LOG_SEVERITY_WARNING,"Large error in the estimated altitude (%f m)",pvtResNew->geodPosition[2]);
		return FALSE_FLAG;
	}
	// velocity
	for(idx=0;idx<3;idx++)
	{
		if((isnan(pvtResNew->ecefVelocity[idx])>0) || (isinf(pvtResNew->ecefVelocity[idx])>0))
		{
			SwrxLog(NGENE_LOG_SEVERITY_WARNING,"Large error in the estimated  velocity");
			return FALSE_FLAG;
		}
	}
	// DOP
	for(idx=0;idx<5;idx++)
	{
		if((isnan(pvtResNew->dop[idx])>0) || (isinf(pvtResNew->dop[idx])>0))
		{
			SwrxLog(NGENE_LOG_SEVERITY_WARNING,"Large error in the estimated DOP");
			return FALSE_FLAG;
		}
	}

	/* Exclude PVT results when GDOP is over the user-chosen limit
	 * But it is NOT a failure! */
	if((pvtData->pvtAlgorithm==LMS_BASED_PVT) && (pvtResNew->dop[GDOP_INDEX_IN_DOP_ARRAY]>maxGdop))
	{
		*failure = FALSE_FLAG;
		SwrxLog(NGENE_LOG_SEVERITY_WARNING,"PVT results discarded due to a bad GDOP");
		return FALSE_FLAG;
	}

	/* In the case of a hard restart, be lenient with checks */
	if(pvtData->hardRestartMade==TRUE_FLAG)
	{
		// is good
		*failure = FALSE_FLAG;
		return TRUE_FLAG;
	}

	/* Complete checks follow */
	/* Analyze the receiver time (coherence between receiver time estimated before and after PVT) */
	if((pvtData->numOfGpsOpChannels>0) && (pvtResNew->rxGpsTime.referenceValid==TRUE_FLAG) && (pvtData->rxGpsTimeValid==TRUE_FLAG))
	{
		deltaTime = (pvtData->rxGpsWn - pvtResNew->rxGpsTime.referenceWn)*SECONDS_IN_A_WEEK;
		deltaTime += (pvtData->rxGpsTow-pvtResNew->rxGpsTime.referenceTow);
		deltaTime = fabs(deltaTime);
		if(deltaTime>PVT_MAXIMUM_TIME_ERROR)
		{
			SwrxLog(NGENE_LOG_SEVERITY_WARNING,"PVT results discarded due to bad receiver GPS time estimation (discrepancy with the predicted time)");
			return FALSE_FLAG;
		}
	}
	if((pvtData->numOfGalileoOpChannels>0) && (pvtResNew->rxGalTime.referenceValid==TRUE_FLAG) && (pvtData->rxGalTimeValid==TRUE_FLAG))
	{
		deltaTime = (pvtData->rxGalWn-pvtResNew->rxGalTime.referenceWn)*SECONDS_IN_A_WEEK;
		deltaTime += (pvtData->rxGalTow-pvtResNew->rxGalTime.referenceTow);
		deltaTime += fabs(deltaTime);
		if(deltaTime>PVT_MAXIMUM_TIME_ERROR)
		{
			SwrxLog(NGENE_LOG_SEVERITY_WARNING,"PVT results discarded due to bad receiver Galileo time estimation (discrepancy with the predicted time)");
			return FALSE_FLAG;
		}
	}
	/* Analyze the reference time (backward time travels are not allowed!) */
	if((pvtData->numOfGpsOpChannels>0) && (pvtResNew->rxGpsTime.referenceValid==TRUE_FLAG) && (pvtResOld->rxGpsTime.referenceValid==TRUE_FLAG))
	{
		if(pvtResNew->rxGpsTime.referenceWn<pvtResOld->rxGpsTime.referenceWn)
		{
			SwrxLog(NGENE_LOG_SEVERITY_WARNING,"PVT results discarded due to bad receiver GPS time estimation (new time in the past)");
			return FALSE_FLAG;
		}
		if((pvtResNew->rxGpsTime.referenceWn==pvtResOld->rxGpsTime.referenceWn) && (pvtResNew->rxGpsTime.referenceTow<pvtResOld->rxGpsTime.referenceTow))
		{
			SwrxLog(NGENE_LOG_SEVERITY_WARNING,"PVT results discarded due to bad receiver GPS time estimation (new time in the past)");
			return FALSE_FLAG;
		}
	}
	if((pvtData->numOfGalileoOpChannels>0) && (pvtResNew->rxGalTime.referenceValid==TRUE_FLAG) && (pvtResOld->rxGalTime.referenceValid==TRUE_FLAG))
	{
		if(pvtResNew->rxGalTime.referenceWn<pvtResOld->rxGalTime.referenceWn)
		{
			SwrxLog(NGENE_LOG_SEVERITY_WARNING,"PVT results discarded due to bad receiver Galileo time estimation (new time in the past)");
			return FALSE_FLAG;
		}
		if((pvtResNew->rxGalTime.referenceWn==pvtResOld->rxGalTime.referenceWn) && (pvtResNew->rxGalTime.referenceTow < pvtResOld->rxGalTime.referenceTow))
		{
			SwrxLog(NGENE_LOG_SEVERITY_WARNING,"PVT results discarded due to bad receiver Galileo time estimation (new time in the past)");
			return FALSE_FLAG;
		}
	}
	/* Check coherence of reference times with respect to the number of elaborated samples */
	if((pvtResNew->rxGpsTime.referenceValid==TRUE_FLAG) && (pvtResOld->rxGpsTime.referenceValid==TRUE_FLAG))
	{
		deltaTime = (pvtResNew->rxGpsTime.referenceWn-pvtResOld->rxGpsTime.referenceWn)*SECONDS_IN_A_WEEK;
		deltaTime += (pvtResNew->rxGpsTime.referenceTow-pvtResOld->rxGpsTime.referenceTow);
		deltaTime = fabs(deltaTime);
		deltaTimeFromSamples = fabs((double)(pvtResNew->rxGpsTime.referenceSample - pvtResOld->rxGpsTime.referenceSample))/cfg->fs;
		if(fabs(deltaTime-deltaTimeFromSamples)>PVT_MAXIMUM_TIME_ERROR)
		{
			SwrxLog(NGENE_LOG_SEVERITY_WARNING,"PVT results discarded due to bad receiver GPS time estimation (discrepancy with the sample count)");
			return FALSE_FLAG;
		}
	}
	if((pvtResNew->rxGalTime.referenceValid==TRUE_FLAG) && (pvtResOld->rxGalTime.referenceValid==TRUE_FLAG))
	{
		deltaTime = (pvtResNew->rxGalTime.referenceWn-pvtResOld->rxGalTime.referenceWn)*SECONDS_IN_A_WEEK;
		deltaTime += (pvtResNew->rxGalTime.referenceTow-pvtResOld->rxGalTime.referenceTow);
		deltaTime = fabs(deltaTime);
		deltaTimeFromSamples = fabs((double)(pvtResNew->rxGalTime.referenceSample - pvtResOld->rxGalTime.referenceSample))/cfg->fs;
		if(fabs(deltaTime-deltaTimeFromSamples)>PVT_MAXIMUM_TIME_ERROR)
		{
			SwrxLog(NGENE_LOG_SEVERITY_WARNING,"PVT results discarded due to bad receiver Galileo time estimation (discrepancy with the sample count)");
			return FALSE_FLAG;
		}
	}
	/* Check position jumps */
	if((pvtResOld->pvtValid==TRUE_FLAG) && (fabs(pvtResNew->dop[0]-pvtResOld->dop[0])<MAXIMUM_GDOP_JUMP_FOR_CHECKS))
	{
		// Do not perform the check when the GDOP change is beyond a threshold
		if(pvtCfg->staticReceiver==TRUE_FLAG)
		{
			// Check for static receiver
			deltaPos =  pow(pvtResNew->ecefPosition[0]-pvtResOld->ecefPosition[0],2.0);
			deltaPos += pow(pvtResNew->ecefPosition[1]-pvtResOld->ecefPosition[1],2.0);
			deltaPos += pow(pvtResNew->ecefPosition[2]-pvtResOld->ecefPosition[2],2.0);
			deltaPos = sqrt(deltaPos);
			if(deltaPos>PVT_MAXIMUM_POSITION_JUMP)
			{
				SwrxLog(NGENE_LOG_SEVERITY_WARNING,"PVT results discarded due to bad position estimation (%f m jump w.r.t. the old position)",deltaPos);
				return FALSE_FLAG;
			}
		}
		else
		{
			// Check for dynamic receiver
			deltaPos =  pow(pvtResNew->ecefPosition[0]-pvtResOld->ecefPosition[0],2.0);
			deltaPos += pow(pvtResNew->ecefPosition[1]-pvtResOld->ecefPosition[1],2.0);
			deltaPos += pow(pvtResNew->ecefPosition[2]-pvtResOld->ecefPosition[2],2.0);
			deltaPos = sqrt(deltaPos);
			//
			vel = pow(pvtResOld->ecefVelocity[0],2.0);
			vel += pow(pvtResOld->ecefVelocity[1],2.0);
			vel += pow(pvtResOld->ecefVelocity[2],2.0);
			vel = sqrt(deltaPos);
			deltaTime = ((double)(pvtResNew->referenceSample-pvtResOld->referenceSample))/cfg->fs;
			deltaPosFromVel = vel * deltaTime;
			//
			deltaPos = fabs(deltaPos-deltaPosFromVel);
			if(deltaPos>PVT_MAXIMUM_POSITION_JUMP)
			{
				SwrxLog(NGENE_LOG_SEVERITY_WARNING,"PVT results discarded due to bad position estimation (%f m jump w.r.t. the position predicted through velocity)");
				return FALSE_FLAG;
			}
		}
	}

	/* The PVT is good or less relevant errors occurred */
	*failure = FALSE_FLAG;

	return TRUE_FLAG;
}
