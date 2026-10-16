; @brief Apply Q14 integer FFT butterflies, four complex products per main iteration.
; @details Inputs are disjoint interleaved int16 arrays with valid FFT headroom.
; Products use arithmetic /2^14, then sum/difference use /2 and signed saturation.
; No allocation or global state; leaf function, concurrent with distinct buffers.
; @param upper Read/write first half, 2*count int16 elements.
; @param lower Read/write second half, 2*count int16 elements.
; @param twiddle Read-only Q14 table, 4*count int16 coefficients.
; @param count Positive number of complex butterflies; never pass zero.
; ABI: Microsoft x64: rcx/rdx/r8/r9. Only volatile GPRs/XMM0..5; no stack.
PUBLIC NgFftButterflies
PUBLIC NgFftFirstStages
PUBLIC NgFftInputInRange
.code
; @brief Check all input components against the inclusive Q14 headroom limits.
; @param rcx Read-only interleaved int16 input, with natural int16 alignment.
; @param rdx Complex sample count, divisible by four and at least eight.
; @return eax Nonzero for valid input; zero if any component exceeds headroom.
NgFftInputInRange PROC
    mov r10, rcx
    mov r9, rdx
    pcmpeqw xmm2, xmm2
    psllw xmm2, 14
    pxor xmm3, xmm3
    psubw xmm3, xmm2
range_loop:
    movdqu xmm0, XMMWORD PTR [r10]
    movdqa xmm1, xmm2
    pcmpgtw xmm1, xmm0
    pcmpgtw xmm0, xmm3
    por xmm0, xmm1
    pmovmskb eax, xmm0
    test eax, eax
    jne range_invalid
    add r10, 16
    sub r9, 4
    jne range_loop
    mov eax, 1
    ret
range_invalid:
    xor eax, eax
    ret
NgFftInputInRange ENDP

; @brief Fuse stages of width two and four, with signed32 sums and /2 at each stage.
; @param rcx Unaligned interleaved int16 workspace, already bit reversed.
; @param rdx Number of complex samples, divisible by four and at least eight.
; Leaf Win64 ABI: only r9/r10 and XMM0..3 are modified.
NgFftFirstStages PROC
    mov r10, rcx
    mov r9, rdx
    shr r9, 2
first_stages_loop:
    movdqu xmm0, XMMWORD PTR [r10]
    movdqa xmm1, xmm0
    psraw xmm1, 15
    movdqa xmm2, xmm0
    punpckhwd xmm2, xmm1
    punpcklwd xmm0, xmm1
    pshufd xmm1, xmm0, 04eh
    movdqa xmm3, xmm0
    paddd xmm3, xmm1
    psubd xmm0, xmm1
    psrad xmm3, 1
    psrad xmm0, 1
    punpcklqdq xmm3, xmm0
    pshufd xmm1, xmm2, 04eh
    movdqa xmm0, xmm2
    paddd xmm0, xmm1
    psubd xmm2, xmm1
    psrad xmm0, 1
    psrad xmm2, 1
    punpcklqdq xmm0, xmm2
    pshufd xmm0, xmm0, 0b4h
    pcmpeqd xmm2, xmm2
    pslldq xmm2, 12
    pxor xmm0, xmm2
    psubd xmm0, xmm2
    movdqa xmm1, xmm3
    paddd xmm1, xmm0
    psubd xmm3, xmm0
    psrad xmm1, 1
    psrad xmm3, 1
    packssdw xmm1, xmm3
    movdqu XMMWORD PTR [r10], xmm1
    add r10, 16
    dec r9
    jne first_stages_loop
    ret
NgFftFirstStages ENDP

NgFftButterflies PROC
    mov r10, rcx
    mov r11, rdx
    mov rax, r8
    cmp r9, 4
    jb loop_pairs
loop_four:
    movdqu xmm0, XMMWORD PTR [r11]
    pshufd xmm1, xmm0, 0fah
    pshufd xmm0, xmm0, 050h
    movdqu xmm5, XMMWORD PTR [rax]
    pmaddwd xmm0, xmm5
    movdqu xmm5, XMMWORD PTR [rax + 16]
    pmaddwd xmm1, xmm5
    psrad xmm0, 14
    psrad xmm1, 14
    ; Q14 products have magnitude <23230 under the FFT input contract: packing is exact.
    ; S=(U&T)+((U^T)>>1), D=S-T give floor((U+/-T)/2) without pre-scaling overflow.
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
    je finished
    cmp r9, 4
    jae loop_four
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
finished:
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
