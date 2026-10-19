/*!  simd.h 

    Author: Alessandro Nordio
    History:   19/12/2006 Added some instruction of the SSE2 instruction set
               18/07/2007 Added some instruction of the MMX  instruction set


    This file defines macros for the SIMD instruction set.
*/

/*! INSTRUCTION SET

    STRING_m2r(var, reg)  
    STRING_r2r(regs, regd)  
    STRING_r2m(reg, var)  
           
             STRING:       asm instruction
             1st argument: source 
             2nd argument: destination
             _m2r        : source is memory,   destination is register
             _r2r        : source is register, destination is register
             _r2m        : source is register, destination is memory
             _i2m        : source is an immediate, destination is register
             _im2r       : two sources: immediate and memory, destination is register
             _ir2r       : two sources: immediate and register, destination is register


             signed/unsigned byte        -> 8 bits
             signed/unsigned word        -> 16 bits
             signed/unsigned double word -> 32 bits
             signed/unsigned quad word   -> 64 bits

    Available registers: xmm0, xmm1, ... , xmm7 for SSE2 instruction set 
                          mm0,  mm1, ... ,  mm7 for MMX and SSE instruction set

    Some macros are valid for both MMX and SSE2 instruction sets. For example the macro 
 
 	\verbatim
    #define	pand_m2r(var, reg)	simd_m2r(pand, var, reg)
    \endverbatim

    can be used as
    a)    pand_m2r(p8,   mmX);      (for MMX and SSE 64-bit registers)
    b)    pand_m2r(p16, xmmX);      (for SSE2 128-bit registers)

    where
    X = 0, 1, .. 7 is the register number,
    p8  is a pointer aligned to  8 bytes ( 64 bit)
    p16 is a pointer aligned to 16 bytes (128 bit)

    Macros valid only for  64-bit registers  are marked with the tag (64)
    Macros valid only for 128-bit registers  are marked with the tag (128)
    Macros valid for both                    are marked with the tag (64-128)
*/


#ifndef _SIMD_H
#define _SIMD_H

/**
 * @def XMM_REGISTER_SIZE
 * @brief Size (in bytes) of a xmm register
 */
#define XMM_REGISTER_SIZE		16

/**
 * @def DEBUG_AND_VERIFY
 * @brief To enable debug code
 */
//#define DEBUG_AND_VERIFY


/// Define a union for a 128 bit register (for SSE2 instruction set)
typedef	union s128 {
  double                dpfp[2];  ///< 2 x Double Precision Floating Point (64 bit)
  float 		spfp[4];  ///< 4 x Single Precision Floating Point (32 bit)
  long long                q[2];  ///< 2 x Signed   Quadword (64-bit)  
  unsigned long long      uq[2];  ///< 2 x Unsigned Quadword (64-bit)    
  int			   d[4];  ///< 4 x Signed   Doubleword (32-bit)   
  unsigned int		  ud[4];  ///< 4 x Unsigned Doubleword (32-bit)  
  short			   w[8];  ///< 8 x Signed   Word (16-bit)
  unsigned short	  uw[8];  ///< 8 x Unsigned Word (16 bit)
  char			   b[16]; ///< 16x Signed   Byte (8-bit)  
  unsigned char		  ub[16]; ///< 16x Unsigned Byte (8 bit)
} s128 __attribute__ ((aligned (16))); // Aligned on an 16-byte (128-bit) boundary

/// Define a union for a 64 bit register (for MMX and SSE instruction set)
typedef	union {
	long long		    q;	///< 1 x Signed   Quadword (64-bit)
	unsigned long long	   uq;	///< 1 x Unsigned Quadword (64-bit)
	int			 d[2];	///< 2 x Signed   Doubleword (32-bit)
	unsigned int		ud[2];	///< 2 x Unsigned Doubleword (32-bit)
	short			 w[4];	///< 4 x Signed   Word (16-bit)
	unsigned short		uw[4];	///< 4 x Unsigned Word (16 bit)
	char			 b[8];	///< 8 x Signed   Byte (8-bit)
	unsigned char		ub[8];	///< 8 x Unsigned   Byte (8-bit)
	float		      spfp[2];	///< 2 x Single Precision Floating Point (32 bit)
} __attribute__ ((aligned (8))) s64;	// Aligned on an 8-byte (64-bit) boundary

// **************** BASE MACROS ************************************************************************************************
#define	simd_i2r(op, imm, reg)          __asm__ __volatile__ (#op " %0, %%" #reg : /* nothing */ : "X" (imm) )
#define	simd_m2r(op, mem, reg)          __asm__ __volatile__ (#op " %0, %%" #reg : /* nothing */ : "m" (mem))
#define	simd_r2m(op, reg, mem) 	        __asm__ __volatile__ (#op " %%" #reg ", %0" : "=m" (mem) : /* nothing */ )
#define	simd_r2r(op, regs, regd)        __asm__ __volatile__ (#op " %" #regs ", %" #regd)

#define	simd_im2r(op, imm, mem, reg)	__asm__ __volatile__ (#op " %0, %1, %%" #reg : /* nothing */ : "i" (imm), "m" (mem) )
#define	simd_ir2r(op, imm, regs, regd)	__asm__ __volatile__ (#op " %0, %%" #regs ", %%" #regd : /* nothing */ : "i" (imm) )
// **************** END BASE MACROS ********************************************************************************************


// ************************************* MACROS MAPPING SINGLE ASSEMBLER INSTRUCTIONS ******************************************

