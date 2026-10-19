/**
 * @file kalmanFilter.c
 *
 * @author Mario Nicola
 *
 * @date 3/11/2014
 *
 * @version 1.0.0
 *
 * @brief Kalman filter functions.
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

void InitKalmanFilterPhi(double *phi,double *phiMulti,double dt)
{
	int idx;

	/* Initialize phi matrices for Kalman filter */
	MatrixInit(phi,KF_SINGLE_STATES,KF_SINGLE_STATES);
	for(idx=0;idx<KF_SINGLE_STATES;idx++)
	{
		phi[(idx*KF_SINGLE_STATES)+idx] = 1.0;
		if(idx<3)
		{
			phi[(idx*KF_SINGLE_STATES)+idx+3] = dt;
		}
		if(idx==6)
		{
			phi[(idx*KF_SINGLE_STATES)+idx+1] = dt;
		}
	}
	MatrixInit(phiMulti,KF_MULTI_STATES,KF_MULTI_STATES);
	for(idx=0;idx<KF_MULTI_STATES;idx++)
	{
		phiMulti[(idx*KF_MULTI_STATES)+idx] = 1.0;
		if(idx<3)
		{
			phiMulti[(idx*KF_MULTI_STATES)+idx+3] = dt;
		}
		if(idx==6)
		{
			phiMulti[(idx*KF_MULTI_STATES)+idx+1] = dt;
		}
		if(idx==8)
		{
			phiMulti[(idx*KF_MULTI_STATES)+idx-1] = dt;
		}
	}
}

void InitKalmanFilterPostCovarianceMatrices(double *pPost,double *pPostMulti)
{
	/* Initialize  P a posteriori covariance matrices for Kalman filter */
	MatrixInit(pPost,KF_SINGLE_STATES,KF_SINGLE_STATES);
	pPost[0] = 100.0;
	pPost[9] = 100.0;
	pPost[18] = 100.0;
	pPost[27] = 10.0;
	pPost[36] = 10.0;
	pPost[45] = 10.0;
	pPost[54] = 1000.0;
	pPost[63] = 1.0;
	MatrixInit(pPostMulti,KF_MULTI_STATES,KF_MULTI_STATES);
	pPostMulti[0] = 100.0;
	pPostMulti[10] = 100.0;
	pPostMulti[20] = 100.0;
	pPostMulti[30] = 10.0;
	pPostMulti[40] = 10.0;
	pPostMulti[50] = 10.0;
	pPostMulti[60] = 1000.0;
	pPostMulti[70] = 1.0;
	pPostMulti[80] = 1000.0;
}

void InitQStateNoiseCovarianceMatrices(double *q,double *qMulti, double dt)
{
	/* Initialize Q noise covariance matrices for Kalman filter */
	MatrixInit(q,KF_SINGLE_STATES,KF_SINGLE_STATES);
	q[0] = 1e3*(KF_SP/3)*pow(dt,3.0);
	q[9] = 1e3*(KF_SP/3)*pow(dt,3.0);
	q[18] = 1e3*(KF_SP/3)*pow(dt,3.0);
	q[27] = 1e2*KF_SP*dt;
	q[36] = 1e2*KF_SP*dt;
	q[45] = 1e2*KF_SP*dt;
	q[54] = 1e2*((KF_SF*dt)+((KF_SG/2)*pow(dt,3.0)));
	q[63] = 1e2*(KF_SG)*dt;
	q[3] = q[12] = q[21] = q[24] = q[33] = q[42] = (KF_SP/2)*pow(dt,2.0);
	q[55] = q[62] = (KF_SG/2)*pow(dt,2.0);
	MatrixInit(qMulti,KF_MULTI_STATES,KF_MULTI_STATES);
	qMulti[0] = 1e3*(KF_SP/3)*pow(dt,3.0);
	qMulti[10] = 1e3*(KF_SP/3)*pow(dt,3.0);
	qMulti[20] = 1e3*(KF_SP/3)*pow(dt,3.0);
	qMulti[30] = 1e2*KF_SP*dt;
	qMulti[40] = 1e2*KF_SP*dt;
	qMulti[50] = 1e2*KF_SP*dt;
	qMulti[60] = 1e2*((KF_SF*dt)+((KF_SG/2)*pow(dt,3.0)));
	qMulti[70] = 1e2*(KF_SG)*dt;
	qMulti[80] = 1e2*((KF_SF*dt)+((KF_SG/2)*pow(dt,3.0)));
	qMulti[3] = qMulti[13] = qMulti[23] = qMulti[27] = qMulti[37] = qMulti[47] = (KF_SP/2)*pow(dt,2.0);
	qMulti[69] = qMulti[71] = (KF_SG/2)*pow(dt,2.0);
	qMulti[61] = qMulti[79] = (KF_SG/2)*pow(dt,2.0);
}


