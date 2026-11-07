/* Development-only candidate kernels, not operational receiver implementations.
 * Replica preparation is outside kernel timing; ingress/conversion are timed separately.
 * No experimental source is linked into the receiver. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "ngene/dsp/gps_ca.h"
#include "platform/platform.h"
#ifdef _WIN32
#include <windows.h>
#endif
#if (defined(_M_X64) || defined(__x86_64__)) && !defined(REFERENCE_ONLY)
#include <emmintrin.h>
#define SSE 1
#endif
#define PI 3.14159265358979323846
#define MAX_N 64003
#define MODES 5
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x);                                   \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
#ifdef _MSC_VER
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE __attribute__((noinline))
#endif
static const char *names[MODES] = {"double_oracle", "iq16_q15_simd", "iq8_q15_simd",
                                   "iq8_xor_square", "sign1_xor_square"};

typedef struct {
    double i, q;
} Complex;

typedef struct {
    size_t n;
    double raw[2 * MAX_N], r[2 * MAX_N], j[2 * MAX_N];
    int16_t x16[2 * MAX_N], wr[2 * MAX_N], wj[2 * MAX_N];
    int8_t x8[2 * MAX_N];
    uint8_t mr[2 * MAX_N], mj[2 * MAX_N];
    uint32_t br[(2 * MAX_N + 31) / 32], bj[(2 * MAX_N + 31) / 32];
    unsigned negativesR, negativesJ;
} Block;

static Block block;
static volatile double sink;
static int Compare(const void *a, const void *b);
static uint32_t rng = 1729;

static double Now(void)
{
#ifdef _WIN32
    LARGE_INTEGER t, f;
    QueryPerformanceCounter(&t);
    QueryPerformanceFrequency(&f);
    return (double)t.QuadPart / (double)f.QuadPart;
#else
    struct timespec t;
    timespec_get(&t, TIME_UTC);
    return (double)t.tv_sec + 1e-9 * t.tv_nsec;
#endif
}

static double Uniform(void)
{
    rng = rng * 1664525u + 1013904223u;
    return ((double)rng + 0.5) / 4294967296.0;
}

static double Gaussian(void)
{
    return sqrt(-2 * log(Uniform())) * cos(2 * PI * Uniform());
}

static double Clip(double x)
{
    return x < -1 ? -1 : x > 0.999969482421875 ? 0.999969482421875 : x;
}

static int16_t Quant16(double x)
{
    long y = lround(2048 * x);
    return (int16_t)(y < -2048 ? -2048 : y > 2047 ? 2047 : y);
}

static int8_t Quant8(double x)
{
    long y = lround(128 * x);
    return (int8_t)(y < -128 ? -128 : y > 127 ? 127 : y);
}

static void Convert(Block *b)
{
    size_t k;
    for (k = 0; k < 2 * b->n; k++) {
        b->x16[k] = Quant16(b->raw[k]);
        b->x8[k] = Quant8(b->raw[k]);
    }
}

static void Prepare(Block *b, size_t n, double fs, double ifHz, double codeOffset)
{
    int8_t chips[1023];
    TNgGpsCaNco code;
    TNgError error = {{0}};
    size_t k;
    CHECK(n <= MAX_N && NgGpsCaGenerate(7, chips, &error) == NG_OK);
    CHECK(NgGpsCaNcoInit(&code, fs, 1023000, codeOffset, &error) == NG_OK);
    b->n = n;
    b->negativesR = b->negativesJ = 0;
    memset(b->br, 0, sizeof(b->br));
    memset(b->bj, 0, sizeof(b->bj));
    for (k = 0; k < n; k++) {
        double phase = 2 * PI * (0.137 + (ifHz + 1375.25) * (double)k / fs);
        double c = cos(phase), s = sin(phase);
        int prn = NgGpsCaNext(chips, &code);
        unsigned t;
        b->r[2 * k] = prn * c;
        b->r[2 * k + 1] = prn * s;
        b->j[2 * k] = -prn * s;
        b->j[2 * k + 1] = prn * c;
        for (t = 0; t < 2; t++) {
            size_t p = 2 * k + t;
            b->wr[p] = (int16_t)lround(b->r[p] * 32767);
            b->wj[p] = (int16_t)lround(b->j[p] * 32767);
            b->mr[p] = b->r[p] < 0 ? 255 : 0;
            b->mj[p] = b->j[p] < 0 ? 255 : 0;
            b->negativesR += (unsigned)(b->mr[p] != 0);
            b->negativesJ += (unsigned)(b->mj[p] != 0);
            if (b->mr[p])
                b->br[p / 32] |= UINT32_C(1) << (p % 32);
            if (b->mj[p])
                b->bj[p / 32] |= UINT32_C(1) << (p % 32);
        }
    }
}

static void Generate(Block *b, double fs, double ifHz, double signal, double sigma, double dc,
                     double cw)
{
    int8_t chips[1023];
    TNgGpsCaNco code;
    TNgError error = {{0}};
    size_t k;
    CHECK(NgGpsCaGenerate(7, chips, &error) == NG_OK);
    CHECK(NgGpsCaNcoInit(&code, fs, 1023000, 0, &error) == NG_OK);
    for (k = 0; k < b->n; k++) {
        double phase = 2 * PI * (0.137 + (ifHz + 1375.25) * (double)k / fs),
               a = signal * NgGpsCaNext(chips, &code);
        double interferer = 2 * PI * (ifHz + 42123.75) * (double)k / fs;
        b->raw[2 * k] = Clip(a * cos(phase) + sigma * Gaussian() + dc + cw * cos(interferer));
        b->raw[2 * k + 1] =
            Clip(a * sin(phase) + sigma * Gaussian() - 0.7 * dc + cw * sin(interferer));
    }
    Convert(b);
}

static int64_t Dot16Scalar(const int16_t *x, const int16_t *w, size_t n)
{
    int64_t sum = 0;
    size_t k;
    for (k = 0; k < n; k++)
        sum += (int32_t)x[k] * w[k];
    return sum;
}
#ifdef SSE
static __m128i AddWide(__m128i acc, __m128i x)
{
    __m128i sign = _mm_srai_epi32(x, 31);
    return _mm_add_epi64(acc,
                         _mm_add_epi64(_mm_unpacklo_epi32(x, sign), _mm_unpackhi_epi32(x, sign)));
}

static int64_t Finish(__m128i a)
{
    int64_t v[2];
    _mm_storeu_si128((__m128i *)v, a);
    return v[0] + v[1];
}
#endif
static int64_t Dot16(const int16_t *x, const int16_t *w, size_t n)
{
    size_t k = 0;
    int64_t sum = 0;
#ifdef SSE
    __m128i acc = _mm_setzero_si128();
    for (; k + 8 <= n; k += 8)
        acc = AddWide(acc, _mm_madd_epi16(_mm_loadu_si128((const __m128i *)(x + k)),
                                          _mm_loadu_si128((const __m128i *)(w + k))));
    sum = Finish(acc);
#endif
    for (; k < n; k++)
        sum += (int32_t)x[k] * w[k];
    return sum;
}

static int64_t Dot8(const int8_t *x, const int16_t *w, size_t n)
{
    size_t k = 0;
    int64_t sum = 0;
#ifdef SSE
    __m128i acc = _mm_setzero_si128(), zero = _mm_setzero_si128();
    for (; k + 16 <= n; k += 16) {
        __m128i v = _mm_loadu_si128((const __m128i *)(x + k)), sign = _mm_cmpgt_epi8(zero, v);
        acc = AddWide(acc, _mm_madd_epi16(_mm_unpacklo_epi8(v, sign),
                                          _mm_loadu_si128((const __m128i *)(w + k))));
        acc = AddWide(acc, _mm_madd_epi16(_mm_unpackhi_epi8(v, sign),
                                          _mm_loadu_si128((const __m128i *)(w + k + 8))));
    }
    sum = Finish(acc);
#endif
    for (; k < n; k++)
        sum += (int32_t)x[k] * w[k];
    return sum;
}

/* XOR keeps 8-bit amplitudes. Complement differs from negation by exactly one.
 * Correct with the number of negative replica elements, even for x=-128. */
