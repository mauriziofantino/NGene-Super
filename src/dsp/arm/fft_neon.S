/**
 * @brief Apply Q14 integer FFT butterflies, two complex products in parallel.
 * @details Inputs are disjoint interleaved int16 arrays with valid FFT headroom.
 * Products use arithmetic /2^14, then sum/difference use /2 and signed saturation.
 * No allocation or global state; leaf function, concurrent with distinct buffers.
 * @param upper Read/write first half, 2*count int16 elements.
 * @param lower Read/write second half, 2*count int16 elements.
 * @param twiddle Read-only Q14 table, 4*count int16 coefficients.
 * @param count Positive number of complex butterflies; never pass zero.
 * ABI: AAPCS64: x0/x1/x2/x3. Only volatile x0..x3/v0..v7; no stack.
 */
#ifdef __APPLE__
#define FN _NgFftButterflies
#else
#define FN NgFftButterflies
#endif
.text
.p2align 2
.globl FN
FN:
1:
    cmp x3, #1
    b.eq 2f
    ldr d0, [x1]
    zip1 v1.4s, v0.4s, v0.4s
    ldr q2, [x2]
    smull v3.4s, v1.4h, v2.4h
    smull2 v4.4s, v1.8h, v2.8h
    addp v3.4s, v3.4s, v4.4s
    sshr v3.4s, v3.4s, #14
    ldr d0, [x0]
    sxtl v0.4s, v0.4h
    add v1.4s, v0.4s, v3.4s
    sub v2.4s, v0.4s, v3.4s
    sshr v1.4s, v1.4s, #1
    sshr v2.4s, v2.4s, #1
    sqxtn v1.4h, v1.4s
    sqxtn v2.4h, v2.4s
    str d1, [x0], #8
    str d2, [x1], #8
    add x2, x2, #16
    subs x3, x3, #2
    b.ne 1b
    ret
2:
    ldr s0, [x1]
    dup v1.2s, v0.s[0]
    ldr d2, [x2]
    smull v3.4s, v1.4h, v2.4h
    addp v3.4s, v3.4s, v3.4s
    sshr v3.4s, v3.4s, #14
    ldr s0, [x0]
    sxtl v0.4s, v0.4h
    add v1.4s, v0.4s, v3.4s
    sub v2.4s, v0.4s, v3.4s
    sshr v1.4s, v1.4s, #1
    sshr v2.4s, v2.4s, #1
    sqxtn v1.4h, v1.4s
    sqxtn v2.4h, v2.4s
    str s1, [x0]
    str s2, [x1]
    ret
#if !defined(__APPLE__) && !defined(_WIN32)
.section .note.GNU-stack,"",@progbits
#endif