#define prefetch(mem)  __asm__ __volatile__ ("prefetch %0 " : /* nothing */ : "m" (mem) )  ///< Prefetch


// ******************************************************************
// **** Memory movement instructions
// ******************************************************************

//      MOVe Double Quadword Aligned (128) -- Only for 128-bit xmmX registers --
#define	movdqa_m2r(var, reg)	simd_m2r(movdqa, var, reg)    ///<   MOVe Double Quadword Aligned from memory   to register
#define	movdqa_r2r(regs, regd)	simd_r2r(movdqa, regs, regd)  ///<   MOVe Double Quadword Aligned from register to register
#define	movdqa_r2m(reg, var)	simd_r2m(movdqa, reg, var)    ///<   MOVe Double Quadword Aligned from register to memory

//      MOVe Double Quadword Unaligned (128) -- Only for 128-bit xmmX registers --
#define	movdqu_m2r(var, reg)	simd_m2r(movdqu, var, reg)    ///<   MOVe Double Quadword Aligned from memory   to register
#define	movdqu_r2r(regs, regd)	simd_r2r(movdqu, regs, regd)  ///<   MOVe Double Quadword Aligned from register to register
#define	movdqu_r2m(reg, var)	simd_r2m(movdqu, reg, var)    ///<   MOVe Double Quadword Aligned from register to memory   

//      MOVe Quadword aligned (64-128) -- For both mmX and xmmX registers 
#define	movq_m2r(var, reg)	simd_m2r(movq, var, reg)      ///<   MOVe Quadword aligned from memory   to register -- if used with xmmX loads 64 bits in the lower half of the register, zero extends the upper half
#define	movq_r2m(reg, var)	simd_r2m(movq, reg, var)      ///<   MOVe Quadword aligned from register to memory   -- if used with xmmX stores lower 64 bits in the specified memory address
#define	movq_r2r(regs, regd)	simd_r2r(movq, regs, regd)    ///<   MOVe Quadword aligned from register to register (only for mmX registers)

//      MOVe Doubleword aligned (64-128) -- For both mmX and xmmX registers
#define	movd_m2r(var, reg)	simd_m2r(movd, var, reg)      ///<   MOVe Doubleword aligned from memory   to register  -- Loads 32 bits in the lower part of the register, zero extends the upper part
#define	movd_r2m(reg, var)	simd_r2m(movd, reg, var)      ///<   MOVe Doubleword aligned from register to memory -- Stores lower 32 bits in the specified memory address



// ***********************************************************************************************************************************
// **** Double Precision Floating Point (DPFP) Arithmetic (128) -- Only for 128-bit xmmX registers -- a,b,c,d  DPFP  (14 instructions)  
// ***********************************************************************************************************************************

// Arithmetic instructions  (4 Instructions) 
//
// - Add (2 instructions)
#define	addpd_m2r(var, reg)	simd_m2r(addpd, var, reg)   ///<  ADD Packed Double precision floating point from memory   to register [a,b] op [c,d] -> [a+c, b+d] 
#define	addpd_r2r(regs, regd)	simd_r2r(addpd, regs, regd) ///<  ADD Packed Double precision floating point from register to register [a,b] op [c,d] -> [a+c, b+d] 

#define	addsd_m2r(var, reg)	simd_m2r(addsd, var, reg)   ///<  ADD Scalar Double precision floating point from memory   to register [a,b]; [c,d] -> [a+c, d] 
#define	addsd_r2r(regs, regd)	simd_r2r(addsd, regs, regd) ///<  ADD Scalar Double precision floating point from register to register [a,b]; [c,d] -> [a+c, d] 

// - Subtract (2 instructions)
#define	subpd_m2r(var, reg)	simd_m2r(subpd, var, reg)   ///<  SUBtract Packed Double precision floating point from memory   to register  [a,b] op [c,d] -> [a-c, b-d] 
#define	subpd_r2r(regs, regd)	simd_r2r(subpd, regs, regd) ///<  SUBtract Packed Double precision floating point from register to register  [a,b] op [c,d] -> [a-c, b-d] 

#define	subsd_m2r(var, reg)	simd_m2r(subsd, var, reg)   ///   SUBtract Scalar Double precision floating point from memory   to register  [a,b] op [c,d] -> [a-c, d] 
#define	subsd_r2r(regs, regd)	simd_r2r(subsd, regs, regd) ///   SUBtract Scalar Double precision floating point from register to register  [a,b] op [c,d] -> [a-c, d] 

// - Multiply (2 instructions )
#define	mulpd_m2r(var, reg)	simd_m2r(mulpd, var, reg)   ///<  MULtiply Packed Double precision floating point from memory   to register  [a,b] op [c,d] -> [a*c, b*d] 
#define	mulpd_r2r(regs, regd)	simd_r2r(mulpd, regs, regd) ///<  MULtiply Packed Double precision floating point from register to register  [a,b] op [c,d] -> [a*c, b*d] 

#define	mulsd_m2r(var, reg)	simd_m2r(mulsd, var, reg)   ///<  MULtiply Scalar Double precision floating point from memory   to register  [a,b] op [c,d] -> [a*c, d] 
#define	mulsd_r2r(regs, regd)	simd_r2r(mulsd, regs, regd) ///<  MULtiply Scalar Double precision floating point from register to register  [a,b] op [c,d] -> [a*c, d] 

// - Divide (2 instructions))
#define	divpd_m2r(var, reg)	simd_m2r(divpd, var, reg)   ///<  DIVide   Packed Double precision floating point from memory   to register  [a,b] op [c,d] -> [a/c, b/d] 
#define	divpd_r2r(regs, regd)	simd_r2r(divpd, regs, regd) ///<  DIVide   Packed Double precision floating point from register to register  [a,b] op [c,d] -> [a/c, b/d] 