static int64_t Xor8(const int8_t *x, const uint8_t *m, size_t n, unsigned negatives)
{
    size_t k = 0;
    int64_t sum = negatives;
#ifdef SSE
    __m128i acc = _mm_setzero_si128(), zero = _mm_setzero_si128(), bias = _mm_set1_epi8((char)-128);
    for (; k + 16 <= n; k += 16) {
        __m128i v = _mm_xor_si128(_mm_loadu_si128((const __m128i *)(x + k)),
                                  _mm_loadu_si128((const __m128i *)(m + k)));
        acc = _mm_add_epi64(acc, _mm_sad_epu8(_mm_xor_si128(v, bias), zero));
    }
    sum += Finish(acc) - (int64_t)k * 128;
#endif
    for (; k < n; k++) {
        unsigned v = (uint8_t)x[k] ^ m[k];
        sum += v >= 128 ? (int)v - 256 : (int)v;
    }
    return sum;
}

static unsigned Pop(uint32_t x)
{
    x -= ((x >> 1) & 0x55555555u);
    x = (x & 0x33333333u) + ((x >> 2) & 0x33333333u);
    x = (x + (x >> 4)) & 0x0f0f0f0fu;
    return (x * 0x01010101u) >> 24;
}

static Complex Bits(const Block *b)
{
    size_t k = 0, n = 2 * b->n;
    int64_t r = 0, j = 0;
    for (; k < n; k += 32) {
        unsigned t, count = (unsigned)(n - k < 32 ? n - k : 32);
        uint32_t bits = 0, mask = count == 32 ? UINT32_MAX : ((UINT32_C(1) << count) - 1);
#ifdef SSE
        if (count == 32) {
            bits = (uint32_t)_mm_movemask_epi8(_mm_loadu_si128((const __m128i *)(b->x8 + k)));
            bits |= (uint32_t)_mm_movemask_epi8(_mm_loadu_si128((const __m128i *)(b->x8 + k + 16)))
                    << 16;
        } else
#endif
            for (t = 0; t < count; t++)
                if (b->x8[k + t] < 0)
                    bits |= UINT32_C(1) << t;
        r += (int)count - 2 * (int)Pop((bits ^ b->br[k / 32]) & mask);
        j += (int)count - 2 * (int)Pop((bits ^ b->bj[k / 32]) & mask);
    }
    {
        Complex z = {(double)r, (double)j};
        return z;
    }
}

