/** @file test_replay.c @brief CI8 replay ranges, leases, EOF and independent-source contracts. */
#include "ngene/frontend.h"
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define NG_TEST_PAIRS 17
#define CHECK(expression)                                                                          \
    do {                                                                                           \
        if ((expression) == 0) {                                                                   \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expression);                          \
            exit(EXIT_FAILURE);                                                                    \
        }                                                                                          \
    } while (0)

/** @brief Create a new signed, interleaved CI8 file containing an ordered test vector.
 * @details Owns the temporary stream only; the caller removes the fixture after all readers stop.
 * @param path New fixture name; existing files are never overwritten.
 */
static void WriteFixture(const char *path)
{
    FILE *output = NgFopenExclusive(path); /* Owned until successful close. */
    size_t pairIndex;                      /* Complex-pair index in the fixed test vector. */
    CHECK(output != NULL);
    for (pairIndex = 0; pairIndex < NG_TEST_PAIRS; ++pairIndex) {
        TNgIq8 pair = {(int8_t)((int)pairIndex - 8), (int8_t)(8 - (int)pairIndex)};
        CHECK(fwrite(&pair, sizeof(pair), 1, output) == 1);
    }
    CHECK(fclose(output) == 0);
}

/** @brief Exercise two simultaneously open replay descriptors without shared buffers or cursors.
 * @details The first descriptor uses a limited offset range; the second starts at the file origin.
 * @param path Existing immutable test vector.
 */
static void CheckIndependentSources(const char *path)
{
    TNgConfig config;       /* Validated source and window settings. */
    TNgFrontend first;      /* Owns a limited-range reader. */
    TNgFrontend second;     /* Owns a distinct full-range reader. */
    TNgIqView firstView;    /* Lease valid until release on first. */
    TNgIqView secondView;   /* Lease valid until release on second. */
    TNgFrontendStats stats; /* Copied counters; no sample ownership. */
    TNgError error = {{0}}; /* Optional diagnostic storage. */
    NgConfigDefaults(&config);
    CHECK(snprintf(config.replayPath, sizeof(config.replayPath), "%s", path) > 0);
    config.windowSamples = 4;
    config.replayRealtime = 0;
    config.replayStartSample = 3;
    config.replaySampleLimit = 7;
    CHECK(NgFrontendOpen(&config, &first, &error) == NG_OK);
    config.replayStartSample = 0;
    config.replaySampleLimit = 0;
    CHECK(NgFrontendOpen(&config, &second, &error) == NG_OK);
    CHECK(first.ops->start(first.context, &error) == NG_OK);
    CHECK(second.ops->start(second.context, &error) == NG_OK);
    CHECK(first.ops->acquire(first.context, &firstView, 0, &error) == NG_OK);
    CHECK(second.ops->acquire(second.context, &secondView, 0, &error) == NG_OK);
    CHECK(firstView.samples != secondView.samples);
    CHECK(firstView.samples[0].i == -5 && secondView.samples[0].i == -8);
    CHECK(firstView.samples[0].q == 5 && secondView.samples[0].q == 8);
    CHECK(first.ops->start(first.context, &error) == NG_ERROR);
    CHECK(first.ops->acquire(first.context, &firstView, 0, &error) == NG_ERROR);
    first.ops->release(first.context);
    CHECK(first.ops->acquire(first.context, &firstView, 0, &error) == NG_OK);
    CHECK(firstView.meta.count == 3 && firstView.meta.firstSample == 4);
    CHECK(firstView.samples[0].i == -1 && firstView.samples[2].i == 1);
    CHECK((firstView.meta.flags & NG_BLOCK_PARTIAL) != 0);
    CHECK(secondView.samples[0].i == -8); /* A second acquire cannot modify the other lease. */
    first.ops->release(first.context);
    CHECK(first.ops->acquire(first.context, &firstView, 0, &error) == NG_END);
    first.ops->stats(first.context, &stats);
    CHECK(stats.receivedSamples == 7 && stats.droppedSamples == 0);
    CHECK(first.ops->stop(first.context, &error) == NG_OK);
    NgFrontendDestroy(&first);
    second.ops->release(second.context);
    CHECK(second.ops->acquire(second.context, &secondView, 0, &error) == NG_OK);
    CHECK(secondView.meta.firstSample == 4 && secondView.samples[0].i == -4);
    second.ops->release(second.context);
    CHECK(second.ops->stop(second.context, &error) == NG_OK);
    NgFrontendDestroy(&second);
}

