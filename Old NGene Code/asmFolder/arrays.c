/**
 * @file arrays.c
 *
 * @author Alessandro Nordio
 *
 * @date 18/05/2007
 *
 * @version 1.0.0
 *
 * @brief Functions that elaborate arrays.
 *        Implementation is optimized through the use of assembly.
 *        Some minor changes introduced by Mario Nicola (20/08/2014)
 */
#include <stdlib.h>
#include "ngeneConstants.h"
#include "arrays.h"
#include "simd.h"
#include "ngeneConfiguration.h"
#include "ioFunctions.h"
#include "closeManager.h"
#include "ngeneConstants.h"

/* global variable for the backup of XMM registers */
extern char *ngeneXmmsBackup;

/* XOR PRODUCT FUNCTIONS */

void ArrayXorAligned123(s128 *a,s128 *b,s128 *c,unsigned int n)
{
	// pointers 'a', 'b', 'c' are aligned
	// can be further optimized by unrolling the loops
	unsigned int i;
	
	/* BACKUP */
	movdqa_r2m(xmm0,ngeneXmmsBackup[XMM_REGISTER_SIZE*0]);
	/**/

	for(i=0; i<n; i++)
	{
		movdqa_m2r(a[0],xmm0);
		pxor_m2r(b[0],xmm0);
		movdqa_r2m(xmm0,c[0]);
		a++;
		b++;
		c++;
	}
	
	/* RESTORE */
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*0],xmm0);
}

void ArrayXorAligned23(char *a,s128 *b,s128 *c,unsigned int n)
{
	// can be further optimized by unrolling the loops
	unsigned int i;

	/* BACKUP */
	movdqa_r2m(xmm0,ngeneXmmsBackup[XMM_REGISTER_SIZE*0]);
	/**/

	for(i=0; i<n;i++)
	{
		movdqu_m2r(a[0],xmm0);
		pxor_m2r(b[0],xmm0);
		movdqa_r2m(xmm0,c[0]);
		a+=XMM_REGISTER_SIZE;
		b++;
		c++;
	}
	
	/* RESTORE */
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*0],xmm0);
}

void ArrayXorAligned3(char *a,char *b,s128 *c,unsigned int n)
{
	// can be further optimized by unrolling the loops
	unsigned int i;

	/* BACKUP */
	movdqa_r2m(xmm0,ngeneXmmsBackup[XMM_REGISTER_SIZE*0]);
	movdqa_r2m(xmm1,ngeneXmmsBackup[XMM_REGISTER_SIZE*1]);
	/**/

	for(i=0; i<n;i++)
	{
		movdqu_m2r(a[0],xmm0);
		movdqu_m2r(b[0],xmm1);
		pxor_r2r(xmm1,xmm0);
		movdqa_r2m(xmm0,c[0]);
		a+=XMM_REGISTER_SIZE;
		b+=XMM_REGISTER_SIZE;
		c++;
	}
	
	/* RESTORE */
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*0],xmm0);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*1],xmm1);
}

/* Change w.r.t. Nordio function:
 * Now it aligns output pointer, because write operation is the slowest with unaligned pointers */
void ArrayXor(char *a, char *b, char *c, unsigned int n)
{
	unsigned int i;
	unsigned int offset1, offset2; /* pointers a and b offsets w.r.t a 16 byte boundary */
	unsigned int offset3; /* pointer  c offset w.r.t a 16 byte boundary */

	if(n==0)
		return;

	if(n<XMM_REGISTER_SIZE)  /* n too small, we cannot use SIMD techniques */
	{
		for(i=0; i<n; i++)
			c[i] = a[i] ^ b[i];
		return;
	}

	/* In order to efficiently employ SIMD output pointer must be aligned with a 16 bytes boundary */
	/* Align output array */
	#ifdef COMPILE_64_BIT
	offset3 = (XMM_REGISTER_SIZE - ((unsigned long long int)c & 0x0000000f)) &0x0000000f;
	#else
	offset3 = (XMM_REGISTER_SIZE - ((unsigned int)c & 0x0000000f)) &0x0000000f;
	#endif
	for(i=0; i<offset3; i++)
	{
		c[i] = a[i] ^ b[i];
	}
	a += offset3;
	b += offset3;
	c += offset3;
	n -= offset3;
	#ifdef COMPILE_64_BIT
	offset1 = (XMM_REGISTER_SIZE - ((unsigned long long int)a & 0x0000000f)) &0x0000000f;
	offset2 = (XMM_REGISTER_SIZE - ((unsigned long long int)b & 0x0000000f)) &0x0000000f;
	#else
	offset1 = (XMM_REGISTER_SIZE - ((unsigned int)a & 0x0000000f)) &0x0000000f;
	offset2 = (XMM_REGISTER_SIZE - ((unsigned int)b & 0x0000000f)) &0x0000000f;
	#endif
	// offset3 = (XMM_REGISTER_SIZE - ((unsigned int)c & 0x0000000f)) &0x0000000f; /* it should be 0x00!!!, now */

	/* call the optimized function */
	if((offset1==0) && (offset2==0)) /* both a and b aligned */
		ArrayXorAligned123((s128 *)a,(s128 *)b,(s128 *)c,n>>4);
	else if(offset1==0) /* a aligned */
		ArrayXorAligned23(b,(s128 *)a,(s128 *)c,n>>4);
	else if(offset2==0) /* b aligned */
		ArrayXorAligned23(a,(s128 *)b,(s128 *)c,n>>4);
	else
		ArrayXorAligned3(a,b,(s128 *)c,n>>4);

	/* Step 3: dot product tail */
	a  += (n & 0xfffffff0);  /* update pointer 'a' (adds 16*(n>>4) elements, i.e. n>>4 blocks) */
	b  += (n & 0xfffffff0);  /* update pointer 'b' (adds 16*(n>>4) elements  i.e. n>>4 blocks) */
	c  += (n & 0xfffffff0);  /* update pointer 'c' (adds 16*(n>>4) elements  i.e. n>>4 blocks) */
	n   = (n & 0x0000000f);  /* remainder of n/16 */

	for(i=0; i<n; i++)
		c[i] = a[i] ^ b[i];
}


