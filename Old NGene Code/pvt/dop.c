/**
 * @file dop.c
 *
 * @author Mario Nicola
 *
 * @date 27/10/2014
 *
 * @version 1.0.0
 *
 * @brief DOP computation
 */

#include <math.h>
#include "pvtConfiguration.h"
#include "ngeneDataStructure.h"
#include "dop.h"
#include "matrices.h"
#include "pvtUtility.h"

int DopEvaluation(TNGeneData *rx,int performedPvt)
{
	TPvtData *pvtData = &(rx->pvtData);
	TLeastSquaresData *lsData = &(pvtData->lsData);
	TPvtChannel *pvtCh;
	TChannel *ch;
	TPvtResults *pvtRes = &(rx->pvtTemporaryResults);
	TSatStatus *satStatus;
	int nCh = pvtData->numOfOperativeChannels;
	int size;
	double *satPos;
	double pos[5];
	int chIdx;
	double rho2;
	double travelTime;

	/* prepare inputs */
	pos[0] = pvtRes->ecefPosition[0];
	pos[1] = pvtRes->ecefPosition[1];
	pos[2] = pvtRes->ecefPosition[2];
	/* single or double constellation?
	 * size is the number of columns in the A matrix
	 * and also the minimum number of satellites in order to proceed
	 * with DOP calculation */
	if((pvtData->numOfGpsOpChannels>0) && (pvtData->numOfGalileoOpChannels>0))
	{
		size = LS_MULTI_SIZE;
		pos[3] = pvtRes->clockBiasGps;
		pos[4] = pvtRes->clockBiasGalileo;
	}
	else
	{
		size = LS_SINGLE_SIZE;
		if(pvtData->numOfGpsOpChannels>0)
		{
			pos[3] = pvtRes->clockBiasGps;
		}
		else
		{
			pos[3] = pvtRes->clockBiasGalileo;
		}
	}
	/* the DOP cannot be evaluated */
	if((pvtRes->pvtValid==FALSE_FLAG) || (pvtData->numOfOperativeChannels<size))
	{
		return FALSE_FLAG;
	}
	/* If Kalman filter has been used, A matrix is initialized here,
	 * elsewhere, A matrix used during Least Square execution is already available */
	if(performedPvt==KALMAN_BASED_PVT)
	{
		for(chIdx=0;chIdx<nCh;chIdx++)
		{
			pvtCh = &(pvtData->pvtChArray[chIdx]);
			ch = pvtCh->ch;
			satStatus = pvtCh->satStatus;
			satPos = satStatus->ecefSatPos;
			lsData->x[(0*nCh)+chIdx] = satPos[0];
			lsData->x[(1*nCh)+chIdx] = satPos[1];
			lsData->x[(2*nCh)+chIdx] = satPos[2];
			rho2 = pow((lsData->x[(0*nCh)+chIdx] - pos[0]), 2.0) +
					pow((lsData->x[(1*nCh)+chIdx] - pos[1]), 2.0) +
					pow((lsData->x[(2*nCh)+chIdx] - pos[2]), 2.0);
			travelTime = sqrt(rho2)/ SPEED_OF_LIGHT;
			EarthCorrection(travelTime,lsData->x,lsData->xRotate,nCh,chIdx);
			// Build the A matrix
			lsData->a[(chIdx*size)+0] = -(lsData->xRotate[(0*nCh)+chIdx]-pos[0])/pvtCh->pseudorangeCsm;
			lsData->a[(chIdx*size)+1] = -(lsData->xRotate[(1*nCh)+chIdx]-pos[1])/pvtCh->pseudorangeCsm;
			lsData->a[(chIdx*size)+2] = -(lsData->xRotate[(2*nCh)+chIdx]-pos[2])/pvtCh->pseudorangeCsm;
			if(size==LS_MULTI_SIZE)
			{
				if(ch->satType==GPS_SAT)
				{
					lsData->a[(chIdx*size)+3] = 1.0;
					lsData->a[(chIdx*size)+4] = 0.0;
				}
				else
				{
					lsData->a[(chIdx*size)+3] = 0.0;
					lsData->a[(chIdx*size)+4] = 1.0;
				}
			}
			else
			{
				lsData->a[(chIdx*size)+3] = 1.0;
			}
		}
	}
	/* Elaborate A matrix and find DOPs */
	MatrixTrasp(lsData->a,nCh,size,lsData->at);
	MatrixProduct(lsData->at,size,nCh,lsData->a,nCh,size,lsData->atA);
	MatrixInv(lsData->atA,size,lsData->invAtA,lsData->matrixL,lsData->matrixD,lsData->matrixU,lsData->matrixUt,lsData->matrixIL,lsData->matrixID,lsData->matrixIU,lsData->matrixIUt,lsData->matrixIUD,lsData->matrixDet);
	if(size==LS_MULTI_SIZE)
	{
		/* double constellation */
		pvtRes->dop[GDOP_INDEX_IN_DOP_ARRAY]  = sqrt(lsData->invAtA[0] + lsData->invAtA[6] + lsData->invAtA[12] + lsData->invAtA[18] + lsData->invAtA[24]);
		pvtRes->dop[PDOP_INDEX_IN_DOP_ARRAY]  = sqrt(lsData->invAtA[0] + lsData->invAtA[6] + lsData->invAtA[12]);
		pvtRes->dop[HDOP_INDEX_IN_DOP_ARRAY]  = sqrt(lsData->invAtA[0] + lsData->invAtA[6]);
		pvtRes->dop[VDOP_INDEX_IN_DOP_ARRAY]  = sqrt(lsData->invAtA[12]);
		pvtRes->dop[TDOP_INDEX_IN_DOP_ARRAY]  = sqrt(lsData->invAtA[18] + lsData->invAtA[24]);
	}
	else
	{
		/* double constellation */
		pvtRes->dop[GDOP_INDEX_IN_DOP_ARRAY]  = sqrt(lsData->invAtA[0] + lsData->invAtA[5] + lsData->invAtA[10] + lsData->invAtA[15]);
		pvtRes->dop[PDOP_INDEX_IN_DOP_ARRAY]  = sqrt(lsData->invAtA[0] + lsData->invAtA[5] + lsData->invAtA[10]);
		pvtRes->dop[HDOP_INDEX_IN_DOP_ARRAY]  = sqrt(lsData->invAtA[0] + lsData->invAtA[5]);
		pvtRes->dop[VDOP_INDEX_IN_DOP_ARRAY]  = sqrt(lsData->invAtA[10]);
		pvtRes->dop[TDOP_INDEX_IN_DOP_ARRAY]  = sqrt(lsData->invAtA[15]);
	}
	return TRUE_FLAG;
}
