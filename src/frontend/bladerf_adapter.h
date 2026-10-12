/** @file bladerf_adapter.h @brief Private native bladeRF adapter boundary. */
#ifndef NGENE_BLADERF_ADAPTER_H
#define NGENE_BLADERF_ADAPTER_H
#include "ngene/frontend.h"
/** @brief Configure one RX channel using native libbladeRF and SC16 Q11 metadata.
 * @param config Validated device settings, queue dimensions and overflow policy.
 * @param frontend Receives owned source on success.
 * @param error Optional diagnostic.
 * @return NG_OK on success, NG_ERROR after releasing partial resources on failure.
 */
int NgBladerfOpen(const TNgConfig *config, TNgFrontend *frontend, TNgError *error);
/** @brief Enumerate native bladeRF devices without starting RX.
 * @param output Writable caller-owned stream.
 * @param error Optional diagnostic.
 * @return NG_OK on enumeration (including no devices), NG_ERROR on native failure.
 */
int NgBladerfList(FILE *output, TNgError *error);
/** @brief Copy frequency-dependent native gain/rate/tuning ranges while stopped.
 * @param config Device serial and desired tuning.
 * @param caps Receives copied bounds, no borrowed vendor pointers.
 * @param error Optional diagnostic.
 * @return NG_OK on success, NG_ERROR on device/query failure.
 */
int NgBladerfCapabilities(const TNgConfig *config, TNgFrontendCaps *caps, TNgError *error);
#endif
