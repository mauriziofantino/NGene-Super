/*
 * @file fft.c
 *
 * @author Alessandro Nordio
 *
 * @date 25-07-2007
 *
 * @brief Header file for Fast Fourier Transform library
 *        Some minor changes introduced by Mario Nicola (20/08/2014)
*/
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "ngeneConstants.h"
#include "fft.h"
#include "ngeneConfiguration.h"
#include "ioFunctions.h"
#include "closeManager.h"
#include "ngeneConstants.h"


/* global variable for the backup of XMM registers */
extern char *ngeneXmmsBackup;


// Not-ANSI
#ifndef __USE_XOPEN2K
#define __USE_XOPEN2K
#endif

/**
 * @def MAX_LOG2_FFT_SIZE
 * @brief The log2 of the maximum supported FFT size
 */
#define MAX_LOG2_FFT_SIZE 	14

/**
 * @brief Generate reverse bit indices
 *
 * @param t  Structure where reverse bit indices are stored
 */
void GenerateReverseBits(TFftTable *t)
{
	unsigned int i, j, rev, index;

	for(j=0;j<t->maxFftSize;j++)
	{
		index = j;
		for(i=rev=0;i<t->maxLog2FftSize;i++)
		{
			rev = (rev << 1) | (index & 1);
			index >>= 1;
		}
		t->reverse[j] = rev;
	}
}

/**
 * @brief Generate twiddle factors
 *
 * @param t  Structure where twiddles are stored
 */
void GenerateTwiddles(TFftTable *t)
{
	unsigned int i;
	unsigned int N;
	unsigned int displacement;
	unsigned int samples;
	short *tw;

	N = t->maxFftSize;
	short s[N/2], c[N/2];   // sin and cos vectors

	for(i=0;i<N/2;i++)
	{
		s[i] = -(short)(16384*sin(2.0*M_PI*i/N));
		c[i] = +(short)(16384*cos(2.0*M_PI*i/N));
	}
	tw = (short *)(t->twiddles);

	displacement = N/8;
	samples      = 4;

	while(displacement)
	{
		for(i=0;i<samples;i++)
		{
			tw[0] =  c[i*displacement];
			tw[1] = -s[i*displacement];
			tw[2] =  s[i*displacement];
			tw[3] =  c[i*displacement];
			tw+=4;
		}
		displacement/=2;
		samples*=2;
	}
}


int InitFftTable(TNGeneConfiguration *cfg,TFftTable *t)
{
	t->maxLog2FftSize = MAX_LOG2_FFT_SIZE;
	if((t->maxLog2FftSize > 32) || (t->maxLog2FftSize<3))
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"InitFftTable: invalid MAX_LOG2_FFT_SIZE, should be less than 32, greater than 2");
		CloseNGene(cfg,EXIT_FAILURE);
	}
	t->maxFftSize = (1<<t->maxLog2FftSize);

	/* Generate twiddles and reverse bits */
	t->reverse = (unsigned int *)malloc(t->maxFftSize*sizeof(unsigned int));
	if(t->reverse==NULL)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"InitFftTable: malloc() returned NULL pointer");
		CloseNGene(cfg,EXIT_FAILURE);
	}
	/* allocate memory for twiddles. We use posix_memalign instead of malloc in order to align to a 16 boundary */
	if(posix_memalign((void **)&(t->twiddles), POSIX_ALIGNMENT, (t->maxFftSize-4)*sizeof(s128)/2) != 0)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"InitFftTable: posix_memalign() returned non zero value");
		CloseNGene(cfg,EXIT_FAILURE);
	}
	GenerateTwiddles(t);
	GenerateReverseBits(t);

	return SUCCESS_RETURN;
}


void DeinitFftTable(TFftTable *t)
{
	free(t->reverse);
	free(t->twiddles);
}


