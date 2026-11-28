/** @file sbas_stream.c @brief Continuous SBAS K=7, rate 1/2 Viterbi and frame recovery. */
#include "navigation/sbas_stream.h"
#include <math.h>
#include <string.h>

/** Return the binary parity of a convolutional register mask. */
static unsigned Parity(unsigned value)
{
    value ^= value >> 4;
    value ^= value >> 2;
    return (value ^ (value >> 1)) & 1u;
}

/** Accept both Costas polarities, using the full frame CRC to reject sync candidates. */
static void Deliver(TNgSbasLane *lane, TNgSbasCorrections *bank, double now, unsigned value)
{
    lane->bits[lane->bit] = (uint8_t)value;
    lane->bit = (lane->bit + 1) % 250;
    if (lane->count < 250 && ++lane->count < 250) {
        return;
    }
    unsigned preamble = 0;
    for (unsigned k = 0; k < 8; ++k) {
        preamble = (preamble << 1) | lane->bits[(lane->bit + k) % 250];
    }
    unsigned invert = 0;
    if (preamble == 0xac || preamble == 0x65 || preamble == 0x39) {
        invert = 1;
    } else if (preamble != 0x53 && preamble != 0x9a && preamble != 0xc6) {
        return;
    }
    /* Adjacent symbol hypotheses may decode the same frame a millisecond apart. */
    if (bank->frameCount && now - bank->lastFrame < 0.5) {
        return;
    }
    uint8_t frame[NG_SBAS_FRAME_BYTES] = {0};
    for (unsigned k = 0; k < 250; ++k) {
        frame[k / 8] |= (uint8_t)((lane->bits[(lane->bit + k) % 250] ^ invert) << (7 - k % 8));
    }
    (void)NgSbasPushFrame(bank, frame, now);
}

/** One trellis transition with normalization and fixed-depth traceback. */
static void Pair(TNgSbasLane *lane, TNgSbasCorrections *bank, double first, double second,
                 double now)
{
    double next[64];
    unsigned best = 0;
    for (unsigned target = 0; target < 64; ++target) {
        const unsigned lower = target >> 1;
        const unsigned upper = lower + 32;
        const double weight =
            (Parity(target & 0117u) ? -first : first) + (Parity(target & 0155u) ? -second : second);
        const double fromLower = lane->metric[lower] + weight;
        const double fromUpper = lane->metric[upper] - weight;
        const int takeUpper = fromUpper > fromLower;
        lane->previous[lane->path][target] = (uint8_t)(takeUpper ? upper : lower);
        next[target] = takeUpper ? fromUpper : fromLower;
        if (next[target] > next[best]) {
            best = target;
        }
    }
    for (unsigned target = 0; target < 64; ++target) {
        lane->metric[target] = next[target] - next[best];
    }
    lane->path = (lane->path + 1) % NG_SBAS_TRACEBACK;
    if (lane->fill < NG_SBAS_TRACEBACK && ++lane->fill < NG_SBAS_TRACEBACK) {
        return;
    }
    unsigned state = best;
    for (unsigned back = 1; back < NG_SBAS_TRACEBACK; ++back) {
        state = lane->previous[(lane->path + NG_SBAS_TRACEBACK - back) % NG_SBAS_TRACEBACK][state];
    }
    Deliver(lane, bank, now, state & 1u);
}

void NgSbasPrompt(TNgSbasStream *stream, TNgSbasCorrections *bank, double prompt, double now)
{
    if (stream->havePrompt) {
        const unsigned phase = stream->parity;
        const double symbol = stream->prompt + prompt;
        for (unsigned pair = 0; pair < 2; ++pair) {
            TNgSbasLane *lane = &stream->lanes[2 * phase + pair];
            if (pair == stream->pairPhase[phase]) {
                lane->pending = symbol;
                lane->paired = 1;
            } else if (lane->paired) {
                Pair(lane, bank, lane->pending, symbol, now);
            }
        }
        stream->pairPhase[phase] ^= 1u;
    }
    stream->havePrompt = 1;
    stream->parity ^= 1u;
    stream->prompt = prompt;
}
