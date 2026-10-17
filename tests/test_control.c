/** @file test_control.c @brief Lifecycle, persistence and independent recording contracts. */
#include "ngene/control.h"
#include "fixtures.h"
#include "platform/platform.h"
#include "platform/files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define NG_TEST_REMOVE_DIRECTORY _rmdir
#else
#include <unistd.h>
#define NG_TEST_REMOVE_DIRECTORY rmdir
#endif
#define CHECK(e)                                                                                   \
    do {                                                                                           \
        if (!(e)) {                                                                                \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #e);                                   \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

/**
 * @brief Verify wait state behaviour.
 *
 * @details
 * Test-only helper exercising the named contract with deterministic inputs. Caller owns
 * fixture/context storage; assertion failures abort the test.
 *
 * @param c Caller-owned test fixture or output storage used by this helper.
 * @param state Test parameter controlling this contract scenario.
 * @param recording Test parameter controlling this contract scenario.
 *
 * @return Test callback result/status, checked by the calling assertion.
 */
static TNgControlStatus WaitState(TNgControl *c, TNgRunState state, TNgRecordState recording)
{
    TNgControlStatus s;
    unsigned k;
    for (k = 0; k < 500; ++k) {
        NgControlSnapshot(c, &s);
        if (s.state == state && s.recordingState == recording &&
            s.completedCommandId == s.commandId) {
            return s;
        }
        NgSleepMs(10);
    }
    fprintf(stderr, "Timeout: state=%d record=%d message=%s\n", s.state, s.recordingState,
            s.message);
    exit(1);
}

/**
 * @brief Verify remove capture behaviour.
 *
 * @details
 * Test-only helper exercising the named contract with deterministic inputs. Caller owns
 * fixture/context storage; assertion failures abort the test.
 *
 * @param context Borrowed test fixture passed through the callback interface.
 * @param name Read-only test input; retained by the fixture owner.
 * @param bytes Test parameter controlling this contract scenario.
 */
static void RemoveCapture(void *context, const char *name, uint64_t bytes)
{
    char path[1024];
    (void)bytes;
    CHECK(snprintf(path, sizeof(path), "%s/%s", (const char *)context, name) < (int)sizeof(path));
    CHECK(remove(path) == 0);
}

/**
 * @brief Run the C contract test executable.
 *
 * @details
 * Assertions abort with a nonzero process status on contract failures. This executable is
 * development tooling, not receiver runtime.
 *
 * @return Zero if every contract assertion passes.
 */
int main(void)
{
    TNgConfig cfg;
    TNgConfig saved;
    TNgControl *c = NULL;
    TNgControlStatus s;
    TNgError error = {{0}};
    uint64_t revision;
    uint64_t id;
    uint64_t before;
    uint64_t bytes;
    unsigned k;
    unsigned char random[4];
    char directory[100];
    char path[1024];
    char firstRecording[160];
    FILE *file;
    CHECK(NgRandomBytes(random, sizeof(random)) == 0);
    snprintf(directory, sizeof(directory), "ngene_control_test_%02x%02x%02x%02x", random[0],
             random[1], random[2], random[3]);
    NgConfigDefaults(&cfg);
    cfg.sampleRateHz = 100000;
    cfg.windowSamples = 1000;
    cfg.runSeconds = 0;
    snprintf(cfg.replayPath, sizeof(cfg.replayPath), "%s_input.cs8", directory);
    CHECK(NgTestWriteTone(cfg.replayPath, 50000, cfg.sampleRateHz) == NG_OK);
    cfg.replayLoop = 1;
    cfg.replayRealtime = 1;
    cfg.recordingEnabled = 0;
    strcpy(cfg.recordingPrefix, "test");
    CHECK(NgControlCreate(&cfg, directory, &c, &error) == NG_OK);
    NgControlSnapshot(c, &s);
    CHECK(s.analysis.config.spectrumEnabled == cfg.analysis.spectrumEnabled);
    CHECK(s.analysis.config.histogramEnabled == cfg.analysis.histogramEnabled);
    NgControlConfig(c, &saved, &revision);
    CHECK(revision == 1);
    CHECK(NgControlConfigure(c, &cfg, 0, &error) == NG_ERROR);
    CHECK(NgControlConfigure(c, &cfg, revision, &error) == NG_OK);
    NgControlConfig(c, &saved, &revision);
    CHECK(revision == 2);
    snprintf(path, sizeof(path), "%s/receiver.ini", directory);
    CHECK(NgConfigLoad(path, &saved, &error) == NG_OK);
    CHECK(saved.sampleRateHz == cfg.sampleRateHz && saved.runSeconds == 0);
    file = tmpfile();
    CHECK(file != NULL);
    CHECK(NgConfigWrite(file, &saved, &error) == NG_OK);
    rewind(file);
    CHECK(NgConfigRead(file, &saved, &error) == NG_OK);
    fclose(file);
    CHECK(NgControlRequest(c, NG_CMD_START, revision - 1, &id, &error) == NG_ERROR);
    CHECK(NgControlRequest(c, NG_CMD_START, revision, &id, &error) == NG_OK);
    WaitState(c, NG_RUNNING, NG_REC_OFF);
    CHECK(NgControlConfigure(c, &cfg, revision, &error) == NG_ERROR);
    CHECK(NgControlRequest(c, NG_CMD_START, revision, &id, &error) == NG_ERROR);
    CHECK(NgControlRequest(c, NG_CMD_RECORD_START, revision, &id, &error) == NG_OK);
    WaitState(c, NG_RUNNING, NG_REC_ACTIVE);
    for (k = 0; k < 300; ++k) {
        NgControlSnapshot(c, &s);
        if (s.recording.samples >= 3000) {
            break;
        }
        NgSleepMs(10);
    }
    CHECK(s.recording.samples >= 3000);
    strcpy(firstRecording, s.recordingName);
    CHECK(NgControlRequest(c, NG_CMD_RECORD_STOP, revision, &id, &error) == NG_OK);
    s = WaitState(c, NG_RUNNING, NG_REC_OFF);
    before = s.receiver.processedSamples;
    CHECK(s.recording.samples > 0 && s.totalWrittenSamples == s.recording.samples);
    snprintf(path, sizeof(path), "%s/%s_000000.cs8", directory, firstRecording);
    CHECK(NgRegularFile(path, &bytes) == 0);
    CHECK(bytes == s.recording.samples * sizeof(TNgIq8));
    for (k = 0; k < 300; ++k) {
        NgControlSnapshot(c, &s);
        if (s.receiver.processedSamples > before + 10000) {
            break;
        }
        NgSleepMs(10);
    }
    CHECK(s.state == NG_RUNNING && s.receiver.processedSamples > before + 10000);
    CHECK(NgControlRequest(c, NG_CMD_RECORD_START, revision, &id, &error) == NG_OK);
    s = WaitState(c, NG_RUNNING, NG_REC_ACTIVE);
    CHECK(strcmp(firstRecording, s.recordingName) != 0);
    CHECK(NgControlRequest(c, NG_CMD_STOP, revision, &id, &error) == NG_OK);
    s = WaitState(c, NG_STOPPED, NG_REC_OFF);
    CHECK(s.commandOk);
    /* Immediate cancellation is legal before or during frontend startup. */
    for (k = 0; k < 8; ++k) {
        CHECK(NgControlRequest(c, NG_CMD_START, revision, &id, &error) == NG_OK);
        CHECK(NgControlRequest(c, NG_CMD_STOP, revision, &id, &error) == NG_OK);
        WaitState(c, NG_STOPPED, NG_REC_OFF);
    }
    cfg.replaySampleLimit = 3500;
    cfg.replayLoop = 0;
    cfg.recordingEnabled = 1;
    CHECK(NgControlConfigure(c, &cfg, revision, &error) == NG_OK);
    NgControlConfig(c, &saved, &revision);
    CHECK(NgControlRequest(c, NG_CMD_START, revision, &id, &error) == NG_OK);
    s = WaitState(c, NG_STOPPED, NG_REC_OFF);
    CHECK(s.commandOk && s.receiver.processedSamples == 3500 && s.recording.samples == 3500);
    /* Runtime switches also need authoritative metadata while the worker is absent. */
    NgControlConfig(c, &saved, &revision);
    saved.analysis.spectrumEnabled = 0;
    CHECK(NgControlAnalysisConfigure(c, &saved.analysis, revision, &error) == NG_OK);
    NgControlSnapshot(c, &s);
    CHECK(s.analysis.config.spectrumEnabled == 0 && s.analysis.sequence == 0);
    saved.analysis.spectrumEnabled = 1;
    CHECK(NgControlAnalysisConfigure(c, &saved.analysis, s.revision, &error) == NG_OK);
    NgControlSnapshot(c, &s);
    CHECK(s.analysis.config.spectrumEnabled == 1 && s.analysis.sequence == 0);
    NgControlDestroy(c);
    CHECK(NgDirectoryFiles(directory, RemoveCapture, directory) == 0);
    CHECK(NG_TEST_REMOVE_DIRECTORY(directory) == 0);
    puts("Controller lifecycle, revision guard, recording and EOF: PASS");
    return 0;
}