#define	divsd_m2r(var, reg)	simd_m2r(divsd, var, reg)   ///<  DIVide   Scalar Double precision floating point from memory   to register  [a,b] op [c,d] -> [a/c, d] 
#define	divsd_r2r(regs, regd)	simd_r2r(divsd, regs, regd) ///<  DIVide   Scalar Double precision floating point from register to register  [a,b] op [c,d] -> [a/c, d] 

// - Maximum (2 instructions))
#define	maxpd_m2r(var, reg)	simd_m2r(maxpd, var, reg)   ///<  MAXimum  Packed Double precision floating point from memory   to register  [a,b] op [c,d] -> [max(a,c), max(b,d)]
#define	maxpd_r2r(regs, regd)	simd_r2r(maxpd, regs, regd) ///<  MAXimum  Packed Double precision floating point from register to register  [a,b] op [c,d] -> [max(a,c), max(b,d)]

#define	maxsd_m2r(var, reg)	simd_m2r(maxsd, var, reg)   ///<  MAXimum  Scalar Double precision floating point from memory   to register  [a,b] op [c,d] -> [max(a,c), d]
#define	maxsd_r2r(regs, regd)	simd_r2r(maxsd, regs, regd) ///<  MAXimum  Scalar Double precision floating point from register to register  [a,b] op [c,d] -> [max(a,c), d]

// - Minimum (2 instructions))
#define	minpd_m2r(var, reg)	simd_m2r(minpd, var, reg)   ///<  MINimum  Packed Double precision floating point from memory   to register  [a,b] op [c,d] -> [min(a,c), min(b,d)]
#define	minpd_r2r(regs, regd)	simd_r2r(minpd, regs, regd) ///<  MINimum  Packed Double precision floating point from register to register  [a,b] op [c,d] -> [min(a,c), min(b,d)]

#define	minsd_m2r(var, reg)	simd_m2r(minsd, var, reg)   ///<  MINimum  Scalar Double precision floating point from memory   to register  [a,b] op [c,d] -> [min(a,c), d]
#define	minsd_r2r(regs, regd)	simd_r2r(minsd, regs, regd) ///<  MINimum  Scalar Double precision floating point from register to register  [a,b] op [c,d] -> [min(a,c), d]

// - Square root (2 instructions))
#define	sqrtpd_m2r(var, reg)	simd_m2r(sqrtpd, var, reg)  ///<  SQuare RooT Packed Double precision floating point from memory   to register [a,b] op [c,d] -> [sqrt(a), sqrt(b)]
#define	sqrtpd_r2r(regs, regd)	simd_r2r(sqrtpd, regs, regd)///<  SQuare RooT Packed Double precision floating point from register to register [a,b] op [c,d] -> [sqrt(a), sqrt(b)]

#define	sqrtsd_m2r(var, reg)	simd_m2r(sqrtsd, var, reg)  ///<  SQuare RooT Scalar Double precision floating point from memory   to register [a,b] op [c,d] -> [sqrt(a), d]
#define	sqrtsd_r2r(regs, regd)	simd_r2r(sqrtsd, regs, regd)///<  SQuare RooT Scalar Double precision floating point from register to register [a,b] op [c,d] -> [sqrt(a), d]

// Logical instructions  (4 Instructions) 
//
// - And
#define	andpd_m2r(var, reg)	simd_m2r(andpd, var, reg)   ///<  bit-wise logical AND Parallel Double
#define	andpd_r2r(regs, regd)	simd_r2r(andpd, regs, regd) ///<  bit-wise logical AND Parallel Double

// - And not
#define	andnpd_m2r(var, reg)	simd_m2r(andnpd, var, reg)  ///<  bit-wise logical AND Not Parallel Double
#define	andnpd_r2r(regs, regd)	simd_r2r(andnpd, regs, regd)///<  bit-wise logical AND Not Parallel Double

// - Or
#define	orpd_m2r(var, reg)	simd_m2r(orpd, var, reg)    ///<  bit-wise logical OR Parallel Double
#define	orpd_r2r(regs, regd)	simd_r2r(orpd, regs, regd)  ///<  bit-wise logical OR Parallel Double

// - Exclusive or
#define	xorpd_m2r(var, reg)	simd_m2r(xorpd, var, reg)   ///<  bit-wise logical eXclusive OR Parallel Double
#define	xorpd_r2r(regs, regd)	simd_r2r(xorpd, regs, regd) ///<  bit-wise logical eXclusive OR Parallel Double


// ******************************************************************
// **** Single Precision Floating Point Arithmetic       
//      
// ******************************************************************

// Add
#define	addps_m2r(var, reg)	simd_m2r(addps, var, reg)   ///<  ADD Packed Single precision floating point (4 spfp)      [a0,...,a3] op [b0,...b3] -> [a0+b0,...a3+b3]
#define	addps_r2r(regs, regd)	simd_r2r(addps, regs, regd) ///<  ADD Packed Single precision floating point (4 spfp)      [a0,...,a3] op [b0,...b3] -> [a0+b0,...a3+b3] 

// Subtract
#define	subps_m2r(var, reg)	simd_m2r(subps, var, reg)   ///<  SUBtract Packed Single precision floating point (4 spfp) [a0,...,a3] op [b0,...b3] -> [a0-b0,...a3-b3]
#define	subps_r2r(regs, regd)	simd_r2r(subps, regs, regd) ///<  SUBtract Packed Single precision floating point (4 spfp) [a0,...,a3] op [b0,...b3] -> [a0-b0,...a3-b3]

