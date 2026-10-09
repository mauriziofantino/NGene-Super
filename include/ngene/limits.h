/** @file limits.h @brief Common units, dimensional limits and default receiver profile. */
#ifndef NGENE_LIMITS_H
#define NGENE_LIMITS_H
#include <stdint.h>
/** Largest complex FFT supported by the architecture-specific kernels. */
#define NG_FFT_MAX_SIZE 16384
/** Smallest complex FFT accepted by the low-level integer kernel. */
#define NG_FFT_MIN_SIZE 8
/** Smallest complex FFT offered by spectrum diagnostics. */
#define NG_SPECTRUM_MIN_FFT_SIZE 256
/** Largest supported Welch segment count. */
#define NG_SPECTRUM_MAX_AVERAGES 64
/** Largest temporal PSD history in linear power. */
#define NG_SPECTRUM_MAX_TEMPORAL_FRAMES 16
/** Largest segment overlap; a positive stride must always remain. */
#define NG_SPECTRUM_MAX_OVERLAP_PERCENT 99
/** Minimum optional diagnostic update frequency in hertz. */
#define NG_DIAGNOSTIC_MIN_UPDATE_HZ 0.1
/** Maximum optional diagnostic update frequency in hertz. */
#define NG_DIAGNOSTIC_MAX_UPDATE_HZ 30.0
/** Smallest histogram observation span in complex pairs. */
#define NG_HISTOGRAM_MIN_PAIRS 1024
/** Smallest supported signed-int8 histogram subdivision. */
#define NG_HISTOGRAM_MIN_BINS 16
/** Default complex sample rate, matching the original NGene 1 ms processing grid. */
#define NG_DEFAULT_SAMPLE_RATE_HZ UINT32_C(16000000)
/** Nominal GPS L1 / Galileo E1 carrier in hertz. */
#define NG_GNSS_L1_FREQUENCY_HZ UINT64_C(1575420000)
/** Default complex-pair count per shared engine window. */
#define NG_DEFAULT_WINDOW_SAMPLES 16000
/** Default copied frontend ingress capacity in windows. */
#define NG_DEFAULT_INGRESS_BLOCKS 128
/** Default shared engine-ring capacity in windows. */
#define NG_DEFAULT_RING_BLOCKS 16
/** Default asynchronous recording queue capacity in windows. */
#define NG_DEFAULT_RECORDING_BLOCKS 128
/** Default continuous DC-estimator alpha exponent. */
#define NG_DEFAULT_DC_SHIFT 16
/** Default HackRF LNA gain in decibels, on an 8 dB device step. */
#define NG_DEFAULT_LNA_GAIN_DB 16
/** Default HackRF VGA gain in decibels, on a 2 dB device step. */
#define NG_DEFAULT_VGA_GAIN_DB 20
/** Default frontend inactivity timeout in milliseconds. */
#define NG_DEFAULT_IDLE_TIMEOUT_MS 3000
/** Default diagnostic complex FFT length. */
#define NG_DEFAULT_SPECTRUM_FFT_SIZE 2048
/** Default Welch overlap percentage. */
#define NG_DEFAULT_OVERLAP_PERCENT 50
/** Default number of Welch segments per PSD observation. */
#define NG_DEFAULT_WELCH_SEGMENTS 4
/** Default maximum optional diagnostic cadence in hertz. */
#define NG_DEFAULT_DIAGNOSTIC_UPDATE_HZ 4.0
/** Default temporal mean depth in PSD observations. */
#define NG_DEFAULT_TEMPORAL_FRAMES 4
/** Default weight of the newest linear-power frame in EMA mode. */
#define NG_DEFAULT_SMOOTHING_ALPHA 0.2
/** Default histogram span in complex pairs. */
#define NG_DEFAULT_HISTOGRAM_PAIRS 65536
/** Default signed-int8 histogram bin count. */
#define NG_DEFAULT_HISTOGRAM_BINS 64
/** Maximum supported frontend complex sample rate in pairs per second. */
#define NG_FRONTEND_MAX_SAMPLE_RATE_HZ UINT32_C(20000000)
/** Minimum supported live HackRF complex sample rate in pairs per second. */
#define NG_HACKRF_MIN_SAMPLE_RATE_HZ UINT32_C(2000000)
/** Maximum HackRF hardware tuning frequency in hertz. */
#define NG_HACKRF_MAX_FREQUENCY_HZ UINT64_C(6000000000)
/** Milliseconds per second, used only for pacing/timeouts. */
#define NG_MILLISECONDS_PER_SECOND 1000.0
#endif
