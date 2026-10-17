/** @file legacy.c @brief Host-only adapters around the untouched original NGene FFT source.
 * This file is benchmark tooling; it is never linked into the receiver.
 */
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <malloc.h>
#endif

/* Skip receiver services: PerformFft uses the configuration only for fatal diagnostics. */
#define NGENE_CONSTANTS_H_INCLUDED
#define NGENE_CONFIGURATION_H_INCLUDED
#define IO_FUNCTIONS_H_INCLUDED
#define CLOSE_MANAGER_H_INCLUDED
#if UINTPTR_MAX > UINT32_MAX
#define COMPILE_64_BIT
#endif
#define POSIX_ALIGNMENT 16
#define SUCCESS_RETURN 0
#define FAILURE_RETURN (-1)
#define NGENE_LOG_SEVERITY_ERROR 2
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
/** @brief Unused opaque receiver configuration required by the original API. */
typedef struct {
    /** No legacy receiver service is called by a valid transform. */
    int unused;
} TNGeneConfiguration;

/** @brief Preserve fatal diagnostics without linking the original receiver logger. */
static void SwrxLog(int severity, const char *message, ...)
{
    va_list args;
    (void)severity;
    va_start(args, message);
    vfprintf(stderr, message, args);
    va_end(args);
    fputc('\n', stderr);
}

/** @brief Stop the benchmark if the original source requests receiver shutdown. */
static void CloseNGene(TNGeneConfiguration *configuration, int status)
{
    (void)configuration;
    exit(status);
}

/* Keep the original plan/allocation code, with a matching aligned allocator on Windows. */
/** @brief Allocate legacy plan storage with its required 16-byte alignment. */
static void *LegacyAllocate(size_t bytes)
{
#ifdef _WIN32
    return _aligned_malloc(bytes, 16);
#else
    void *p = NULL;
    return posix_memalign(&p, 16, bytes) == 0 ? p : NULL;
#endif
}

/** @brief Free legacy storage with the matching host allocator. */
static void LegacyFree(void *p)
{
#ifdef _WIN32
    _aligned_free(p);
#else
    free(p);
#endif
}

/** @brief Supply the original posix_memalign contract for the required alignment. */
static int LegacyAlignedAllocate(void **out, size_t alignment, size_t bytes)
{
    if (alignment != 16) {
        return -1;
    }
    *out = LegacyAllocate(bytes);
    return *out != NULL ? 0 : -1;
}

#define malloc LegacyAllocate
#define free LegacyFree
#define posix_memalign LegacyAlignedAllocate
#include NGENE_LEGACY_FFT_SOURCE
#undef malloc
#undef free
#undef posix_memalign

/** Single benchmark thread owns the original table. */
static TFftTable table;
/** Stub context remains valid through all original API calls. */
static TNGeneConfiguration configuration;
/** Original global XMM backup: this baseline is deliberately single-threaded. */
char *ngeneXmmsBackup;

/** @brief Create the untouched original plan and its externally owned register backup. */
int NgBenchLegacyCreate(void)
{
    ngeneXmmsBackup = LegacyAllocate(4 * XMM_REGISTER_SIZE);
    return ngeneXmmsBackup != NULL ? InitFftTable(&configuration, &table) : -1;
}

/** @brief Release original plan and backup after all timed calls finish. */
void NgBenchLegacyDestroy(void)
{
    DeinitFftTable(&table);
    LegacyFree(ngeneXmmsBackup);
}

/** @brief Invoke original PerformFft on aligned arrays with every stage scaled by two. */
int NgBenchLegacyExecute(size_t n, const int16_t *input, int16_t *output)
{
    unsigned bits = 0;
    size_t x = n;
    while (x > 1) {
        ++bits;
        x >>= 1;
    }
    return PerformFft(&configuration, &table, (short *)(uintptr_t)input, output, bits, -(int)bits);
}