// Multiply
#define	mulps_m2r(var, reg)	simd_m2r(mulps, var, reg)   ///<  MULtiply Packed Single precision floating point (4 spfp) [a0,...,a3] op [b0,...b3] -> [a0*b0,...a3*b3]
#define	mulps_r2r(regs, regd)	simd_r2r(mulps, regs, regd) ///<  MULtiply Packed Single precision floating point (4 spfp) [a0,...,a3] op [b0,...b3] -> [a0*b0,...a3*b3]

// Divide
#define	divps_m2r(var, reg)	simd_m2r(divps, var, reg)   ///<  DIVide   Packed Single precision floating point (4 spfp) [a0,...,a3] op [b0,...b3] -> [a0/b0,...a3/b3]
#define	divps_r2r(regs, regd)	simd_r2r(divps, regs, regd) ///<  DIVide   Packed Single precision floating point (4 spfp) [a0,...,a3] op [b0,...b3] -> [a0/b0,...a3/b3]



/* To add

ADDSS	Scalar Single-FP Add	arithmetic
DIVSS	Scalar Single-FP Divide	arithmetic
MAXPS	Packed Single-FP Maximum	arithmetic
MAXSS	Scalar Single-FP Maximum	arithmetic
MINPS	Packed Single-FP Minimum	arithmetic
MINSS	Scalar Single-FP Minimum	arithmetic
MULSS	Scalar Single-FP Multiply	arithmetic
RCPPS	Packed Single-FP Reciprocal	arithmetic
RCPSS	Scalar Single-FP Reciprocal	arithmetic
RSQRTPS	Packed Single-FP Square Root Reciprocal	arithmetic
RSQRTSS	Scalar Single-FP Square Root Reciprocal	arithmetic
SQRTPS	Packed Single-FP Square Root	arithmetic
SQRTSS	Scalar Single-FP Square Root	arithmetic
SUBSS	Scalar Single-FP Substract	arithmetic
*/



// ******************************************************************
// **** Conversion instructions (float <--> integer)
// ******************************************************************

// From Packed Single floatin point
#define	cvtps2dq_m2r(var, reg)	  simd_m2r(cvtps2dq, var, reg)    ///< ConVerT Packed Single-floating point to signed Doublewords (int)  [a0,a1,a2,a3]; [b0,b1,b2,b3] -> [(int)a0,...,(int)a3]
#define	cvtps2dq_r2r(regs, regd)  simd_r2r(cvtps2dq, regs, regd)  ///< ConVerT Packed Single-floating point to signed Doublewords (int)  [a0,a1,a2,a3]; [b0,b1,b2,b3] -> [(int)a0,...,(int)a3]

#define	cvttps2dq_m2r(var, reg)	  simd_m2r(cvttps2dq, var, reg)   ///< ConVerT and Truncate Packed Single-floating point to signed Doublewords (int)  [a0,a1,a2,a3]; [b0,b1,b2,b3] -> [(int)a0,...,(int)a3]
#define	cvttps2dq_r2r(regs, regd) simd_r2r(cvttps2dq, regs, regd) ///< ConVerT and Truncate Packed Single-floating point to signed Doublewords (int)  [a0,a1,a2,a3]; [b0,b1,b2,b3] -> [(int)a0,...,(int)a3]

// From Scalar Double floating point
#define	cvtsd2si_m2r(var, reg)	  simd_m2r(cvtsd2si, var, reg)    ///< ConVerT Scalar Double-FP to Signed Integer doubleword (int 32)
#define	cvtsd2si_r2r(regs, regd)  simd_r2r(cvtsd2si, regs, regd)  ///< ConVerT Scalar Double-FP to Signed Integer doubleword (int 32)

/* To add
CVTDQ2PD	Packed Signed INT32 to Packed Double-FP Conversion	conversion
CVTPI2PD	Packed Signed INT32 to Packed Double-FP Conversion	conversion
CVTDQ2PS	Packed Signed INT32 to Packed Single-FP Conversion	conversion
CVTSI2SD	Scalar Signed INT32 to Scalar Double-FP Conversion	conversion

CVTPD2PI	Packed Double-FP to Packed INT32 Conversion	conversion
CVTPD2PQ	Packed Double-FP to Packed INT32 Conversion	conversion
CVTTPD2PI	Packed Double-FP to Packed INT32 Conversion (truncate)	conversion
CVTTPD2DQ	Packed Double-FP to Packed INT32 Conversion (truncate)	conversion
CVTPD2PS	Packed Double-FP to Packed Single-FP Conversion	conversion

CVTSD2SI	Scalar Double-FP to Signed INT32 Conversion	conversion
* CVTTSD2SI	Scalar Double-FP to Signed INT32 Conversion (truncate)	conversion
CVTSD2SS	Scalar Double-FP to Scalar Single-FP Conversion	conversion

CVTPS2PD	Packed Single-FP to Packed Double-FP Conversion	conversion
* CVTPS2DQ	Packed Single-FP to Packed Signed INT32 Conversion	conversion
* CVTTPS2DQ	Packed Single-FP to Packed Signed INT32 Conversion (truncate)	conversion
CVTSS2SD	Scalar Single-FP to Scalar Double-FP Conversion	conversion
*/



// ******************************************************************
// **** Integer (XMMX) instructions 
// ******************************************************************

// **** Logical instructions  (4 Instructions) 

