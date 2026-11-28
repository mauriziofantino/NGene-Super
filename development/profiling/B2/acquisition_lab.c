/** @file acquisition_lab.c @brief Offline B1C primary pilot acquisition comparison. */
#include "ngene/dsp/beidou_b1c.h"
#include "dsp/sample_backend.h"
#include "platform/platform.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NG_B1C_FFT_SIZE 16384u
#define NG_B1C_BLOCK (NG_B1C_FFT_SIZE / 2)
#define NG_B1C_MAX_SAMPLES 81838u
#define NG_B1C_MAX_BLOCKS ((NG_B1C_MAX_SAMPLES + NG_B1C_BLOCK - 1) / NG_B1C_BLOCK)
#define NG_B1C_TAU 6.28318530717958647692

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "line %d: %s\n", __LINE__, #condition);                                \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

typedef struct {
    TNgSearchFft *fft;    /**< Shared tables for grid and zero-padded blocks. */
    double nativeEnergy;  /**< Matched replica energy for normalization. */
    double gridEnergy;    /**< Energy of the zero-lag grid replica. */
    size_t count, blocks; /**< Native samples per period and number of linear partitions. */
    TNgComplex *code, *carrier, *mixed, *capture; /**< Native samples in capture order. */
    TNgComplex *codeSpectra, *signalSpectra;      /**< Transform storage for linear blocks. */
    TNgComplex *gridSpectrum, *work, *product;    /**< Grid and independent FFT scratch. */
    TNgComplex *map;      /**< Complex cyclic correlation in increasing delay order. */
    float *codeI, *codeQ; /**< Two repeated periods for contiguous real SIMD dot products. */
} TNgB1cLab;

typedef struct {
    size_t delay;                   /**< Circular lag on the grid of the chosen backend. */
    double hz, power, milliseconds; /**< Best row frequency, peak power and scan wall time. */
} TNgB1cPeak;

/** Allocate test arrays and fail with a diagnostic on insufficient memory. */
static void *Allocate(size_t count, size_t bytes)
{
    void *out = calloc(count, bytes);
    CHECK(out != NULL);
    return out;
}

/** Add interleaved complex vectors with two complex lanes per iteration. */
static void Accumulate(TNgComplex *to, const TNgComplex *from, size_t count)
{
    size_t k = 0;
#if NG_SSE2
    for (; k + 2 <= count; k += 2) {
        _mm_storeu_ps((float *)(to + k), _mm_add_ps(_mm_loadu_ps((const float *)(to + k)),
                                                    _mm_loadu_ps((const float *)(from + k))));
    }
#elif NG_NEON
    for (; k + 2 <= count; k += 2) {
        vst1q_f32((float *)(to + k), vaddq_f32(vld1q_f32((const float *)(to + k)),
                                               vld1q_f32((const float *)(from + k))));
    }
#endif
    for (; k < count; ++k) {
        to[k].i += from[k].i;
        to[k].q += from[k].q;
    }
}

/** Box-average identical rational cells with widened I and Q lanes. */
static void Grid(const TNgB1cLab *lab, const TNgComplex *input, TNgComplex *output)
{
    size_t begin = 0;
    for (size_t k = 0; k < NG_B1C_FFT_SIZE; ++k) {
        const size_t end = (k + 1) * lab->count / NG_B1C_FFT_SIZE;
        const double width = (double)(end - begin);
#if NG_SSE2
        __m128d sum = _mm_setzero_pd();
        for (size_t n = begin; n < end; ++n) {
            const __m128 z = _mm_loadl_pi(_mm_setzero_ps(), (const __m64 *)(input + n));
            sum = _mm_add_pd(sum, _mm_cvtps_pd(z));
        }
        _mm_storel_pi((__m64 *)(output + k), _mm_cvtpd_ps(_mm_div_pd(sum, _mm_set1_pd(width))));
#elif NG_NEON
        float64x2_t sum = vdupq_n_f64(0);
        for (size_t n = begin; n < end; ++n) {
            sum = vaddq_f64(sum, vcvt_f64_f32(vld1_f32((const float *)(input + n))));
        }
        vst1_f32((float *)(output + k), vcvt_f32_f64(vdivq_f64(sum, vdupq_n_f64(width))));
#else
        double i = 0, q = 0;
        for (size_t n = begin; n < end; ++n) {
            i += input[n].i;
            q += input[n].q;
        }
        output[k] = (TNgComplex){(float)(i / width), (float)(q / width)};
#endif
        begin = end;
    }
}

