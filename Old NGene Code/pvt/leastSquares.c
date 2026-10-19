/**
 * @file leastSquares.c
 *
 * @author Mario Nicola
 *
 * @date 24/10/2014
 *
 * @version 1.0.0
 *
 * @brief Least squares functions.
 */

#include <stdlib.h>
#include <math.h>
#include "ngeneDataStructure.h"
#include "ngeneConfiguration.h"
#include "pvtDataStructure.h"
#include "ngeneConstants.h"
#include "ioFunctions.h"
#include "closeManager.h"
#include "matrices.h"
#include "pvtUtility.h"


int InitLeastSquaresData(TNGeneConfiguration *cfg,TNGeneData *rx)
{
	int n = cfg->numberOfChannels;
	TLeastSquaresData *lsData = &(rx->pvtData.lsData);
	int errors = 0;

	lsData->a = (double *)malloc(n*LS_MULTI_SIZE*sizeof(double));
	if(lsData->a==NULL)
		errors++;
	lsData->at = (double *)malloc(LS_MULTI_SIZE*n*sizeof(double));
	if(lsData->at==NULL)
		errors++;
	lsData->ac = (double *)malloc(n*4*sizeof(double));
	if(lsData->ac==NULL)
		errors++;
	lsData->act = (double *)malloc(4*n*sizeof(double));
	if(lsData->act==NULL)
		errors++;
	lsData->satRadialVelAndDoppler = (double *)malloc(n*1*sizeof(double));
	if(lsData->satRadialVelAndDoppler==NULL)
		errors++;
	lsData->x = (double *)malloc(3*n*sizeof(double));
	if(lsData->x==NULL)
		errors++;
	lsData->xRotate = (double *)malloc(3*n*sizeof(double));
	if(lsData->xRotate==NULL)
		errors++;
	lsData->b = (double *)malloc(n*1*sizeof(double));
	if(lsData->b==NULL)
		errors++;
	lsData->az = (double *)malloc(1*n*sizeof(double));
	if(lsData->az==NULL)
		errors++;
	lsData->el = (double *)malloc(1*n*sizeof(double));
	if(lsData->el==NULL)
		errors++;

	if(errors>0)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Malloc returned a NULL pointer in InitLeastSquaresData");
		CloseNGene(cfg,EXIT_FAILURE);
	}

	return SUCCESS_RETURN;
}


void ClearLeastSquaresData(TNGeneConfiguration *cfg,TLeastSquaresData *lsData)
{
	int n = cfg->numberOfChannels;

	MatrixInit(lsData->a,n,LS_MULTI_SIZE);
	MatrixInit(lsData->at,LS_MULTI_SIZE,n);
	MatrixInit(lsData->atA,LS_MULTI_SIZE,LS_MULTI_SIZE);
	MatrixInit(lsData->invAtA,LS_MULTI_SIZE,LS_MULTI_SIZE);
	MatrixInit(lsData->atB,LS_MULTI_SIZE,1);
	MatrixInit(lsData->pos,LS_MULTI_SIZE,1);
	MatrixInit(lsData->sol,LS_MULTI_SIZE,1);
	MatrixInit(lsData->ac,n,4);
	MatrixInit(lsData->act,4,n);
	MatrixInit(lsData->actAc,4,4);
	MatrixInit(lsData->invActAc,4,4);
	MatrixInit(lsData->actB,4,1);
	MatrixInit(lsData->sol2,4,1);
	MatrixInit(lsData->userVel,4,1);
	MatrixInit(lsData->satVel,3,1);
	MatrixInit(lsData->distVect,1,3);
	lsData->distance = 0.0;
	MatrixInit(lsData->satRadialVelAndDoppler,n,1);
	MatrixInit(lsData->x,3,n);
	MatrixInit(lsData->xRotate,3,n);
	MatrixInit(lsData->b,n,1);
	MatrixInit(lsData->az,1,n);
	MatrixInit(lsData->el,1,n);
	MatrixInit(lsData->matrixL,LS_MULTI_SIZE,LS_MULTI_SIZE);
	MatrixInit(lsData->matrixD,LS_MULTI_SIZE,LS_MULTI_SIZE);
	MatrixInit(lsData->matrixU,LS_MULTI_SIZE,LS_MULTI_SIZE);
	MatrixInit(lsData->matrixUt,LS_MULTI_SIZE,LS_MULTI_SIZE);
	MatrixInit(lsData->matrixIL,LS_MULTI_SIZE,LS_MULTI_SIZE);
	MatrixInit(lsData->matrixID,LS_MULTI_SIZE,LS_MULTI_SIZE);
	MatrixInit(lsData->matrixIU,LS_MULTI_SIZE,LS_MULTI_SIZE);
	MatrixInit(lsData->matrixIUt,LS_MULTI_SIZE,LS_MULTI_SIZE);
	MatrixInit(lsData->matrixIUD,LS_MULTI_SIZE,LS_MULTI_SIZE);
	MatrixInit(lsData->matrixDet,LS_MULTI_SIZE,LS_MULTI_SIZE);
	MatrixInit(lsData->dop,5,5);
	MatrixInit(lsData->q,5,5);
}