/** @brief Verify loop discontinuities, explicit metadata and rejected file/range formats.
 * @details A repeated range begins a new epoch and never fabricates continuous RF samples.
 * @param path Existing immutable test vector.
 */
static void CheckLoopAndValidation(const char *path)
{
    TNgConfig config;       /* Source configuration reused only while descriptors are stopped. */
    TNgFrontend frontend;   /* Owned loop reader. */
    TNgIqView view;         /* Current bounded lease. */
    TNgError error = {{0}}; /* Validation diagnostic. */
    FILE *output;           /* Owned stream for malformed-file fixtures. */
    char badPath[256];      /* Distinct malformed input filename. */
    NgConfigDefaults(&config);
    CHECK(snprintf(config.replayPath, sizeof(config.replayPath), "%s", path) > 0);
    config.windowSamples = 4;
    config.replayRealtime = 0;
    config.replayLoop = 1;
    config.replaySampleLimit = 3;
    config.centerFrequencyHz = UINT64_C(1576420000);
    config.signalFrequencyHz = UINT64_C(1575420000);
    CHECK(NgFrontendOpen(&config, &frontend, &error) == NG_OK);
    CHECK(frontend.info.digitalIfHz == -1000000);
    CHECK(frontend.ops->start(frontend.context, &error) == NG_OK);
    CHECK(frontend.ops->acquire(frontend.context, &view, 0, &error) == NG_OK);
    CHECK(view.meta.count == 3 && view.meta.epoch == 0);
    frontend.ops->release(frontend.context);
    CHECK(frontend.ops->acquire(frontend.context, &view, 0, &error) == NG_OK);
    CHECK(view.meta.epoch == 1 && view.meta.firstSample == 3);
    CHECK((view.meta.flags & NG_BLOCK_GAP) != 0 && view.meta.missingKnown == 0);
    frontend.ops->release(frontend.context);
    CHECK(frontend.ops->stop(frontend.context, &error) == NG_OK);
    NgFrontendDestroy(&frontend);
    config.replayStartSample = NG_TEST_PAIRS;
    CHECK(NgFrontendOpen(&config, &frontend, &error) == NG_ERROR);
    config.replayStartSample = 0;
    CHECK(snprintf(badPath, sizeof(badPath), "%s.bad", path) > 0);
    CHECK(snprintf(config.replayPath, sizeof(config.replayPath), "%s", badPath) > 0);
    output = NgFopenExclusive(badPath);
    CHECK(output != NULL && fclose(output) == 0);
    CHECK(NgFrontendOpen(&config, &frontend, &error) == NG_ERROR);
    output = fopen(badPath, "ab");
    CHECK(output != NULL && fputc(1, output) != EOF && fclose(output) == 0);
    CHECK(NgFrontendOpen(&config, &frontend, &error) == NG_ERROR);
    CHECK(remove(badPath) == 0);
    config.replayPath[0] = '\0';
    CHECK(NgFrontendOpen(&config, &frontend, &error) == NG_ERROR);
}

/** @brief Run file-source contract tests and release all temporary fixtures.
 * @details No SDR or GUI is required; process failure reports the first violated contract.
 * @return EXIT_SUCCESS after ownership, range, signedness and format checks pass.
 */
int main(void)
{
    char path[192]; /* Process-unique fixture created exclusively. */
    CHECK(snprintf(path, sizeof(path), "ngene_replay_%llu.cs8",
                   (unsigned long long)(NgMonotonicSeconds() * 1000000.0)) > 0);
    WriteFixture(path);
    CheckIndependentSources(path);
    CheckLoopAndValidation(path);
    CHECK(remove(path) == 0);
    puts("PASS: independent replay buffers/ranges, leases, EOF, loop gaps and signed CI8");
    return EXIT_SUCCESS;
}