static NOINLINE Complex Run(const Block *b, int mode, double sigma)
{
    Complex z = {0, 0};
    size_t k;
    double scale = 1;
    if (mode == 0) {
        for (k = 0; k < 2 * b->n; k++) {
            z.i += b->raw[k] * b->r[k];
            z.q += b->raw[k] * b->j[k];
        }
        return z;
    }
    if (mode == 1) {
        z.i = (double)Dot16(b->x16, b->wr, 2 * b->n);
        z.q = (double)Dot16(b->x16, b->wj, 2 * b->n);
        scale = 1.0 / (2048 * 32767.0);
    }
    if (mode == 2) {
        z.i = (double)Dot8(b->x8, b->wr, 2 * b->n);
        z.q = (double)Dot8(b->x8, b->wj, 2 * b->n);
        scale = 1.0 / (128 * 32767.0);
    }
    if (mode == 3) {
        z.i = (double)Xor8(b->x8, b->mr, 2 * b->n, b->negativesR);
        z.q = (double)Xor8(b->x8, b->mj, 2 * b->n, b->negativesJ);
        scale = PI / (4 * 128);
    }
    if (mode == 4) {
        z = Bits(b);
        scale = sigma * sqrt(PI / 2) * PI / 4;
    }
    z.i *= scale;
    z.q *= scale;
    return z;
}