/* CORRELATION FUNCTIONS */

/*
     Computes the inner product of the two integer byte sequences using bitwise xor
     and SIMD SSE2 instruction set

     y = sum_i (a[i] bitwise_xor b[i]), i=0, ..., n-1

     a:  pointer to the first byte sequence. The sequence is 16*n bytes long. a is ALIGNED to a 16 byte boundary

     b:  pointer to the second byte sequence. The sequence is 16*n bytes long. b is ALIGNED to a 16 byte boundary

         The elements of one sequence should be signed integers with values either 0 (hex 0x00) or -1 (hex 0xff);
         The other sequence has signed integer elements 'x' with 'd' BITS OF DYNAMIC, i.e.  -2^(d-1)  <= x <= 2^(d-1)-1

     n:  sequence length, in blocks of 16 bytes each


     WARNINGS
         1) XOR:
                 if b=0  then (a xor b) = a
                 if b=-1 then (a xor b) = -a-1  !!!

         2) SATURATION:
                 the result is a 32-bit signed integer. To avoid saturation check that
                 input dynamic <=5 bit and 'n'<1024
                 If such conditions are not satisfied saturation may occur with probability
                 depending on 'n' and the input dynamic.

 */
int ArrayMacAligned(s128 *a, s128 *b, unsigned int n)
{
	unsigned int i;
	s128         sum;             /* partial result */
	int          y;               /* final   result */

	/* BACKUP */
	movdqa_r2m(xmm0,ngeneXmmsBackup[XMM_REGISTER_SIZE*0]);
	movdqa_r2m(xmm1,ngeneXmmsBackup[XMM_REGISTER_SIZE*1]);
	movdqa_r2m(xmm2,ngeneXmmsBackup[XMM_REGISTER_SIZE*2]);
	movdqa_r2m(xmm3,ngeneXmmsBackup[XMM_REGISTER_SIZE*3]);
	movdqa_r2m(xmm4,ngeneXmmsBackup[XMM_REGISTER_SIZE*4]);
	movdqa_r2m(xmm5,ngeneXmmsBackup[XMM_REGISTER_SIZE*5]);
	movdqa_r2m(xmm6,ngeneXmmsBackup[XMM_REGISTER_SIZE*6]);
	movdqa_r2m(xmm7,ngeneXmmsBackup[XMM_REGISTER_SIZE*7]);
	/**/

	pxor_r2r(xmm0,xmm0);          /* set xmm0  to 0 */
	movdqa_r2m(xmm0, sum);        /* set 'sum' to 0 */

	for(i=0; i<(n>>3); i++)       /* the loop over the 'n' block is unrolled: we process 8 blocks at a time */
	{
		movdqa_m2r(a[0],xmm0);      /* load 8 blocks of 'a' into registers */
		movdqa_m2r(a[1],xmm1);
		movdqa_m2r(a[2],xmm2);
		movdqa_m2r(a[3],xmm3);
		movdqa_m2r(a[4],xmm4);
		movdqa_m2r(a[5],xmm5);
		movdqa_m2r(a[6],xmm6);
		movdqa_m2r(a[7],xmm7);
		pxor_m2r(b[0],xmm0);        /* w0 = a[0] xor b[0] */
		pxor_m2r(b[1],xmm1);        /* w1 = a[1] xor b[1] */
		paddsb_r2r(xmm0, xmm1);     /* w0+w1  (add with signed saturation) */
		pxor_m2r(b[2],xmm2);        /* w2 = a[2] xor b[2] */
		pxor_m2r(b[3],xmm3);        /* w3 = a[3] xor b[3] */
		paddsb_r2r(xmm2, xmm3);     /* w2+w3  (add with signed saturation) */
		paddsb_r2r(xmm1, xmm3);     /* w0+w1+w2+w3  (add with signed saturation) */
		pxor_m2r(b[4],xmm4);        /* w4 = a[4] xor b[4] */
		pxor_m2r(b[5],xmm5);        /* w5 = a[5] xor b[5] */
		paddsb_r2r(xmm4, xmm5);     /* w4+w5  (add with signed saturation) */
		pxor_m2r(b[6],xmm6);        /* w6 = a[6] xor b[6] */
		pxor_m2r(b[7],xmm7);        /* w7 = a[7] xor b[7] */
		paddsb_r2r(xmm6, xmm7);     /* w6+w7  (add with signed saturation) */
		paddsb_r2r(xmm5, xmm7);     /* w4+w5+w6+w7  (add with signed saturation) */
		paddsb_r2r(xmm3, xmm7);     /* w0+w1+w2+w3+w4+w5+w6+w7 (add with signed saturation) */
		pxor_r2r(xmm5,xmm5);        /* set to 0 xmm5 */
		pxor_r2r(xmm6,xmm6);        /* set to 0 xmm6 */
		punpcklbw_r2r(xmm7, xmm5);  /* move low  8 bytes of xmm7 to words (16 bit) */
		punpckhbw_r2r(xmm7, xmm6);  /* move high 8 bytes of xmm7 to words (16 bit) */
		psraw_i2r(8, xmm5);         /* shift right xmm5 */
		psraw_i2r(8, xmm6);         /* shift right xmm6 */
		paddsw_r2r(xmm5, xmm6);     /* add xmm5 and xmm6 */
		paddsw_m2r(sum, xmm6);      /* add 'sum' to xmm6. 'sum' contains the sum of everything. */
		movdqa_r2m(xmm6, sum);      /* store 'sum' */

		a+=8;                       /* increase pointers */
		b+=8;
	}

	n = n&7;                      /* remainder of n/8; */

	if(n)                         /* if the remainder is not 0 */
	{
		pxor_r2r(xmm7,xmm7);        /* set xmm7 to 0 */
		for(i=0; i<n; i++)          /* for each remaining block */
		{
			movdqa_m2r(a[0],xmm0);    /* load block a[0] */
			pxor_m2r(b[0],xmm0);      /* compute xor with b[0] */
			paddsb_r2r(xmm0, xmm7);   /* add xor result to xmm7 */
			a++;                      /* increase pointers */
			b++;
		}

		pxor_r2r(xmm5,xmm5);        /* set xmm5 and xmm6 to 0 */
		pxor_r2r(xmm6,xmm6);
		punpcklbw_r2r(xmm7, xmm5);  /* move low  8 bytes of xmm7 to words */
		punpckhbw_r2r(xmm7, xmm6);  /* move high 8 bytes of xmm7 to words (16 bit) */
		psraw_i2r(8, xmm5);         /* shift right xmm5 */
		psraw_i2r(8, xmm6);         /* shift right xmm6 */
		paddsw_r2r(xmm5, xmm6);     /* add xmm5 and xmm6 */
		paddsw_m2r(sum, xmm6);      /* add 'sum' to xmm6 */
		movdqa_r2m(xmm6, sum);      /* store 'sum' */
	}

	y = sum.w[0]+sum.w[1]+sum.w[2]+sum.w[3]+sum.w[4]+sum.w[5]+sum.w[6]+sum.w[7]; /* sum up everything */

	/* RESTORE */
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*0],xmm0);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*1],xmm1);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*2],xmm2);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*3],xmm3);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*4],xmm4);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*5],xmm5);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*6],xmm6);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*7],xmm7);	
	/**/

	return y;
}

