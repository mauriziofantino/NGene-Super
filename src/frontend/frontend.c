/** @file frontend.c
 * @brief NGene frontend implementation.
 */
#include "ngene/frontend.h"
#include <string.h>
#include "frontend/file.h"
#ifdef NGENE_HAVE_HACKRF
#include "frontend/hackrf_adapter.h"
#endif
/**
 * @brief Open and configure a native frontend without starting reception.
 *
 * @details
 * Selects HackRF or the deterministic file replay source from config. Each descriptor owns one
 * frontend; independent receivers may run concurrently. The receiver controls start/stop; the
 * caller destroys the instance after use.
 *
 * Parameter config: Validated receiver configuration.
 * Parameter frontend: Receives owned frontend context/operations/stream description.
 * Parameter error: Optional error destination.
 *
 * Result: NG_OK on success; NG_ERROR for configuration, device or allocation errors.
 */
int NgFrontendOpen(const TNgConfig *config, TNgFrontend *frontend, TNgError *error)
{
    if (frontend == NULL || NgConfigValidate(config, error) != NG_OK) {
        return NG_ERROR;
    }
    memset(frontend, 0, sizeof(*frontend));
    if (config->frontend == NG_FRONTEND_FILE) {
        return NgFileOpen(config, frontend, error);
    }
#ifdef NGENE_HAVE_HACKRF
    return NgHackrfOpen(config, frontend, error);
#else
    return NgFail(error, "HackRF support is disabled; rebuild with -DNGENE_WITH_HACKRF=ON");
#endif
}

/**
 * @brief Destroy an opened frontend after reception stops.
 *
 * @details
 * Calls the native implementation destructor and clears the public descriptor. Do not access
 * borrowed views or stream strings afterwards; NULL is accepted.
 *
 * Parameter frontend: Frontend whose transport has been stopped.
 */
void NgFrontendDestroy(TNgFrontend *frontend)
{
    if (frontend != NULL && frontend->ops != NULL) {
        frontend->ops->destroy(frontend->context);
        memset(frontend, 0, sizeof(*frontend));
    }
}

/**
 * @brief Report whether the native HackRF adapter is compiled in.
 *
 * @details
 * This does not enumerate USB devices or establish whether a device is connected.
 *
 * Result: Nonzero if compiled with libhackrf, otherwise zero.
 */
int NgHackrfAvailable(void)
{
#ifdef NGENE_HAVE_HACKRF
    return 1;
#else
    return 0;
#endif
}

/**
 * @brief Enumerate native HackRF devices to a text stream.
 *
 * @details
 * Does not start sample reception. Initializes and cleans up the vendor library for
 * enumeration; do not run concurrently with an active frontend instance.
 *
 * Parameter output: Writable output stream retained by caller.
 * Parameter error: Optional error destination.
 *
 * Result: NG_OK on successful enumeration; NG_ERROR if unavailable or the vendor API fails.
 */
int NgFrontendListHackrf(FILE *output, TNgError *error)
{
#ifdef NGENE_HAVE_HACKRF
    return NgHackrfList(output, error);
#else
    (void)output;
    return NgFail(error, "HackRF support is disabled");
#endif
}
