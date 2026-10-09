/** @file main.c
 * @brief NGene command-line and embedded console entry point.
 */
#include "ngene/receiver.h"
#include "ngene/build.h"
#include "ngene/dsp/fft.h"
#ifdef NGENE_HAVE_WEB
#include "ngene/web.h"
#endif
#include <stdlib.h>
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
/** Signal-handler stop flag; accessed only as a volatile sig_atomic_t. */
static volatile sig_atomic_t interrupted = 0;

/**
 * @brief Latch a stop request from SIGINT or SIGTERM.
 *
 * @details
 * Signal handler only stores a sig_atomic_t flag; no allocation, I/O or locking.
 *
 * @param signum Signal number, intentionally unused.
 */
static void OnSignal(int signum)
{
    (void)signum;
    interrupted = 1;
}

/**
 * @brief Expose the signal flag through the engine stop predicate.
 *
 * @details
 * Called by receiver/service loops; no locks or side effects.
 *
 * @param context Unused predicate context.
 *
 * @return Nonzero when a signal requested shutdown.
 */
static int ShouldStop(void *context)
{
    (void)context;
    return interrupted != 0;
}

/**
 * @brief Select CLI/web mode, configure the receiver and run until completion.
 *
 * @details
 * Handles informational/device-list commands, INI loading, signal hooks and cleanup. Web mode
 * serves the embedded console; CLI mode captures/records configured samples. Does not
 * implement satellite acquisition/tracking.
 *
 * @param argc Command-line argument count.
 * @param argv NUL-terminated argument vector valid for process lifetime.
 *
 * @return Zero on success; 1 on runtime/startup failure; 2 on invalid command-line arguments.
 */
int main(int argc, char **argv)
{
    const char *path = "config/hackrf.ini";
    TNgError error = {{0}};
    TNgConfig config;
    TNgFrontend frontend;
    TNgReceiverStats stats;
    TNgConditioner conditioner = NgConditionerBypass();
    int result;
    int web = 0;
    int k;
    unsigned port = 8080;
    const char *outputDirectory = "captures";
    int explicitConfig = 0;
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        puts("NGene: [--web] [--config FILE] [--port 8080] [--output-dir captures]\n       "
             "--list-devices | --version\nC receiver and local web console; no acquisition, "
             "tracking or PVT yet.");
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("NGene %s %s/%s native-HackRF=%s; FFT=%s\n", NGENE_VERSION, NGENE_OS, NGENE_ARCH,
               (NgHackrfAvailable()) != 0 ? "yes" : "no", NgFftBackend());
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--list-devices") == 0) {
        result = NgFrontendListHackrf(stdout, &error);
        if (result != NG_OK) {
            fprintf(stderr, "%s\n", error.message);
        }
        return result == NG_OK ? 0 : 1;
    }
    for (k = 1; k < argc; ++k) {
        if (strcmp(argv[k], "--web") == 0) {
            web = 1;
        } else if (strcmp(argv[k], "--config") == 0 && k + 1 < argc) {
            path = argv[++k];
            explicitConfig = 1;
        } else if (strcmp(argv[k], "--output-dir") == 0 && k + 1 < argc) {
            outputDirectory = argv[++k];
        } else if (strcmp(argv[k], "--port") == 0 && k + 1 < argc) {
            char *end;
            unsigned long value;
            errno = 0;
            value = strtoul(argv[++k], &end, 10);
            if (errno != 0 || *end != 0 || value < 1024 || value > 65535) {
                fputs("Invalid port\n", stderr);
                return 2;
            }
            port = (unsigned)value;
        } else {
            fputs("Usage: ngene [--web] [--config FILE] [--port N] [--output-dir DIR]\n", stderr);
            return 2;
        }
    }
#ifndef NGENE_HAVE_WEB
    (void)port;
    (void)outputDirectory;
    (void)explicitConfig;
#endif
    (void)signal(SIGINT, OnSignal);
    (void)signal(SIGTERM, OnSignal);
    if (web != 0) {
#ifdef NGENE_HAVE_WEB
        if (explicitConfig != 0) {
            if (NgConfigLoad(path, &config, &error) != NG_OK) {
                fprintf(stderr, "%s\n", error.message);
                return 1;
            }
        } else {
            NgConfigDefaults(&config);
            config.frontend = (NgHackrfAvailable()) != 0 ? NG_FRONTEND_HACKRF : NG_FRONTEND_FILE;
            config.runSeconds = 0;
            /* Reload the profile saved by the GUI, if it already exists. */
            char profile[1024];
            FILE *saved;
            if (snprintf(profile, sizeof(profile), "%s/receiver.ini", outputDirectory) >=
                (int)sizeof(profile)) {
                return 2;
            }
            saved = fopen(profile, "rb");
            if (saved != NULL) {
                result = NgConfigRead(saved, &config, &error);
                fclose(saved);
                if (result != NG_OK) {
                    fprintf(stderr, "%s\n", error.message);
                    return 1;
                }
            }
        }
        result = NgWebRun(&config, port, outputDirectory, ShouldStop, NULL, &error);
        if (result != NG_OK) {
            fprintf(stderr, "%s\n", error.message);
        }
        return result == NG_OK ? 0 : 1;
#else
        fputs("Web support disabled; build with -DNGENE_WITH_WEB=ON\n", stderr);
        return 1;
#endif
    }
    if (NgConfigLoad(path, &config, &error) != NG_OK ||
        NgFrontendOpen(&config, &frontend, &error) != NG_OK) {
        fprintf(stderr, "%s\n", error.message);
        return 1;
    }
    (void)signal(SIGINT, OnSignal);
    (void)signal(SIGTERM, OnSignal);
    printf("NGene %s %s/%s\nsource=%s format=CI8 fs=%u pairs/s tune=%" PRIu64
           " Hz filter=%u Hz window=%zu pairs\n",
           NGENE_VERSION, NGENE_OS, NGENE_ARCH, frontend.info.sourceName,
           frontend.info.sampleRateHz, frontend.info.centerFrequencyHz, frontend.info.bandwidthHz,
           config.windowSamples);
    puts("Capture core active; GNSS algorithms are not yet attached.");
    if (config.dcRemoval != 0 && NgConditionerDcCreate(config.windowSamples, config.dcShift,
                                                       &conditioner, &error) != NG_OK) {
        fprintf(stderr, "%s\n", error.message);
        NgFrontendDestroy(&frontend);
        return 1;
    }
    printf("signal=%" PRIu64 " Hz digital_IF=%" PRId64 " Hz conditioning=%s\n",
           frontend.info.signalFrequencyHz, frontend.info.digitalIfHz, conditioner.name);
    result =
        NgReceiverRun(&config, &frontend, &conditioner, NULL, 0, ShouldStop, NULL, &stats, &error);
    printf("windows=%" PRIu64 " processed_pairs=%" PRIu64 " written_pairs=%" PRIu64 " gaps=%" PRIu64
           " known_input_drops=%" PRIu64 " queued_discarded_at_stop=%" PRIu64
           " wall_seconds=%.6f\n",
           stats.windows, stats.processedSamples, stats.recording.samples, stats.gapEvents,
           stats.frontend.droppedSamples, stats.frontend.discardedOnStop, stats.wallSeconds);
    NgConditionerDcDestroy(&conditioner);
    NgFrontendDestroy(&frontend);
    if (result != NG_OK) {
        fprintf(stderr, "%s\n", error.message);
        return 1;
    }
    return 0;
}