/*
     Computes the inner product of the two integer byte sequences using bitwise xor
     and SIMD SSE2 instruction set

     y = sum_i (a[i] bitwise_xor b[i]), i=0, ..., n-1

     a:  pointer to the first byte sequence. The sequence is 16*n bytes long. a is NOT ALIGNED to a 16 byte boundary

     b:  pointer to the second byte sequence. The sequence is 16*n bytes long. b is ALIGNED to a 16 byte boundary

         The elements of one sequence should be signed integers with values either 0 (hex 0x00) or -1 (hex 0xff);
         The other sequence has signed integer elements 'x' with 'd' BITS OF DYNAMIC, i.e.  -2^(d-1)  <= x <= 2^(d-1)-1

     n:  sequence length, in blocks of 16 bytes each


     WARNINGS
         1) XOR:
                 if b=0  then (a xor b) = a
                 if b=-1 then (a xor b) = -a-1  !!!

         2) SATURATION:
                 the result is a 32-bit signed integer. To avoid saturation check that
                 input dynamic <=5 bit and 'n'<1024
                 If such conditions are not satisfied saturation may occur with probability
                 depending on 'n' and the input dynamic.

 */
int ArrayMacUnaligned(char *a, s128 *b, unsigned int n)
{
	unsigned int i;
	s128         sum;             /* partial result */
	int          y;               /* final   result */

	/* BACKUP */
	movdqa_r2m(xmm0,ngeneXmmsBackup[XMM_REGISTER_SIZE*0]);
	movdqa_r2m(xmm1,ngeneXmmsBackup[XMM_REGISTER_SIZE*1]);
	movdqa_r2m(xmm2,ngeneXmmsBackup[XMM_REGISTER_SIZE*2]);
	movdqa_r2m(xmm3,ngeneXmmsBackup[XMM_REGISTER_SIZE*3]);
	movdqa_r2m(xmm4,ngeneXmmsBackup[XMM_REGISTER_SIZE*4]);
	movdqa_r2m(xmm5,ngeneXmmsBackup[XMM_REGISTER_SIZE*5]);
	movdqa_r2m(xmm6,ngeneXmmsBackup[XMM_REGISTER_SIZE*6]);
	movdqa_r2m(xmm7,ngeneXmmsBackup[XMM_REGISTER_SIZE*7]);
	/**/

	pxor_r2r(xmm0,xmm0);          /* set xmm0  to 0 */
	movdqa_r2m(xmm0, sum);        /* set 'sum' to 0 */

	// The pointer 'a' is NOT aligned to a 16 byte boundary. Therefore we employ unaligned double quadword move (movdqu) when we access 'a'

	for(i=0;i<(n>>3); i++)        /* loop unrolled:  8 blocks at a time */
	{
		movdqu_m2r(a[0],xmm0);      /* load 8 blocks of 'a' into registers */
		movdqu_m2r(a[16],xmm1);
		movdqu_m2r(a[32],xmm2);
		movdqu_m2r(a[48],xmm3);
		movdqu_m2r(a[64],xmm4);
		movdqu_m2r(a[80],xmm5);
		movdqu_m2r(a[96],xmm6);
		movdqu_m2r(a[112],xmm7);
		pxor_m2r(b[0],xmm0);        /* w0 = a[0] xor b[0] */
		pxor_m2r(b[1],xmm1);        /* w1 = a[1] xor b[1] */
		paddsb_r2r(xmm0, xmm1);     /* w0+w1  (add with signed saturation) */
		pxor_m2r(b[2],xmm2);        /* w2 = a[2] xor b[2] */
		pxor_m2r(b[3],xmm3);        /* w3 = a[3] xor b[3] */
		paddsb_r2r(xmm2, xmm3);     /* w2+w3  (add with signed saturation) */
		paddsb_r2r(xmm1, xmm3);     /* w0+w1+w2+w3  (add with signed saturation) */
		pxor_m2r(b[4],xmm4);        /* w4 = a[4] xor b[4] */
		pxor_m2r(b[5],xmm5);        /* w5 = a[5] xor b[5] */
		paddsb_r2r(xmm4, xmm5);     /* w4+w5  (add with signed saturation) */
		pxor_m2r(b[6],xmm6);        /* w6 = a[6] xor b[6] */
		pxor_m2r(b[7],xmm7);        /* w7 = a[7] xor b[7] */
		paddsb_r2r(xmm6, xmm7);     /* w6+w7  (add with signed saturation) */
		paddsb_r2r(xmm5, xmm7);     /* w4+w5+w6+w7  (add with signed saturation) */
		paddsb_r2r(xmm3, xmm7);     /* w0+w1+w2+w3+w4+w5+w6+w7 (add with signed saturation) */
		pxor_r2r(xmm5,xmm5);        /* set to 0 xmm5 */
		pxor_r2r(xmm6,xmm6);        /* set to 0 xmm6 */
		punpcklbw_r2r(xmm7, xmm5);  /* move low  8 bytes of xmm7 to words (16 bit) */
		punpckhbw_r2r(xmm7, xmm6);  /* move high 8 bytes of xmm7 to words (16 bit) */
		psraw_i2r(8, xmm5);	        /* shift right xmm5 */
		psraw_i2r(8, xmm6);	        /* shift right xmm6 */
		paddsw_r2r(xmm5, xmm6);     /* add xmm5 and xmm6 */
		paddsw_m2r(sum, xmm6);      /* add 'sum' to xmm6. 'sum' contains the sum of everything. */
		movdqa_r2m(xmm6, sum);      /* store 'sum' */

		a+=128;                     /* increase pointers */
		b+=8;
	}

	n = n&7;                     /* remainder of n/8; */

	if(n)                        /* if the remainder is not 0 */
	{
		pxor_r2r(xmm7,xmm7);       /* set xmm7 to 0 */
		for(i=0; i<n; i++)         /* for each remaining block */
		{
			movdqu_m2r(a[0],xmm0);   /* load block a[0] */
			pxor_m2r(b[0],xmm0);     /* compute xor with b[0] */
			paddsb_r2r(xmm0, xmm7);  /* add xor result to xmm7 */
			a+=16;		       /* increase pointers */
			b++;
		}

		pxor_r2r(xmm5,xmm5);       /* set xmm5 and xmm6 to 0 */
		pxor_r2r(xmm6,xmm6);
		punpcklbw_r2r(xmm7, xmm5); /* move low  8 bytes of xmm7 to words */
		punpckhbw_r2r(xmm7, xmm6); /* move high 8 bytes of xmm7 to words (16 bit) */
		psraw_i2r(8, xmm5);	       /* shift right xmm5 */
		psraw_i2r(8, xmm6);	       /* shift right xmm6 */
		paddsw_r2r(xmm5, xmm6);    /* add xmm5 and xmm6 */
		paddsw_m2r(sum, xmm6);     /* add 'sum' to xmm6 */
		movdqa_r2m(xmm6, sum);     /* store 'sum' */
	}
	y = sum.w[0]+sum.w[1]+sum.w[2]+sum.w[3]+sum.w[4]+sum.w[5]+sum.w[6]+sum.w[7]; /* sum up everything */

	/* RESTORE */
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*0],xmm0);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*1],xmm1);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*2],xmm2);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*3],xmm3);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*4],xmm4);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*5],xmm5);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*6],xmm6);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*7],xmm7);	
	/**/

	return y;
}


