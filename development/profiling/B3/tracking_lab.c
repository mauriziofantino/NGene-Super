/** @file tracking_lab.c @brief Offline primary-pilot BOC(1,1) tracking DSP contract. */
#include "ngene/dsp/beidou_b1c.h"
#include "ngene/dsp/tracking_replica.h"
#include "dsp/sample_backend.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NG_B3_TAU 6.28318530717958647692
#define NG_B3_GUARD_RATIO 1.5
#define NG_B3_RATE_MAX 8183800u
#define NG_B3_WINDOW_MAX (NG_B3_RATE_MAX / 1000u + 1u)

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "line %d: %s\n", __LINE__, #x);                                        \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

typedef struct {
    double i, q; /**< Coherent coordinates, without magnitude conversion. */
} TNgB3Prompt;

typedef struct {
    size_t next, origin;      /**< Next sample and most recent rate-change sample. */
    double chips, cycles;     /**< Code and carrier phase at origin. */
    double codeHz, carrierHz; /**< Constant rates since origin. */
    unsigned wide;            /**< Include the two half-chip guard taps when requested. */
    TNgB3Prompt taps[7];      /**< Coherent Early, Prompt and Late, respectively. */
} TNgB3State;

typedef struct {
    TNgComplex rotated[NG_B3_WINDOW_MAX], carrier[NG_B3_WINDOW_MAX];
    double phases[NG_B3_WINDOW_MAX];
    float signs[NG_B3_WINDOW_MAX];
    int8_t halfChips[2 * NG_BEIDOU_B1C_LENGTH];
} TNgB3Workspace;

static const double offsets[] = {-0.25, 0, 0.25, -0.5, 0.5, -0.375, 0.375};

/** Fold a chip or carrier phase into its cyclic interval. */
static double Wrap(double phase, double period)
{
    const double rest = fmod(phase, period);
    return rest < 0 ? rest + period : rest;
}

/** Rebase both NCOs without changing their phase at the update boundary. */
static void SetRate(TNgB3State *state, uint32_t rate, double doppler, double ifHz)
{
    const size_t age = state->next - state->origin;
    state->chips = Wrap(state->chips + (double)age * state->codeHz / rate, NG_BEIDOU_B1C_LENGTH);
    state->cycles = Wrap(state->cycles + (double)age * state->carrierHz / rate, 1);
    state->origin = state->next;
    state->carrierHz = ifHz + doppler;
    state->codeHz = NG_BEIDOU_B1C_RATE_HZ + doppler / 1540.0;
}

/** Process native samples using the shared SIMD phase, replica, rotation and dot kernels. */
static int Window(TNgB3Workspace *work, TNgB3State *state, const TNgComplex *input, size_t first,
                  size_t count, uint32_t rate)
{
    if (state->codeHz <= 0 || first != state->next) {
        memset(state, 0, sizeof(*state));
        return NG_ERROR;
    }
    CHECK(count <= NG_B3_WINDOW_MAX);
    const size_t age = first - state->origin;
    const double chip =
        Wrap(state->chips + (double)age * state->codeHz / rate, NG_BEIDOU_B1C_LENGTH);
    const double cycles = Wrap(state->cycles + (double)age * state->carrierHz / rate, 1);
    NgSearchCarrier(work->carrier, count, rate, state->carrierHz, cycles, 0);
    memcpy(work->rotated, input, count * sizeof(*input));
    NgSearchMix(work->rotated, work->carrier, count);
    NgTrackingCodePhases(work->phases, count, chip, state->codeHz, rate);
    const unsigned taps = 3 + 2 * state->wide;
    for (unsigned tap = 0; tap < taps; ++tap) {
        double i, q;
        NgTrackingReplica(work->signs, work->phases, count, work->halfChips, NG_BEIDOU_B1C_LENGTH,
                          offsets[tap], 1);
        NgSearchCorrelate(work->rotated, work->signs, count, 1, &i, &q);
        state->taps[tap].i += i;
        state->taps[tap].q += q;
    }
    state->next += count;
    return NG_OK;
}

/** Evaluate the independently sampled code sign at an arbitrary chip phase. */
static int Sign(const int8_t *chips, double phase)
{
    const size_t half = (size_t)floor(2 * Wrap(phase, NG_BEIDOU_B1C_LENGTH));
    return half % 2 ? -chips[half / 2] : chips[half / 2];
}

