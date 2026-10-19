/**
 * @file carrierTable.c
 *
 * @author Mario Nicola
 *
 * @date 19/08/2014
 *
 * @version 1.0.0
 *
 * @brief Functions that manage the generation of a carrier at sampling frequency.
 *  	  Derived from the implementation by Nordio (2007)
 */
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include "ngeneConstants.h"
#include "carrierTable.h"
#include "ngeneConfiguration.h"
#include "simd.h"
#include "ioFunctions.h"
#include "closeManager.h"


/* global variable for the backup of XMM registers */
extern char *ngeneXmmsBackup;

int InitCarrierTable(TNGeneConfiguration *cfg, TSinCosTable *t, double f)
{
	double normalizedF;
	unsigned int i, j;
	double tau;
	char *p;
	double s,c; /* sin and cos values */
	double alpha; /* angle */

	normalizedF = f/cfg->fs;
	t->log2Rows = LOG2_ROWS;
	t->log2SamplesPerRow = LOG2_SAMPLES_PER_ROW;

	/* Fill table fields */
	t->tableSize = (1<< (t->log2Rows + t->log2SamplesPerRow +1)); /* rows*samples*2 since sin and cos are in the same row */
	posix_memalign((void **)&(t->table), 16, sizeof(s128)*(t->tableSize >>4)); /* allocates memory for table. t->tableSize bytes or  (t->tableSize >>4) s128 */

	if(t->table == NULL)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"InitializeCarrierTable: posix_memalign() returned a NULL pointer");
		CloseNGene(cfg,EXIT_FAILURE);
	}
	/* Fill structure fields */
	t->samplesPerRow = (1<<t->log2SamplesPerRow);
	t->rows = (1<<t->log2Rows);
	t->f = normalizedF-floor(normalizedF); /* normalized frequency, is taken mod 1 since needs to be in [0,1) */

	/* Compute local variables */
	tau = TWICE_M_PI*t->f; /* phase increment between two consecutive samples */
	p = (char *)&(t->table[0]);  /* p points to the table head */

	/* Fill table */
	for(i=0;i<t->rows;i++)
	{
		alpha = (TWICE_M_PI*i)/(double)t->rows;
		for(j=0;j<t->samplesPerRow;j++)
		{
			s = sin(alpha);
			if(s>=0.0)
				p[j] = 0x00;
			else
				p[j] = 0xff;
			c = cos(alpha);
			if(c>=0.0)
				p[j+t->samplesPerRow] = 0x00;
			else
				p[j+t->samplesPerRow] = 0xff;
			alpha += tau;
		}
		p+= 2*t->samplesPerRow;   /* 2*,  since every row contains sin and cos values */
	}

	return SUCCESS_RETURN;
}

void DeinitCarrierTable(TSinCosTable *t)
{
	free(t->table);
}


