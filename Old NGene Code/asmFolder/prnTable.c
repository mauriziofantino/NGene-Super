/**
 * @file prnTable.c
 *
 * @author Mario Nicola
 *
 * @date 18/08/2014
 *
 * @version 1.0.0
 *
 * @brief Functions that manage the sampling of the spreading codes.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include "simd.h"
#include "ngeneConfiguration.h"
#include "prnTable.h"
#include "ngeneConstants.h"
#include "closeManager.h"
#include "spreadingCodes.h"
#include "ioFunctions.h"
#include "arrays.h"

/* global variable for the backup of XMM registers */
extern char *ngeneXmmsBackup;

void OversampleCode(char *code, int codeLen,s128 *oversampled)
{
	int i;

	/* BACKUP */
	movdqa_r2m(xmm0,ngeneXmmsBackup[XMM_REGISTER_SIZE*0]);
	/**/

	for(i=0;i<codeLen;i++)
	{
		oversampled[i].b[0]  = code[i];
		oversampled[i].b[1]  = code[i];
		oversampled[i].b[2]  = code[i];
		oversampled[i].b[3]  = code[i];
		oversampled[i].b[4]  = code[i];
		oversampled[i].b[5]  = code[i];
		oversampled[i].b[6]  = code[i];
		oversampled[i].b[7]  = code[i];
		oversampled[i].b[8]  = code[i];
		oversampled[i].b[9]  = code[i];
		oversampled[i].b[10] = code[i];
		oversampled[i].b[11] = code[i];
		oversampled[i].b[12] = code[i];
		oversampled[i].b[13] = code[i];
		oversampled[i].b[14] = code[i];
		oversampled[i].b[15] = code[i];
		// Copy twice
		movdqa_m2r(oversampled[i], xmm0);
		movdqa_r2m(xmm0, oversampled[codeLen+i]);
	}
	movdqa_m2r(oversampled[0], xmm0);
	movdqa_r2m(xmm0, oversampled[2*codeLen]);
	
	/* RESTORE */
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*0],xmm0);
}