/** Synthesize one Doppler segment and accumulate the scalar double I/Q oracle. */
static void Fixture(TNgComplex *samples, size_t count, const int8_t *chips, uint32_t rate,
                    double chip, double cycles, double doppler, double ifHz, double ramp,
                    float amplitude, TNgB3Prompt *ref)
{
    for (size_t n = 0; n < count; ++n) {
        const double time = (double)n / rate;
        const double dopplerCycles = time * (doppler + 0.5 * ramp * time);
        const double phase = chip + time * NG_BEIDOU_B1C_RATE_HZ + dopplerCycles / 1540.0;
        const double angle = NG_B3_TAU * (cycles + time * ifHz + dopplerCycles);
        const double sine = sin(angle), cosine = cos(angle);
        const double wanted = Sign(chips, phase);
        samples[n].i = (float)(-amplitude * wanted * sine);
        samples[n].q = (float)(amplitude * wanted * cosine);
        for (unsigned tap = 0; ref && tap < 3; ++tap) {
            const double sign = Sign(chips, phase - offsets[tap]);
            ref[tap].i += (samples[n].i * cosine + samples[n].q * sine) * sign;
            ref[tap].q += (samples[n].q * cosine - samples[n].i * sine) * sign;
        }
    }
}

/** Accumulate caller windows without rounding their boundaries to a millisecond. */
static void Segment(TNgB3Workspace *work, TNgB3State *state, const TNgComplex *samples,
                    size_t count, uint32_t rate, int irregular)
{
    const size_t first = state->next;
    for (size_t pos = 0; pos < count;) {
        size_t take = irregular ? 1 + (pos * 37 + 113) % 977 : rate / 1000;
        if (take > count - pos)
            take = count - pos;
        CHECK(Window(work, state, samples + pos, first + pos, take, rate) == NG_OK);
        pos += take;
    }
}

/** Preexpand the pilot primary to the two BOC(1,1) halves per chip. */
static void PreparePilot(TNgB3Workspace *work, int8_t *chips, unsigned prn)
{
    CHECK(NgBeidouB1cGenerate(prn, NG_BEIDOU_B1C_PILOT, chips, NULL) == NG_OK);
    for (size_t k = 0; k < NG_BEIDOU_B1C_LENGTH; ++k) {
        work->halfChips[2 * k] = chips[k];
        work->halfChips[2 * k + 1] = (int8_t)-chips[k];
    }
}

/** Exercise several code wraps, rate updates and an explicit sample gap. */
static void Run(TNgB3Workspace *work, uint32_t rate, unsigned prn)
{
    int8_t chips[NG_BEIDOU_B1C_LENGTH];
    const double doppler[] = {-2475, -1250, 375, 2311};
    const double ifHz = 38000;
    const size_t count = rate / 100;
    const double initialChip = NG_BEIDOU_B1C_LENGTH - 0.173763;
    const double initialCycles = 0.173;
    TNgB3State regular = {.chips = initialChip, .cycles = initialCycles};
    TNgB3State irregular = regular;
    TNgB3Prompt ref[3] = {{0}};
    double chip = initialChip, cycles = initialCycles;
    TNgComplex *samples = calloc(count, sizeof(*samples));
    CHECK(samples != NULL);
    PreparePilot(work, chips, prn);
    for (unsigned s = 0; s < 4; ++s) {
        Fixture(samples, count, chips, rate, chip, cycles, doppler[s], ifHz, 0, 1, ref);
        SetRate(&regular, rate, doppler[s], ifHz);
        SetRate(&irregular, rate, doppler[s], ifHz);
        Segment(work, &regular, samples, count, rate, 0);
        Segment(work, &irregular, samples, count, rate, 1);
        chip += (double)count / rate * (1023000.0 * (1 + doppler[s] / 1575420000.0));
        cycles += (double)count / rate * (ifHz + doppler[s]);
    }
    double oracleError = 0, partitionError = 0;
    for (unsigned tap = 0; tap < 3; ++tap) {
        const TNgB3Prompt a = regular.taps[tap], b = irregular.taps[tap];
        oracleError = fmax(oracleError, hypot(a.i - ref[tap].i, a.q - ref[tap].q));
        partitionError = fmax(partitionError, hypot(a.i - b.i, a.q - b.q));
    }
    oracleError /= (double)regular.next;
    partitionError /= (double)regular.next;
    CHECK(oracleError < 3e-7 && partitionError < 3e-7);
    CHECK(regular.taps[1].q > 0.999 * (double)regular.next);
    CHECK(regular.taps[1].q > fabs(regular.taps[0].q));
    CHECK(regular.taps[1].q > fabs(regular.taps[2].q));
    CHECK(Window(work, &regular, samples, regular.next + 3, 1, rate) == NG_ERROR);
    for (unsigned tap = 0; tap < 3; ++tap)
        CHECK(regular.taps[tap].i == 0 && regular.taps[tap].q == 0);
    CHECK(Window(work, &regular, samples, 0, 1, rate) == NG_ERROR);
    printf("%u,%u,%zu,%.9g,%.9g\n", rate, prn, irregular.next, oracleError, partitionError);
    free(samples);
}