int ArrayMac(char *a, char *b, unsigned int n)
{
	int y;
	unsigned int i;
	unsigned int offset1, offset2; /* offsets of pointers a and b w.r.t a 16 byte memory boundary */

	if(n==0)
		return 0;

	/* y is the final sum. Initialize with 0 */
	y=0;

	/* if n<16 we cannot use SIMD techniques */
	if(n<16)
	{
		for(i=0; i<n; i++)
			y+= a[i] ^ b[i];
		return y;
	}

	/* In order to efficiently employ SIMD at least one pointer must be aligned to a 16 byte boundary
     When pointers are not aligned then dot product is computed in 3 steps (head,body,tail) */

	/*  Step 1: compute pointer offsets */
	#ifdef COMPILE_64_BIT
	offset1 = (XMM_REGISTER_SIZE - ((unsigned long long int)a & 0x0000000f)) & 0x0000000f;
	offset2 = (XMM_REGISTER_SIZE - ((unsigned long long int)b & 0x0000000f)) & 0x0000000f;
	#else
	offset1 = (XMM_REGISTER_SIZE - ((unsigned int)a & 0x0000000f)) & 0x0000000f;
	offset2 = (XMM_REGISTER_SIZE - ((unsigned int)b & 0x0000000f)) & 0x0000000f;
	#endif

	/*  Step 2: dot product head and body */
	if(offset1>offset2)
	{
		for(i=0; i<offset2; i++)
			y+= a[i]^b[i];

		a += offset2;
		b += offset2;
		n -= offset2;
		y += ArrayMacUnaligned(a, (s128 *)b, n>>4);
	}
	else
	{
		for(i=0; i<offset1; i++)
			y+= a[i] ^ b[i];

		a += offset1;
		b += offset1;
		n -= offset1;

		if(offset1==offset2)
			y += ArrayMacAligned((s128 *)a, (s128 *)b, n>>4);
		else
			y += ArrayMacUnaligned(b, (s128 *)a, n>>4);
	}

	/// Step 3: dot product tail
	a  += (n & 0xfffffff0); /* update pointer a (adds 16*(n>>4) elements, i.e. n>>4 blocks) */
	b  += (n & 0xfffffff0); /* update pointer b (adds 16*(n>>4) elements  i.e. n>>4 blocks) */
	n   = (n & 0x0000000f); /* remainder of n/16 */

	for(i=0; i<n; i++)
		y += a[i] ^ b[i];

	return y;
}


