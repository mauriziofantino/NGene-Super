/** @file stream_info.c @brief Frequency metadata conversions without sample mixing. */
#include "ngene/types.h"

/**
 * @brief Convert physical Doppler into a receiver carrier-NCO frequency.
 *
 * @details
 * Uses carrier = digital IF + Doppler. This metadata calculation performs no sample mixing and
 * uses the stream convention signal minus hardware tuning.
 *
 * Parameter info: Stream metadata with digitalIfHz initialized.
 * Parameter dopplerHz: Physical satellite Doppler estimate in hertz.
 *
 * Result: Carrier frequency in the hardware-baseband reference, in hertz.
 */
double NgCarrierFromDoppler(const TNgStreamInfo *info, double dopplerHz)
{
    return (double)info->digitalIfHz + dopplerHz;
}

/**
 * @brief Remove digital IF from an estimated carrier frequency.
 *
 * @details
 * Use this conversion before passing a physical Doppler observable to navigation/PVT. It
 * performs no sample processing.
 *
 * Parameter info: Stream metadata with digitalIfHz initialized.
 * Parameter carrierHz: Estimated baseband carrier-NCO frequency in hertz.
 *
 * Result: Physical Doppler in hertz: carrier minus digital IF.
 */
double NgDopplerFromCarrier(const TNgStreamInfo *info, double carrierHz)
{
    return carrierHz - (double)info->digitalIfHz;
}