// And 
#define	pand_m2r(var, reg)	simd_m2r(pand, var, reg)      ///<  Bit-Wise logical AND 
#define	pand_r2r(regs, regd)	simd_r2r(pand, regs, regd)    ///<  Bit-Wise logical AND 

// And not
#define	pandn_m2r(var, reg)	simd_m2r(pandn, var, reg)     ///<  Bit-Wise logical AND Not
#define	pandn_r2r(regs, regd)	simd_r2r(pandn, regs, regd)   ///<  Bit-Wise logical AND Not

// Or
#define	por_m2r(var, reg)	simd_m2r(por, var, reg)       ///<  Bit-Wise logical OR 
#define	por_r2r(regs, regd)	simd_r2r(por, regs, regd)     ///<  Bit-Wise logical OR 

// Exclusive or 
#define	pxor_m2r(var, reg)	simd_m2r(pxor, var, reg)      ///<  Bit-Wise logical eXclusive OR 
#define	pxor_r2r(regs, regd)	simd_r2r(pxor, regs, regd)    ///<  Bit-Wise logical eXclusive OR 
 

// **** Arithmetic Instructions

// Add (8)
#define	paddb_m2r(var, reg)	simd_m2r(paddb, var, reg)     ///<  Packed ADD Byte          (16 bytes in parallel)
#define	paddb_r2r(regs, regd)	simd_r2r(paddb, regs, regd)   ///<  Packed ADD Byte          (16 bytes in parallel)

#define	paddsb_m2r(var, reg)	simd_m2r(paddsb, var, reg)    ///<  Packed ADD Byte with Saturation (16 bytes in parallel) saturates to +127, -128
#define	paddsb_r2r(regs, regd)	simd_r2r(paddsb, regs, regd)  ///<  Packed ADD Byte with Saturation (16 bytes in parallel) saturates to +127, -128

#define	paddusb_m2r(var, reg)	simd_m2r(paddusb, var, reg)   ///<  Packed ADD Unsigned Byte with saturation (16 bytes in parallel) saturates to +255
#define	paddusb_r2r(regs, regd)	simd_r2r(paddusb, regs, regd) ///<  Packed ADD Unsigned Byte with saturation (16 bytes in parallel) saturates to +255

#define	paddw_m2r(var, reg)	simd_m2r(paddw, var, reg)     ///<  Packed ADD Word          ( 8 words in parallel)
#define	paddw_r2r(regs, regd)	simd_r2r(paddw, regs, regd)   ///<  Packed ADD Word          ( 8 words in parallel)

#define	paddsw_m2r(var, reg)	simd_m2r(paddsw, var, reg)    ///<  Packed ADD Word with Saturation ( 8 words in parallel) saturates to +32767, -32768
#define	paddsw_r2r(regs, regd)	simd_r2r(paddsw, regs, regd)  ///<  Packed ADD Word with Saturation ( 8 words in parallel) saturates to +32767, -32768

#define	paddusw_m2r(var, reg)	simd_m2r(paddusw, var, reg)   ///<  Packed ADD Unsigned Word with saturation ( 8 words in parallel) saturates to +65535
#define	paddusw_r2r(regs, regd)	simd_r2r(paddusw, regs, regd) ///<  Packed ADD Unsigned Word with saturation ( 8 words in parallel) saturates to +65535

#define	paddd_m2r(var, reg)	simd_m2r(paddd, var, reg)     ///<  Packed ADD Double Word   ( 4 double words in parallel)
#define	paddd_r2r(regs, regd)	simd_r2r(paddd, regs, regd)   ///<  Packed ADD Double Word   ( 4 double words in parallel)

#define	paddq_m2r(var, reg)	simd_m2r(paddq, var, reg)     ///<  Packed ADD Quad Words    ( 2 quadwords in parallel)
#define	paddq_r2r(regs, regd)	simd_r2r(paddq, regs, regd)   ///<  Packed ADD Quad Words    ( 2 quadwords in parallel)

// Subtract (8)
#define	psubb_m2r(var, reg)	simd_m2r(psubb, var, reg)     ///<  Packed SUBtract Byte          (16 bytes in parallel)
#define	psubb_r2r(regs, regd)	simd_r2r(psubb, regs, regd)   ///<  Packed SUBtract Byte          (16 bytes in parallel)

#define	psubsb_m2r(var, reg)	simd_m2r(psubsb, var, reg)    ///<  Packed SUBtract Byte with Saturation (16 bytes in parallel) saturates to +127, -128
#define	psubsb_r2r(regs, regd)	simd_r2r(psubsb, regs, regd)  ///<  Packed SUBtract Byte with Saturation (16 bytes in parallel) saturates to +127, -128

#define	psubusb_m2r(var, reg)	simd_m2r(psubusb, var, reg)   ///<  Packed SUBtract Unsigned Byte with saturation (16 bytes in parallel) saturates to +255
#define	psubusb_r2r(regs, regd)	simd_r2r(psubusb, regs, regd) ///<  Packed SUBtract Unsigned Byte with saturation (16 bytes in parallel) saturates to +255

#define	psubw_m2r(var, reg)	simd_m2r(psubw, var, reg)     ///<  Packed SUBtract Word          ( 8 words in parallel)
#define	psubw_r2r(regs, regd)	simd_r2r(psubw, regs, regd)   ///<  Packed SUBtract Word          ( 8 words in parallel)