/** Apply the fixture filter before preparing the shared matched replica. */
static void FilterReplica(TNgB1cLab *lab, uint32_t rate)
{
    /* Reference model only: periodic, symmetric 17-tap low-pass on the sampled code.
     * Noise is added after this step. There is no model of the analog front end. */
    const double cutoff = fmin(0.39, 2500000.0 / rate);
    double taps[17], gain = 0;
    for (int t = -8; t <= 8; ++t) {
        const double sinc = t ? sin(NG_B1C_TAU * cutoff * t) / (0.5 * NG_B1C_TAU * t) : 2 * cutoff;
        taps[t + 8] = sinc * (0.54 + 0.46 * cos(NG_B1C_TAU * t / 16));
        gain += taps[t + 8];
    }
    for (size_t k = 0; k < lab->count; ++k) {
        double i = 0, q = 0;
        for (size_t t = 0; t < 17; ++t) {
            const TNgComplex c = lab->code[(k + lab->count + t - 8) % lab->count];
            i += taps[t] * c.i;
            q += taps[t] * c.q;
        }
        lab->mixed[k] = (TNgComplex){(float)(i / gain), (float)(q / gain)};
    }
    memcpy(lab->code, lab->mixed, lab->count * sizeof(*lab->code));
}

/** Prepare filtered or ideal replica and both coarse templates once per PRN. */
static void Prepare(TNgB1cLab *lab, unsigned prn, uint32_t rate, int filtered)
{
    int8_t chips[NG_BEIDOU_B1C_LENGTH];
    CHECK(NgBeidouB1cGenerate(prn, NG_BEIDOU_B1C_PILOT, chips, NULL) == NG_OK);
    lab->count = rate / 100;
    lab->blocks = (lab->count + NG_B1C_BLOCK - 1) / NG_B1C_BLOCK;
    CHECK(lab->count >= NG_B1C_FFT_SIZE && lab->count <= NG_B1C_MAX_SAMPLES);
    for (size_t k = 0; k < lab->count; ++k) {
        const uint64_t phase = (uint64_t)k * NG_BEIDOU_B1C_RATE_HZ * NG_BEIDOU_B1C_SUBCHIPS / rate;
        lab->code[k] = NgBeidouB1cPilotAt(chips, phase);
    }
    if (filtered) {
        FilterReplica(lab, rate);
    }
    lab->nativeEnergy = 0;
    for (size_t k = 0; k < lab->count; ++k) {
        const TNgComplex c = lab->code[k];
        lab->nativeEnergy += (double)c.i * c.i + (double)c.q * c.q;
        lab->codeI[k] = lab->codeI[k + lab->count] = lab->code[k].i;
        lab->codeQ[k] = lab->codeQ[k + lab->count] = lab->code[k].q;
    }
    Grid(lab, lab->code, lab->gridSpectrum);
    lab->gridEnergy = 0;
    for (size_t k = 0; k < NG_B1C_FFT_SIZE; ++k) {
        const TNgComplex c = lab->gridSpectrum[k];
        lab->gridEnergy += (double)c.i * c.i + (double)c.q * c.q;
    }
    NgSearchFftExecute(lab->fft, lab->gridSpectrum, 0);
    for (size_t k = 0; k < NG_B1C_FFT_SIZE; ++k) {
        lab->gridSpectrum[k].q = -lab->gridSpectrum[k].q;
    }
    memset(lab->codeSpectra, 0, lab->blocks * NG_B1C_FFT_SIZE * sizeof(*lab->codeSpectra));
    for (size_t k = 0; k < lab->count; ++k) {
        const TNgComplex c = lab->code[k ? lab->count - k : 0];
        lab->codeSpectra[k / NG_B1C_BLOCK * NG_B1C_FFT_SIZE + k % NG_B1C_BLOCK] =
            (TNgComplex){c.i, -c.q};
    }
    for (size_t b = 0; b < lab->blocks; ++b) {
        NgSearchFftExecute(lab->fft, lab->codeSpectra + b * NG_B1C_FFT_SIZE, 0);
    }
}

