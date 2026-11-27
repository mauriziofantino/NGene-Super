/** @file sbas.c @brief EGNOS L1 mask, fast/range and long-term orbit/clock messages. */
#include "ngene/sbas.h"
#include <math.h>
#include <string.h>

/** Extract an MSB-first field of at most 12 bits. */
static unsigned Bits(const uint8_t *data, unsigned bit, unsigned size)
{
    unsigned value = 0;
    for (; size; --size, ++bit) {
        value = (value << 1) | ((unsigned)data[bit / 8] >> (7 - bit % 8) & 1u);
    }
    return value;
}

/** Decode a binary two's-complement field without implementation-defined casts. */
static int Signed(const uint8_t *data, unsigned bit, unsigned size)
{
    unsigned sign = 1u << (size - 1);
    return (int)(Bits(data, bit, size) ^ sign) - (int)sign;
}

/** The SBAS CRC covers 226 data bits and 24 transmitted check bits. */
static int Valid(const uint8_t *data)
{
    if (data[0] != 0x53 && data[0] != 0x9a && data[0] != 0xc6) {
        return 0;
    }
    uint32_t crc = 0;
    for (unsigned bit = 0; bit < 250; ++bit) {
        crc = (crc << 1) | Bits(data, bit, 1);
        if (crc & UINT32_C(0x1000000)) {
            crc ^= UINT32_C(0x1864cfb);
        }
    }
    return crc == 0;
}

/** Preserve corrections only for an unchanged, complete MT1 mask and issue. */
static void Mask(TNgSbasCorrections *bank, const uint8_t *data, double now)
{
    unsigned prn[NG_SBAS_MASK_SIZE] = {0};
    unsigned count = 0;
    for (unsigned slot = 1; slot <= 210; ++slot) {
        if (Bits(data, 13 + slot, 1)) {
            if (count == NG_SBAS_MASK_SIZE) {
                count = 0;
                break;
            }
            prn[count++] = slot;
        }
    }
    const unsigned iodp = Bits(data, 224, 2);
    if (iodp != bank->iodp || count != bank->count || memcmp(prn, bank->prn, sizeof(prn))) {
        memset(bank->fast, 0, sizeof(bank->fast));
        memset(bank->orbit, 0, sizeof(bank->orbit));
        memcpy(bank->prn, prn, sizeof(prn));
    }
    bank->count = count;
    bank->iodp = iodp;
    bank->maskTime = now;
}

/** Store each mask-relative fast correction and its current UDRE status. */
static void Fast(TNgSbasCorrections *bank, const uint8_t *data, unsigned block, unsigned count,
                 unsigned base, unsigned issue, double now)
{
    const unsigned iodf = Bits(data, issue + 2, 2);
    if (bank->iodp != Bits(data, issue, 2)) {
        return;
    }
    for (unsigned k = 0; k < count && 13 * block + k < bank->count; ++k) {
        const unsigned slot = 13 * block + k;
        bank->fast[slot].range = 0.125 * Signed(data, base + 12 * k, 12);
        bank->fast[slot].valid = Bits(data, base + 12 * count + 4 * k, 4) < 14;
        bank->fast[slot].iodf = iodf;
        bank->fast[slot].time = now;
    }
}

/** Decode one MT24/25 half, including optional orbit velocity and clock drift. */
static void LongTerm(TNgSbasCorrections *bank, const uint8_t *data, unsigned start, double now)
{
    const unsigned dynamic = Bits(data, start, 1);
    const unsigned iodp = Bits(data, start + (dynamic ? 104u : 103u), 2);
    if (iodp != bank->iodp) {
        return;
    }
    const unsigned width = dynamic ? 11u : 9u;
    for (unsigned k = 0; k < (dynamic ? 1u : 2u); ++k) {
        const unsigned base = start + 1u + 51u * k;
        const unsigned slot = Bits(data, base, 6);
        if (!slot || slot > bank->count) {
            continue;
        }
        const unsigned i = slot - 1;
        bank->orbit[i].iode = Bits(data, base + 6, 8);
        for (unsigned axis = 0; axis < 3; ++axis) {
            bank->orbit[i].xyz[axis] = 0.125 * Signed(data, base + 14 + axis * width, width);
            bank->orbit[i].velocity[axis] = dynamic ?
                ldexp(Signed(data, base + 58 + 8 * axis, 8), -11) : 0;
        }
        bank->orbit[i].clock = ldexp(Signed(data, base + 14 + 3 * width, dynamic ? 11u : 10u), -31);
        bank->orbit[i].drift = dynamic ? ldexp(Signed(data, base + 82, 8), -39) : 0;
        bank->orbit[i].tod = dynamic ? 16.0 * Bits(data, base + 90, 13) : 0;
        bank->orbit[i].time = now;
        bank->orbit[i].valid = 1;
        bank->orbit[i].dynamic = dynamic;
    }
}

