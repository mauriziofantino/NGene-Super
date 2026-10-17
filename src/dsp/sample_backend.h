/** @file sample_backend.h @brief Private compiler-target selection for sample primitives. */
#ifndef NGENE_DSP_SAMPLE_BACKEND_H
#define NGENE_DSP_SAMPLE_BACKEND_H
#if !defined(NGENE_SAMPLE_REFERENCE) && (defined(__x86_64__) || defined(_M_X64))
#include <emmintrin.h>
#define NG_SSE2 1
#elif !defined(NGENE_SAMPLE_REFERENCE) && (defined(__aarch64__) || defined(_M_ARM64))
#if defined(_MSC_VER) && !defined(__clang__)
#include <arm64_neon.h>
#else
#include <arm_neon.h>
#endif
#define NG_NEON 1
#endif
#endif