int InitKalmanFilterData(TNGeneConfiguration *cfg,TNGeneData *rx)
{
	int nCh = cfg->numberOfChannels;
	int nCh2 = 2*nCh;
	TKalmanFilterData *kfData = &(rx->pvtData.kfData);
	int errors = 0;

	kfData->aaa = (double *)malloc(1*nCh*sizeof(double));
	if(kfData->aaa==NULL)
		errors++;
	kfData->az = (double *)malloc(1*nCh*sizeof(double));
	if(kfData->az==NULL)
		errors++;
	kfData->el = (double *)malloc(1*nCh*sizeof(double));
	if(kfData->el==NULL)
		errors++;
	kfData->r = (double *)malloc(nCh2*nCh2*sizeof(double));
	if(kfData->r==NULL)
		errors++;
	kfData->range = (double *)malloc(nCh*1*sizeof(double));
	if(kfData->range==NULL)
		errors++;
	kfData->deltaRange = (double *)malloc(nCh*1*sizeof(double));
	if(kfData->deltaRange==NULL)
		errors++;
	kfData->u = (double *)malloc(nCh*KF_MULTI_SIZE*sizeof(double));
	if(kfData->u==NULL)
		errors++;
	kfData->pr = (double *)malloc(nCh*1*sizeof(double));
	if(kfData->pr==NULL)
		errors++;
	kfData->prCorr = (double *)malloc(nCh*1*sizeof(double));
	if(kfData->prCorr==NULL)
		errors++;
	kfData->deltaPrCorr = (double *)malloc(nCh*1*sizeof(double));
	if(kfData->deltaPrCorr==NULL)
		errors++;
	kfData->zCurr = (double *)malloc(nCh2*1*sizeof(double));
	if(kfData->zCurr==NULL)
		errors++;
	kfData->nominalMeasurement = (double *)malloc(nCh2*1*sizeof(double));
	if(kfData->nominalMeasurement==NULL)
		errors++;
	kfData->kfMeasurement = (double *)malloc(nCh2*1*sizeof(double));
	if(kfData->kfMeasurement==NULL)
		errors++;
	kfData->h = (double *)malloc(nCh2*KF_MULTI_STATES*sizeof(double));
	if(kfData->h==NULL)
		errors++;
	kfData->matrixL = (double *)malloc(nCh2*nCh2*sizeof(double));
	if(kfData->matrixL==NULL)
		errors++;
	kfData->matrixD = (double *)malloc(nCh2*nCh2*sizeof(double));
	if(kfData->matrixD==NULL)
		errors++;
	kfData->matrixU = (double *)malloc(nCh2*nCh2*sizeof(double));
	if(kfData->matrixU==NULL)
		errors++;
	kfData->matrixUt = (double *)malloc(nCh2*nCh2*sizeof(double));
	if(kfData->matrixUt==NULL)
		errors++;
	kfData->matrixIL = (double *)malloc(nCh2*nCh2*sizeof(double));
	if(kfData->matrixIL==NULL)
		errors++;
	kfData->matrixID = (double *)malloc(nCh2*nCh2*sizeof(double));
	if(kfData->matrixID==NULL)
		errors++;
	kfData->matrixIU = (double *)malloc(nCh2*nCh2*sizeof(double));
	if(kfData->matrixIU==NULL)
		errors++;
	kfData->matrixIUt = (double *)malloc(nCh2*nCh2*sizeof(double));
	if(kfData->matrixIUt==NULL)
		errors++;
	kfData->matrixIUD = (double *)malloc(nCh2*nCh2*sizeof(double));
	if(kfData->matrixIUD==NULL)
		errors++;
	kfData->matrixDet = (double *)malloc(nCh2*nCh2*sizeof(double));
	if(kfData->matrixDet==NULL)
		errors++;
	kfData->kGain = (double *)malloc(nCh2*KF_MULTI_STATES*sizeof(double));
	if(kfData->kGain==NULL)
		errors++;
	kfData->ht = (double *)malloc(nCh2*KF_MULTI_STATES*sizeof(double));
	if(kfData->ht==NULL)
		errors++;
	kfData->pprioriHt = (double *)malloc(nCh2*KF_MULTI_STATES*sizeof(double));
	if(kfData->pprioriHt==NULL)
		errors++;
	kfData->hPpriori = (double *)malloc(nCh2*KF_MULTI_STATES*sizeof(double));
	if(kfData->hPpriori==NULL)
		errors++;
	kfData->hPprioriHt = (double *)malloc(nCh2*nCh2*sizeof(double));
	if(kfData->hPprioriHt==NULL)
		errors++;
	kfData->hPprioriHtR = (double *)malloc(nCh2*nCh2*sizeof(double));
	if(kfData->hPprioriHtR==NULL)
		errors++;
	kfData->invHPprioriHtR = (double *)malloc(nCh2*nCh2*sizeof(double));
	if(kfData->invHPprioriHtR==NULL)
		errors++;
	kfData->hXpred = (double *)malloc(nCh2*1*sizeof(double));
	if(kfData->hXpred==NULL)
		errors++;
	kfData->zcurrHXpred = (double *)malloc(nCh2*1*sizeof(double));
	if(kfData->zcurrHXpred==NULL)
		errors++;
	kfData->kgainR = (double *)malloc(KF_MULTI_STATES*nCh2*sizeof(double));
	if(kfData->kgainR==NULL)
		errors++;
	kfData->kgaint = (double *)malloc(KF_MULTI_STATES*nCh2*sizeof(double));
	if(kfData->kgaint==NULL)
		errors++;

	if(errors>0)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Malloc returned a NULL pointer in InitKalmanFilterData");
		CloseNGene(cfg,EXIT_FAILURE);
	}

	InitKalmanFilterPhi(kfData->phi,kfData->phiMulti,cfg->pvtPeriodSeconds);
	InitKalmanFilterPostCovarianceMatrices(kfData->pPost,kfData->pPostMulti);
	InitQStateNoiseCovarianceMatrices(kfData->q,kfData->qMulti,cfg->pvtPeriodSeconds);
	return SUCCESS_RETURN;
}


