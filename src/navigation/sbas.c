/** @file sbas.c @brief EGNOS L1 mask, fast/range and long-term orbit/clock messages. */
#include "ngene/sbas.h"
#include <math.h>
#include <string.h>
#include "navigation/sbas_igp.h"

/** Extract an MSB-first field from the packed frame. */
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
                 unsigned iodp, unsigned iodf, double now)
{
    if (bank->iodp != iodp) {
        return;
    }
    const unsigned base = count == 6 ? 14u : 18u;
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
            bank->orbit[i].velocity[axis] =
                dynamic ? ldexp(Signed(data, base + 58 + 8 * axis, 8), -11) : 0;
        }
        bank->orbit[i].clock = ldexp(Signed(data, base + 14 + 3 * width, dynamic ? 11u : 10u), -31);
        bank->orbit[i].drift = dynamic ? ldexp(Signed(data, base + 82, 8), -39) : 0;
        bank->orbit[i].tod = dynamic ? 16.0 * Bits(data, base + 90, 13) : 0;
        bank->orbit[i].time = now;
        bank->orbit[i].valid = 1;
        bank->orbit[i].dynamic = dynamic;
    }
}

/** Update the indicated band's mask or the corresponding block of 15 delays. */
static void IonoMessage(TNgSbasCorrections *bank, const uint8_t *data, unsigned type, double now)
{
    if (type == 18 && !Bits(data, 14, 4)) {
        memset(bank->ionosphere, 0, sizeof(bank->ionosphere));
        return;
    }
    const unsigned band = Bits(data, type == 18 ? 18u : 14u, 4);
    if (band >= 9) {
        return;
    }
    TNgSbasIonoBand *grid = &bank->ionosphere[band];
    if (type == 18) {
        uint8_t mask[201] = {0};
        unsigned count = 0;
        for (unsigned i = 0; i < (band == 8 ? 200u : 201u); ++i) {
            if (Bits(data, 24 + i, 1)) {
                mask[count++] = (uint8_t)i;
            }
        }
        const unsigned issue = Bits(data, 22, 2);
        if (grid->issue != issue || grid->count != count ||
            memcmp(grid->mask, mask, sizeof(mask))) {
            memset(grid, 0, sizeof(*grid));
        }
        memcpy(grid->mask, mask, sizeof(mask));
        grid->issue = issue;
        grid->count = count;
        grid->maskTime = now;
    } else if (Bits(data, 217, 2) == grid->issue && now - grid->maskTime <= 600) {
        const unsigned start = 15 * Bits(data, 18, 4);
        for (unsigned i = 0; i < 15 && start + i < grid->count; ++i) {
            const unsigned delay = Bits(data, 22 + 13 * i, 9);
            const unsigned give = Bits(data, 31 + 13 * i, 4);
            grid->delay[start + i] = (uint16_t)(delay == 511 ? UINT16_MAX
                                                : give == 15 ? 0
                                                             : delay + 1);
            grid->time[start + i] = now;
        }
    }
}

/** Validate CRC and chronological order before changing the correction bank. */
int NgSbasPushFrame(TNgSbasCorrections *bank, const uint8_t data[NG_SBAS_FRAME_BYTES], double now)
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
        memset(bank->ionosphere, 0, sizeof(bank->ionosphere));
        memset(bank->fast, 0, sizeof(bank->fast));
        memset(bank->orbit, 0, sizeof(bank->orbit));
        bank->blockedUntil = now + 60;
    } else if (now >= bank->blockedUntil) {
        if (type == 1) {
            Mask(bank, data, now);
        } else if (type >= 2 && type <= 5) {
            Fast(bank, data, type - 2, 13, Bits(data, 16, 2), Bits(data, 14, 2), now);
        } else if (type == 6) {
            for (unsigned k = 0; k < bank->count; ++k) {
                unsigned issue = Bits(data, 14 + 2 * (k / 13), 2);
                if ((issue == 3 || issue == bank->fast[k].iodf) &&
                    Bits(data, 22 + 4 * k, 4) >= 14) {
                    bank->fast[k].valid = 0;
                }
            }
        } else if (type == 18 || type == 26) {
            IonoMessage(bank, data, type, now);
        } else if (type == 24) {
            Fast(bank, data, Bits(data, 112, 2), 6, Bits(data, 110, 2), Bits(data, 114, 2), now);
            LongTerm(bank, data, 120, now);
        } else if (type == 25) {
            LongTerm(bank, data, 14, now);
            LongTerm(bank, data, 120, now);
        }
    }
    return 1;
}

/** Require matching broadcast IODE and both valid differential corrections. */
int NgSbasRead(const TNgSbasCorrections *bank, unsigned prn, unsigned iode, double now, double tx,
               TNgSbasCorrection *out)
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

