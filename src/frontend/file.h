/** @file file.h @brief Independent recorded-CI8 source lifecycle. */
#ifndef NGENE_FRONTEND_FILE_H
#define NGENE_FRONTEND_FILE_H
#include "ngene/frontend.h"
/**
 * @brief Open a recorded signed interleaved I/Q file without starting playback.
 * @details Each instance owns a file handle, window, cursor and pacing clock. It can run
 * concurrently with a live frontend through a different receiver engine. Sample rate and
 * RF references come from config; raw CI8 files contain no calibration metadata.
 * @param config Validated file-source configuration.
 * @param frontend Receives the owned adapter on success; destroy after stop.
 * @param error Optional file/format/allocation diagnostic.
 * @return NG_OK on success; NG_ERROR for an empty/odd/unreadable file or invalid range.
 */
int NgFileOpen(const TNgConfig *config, TNgFrontend *frontend, TNgError *error);
#endif