/** Build a reproducible cyclic delay plus coherent carrier and independent complex noise. */
static void Capture(TNgB1cLab *lab, uint32_t rate, size_t delay, double hz, unsigned interval,
                    float amplitude)
{
    uint32_t noise = UINT32_C(2654435761) ^ (interval + 1u);
    for (size_t k = 0; k < lab->count; ++k) {
        const TNgComplex c = lab->code[(k + lab->count - delay) % lab->count];
        const double angle = NG_B1C_TAU * hz * ((double)interval / 100 + (double)k / rate);
        const double cosine = cos(angle), sine = sin(angle);
        noise = noise * UINT32_C(1664525) + UINT32_C(1013904223);
        const double i = ((double)(noise >> 16) / 32768 - 1) * 0.10;
        noise = noise * UINT32_C(1664525) + UINT32_C(1013904223);
        const double q = ((double)(noise >> 16) / 32768 - 1) * 0.10;
        lab->capture[k].i = (float)(amplitude * (c.i * cosine - c.q * sine) + i);
        lab->capture[k].q = (float)(amplitude * (c.q * cosine + c.i * sine) + q);
    }
}

/** SIMD wipe-off at the native rate; use the same FFT input for both coarse methods. */
static void Mix(TNgB1cLab *lab, uint32_t rate, double hz)
{
    memcpy(lab->mixed, lab->capture, lab->count * sizeof(*lab->mixed));
    NgSearchCarrier(lab->carrier, lab->count, rate, hz, 0, 0);
    NgSearchMix(lab->mixed, lab->carrier, lab->count);
}

/** Grid correlation using complex pilot weights and the shared SIMD PCPS primitives. */
static void GridMap(TNgB1cLab *lab, const TNgComplex *mixed)
{
    Grid(lab, mixed, lab->map);
    NgSearchFftExecute(lab->fft, lab->map, 0);
    NgSearchMix(lab->map, lab->gridSpectrum, NG_B1C_FFT_SIZE);
    NgSearchFftExecute(lab->fft, lab->map, 1);
}

/** Exact cyclic map from SIMD block convolution followed by folding at the true period. */
static void BlockMap(TNgB1cLab *lab)
{
    memset(lab->map, 0, lab->count * sizeof(*lab->map));
    for (size_t b = 0; b < lab->blocks; ++b) {
        TNgComplex *spectrum = lab->signalSpectra + b * NG_B1C_FFT_SIZE;
        const size_t left = lab->count - b * NG_B1C_BLOCK;
        const size_t length = left < NG_B1C_BLOCK ? left : NG_B1C_BLOCK;
        memset(spectrum, 0, NG_B1C_FFT_SIZE * sizeof(*spectrum));
        memcpy(spectrum, lab->mixed + b * NG_B1C_BLOCK, length * sizeof(*spectrum));
        NgSearchFftExecute(lab->fft, spectrum, 0);
    }
    for (size_t d = 0; d < 2 * lab->blocks - 1; ++d) {
        const size_t first = d < lab->blocks ? 0 : d + 1 - lab->blocks;
        const size_t last = d < lab->blocks ? d : lab->blocks - 1;
        const size_t offset = d * NG_B1C_BLOCK;
        const size_t available = 2 * lab->count - 1 - offset;
        const size_t length = available < NG_B1C_FFT_SIZE - 1 ? available : NG_B1C_FFT_SIZE - 1;
        const size_t start = offset % lab->count;
        const size_t head = lab->count - start < length ? lab->count - start : length;
        memset(lab->work, 0, NG_B1C_FFT_SIZE * sizeof(*lab->work));
        for (size_t b = first; b <= last; ++b) {
            memcpy(lab->product, lab->signalSpectra + b * NG_B1C_FFT_SIZE,
                   NG_B1C_FFT_SIZE * sizeof(*lab->product));
            NgSearchMix(lab->product, lab->codeSpectra + (d - b) * NG_B1C_FFT_SIZE,
                        NG_B1C_FFT_SIZE);
            Accumulate(lab->work, lab->product, NG_B1C_FFT_SIZE);
        }
        NgSearchFftExecute(lab->fft, lab->work, 1);
        Accumulate(lab->map + start, lab->work, head);
        Accumulate(lab->map, lab->work + head, length - head);
    }
}

