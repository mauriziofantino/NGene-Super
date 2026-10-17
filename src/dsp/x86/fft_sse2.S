/**
 * @brief Apply Q14 integer FFT butterflies, four complex products per main iteration.
 * @details Inputs are disjoint interleaved int16 arrays with valid FFT headroom.
 * Products use arithmetic /2^14, then sum/difference use /2 and signed saturation.
 * No allocation or global state; leaf function, concurrent with distinct buffers.
 * @param upper Read/write first half, 2*count int16 elements.
 * @param lower Read/write second half, 2*count int16 elements.
 * @param twiddle Read-only Q14 table, 4*count int16 coefficients.
 * @param count Positive number of complex butterflies; never pass zero.
 * ABI: System V: rdi/rsi/rdx/rcx; Win64: rcx/rdx/r8/r9. Only volatile GPRs/XMM0..5.
 */
#ifdef __APPLE__
#define FN _NgFftButterflies
#define FIRST _NgFftFirstStages
#define RANGE _NgFftInputInRange
#else
#define FN NgFftButterflies
#define FIRST NgFftFirstStages
#define RANGE NgFftInputInRange
#endif
.text
.intel_syntax noprefix
.globl FN
.globl FIRST
.globl RANGE
#ifndef __APPLE__
#ifndef _WIN32
.type FN,@function
.type FIRST,@function
.type RANGE,@function
#endif
#endif
/* Check inclusive Q14 headroom limits, eight unaligned components at a time.
 * ABI: data/size in rdi/rsi (System V), rcx/rdx (Win64); returns eax=0/1. */
RANGE:
#ifdef _WIN32
    mov r10, rcx
    mov r9, rdx
#else
    mov r10, rdi
    mov r9, rsi
#endif
    pcmpeqw xmm2, xmm2
    psllw xmm2, 14
    pxor xmm3, xmm3
    psubw xmm3, xmm2
.Lrange:
    movdqu xmm0, XMMWORD PTR [r10]
    movdqa xmm1, xmm2
    pcmpgtw xmm1, xmm0
    pcmpgtw xmm0, xmm3
    por xmm0, xmm1
    pmovmskb eax, xmm0
    test eax, eax
    jne .Linvalid
    add r10, 16
    sub r9, 4
    jne .Lrange
    mov eax, 1
    ret
.Linvalid:
    xor eax, eax
    ret
#if !defined(__APPLE__) && !defined(_WIN32)
.size RANGE,.-RANGE
#endif

/* In-register width-two/width-four codelet: input/output xmm0, scratch xmm1/xmm2.
 * xmm4 has word mask (0,0,0,-1,0,0,0,-1). Components stay in [-16384,16384]. */
.macro FFT_FIRST_TWO
    /* A=(a,c,a,c), B=(b,d,b,d). S=(A&B)+((A^B)>>1), D=S-B.
     * Both identities retain floor rounding, including odd negative components. */
    pshufd xmm1, xmm0, 0xdd
    pshufd xmm0, xmm0, 0x88
    movdqa xmm2, xmm0
    pxor xmm2, xmm1
    psraw xmm2, 1
    pand xmm0, xmm1
    paddw xmm0, xmm2
    movdqa xmm2, xmm0
    psubw xmm2, xmm1
    punpckldq xmm0, xmm2
    /* The low qword now contains the first pair; rotate the second pair. */
    pshufd xmm1, xmm0, 0xee
    pshuflw xmm1, xmm1, 0xb4
    pxor xmm1, xmm4
    psubw xmm1, xmm4
    movdqa xmm2, xmm0
    pxor xmm2, xmm1
    psraw xmm2, 1
    pand xmm0, xmm1
    paddw xmm0, xmm2
    movdqa xmm2, xmm0
    psubw xmm2, xmm1
    punpcklqdq xmm0, xmm2
.endm

/* Exact plan-generated Q14 coefficients for exp(-j*pi*k/4), k=0..3. */
.p2align 4
.Leighth_twiddle_low:
    .short 16384,0,0,16384,11585,11585,-11585,11585
.Leighth_twiddle_high:
    .short 0,16384,-16384,0,-11585,11585,-11585,-11585

/* Fuse the first three radix-two stages as an eight-sample register codelet.
 * data/size: rdi/rsi (System V), rcx/rdx (Win64); size divisible by eight, >=8.
 * Every Q14 product and per-stage floor average agrees with the scalar stages.
 * Only volatile registers; constants are read-only. */
FIRST:
#ifdef _WIN32
    mov r10, rcx
    mov r9, rdx
#else
    mov r10, rdi
    mov r9, rsi
#endif
    shr r9, 3
    pcmpeqw xmm4, xmm4
    psllq xmm4, 48