/* OTHERS FUNCTIONS */

void ArrayProductComplex(TNGeneConfiguration *cfg,short *a,short *b,short *c,unsigned int n)
{
	unsigned int blocks;
	int yr, yi;

	if(n==0)
		return;

	/* BACKUP */
	movdqa_r2m(xmm0,ngeneXmmsBackup[XMM_REGISTER_SIZE*0]);
	movdqa_r2m(xmm1,ngeneXmmsBackup[XMM_REGISTER_SIZE*1]);
	/**/

	#ifdef COMPILE_64_BIT
	if(((unsigned long long int)a & 0x0000000f) || ((unsigned long long int)b & 0x0000000f) || ((unsigned long long int)c & 0x0000000f))
	#else
	if(((unsigned int)a & 0x0000000f) || ((unsigned int)b & 0x0000000f) || ((unsigned int)c & 0x0000000f))
	#endif
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR, "ArrayProductComplex: pointers 'a' and/or  'b' and/or 'c' not aligned to a 16 byte boundary");
		CloseNGene(cfg,EXIT_FAILURE);
	}

	blocks = n>>2;   /* 4 samples per block */

	s128  *pa, *pb, *pc;
	short *ps_a, *ps_b, *ps_c;

	pa = (s128 *)a;
	pb = (s128 *)b;
	pc = (s128 *)c;

	while(blocks)
	{
		/* {} indicate the content of an array; a[i] is the i-th complex sample in the array a:
		 * Re(a[i]) is its real part and Im(a[i]) is its imaginary part */
		/* pa[0] = {Re(a[0]) Im(a[0]) Re(a[1]) Im(a1) Re(a[2]) Im(a[2]) Re(a[3]) Im(a[3])} */
		pshufd_im2r(0x50, pa[0], xmm0);    /* xmm0 = {Re(a[0]) Im(a[0]) Re(a[0]) Im(a[0]) Re(a[1]) Im(a[1]) Re(a[1]) Im(a[1])} */
		pshufd_im2r(0xfa, pa[0], xmm1);    /* xmm1 = {Re(a[2]) Im(a[2]) Re(a[2]) Im(a[2]) Re(a[3]) Im(a[3]) Re(a[3]) Im(a[3])} */
		/* pb[0] = {Re(b[0]) -Im(b[0]) Im(b[0]) Re(b[0]) Re(b[1]) -Im(b[1]) Im(b[1]) Re(b[1])} */
		pmaddwd_m2r(pb[0], xmm0);          /* xmm0 = {Re(c[0]) Im(c[0]) Re(c[1]) Im(c[1])} */
		/* pb[1] = {Re(b[2]) -Im(b[2]) Im(b[2]) Re(b[2]) Re(b[3]) -Im(b[3]) Im(b[3]) Re(b[3])} */
		pmaddwd_m2r(pb[1], xmm1);          /* xmm0 = {Re(c[2]) Im(c[2]) Re(c[3]) Im(c[3])} */
		psrad_i2r(16, xmm0);
		psrad_i2r(16, xmm1);
		packssdw_r2r(xmm1, xmm0);          /* xmm0 = {Re(c[0]) Im(c[0]) Re(c[1]) Im(c[1]) Re(c[2]) Im(c[2]) Re(c[3]) Im(c[3])} */
		movdqa_r2m(xmm0, pc[0]);
		pa++;
		pb+=2;
		pc++;
		blocks--;
	}

	ps_a = (short *)pa;
	ps_b = (short *)pb;
	ps_c = (short *)pc;
	n = n&0x00000003;                    /* remaining samples */

	while(n)
	{
		yr = ps_a[0]*ps_b[0]+ps_a[1]*ps_b[1];  /* Real part of the sample */
		yi = ps_a[0]*ps_b[2]+ps_a[1]*ps_b[3];  /* Imag part of the sample */

		ps_c[0] = (short)(yr>>16);
		ps_c[1] = (short)(yi>>16);
		ps_a+=2;
		ps_b+=4;
		ps_c+=2;
		n--;
	}
	
	/* RESTORE */
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*0],xmm0);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*1],xmm1);
}