static void Contracts(void)
{
    int v, sign;
    size_t k, n;
    uint8_t masks[521];
    int8_t x[521];
    int16_t w[521], wide[521];
    for (v = -128; v <= 127; v++)
        for (sign = 0; sign <= 1; sign++) {
            uint8_t a = (uint8_t)v ^ (sign ? 255 : 0);
            int y = a >= 128 ? (int)a - 256 : (int)a;
            CHECK(y + sign == (sign ? -v : v));
        }
    for (k = 0; k < 521; k++) {
        x[k] = (int8_t)((int)(k % 256) - 128);
        wide[k] = x[k];
        w[k] = (int16_t)((int)(k * 59 % 65535) - 32767);
        masks[k] = (uint8_t)(k % 3 ? 255 : 0);
    }
    for (n = 1; n <= 521; n++) {
        int64_t exact = 0;
        unsigned neg = 0;
        for (k = 0; k < n; k++) {
            exact += masks[k] ? -(int)x[k] : x[k];
            neg += (unsigned)(masks[k] != 0);
        }
        CHECK(Xor8(x, masks, n, neg) == exact);
        CHECK(Dot8(x, w, n) == Dot16Scalar(wide, w, n));
        CHECK(Dot16(wide, w, n) == Dot16Scalar(wide, w, n));
    }
    for (k = 0; k < 521; k++)
        wide[k] = (int16_t)(k % 2 ? -32768 : 32767);
    CHECK(Dot16(wide, w, 521) == Dot16Scalar(wide, w, 521));
    Prepare(&block, 16003, 16000000, 0, 0);
    Generate(&block, 16000000, 0, 0.05, 0.14, 0, 0);
    for (k = 0; k < 2 * block.n; k++)
        block.x16[k] = (int16_t)((int)block.x8[k] * 16);
    {
        Complex a = Run(&block, 1, 0.14), b = Run(&block, 2, 0.14);
        CHECK(fabs(a.i - b.i) < 1e-10 && fabs(a.q - b.q) < 1e-10);
    }
    for (n = 1; n <= 16003; n += 997) {
        Complex z;
        int64_t a = 0, c = 0;
        block.n = n;
        for (k = 0; k < 2 * n; k++) {
            a += (block.x8[k] < 0 ? -1 : 1) * (block.mr[k] ? -1 : 1);
            c += (block.x8[k] < 0 ? -1 : 1) * (block.mj[k] ? -1 : 1);
        }
        z = Bits(&block);
        CHECK(z.i == (double)a && z.q == (double)c);
    }
    puts(
        "# PASS: exhaustive byte XOR identity; SIMD/scalar exact dot products, rails and all tails 1..521; packed signs and tails.");
}

static int16_t common[2 * MAX_N];
static uint8_t compact[2 * MAX_N];