void ClearKalmanFilterData(TNGeneConfiguration *cfg,TKalmanFilterData *kfData)
{
	int n = cfg->numberOfChannels;
	int n2 = 2*n;

	MatrixInit(kfData->aaa,1,n);
	MatrixInit(kfData->az,1,n);
	MatrixInit(kfData->el,1,n);
	MatrixInit(kfData->r,n2,n2);
	MatrixInit(kfData->range,n,1);
	MatrixInit(kfData->deltaRange,n,1);
	MatrixInit(kfData->u,n,KF_MULTI_SIZE);
	MatrixInit(kfData->pr,n,1);
	MatrixInit(kfData->prCorr,n,1);
	MatrixInit(kfData->deltaPrCorr,n,1);
	MatrixInit(kfData->zCurr,n2,1);
	MatrixInit(kfData->nominalMeasurement,n2,1);
	MatrixInit(kfData->kfMeasurement,n2,1);
	MatrixInit(kfData->h,n2,KF_MULTI_STATES);
	MatrixInit(kfData->stateAPosteriori,KF_MULTI_STATES,1);
	MatrixInit(kfData->matrixL,n2,n2);
	MatrixInit(kfData->matrixD,n2,n2);
	MatrixInit(kfData->matrixU,n2,n2);
	MatrixInit(kfData->matrixUt,n2,n2);
	MatrixInit(kfData->matrixIL,n2,n2);
	MatrixInit(kfData->matrixID,n2,n2);
	MatrixInit(kfData->matrixIU,n2,n2);
	MatrixInit(kfData->matrixIUt,n2,n2);
	MatrixInit(kfData->matrixIUD,n2,n2);
	MatrixInit(kfData->matrixDet,n2,n2);
	MatrixInit(kfData->xPred,KF_MULTI_STATES,1);
	MatrixInit(kfData->pPriori,KF_MULTI_STATES,KF_MULTI_STATES);
	MatrixInit(kfData->phiPap,KF_MULTI_STATES,KF_MULTI_STATES);
	MatrixInit(kfData->phit,KF_MULTI_STATES,KF_MULTI_STATES);
	MatrixInit(kfData->phiPapPhit,KF_MULTI_STATES,KF_MULTI_STATES);
	MatrixInit(kfData->kGain,n2,KF_MULTI_STATES);
	MatrixInit(kfData->ht,n2,KF_MULTI_STATES);
	MatrixInit(kfData->pprioriHt,n2,KF_MULTI_STATES);
	MatrixInit(kfData->hPpriori,n2,KF_MULTI_STATES);
	MatrixInit(kfData->hPprioriHt,n2,n2);
	MatrixInit(kfData->hPprioriHtR,n2,n2);
	MatrixInit(kfData->invHPprioriHtR,n2,n2);
	MatrixInit(kfData->xCurr,KF_MULTI_STATES,1);
	MatrixInit(kfData->hXpred,n2,1);
	MatrixInit(kfData->zcurrHXpred,n2,1);
	MatrixInit(kfData->kgainZcurrHXpred,KF_MULTI_STATES,1);
	MatrixInit(kfData->i,KF_MULTI_STATES,KF_MULTI_STATES);
	MatrixInit(kfData->kgainH,KF_MULTI_STATES,KF_MULTI_STATES);
	MatrixInit(kfData->iKgainH,KF_MULTI_STATES,KF_MULTI_STATES);
	MatrixInit(kfData->iKgainHPpriori,KF_MULTI_STATES,KF_MULTI_STATES);
	MatrixInit(kfData->iKgainHT,KF_MULTI_STATES,KF_MULTI_STATES);
	MatrixInit(kfData->kgainR,KF_MULTI_STATES,n2);
	MatrixInit(kfData->kgaint,n2,KF_MULTI_STATES);
	MatrixInit(kfData->kgainRKgaint,KF_MULTI_STATES,KF_MULTI_STATES);
	MatrixInit(kfData->aB,KF_MULTI_STATES,KF_MULTI_STATES);
}