/** Return distance modulo the native period. */
static size_t Distance(size_t a, size_t b, size_t count)
{
    const size_t d = a > b ? a - b : b - a;
    return d < count - d ? d : count - d;
}

/** Accumulate a prompt span in the phase frame of the whole period. */
static void PromptSpan(const TNgB1cLab *lab, size_t delay, size_t from, size_t to, double *i,
                       double *q)
{
    const size_t start = lab->count - delay + from;
    double ii, qi, iq, qq;
    NgSearchCorrelate(lab->mixed + from, lab->codeI + start, to - from, 0, &ii, &qi);
    NgSearchCorrelate(lab->mixed + from, lab->codeQ + start, to - from, 0, &iq, &qq);
    *i += ii + qq;
    *q += qi - iq;
}

/** Combine the two SIMD real-code correlations into complex prompt power. */
static double Prompt(const TNgB1cLab *lab, size_t delay)
{
    double i = 0, q = 0;
    PromptSpan(lab, delay, 0, lab->count, &i, &q);
    return i * i + q * q;
}

/** Compute complex energy in widened arithmetic. */
static double Power(TNgComplex value)
{
    return (double)value.i * value.i + (double)value.q * value.q;
}

/** Keep the first lag on a tie, regardless of backend. */
static void Consider(TNgB1cPeak *best, size_t lag, double hz, double power)
{
    if (power > best->power) {
        best->delay = lag;
        best->hz = hz;
        best->power = power;
    }
}

/** Reduce powers in native lag order, widening before squaring. */
static void PeakRow(const TNgComplex *map, size_t size, double hz, TNgB1cPeak *best)
{
    size_t k = 0;
#if NG_SSE2
    for (; k + 2 <= size; k += 2) {
        const __m128 z = _mm_loadu_ps((const float *)(map + k));
        const __m128d a = _mm_cvtps_pd(z);
        const __m128d b = _mm_cvtps_pd(_mm_movehl_ps(z, z));
        const __m128d i = _mm_unpacklo_pd(a, b), q = _mm_unpackhi_pd(a, b);
        const __m128d p = _mm_add_pd(_mm_mul_pd(i, i), _mm_mul_pd(q, q));
        Consider(best, k, hz, _mm_cvtsd_f64(p));
        Consider(best, k + 1, hz, _mm_cvtsd_f64(_mm_unpackhi_pd(p, p)));
    }
#elif NG_NEON
    for (; k + 2 <= size; k += 2) {
        const float32x2x2_t z = vld2_f32((const float *)(map + k));
        const float64x2_t i = vcvt_f64_f32(z.val[0]), q = vcvt_f64_f32(z.val[1]);
        const float64x2_t p = vaddq_f64(vmulq_f64(i, i), vmulq_f64(q, q));
        Consider(best, k, hz, vgetq_lane_f64(p, 0));
        Consider(best, k + 1, hz, vgetq_lane_f64(p, 1));
    }
#endif
    for (; k < size; ++k) {
        Consider(best, k, hz, Power(map[k]));
    }
}

typedef enum { NG_B1C_GRID, NG_B1C_BLOCKS, NG_B1C_STAGGERED } TNgB1cCoarseMode;

/** Translate the best grid lag and alignment offset back to a native delay. */
static void GridPeak(TNgB1cLab *lab, const TNgComplex *input, size_t offset, double hz,
                     TNgB1cPeak *best)
{
    TNgB1cPeak row = {0};
    GridMap(lab, input);
    PeakRow(lab->map, NG_B1C_FFT_SIZE, hz, &row);
    const size_t lag = (row.delay * lab->count + NG_B1C_FFT_SIZE / 2) / NG_B1C_FFT_SIZE;
    Consider(best, (lag + offset) % lab->count, hz, row.power);
}