/** Interpolate a rectangle or a triangle that contains the pierce point. */
static int GridCell(const TNgSbasCorrections *bank, double now, double x, double y, int west,
                    int south, int width, int height, double *delay)
{
    if (south < 0 || south + height > 30) {
        return 0;
    }
    const unsigned w = (unsigned)(west + 72) % 72;
    const unsigned e = (unsigned)(west + width + 72) % 72;
    const unsigned lower = (unsigned)south, upper = (unsigned)(south + height);
    const unsigned corners[4] = {1 + 31 * w + lower, 1 + 31 * w + upper, 1 + 31 * e + lower,
                                 1 + 31 * e + upper};
    unsigned found = 0, issue = 0;
    double values[4] = {0};
    for (unsigned b = 0; b < 9 && found != 15; ++b) {
        const TNgSbasIonoBand *grid = &bank->ionosphere[b];
        if (now - grid->maskTime > 600 || (found && grid->issue != issue)) {
            continue;
        }
        for (unsigned i = 0; i < grid->count && found != 15; ++i) {
            if (!grid->delay[i] || now < grid->time[i] || now - grid->time[i] > 600) {
                continue;
            }
            const unsigned point = gridPoint[b][grid->mask[i]];
            for (unsigned k = 0; k < 4; ++k) {
                if (point == corners[k] && !(found & (1u << k))) {
                    if (grid->delay[i] == UINT16_MAX) {
                        return 0;
                    }
                    issue = grid->issue;
                    values[k] = (grid->delay[i] - 1) * 0.125;
                    found |= 1u << k;
                }
            }
        }
    }
    const double dx = (x - west) / width, dy = (y - south) / height;
    double weights[4] = {(1 - dx) * (1 - dy), (1 - dx) * dy, dx * (1 - dy), dx * dy};
    switch (found) {
    case 15:
        break;
    case 14:
        weights[1] = 1 - dx;
        weights[2] = 1 - dy;
        weights[3] = dx + dy - 1;
        break;
    case 13:
        weights[0] = 1 - dx;
        weights[2] = dx - dy;
        weights[3] = dy;
        break;
    case 11:
        weights[0] = 1 - dy;
        weights[1] = dy - dx;
        weights[3] = dx;
        break;
    case 7:
        weights[0] = 1 - dx - dy;
        weights[1] = dy;
        weights[2] = dx;
        break;
    default:
        return 0;
    }
    double value = 0;
    for (unsigned k = 0; k < 4; ++k) {
        if (found & (1u << k)) {
            if (weights[k] < 0) {
                return 0;
            }
            value += weights[k] * values[k];
        }
    }
    *delay = value;
    return found == 15 ? 2 : 1;
}

/** Project the line of sight onto the 350-km shell and interpolate eligible grid points. */
int NgSbasIonosphere(const TNgSbasCorrections *bank, double now, double latitude, double longitude,
                     double azimuth, double elevation, double *delay)
{
    const double degree = 0.017453292519943295;
    if (!bank || !delay || !(elevation > 0) || !isfinite(latitude) || !isfinite(longitude) ||
        !isfinite(azimuth) || !isfinite(now) || now < bank->blockedUntil || now < bank->lastFrame ||
        now - bank->lastFrame > 4) {
        return 0;
    }
    const double ratio = 6378.1363 / (6378.1363 + 350);
    const double zenith = ratio * cos(elevation);
    const double psi = 90 * degree - elevation - asin(zenith);
    const double phi =
        asin(fmax(-1, fmin(1, sin(latitude) * cos(psi) + cos(latitude) * sin(psi) * cos(azimuth))));
    const double lambda =
        longitude + atan2(sin(psi) * sin(azimuth),
                          cos(latitude) * cos(psi) - sin(latitude) * sin(psi) * cos(azimuth));
    const double y = (phi / degree + 75) / 5;
    if (y < 0 || y >= 30) {
        return 0;
    }
    const double x = (remainder(lambda / degree, 360) + 180) / 5;
    const int firstWidth = fabs(phi / degree) <= 60 ? 1 : 2;
    for (int height = 1; height <= 2; ++height) {
        const int width = height == 1 ? firstWidth : 2;
        int best = 0;
        double value = 0;
        for (int row = 0; row < height && best != 2; ++row) {
            for (int col = 0; col < width && best != 2; ++col) {
                double cell;
                const int kind =
                    GridCell(bank, now, x, y, (int)x - col, (int)y - row, width, height, &cell);
                if (kind > best) {
                    best = kind;
                    value = cell;
                }
            }
        }
        if (best) {
            *delay = value / sqrt(1 - zenith * zenith);
            return 1;
        }
    }
    return 0;
}
