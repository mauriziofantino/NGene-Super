/** @file hackrf_adapter.h @brief Native vendor adapter boundary; one owner per device. */
#ifndef NGENE_HACKRF_ADAPTER_H
#define NGENE_HACKRF_ADAPTER_H
#include "ngene/frontend.h"
/**
 * @brief Create and configure the native HackRF source adapter.
 *
 * @details
 * Acquires vendor-library/device resources, applies hardware sample rate/tuning/gains and
 * allocates ingress storage. Reception begins only through the frontend start operation.
 * Caller destroys after stop.
 *
 * @param config Validated HackRF configuration.
 * @param frontend Receives owned frontend descriptor on success.
 * @param error Optional device/configuration error destination.
 *
 * @return NG_OK on success; NG_ERROR for library, USB, configuration or allocation errors.
 */
int NgHackrfOpen(const TNgConfig *config, TNgFrontend *frontend, TNgError *error);
/**
 * @brief Enumerate devices with the compiled libhackrf API.
 *
 * @details
 * Internal implementation of NgFrontendListHackrf; owns temporary vendor enumeration resources
 * only.
 *
 * @param output Writable output stream retained by caller.
 * @param error Optional vendor error destination.
 *
 * @return NG_OK on success; NG_ERROR for initialization/enumeration failures.
 */
int NgHackrfList(FILE *output, TNgError *error);
/** @brief Probe the selected stopped device without RX or hardware configuration.
 * @param config Device serial, or exactly one attached device when empty.
 * @param caps Receives owned identity and verified gain policy.
 * @param error Optional native error destination.
 * @return NG_OK after a successful identity query, otherwise NG_ERROR; resources released.
 */
int NgHackrfCapabilities(const TNgConfig *config, TNgFrontendCaps *caps, TNgError *error);
#endif