unsigned int Generate_carrier_s8_r256_c32_simd(double f, unsigned int phase, TSinCosTable *t, unsigned int samples, s128 *sin_out, s128 *cos_out)
{
	unsigned int Tau;  /* block  phase increment */
	unsigned int Tau1; /* sample phase increment */
	unsigned int n;    /* number of samples computed at (f_IF+fd) */
	unsigned int r;    /* remainder */
	unsigned int i;    /* counters */
	unsigned int index;
	unsigned int Phase;

	s128 *sin_ptr, *cos_ptr;   /* pointers to output vectors */
	s128 *p, *p1;  /* table pointer */
	char *p2, *sin_ptr2, *cos_ptr2; /* table pointer for the last row */

	/* BACKUP */
	movdqa_r2m(xmm0,ngeneXmmsBackup[XMM_REGISTER_SIZE*0]);
	movdqa_r2m(xmm1,ngeneXmmsBackup[XMM_REGISTER_SIZE*1]);
	movdqa_r2m(xmm2,ngeneXmmsBackup[XMM_REGISTER_SIZE*2]);
	movdqa_r2m(xmm3,ngeneXmmsBackup[XMM_REGISTER_SIZE*3]);
	/**/

	/* input f is already normalized w.r.t. the sampling frequency */
	Tau1    = (unsigned int)(f*(double)UINT_MAX); /* f represented as an integer over 32 bits */
	Tau     = (Tau1<<5);           /* mpy by 32  (samples per row), Tau represents the block phase increment */
	Phase   = phase;               /* phase represented as an integer over 32 bits */
	n       = (samples>>5);        /* computes only n samples at normalized frequency (f_IF+fd), (n=samples/32) */
	r       = samples&0x0000001f;  /* number of samples to consider in the last row, remainder of samples/32 */
	sin_ptr   = sin_out;
	cos_ptr   = cos_out;
	p         = (s128 *)(t->table);

	i=(n>>2); /* loop unrolling by a factor 4 */

	/*
	 * Phase is represented as an unsigned integer number:
	 * 		0 is 0 rad
	 * 		UINT_MAX is 2pi rad
	 * The phase of the first sample of the i-th row in the table
	 * and the phase of the first sample of the (i+1)-th row in the table
	 * differs of 2pi/256 that is UINT_MAX/256.
	 * So, the 8 most significant bits of the phase (unsigned integer)
	 * are used as the index of the row from where samples are copied.
	 * */

	while(i>0) /* loop unrolling: computes 4 rows at a time */
	{
		index = (Phase>>24); /* index of the row */
		index = (index<<2);  /* each row contains 64 samples (32 for sine and 32 for cosine: that is 4 variables of 16 bytes */
		p1 = p+index;

		movdqa_m2r(p1[0], xmm0);
		movdqa_m2r(p1[1], xmm1);
		movdqa_m2r(p1[2], xmm2);
		movdqa_m2r(p1[3], xmm3);
		movdqa_r2m(xmm0, sin_ptr[0]);
		movdqa_r2m(xmm1, sin_ptr[1]);
		movdqa_r2m(xmm2, cos_ptr[0]);
		movdqa_r2m(xmm3, cos_ptr[1]);

		Phase +=Tau;

		index = (Phase>>24);
		index = (index<<2);
		p1 = p+index;

		movdqa_m2r(p1[0], xmm0);
		movdqa_m2r(p1[1], xmm1);
		movdqa_m2r(p1[2], xmm2);
		movdqa_m2r(p1[3], xmm3);
		movdqa_r2m(xmm0, sin_ptr[2]);
		movdqa_r2m(xmm1, sin_ptr[3]);
		movdqa_r2m(xmm2, cos_ptr[2]);
		movdqa_r2m(xmm3, cos_ptr[3]);

		Phase +=Tau;

		index = (Phase>>24);
		index = (index<<2);
		p1 = p+index;

		movdqa_m2r(p1[0], xmm0);
		movdqa_m2r(p1[1], xmm1);
		movdqa_m2r(p1[2], xmm2);
		movdqa_m2r(p1[3], xmm3);
		movdqa_r2m(xmm0, sin_ptr[4]);
		movdqa_r2m(xmm1, sin_ptr[5]);
		movdqa_r2m(xmm2, cos_ptr[4]);
		movdqa_r2m(xmm3, cos_ptr[5]);

		Phase +=Tau;

		index = (Phase>>24);
		index = (index<<2);
		p1 = p+index;

		movdqa_m2r(p1[0], xmm0);
		movdqa_m2r(p1[1], xmm1);
		movdqa_m2r(p1[2], xmm2);
		movdqa_m2r(p1[3], xmm3);
		movdqa_r2m(xmm0, sin_ptr[6]);
		movdqa_r2m(xmm1, sin_ptr[7]);
		movdqa_r2m(xmm2, cos_ptr[6]);
		movdqa_r2m(xmm3, cos_ptr[7]);

		Phase +=Tau;
		sin_ptr+=8;
		cos_ptr+=8;
		i--;
	}

	i = n&0x00000003;   /* remainder */

	while(i>0) /* last rows */
	{
		index = (Phase>>24);
		index = (index<<2);
		p1 = p+index;

		movdqa_m2r(p1[0], xmm0);
		movdqa_m2r(p1[1], xmm1);
		movdqa_m2r(p1[2], xmm2);
		movdqa_m2r(p1[3], xmm3);
		movdqa_r2m(xmm0, sin_ptr[0]);
		movdqa_r2m(xmm1, sin_ptr[1]);
		movdqa_r2m(xmm2, cos_ptr[0]);
		movdqa_r2m(xmm3, cos_ptr[1]);

		Phase +=Tau;
		sin_ptr+=2;
		cos_ptr+=2;
		i--;
	}

	if(r)  /* last row: we consider only r samples */
	{
		index = (Phase >> 24);  /* 't->log2_rows' most significant bits of Phase */
		index = (index <<  2);
		p1    = p+index;
		p2    = (char *)p1; /* p2 is a char pointer */
		sin_ptr2 = (char *)sin_ptr;
		cos_ptr2 = (char *)cos_ptr;

		for(i=0; i<r; i++) /* copy row to sin and cos vectors */
		{
			sin_ptr2[i] = p2[i];
			cos_ptr2[i] = p2[i+32];
		}
		Phase += Tau1;
	}

	/* RESTORE */
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*0],xmm0);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*1],xmm1);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*2],xmm2);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*3],xmm3);
	/**/

	return(Phase);
}


