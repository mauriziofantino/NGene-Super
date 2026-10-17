# Select a kernel from a production or frozen source tree using the compiler target.
# Callers enable the returned assembler language at directory scope, add the relative
# kernel when nonempty, and keep the ISA flags local to their respective source files.
include_guard(GLOBAL)

# ngene_select_fft_backend(source_root prefix [REFERENCE])
# Outputs in the caller's scope:
#   <prefix>_relative            Kernel path relative to source_root, empty for scalar.
#   <prefix>_definitions         Private compile definitions for fft.c and the kernel.
#   <prefix>_isa_options         Kernel-only compiler options.
#   <prefix>_scalar_isa_options  Dispatcher/oracle-only compiler options.
#   <prefix>_assembler           Empty, ASM, ASM_MASM or ASM_MARMASM.
#   <prefix>_requires_neon       Whether the intrinsic kernel requires a NEON check.
# Missing newer kernels in a frozen source tree select its historical scalar/ARM64
# backend. The production caller can reject that fallback when SIMD is required.
function(ngene_select_fft_backend source_root prefix)
    cmake_parse_arguments(FFT "REFERENCE" "" "" ${ARGN})
    if(FFT_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown FFT backend arguments: ${FFT_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT NGENE_ARCH MATCHES "^(x86|x86_64|arm|arm64)$")
        message(FATAL_ERROR "Select NGENE_ARCH from the compiler target before selecting an FFT backend")
    endif()
    set(kernel "")
    set(definitions NGENE_FFT_BACKEND="C-reference")
    set(isa_options "")
    set(scalar_isa_options "")
    set(assembler "")
    set(requires_neon FALSE)

    if(NGENE_ARCH STREQUAL "x86")
        # The dispatcher and reference must remain safe before the SSE2 CPU probe.
        if(MSVC)
            set(scalar_isa_options /arch:IA32)
        else()
            set(scalar_isa_options -mno-sse2)
        endif()
    endif()

    if(NOT FFT_REFERENCE)
        if(NGENE_ARCH STREQUAL "x86_64")
            # MSVC and clang-cl use the Windows MASM tool; GNU-driver Clang uses GAS.
            if(MSVC)
                set(kernel "src/dsp/x86/fft_sse2.asm")
                set(assembler ASM_MASM)
            else()
                set(kernel "src/dsp/x86/fft_sse2.S")
                set(assembler ASM)
            endif()
            set(definitions NGENE_FFT_ASSEMBLY=1 NGENE_FFT_BACKEND="SSE2-assembly")
        elseif(NGENE_ARCH STREQUAL "x86" AND EXISTS "${source_root}/src/dsp/x86/fft_sse2_32.c")
            set(kernel "src/dsp/x86/fft_sse2_32.c")
            set(definitions NGENE_FFT_SIMD=1 NGENE_FFT_RUNTIME_SSE2=1 NGENE_FFT_BACKEND="SSE2-intrinsics")
            if(MSVC)
                set(isa_options /arch:SSE2)
            else()
                set(isa_options -msse2)
            endif()
        elseif(NGENE_ARCH MATCHES "^arm" AND EXISTS "${source_root}/src/dsp/arm/fft_neon_intrinsics.c")
            set(kernel "src/dsp/arm/fft_neon_intrinsics.c")
            set(definitions NGENE_FFT_SIMD=1 NGENE_FFT_BACKEND="NEON-intrinsics")
            set(requires_neon TRUE)
            if(NGENE_ARCH STREQUAL "arm" AND NOT MSVC)
                set(isa_options -mfpu=neon)
            endif()
        elseif(NGENE_ARCH STREQUAL "arm64")
            if(MSVC)
                set(kernel "src/dsp/arm/fft_neon.asm")
                set(assembler ASM_MARMASM)
            else()
                set(kernel "src/dsp/arm/fft_neon.S")
                set(assembler ASM)
            endif()
            set(definitions NGENE_FFT_ASSEMBLY=1 NGENE_FFT_BACKEND="NEON-assembly")
        endif()
    endif()

    set(${prefix}_relative "${kernel}" PARENT_SCOPE)
    set(${prefix}_definitions "${definitions}" PARENT_SCOPE)
    set(${prefix}_isa_options "${isa_options}" PARENT_SCOPE)
    set(${prefix}_scalar_isa_options "${scalar_isa_options}" PARENT_SCOPE)
    set(${prefix}_assembler "${assembler}" PARENT_SCOPE)
    set(${prefix}_requires_neon "${requires_neon}" PARENT_SCOPE)
endfunction()