void DeinitKalmanFilterData(TNGeneData *rx)
{
	TKalmanFilterData *kfData = &(rx->pvtData.kfData);

	free(kfData->aaa);
	free(kfData->az);
	free(kfData->el);
	free(kfData->r);
	free(kfData->range);
	free(kfData->deltaRange);
	free(kfData->u);
	free(kfData->pr);
	free(kfData->prCorr);
	free(kfData->deltaPrCorr);
	free(kfData->zCurr);
	free(kfData->nominalMeasurement);
	free(kfData->kfMeasurement);
	free(kfData->h);
	free(kfData->matrixL);
	free(kfData->matrixD);
	free(kfData->matrixU);
	free(kfData->matrixUt);
	free(kfData->matrixIL);
	free(kfData->matrixID);
	free(kfData->matrixIU);
	free(kfData->matrixIUt);
	free(kfData->matrixIUD);
	free(kfData->matrixDet);
	free(kfData->kGain);
	free(kfData->ht);
	free(kfData->pprioriHt);
	free(kfData->hPpriori);
	free(kfData->hPprioriHt);
	free(kfData->hPprioriHtR);
	free(kfData->invHPprioriHtR);
	free(kfData->hXpred);
	free(kfData->zcurrHXpred);
	free(kfData->kgainR);
	free(kfData->kgaint);
}


void KalmanFilterPrediction(int nStates,TKalmanFilterData *kfData,double *phi,double *q,double *pap)
{
	/* Calculate Predicted (a priori) state estimate = State Transition Model * previous state estimate */
	MatrixProduct(phi,nStates,nStates,kfData->stateAPosteriori,nStates,1,kfData->xPred);
	/* Calculate Predicted (a priori) estimate covariance */
	/* a. State Transition Model * A posteriori error Covariance */
	MatrixProduct(phi,nStates,nStates,pap,nStates,nStates,kfData->phiPap);
	/* b. Transpose of State Transition Model matrix */
	MatrixTrasp(phi,nStates,nStates,kfData->phit);
	/* 3. Product of a) * b) */
	MatrixProduct(kfData->phiPap,nStates,nStates,kfData->phit,nStates,nStates,kfData->phiPapPhit);
	/* c) plus Covariance of process noise */
	MatrixSum(kfData->phiPapPhit,nStates,nStates,q,nStates,nStates,pap);
}