double GenerateCarrier(TNGeneConfiguration *cfg, TSinCosTable *t, double f, double phaseIn, unsigned int samples, char *sinOut, char *cosOut)
{
	unsigned int offsetSin, offsetCos;
	unsigned int phase;
	double phaseOut;

	#ifdef COMPILE_64_BIT
	offsetSin = (unsigned long long int)sinOut & 0x0000000F; /* offset of the first vector w.r.t a 16 byte boundary (bytes), is always <16 */
	offsetCos = (unsigned long long int)cosOut & 0x0000000F; /* offset of the first vector w.r.t a 16 byte boundary (bytes), is always <16 */
	#else
	offsetSin = (unsigned int)sinOut & 0x0000000F; /* offset of the first vector w.r.t a 16 byte boundary (bytes), is always <16 */
	offsetCos = (unsigned int)cosOut & 0x0000000F; /* offset of the first vector w.r.t a 16 byte boundary (bytes), is always <16 */
	#endif

	if(offsetSin!=0 || offsetCos!=0)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"GenerateCarrier: pointers sinOut and/or cosOut not aligned to a 16 byte boundary");
		CloseNGene(cfg,EXIT_FAILURE);
	}
	/* normalize carrier frequency */
	f = f/cfg->fs;
	/* generate the samples */
	/*With gcc v5.4.0 on Ubuntu (14.04) 64 bits it was necessary to add the floor at the following line*/
	phase = (unsigned int)floor(phaseIn * (double)MAX_UINT_DIV_BY_2PI);
	Generate_carrier_s8_r256_c32_simd(f, phase, t, samples, (s128 *)sinOut, (s128 *)cosOut);
	/* update carrier phase 
	 * In Ubuntu 18.04, with the Old code, results in Debug and release are slightly different.
	 * Replacing with New code, results are the same. */
	phaseOut = phaseIn + (TWICE_M_PI*(double)samples*f);
	/* Old code */
	// cycles = floor(phaseOut*INV_2PI);
	// phaseOut = phaseOut - (cycles*TWICE_M_PI);
	/* New code */
	phaseOut = fmod(phaseOut,TWICE_M_PI);
	while(phaseOut<0)
	{
		phaseOut += TWICE_M_PI;
	}
	/**/
	return phaseOut;
}