/** Search identical Doppler rows using a native map or one or two grid alignments. */
static TNgB1cPeak Coarse(TNgB1cLab *lab, uint32_t rate, TNgB1cCoarseMode mode)
{
    const double bins[] = {-100, 0, 100};
    const double start = NgMonotonicSeconds();
    const size_t offset = (lab->count + NG_B1C_FFT_SIZE) / (2 * NG_B1C_FFT_SIZE);
    TNgB1cPeak best = {0};
    for (size_t b = 0; b < sizeof(bins) / sizeof(*bins); ++b) {
        Mix(lab, rate, bins[b]);
        if (mode == NG_B1C_BLOCKS) {
            BlockMap(lab);
            PeakRow(lab->map, lab->count, bins[b], &best);
        } else {
            GridPeak(lab, lab->mixed, 0, bins[b], &best);
            if (mode == NG_B1C_STAGGERED) {
                const size_t head = lab->count - offset;
                memcpy(lab->carrier, lab->mixed + offset, head * sizeof(*lab->carrier));
                memcpy(lab->carrier + head, lab->mixed, offset * sizeof(*lab->carrier));
                GridPeak(lab, lab->carrier, offset, bins[b], &best);
            }
        }
    }
    best.milliseconds = 1000 * (NgMonotonicSeconds() - start);
    return best;
}

/** Evaluate a separate Doppler interval and code interval with native SIMD prompts. */
static TNgB1cPeak Refine(TNgB1cLab *lab, uint32_t rate, TNgB1cPeak coarse, size_t delay, double hz)
{
    TNgB1cPeak best = coarse;
    best.power = 0;
    Capture(lab, rate, delay, hz, 1, 1);
    for (int delta = -50; delta <= 50; delta += 10) {
        const double frequency = coarse.hz + delta;
        Mix(lab, rate, frequency);
        const double power = Prompt(lab, coarse.delay);
        if (power > best.power) {
            best.hz = frequency;
            best.power = power;
        }
    }
    Capture(lab, rate, delay, hz, 2, 1);
    Mix(lab, rate, best.hz);
    const size_t radius = (rate + NG_BEIDOU_B1C_RATE_HZ - 1) / NG_BEIDOU_B1C_RATE_HZ;
    best.power = 0;
    for (size_t k = 0; k <= 2 * radius; ++k) {
        const size_t lag = (coarse.delay + lab->count - radius + k) % lab->count;
        const double power = Prompt(lab, lag);
        if (power > best.power) {
            best.delay = lag;
            best.power = power;
        }
    }
    return best;
}

/** Check three-of-four sequencing with disjoint clean/noise fixtures and a test-only amplitude cut. */
static unsigned Confirm(TNgB1cLab *lab, uint32_t rate, TNgB1cPeak peak, size_t delay, double hz,
                        unsigned present)
{
    const double cut = 0.25 * (double)lab->count;
    unsigned successes = 0;
    for (unsigned attempt = 0; attempt < 4; ++attempt) {
        Capture(lab, rate, delay, hz, attempt + 3, (present & (1u << attempt)) ? 1.0f : 0.0f);
        Mix(lab, rate, peak.hz);
        successes += Prompt(lab, peak.delay) > cut * cut;
        if (successes >= 3 || successes + 3 - attempt < 3) {
            break;
        }
    }
    return successes;
}

/** Direct double-precision circular oracle at a selected lag. */
static TNgComplex Oracle(const TNgB1cLab *lab, size_t delay)
{
    double i = 0, q = 0;
    for (size_t k = 0; k < lab->count; ++k) {
        const TNgComplex c = lab->code[(k + lab->count - delay) % lab->count];
        const TNgComplex x = lab->mixed[k];
        i += (double)x.i * c.i + (double)x.q * c.q;
        q += (double)x.q * c.i - (double)x.i * c.q;
    }
    return (TNgComplex){(float)i, (float)q};
}