int InitPrnTable(TNGeneConfiguration *cfg,TPrnTable *t)
{
	int errorsFound = 0;
	int index;
	int gpsSize = (2*GPS_CODE_LEN)+1;
	int egnosSize = (2*EGNOS_CODE_LEN)+1;
	int galileoSize = (2*GALILEO_CODE_LEN)+1;
	char code[GALILEO_CODE_LEN];
	char filename[MAX_ROW_LENGTH];
	FILE *codeFile;
	char row[MAX_ROW_LENGTH];
	int prn;

	/* Allocate GPS */
	for(index=0;index<GPS_NUMBER_OF_PRNS;index++)
	{
		posix_memalign((void **)&(t->gpsChipsOversampledBpsk[index]),POSIX_ALIGNMENT,gpsSize*sizeof(s128));
		if(t->gpsChipsOversampledBpsk[index]==NULL)
			errorsFound++;

		posix_memalign((void **)&(t->gpsChipsOversampled[index]),POSIX_ALIGNMENT,gpsSize*sizeof(s128));
		if(t->gpsChipsOversampled[index]==NULL)
			errorsFound++;
	}
	/* Allocate EGNOS */
	for(index=0;index<EGNOS_NUMBER_OF_PRNS;index++)
	{
		posix_memalign((void **)&(t->egnosChipsOversampledBpsk[index]),POSIX_ALIGNMENT,egnosSize*sizeof(s128));
		if(t->egnosChipsOversampledBpsk[index]==NULL)
			errorsFound++;

		posix_memalign((void **)&(t->egnosChipsOversampled[index]),POSIX_ALIGNMENT,egnosSize*sizeof(s128));
		if(t->egnosChipsOversampled[index]==NULL)
			errorsFound++;
	}
	/* Allocate Galileo */
	for(index=0;index<GALILEO_NUMBER_OF_PRNS;index++)
	{
		posix_memalign((void **)&(t->galileoChipsOversampledBpsk[index]),POSIX_ALIGNMENT,galileoSize*sizeof(s128));
		if(t->galileoChipsOversampledBpsk[index]==NULL)
			errorsFound++;

		posix_memalign((void **)&(t->galileoChipsOversampled[index]),POSIX_ALIGNMENT,galileoSize*sizeof(s128));
		if(t->galileoChipsOversampled[index]==NULL)
			errorsFound++;
	}
	/* errors found */
	if(errorsFound>0)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Malloc returned a NULL pointer in InitPrnTable");
		CloseNGene(cfg,EXIT_FAILURE);
	}
	/* Initialize from file */
	/* GPS */
	filename[0] = '\0';
	strcat(filename, cfg->otherDataPath);
	strcat(filename, "GPS_codes.dat");
	codeFile = fopen(filename,"rb");
	if(codeFile==NULL)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Cannot read file with GPS despreading codes (%s)",filename);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	for(index=0;index<GPS_NUMBER_OF_PRNS;index++)
	{
		fread(code,sizeof(char),GPS_CODE_LEN,codeFile);
		OversampleCode(code, GPS_CODE_LEN,t->gpsChipsOversampledBpsk[index]);
		fread(code,sizeof(char),GPS_CODE_LEN,codeFile);
		OversampleCode(code, GPS_CODE_LEN,t->gpsChipsOversampled[index]);
	}
	fclose(codeFile);
	/* EGNOS */
	filename[0] = '\0';
	strcat(filename, cfg->otherDataPath);
	strcat(filename, "EGNOS_codes_v2.dat");
	codeFile = fopen(filename,"rb");
	if(codeFile==NULL)
	{
		/* if the new file containing PRN 136 is not available, read the old file */
		SwrxLog(NGENE_LOG_SEVERITY_WARNING,"Cannot read file with EGNOS despreading codes (%s). PRN 136 not available!!!",filename);
		filename[0] = '\0';
		strcat(filename, cfg->otherDataPath);
		strcat(filename, "EGNOS_codes.dat");
		codeFile = fopen(filename,"rb");
		if(codeFile==NULL)
		{
			SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Cannot read file with EGNOS despreading codes (%s)",filename);
			CloseNGene(cfg,EXIT_FAILURE);
		}
		t->egnos136Valid = FALSE_FLAG;
	}
	else
	{
		/* EGNOS PRN 136 is available */
		t->egnos136Valid = TRUE_FLAG;
	}
	for(index=0;index<EGNOS_NUMBER_OF_PRNS;index++)
	{
		/* In a PRN is not present, use a fake code made of zeros */
		if(fread(code,sizeof(char),EGNOS_CODE_LEN,codeFile)==EGNOS_CODE_LEN)
		{
			OversampleCode(code, EGNOS_CODE_LEN,t->egnosChipsOversampledBpsk[index]);
		}
		else
		{
			ArrayClean((char *)t->egnosChipsOversampledBpsk[index],EGNOS_CODE_LEN*sizeof(s128));
		}
		if(fread(code,sizeof(char),EGNOS_CODE_LEN,codeFile)==EGNOS_CODE_LEN)
		{
			OversampleCode(code, EGNOS_CODE_LEN,t->egnosChipsOversampled[index]);
		}
		else
		{
			ArrayClean((char *)t->egnosChipsOversampled[index],EGNOS_CODE_LEN*sizeof(s128));
		}
	}
	fclose(codeFile);
	/* Galileo E1b */
	filename[0] = '\0';
	strcat(filename, cfg->otherDataPath);
	strcat(filename, "Galileo_E1b_codes.dat");
	codeFile = fopen(filename,"rb");
	if(codeFile==NULL)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Cannot read file with Galileo despreading codes (%s)",filename);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	for(index=0;index<GALILEO_NUMBER_OF_PRNS;index++)
	{
		fread(code,sizeof(char),GALILEO_CODE_LEN,codeFile);
		OversampleCode(code, GALILEO_CODE_LEN,t->galileoChipsOversampledBpsk[index]);
		fread(code,sizeof(char),GALILEO_CODE_LEN,codeFile);
		OversampleCode(code, GALILEO_CODE_LEN,t->galileoChipsOversampled[index]);
	}
	fclose(codeFile);
	/* Manage real Galileo satellites */
	t->numberOfRealGalileoSat = 0;
	t->prnOfRealGalileoSat = NULL;
	if(cfg->allGalileoPrnsEnabled==FALSE_FLAG)
	{
		filename[0] = '\0';
		strcat(filename, cfg->otherDataPath);
		strcat(filename, "Galileo_real_PRNs.txt");
		codeFile = fopen(filename,"r");
		if(codeFile==NULL)
		{
			cfg->allGalileoPrnsEnabled = TRUE_FLAG; /* cannot use the list of all real satellites */
		}
		else
		{
			while(fgets(row,MAX_ROW_LENGTH,codeFile)!=NULL)
			{
				if((row[0]!='#') && (sscanf(row,"%d",&prn)==1))
				{
					t->numberOfRealGalileoSat++;
				}
			}
			t->prnOfRealGalileoSat = (int *)malloc(t->numberOfRealGalileoSat*sizeof(int));
			if(t->prnOfRealGalileoSat==NULL)
			{
				SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Malloc returned a NULL pointer in InitPrnTable");
				CloseNGene(cfg,EXIT_FAILURE);
			}
			rewind(codeFile);
			index = 0;
			while(fgets(row,MAX_ROW_LENGTH,codeFile)!=NULL)
			{
				if((row[0]!='#') && (sscanf(row,"%d",&prn)==1))
				{
					t->prnOfRealGalileoSat[index] = prn;
					index++;
				}
			}
			fclose(codeFile);
		}
	}
	/* other variables */
	t->scaling = floor((double)UINT_MAX/(4.0*cfg->fs));
	t->ncoThreshold = (unsigned int)floor(t->scaling*cfg->fs);
	t->chipShortLen = (unsigned int)floor(cfg->fs/(double)GPS_CODE_RATE);
	t->chipLongLen = t->chipShortLen +1;
	return SUCCESS_RETURN;
}