void KalmanFilterRoutine(int nCh,int nStates,TKalmanFilterData *kfData,double *pap,double *phi,double *q)
{
	int nCh2 = nCh*2;
	int i;

	/* PREDICTION STEP */
	MatrixProduct(phi,nStates,nStates,kfData->stateAPosteriori,nStates,1,kfData->xPred);
	MatrixProduct(phi,nStates,nStates,pap,nStates,nStates,kfData->phiPap);
	MatrixTrasp(phi,nStates,nStates,kfData->phit);
	MatrixProduct(kfData->phiPap,nStates,nStates,kfData->phit,nStates,nStates,kfData->phiPapPhit);
	MatrixSum(kfData->phiPapPhit,nStates,nStates,q,nStates,nStates,kfData->pPriori);

	/* UPDATE STEP */
	/* 1. Innovation or residual covariance */
	MatrixTrasp(kfData->h,nCh2,nStates,kfData->ht);
	MatrixProduct(kfData->pPriori,nStates,nStates,kfData->ht,nStates,nCh2,kfData->pprioriHt);
	MatrixProduct(kfData->h,nCh2,nStates,kfData->pPriori,nStates,nStates,kfData->hPpriori);
	MatrixProduct(kfData->hPpriori,nCh2,nStates,kfData->ht,nStates,nCh2,kfData->hPprioriHt);
	MatrixSum(kfData->hPprioriHt,nCh2,nCh2,kfData->r,nCh2,nCh2,kfData->hPprioriHtR);
	/* 2. Optimal Kalman Gain */
	MatrixInv(kfData->hPprioriHtR,nCh2,kfData->invHPprioriHtR,kfData->matrixL,kfData->matrixD,kfData->matrixU,kfData->matrixUt,kfData->matrixIL,kfData->matrixID,kfData->matrixIU,kfData->matrixIUt,kfData->matrixIUD,kfData->matrixDet);
	MatrixProduct(kfData->pprioriHt,nStates,nCh2,kfData->invHPprioriHtR,nCh2,nCh2,kfData->kGain);
	/* 3. Innovation or measurement residual */
	MatrixProduct(kfData->h,nCh2,nStates,kfData->xPred,nStates,1,kfData->hXpred);
	MatrixDiff(kfData->kfMeasurement,nCh2,1,kfData->hXpred,nCh2,1,kfData->zcurrHXpred);
	/* 4. Updated (a posteriori) state estimate */
	MatrixProduct(kfData->kGain,nStates,nCh2,kfData->zcurrHXpred,nCh2,1,kfData->kgainZcurrHXpred);
	MatrixSum(kfData->xPred,nStates,1,kfData->kgainZcurrHXpred,nStates,1,kfData->xCurr);
	for(i=0;i<nStates;i++)
	{
		kfData->stateAPosteriori[i] = kfData->xCurr[i];
	}
	/* 5. Updated (a posteriori) estimate covariance */
	MatrixIdentity(kfData->i,nStates);
	MatrixProduct(kfData->kGain,nStates,nCh2,kfData->h,nCh2,nStates,kfData->kgainH);
	MatrixDiff(kfData->i,nStates,nStates,kfData->kgainH,nStates,nStates,kfData->iKgainH);
	MatrixProduct(kfData->iKgainH,nStates,nStates,kfData->pPriori,nStates,nStates,kfData->iKgainHPpriori);
	/* 6. Complete the Joseph form */
	MatrixTrasp(kfData->iKgainH,nStates,nStates,kfData->iKgainHT);
	MatrixProduct(kfData->kGain,nStates,nCh2,kfData->r,nCh2,nCh2,kfData->kgainR);
	MatrixTrasp(kfData->kGain,nStates,nCh2,kfData->kgaint);
	MatrixProduct(kfData->kgainR,nStates,nCh2,kfData->kgaint,nCh2,nStates,kfData->kgainRKgaint);
	MatrixProduct(kfData->iKgainHPpriori,nStates,nStates,kfData->iKgainHT,nStates,nStates,kfData->aB);
	MatrixSum(kfData->aB,nStates,nStates,kfData->kgainRKgaint,nStates,nStates,pap);
}


void PreparePPost(TKalmanFilterData *kfData,TSatConstellation satType)
{
	int rowIdx;
	int colIdx;
	int idx;

	if(satType==GPS_SAT)
	{
		/* updates all the variables */
		for(rowIdx=0;rowIdx<KF_SINGLE_STATES;rowIdx++)
		{
			for(colIdx=0;colIdx<KF_SINGLE_STATES;colIdx++)
			{
				kfData->pPost[(rowIdx*KF_SINGLE_STATES)+colIdx] = kfData->pPostMulti[(rowIdx*KF_MULTI_STATES)+colIdx];
			}
		}
	}
	else /* if(satType==GALILEO_SAT) */
	{
		/* updates position and velocity variables */
		for(rowIdx=0;rowIdx<6;rowIdx++)
		{
			for(colIdx=0;colIdx<6;colIdx++)
			{
				kfData->pPost[(rowIdx*KF_SINGLE_STATES)+colIdx] = kfData->pPostMulti[(rowIdx*KF_MULTI_STATES)+colIdx];
			}
		}
		/* update clock drift variables */
		for(idx=0;idx<6;idx++)
		{
			kfData->pPost[(idx*KF_SINGLE_STATES)+7] = kfData->pPostMulti[(idx*KF_MULTI_STATES)+7];
			kfData->pPost[(7*KF_SINGLE_STATES)+idx] = kfData->pPostMulti[(7*KF_MULTI_STATES)+idx];
		}
		kfData->pPost[(7*KF_SINGLE_STATES)+7] = kfData->pPostMulti[(7*KF_MULTI_STATES)+7];
		/* update Galileo clock bias variables */
		for(idx=0;idx<6;idx++)
		{
			kfData->pPost[(idx*KF_SINGLE_STATES)+6] = kfData->pPostMulti[(idx*KF_MULTI_STATES)+8];
			kfData->pPost[(6*KF_SINGLE_STATES)+idx] = kfData->pPostMulti[(8*KF_MULTI_STATES)+idx];
		}
		kfData->pPost[(6*KF_SINGLE_STATES)+6] = kfData->pPostMulti[(8*KF_MULTI_STATES)+8];
		kfData->pPost[(7*KF_SINGLE_STATES)+6] = kfData->pPostMulti[(8*KF_MULTI_STATES)+7];
		kfData->pPost[(6*KF_SINGLE_STATES)+7] = kfData->pPostMulti[(7*KF_MULTI_STATES)+8];
	}
}