unsigned int Generate_sine_s8_r256_c32_simd(double f, unsigned int phase, TSinCosTable *t, unsigned int samples, s128 *sin_out)
{
	unsigned int Tau;  /* block  phase increment */
	unsigned int Tau1; /* sample phase increment */
	unsigned int n;    /* number of samples computed at (f_IF+fd) */
	unsigned int r;    /* remainder */
	unsigned int i;    /* counters */
	unsigned int index;
	unsigned int Phase;

	s128 *sin_ptr;   /* pointer to output vector */
	s128 *p, *p1;  /* table pointer */
	char *p2, *sin_ptr2; /* table pointer for the last row */

	/* BACKUP */
	movdqa_r2m(xmm0,ngeneXmmsBackup[XMM_REGISTER_SIZE*0]);
	movdqa_r2m(xmm1,ngeneXmmsBackup[XMM_REGISTER_SIZE*1]);
	/**/

	/* input f is already normalized w.r.t. the sampling frequency */
	Tau1    = (unsigned int)(f*(double)UINT_MAX); /* f represented as an integer over 32 bits */
	Tau     = (Tau1<<5);           /* mpy by 32  (samples per row), Tau represents the block phase increment */
	Phase   = phase;               /* phase represented as an integer over 32 bits */
	n       = (samples>>5);        /* computes only n samples at normalized frequency (f_IF+fd), (n=samples/32) */
	r       = samples&0x0000001f;  /* number of samples to consider in the last row, remainder of samples/32 */
	sin_ptr   = sin_out;
	p         = (s128 *)(t->table);

	i=(n>>2); /* loop unrolling by a factor 4 */

	/*
	 * Phase is represented as an unsigned integer number:
	 * 		0 is 0 rad
	 * 		UINT_MAX is 2pi rad
	 * The phase of the first sample of the i-th row in the table
	 * and the phase of the first sample of the (i+1)-th row in the table
	 * differs of 2pi/256 that is UINT_MAX/256.
	 * So, the 8 most significant bits of the phase (unsigned integer)
	 * are used as the index of the row from where samples are copied.
	 * */

	while(i>0) /* loop unrolling: computes 4 rows at a time */
	{
		index = (Phase>>24); /* index of the row */
		index = (index<<2);  /* each row contains 64 samples (32 for sine and 32 for cosine: that is 4 variables of 16 bytes */
		p1 = p+index;

		movdqa_m2r(p1[0], xmm0);
		movdqa_m2r(p1[1], xmm1);
		movdqa_r2m(xmm0, sin_ptr[0]);
		movdqa_r2m(xmm1, sin_ptr[1]);

		Phase +=Tau;

		index = (Phase>>24);
		index = (index<<2);
		p1 = p+index;

		movdqa_m2r(p1[0], xmm0);
		movdqa_m2r(p1[1], xmm1);
		movdqa_r2m(xmm0, sin_ptr[2]);
		movdqa_r2m(xmm1, sin_ptr[3]);

		Phase +=Tau;

		index = (Phase>>24);
		index = (index<<2);
		p1 = p+index;

		movdqa_m2r(p1[0], xmm0);
		movdqa_m2r(p1[1], xmm1);
		movdqa_r2m(xmm0, sin_ptr[4]);
		movdqa_r2m(xmm1, sin_ptr[5]);

		Phase +=Tau;

		index = (Phase>>24);
		index = (index<<2);
		p1 = p+index;

		movdqa_m2r(p1[0], xmm0);
		movdqa_m2r(p1[1], xmm1);
		movdqa_r2m(xmm0, sin_ptr[6]);
		movdqa_r2m(xmm1, sin_ptr[7]);

		Phase +=Tau;
		sin_ptr+=8;
		i--;
	}

	i = n&0x00000003;   /* remainder */

	while(i>0) /* last rows */
	{
		index = (Phase>>24);
		index = (index<<2);
		p1 = p+index;

		movdqa_m2r(p1[0], xmm0);
		movdqa_m2r(p1[1], xmm1);
		movdqa_r2m(xmm0, sin_ptr[0]);
		movdqa_r2m(xmm1, sin_ptr[1]);

		Phase +=Tau;
		sin_ptr+=2;
		i--;
	}

	if(r)  /* last row: we consider only r samples */
	{
		index = (Phase >> 24);  /* 't->log2_rows' most significant bits of Phase */
		index = (index <<  2);
		p1    = p+index;
		p2    = (char *)p1; /* p2 is a char pointer */
		sin_ptr2 = (char *)sin_ptr;

		for(i=0; i<r; i++) /* copy row to sin and cos vectors */
		{
			sin_ptr2[i] = p2[i];
		}
		Phase += Tau1;
	}

	/* RESTORE */
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*0],xmm0);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*1],xmm1);
	/**/

	return(Phase);
}


double GenerateSine(TNGeneConfiguration *cfg, TSinCosTable *t, double f, double phaseIn, unsigned int samples, char *sinOut)
{
	unsigned int offsetSin;
	unsigned int phase;
	double cycles;
	double phaseOut;

	#ifdef COMPILE_64_BIT
	offsetSin = (unsigned long long int)sinOut & 0x0000000F; /* offset of the first vector w.r.t a 16 byte boundary (bytes), is always <16 */
	#else
	offsetSin = (unsigned int)sinOut & 0x0000000F; /* offset of the first vector w.r.t a 16 byte boundary (bytes), is always <16 */
	#endif

	if(offsetSin!=0)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"GenerateSine: pointers sinOut not aligned to a 16 byte boundary");
		CloseNGene(cfg,EXIT_FAILURE);
	}
	/* normalize carrier frequency */
	f = f/cfg->fs;
	/* generate the samples */
	phase = (unsigned int)(phaseIn * (double)MAX_UINT_DIV_BY_2PI);
	Generate_sine_s8_r256_c32_simd(f, phase, t, samples, (s128 *)sinOut);
	/* update carrier phase */
	phaseOut = phaseIn + (TWICE_M_PI*(double)samples*f);
	cycles = floor(phaseOut*INV_2PI);
	phaseOut = phaseOut - (cycles*TWICE_M_PI);
	return phaseOut;
}