void DeinitPrnTable(TPrnTable *t)
{
	int index;

	for(index=0;index<GPS_NUMBER_OF_PRNS;index++)
	{
		free(t->gpsChipsOversampledBpsk[index]);
		free(t->gpsChipsOversampled[index]);
	}
	for(index=0;index<EGNOS_NUMBER_OF_PRNS;index++)
	{
		free(t->egnosChipsOversampledBpsk[index]);
		free(t->egnosChipsOversampled[index]);
	}
	for(index=0;index<GALILEO_NUMBER_OF_PRNS;index++)
	{
		free(t->galileoChipsOversampledBpsk[index]);
		free(t->galileoChipsOversampled[index]);
	}
	if(t->prnOfRealGalileoSat!=NULL)
	{
		free(t->prnOfRealGalileoSat);
		t->prnOfRealGalileoSat = NULL;
	}
}

int GenerateCode(TNGeneConfiguration *cfg,TPrnTable *t,TSatConstellation prnConstellation,int prn,double codeRate,double *codePhase,double *codePhaseFrac,TEmployedModulation employedModulation,int n,char *codeOut)
{
	char *code;
	unsigned int chipIndex;
	unsigned int nco;
	unsigned int ncoIncrement;
	unsigned int ncoChipVeryShort;
	unsigned int ncoChipShort;
	int ncoIncrementChipShort;
	int ncoIncrementChipLong;
	char *ptrOut = codeOut;
	int index=n;
	unsigned int index2;
	s128 xmm0Backup;

	/* initialization */
	if(prnConstellation==GPS_SAT)
	{
		if(prn>GPS_NUMBER_OF_PRNS)
			return FAILURE_RETURN;
		code = &(t->gpsChipsOversampled[prn-BASE_PRN][0].b[0]);
		if(employedModulation==BPSK_MODULATION)
			code = &(t->gpsChipsOversampledBpsk[prn-BASE_PRN][0].b[0]);
	}
	else if(prnConstellation==GALILEO_SAT)
	{
		if(prn>GALILEO_NUMBER_OF_PRNS)
			return FAILURE_RETURN;
		code = &(t->galileoChipsOversampled[prn-BASE_PRN][0].b[0]);
		if(employedModulation==BPSK_MODULATION)
			code = &(t->galileoChipsOversampledBpsk[prn-BASE_PRN][0].b[0]);
	}
	else /* EGNOS_SAT */
	{
		if(prn==EGNOS_FIRST_PRN)
		{
			code = &(t->egnosChipsOversampled[0][0].b[0]);
			if(employedModulation==BPSK_MODULATION)
				code = &(t->egnosChipsOversampledBpsk[0][0].b[0]);
		}
		else if(prn==EGNOS_SECOND_PRN)
		{
			code = &(t->egnosChipsOversampled[1][0].b[0]);
			if(employedModulation==BPSK_MODULATION)
				code = &(t->egnosChipsOversampledBpsk[1][0].b[0]);
		}
		else if(prn==EGNOS_THIRD_PRN)
		{
			code = &(t->egnosChipsOversampled[2][0].b[0]);
			if(employedModulation==BPSK_MODULATION)
				code = &(t->egnosChipsOversampledBpsk[2][0].b[0]);
		}
		else if(prn==EGNOS_FOURTH_PRN)
		{
			code = &(t->egnosChipsOversampled[3][0].b[0]);
			if(employedModulation==BPSK_MODULATION)
				code = &(t->egnosChipsOversampledBpsk[3][0].b[0]);
		}
		else
			return FAILURE_RETURN;
	}
	/* Without floor in the following lines,
	 * the receiver compiled in release and in Ubuntu 64 bit does not work:
	 * codePhase is fixed to 0.0 and tracking unlocks */
	/* TODO Discover why!!! */
	chipIndex = (unsigned int)floor(*codePhase);
	nco = (unsigned int)floor(((double)t->ncoThreshold)*(*codePhaseFrac));
	ncoIncrement = (unsigned int)floor(codeRate*t->scaling);
	ncoChipShort = t->chipShortLen*ncoIncrement;
	ncoChipVeryShort = ncoChipShort - ncoIncrement;
	ncoIncrementChipShort = ncoChipShort - t->ncoThreshold;
	ncoIncrementChipLong = ncoIncrementChipShort + ncoIncrement;
	code += (chipIndex*XMM_REGISTER_SIZE);
	/* initial transient */
	if((nco+ncoChipVeryShort)>t->ncoThreshold)
	{
		while(nco<t->ncoThreshold)
		{
			ptrOut[0]=code[0];
			index--;
			nco += ncoIncrement;
			ptrOut++;
		}
		code += XMM_REGISTER_SIZE;
		nco -= t->ncoThreshold;
	}
	/**/
	/* Starting from Ubuntu 18.04 64 bit, this function, when compiled in release,
	 * avoids the receiver standard behaviour (it does not acquire signals!).
	 * Analysis led to the conclusion that the variable codeRate loses its correct value
	 * inside GenerateCode, probably because the compiler allocates it in the xmm0 register,
	 * which is used for the copy of chip samples with movdqu.
	 * To avoid this malfunctioning, the content of xmm0 is saved in the stack
	 * before the main loop and restored after it.
	 * It is faster than copying bytes with memcpy (which works also). */
	movdqu_r2m(xmm0,xmm0Backup);
	while(index>=0)
	{
		if((nco+ncoChipShort)<t->ncoThreshold)
		{
			index2=0;
			while(index2<t->chipLongLen)
			{
				movdqu_m2r(code[0],xmm0);
				movdqu_r2m(xmm0, ptrOut[index2]);
				index2 += XMM_REGISTER_SIZE;
			}
			nco += ncoIncrementChipLong;
			code += XMM_REGISTER_SIZE;
			index -= t->chipLongLen;
			ptrOut += t->chipLongLen;
		}
		else
		{
			index2=0;
			while(index2<t->chipShortLen)
			{
				movdqu_m2r(code[0],xmm0);
				movdqu_r2m(xmm0, ptrOut[index2]);
				index2 += XMM_REGISTER_SIZE;
			}
			code += XMM_REGISTER_SIZE;
			nco += ncoIncrementChipShort;
			index -= t->chipShortLen;
			ptrOut += t->chipShortLen;
		}
	}
	movdqu_m2r(xmm0Backup,xmm0);
	/* update codePhase */
	UpdateCodePhase(cfg,prnConstellation,codePhase,codePhaseFrac,n,codeRate);
	return SUCCESS_RETURN;
}