/** Square the coherent tap after all windows in an integration have arrived. */
static double TapPower(TNgB3Prompt tap)
{
    return tap.i * tap.i + tap.q * tap.q;
}

typedef enum { NG_B3_BASELINE, NG_B3_FADE, NG_B3_GAP, NG_B3_SIDE, NG_B3_ECHO } TNgB3Scenario;

typedef struct {
    double delay, phase; /**< Echo delay in chips and phase in carrier cycles. */
    int lateralSeed;     /**< Sign of the original half-chip exception. */
} TNgB3Echo;

/** Return the strongest configured guard tap. */
static double Guard(const TNgB3State *state)
{
    double best = 0;
    for (unsigned k = 3; k < 3 + 2 * state->wide; ++k)
        best = fmax(best, TapPower(state->taps[k]));
    return best;
}

/** Exercise closed-loop tracking and supervised recovery with explicit fault injection. */
static void ClosedLoop(TNgB3Workspace *work, uint32_t rate, unsigned prn, int direction,
                       double ramp, float amplitude, TNgB3Scenario scenario, const TNgB3Echo *echo,
                       unsigned geometry)
{
    int8_t chips[NG_BEIDOU_B1C_LENGTH];
    const size_t count = rate / 100;
    const double ifHz = 38000, trueDoppler = 1234.5;
    const double dt = (double)count / rate;
    double chip = NG_BEIDOU_B1C_LENGTH - 0.2731;
    double cycles = 0.214;
    double estimate = trueDoppler + direction * 15;
    const double seedOffset = scenario == NG_B3_SIDE || echo ? 0.5 : 0.14;
    TNgB3State loop = {.chips = chip + direction * seedOffset,
                       .cycles = cycles + direction * 0.10,
                       .wide = geometry};
    TNgB3Prompt previous = {0};
    uint32_t noise = UINT32_C(8107);
    double phaseMax = 0, codeMax = 0, frequencyMax = 0;
    unsigned steady = 0, pllEpoch = 0, relockEpoch = 0, lossEpoch = 0;
    unsigned weak = 0, side = 0, falsePll = 0;
    double endCodeError = 0;
    int pll = 0, havePrevious = 0, waitingSeed = 0, rejected = 0;
    TNgComplex *samples = calloc(count, sizeof(*samples));
    TNgComplex *reflected = echo ? calloc(count, sizeof(*reflected)) : NULL;
    CHECK(samples && (!echo || reflected));
    PreparePilot(work, chips, prn);
    SetRate(&loop, rate, estimate, ifHz);
    for (unsigned epoch = 0; epoch < 80; ++epoch) {
        const double doppler = trueDoppler + ramp * (double)epoch * dt;
        const double nextDopplerCycles = dt * (doppler + 0.5 * dt * ramp);
        const double nextChip = Wrap(chip + dt * NG_BEIDOU_B1C_RATE_HZ + nextDopplerCycles / 1540.0,
                                     NG_BEIDOU_B1C_LENGTH);
        const double nextCycles = Wrap(cycles + dt * ifHz + nextDopplerCycles, 1);
        const size_t firstSample = epoch * count;
        double energy = 0;
        if (scenario == NG_B3_GAP) {
            if (epoch == 30) {
                /* Accumulate the first half, then deliver an index after the gap. */
                CHECK(pll);
                Fixture(samples, count, chips, rate, chip, cycles, doppler, ifHz, ramp, amplitude,
                        NULL);
                Segment(work, &loop, samples, count / 2, rate, 1);
                CHECK(TapPower(loop.taps[1]) > 0);
                CHECK(Window(work, &loop, samples, firstSample + count, 1, rate) == NG_ERROR);
                CHECK(loop.taps[1].i == 0 && loop.taps[1].q == 0);
                pll = havePrevious = 0;
                weak = steady = side = 0;
                lossEpoch = epoch + 1;
                waitingSeed = 1;
            }
            if (waitingSeed) {
                if (epoch < 33) {
                    CHECK(Window(work, &loop, samples, firstSample, 1, rate) == NG_ERROR);
                    chip = nextChip;
                    cycles = nextCycles;
                    continue;
                }
                /* A fresh acquisition result is injected by the fixture at epoch 33. */
                loop = (TNgB3State){.next = firstSample,
                                    .origin = firstSample,
                                    .chips = chip + direction * 0.14,
                                    .cycles = cycles + direction * 0.10,
                                    .wide = 1};
                estimate = doppler + direction * 15;
                SetRate(&loop, rate, estimate, ifHz);
                waitingSeed = 0;
            }
        }
        const float level = scenario == NG_B3_FADE && epoch >= 30 && epoch < 36 ? 0 : amplitude;
        Fixture(samples, count, chips, rate, chip, cycles, doppler, ifHz, ramp, level, NULL);
        if (echo) {
            Fixture(reflected, count, chips, rate, chip - echo->delay, cycles + echo->phase,
                    doppler, ifHz, ramp, 0.9f * level, NULL);
            for (size_t n = 0; n < count; ++n) {
                samples[n].i += reflected[n].i;
                samples[n].q += reflected[n].q;
            }
        }
        for (size_t n = 0; n < count; ++n) {
            noise = noise * UINT32_C(1664525) + UINT32_C(1013904223);
            samples[n].i += (float)((double)(noise >> 16) / 32768 - 1) * 0.35f;
            noise = noise * UINT32_C(1664525) + UINT32_C(1013904223);
            samples[n].q += (float)((double)(noise >> 16) / 32768 - 1) * 0.35f;
            energy += (double)samples[n].i * samples[n].i + (double)samples[n].q * samples[n].q;
        }
        Segment(work, &loop, samples, count, rate, 1);
        const TNgB3Prompt prompt = loop.taps[1];
        const double early = TapPower(loop.taps[0]), late = TapPower(loop.taps[2]);
        const double phase = atan2(-prompt.i, prompt.q) / NG_B3_TAU;
        const double dll = early + late > 0 ? (early - late) / (early + late) : 0;
        const double power = TapPower(prompt);
        const double coherence = energy > 0 ? power / ((double)count * energy) : 0;
        const double cross = previous.i * prompt.q - previous.q * prompt.i;
        const double dot = previous.i * prompt.i + previous.q * prompt.q;
        const double residual = atan2(cross, dot) / (NG_B3_TAU * dt);
        const int usable = coherence > 0.003;
        const double guard = Guard(&loop);
        const int sidePeak = loop.wide && usable && guard > NG_B3_GUARD_RATIO * power;
        if (sidePeak) {
            if (++side == 4) {
                rejected = 1;
                lossEpoch = epoch + 1;
                break;
            }
        } else {
            side = 0;
        }
        if (pll && !usable) {
            if (++weak == 3) {
                pll = 0;
                steady = 0;
                lossEpoch = epoch + 1;
            }
        } else {
            weak = 0;
        }
        if (!pll) {
            if (havePrevious && !sidePeak && usable && fabs(residual) < 2 &&
                power > 1.5 * fmax(early, late))
                ++steady;
            else
                steady = 0;
            if (steady == 4) {
                pll = 1;
                if (!pllEpoch)
                    pllEpoch = epoch + 1;
                else
                    relockEpoch = epoch + 1;
            }
        }
        if (echo) {
            endCodeError = fabs(remainder(loop.chips - chip, NG_BEIDOU_B1C_LENGTH));
            if (pll && endCodeError > 0.25)
                ++falsePll;
        }
        if (epoch >= 60 && amplitude > 0 && !echo) {
            phaseMax = fmax(phaseMax, fabs(phase));
            codeMax = fmax(codeMax, fabs(remainder(loop.chips - chip, NG_BEIDOU_B1C_LENGTH)));
            frequencyMax = fmax(frequencyMax, fabs(estimate - (doppler + 0.5 * dt * ramp)));
            CHECK(TapPower(prompt) > early && TapPower(prompt) > late);
        }
        if (pll && usable)
            estimate += 0.12 * phase / dt;
        else if (havePrevious && usable && !sidePeak)
            estimate += 0.55 * residual;
        SetRate(&loop, rate, estimate, ifHz);
        if (pll && usable)
            loop.cycles = Wrap(loop.cycles + 0.48 * phase, 1);
        if (usable && !sidePeak)
            loop.chips = Wrap(loop.chips + 0.075 * dll, NG_BEIDOU_B1C_LENGTH);
        memset(loop.taps, 0, sizeof(loop.taps));
        havePrevious = usable && !sidePeak;
        previous = prompt;
        chip = nextChip;
        cycles = nextCycles;
    }
    if (echo) {
        printf("%u,%u,%.3f,%.2f,%d,%u,%u,%d,%u,%d,%.7g\n", rate, prn, echo->delay, echo->phase,
               direction, geometry, pllEpoch, rejected, falsePll, pll, endCodeError);
        free(reflected);
        free(samples);
        return;
    }
    if (scenario != NG_B3_BASELINE) {
        printf("%u,%u,%d,%d,%u,%u,%u,%.7g\n", rate, prn, direction, (int)scenario, pllEpoch,
               lossEpoch, relockEpoch, codeMax);
    } else {
        printf("%u,%u,%d,%.1f,%.3f,%u,%.7g,%.7g,%.7g\n", rate, prn, direction, ramp, amplitude,
               pllEpoch, phaseMax, codeMax, frequencyMax);
    }
    if (scenario == NG_B3_SIDE) {
        CHECK(rejected && !pllEpoch && lossEpoch == 4);
    } else if (amplitude > 0) {
        CHECK(pllEpoch > 0 && pllEpoch <= 50);
        CHECK(phaseMax < 0.06 && codeMax < 0.08 && frequencyMax < 5);
        if (scenario == NG_B3_FADE)
            CHECK(lossEpoch == 33 && relockEpoch > 36 && relockEpoch <= 60);
        else if (scenario == NG_B3_GAP)
            CHECK(lossEpoch == 31 && relockEpoch > 33 && relockEpoch <= 60);
        else
            CHECK(!lossEpoch && !relockEpoch);
    } else {
        CHECK(pllEpoch == 0);
    }
    free(samples);
}

