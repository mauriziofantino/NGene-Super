/** @file fixtures.c @brief Generate immutable recorded test vectors outside receiver code. */
#include "fixtures.h"
#include <math.h>

/** @brief Write the bounded, deterministic CI8 tone fixture.
 * @details Owns only the temporary FILE handle; no device, stream or runtime producer.
 * Parameter path: New fixture path.
 * Parameter samples: Number of complex pairs.
 * Parameter rate: Complex sample rate.
 * Result: NG_OK if complete, otherwise NG_ERROR.
 */
int NgTestWriteTone(const char *path, uint64_t samples, uint32_t rate)
{
    FILE *output = fopen(path, "wbx");
    uint64_t sample;
    int result = NG_OK;
    if (output == NULL || rate == 0) {
        if (output != NULL) {
            (void)fclose(output);
        }
        return NG_ERROR;
    }
    for (sample = 0; sample < samples; ++sample) {
        double phase = 6.283185307179586476925286766559 * 10000.0 * (double)sample / rate;
        TNgIq8 pair = {(int8_t)lround(32.0 * cos(phase)), (int8_t)lround(32.0 * sin(phase))};
        if (fwrite(&pair, sizeof(pair), 1, output) != 1) {
            result = NG_ERROR;
            break;
        }
    }
    if (fclose(output) != 0) {
        result = NG_ERROR;
    }
    return result;
}
