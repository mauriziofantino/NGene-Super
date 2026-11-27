/** @file sbas.h @brief EGNOS L1 message and differential correction contracts. */
#ifndef NG_SBAS_H
#define NG_SBAS_H
#include <stdint.h>

#define NG_SBAS_FRAME_BYTES 32u
#define NG_SBAS_MASK_SIZE 51u

/** One GEO's correction bank, initialized to zero on a source or continuity change. */
typedef struct {
    unsigned count, iodp;
    unsigned prn[NG_SBAS_MASK_SIZE];
    double maskTime, lastFrame, blockedUntil;
    unsigned frameCount;
    struct {
        double range, time;
        unsigned iodf, valid;
    } fast[NG_SBAS_MASK_SIZE];
    struct {
        double xyz[3], velocity[3], clock, drift, tod, time;
        unsigned iode, valid, dynamic;
    } orbit[NG_SBAS_MASK_SIZE];
} TNgSbasCorrections;

/** Zero-filled copy of applicable GPS L1 corrections in metres, m/s and seconds. */
typedef struct {
    double position[3], velocity[3], clock, drift, range;
} TNgSbasCorrection;

/** Accept one packed MSB-first 250-bit frame with CRC, at monotonic signal time in seconds. */
int NgSbasPushFrame(TNgSbasCorrections *bank, const uint8_t data[NG_SBAS_FRAME_BYTES],
                    double signalTime);
/** Return zero when integrity, age, PRN or GPS broadcast issue fails applicability. */
int NgSbasRead(const TNgSbasCorrections *bank, unsigned prn, unsigned iode, double signalTime,
               double gpsTransmitTow, TNgSbasCorrection *correction);
#endif