/** Update a deterministic PRNG and return a uniform value strictly inside (0,1). */
static double Uniform(uint32_t *state)
{
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return ((double)*state + 0.5) / 4294967296.0;
}

/** Inject independent Gaussian I/Q before the native SIMD despreader. */
static void Noise(TNgComplex *samples, size_t count, double sigma, uint32_t *state)
{
    for (size_t n = 0; n < count; ++n) {
        const double radius = sigma * sqrt(-2 * log(Uniform(state)));
        const double angle = NG_B3_TAU * Uniform(state);
        samples[n].i += (float)(radius * cos(angle));
        samples[n].q += (float)(radius * sin(angle));
    }
}

/** Measure the moment estimator on despread prompts for known C/N0. */
static void Cn0(TNgB3Workspace *work, uint32_t rate, unsigned prn)
{
    int8_t chips[NG_BEIDOU_B1C_LENGTH];
    const size_t count = rate / 100;
    const unsigned windows = 192;
    const double inputDb[] = {26, 35, 45};
    const double duration = (double)count / rate;
    const double sigma = 1;
    const double base = NG_BEIDOU_B1C_LENGTH - 0.23719;
    TNgComplex *clean = calloc(count, sizeof(*clean));
    TNgComplex *samples = calloc(count, sizeof(*samples));
    CHECK(clean && samples);
    PreparePilot(work, chips, prn);
    Fixture(clean, count, chips, rate, base, 0, 0, 0, 0, 1, NULL);
    for (unsigned level = 0; level < sizeof(inputDb) / sizeof(*inputDb); ++level) {
        /* Complex noise power is 2*sigma^2 per sample. */
        const double amplitude = sqrt(pow(10, inputDb[level] / 10) * (2 * sigma * sigma) / rate);
        double m2 = 0, m4 = 0, meanI = 0, meanQ = 0;
        uint32_t random = UINT32_C(593373) ^ rate;
        for (unsigned j = 0; j < windows; ++j) {
            TNgB3State state = {.chips = base};
            SetRate(&state, rate, 0, 0);
            for (size_t n = 0; n < count; ++n) {
                samples[n].i = 0;
                samples[n].q = (float)(amplitude * clean[n].q);
            }
            Noise(samples, count, sigma, &random);
            Segment(work, &state, samples, count, rate, 0);
            const TNgB3Prompt prompt = {state.taps[1].i / (double)count,
                                        state.taps[1].q / (double)count};
            const double p = TapPower(prompt);
            meanI += prompt.i;
            meanQ += prompt.q;
            m2 += p;
            m4 += p * p;
        }
        m2 /= windows;
        m4 /= windows;
        const double radicand = 2 * m2 * m2 - m4;
        CHECK(radicand > 0);
        const double signal = sqrt(radicand);
        const double knownPhase = (meanI * meanI + meanQ * meanQ) / ((double)windows * windows);
        CHECK(signal < m2 && knownPhase < m2);
        const double estimate = 10 * log10(signal / ((m2 - signal) * duration));
        const double coherent = 10 * log10(knownPhase / ((m2 - knownPhase) * duration));
        printf("cn0,%u,%u,%.0f,%.5f,%.5f\n", rate, prn, inputDb[level], estimate, coherent);
        CHECK(fabs(estimate - inputDb[level]) < 1.5);
        CHECK(fabs(coherent - inputDb[level]) < 1.5);
    }
    free(samples);
    free(clean);
}