/**
 * @brief After a Single constellation PVT has been performed, the pPost matrix is updated.
 * This function updates the pPostMulti matrix accordingly
 */
void UpdatePPostMulti(TKalmanFilterData *kfData,TSatConstellation satType)
{
	int rowIdx;
	int colIdx;
	int idx;

	if(satType==GPS_SAT)
	{
		/* updates all the variables */
		for(rowIdx=0;rowIdx<KF_SINGLE_STATES;rowIdx++)
		{
			for(colIdx=0;colIdx<KF_SINGLE_STATES;colIdx++)
			{
				kfData->pPostMulti[(rowIdx*KF_MULTI_STATES)+colIdx] = kfData->pPost[(rowIdx*KF_SINGLE_STATES)+colIdx];
			}
		}
		/* Galileo clock bias variables are maintained */
	}
	else /* if(satType==GALILEO_SAT) */
	{
		/* updates position and velocity variables */
		for(rowIdx=0;rowIdx<6;rowIdx++)
		{
			for(colIdx=0;colIdx<6;colIdx++)
			{
				kfData->pPostMulti[(rowIdx*KF_MULTI_STATES)+colIdx] = kfData->pPost[(rowIdx*KF_SINGLE_STATES)+colIdx];
			}
		}
		/* update clock drift variables */
		for(idx=0;idx<6;idx++)
		{
			kfData->pPostMulti[(idx*KF_MULTI_STATES)+7] = kfData->pPost[(idx*KF_SINGLE_STATES)+7];
			kfData->pPostMulti[(7*KF_MULTI_STATES)+idx] = kfData->pPost[(7*KF_SINGLE_STATES)+idx];
		}
		kfData->pPostMulti[(7*KF_MULTI_STATES)+7] = kfData->pPost[(7*KF_SINGLE_STATES)+7];
		/* update Galileo clock bias variables */
		for(idx=0;idx<6;idx++)
		{
			kfData->pPostMulti[(idx*KF_MULTI_STATES)+8] = kfData->pPost[(idx*KF_SINGLE_STATES)+6];
			kfData->pPostMulti[(8*KF_MULTI_STATES)+idx] = kfData->pPost[(6*KF_SINGLE_STATES)+idx];
		}
		kfData->pPostMulti[(8*KF_MULTI_STATES)+8] = kfData->pPost[(6*KF_SINGLE_STATES)+6];
		kfData->pPostMulti[(8*KF_MULTI_STATES)+7] = kfData->pPost[(7*KF_SINGLE_STATES)+6];
		kfData->pPostMulti[(7*KF_MULTI_STATES)+8] = kfData->pPost[(6*KF_SINGLE_STATES)+7];
		/* GPS clock bias variables are maintained */
	}
}