#define	psubsw_m2r(var, reg)	simd_m2r(psubsw, var, reg)    ///<  Packed SUBtract Word with Saturation ( 8 words in parallel) saturates to +32767, -32768
#define	psubsw_r2r(regs, regd)	simd_r2r(psubsw, regs, regd)  ///<  Packed SUBtract Word with Saturation ( 8 words in parallel) saturates to +32767, -32768

#define	psubusw_m2r(var, reg)	simd_m2r(psubusw, var, reg)   ///<  Packed SUBtract Unsigned Word with saturation ( 8 words in parallel) saturates to +65535
#define	psubusw_r2r(regs, regd)	simd_r2r(psubusw, regs, regd) ///<  Packed SUBtract Unsigned Word with saturation ( 8 words in parallel) saturates to +65535

#define	psubd_m2r(var, reg)	simd_m2r(psubd, var, reg)     ///<  Packed SUBtract Double Word   ( 4 doublewords in parallel)
#define	psubd_r2r(regs, regd)	simd_r2r(psubd, regs, regd)   ///<  Packed SUBtract Double Word   ( 4 doublewords in parallel)

#define	psubq_m2r(var, reg)	simd_m2r(psubq, var, reg)     ///<  Packed SUBtract Quad Words    ( 2 quadwords in parallel)
#define	psubq_r2r(regs, regd)	simd_r2r(psubq, regs, regd)   ///<  Packed SUBtract Quad Words    ( 2 quadwords in parallel)

// Average (2)
#define	pavgb_m2r(var, reg)	simd_m2r(pavgb, var, reg)     ///<  Packed AVeraGe unsigned Byte (16 bytes in parallel) 
#define	pavgb_r2r(regs, regd)	simd_r2r(pavgb, regs, regd)   ///<  Packed AVeraGe unsigned Byte (16 bytes in parallel) 

#define	pavgw_m2r(var, reg)	simd_m2r(pavgw, var, reg)     ///<  Packed AVeraGe unsigned Word ( 8 words in parallel) 
#define	pavgw_r2r(regs, regd)	simd_r2r(pavgw, regs, regd)   ///<  Packed AVeraGe unsigned Word ( 8 words in parallel) 

// Maximum (2)
#define	pmaxub_m2r(var, reg)	simd_m2r(pmaxub, var, reg)    ///<  Packed MAXimum Unsigned Byte (16 bytes in parallel) 
#define	pmaxub_r2r(regs, regd)	simd_r2r(pmaxub, regs, regd)  ///<  Packed MAXimum Unsigned Byte (16 bytes in parallel) 

#define	pmaxsw_m2r(var, reg)	simd_m2r(pmaxub, var, reg)    ///<  Packed MAXimum Signed Word   ( 8 words in parallel) 
#define	pmaxsw_r2r(regs, regd)	simd_r2r(pmaxub, regs, regd)  ///<  Packed MAXimum Signed Word   ( 8 words in parallel) 

// Minimum (2)
#define	pminub_m2r(var, reg)	simd_m2r(pminub, var, reg)    ///<  Packed MINimum Unsigned Byte (16 bytes in parallel) 
#define	pminub_r2r(regs, regd)	simd_r2r(pminub, regs, regd)  ///<  Packed MINimum Unsigned Byte (16 bytes in parallel) 

#define	pminsw_m2r(var, reg)	simd_m2r(pminub, var, reg)    ///<  Packed MINimum Signed Word   ( 8 words in parallel) 
#define	pminsw_r2r(regs, regd)	simd_r2r(pminub, regs, regd)  ///<  Packed MINimum Signed Word   ( 8 words in parallel) 

// Multiply (5)
#define	pmulhw_m2r(var, reg)	simd_m2r(pmulhw, var, reg)    ///<  Packed MULtiply High Word  ( 8 words in parallel)           [a0,...,a7],[b0,...,b7] -> [(a0*b0)>>16,...,(a7*b7)>>16] 
#define	pmulhw_r2r(regs, regd)	simd_r2r(pmulhw, regs, regd)  ///<  Packed MULtiply High Word  ( 8 words in parallel)           [a0,...,a7],[b0,...,b7] -> [(a1*b0)>>16,...,(a7*b7)>>16] 

#define	pmulhuw_m2r(var, reg)	simd_m2r(pmulhuw, var, reg)   ///<  Packed MULtiply High Unsigned Word ( 8 words in parallel)   [a0,...,a7],[b0,...,b7] -> [(a0*b0)>>16,...,(a7*b7)>>16] 	
#define	pmulhuw_r2r(regs, regd)	simd_r2r(pmulhuw, regs, regd) ///<  Packed MULtiply High Unsigned Word ( 8 words in parallel)   [a0,...,a7],[b0,...,b7] -> [(a0*b0)>>16,...,(a7*b7)>>16] 	

#define	pmullw_m2r(var, reg)	simd_m2r(pmullw, var, reg)    ///<  Packed MULtiply Low Word   ( 8 words in parallel)           [a0,...,a7],[b0,...,b7] -> [a0*b0,...,a7*b7] 
#define	pmullw_r2r(regs, regd)	simd_r2r(pmullw, regs, regd)  ///<  Packed MULtiply Low Word   ( 8 words in parallel)           [a0,...,a7],[b0,...,b7] -> [a0*b0,...,a7*b7] 

#define	pmuludq_m2r(var, reg)	simd_m2r(pmuludq, var, reg)   ///<  Packed MULtiply Unsigned Doubleword to Quadword ( 4 doublewords in parallel) [a0,...,a3],[b0,...,b3] -> [a0*b0,a2*b2] 
#define	pmuludq_r2r(regs, regd)	simd_r2r(pmuludq, regs, regd) ///<  Packed MULtiply Unsigned Doubleword to Quadword ( 4 doublewords in parallel) [a0,...,a3],[b0,...,b3] -> [a0*b0,a2*b2] 

