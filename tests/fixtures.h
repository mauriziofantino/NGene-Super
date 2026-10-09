/** @file fixtures.h @brief Test-only recorded CI8 inputs; not a frontend implementation. */
#ifndef NGENE_TEST_FIXTURES_H
#define NGENE_TEST_FIXTURES_H
#include "ngene/config.h"
/** @brief Write a deterministic complex tone to a caller-selected fixture file.
 * @details Used only by tests of playback and statistics. Does not ship in the receiver.
 * @param path New fixture path; caller removes it after all readers close.
 * @param samples Number of complete complex pairs.
 * @param rate Complex pairs per second.
 * @return NG_OK if all bytes and close succeed; NG_ERROR otherwise.
 */
int NgTestWriteTone(const char *path, uint64_t samples, uint32_t rate);
#endif