void DeinitLeastSquaresData(TNGeneData *rx)
{
	TLeastSquaresData *lsData = &(rx->pvtData.lsData);

	free(lsData->a);
	free(lsData->at);
	free(lsData->ac);
	free(lsData->act);
	free(lsData->satRadialVelAndDoppler);
	free(lsData->x);
	free(lsData->xRotate);
	free(lsData->b);
	free(lsData->az);
	free(lsData->el);
}


int PerformLeastSquares(TNGeneConfiguration *cfg,TNGeneData *rx)
{
	TPvtData *pvtData = &(rx->pvtData);
	TLeastSquaresData *lsData = &(pvtData->lsData);
	TPvtChannel *pvtCh;
	TPvtResults *pvtRes = &(rx->pvtTemporaryResults);
	TChannel *ch;
	TSatStatus *satStatus;
	int curSize;
	int idxGpsBias;
	int idxGalBias;
	int iter;
	int chIdx;
	int nCh = pvtData->numOfOperativeChannels;
	double rho2 = 0;
	double travelTime = 0;
	double topoXRotate[3*1];
	double geomRange;
	double aCorrectRow[1*3];
	double prod;

	/* Initializations */
	ClearLeastSquaresData(cfg,lsData);
	if(pvtData->pvtIndex>0)
	{
		lsData->pos[0]=pvtRes->ecefPosition[0];
		lsData->pos[1]=pvtRes->ecefPosition[1];
		lsData->pos[2]=pvtRes->ecefPosition[2];
	}
	if((pvtData->numOfGpsOpChannels>0) && (pvtData->numOfGalileoOpChannels>0))
	{
		curSize = LS_MULTI_SIZE;
		idxGpsBias = 3;
		idxGalBias = 4;
	}
	else
	{
		curSize = LS_SINGLE_SIZE;
		idxGpsBias = 3;
		idxGalBias = 3;
	}
	for(chIdx=0;chIdx<nCh;chIdx++)
	{
		satStatus = pvtData->pvtChArray[chIdx].satStatus;
		lsData->x[(0*nCh) + chIdx] = satStatus->ecefSatPos[0];
		lsData->x[(1*nCh) + chIdx] = satStatus->ecefSatPos[1];
		lsData->x[(2*nCh) + chIdx] = satStatus->ecefSatPos[2];
	}
	/* Iterative computation of the position and clock bias (biases, in the multiple constellation case) */
	for(iter=0;iter<LS_ITERATIONS;iter++)
	{
		for(chIdx=0;chIdx<nCh;chIdx++)
		{
			pvtCh = &(pvtData->pvtChArray[chIdx]);
			ch = pvtCh->ch;
			satStatus = pvtCh->satStatus;
			if(iter==0)
			{
				lsData->xRotate[(0*nCh) + chIdx] = lsData->x[(0*nCh) + chIdx];
				lsData->xRotate[(1*nCh) + chIdx] = lsData->x[(1*nCh) + chIdx];
				lsData->xRotate[(2*nCh) + chIdx] = lsData->x[(2*nCh) + chIdx];
				pvtCh->tropoCorrection = 0.0; /* PROSA: 2.0 */
			}
			else
			{
				rho2 = pow((lsData->x[(0*nCh)+chIdx] - lsData->pos[0]), 2.0) +
					   pow((lsData->x[(1*nCh)+chIdx] - lsData->pos[1]), 2.0) +
					   pow((lsData->x[(2*nCh)+chIdx] - lsData->pos[2]), 2.0);
				travelTime = sqrt(rho2)/ SPEED_OF_LIGHT;
				EarthCorrection(travelTime,lsData->x,lsData->xRotate,nCh,chIdx);
				topoXRotate[0] = lsData->xRotate[(0*nCh)+chIdx];
				topoXRotate[1] = lsData->xRotate[(1*nCh)+chIdx];
				topoXRotate[2] = lsData->xRotate[(2*nCh)+chIdx];
				satStatus->ecefSatPosRot[0] = topoXRotate[0];
				satStatus->ecefSatPosRot[1] = topoXRotate[1];
				satStatus->ecefSatPosRot[2] = topoXRotate[2];
				Topocent(lsData->pos,topoXRotate,lsData->el);
				satStatus->elevation = lsData->el[0] * DEGREE_TO_RADIAN;
				satStatus->azimuth = lsData->el[1] * DEGREE_TO_RADIAN;
				pvtCh->tropoCorrection = Tropo(sin(satStatus->elevation), 0.0, 1013.0, 293.0, 50.0, 0.0, 0.0, 0.0);
			}
			/* Calculate geometric range */
			geomRange = sqrt((lsData->xRotate[(0*nCh)+chIdx]-lsData->pos[0]) * (lsData->xRotate[(0*nCh)+chIdx]-lsData->pos[0]) +
							 (lsData->xRotate[(1*nCh)+chIdx]-lsData->pos[1]) * (lsData->xRotate[(1*nCh)+chIdx]-lsData->pos[1]) +
							 (lsData->xRotate[(2*nCh)+chIdx]-lsData->pos[2]) * (lsData->xRotate[(2*nCh)+chIdx]-lsData->pos[2]));
			/* Build the A matrix */
			lsData->a[(curSize*chIdx)+0] = -(lsData->xRotate[(0*nCh)+chIdx] - lsData->pos[0] ) / pvtCh->pseudorangeCsm;
			lsData->a[(curSize*chIdx)+1] = -(lsData->xRotate[(1*nCh)+chIdx] - lsData->pos[1] ) / pvtCh->pseudorangeCsm;
			lsData->a[(curSize*chIdx)+2] = -(lsData->xRotate[(2*nCh)+chIdx] - lsData->pos[2] ) / pvtCh->pseudorangeCsm;
			lsData->a[(curSize*chIdx)+idxGpsBias]=0.0; /* TODO dovrebbe già essere zero: verificare! */
			lsData->a[(curSize*chIdx)+idxGalBias]=0.0; /* TODO dovrebbe già essere zero: verificare! */
			if(ch->satType==GPS_SAT)
			{
				lsData->b[chIdx] = pvtCh->pseudorangeCsm - geomRange - lsData->pos[idxGpsBias] - pvtCh->tropoCorrection - pvtCh->ionoCorrection;
				lsData->a[(curSize*chIdx)+idxGpsBias]=1.0;
			}
			else /* if(ch->satType==GALILEO_SAT) */
			{
				lsData->b[chIdx] = pvtCh->pseudorangeCsm - geomRange - lsData->pos[idxGalBias] - pvtCh->tropoCorrection - pvtCh->ionoCorrection;
				lsData->a[(curSize*chIdx)+idxGalBias]=1.0;
			}
		}
		/* Calculate the solution of the AX= B system with the formula x = inv(A'A) * (A'B) */
		MatrixTrasp(lsData->a,nCh,curSize,lsData->at);
		MatrixProduct(lsData->at,curSize,nCh,lsData->a,nCh,curSize,lsData->atA);
		MatrixInv(lsData->atA,curSize,lsData->invAtA,lsData->matrixL,lsData->matrixD,lsData->matrixU,
				  lsData->matrixUt,lsData->matrixIL,lsData->matrixID,lsData->matrixIU,lsData->matrixIUt,lsData->matrixIUD,lsData->matrixDet);
		MatrixProduct(lsData->at,curSize,nCh,lsData->b,nCh,1,lsData->atB);
		MatrixProduct(lsData->invAtA,curSize,curSize,lsData->atB,curSize,1,lsData->sol);
		MatrixSum(lsData->pos,curSize,1,lsData->sol,curSize,1,lsData->pos);
	}
	/* Compute the velocity and the clock drift */
	for(chIdx=0;chIdx<nCh;chIdx++)
	{
		pvtCh = &(pvtData->pvtChArray[chIdx]);
		satStatus = pvtCh->satStatus;
		lsData->distVect[0] = lsData->xRotate[(0*nCh)+chIdx] - lsData->pos[0];
		lsData->distVect[1] = lsData->xRotate[(1*nCh)+chIdx] - lsData->pos[1];
		lsData->distVect[2] = lsData->xRotate[(2*nCh)+chIdx] - lsData->pos[2];
		MatrixProduct(lsData->distVect,1,3,lsData->distVect,3,1,&(lsData->distance));
		lsData->distance = sqrt(lsData->distance);
		aCorrectRow[0] = lsData->ac[(4*chIdx)+0] = lsData->distVect[0]/lsData->distance;
		aCorrectRow[1] = lsData->ac[(4*chIdx)+1] = lsData->distVect[1]/lsData->distance;
		aCorrectRow[2] = lsData->ac[(4*chIdx)+2] = lsData->distVect[2]/lsData->distance;
		lsData->ac[(4*chIdx)+3] = 1.0;
		lsData->satVel[0] = satStatus->ecefSatVel[0];
		lsData->satVel[1] = satStatus->ecefSatVel[1];
		lsData->satVel[2] = satStatus->ecefSatVel[2];
		MatrixProduct(aCorrectRow,1,3,lsData->satVel,3,1,&prod);
		lsData->satRadialVelAndDoppler[chIdx] = SPEED_OF_LIGHT / L1_FREQUENCY * pvtCh->deltaPseudorange + prod;
	}
	MatrixTrasp(lsData->ac,nCh,4,lsData->act);
	MatrixProduct(lsData->act,4,nCh,lsData->ac,nCh,4,lsData->actAc);
	MatrixInv(lsData->actAc,4,lsData->invActAc,lsData->matrixL,lsData->matrixD,lsData->matrixU,
			  lsData->matrixUt,lsData->matrixIL,lsData->matrixID,lsData->matrixIU,lsData->matrixIUt,lsData->matrixIUD,lsData->matrixDet);
	MatrixProduct(lsData->act,4,nCh,lsData->satRadialVelAndDoppler,nCh,1,lsData->actB);
	MatrixProduct(lsData->invActAc,4,4,lsData->actB,4,1,lsData->userVel);
	/* Save the results */
	pvtRes->ecefPosition[0] = lsData->pos[0];
	pvtRes->ecefPosition[1] = lsData->pos[1];
	pvtRes->ecefPosition[2] = lsData->pos[2];
	pvtRes->clockBiasGps = lsData->pos[idxGpsBias];
	pvtRes->clockBiasGalileo = lsData->pos[idxGalBias];
	pvtRes->ecefVelocity[0] = lsData->userVel[0];
	pvtRes->ecefVelocity[1] = lsData->userVel[1];
	pvtRes->ecefVelocity[2] = lsData->userVel[2];
	pvtRes->clockDrift = lsData->userVel[3];
	if(pvtData->numOfGpsOpChannels>0)
	{
		pvtRes->clockBiasGpsValid=TRUE_FLAG;
	}
	else
	{
		pvtRes->clockBiasGpsValid=FALSE_FLAG;
	}
	if(pvtData->numOfGalileoOpChannels>0)
	{
		pvtRes->clockBiasGalileoValid=TRUE_FLAG;
	}
	else
	{
		pvtRes->clockBiasGalileoValid=FALSE_FLAG;
	}
	/* correct the atmospheric errors in pseudorangeCsm (for the log) */
	for(chIdx=0;chIdx<nCh;chIdx++)
	{
		pvtCh = &(pvtData->pvtChArray[chIdx]);
		pvtCh->pseudorangeCsm += (-pvtCh->tropoCorrection - pvtCh->ionoCorrection);
	}
	return SUCCESS_RETURN;
}
