/** @file rtlsdr_adapter.h @brief Private native RTL-SDR adapter boundary. */
#ifndef NGENE_RTLSDR_ADAPTER_H
#define NGENE_RTLSDR_ADAPTER_H
#include "ngene/frontend.h"
/** @brief Open native RTL-SDR; reception starts through the shared frontend operations.
 * @param config Validated RTL-SDR settings.
 * @param frontend Receives owned source on success.
 * @param error Optional diagnostic.
 * @return NG_OK on success, NG_ERROR after cleanup on failure.
 */
int NgRtlsdrOpen(const TNgConfig *config, TNgFrontend *frontend, TNgError *error);
/** @brief Enumerate native USB devices without starting RX.
 * @param output Caller-owned output stream.
 * @param error Optional diagnostic.
 * @return NG_OK on enumeration, NG_ERROR on invalid output.
 */
int NgRtlsdrList(FILE *output, TNgError *error);
/** @brief Read the selected tuner's discrete native gains and limits.
 * @param config Device serial and source settings.
 * @param caps Copied capability result.
 * @param error Optional diagnostic.
 * @return NG_OK on success, NG_ERROR on device/query failure.
 */
int NgRtlsdrCapabilities(const TNgConfig *config, TNgFrontendCaps *caps, TNgError *error);
#endif