int NgSbasPushFrame(TNgSbasCorrections *bank, const uint8_t data[NG_SBAS_FRAME_BYTES],
                    double now)
{
    if (!bank || !data || !isfinite(now) || now < 0 || !Valid(data) ||
        (bank->frameCount && now <= bank->lastFrame)) {
        return 0;
    }
    ++bank->frameCount;
    bank->lastFrame = now;
    const unsigned type = Bits(data, 8, 6);
    if (type == 0) {
        bank->count = 0;
        memset(bank->fast, 0, sizeof(bank->fast));
        memset(bank->orbit, 0, sizeof(bank->orbit));
        bank->blockedUntil = now + 60;
    } else if (now >= bank->blockedUntil) {
        if (type == 1) {
            Mask(bank, data, now);
        } else if (type >= 2 && type <= 5) {
            /* MT2-5 carry IODF then IODP, MT24 reverses the issue-field order. */
            const unsigned iodp = Bits(data, 16, 2);
            if (iodp == bank->iodp) {
                const unsigned iodf = Bits(data, 14, 2);
                for (unsigned k = 0; k < 13 && (type - 2) * 13 + k < bank->count; ++k) {
                    const unsigned slot = (type - 2) * 13 + k;
                    bank->fast[slot].range = 0.125 * Signed(data, 18 + 12 * k, 12);
                    bank->fast[slot].valid = Bits(data, 174 + 4 * k, 4) < 14;
                    bank->fast[slot].iodf = iodf;
                    bank->fast[slot].time = now;
                }
            }
        } else if (type == 6) {
            for (unsigned k = 0; k < bank->count; ++k) {
                unsigned issue = Bits(data, 14 + 2 * (k / 13), 2);
                if ((issue == 3 || issue == bank->fast[k].iodf) &&
                    Bits(data, 22 + 4 * k, 4) >= 14) {
                    bank->fast[k].valid = 0;
                }
            }
        } else if (type == 24) {
            Fast(bank, data, Bits(data, 112, 2), 6, 14, 110, now);
            LongTerm(bank, data, 120, now);
        } else if (type == 25) {
            LongTerm(bank, data, 14, now);
            LongTerm(bank, data, 120, now);
        }
    }
    return 1;
}

int NgSbasRead(const TNgSbasCorrections *bank, unsigned prn, unsigned iode, double now,
               double tx, TNgSbasCorrection *out)
{
    if (!out) {
        return 0;
    }
    memset(out, 0, sizeof(*out));
    if (!bank || prn < 1 || prn > 32 || !isfinite(now) || !isfinite(tx) ||
        now < bank->blockedUntil || now - bank->lastFrame > 4 || now - bank->maskTime > 600) {
        return 0;
    }
    for (unsigned k = 0; k < bank->count; ++k) {
        if (bank->prn[k] != prn || !bank->fast[k].valid || !bank->orbit[k].valid ||
            bank->orbit[k].iode != iode || now < bank->fast[k].time || now < bank->orbit[k].time ||
            now - bank->fast[k].time > 12 || now - bank->orbit[k].time > 120) {
            continue;
        }
        const double dt = bank->orbit[k].dynamic ? remainder(tx - bank->orbit[k].tod, 86400) : 0;
        if (fabs(dt) > 120) {
            return 0;
        }
        out->range = bank->fast[k].range;
        out->clock = bank->orbit[k].clock + dt * bank->orbit[k].drift;
        out->drift = bank->orbit[k].drift;
        for (unsigned axis = 0; axis < 3; ++axis) {
            out->velocity[axis] = bank->orbit[k].velocity[axis];
            out->position[axis] = bank->orbit[k].xyz[axis] + dt * out->velocity[axis];
        }
        return 1;
    }
    return 0;
}