.Lfirst_stages:
    movdqu xmm0, XMMWORD PTR [r10]
    FFT_FIRST_TWO
    movdqa xmm3, xmm0
    movdqu xmm0, XMMWORD PTR [r10 + 16]
    FFT_FIRST_TWO
    /* Width eight: retain the Q14 floor before the final /2, including pi/4 rotations. */
    pshufd xmm1, xmm0, 0xfa
    pshufd xmm0, xmm0, 0x50
    pmaddwd xmm0, XMMWORD PTR [rip + .Leighth_twiddle_low]
    pmaddwd xmm1, XMMWORD PTR [rip + .Leighth_twiddle_high]
    psrad xmm0, 14
    psrad xmm1, 14
    packssdw xmm0, xmm1
    movdqa xmm2, xmm3
    pxor xmm2, xmm0
    psraw xmm2, 1
    pand xmm3, xmm0
    paddw xmm3, xmm2
    movdqa xmm2, xmm3
    psubw xmm2, xmm0
    movdqu XMMWORD PTR [r10], xmm3
    movdqu XMMWORD PTR [r10 + 16], xmm2
    add r10, 32
    dec r9
    jne .Lfirst_stages
    ret
#if !defined(__APPLE__) && !defined(_WIN32)
.size FIRST,.-FIRST
#endif

FN:
#ifdef _WIN32
    mov r10, rcx
    mov r11, rdx
    mov rax, r8
#else
    mov r10, rdi
    mov r11, rsi
    mov rax, rdx
    mov r9, rcx
#endif
    cmp r9, 4
    jb .Lloop
.Lfour:
    movdqu xmm0, XMMWORD PTR [r11]
    pshufd xmm1, xmm0, 0xfa
    pshufd xmm0, xmm0, 0x50
    movdqu xmm5, XMMWORD PTR [rax]
    pmaddwd xmm0, xmm5
    movdqu xmm5, XMMWORD PTR [rax + 16]
    pmaddwd xmm1, xmm5
    psrad xmm0, 14
    psrad xmm1, 14
    /* Q14 products have magnitude <23230 under the FFT input contract: packing is exact.
     * S=(U&T)+((U^T)>>1), D=S-T give floor((U+/-T)/2) without pre-scaling overflow. */
    packssdw xmm0, xmm1
    movdqu xmm1, XMMWORD PTR [r10]
    movdqa xmm2, xmm1
    pxor xmm2, xmm0
    psraw xmm2, 1
    pand xmm1, xmm0
    paddw xmm1, xmm2
    movdqa xmm2, xmm1
    psubw xmm2, xmm0
    movdqu XMMWORD PTR [r10], xmm1
    movdqu XMMWORD PTR [r11], xmm2
    add r10, 16
    add r11, 16
    add rax, 32
    sub r9, 4
    je .Ldone
    cmp r9, 4
    jae .Lfour
.Lloop:
    cmp r9, 1
    je .Lsingle
    movq xmm0, QWORD PTR [r11]
    pshufd xmm0, xmm0, 0x50
    movdqu xmm1, XMMWORD PTR [rax]
    pmaddwd xmm0, xmm1
    psrad xmm0, 14
    movq xmm1, QWORD PTR [r10]
    movdqa xmm2, xmm1
    psraw xmm2, 15
    punpcklwd xmm1, xmm2
    movdqa xmm2, xmm1
    paddd xmm1, xmm0
    psubd xmm2, xmm0
    psrad xmm1, 1
    psrad xmm2, 1
    packssdw xmm1, xmm1
    packssdw xmm2, xmm2
    movq QWORD PTR [r10], xmm1
    movq QWORD PTR [r11], xmm2
    add r10, 8
    add r11, 8
    add rax, 16
    sub r9, 2
    jne .Lloop
.Ldone:
    ret
.Lsingle:
    movd xmm0, DWORD PTR [r11]
    pshufd xmm0, xmm0, 0
    movq xmm1, QWORD PTR [rax]
    pmaddwd xmm0, xmm1
    psrad xmm0, 14
    movd xmm1, DWORD PTR [r10]
    movdqa xmm2, xmm1
    psraw xmm2, 15
    punpcklwd xmm1, xmm2
    movdqa xmm2, xmm1
    paddd xmm1, xmm0
    psubd xmm2, xmm0
    psrad xmm1, 1
    psrad xmm2, 1
    packssdw xmm1, xmm1
    packssdw xmm2, xmm2
    movd DWORD PTR [r10], xmm1
    movd DWORD PTR [r11], xmm2
    ret
#if !defined(__APPLE__) && !defined(_WIN32)
.size FN,.-FN
.section .note.GNU-stack,"",@progbits
#endif