/** Measure whether the guard veto is selective on one direct ray and one coherent echo. */
static void Multipath(TNgB3Workspace *work, uint32_t rate, unsigned prn)
{
    int8_t chips[NG_BEIDOU_B1C_LENGTH];
    const double delays[] = {0.25, 0.5, 0.75, 1.5};
    const double gains[] = {0, 0.25, 0.5, 0.9};
    const double seed[] = {0, -0.5, 0.5};
    const size_t count = rate / 100;
    const double base = NG_BEIDOU_B1C_LENGTH - 0.23719;
    TNgComplex *samples = calloc(count, sizeof(*samples));
    float *direct = calloc(count, sizeof(*direct));
    float *echo = calloc(count, sizeof(*echo));
    CHECK(samples && direct && echo);
    PreparePilot(work, chips, prn);
    for (size_t n = 0; n < count; ++n)
        direct[n] = (float)Sign(chips, base + (double)n * NG_BEIDOU_B1C_RATE_HZ / rate);
    for (unsigned g = 0; g < sizeof(gains) / sizeof(*gains); ++g) {
        unsigned trueVeto = 0, lobeVeto = 0, cases = 0;
        for (unsigned d = 0; d < sizeof(delays) / sizeof(*delays); ++d) {
            for (size_t n = 0; n < count; ++n) {
                const double chip = base + (double)n * NG_BEIDOU_B1C_RATE_HZ / rate;
                echo[n] = (float)Sign(chips, chip - delays[d]);
            }
            for (unsigned quadrant = 0; quadrant < 4; ++quadrant) {
                const double angle = (double)quadrant * NG_B3_TAU / 4;
                const float i = (float)(gains[g] * sin(angle));
                const float q = (float)(gains[g] * cos(angle));
                for (size_t n = 0; n < count; ++n) {
                    samples[n].i = -i * echo[n];
                    samples[n].q = direct[n] + q * echo[n];
                }
                for (unsigned k = 0; k < sizeof(seed) / sizeof(*seed); ++k) {
                    TNgB3State state = {.chips = base + seed[k], .wide = 1};
                    SetRate(&state, rate, 0, 0);
                    Segment(work, &state, samples, count, rate, 0);
                    const double guard = fmax(TapPower(state.taps[3]), TapPower(state.taps[4]));
                    const int veto = guard > NG_B3_GUARD_RATIO * TapPower(state.taps[1]);
                    if (!k)
                        trueVeto += (unsigned)veto;
                    else
                        lobeVeto += (unsigned)veto;
                    if ((k == 0 && veto) || (k != 0 && !veto))
                        printf("exception,%u,%u,%.2f,%.2f,%u,%.2f\n", rate, prn, gains[g],
                               delays[d], quadrant * 90, seed[k]);
                }
                ++cases;
            }
        }
        if (!g)
            CHECK(trueVeto == 0 && lobeVeto == 2 * cases);
        printf("multipath,%u,%u,%.2f,%u,%u,%u\n", rate, prn, gains[g], cases, trueVeto, lobeVeto);
    }
    free(echo);
    free(direct);
    free(samples);
}