int ArrayAbs2(TNGeneConfiguration *cfg, short *c, unsigned int *s, unsigned int n)
{
	/* BACKUP */
	movdqa_r2m(xmm0,ngeneXmmsBackup[XMM_REGISTER_SIZE*0]);
	/**/
	
	#ifdef COMPILE_64_BIT
	if(((unsigned long long int)(c) & 0x0000000f) ||  ((unsigned long long int)(s) & 0x0000000f))
	#else
	if(((unsigned int)(c) & 0x0000000f) ||  ((unsigned int)(s) & 0x0000000f))
	#endif
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"abs2: Input pointers are not aligned to 16 byte boundary");
		CloseNGene(cfg,EXIT_FAILURE);
	}

	n = n>>2; /* we compute 4 absolute values at a time */
	while(n)
	{
		movdqa_m2r(c[0], xmm0);
		pmaddwd_m2r(c[0], xmm0);
		movdqa_r2m(xmm0, s[0]);
		n--;
		c+=8;
		s+=4;
	}
	
	/* RESTORE */
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*0],xmm0);
	/**/
	
	return SUCCESS_RETURN;
}

void ArrayMax(unsigned int *v, unsigned int *max, unsigned int *index, unsigned int n)
{
	unsigned int i;
	*max = 0;
	*index = 0;

	for(i=0; i<n; i++)
	{
		if(v[i] > *max)
		{
			*max   = v[i];
			*index = i;
		}
	}
}