static NOINLINE void Adapter(const Block *b, int mode)
{
    size_t k = 0, n = 2 * b->n;
    if (mode == 0) {
        memcpy(compact, b->x8, n);
        return;
    }
#ifdef SSE
    for (; k + 16 <= n; k += 16) {
        if (mode == 1) {
            __m128i v = _mm_loadu_si128((const __m128i *)(b->x8 + k));
            _mm_storeu_si128((__m128i *)(compact + k), _mm_xor_si128(v, _mm_set1_epi8((char)-128)));
        } else if (mode == 2) {
            __m128i v = _mm_loadu_si128((const __m128i *)(b->x8 + k)),
                    sign = _mm_cmpgt_epi8(_mm_setzero_si128(), v);
            _mm_storeu_si128((__m128i *)(common + k),
                             _mm_slli_epi16(_mm_unpacklo_epi8(v, sign), 8));
            _mm_storeu_si128((__m128i *)(common + k + 8),
                             _mm_slli_epi16(_mm_unpackhi_epi8(v, sign), 8));
        } else {
            __m128i a = _mm_loadu_si128((const __m128i *)(b->x16 + k)),
                    c = _mm_loadu_si128((const __m128i *)(b->x16 + k + 8));
            if (mode == 3) {
                _mm_storeu_si128((__m128i *)(common + k), _mm_slli_epi16(a, 4));
                _mm_storeu_si128((__m128i *)(common + k + 8), _mm_slli_epi16(c, 4));
            } else {
                a = _mm_srai_epi16(_mm_add_epi16(a, _mm_set1_epi16(8)), 4);
                c = _mm_srai_epi16(_mm_add_epi16(c, _mm_set1_epi16(8)), 4);
                _mm_storeu_si128((__m128i *)(compact + k), _mm_packs_epi16(a, c));
            }
        }
    }
#endif
    for (; k < n; k++) {
        if (mode == 1)
            compact[k] = (uint8_t)b->x8[k] ^ 128u;
        else if (mode == 2)
            common[k] = (int16_t)((int)b->x8[k] * 256);
        else if (mode == 3)
            common[k] = (int16_t)((int)b->x16[k] * 16);
        else {
            int x = (int)floor(((double)b->x16[k] + 8) / 16);
            compact[k] = (uint8_t)(x > 127 ? 127 : x);
        }
    }
}

static void AdapterTiming(void)
{
    const char *labels[] = {"ci8_copy", "cu8_to_ci8", "ci8_to_iq16", "q11_to_iq16",
                            "q11_to_ci8_lossy"};
    int m, r;
    size_t k;
    const unsigned repeats = 1000;
    Prepare(&block, 16003, 16000000, 0, 0);
    Generate(&block, 16000000, 0, 0.03, 0.14, 0, 0);
    /* Validate all adapter modes including unaligned vector tails and every Q11 level. */
    for (k = 0; k < 2 * block.n; k++)
        block.x16[k] = (int16_t)((int)(k % 4096) - 2048);
    for (m = 0; m < 5; m++) {
        double samples[15];
        Adapter(&block, m);
        for (k = 0; k < 2 * block.n; k++) {
            if (m == 0)
                CHECK(compact[k] == (uint8_t)block.x8[k]);
            if (m == 1)
                CHECK(compact[k] == ((uint8_t)block.x8[k] ^ 128u));
            if (m == 2)
                CHECK(common[k] == (int)block.x8[k] * 256);
            if (m == 3)
                CHECK(common[k] == (int)block.x16[k] * 16);
            if (m == 4) {
                int x = (int)floor(((double)block.x16[k] + 8) / 16);
                CHECK(compact[k] == (uint8_t)(x > 127 ? 127 : x));
            }
        }
        for (r = 0; r < 15; r++) {
            unsigned t;
            double start = Now();
            for (t = 0; t < repeats; t++) {
                Adapter(&block, m);
                sink += common[t % 32] + compact[t % 32];
            }
            samples[r] = (Now() - start) * 1e6 / repeats;
        }
        qsort(samples, 15, sizeof(double), Compare);
        printf("adapter,%s,%zu,%.4f,%.4f,%.3f\n", labels[m], block.n, samples[7], samples[14],
               block.n / samples[7]);
    }
}

static int Compare(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return x < y ? -1 : x > y;
}

