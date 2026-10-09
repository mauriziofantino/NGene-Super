/** @file web.h @brief Optional local web application for the NGene executable. */
#ifndef NGENE_WEB_H
#define NGENE_WEB_H
#include "ngene/receiver.h"
/**
 * @brief Serve the embedded console and controller on loopback until shutdown.
 *
 * @details
 * Binds to local interfaces only. REST mutations require the session token and same-origin
 * checks; WebSocket status copies precede serialization. Owns the controller and server for
 * this blocking call, then stops/joins them. Remote use requires the documented forwarding
 * arrangement.
 *
 * @param config Initial receiver configuration.
 * @param port Local TCP port 1024..65535.
 * @param outputDirectory Receiver profile/output directory; parents must exist.
 * @param stop Optional stop predicate, also polled by the service.
 * @param stopContext Context passed to stop.
 * @param error Optional service/startup error destination.
 *
 * @return NG_OK on clean shutdown; NG_ERROR on configuration, bind, thread or startup
 * failures.
 */
int NgWebRun(const TNgConfig *config, unsigned port, const char *outputDirectory,
             TNgStopRequested stop, void *stopContext, TNgError *error);
#endif