int PerformKalmanFilter(TNGeneConfiguration *cfg,TNGeneData *rx)
{
	TPvtData *pvtData = &(rx->pvtData);
	TKalmanFilterData *kfData = &(pvtData->kfData);
	int nCh = pvtData->numOfOperativeChannels;
	int nCh2 = nCh*2;
	TPvtChannel *pvtCh;
	TSatStatus *satStatus;
	TPvtResults *pvtRes = &(rx->pvtTemporaryResults);
	double dt = cfg->pvtPeriodSeconds;
	double positionAndVelocity[KF_MULTI_STATES];
	TSatConstellation satTypeForSingleConst = INVALID_SAT;
	int nStates;
	int size;
	int idxGpsBias;
	int idxGalBias;
	int idx;
	double *phi;
	double *q;
	double *pap;
	double dX, dY, dZ, dXv, dYv, dZv;

	ClearKalmanFilterData(cfg,kfData);
	/* Predict new position using last estimated velocity */
	positionAndVelocity[0] = pvtRes->ecefPosition[0] + (pvtRes->ecefVelocity[0]*dt);
	positionAndVelocity[1] = pvtRes->ecefPosition[1] + (pvtRes->ecefVelocity[1]*dt);
	positionAndVelocity[2] = pvtRes->ecefPosition[2] + (pvtRes->ecefVelocity[2]*dt);
	positionAndVelocity[3] = pvtRes->ecefVelocity[0];
	positionAndVelocity[4] = pvtRes->ecefVelocity[1];
	positionAndVelocity[5] = pvtRes->ecefVelocity[2];
	positionAndVelocity[6] = pvtRes->clockBiasGps;
	positionAndVelocity[7] = pvtRes->clockDrift;
	positionAndVelocity[8] = pvtRes->clockBiasGalileo;

	if(nCh==0)
	{
		/* No measures are available, prediction is performed */
		if((pvtRes->clockBiasGpsValid==TRUE_FLAG) && (pvtRes->clockBiasGalileoValid==TRUE_FLAG))
		{
			size = KF_MULTI_SIZE;
			nStates = KF_MULTI_STATES;
			phi = kfData->phiMulti;
			q = kfData->qMulti;
			pap = kfData->pPostMulti;
		}
		else
		{
			if(pvtRes->clockBiasGalileoValid==TRUE_FLAG)
			{
				satTypeForSingleConst = GALILEO_SAT;
			}
			else
			{
				satTypeForSingleConst = GPS_SAT;
			}
			PreparePPost(kfData,satTypeForSingleConst);
			size = KF_SINGLE_SIZE;
			nStates = KF_SINGLE_STATES;
			phi = kfData->phi;
			q = kfData->q;
			pap = kfData->pPost;
		}
		KalmanFilterPrediction(nStates,kfData,phi,q,pap);
	}
	else
	{
		/* Uses the pseudorange and Doppler measurements */
		if((pvtData->numOfGalileoOpChannels>0) && (pvtData->numOfGpsOpChannels>0))
		{
			/* Multiple constellation */
			size = KF_MULTI_SIZE;
			nStates = KF_MULTI_STATES;
			idxGpsBias = 3;
			idxGalBias = 4;
			phi = kfData->phiMulti;
			q = kfData->qMulti;
			pap = kfData->pPostMulti;
		}
		else
		{
			/* Single constellation */
			if(pvtData->numOfGalileoOpChannels>0)
			{
				satTypeForSingleConst = GALILEO_SAT;
			}
			else
			{
				satTypeForSingleConst = GPS_SAT;
			}
			PreparePPost(kfData,satTypeForSingleConst);
			size = KF_SINGLE_SIZE;
			nStates = KF_SINGLE_STATES;
			idxGpsBias = 3;
			idxGalBias = 3;
			phi = kfData->phi;
			q = kfData->q;
			pap = kfData->pPost;
		}
		for(idx=0;idx<nCh;idx++)
		{
			pvtCh = &(pvtData->pvtChArray[idx]);
			satStatus = pvtCh->satStatus;

			kfData->aaa[idx] = (pvtCh->pseudorangeCsm/SPEED_OF_LIGHT)*WEDOT;
			kfData->el[idx] = pvtCh->satStatus->elevation;
			kfData->az[idx] = pvtCh->satStatus->azimuth;
			pvtCh->pseudorangeCsm += (-pvtCh->tropoCorrection - pvtCh->ionoCorrection); /* Pseudo2True */

			kfData->r[idx*(nCh2+1)] = pvtCh->posSigma2 / sin(kfData->el[idx]);
			kfData->r[(idx+nCh)*(nCh2+1)] = pvtCh->velSigma2 / sin(kfData->el[idx]);

			satStatus->ecefSatPosRot[0] = (pvtCh->satStatus->ecefSatPos[0] * cos(kfData->aaa[idx])) + (pvtCh->satStatus->ecefSatPos[1] * sin(kfData->aaa[idx]));
			satStatus->ecefSatPosRot[1] = (pvtCh->satStatus->ecefSatPos[1] * cos(kfData->aaa[idx])) - (pvtCh->satStatus->ecefSatPos[0] * sin(kfData->aaa[idx]));
			satStatus->ecefSatPosRot[2] = pvtCh->satStatus->ecefSatPos[2];
			dX = satStatus->ecefSatPosRot[0] - positionAndVelocity[0];
			dY = satStatus->ecefSatPosRot[1] - positionAndVelocity[1];
			dZ = satStatus->ecefSatPosRot[2] - positionAndVelocity[2];
			dXv = pvtCh->satStatus->ecefSatVel[0] - positionAndVelocity[3];
			dYv = pvtCh->satStatus->ecefSatVel[1] - positionAndVelocity[4];
			dZv = pvtCh->satStatus->ecefSatVel[2] - positionAndVelocity[5];
			kfData->range[idx] = sqrt(pow(dX,2.0) + pow(dY,2.0) + pow(dZ,2.0));
			kfData->deltaRange[idx]= ((dXv*dX) + (dYv*dY) + (dZv*dZ)) / kfData->range[idx];

			kfData->u[(idx*size)+0] = dX / kfData->range[idx];
			kfData->u[(idx*size)+1] = dY / kfData->range[idx];
			kfData->u[(idx*size)+2] = dZ / kfData->range[idx];
			kfData->u[(idx*size)+idxGpsBias] = 0.0; /* TODO dovrebbe già essere zero: verificare! */
			kfData->u[(idx*size)+idxGalBias] = 0.0; /* TODO dovrebbe già essere zero: verificare! */
			if(pvtCh->ch->satType==GPS_SAT)
			{
				kfData->u[(idx*size)+idxGpsBias] = -1.0;
			}
			else /* if(pvtCh->ch->satType==GALILEO_SAT) */
			{
				kfData->u[(idx*size)+idxGalBias] = -1.0;
			}

			kfData->pr[idx] = pvtCh->pseudorangeCsm;
			if(pvtCh->ch->satType == GPS_SAT)
			{
				kfData->prCorr[idx] = kfData->pr[idx] - pvtRes->clockBiasGps;
			}
			else  /* if(pvtCh->ch->satType == GALILEO_SAT) */
			{
				kfData->prCorr[idx] = kfData->pr[idx] - pvtRes->clockBiasGalileo;
			}
			kfData->deltaPrCorr[idx] = (-(pvtCh->deltaPseudorange) * LAMBDA) + pvtRes->clockDrift;

			kfData->h[(idx*nStates)+0] = -kfData->u[(idx*size)+0];
			kfData->h[(idx*nStates)+1] = -kfData->u[(idx*size)+1];
			kfData->h[(idx*nStates)+2] = -kfData->u[(idx*size)+2];
			kfData->h[(idx*nStates)+6] = -kfData->u[(idx*size)+3];
			kfData->h[(idx*nStates)+8] = -kfData->u[(idx*size)+4]; /* if single constellation, this value will be overwritten */
			kfData->h[(idx*nStates)+0+(nCh*nStates)+3] = -kfData->u[(idx*size)+0];
			kfData->h[(idx*nStates)+1+(nCh*nStates)+3] = -kfData->u[(idx*size)+1];
			kfData->h[(idx*nStates)+2+(nCh*nStates)+3] = -kfData->u[(idx*size)+2];
			kfData->h[(idx*nStates)+(nCh*nStates)+7] = 1.0;
		}
		MergeMatricesVertically(kfData->prCorr,nCh,1,kfData->deltaPrCorr,nCh,1,kfData->zCurr);
		MergeMatricesVertically(kfData->range,nCh,1,kfData->deltaRange,nCh,1,kfData->nominalMeasurement);
		MatrixDiff(kfData->zCurr,nCh2,1,kfData->nominalMeasurement,nCh2,1,kfData->kfMeasurement);
		KalmanFilterRoutine(nCh,nStates,kfData,pap,phi,q);
		/* Correct PVT solution */
		positionAndVelocity[0] = positionAndVelocity[0] + kfData->stateAPosteriori[0];
		positionAndVelocity[1] = positionAndVelocity[1] + kfData->stateAPosteriori[1];
		positionAndVelocity[2] = positionAndVelocity[2] + kfData->stateAPosteriori[2];
		positionAndVelocity[3] = positionAndVelocity[3] + kfData->stateAPosteriori[3];
		positionAndVelocity[4] = positionAndVelocity[4] + kfData->stateAPosteriori[4];
		positionAndVelocity[5] = positionAndVelocity[5] + kfData->stateAPosteriori[5];
		positionAndVelocity[7] = positionAndVelocity[7] - kfData->stateAPosteriori[7];
		if((pvtData->numOfGalileoOpChannels>0) && (pvtData->numOfGpsOpChannels>0))
		{
			positionAndVelocity[6] = positionAndVelocity[6] + kfData->stateAPosteriori[6];
			positionAndVelocity[8] = positionAndVelocity[8] + kfData->stateAPosteriori[8];
		}
		else if(pvtData->numOfGpsOpChannels>0)
		{
			positionAndVelocity[6] = positionAndVelocity[6] + kfData->stateAPosteriori[6];
		}
		else if(pvtData->numOfGalileoOpChannels>0)
		{
			positionAndVelocity[8] = positionAndVelocity[8] + kfData->stateAPosteriori[6];
		}
	}
	/* Update PPostMulti when single constellation PVT is performed,
	 * or just one bias is predicted (outage) */
	if(size==KF_SINGLE_SIZE)
	{
		UpdatePPostMulti(kfData,satTypeForSingleConst);
	}
	/* Save the results */
	pvtRes->ecefPosition[0] = positionAndVelocity[0];
	pvtRes->ecefPosition[1] = positionAndVelocity[1];
	pvtRes->ecefPosition[2] = positionAndVelocity[2];
	pvtRes->ecefVelocity[0] = positionAndVelocity[3];
	pvtRes->ecefVelocity[1] = positionAndVelocity[4];
	pvtRes->ecefVelocity[2] = positionAndVelocity[5];
	if(pvtRes->clockBiasGpsValid==TRUE_FLAG)
	{
		pvtRes->clockBiasGps = positionAndVelocity[6];
	}
	pvtRes->clockDrift = positionAndVelocity[7];
	if(pvtRes->clockBiasGalileoValid==TRUE_FLAG)
	{
		pvtRes->clockBiasGalileo = positionAndVelocity[8];
	}
	return SUCCESS_RETURN;
}