// Pack   (8)
#define	packssdw_m2r(var, reg)	 simd_m2r(packssdw, var, reg)
#define	packssdw_r2r(regs, regd) simd_r2r(packssdw, regs, regd)

#define	packsswb_m2r(var, reg)	 simd_m2r(packsswb, var, reg)
#define	packsswb_r2r(regs, regd) simd_r2r(packsswb, regs, regd) ///< Pack two registers together on a WORD-to-Byte basis with saturation

#define	packuswb_m2r(var, reg)	 simd_m2r(packuswb, var, reg)
#define	packuswb_r2r(regs, regd) simd_r2r(packuswb, regs, regd)



// Unpack (8)
#define	punpckhbw_m2r(var, reg)	simd_m2r(punpckhbw, var, reg)     ///<  Packed data UNPaCK High Byte to Word [a0, ... , a15] op [b0, ... b15] -> [a8, b8, ..., a15, b15]
#define	punpckhbw_r2r(regs, regd) simd_r2r(punpckhbw, regs, regd) ///<  Packed data UNPaCK High Byte to Word [a0, ... , a15] op [b0, ... b15] -> [a8, b8, ..., a15, b15]

#define	punpcklbw_m2r(var, reg)	simd_m2r(punpcklbw, var, reg)     ///<  Packed data UNPaCK Low Byte to Word  [a0, ... , a15] op [b0, ... b15] -> [a0, b0, ..., a7, b7]
#define	punpcklbw_r2r(regs, regd) simd_r2r(punpcklbw, regs, regd) ///<  Packed data UNPaCK Low Byte to Word  [a0, ... , a15] op [b0, ... b15] -> [a0, b0, ..., a7, b7]

#define	punpckhwd_m2r(var, reg)	simd_m2r(punpckhwd, var, reg)     ///<  Packed data UNPaCK High Word to Doubleword [a0, ... , a7] op [b0, ... b7] -> [a4, b4, ..., a7, b7]
#define	punpckhwd_r2r(regs, regd) simd_r2r(punpckhwd, regs, regd) ///<  Packed data UNPaCK High Word to Doubleword [a0, ... , a7] op [b0, ... b7] -> [a4, b4, ..., a7, b7]

#define	punpcklwd_m2r(var, reg)	simd_m2r(punpcklwd, var, reg)     ///<  Packed data UNPaCK Low  Word to Doubleword [a0, ... , a7] op [b0, ... b7] -> [a0, b0, ..., a3, b3]
#define	punpcklwd_r2r(regs, regd) simd_r2r(punpcklwd, regs, regd) ///<  Packed data UNPaCK Low  Word to Doubleword [a0, ... , a7] op [b0, ... b7] -> [a0, b0, ..., a3, b3]

#define	punpckhdq_m2r(var, reg)	simd_m2r(punpckhdq, var, reg)     ///<  Packed data UNPaCK High Doubleword to Quadword [a0, ... , a3] op [b0, ... b3] -> [a2, b2, a3, b3]
#define	punpckhdq_r2r(regs, regd) simd_r2r(punpckhdq, regs, regd) ///<  Packed data UNPaCK High Doubleword to Quadword [a0, ... , a3] op [b0, ... b3] -> [a2, b2, a3, b3]

#define	punpckldq_m2r(var, reg)	simd_m2r(punpckldq, var, reg)///<  Packed data UNPaCK Low  Doubleword to Quadword [a0, ... , a3] op [b0, ... b3] -> [a0, b0, a1, b1]
#define	punpckldq_r2r(regs, regd) simd_r2r(punpckldq, regs, regd)   ///<  Packed data UNPaCK Low  Doubleword to Quadword [a0, ... , a3] op [b0, ... b3] -> [a0, b0, a1, b1]

#define	punpckhqdq_m2r(var, reg) simd_m2r(punpckhqdq, var, reg)     ///<  Packed data UNPaCK High Quadword to DoubleQuadword [a0, a1] op [b0, b1] -> [a1, b1]
#define	punpckhqdq_r2r(regs, regd) simd_r2r(punpckhqdq, regs, regd) ///<  Packed data UNPaCK High Quadword to DoubleQuadword [a0, a1] op [b0, b1] -> [a1, b1]

#define	punpcklqdq_m2r(var, reg) simd_m2r(punpcklqdq, var, reg)     ///<  Packed data UNPaCK Low  Quadword to DoubleQuadword [a0, a1] op [b0, b1] -> [a0, b0] 
#define	punpcklqdq_r2r(regs, regd) simd_r2r(punpcklqdq, regs, regd) ///<  Packed data UNPaCK Low  Quadword to DoubleQuadword [a0, a1] op [b0, b1] -> [a0, b0] 

// Shuffle (for xmmX 128 bit registers)
#define	pshufd_im2r(imm, var, reg)   simd_im2r(pshufd, imm, var, reg)      ///<  Packed SHUFfle Doublewords
#define	pshufd_ir2r(imm, regs, regd) simd_ir2r(pshufd, imm, regs, regd)    ///<  Packed SHUFfle Doublewords

#define	pshufhw_im2r(imm, var, reg)   simd_im2r(pshufhw, imm, var, reg)    ///<  Packed SHUFfle High Words
#define	pshufhw_ir2r(imm, regs, regd) simd_ir2r(pshufhw, imm, regs, regd)  ///<  Packed SHUFfle High Words