int PerformFft(TNGeneConfiguration *cfg,TFftTable *table,short *a,short *b,unsigned int log2Size,int gain)
{
	unsigned int k, i,loop;
	unsigned int log2_blocks, log2_block_size, block_size, blocks;
	s128 twiddle4;
	s128 twiddle2;
	s128 *ptr, *ptr2;
	s128 *t;
	int  *ptr_a, *ptr_b;

	unsigned int *r;               // pointer to reverse bits
	unsigned int rev_displacement; // displacement in reading reverse bit vector
	s128 *twiddle;

	#ifdef DEBUG_AND_VERIFY_FFT
	int index2;
	#endif

	/* BACKUP */
	movdqa_r2m(xmm0,ngeneXmmsBackup[XMM_REGISTER_SIZE*0]);
	movdqa_r2m(xmm1,ngeneXmmsBackup[XMM_REGISTER_SIZE*1]);
	movdqa_r2m(xmm2,ngeneXmmsBackup[XMM_REGISTER_SIZE*2]);
	movdqa_r2m(xmm3,ngeneXmmsBackup[XMM_REGISTER_SIZE*3]);
	/**/

	#ifdef COMPILE_64_BIT
	if(((unsigned long long int)(a) & 0x0000000f) ||  ((unsigned long long int)(b) & 0x0000000f))
	#else
	if(((unsigned int)(a) & 0x0000000f) ||  ((unsigned int)(b) & 0x0000000f))
	#endif
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"fft: Input pointers 'a' and/or 'b' are not aligned to 16 byte boundary");
		CloseNGene(cfg,EXIT_FAILURE);
	}
	if(log2Size<3)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR, "fft: log2Size too small, should be at least 3");
		return FAILURE_RETURN;
	}
	if(gain>0 || -gain>(int)log2Size)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR, "fft: gain should be in  -log2Size<= gain <=0");
		return FAILURE_RETURN;
	}

	// set twiddles for block size 2 and 4
	twiddle2.w[0] =  1;
	twiddle2.w[1] =  1;
	twiddle2.w[2] = -1;
	twiddle2.w[3] = -1;
	twiddle2.w[4] =  1;
	twiddle2.w[5] =  1;
	twiddle2.w[6] = -1;
	twiddle2.w[7] = -1;

	twiddle4.w[0] =  1;
	twiddle4.w[1] =  1;
	twiddle4.w[2] =  1;
	twiddle4.w[3] = -1;
	twiddle4.w[4] = -1;
	twiddle4.w[5] = -1;
	twiddle4.w[6] = -1;
	twiddle4.w[7] =  1;

	ptr_a = (int *)a;
	ptr_b = (int *)b;

	// Here we do reverse bit ordering
	r = table->reverse;
	rev_displacement = 1<<(table->maxLog2FftSize-log2Size);
	for(i=0;i<(unsigned int)(1<<log2Size);i++)
		ptr_b[r[rev_displacement*i]] = ptr_a[i];


	// ********** Step 1: Butterfly for block_size=2 ********************

	log2_blocks  = log2Size - 1;  // block_size = 2, blocks = size/2
	ptr = (s128 *)&b[0];          // set pointer to the first sample
	k = 1<<(log2_blocks-2);       // k = blocks/4

	// Does 4 blocks at time, ptr[0] = [x0, x1, x2, x3], each x has format (xr, xi), where xr and xi are signed short
	//                        ptr[1] = [x4, x5, x6, x7]
	if(gain)
	{
		// here we reduce the signal dynamic (we do shift right by 1)
		while(k)
		{
			pshufd_im2r(0xf5, ptr[0], xmm1);  // 1 - xmm1 = [ x2, x2, x3, x3 ]
			pshufd_im2r(0xa0, ptr[0], xmm0);  // 1 - xmm0 = [ x0, x0, x1, x1 ]
			pmullw_m2r(twiddle2, xmm1);       // 1 - xmm1 = [ x2, -x2, x3, -x3 ]
			pshufd_im2r(0xf5, ptr[1], xmm3);  // 2 - xmm3 = [ x6, x6, x7, x7 ]
			paddsw_r2r(xmm1,xmm0);            // 1 - xmm0 = [ x0+x2, x0-x2, x1+x3, x1-x3 ]
			pshufd_im2r(0xa0, ptr[1], xmm2);  // 2 - xmm2 = [ x4, x4, x5, x5 ]
			pmullw_m2r(twiddle2, xmm3);       // 2 - xmm3 = [ x6, -x6, x7, -x7 ]
			psraw_i2r(1, xmm0);               // 1-  shift right by 1
			movdqa_r2m(xmm0, ptr[0]);         // 1 - store
			paddsw_r2r(xmm3,xmm2);            // 2 - xmm2 = [ x4+x6, x4-x6, x5+x7, x5-x7 ]
			psraw_i2r(1, xmm2);               // 2-  shift right by 1
			movdqa_r2m(xmm2, ptr[1]);         // 2 - store

            /**/
            #ifdef DEBUG_AND_VERIFY_FFT
            printf("ptr=[");
            for(index2=0;index2<8;index2++)
            {

                   printf("%d ",ptr->w[index2]);
            }
            printf("]\n");
            printf("ptr+1=[");
            for(index2=0;index2<8;index2++)
            {

                   printf("%d ",(ptr+1)->w[index2]);
            }
            printf("]\n");
            #endif
            /**/

			ptr+=2;
			k--;
		}
		gain++;
	}
	else
	{
		// here we do not reduce the signal dynamic (we do not shift right by 1)
		while(k)
		{
			pshufd_im2r(0xf5, ptr[0], xmm1);  // 1 - xmm1 = [ x2, x2, x3, x3 ]
			pshufd_im2r(0xa0, ptr[0], xmm0);  // 1 - xmm0 = [ x0, x0, x1, x1 ]
			pmullw_m2r(twiddle2, xmm1);       // 1 - xmm1 = [ x2, -x2, x3, -x3 ]
			pshufd_im2r(0xf5, ptr[1], xmm3);  // 2 - xmm3 = [ x6, x6, x7, x7 ]
			paddsw_r2r(xmm1,xmm0);            // 1 - xmm0 = [ x0+x2, x0-x2, x1+x3, x1-x3 ]
			pshufd_im2r(0xa0, ptr[1], xmm2);  // 2 - xmm2 = [ x4, x4, x5, x5 ]
			pmullw_m2r(twiddle2, xmm3);       // 2 - xmm3 = [ x6, -x6, x7, -x7 ]
			movdqa_r2m(xmm0, ptr[0]);         // 1 - store
			paddsw_r2r(xmm3,xmm2);            // 2 - xmm2 = [ x4+x6, x4-x6, x5+x7, x5-x7 ]
			movdqa_r2m(xmm2, ptr[1]);         // 2 - store

			ptr+=2;
			k--;
		}
	}


	// ********** Step 2: Butterfly for block_size=4 ********************
	log2_blocks--;                 // block_size = 4, blocks = size/4
	ptr = (s128 *)&b[0];           // set pointer to the first sample
	k   = 1<<(log2_blocks-1);      // k = blocks/2

	// Does 2 blocks at time, ptr[0] = [x0, x1, x2, x3], each x has format (xr, xi), where xr and xi are signed short
	//                        ptr[1] = [x4, x5, x6, x7]
	if(gain)
	{
		while(k)
		{
			// here we reduce the signal dynamic (we do shift right by 1)
			pshufd_im2r(0xee, ptr[0], xmm1);  // 1 - xmm1 = [ x2, x3, x2, x3 ]
			pshufd_im2r(0x44, ptr[0], xmm0);  // 1 - xmm0 = [ x0, x1, x0, x1 ]
			pshuflw_ir2r(0xb4, xmm1, xmm1);   // 1 - xmm1 = [ x2r, x2i, x3i, x3r, x2r, x2i, x3r, x3i ]
			pshufd_im2r(0xee, ptr[1], xmm3);  // 2 - xmm3 = [ x6, x7, x6, x7 ]
			pshufhw_ir2r(0xb4, xmm1, xmm1);   // 1 - xmm1 = [ x2r, x2i, x3i, x3r,  x2r, x2i, x3i, x3r ]
			pshufd_im2r(0x44, ptr[1], xmm2);  // 2 - xmm2 = [ x4, x5, x4, x5 ]
			pmullw_m2r(twiddle4, xmm1);       // 1 - xmm1 = [ x2r, x2i, -x3i, -x3r, -x2r, -x2i, -x3i, x3r ]
			pshuflw_ir2r(0xb4, xmm3, xmm3);   // 2 - xmm3 = [ x6r, x6i, x7i, x7r,  x6r, x6i, x7r, x7i ]
			paddsw_r2r(xmm1,xmm0);            // 1 - xmm0 = [ x0r+x2r, x0i+x2i, x1r-x3i, x1i-x3r, x0r-x2r, x0i-x2i, x1r-x3i, x1i+x3r ]
			pshufhw_ir2r(0xb4, xmm3, xmm3);   // 2 - xmm3 = [ x6r, x6i, x7i, x7r,  x6r, x6i, x7i, x7r ]
			psraw_i2r(1, xmm0);               // 1-  shift right by 1
			movdqa_r2m(xmm0, ptr[0]);         // 1 - store
			pmullw_m2r(twiddle4, xmm3);       // 2 - xmm3 = [ x6r, x6i, -x7i, -x7r, -x6r, -x6i, -x7i, x7r ]
			paddsw_r2r(xmm3,xmm2);            // 2 - xmm2 = [ x4r+x6r, x4i+x6i, x5r-x7i, x5i-x7r, x4r-x6r, x4i-x6i, x5r-x7i, x5i+x7r ]
			psraw_i2r(1, xmm2);               // 1-  shift right by 1
			movdqa_r2m(xmm2, ptr[1]);         // 2 - store

            /**/
            #ifdef DEBUG_AND_VERIFY_FFT
            printf("ptr=[");
            for(index2=0;index2<8;index2++)
            {

                   printf("%d ",ptr->w[index2]);
            }
            printf("]\n");
            printf("ptr+1=[");
            for(index2=0;index2<8;index2++)
            {

                   printf("%d ",(ptr+1)->w[index2]);
            }
            printf("]\n");
            #endif
            /**/

			ptr+=2;
			k--;
		}
		gain++;
	}
	else
	{
		// here we do not reduce the signal dynamic (we do not shift right by 1)
		while(k)
		{
			pshufd_im2r(0xee, ptr[0], xmm1);  // 1 - xmm1 = [ x2, x3, x2, x3 ]
			pshufd_im2r(0x44, ptr[0], xmm0);  // 1 - xmm0 = [ x0, x1, x0, x1 ]
			pshuflw_ir2r(0xb4, xmm1, xmm1);   // 1 - xmm1 = [ x2r, x2i, x3i, x3r, x2r, x2i, x3r, x3i ]
			pshufd_im2r(0xee, ptr[1], xmm3);  // 2 - xmm3 = [ x6, x7, x6, x7 ]
			pshufhw_ir2r(0xb4, xmm1, xmm1);   // 1 - xmm1 = [ x2r, x2i, x3i, x3r,  x2r, x2i, x3i, x3r ]
			pshufd_im2r(0x44, ptr[1], xmm2);  // 2 - xmm2 = [ x4, x5, x4, x5 ]
			pmullw_m2r(twiddle4, xmm1);       // 1 - xmm1 = [ x2r, x2i, -x3i, -x3r, -x2r, -x2i, -x3i, x3r ]
			pshuflw_ir2r(0xb4, xmm3, xmm3);   // 2 - xmm3 = [ x6r, x6i, x7i, x7r,  x6r, x6i, x7r, x7i ]
			paddsw_r2r(xmm1,xmm0);            // 1 - xmm0 = [ x0r+x2r, x0i+x2i, x1r-x3i, x1i-x3r, x0r-x2r, x0i-x2i, x1r-x3i, x1i+x3r ]
			pshufhw_ir2r(0xb4, xmm3, xmm3);   // 2 - xmm3 = [ x6r, x6i, x7i, x7r,  x6r, x6i, x7i, x7r ]
			movdqa_r2m(xmm0, ptr[0]);         // 1 - store
			pmullw_m2r(twiddle4, xmm3);       // 2 - xmm3 = [ x6r, x6i, -x7i, -x7r, -x6r, -x6i, -x7i, x7r ]
			paddsw_r2r(xmm3,xmm2);            // 2 - xmm2 = [ x4r+x6r, x4i+x6i, x5r-x7i, x5i-x7r, x4r-x6r, x4i-x6i, x5r-x7i, x5i+x7r ]
			movdqa_r2m(xmm2, ptr[1]);         // 2 - store
			ptr+=2;
			k--;
		}
	}

	// ********** Steps 3 to log2_size  ********************
	log2_block_size = 3;                  // we start with block_size = 8
	log2_blocks --;
	twiddle = table->twiddles;

	while(log2_blocks+1)
	{
		block_size = 1<<log2_block_size;
		blocks     = 1<<log2_blocks;
		loop       = 1<<(log2_block_size-3); // number of loops per block, each loop processes 8 samples, hence loop = blocks = block_size/8
		ptr  = (s128 *)&b[0];                // pointer ptr  points to the 1st half of the block
		ptr2 = (s128 *)&b[block_size];       // pointer ptr2 points to the 2nd half of the block

		k=blocks;

		if(gain)
		{
			// here we reduce the signal dynamic (we do shift right by 1)
			while(k)                            // for each block
			{
				t = twiddle;                      // pointer to the first twiddle of the current stage
				i=loop;
				while(i)                             // loop
				{
					pshufd_im2r(0x50, ptr2[0], xmm0);  // xmm0 = [ x4, x4, x5, x5 ]
					pshufd_im2r(0xfa, ptr2[0], xmm1);  // xmm1 = [ x6, x6, x7, x7 ]
					pmaddwd_m2r(t[0], xmm0);           // xmm0 = [ w4r, w4i, w5r, w5i]
					pmaddwd_m2r(t[1], xmm1);           // xmm1 = [ w6r, w6i, w7r, w7i]
					psrad_i2r(14,xmm0);                // xmm0 = [ w4r, w4i, w5r, w5i]
					psrad_i2r(14,xmm1);                // xmm1 = [ w6r, w6i, w7r, w7i]
					movdqa_m2r(ptr[0], xmm2);          // xmm2 = [ x0, x1, x2, x3]
					packssdw_r2r(xmm1, xmm0);          // xmm0 = [ w4, w5, w6, w7]
					psubsw_r2r(xmm0,xmm2);             // xmm2 = [ x0-w4, x1-w5, x2-w6, x3-w7]
					paddsw_m2r(ptr[0],xmm0);           // xmm0 = [ x0+w4, x1+w5, x2+w6, x3+w7]
					psraw_i2r(1, xmm2);                // shift right by 1
					psraw_i2r(1, xmm0);                // shift right by 1
					movdqa_r2m(xmm2, ptr2[0]);         // store
					movdqa_r2m(xmm0, ptr[0]);          // store

					//**
					#ifdef DEBUG_AND_VERIFY_FFT
					printf("ptr=[");
					for(index2=0;index2<8;index2++)
					{

						   printf("%d ",ptr->w[index2]);
					}
					printf("]\n");
					printf("ptr2=[");
					for(index2=0;index2<8;index2++)
					{

						   printf("%d ",ptr2->w[index2]);
					}
					printf("]\n");
					#endif
					/**/

					ptr++;
					ptr2++;
					t+=2;
					i--;
				}
				ptr  += loop;                        // move ptr  to the 1st half of the next block
				ptr2 += loop;                        // move ptr2 to the 2nd half of the next block
				k--;
			}
			gain++;
		}
		else
		{
			// here we do not reduce the signal dynamic (we do not shift right by 1)
			while(k)                            // for each block
			{
				t = twiddle;                      // pointer to the first twiddle of the current stage
				i=loop;
				while(i)                             // loop
				{
					pshufd_im2r(0x50, ptr2[0], xmm0);  // xmm0 = [ x4, x4, x5, x5 ]
					pshufd_im2r(0xfa, ptr2[0], xmm1);  // xmm1 = [ x6, x6, x7, x7 ]
					pmaddwd_m2r(t[0], xmm0);           // xmm0 = [ w4r, w4i, w5r, w5i]
					pmaddwd_m2r(t[1], xmm1);           // xmm1 = [ w6r, w6i, w7r, w7i]
					psrad_i2r(14,xmm0);                // xmm0 = [ w4r, w4i, w5r, w5i]
					psrad_i2r(14,xmm1);                // xmm1 = [ w6r, w6i, w7r, w7i]
					movdqa_m2r(ptr[0], xmm2);          // xmm2 = [ x0, x1, x2, x3]
					packssdw_r2r(xmm1, xmm0);          // xmm0 = [ w4, w5, w6, w7]
					psubsw_r2r(xmm0,xmm2);             // xmm2 = [ x0-w4, x1-w5, x2-w6, x3-w7]
					paddsw_m2r(ptr[0],xmm0);           // xmm0 = [ x0+w4, x1+w5, x2+w6, x3+w7]
					movdqa_r2m(xmm2, ptr2[0]);         // store
					movdqa_r2m(xmm0, ptr[0]);          // store

                    /**/
                    #ifdef DEBUG_AND_VERIFY_FFT
                    printf("ptr=[");
                    for(index2=0;index2<8;index2++)
                    {

                           printf("%d ",ptr->w[index2]);
                    }
                    printf("]\n");
                    printf("ptr2=[");
                    for(index2=0;index2<8;index2++)
                    {

                           printf("%d ",ptr2->w[index2]);
                    }
                    printf("]\n");
                    #endif
                    /**/

					ptr++;
					ptr2++;
					t+=2;
					i--;
				}
				ptr  += loop;                        // move ptr  to the 1st half of the next block
				ptr2 += loop;                        // move ptr2 to the 2nd half of the next block
				k--;
			}
		}

		log2_blocks--;                        // half number of blocks
		log2_block_size++;                    // double block size
		twiddle += 2*loop;                    // update twiddle pointer, for next stage
	}

	/* RESTORE */
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*0],xmm0);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*1],xmm1);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*2],xmm2);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*3],xmm3);
	/**/

	return SUCCESS_RETURN;
}