/** Test truth, periodic boundaries and short-lag BOC ambiguity at both native rates. */
static void Run(TNgB1cLab *lab, uint32_t rate, unsigned prn)
{
    Prepare(lab, prn, rate, 0);
    const size_t delays[] = {
        0, 1, lab->count - 1, lab->count / 3, NG_B1C_BLOCK - 1, NG_B1C_BLOCK, NG_B1C_BLOCK + 1};
    for (unsigned t = 0; t < sizeof(delays) / sizeof(*delays); ++t) {
        const size_t delay = delays[t];
        const double coarseHz = t < 3 ? 0 : (t % 2 == 0 ? 100 : -100);
        const double hz = coarseHz + (t % 2 == 0 ? 30 : -30);
        TNgB1cPeak grid, block;
        Capture(lab, rate, delay, hz, 0, 1);
        if (t % 2 == 0) {
            grid = Coarse(lab, rate, NG_B1C_GRID);
            block = Coarse(lab, rate, NG_B1C_BLOCKS);
        } else {
            block = Coarse(lab, rate, NG_B1C_BLOCKS);
            grid = Coarse(lab, rate, NG_B1C_GRID);
        }
        CHECK(block.delay == delay && block.hz == coarseHz);
        Mix(lab, rate, hz);
        BlockMap(lab);
        const size_t halfChip = (rate + NG_BEIDOU_B1C_RATE_HZ) / (2 * NG_BEIDOU_B1C_RATE_HZ);
        const size_t probes[] = {0,
                                 1,
                                 lab->count - 1,
                                 delay,
                                 (delay + halfChip) % lab->count,
                                 (delay + NG_B1C_BLOCK) % lab->count,
                                 lab->count / 2};
        double residual = 0;
        for (unsigned k = 0; k < sizeof(probes) / sizeof(*probes); ++k) {
            const TNgComplex sample = lab->map[probes[k]];
            const TNgComplex ref = Oracle(lab, probes[k]);
            const double error =
                hypot((double)sample.i - ref.i, (double)sample.q - ref.q) / (double)lab->count;
            if (error > residual) {
                residual = error;
            }
        }
        CHECK(residual < 2e-6);
        const double sidelobe =
            Power(lab->map[(delay + halfChip) % lab->count]) / Power(lab->map[delay]);
        const double nativeEnergy = lab->nativeEnergy;
        const double blockGain = block.power / (nativeEnergy * nativeEnergy);
        const double gridGain = grid.power / (lab->gridEnergy * lab->gridEnergy);
        const double gridLossDb = 10 * log10(gridGain / blockGain);
        TNgB1cPeak refined = Refine(lab, rate, block, delay, hz);
        CHECK(refined.delay == delay && refined.hz == hz);
        CHECK(Confirm(lab, rate, refined, delay, hz, 0xbu) == 3);
        CHECK(Confirm(lab, rate, refined, delay, hz, 0x5u) == 2);
        CHECK(Confirm(lab, rate, refined, delay, hz, 0) == 0);
        refined = Refine(lab, rate, grid, delay, hz);
        CHECK(refined.delay == delay && refined.hz == hz);
        printf("%u,%u,%zu,%.0f,%zu,%.0f,%zu,%.0f,%.3f,%.3f,%.8g,%.6f,%.4f\n", rate, prn, delay, hz,
               Distance(grid.delay, delay, lab->count), grid.hz,
               Distance(refined.delay, delay, lab->count), refined.hz, grid.milliseconds,
               block.milliseconds, residual, sidelobe, gridLossDb);
    }
}

/** Verify coherent subdivision using the full-period carrier and code phase origins. */
static double CheckTranches(const TNgB1cLab *lab, size_t delay)
{
    double i = 0, q = 0;
    for (size_t ms = 0; ms < 10; ++ms) {
        PromptSpan(lab, delay, ms * lab->count / 10, (ms + 1) * lab->count / 10, &i, &q);
    }
    const TNgComplex oracle = Oracle(lab, delay);
    return hypot(i - oracle.i, q - oracle.q) / (double)lab->count;
}

/** Compare coarse methods on filtered, weak-signal fixtures at every offset near zero. */
static void Sweep(TNgB1cLab *lab, uint32_t rate, unsigned prn)
{
    const float amplitudes[] = {0.004f, 0.012f};
    const size_t radius = (rate + NG_BEIDOU_B1C_RATE_HZ - 1) / NG_BEIDOU_B1C_RATE_HZ;
    const TNgB1cCoarseMode modes[] = {NG_B1C_GRID, NG_B1C_STAGGERED, NG_B1C_BLOCKS};
    Prepare(lab, prn, rate, 1);
    for (unsigned level = 0; level < sizeof(amplitudes) / sizeof(*amplitudes); ++level) {
        for (unsigned t = 0; t < 24; ++t) {
            const size_t delay =
                t < 12 ? (lab->count + t - 6) % lab->count : (lab->count / 3 + t - 12) % lab->count;
            const double hz = (t % 2 == 0) ? 30 : -30;
            TNgB1cPeak peak[3];
            Capture(lab, rate, delay, hz, t, amplitudes[level]);
            for (unsigned order = 0; order < 3; ++order) {
                const unsigned m = (order + t + level) % 3;
                peak[m] = Coarse(lab, rate, modes[m]);
            }
            Mix(lab, rate, hz);
            const double trancheError = CheckTranches(lab, delay);
            CHECK(trancheError < 2e-7);
            for (unsigned m = 0; m < 3; ++m) {
                const size_t miss = Distance(peak[m].delay, delay, lab->count);
                const double energy = m == 2 ? lab->nativeEnergy : lab->gridEnergy;
                const double normalized = peak[m].power / (energy * energy);
                const double exact = peak[2].power / (lab->nativeEnergy * lab->nativeEnergy);
                printf("%u,%u,%.3f,%zu,%.0f,%u,%zu,%.0f,%d,%.3f,%.5f,%.9g\n", rate, prn,
                       amplitudes[level], delay, hz, m, miss, peak[m].hz,
                       miss <= radius && peak[m].hz == 0.0, peak[m].milliseconds,
                       10 * log10(normalized / exact), trancheError);
            }
        }
    }
}