static void Timing(void)
{
    size_t sizes[] = {16000, 64000, 16003}, s;
    int mode, round;
    puts("timing,mode,n,median_us,p95_batch_us,Mcomplex_s");
    for (s = 0; s < 3; s++) {
        double times[MODES][15];
        unsigned reps[MODES];
        Prepare(&block, sizes[s], 16000000, 4000000, 0);
        Generate(&block, 16000000, 4000000, 0.01, 0.14, 0, 0);
        for (mode = 0; mode < MODES; mode++) {
            unsigned r = 1;
            double elapsed;
            do {
                unsigned t;
                double start = Now();
                for (t = 0; t < r; t++) {
                    Complex z = Run(&block, mode, 0.14);
                    sink += z.i + z.q;
                }
                elapsed = Now() - start;
                if (elapsed < 0.003)
                    r *= 2;
            } while (elapsed < 0.003);
            reps[mode] = r;
        }
        for (round = 0; round < 15; round++)
            for (mode = 0; mode < MODES; mode++) {
                int m = (mode + round) % MODES;
                unsigned t;
                double start = Now();
                for (t = 0; t < reps[m]; t++) {
                    Complex z = Run(&block, m, 0.14);
                    sink += z.i + z.q;
                }
                times[m][round] = (Now() - start) * 1e6 / reps[m];
            }
        for (mode = 0; mode < MODES; mode++) {
            qsort(times[mode], 15, sizeof(double), Compare);
            printf("timing,%s,%zu,%.4f,%.4f,%.3f\n", names[mode], sizes[s], times[mode][7],
                   times[mode][14], sizes[s] / times[mode][7]);
        }
    }
}

static void Quality(void)
{
    const char *cases[] = {"clean", "weak", "dc", "cw", "clipping", "code_halfchip"};
    int c, m;
    puts("quality,case,mode,if_hz,relative_vector_error,phase_error_deg");
    for (c = 0; c < 6; c++) {
        unsigned f;
        for (f = 0; f < 2; f++) {
            double ifHz = f ? 4000000.0 : 0;
            Complex reference;
            Prepare(&block, 64000, 16000000, ifHz, c == 5 ? 0.5 : 0);
            rng = 1729;
            Generate(&block, 16000000, ifHz, c == 1 ? 0.003 : 0.03, 0.14, c == 2 ? 0.10 : 0,
                     c == 3   ? 0.35
                     : c == 4 ? 1.2
                              : 0);
            reference = Run(&block, 0, 0.14);
            for (m = 1; m < MODES; m++) {
                Complex z = Run(&block, m, 0.14);
                double error = hypot(z.i - reference.i, z.q - reference.q) /
                               (hypot(reference.i, reference.q) + 1e-12);
                double phase = atan2(z.q * reference.i - z.i * reference.q,
                                     z.i * reference.i + z.q * reference.q) *
                               180 / PI;
                if (m == 1)
                    CHECK(error < 0.001);
                if (m == 2)
                    CHECK(error < 0.01);
                printf("quality,%s,%s,%.0f,%.8f,%.6f\n", cases[c], names[m], ifHz, error, phase);
            }
        }
    }
}

static void Harmonics(void)
{
    const int harmonics[] = {1, -3, 5};
    unsigned h;
    int m;
    double fundamental[MODES] = {0};
    int8_t chips[1023];
    TNgError error = {{0}};
    CHECK(NgGpsCaGenerate(7, chips, &error) == NG_OK);
    puts("harmonic,mode,carrier_multiple,prompt_relative_to_fundamental");
    Prepare(&block, 64000, 16000000, 15625 - 1375.25, 0);
    for (h = 0; h < 3; h++) {
        size_t k;
        TNgGpsCaNco code;
        CHECK(NgGpsCaNcoInit(&code, 16000000, 1023000, 0, &error) == NG_OK);
        for (k = 0; k < block.n; k++) {
            double phase = harmonics[h] * 2 * PI * (0.137 + 15625 * (double)k / 16000000);
            double value = 0.1 * NgGpsCaNext(chips, &code);
            block.raw[2 * k] = value * cos(phase);
            block.raw[2 * k + 1] = value * sin(phase);
        }
        Convert(&block);
        for (m = 0; m < MODES; m++) {
            Complex z = Run(&block, m, 0.14);
            double amplitude = hypot(z.i, z.q);
            if (!h)
                fundamental[m] = amplitude;
            if (h && m <= 2)
                CHECK(amplitude / fundamental[m] < 0.01);
            if (h && m == 3)
                CHECK(fabs(amplitude / fundamental[m] - 1.0 / fabs((double)harmonics[h])) < 0.005);
            printf("harmonic,%s,%d,%.8f\n", names[m], harmonics[h], amplitude / fundamental[m]);
        }
    }
}