/** Test only actual SIMD targets and keep scratch outside the operating receiver. */
int main(int argc, char **argv)
{
#if !NG_SSE2 && !NG_NEON
    fputs("B3 lab requires SSE2 or AArch64 NEON\n", stderr);
    return 1;
#endif
    TNgB3Workspace *work = calloc(1, sizeof(*work));
    CHECK(work != NULL);
    if (argc == 2 && strcmp(argv[1], "--closed-loop") == 0) {
        puts("rate_hz,prn,seed_sign,ramp_hz_per_s,amplitude,pll_epoch,max_phase_cycles,"
             "max_code_chips,max_doppler_hz");
        const uint32_t rates[] = {2048000, 8000000, 8183800};
        const unsigned prns[] = {1, 33, 63};
        const float amplitudes[] = {1.0f, 0.12f, 0.04f};
        const double ramps[] = {-35, -8, 0, 8, 35};
        for (unsigned k = 0; k < sizeof(rates) / sizeof(*rates); ++k) {
            for (int direction = -1; direction <= 1; direction += 2)
                for (unsigned level = 0; level < sizeof(amplitudes) / sizeof(*amplitudes); ++level)
                    for (unsigned slope = 0; slope < sizeof(ramps) / sizeof(*ramps); ++slope)
                        ClosedLoop(work, rates[k], prns[k], direction, ramps[slope],
                                   amplitudes[level], NG_B3_BASELINE, NULL, 0);
            ClosedLoop(work, rates[k], prns[k], 1, 0, 0, NG_B3_BASELINE, NULL, 0);
        }
        free(work);
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--recovery") == 0) {
        puts("rate_hz,prn,seed_sign,scenario,pll_epoch,loss_epoch,relock_epoch,max_code_chips");
        const uint32_t rates[] = {2048000, 8000000, 8183800};
        const unsigned prns[] = {1, 33, 63};
        for (unsigned k = 0; k < sizeof(rates) / sizeof(*rates); ++k)
            for (int direction = -1; direction <= 1; direction += 2) {
                ClosedLoop(work, rates[k], prns[k], direction, direction * 8, 0.12f, NG_B3_FADE,
                           NULL, 1);
                ClosedLoop(work, rates[k], prns[k], direction, direction * 8, 0.12f, NG_B3_GAP,
                           NULL, 1);
                ClosedLoop(work, rates[k], prns[k], direction, 0, 1, NG_B3_SIDE, NULL, 1);
            }
        free(work);
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--echo-loop") == 0) {
        puts("rate_hz,prn,echo_chips,echo_cycles,seed_sign,guard_pairs,pll_epoch,"
             "rejected,wrong_pll_epochs,pll_final,end_code_chips");
        const uint32_t rates[] = {2048000, 8000000, 8183800};
        const unsigned prns[] = {1, 33, 63};
        const TNgB3Echo cases[] = {
            {0.25, 0.5, -1}, {0.5, 0, 1}, {0.5, 0.25, -1}, {0.5, 0.5, -1}, {0.5, 0.75, -1}};
        for (unsigned rate = 0; rate < sizeof(rates) / sizeof(*rates); ++rate)
            for (unsigned c = 0; c < sizeof(cases) / sizeof(*cases); ++c)
                for (unsigned geometry = 1; geometry <= 2; ++geometry) {
                    ClosedLoop(work, rates[rate], prns[rate], 0, 0, 1, NG_B3_ECHO, &cases[c],
                               geometry);
                    ClosedLoop(work, rates[rate], prns[rate], cases[c].lateralSeed, 0, 1,
                               NG_B3_ECHO, &cases[c], geometry);
                }
        free(work);
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--quality") == 0) {
        puts("cn0,rate_hz,prn,input_dbhz,moments_dbhz,known_phase_dbhz");
        Cn0(work, 2048000, 1);
        Cn0(work, 8000000, 33);
        Cn0(work, 8183800, 63);
        puts("multipath,rate_hz,prn,echo_gain,cases,true_veto,lobe_veto");
        puts("exception,rate_hz,prn,echo_gain,echo_chips,echo_degrees,seed_chips");
        Multipath(work, 2048000, 1);
        Multipath(work, 8000000, 33);
        Multipath(work, 8183800, 63);
        free(work);
        return 0;
    }
    CHECK(argc == 1);
    puts("rate_hz,prn,samples,max_oracle_residual,max_partition_residual");
    Run(work, 2048000, 1);
    Run(work, 8000000, 33);
    Run(work, 8183800, 63);
    free(work);
    return 0;
}
