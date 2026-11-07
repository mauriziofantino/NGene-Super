/** @file format.c @brief Exact integer IQ representation helpers; no float conversions. */
#include "ngene/types.h"

/** @brief Describe the native IQ pair stride; zero denotes legacy CI8.
 * Parameter format Native sample representation.
 * Result: Two bytes for CI8/CU8, four for little-endian SC16 Q11, zero if unsupported.
 */
size_t NgSampleBytes(TNgSampleFormat format)
{
    return format == NG_FORMAT_SC16_Q11 || format == NG_FORMAT_CI16            ? 4u
           : format == 0 || format == NG_FORMAT_CI8 || format == NG_FORMAT_CU8 ? 2u
                                                                               : 0u;
}

/** @brief Return the stable name of a native sample representation.
 * Parameter format Native format; zero denotes CI8 compatibility.
 * Result: Borrowed static name, or "invalid".
 */
const char *NgSampleFormatName(TNgSampleFormat format)
{
    return format == NG_FORMAT_CI16                 ? "CI16"
           : format == NG_FORMAT_SC16_Q11           ? "SC16_Q11"
           : format == NG_FORMAT_CU8                ? "CU8"
           : format == 0 || format == NG_FORMAT_CI8 ? "CI8"
                                                    : "invalid";
}

/** @brief Read one signed ADC component without losing bladeRF precision.
 * @details CI8 reads are signed; CU8 subtracts 128; SC16 Q11 reads little-endian
 * signed 16-bit storage. Call only with a validated view and an in-range index.
 * Parameter view Borrowed native input.
 * Parameter index Complex-pair index smaller than meta.count.
 * Parameter quadrature Zero selects I, one selects Q.
 * Result: Signed ADC level, in native ADC units.
 */
int NgIqComponent(const TNgIqView *view, size_t index, unsigned quadrature)
{
    const unsigned char *bytes = (const unsigned char *)(const void *)view->samples;
    size_t component = index * 2u + quadrature;
    unsigned value;
    if (view->format == NG_FORMAT_SC16_Q11 || view->format == NG_FORMAT_CI16) {
        value = (unsigned)bytes[component * 2u] | ((unsigned)bytes[component * 2u + 1u] << 8);
        return value >= 32768u ? (int)value - 65536 : (int)value;
    }
    value = bytes[component];
    if (view->format == NG_FORMAT_CU8) {
        return (int)value - 128;
    }
    return value >= 128u ? (int)value - 256 : (int)value;
}

/** @brief Store a signed ADC component in native sample storage.
 * @details Value must lie within the representation's ADC range. SC16 Q11 is
 * written little-endian. No scaling or quantization is performed.
 * Parameter storage Writable native IQ pair array.
 * Parameter format Native representation.
 * Parameter index Complex-pair index within caller-owned capacity.
 * Parameter quadrature Zero selects I, one selects Q.
 * Parameter value Signed ADC level; CU8 is recentered by adding 128.
 */
void NgIqStore(void *storage, TNgSampleFormat format, size_t index, unsigned quadrature, int value)
{
    unsigned char *bytes = storage;
    size_t component = index * 2u + quadrature;
    if (format == NG_FORMAT_SC16_Q11 || format == NG_FORMAT_CI16) {
        uint16_t bits = (uint16_t)value;
        bytes[component * 2u] = (unsigned char)(bits & 255u);
        bytes[component * 2u + 1u] = (unsigned char)(bits >> 8);
    } else {
        bytes[component] = (unsigned char)(format == NG_FORMAT_CU8 ? value + 128 : value);
    }
}