/* Paired noise-only and weak-signal realizations: estimate correlation deflection.
 * Loss is relative to double oracle; this is not a detection probability/PVT test. */
static void Sensitivity(void)
{
    unsigned trial, f;
    int m;
    const unsigned trials = 1024;
    puts("sensitivity,mode,if_hz,trials,loss_db");
    for (f = 0; f < 2; f++) {
        double sum[MODES] = {0}, square[MODES] = {0}, response[MODES] = {0}, deflection[MODES],
               ifHz = f ? 4000000.0 : 0;
        Prepare(&block, 16000, 16000000, ifHz, 0);
        for (trial = 0; trial < trials; trial++) {
            double off[MODES];
            uint32_t seed = 1729 + trial * 7919;
            rng = seed;
            Generate(&block, 16000000, ifHz, 0, 0.14, 0, 0);
            for (m = 0; m < MODES; m++) {
                off[m] = Run(&block, m, 0.14).i;
                sum[m] += off[m];
                square[m] += off[m] * off[m];
            }
            rng = seed;
            Generate(&block, 16000000, ifHz, 0.003, 0.14, 0, 0);
            for (m = 0; m < MODES; m++)
                response[m] += Run(&block, m, 0.14).i - off[m];
        }
        for (m = 0; m < MODES; m++) {
            double var = (square[m] - sum[m] * sum[m] / trials) / (trials - 1);
            deflection[m] = pow(response[m] / trials, 2) / var;
        }
        for (m = 1; m < MODES; m++)
            printf("sensitivity,%s,%.0f,%u,%.6f\n", names[m], ifHz, trials,
                   10 * log10(deflection[0] / deflection[m]));
    }
}

typedef struct {
    int mode;
    unsigned repetitions;
    TNgAtomicInt ready, go;
    double value, elapsed;
} Worker;

static void Work(void *context)
{
    Worker *w = context;
    unsigned k;
    double start, total = 0;
    NgAtomicSet(&w->ready, 1);
    while (!NgAtomicGet(&w->go))
        NgSleepMs(1);
    start = Now();
    for (k = 0; k < w->repetitions; k++) {
        Complex z = Run(&block, w->mode, 0.14);
        total += z.i + z.q;
    }
    w->elapsed = Now() - start;
    w->value = total;
}

static void Parallel(void)
{
    unsigned count, k;
    int mode, round;
    Prepare(&block, 16000, 16000000, 4000000, 0);
    Generate(&block, 16000000, 4000000, 0.01, 0.14, 0, 0);
    puts("parallel,mode,workers,median_Mcomplex_s,p95_batch_us_per_window");
    for (mode = 1; mode < MODES; mode++)
        for (count = 1; count <= 4; count *= 2) {
            double rates[7], latencies[7];
            for (round = 0; round < 7; round++) {
                TNgThread threads[4];
                Worker workers[4];
                double longest = 0;
                memset(workers, 0, sizeof(workers));
                for (k = 0; k < count; k++) {
                    workers[k].mode = mode;
                    workers[k].repetitions = 8192;
                    CHECK(NgThreadStart(&threads[k], Work, &workers[k]) == 0);
                }
                for (k = 0; k < count; k++)
                    while (!NgAtomicGet(&workers[k].ready))
                        NgSleepMs(1);
                for (k = 0; k < count; k++)
                    NgAtomicSet(&workers[k].go, 1);
                for (k = 0; k < count; k++) {
                    NgThreadJoin(threads[k]);
                    if (workers[k].elapsed > longest)
                        longest = workers[k].elapsed;
                    sink += workers[k].value;
                    CHECK(fabs(workers[k].value - workers[0].value) < 1e-9);
                }
                rates[round] = (double)count * 8192 * 16000 / longest / 1e6;
                latencies[round] = longest / 8192 * 1e6;
            }
            qsort(rates, 7, sizeof(double), Compare);
            qsort(latencies, 7, sizeof(double), Compare);
            printf("parallel,%s,%u,%.3f,%.4f\n", names[mode], count, rates[3], latencies[6]);
        }
}