void ArrayAdd(char *a, char *b, char *c, int arrayLength)
{
	unsigned int offset_a, offset_b, offset_out;
	int index = 0;

	if(arrayLength==0)
		return;

	/* BACKUP */
	movdqa_r2m(xmm0,ngeneXmmsBackup[XMM_REGISTER_SIZE*0]);
	movdqa_r2m(xmm1,ngeneXmmsBackup[XMM_REGISTER_SIZE*1]);
	/**/

	#ifdef COMPILE_64_BIT
	offset_out = (unsigned long long int)c & 0x0f;
	#else
	offset_out = (unsigned int)c & 0x0f;
	#endif
	if(offset_out!=0)
	{
		for(index=0;index<((int)16-(int)offset_out) && index<arrayLength;index++)
			c[index] = a[index]+b[index];
	}
	/**/
	#ifdef COMPILE_64_BIT
	offset_a = ((unsigned long long int)(&(a[index]))) & 0x0f;
	offset_b = ((unsigned long long int)(&(b[index]))) & 0x0f;
	#else
	offset_a = ((unsigned int)(&(a[index]))) & 0x0f;
	offset_b = ((unsigned int)(&(b[index]))) & 0x0f;
	#endif
	if((offset_a== 0)&&(offset_b==0))
	{
		while((index+XMM_REGISTER_SIZE) <= arrayLength)
		{
			movdqa_m2r(a[index], xmm0);
			movdqa_m2r(b[index], xmm1);
			paddb_r2r(xmm0, xmm1);
			movdqa_r2m(xmm1, c[index]);
			index += XMM_REGISTER_SIZE;
		}
	}
	else if(offset_a==0)
	{
		while((index+XMM_REGISTER_SIZE) <= arrayLength)
		{
			movdqa_m2r(a[index], xmm0);
			movdqu_m2r(b[index], xmm1);
			paddb_r2r(xmm0, xmm1);
			movdqa_r2m(xmm1, c[index]);
			index += XMM_REGISTER_SIZE;
		}
	}
	else if(offset_b==0)
	{
		while((index+XMM_REGISTER_SIZE) <= arrayLength)
		{
			movdqu_m2r(a[index], xmm0);
			movdqa_m2r(b[index], xmm1);
			paddb_r2r(xmm0, xmm1);
			movdqa_r2m(xmm1, c[index]);
			index += XMM_REGISTER_SIZE;
		}
	}
	else
	{
		while((index+XMM_REGISTER_SIZE) <= arrayLength)
		{
			movdqu_m2r(a[index], xmm0);
			movdqu_m2r(b[index], xmm1);
			paddb_r2r(xmm0, xmm1);
			movdqa_r2m(xmm1, c[index]);
			index += XMM_REGISTER_SIZE;
		}
	}
	/**/
	for(;index<arrayLength;index++)
	{
		c[index] = a[index]+b[index];
	}
	
	/* RESTORE */
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*0],xmm0);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*1],xmm1);
}


void ArrayAddUnsignedInt(unsigned int *a, unsigned int *b, unsigned int *c, int arrayLength)
{
	int xmmSize = XMM_REGISTER_SIZE/sizeof(unsigned int);
	int index = 0;

	if(arrayLength==0)
		return;

	/* BACKUP */
	movdqa_r2m(xmm0,ngeneXmmsBackup[XMM_REGISTER_SIZE*0]);
	movdqa_r2m(xmm1,ngeneXmmsBackup[XMM_REGISTER_SIZE*1]);
	/**/

	while((index+xmmSize) <= arrayLength)
	{
		movdqu_m2r(a[index], xmm0);
		movdqu_m2r(b[index], xmm1);
		paddd_r2r(xmm0, xmm1);
		movdqu_r2m(xmm1, c[index]);
		index += xmmSize;
	}
	for(;index<arrayLength;index++)
	{
		c[index] = a[index]+b[index];
	}
	
	/* RESTORE */
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*0],xmm0);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*1],xmm1);
}


void ArraySub(char *a, char *b, char *c, int arrayLength)
{
	unsigned int offset_a, offset_b, offset_out;
	int index = 0;

	if(arrayLength==0)
		return;

	/* BACKUP */
	movdqa_r2m(xmm0,ngeneXmmsBackup[XMM_REGISTER_SIZE*0]);
	movdqa_r2m(xmm1,ngeneXmmsBackup[XMM_REGISTER_SIZE*1]);
	/**/

	#ifdef COMPILE_64_BIT
	offset_out = (unsigned long long int)c & 0x0f;
	#else
	offset_out = (unsigned int)c & 0x0f;
	#endif
	if(offset_out!=0)
	{
		for(index=0;index<((int)16-(int)offset_out) && index<arrayLength;index++)
			c[index] = a[index]-b[index];
	}
	/**/
	#ifdef COMPILE_64_BIT
	offset_a = ((unsigned long long int)(&(a[index]))) & 0x0f;
	offset_b = ((unsigned long long int)(&(b[index]))) & 0x0f;
	#else
	offset_a = ((unsigned int)(&(a[index]))) & 0x0f;
	offset_b = ((unsigned int)(&(b[index]))) & 0x0f;
	#endif
	if((offset_a== 0)&&(offset_b==0))
	{
		while((index+XMM_REGISTER_SIZE) <= arrayLength)
		{
			movdqa_m2r(a[index], xmm0);
			movdqa_m2r(b[index], xmm1);
			psubb_r2r(xmm1, xmm0);
			movdqa_r2m(xmm0, c[index]);
			index += XMM_REGISTER_SIZE;
		}
	}
	else if(offset_a==0)
	{
		while((index+XMM_REGISTER_SIZE) <= arrayLength)
		{
			movdqa_m2r(a[index], xmm0);
			movdqu_m2r(b[index], xmm1);
			psubb_r2r(xmm1, xmm0);
			movdqa_r2m(xmm0, c[index]);
			index += XMM_REGISTER_SIZE;
		}
	}
	else if(offset_b==0)
	{
		while((index+XMM_REGISTER_SIZE) <= arrayLength)
		{
			movdqu_m2r(a[index], xmm0);
			movdqa_m2r(b[index], xmm1);
			psubb_r2r(xmm1, xmm0);
			movdqa_r2m(xmm0, c[index]);
			index += XMM_REGISTER_SIZE;
		}
	}
	else
	{
		while((index+XMM_REGISTER_SIZE) <= arrayLength)
		{
			movdqu_m2r(a[index], xmm0);
			movdqu_m2r(b[index], xmm1);
			psubb_r2r(xmm1, xmm0);
			movdqa_r2m(xmm0, c[index]);
			index += XMM_REGISTER_SIZE;
		}
	}
	/**/
	for(;index<arrayLength;index++)
	{
		c[index] = a[index]-b[index];
	}
	
	/* RESTORE */
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*0],xmm0);
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*1],xmm1);
}