/** Run synthetic B2 only on a build with actual vector FFT and correlation kernels. */
int main(int argc, char **argv)
{
    TNgB1cLab lab = {0};
    TNgError error = {{0}};
#if !NG_SSE2 && !NG_NEON
    fputs("B1C lab requires the SSE2 or AArch64 NEON DSP backend\n", stderr);
    return 1;
#endif
    if (argc > 2 || (argc == 2 && strcmp(argv[1], "--phase-sweep") != 0)) {
        fprintf(stderr, "usage: %s [--phase-sweep]\n", argv[0]);
        return 1;
    }
    CHECK(NgSearchFftCreate(NG_B1C_FFT_SIZE, &lab.fft, &error) == NG_OK);
    lab.code = Allocate(NG_B1C_MAX_SAMPLES, sizeof(*lab.code));
    lab.carrier = Allocate(NG_B1C_MAX_SAMPLES, sizeof(*lab.carrier));
    lab.mixed = Allocate(NG_B1C_MAX_SAMPLES, sizeof(*lab.mixed));
    lab.capture = Allocate(NG_B1C_MAX_SAMPLES, sizeof(*lab.capture));
    lab.codeSpectra = Allocate(NG_B1C_MAX_BLOCKS * NG_B1C_FFT_SIZE, sizeof(*lab.codeSpectra));
    lab.signalSpectra = Allocate(NG_B1C_MAX_BLOCKS * NG_B1C_FFT_SIZE, sizeof(*lab.signalSpectra));
    lab.gridSpectrum = Allocate(NG_B1C_FFT_SIZE, sizeof(*lab.gridSpectrum));
    lab.work = Allocate(NG_B1C_FFT_SIZE, sizeof(*lab.work));
    lab.product = Allocate(NG_B1C_FFT_SIZE, sizeof(*lab.product));
    lab.map = Allocate(NG_B1C_MAX_SAMPLES, sizeof(*lab.map));
    lab.codeI = Allocate(2 * NG_B1C_MAX_SAMPLES, sizeof(*lab.codeI));
    lab.codeQ = Allocate(2 * NG_B1C_MAX_SAMPLES, sizeof(*lab.codeQ));
    if (argc == 2) {
        puts("rate_hz,prn,amplitude,delay,doppler_hz,method,delay_error,coarse_hz,in_basin,"
             "coarse_ms,relative_peak_db,tranche_error");
        Sweep(&lab, 2048000, 1);
        Sweep(&lab, 8000000, 33);
        Sweep(&lab, 8183800, 63);
    } else {
        puts(
            "rate_hz,prn,delay,doppler_hz,grid_error,grid_hz,refined_grid_error,refined_grid_hz,"
            "grid_ms,block_ms,max_normalized_complex_error,half_chip_relative_power,grid_relative_peak_db");
        Run(&lab, 2048000, 1);
        Run(&lab, 2048000, 32);
        Run(&lab, 8183800, 33);
        Run(&lab, 8183800, 63);
    }
    free(lab.codeQ);
    free(lab.codeI);
    free(lab.map);
    free(lab.product);
    free(lab.work);
    free(lab.gridSpectrum);
    free(lab.signalSpectra);
    free(lab.codeSpectra);
    free(lab.capture);
    free(lab.mixed);
    free(lab.carrier);
    free(lab.code);
    NgSearchFftDestroy(lab.fft);
    return 0;
}