void UpdateCodePhase(TNGeneConfiguration *cfg,TSatConstellation satType, double *codePhase, double *codePhaseFrac, unsigned int nSamples, double codeRate)
{
	float codeLen;

	if(satType==GPS_SAT)
		codeLen = (float)GPS_CODE_LEN;
	else if(satType==GALILEO_SAT)
		codeLen = (float)GALILEO_CODE_LEN;
	else
		codeLen = (float)EGNOS_CODE_LEN;

	(*codePhase) += ((double)nSamples*codeRate*cfg->ts);
	while((*codePhase)>=codeLen)
		(*codePhase) -= codeLen;

	(*codePhaseFrac) = (*codePhase)-floor(*codePhase);
}

void AddCodePhases(TSatConstellation satType, double codePhase1, double codePhase2, double *codePhase, double *codePhaseFrac)
{
	float codeLen;

	if(satType==GPS_SAT)
		codeLen = (float)GPS_CODE_LEN;
	else if(satType==GALILEO_SAT)
		codeLen = (float)GALILEO_CODE_LEN;
	else
		codeLen = (float)EGNOS_CODE_LEN;

	(*codePhase) = codePhase1 + codePhase2;

	while((*codePhase)>=codeLen)
		(*codePhase) -= codeLen;

	while((*codePhase)<0)
		(*codePhase) += codeLen;

	(*codePhaseFrac) = (*codePhase)-floor(*codePhase);
}