void ArrayClean(char *outputArray, int arrayLength)
{
	unsigned int offsetOut;
	int index = 0;

	if(arrayLength==0)
		return;

	/* BACKUP */
	movdqa_r2m(xmm0,ngeneXmmsBackup[XMM_REGISTER_SIZE*0]);
	/**/

	#ifdef COMPILE_64_BIT
	offsetOut = (unsigned long long int)outputArray & 0x0f;
	#else
	offsetOut = (unsigned int)outputArray & 0x0f;
	#endif
	if(offsetOut!=0)
	{
		for(index=0;index<((int)XMM_REGISTER_SIZE-(int)offsetOut) && index<arrayLength;index++)
			outputArray[index] = 0;
	}
	/**/
	pxor_r2r(xmm0, xmm0);
	while((index+XMM_REGISTER_SIZE) <= arrayLength)
	{
		movdqa_r2m(xmm0, outputArray[index]);
		index += XMM_REGISTER_SIZE;
	}
	/**/
	for(;index<arrayLength;index++)
	{
		outputArray[index] = 0;
	}
	
	/* RESTORE */
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*0],xmm0);
}


void ArrayCopy(char *inputArray,char *outputArray,int arrayLength)
{
	unsigned int inputOffset;
	unsigned int outputOffset;
	int index = 0;

	if(arrayLength==0)
		return;

	/* BACKUP */
	movdqa_r2m(xmm0,ngeneXmmsBackup[XMM_REGISTER_SIZE*0]);
	/**/

	#ifdef COMPILE_64_BIT
	outputOffset = (unsigned long long int)outputArray & 0x0f;
	#else
	outputOffset = (unsigned int)outputArray & 0x0f;
	#endif
	if(outputOffset!=0)
	{
		for(index=0;index<((int)XMM_REGISTER_SIZE-(int)outputOffset) && index<arrayLength;index++)
			outputArray[index] = inputArray[index];
	}
	/**/
	#ifdef COMPILE_64_BIT
	inputOffset = (unsigned long long int)(&(inputArray[index])) & 0x0f;
	#else
	inputOffset = (unsigned int)(&(inputArray[index])) & 0x0f;
	#endif
	if(inputOffset==0)
	{
		while((index+XMM_REGISTER_SIZE) <= arrayLength)
		{
			movdqa_m2r(inputArray[index], xmm0);
			movdqa_r2m(xmm0, outputArray[index]);
			index += XMM_REGISTER_SIZE;
		}
	}
	else
	{
		while((index+XMM_REGISTER_SIZE) <= arrayLength)
		{
			movdqu_m2r(inputArray[index], xmm0);
			movdqa_r2m(xmm0, outputArray[index]);
			index += XMM_REGISTER_SIZE;
		}
	}
	/**/
	for(;index<arrayLength;index++)
	{
		outputArray[index] = inputArray[index];
	}
	
	/* RESTORE */
	movdqa_m2r(ngeneXmmsBackup[XMM_REGISTER_SIZE*0],xmm0);
}


void CarrierComplexWipeOff(char *input1, unsigned int size1, char *input2, unsigned int size2, char *cosArray, char *sinArray, unsigned int carrierSize, char *cosDemod, char *sinDemod)
{
	if(size1>=carrierSize)
	{
		ArrayXor(input1,cosArray,cosDemod,carrierSize);
		ArrayXor(input1,sinArray,sinDemod,carrierSize);
	}
	else if(size1 + size2 >= carrierSize)
	{
		ArrayXor(input1,cosArray,cosDemod,size1);
		ArrayXor(input1,sinArray,sinDemod,size1);
		ArrayXor(input2,&(cosArray[size1]),&(cosDemod[size1]),carrierSize-size1);
		ArrayXor(input2,&(sinArray[size1]),&(sinDemod[size1]),carrierSize-size1);
	}
	/* else, do nothing
	 * (it should not happen: if not enough samples are available in the input buffer, then receiver load new samples!) */
}

void CarrierRealWipeOff(char *input1, unsigned int size1, char *input2, unsigned int size2, char *carrierArray, unsigned int carrierSize, char *demod)
{
	if(size1>=carrierSize)
	{
		ArrayXor(input1,carrierArray,demod,carrierSize);
	}
	else if(size1 + size2 >= carrierSize)
	{
		ArrayXor(input1,carrierArray,demod,size1);
		ArrayXor(input2,&(carrierArray[size1]),&(demod[size1]),carrierSize-size1);
	}
	/* else, do nothing
     * (it should not happen: if not enough samples are available in the input buffer, then receiver load new samples!) */
}
