; @brief Apply Q14 integer FFT butterflies, two complex products in parallel.
; @details Inputs are disjoint interleaved int16 arrays with valid FFT headroom.
; Products use arithmetic /2^14, then sum/difference use /2 and signed saturation.
; No allocation or global state; leaf function, concurrent with distinct buffers.
; @param upper Read/write first half, 2*count int16 elements.
; @param lower Read/write second half, 2*count int16 elements.
; @param twiddle Read-only Q14 table, 4*count int16 coefficients.
; @param count Positive number of complex butterflies; never pass zero.
; ABI: Microsoft x64: rcx/rdx/r8/r9. Only volatile GPRs/XMM0..5; no stack.
PUBLIC NgFftButterflies
.code
NgFftButterflies PROC
    mov r10, rcx
    mov r11, rdx
    mov rax, r8
loop_pairs:
    cmp r9, 1
    je single_pair
    movq xmm0, QWORD PTR [r11]
    pshufd xmm0, xmm0, 050h
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
    jne loop_pairs
    ret
single_pair:
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
NgFftButterflies ENDP
END