#define	pshuflw_im2r(imm, var, reg)   simd_im2r(pshuflw, imm, var, reg)    ///<  Packed SHUFfle Low Words
#define	pshuflw_ir2r(imm, regs, regd) simd_ir2r(pshuflw, imm, regs, regd)  ///<  Packed SHUFfle Low Words

// Shuffle words (for mmX 64 bit registers)
#define	pshufw_im2r(imm, var, reg)    simd_im2r(pshufw, imm, var, reg)      ///<  Packed SHUFfle Low Words
#define	pshufw_ir2r(imm, regs, regd)  simd_ir2r(pshufw, imm, regs, regd)    ///<  Packed SHUFfle Low Words
                            


// Shift (10)
#define	psraw_i2r(imm, reg)	simd_i2r(psraw, imm, reg)    ///<  Packed Shift Right Arithmetic Word
#define	psrlw_i2r(imm, reg)	simd_i2r(psrlw, imm, reg)    ///<  Packed Shift Right Logical Word
#define	psllw_i2r(imm, reg)	simd_i2r(psllw, imm, reg)    ///<  Packed Shift Left  Logical Word

#define	psrad_i2r(imm, reg)	simd_i2r(psrad, imm, reg)    ///<  Packed Shift Right Arithmetic Word
#define	psrld_i2r(imm, reg)	simd_i2r(psrld, imm, reg)    ///<  Packed Shift Right Logical Double word
#define	pslld_i2r(imm, reg)	simd_i2r(pslld, imm, reg)    ///<  Packed Shift Left  Logical Double word

#define	psrlq_i2r(imm, reg)	simd_i2r(psrlq, imm, reg)    ///<  Packed Shift Right Logical Quadword
#define	psllq_i2r(imm, reg)	simd_i2r(psllq, imm, reg)    ///<  Packed Shift Left  Logical Quadword

#define	psrldq_i2r(imm, reg)	simd_i2r(psrldq, imm, reg)   ///<  Packed Shift Right Logical Double Quadword 
#define	pslldq_i2r(imm, reg)	simd_i2r(pslldq, imm, reg)   ///<  Packed Shift Left  Logical Double Quadword 

// Compare (6)

/*
PCMPEQB	Packed Bytes Compare For Equal	xmm integer
PCMPEQW	Packed Words Compare For Equal	xmm integer
PCMPEQD	Packed Doublewords Compare For Equal	xmm integer
PCMPGTB	Packed Bytes Compare For Greater Than	xmm integer
PCMPGTW	Packed Words Compare For Greater Than	xmm integer
PCMPGTD	Packed Doublewords Compare For Greater Than	xmm integer
*/

// Other
#define	pmaddwd_m2r(var, reg)	simd_m2r(pmaddwd, var, reg)    ///<  Packed Multiply and ADD Word to Doubleword ( 8 words in parallel) [a0,...,a7],[b0,...,b7] -> [a0*b0+a1*b1,..., a6*b6+a7*b7] 
#define	pmaddwd_r2r(regs, regd)	simd_r2r(pmaddwd, regs, regd)  ///<  Packed Multiply and ADD Word to Doubleword ( 8 words in parallel) [a0,...,a7],[b0,...,b7] -> [a0*b0+a1*b1,..., a6*b6+a7*b7] 

#define	psadbw_m2r(var, reg)	simd_m2r(psadbw, var, reg)     ///<  Packed Sum Absolute Differences Byte to Word ( 16 bytes in parallel) [a0,...,a15],[b0,...,b15] -> [sum_{i=0..7}(abs(ai-bi)),0,0,0,sum_{i=8..15}(abs(ai-bi)),0, 0, 0]
#define	psadbw_r2r(regs, regd)	simd_r2r(psadbw, regs, regd)   ///<  Packed Sum Absolute Differences Byte to Word ( 16 bytes in parallel) [a0,...,a15],[b0,...,b15] -> [sum_{i=0..7}(abs(ai-bi)),0,0,0,sum_{i=8..15}(abs(ai-bi)),0, 0, 0]

/*
PEXTRW	Extract Word	xmm integer
PINSRW	Insert Word	xmm integer
PMOVMSKB	Move Byte Mask To Integer	xmm integer
*/


// ****** SSE3 instruction set ******************************************************************
// ADDSUBPD
#define	addsubpd_m2r(var, reg)	 simd_m2r(addsubpd, var, reg)   ///<  
#define	addsubpd_r2r(regs, regd) simd_r2r(addsubpd, regs, regd) ///<  

// ****** MN added function... ******************************************************************
// PACKSSWB
//#define	packsswb_r2r(regs, regd) simd_r2r(packsswb, regs, regd) ///< Pack two registers together on a WORD-to-Byte basis with saturation
// PSHUFB
//#define pshufb_r2r(regs, regd)   simd_r2r(pshufb,   regs, regd) ///< More recent alternative to PACKSSWB (????)     // NOT SUPPORTED BY P4
//#define pshufb_r2m(reg,  var)    simd_r2r(pshufb,   reg,  var)  ///< More recent alternative to PACKSSWB (????)     // NOT SUPPORTED BY P4

#endif   

// *********************  TEST INSTRUCTIONS ************************************************************
// insert word to xmm register
#define	pinsrw(mem16, reg, imm)	simd_im2r(pinsrw, imm, mem16, reg)
// ********************************************************************************


// ****************************** MMX and SSE instruction set working on mm0, ... mm7 64-bit registers *******************************************
#define	emms()			__asm__ __volatile__ ("emms")      ///< Empty MMx State