static NOINLINE void StreamCopy(const int8_t *input, const int16_t *input16, void *output,
                                size_t components, int mode)
{
    size_t k = 0;
    int16_t *out = output;
    if (mode != 1) {
        memcpy(output, mode == 0 ? (const void *)input : (const void *)input16,
               components * (mode == 0 ? 1u : 2u));
        return;
    }
#ifdef SSE
    for (; k + 16 <= components; k += 16) {
        __m128i v = _mm_loadu_si128((const __m128i *)(input + k)),
                sign = _mm_cmpgt_epi8(_mm_setzero_si128(), v);
        _mm_storeu_si128((__m128i *)(out + k), _mm_slli_epi16(_mm_unpacklo_epi8(v, sign), 8));
        _mm_storeu_si128((__m128i *)(out + k + 8), _mm_slli_epi16(_mm_unpackhi_epi8(v, sign), 8));
    }
#endif
    for (; k < components; k++)
        out[k] = (int16_t)((int)input[k] * 256);
}

static void StreamTiming(void)
{
    const size_t components = 32u * 1024u * 1024u;
    size_t k;
    int8_t *source = malloc(components);
    int16_t *source16 = malloc(components * 2u);
    unsigned char *dest = malloc(components * 2u);
    int mode, round;
    const char *labels[] = {"ring_ci8_copy", "ring_ci8_to_iq16", "ring_iq16_copy"};
    CHECK(source && source16 && dest);
    for (k = 0; k < components; k++) {
        source[k] = (int8_t)((int)(k % 256) - 128);
        source16[k] = (int16_t)((int)source[k] * 256);
    }
    puts("stream,mode,complex_pairs,median_ms,p95_ms,Mcomplex_s");
    for (mode = 0; mode < 3; mode++) {
        double samples[15];
        StreamCopy(source, source16, dest, components, mode);
        for (round = 0; round < 15; round++) {
            double start = Now();
            StreamCopy(source, source16, dest, components, mode);
            samples[round] = (Now() - start) * 1000;
            sink += dest[(size_t)round * 4093];
        }
        if (mode == 0)
            CHECK(memcmp(source, dest, components) == 0);
        else
            CHECK(memcmp(source16, dest, components * 2u) == 0);
        qsort(samples, 15, sizeof(double), Compare);
        printf("stream,%s,%zu,%.4f,%.4f,%.3f\n", labels[mode], components / 2, samples[7],
               samples[14], (double)components / 2 / samples[7] / 1000);
    }
    free(dest);
    free(source16);
    free(source);
}

int main(int argc, char **argv)
{
#ifndef NDEBUG
    fprintf(stderr, "Release build required\n");
    return 1;
#endif
    printf("# compiler=%s backend=%s fixed_seed=1729\n", NGENE_EXPERIMENT_COMPILER,
#ifdef SSE
           "SSE2"
#else
           "C-reference"
#endif
    );
    Contracts();
    if (argc == 2 && strcmp(argv[1], "--verify") == 0) {
        Quality();
        Harmonics();
        return 0;
    }
    AdapterTiming();
    Timing();
    Quality();
    Harmonics();
    Sensitivity();
    Parallel();
    StreamTiming();
    return sink == 123456789.0;
}
