/** @file test_channel_slots.c @brief Bounded allocation and per-slot lifecycle on the common stream. */
#include "tracking/bank.h"
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "line %d: %s\n", __LINE__, #x);                                        \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

/** Deterministic completed strategy; real search arithmetic is tested separately. */
static int Search(void *context, const TNgAcquisitionCapture *capture, unsigned prn,
                  const TNgAcquisitionConfig *config, TNgAcquisitionCancel cancel,
                  void *cancelContext, TNgAcquisitionResult *result, TNgError *error)
{
    if (context)
        NgSleepMs(*(const unsigned *)context);
    (void)config;
    (void)cancel;
    (void)cancelContext;
    (void)error;
    memset(result, 0, sizeof(*result));
    result->prn = prn;
    result->confirmed = 1;
    result->successes = 3;
    result->epoch = capture->origin.epoch;
    result->firstSample = capture->origin.firstSample;
    return NG_OK;
}

/** Feed a single borrowed common window, then allow the asynchronous search worker to finish. */
static void Window(TNgChannel *channel, TNgChannel *galileo, TNgIqView *view)
{
    TNgError error = {{0}};
    TNgChannelState next;
    CHECK(channel->ops->process(channel->context, &channel->id, channel->state, view, &next,
                                &error) == NG_OK);
    channel->state = next;
    CHECK(galileo->ops->process(galileo->context, &galileo->id, galileo->state, view, &next,
                                &error) == NG_OK);
    galileo->state = next;
    view->meta.firstSample += view->meta.count;
    view->meta.flags = 0;
    NgSleepMs(1);
}

/** Faster confirmed results claim shared slots, independent of constellation order. */
static void AutomaticPool(unsigned faster, unsigned priority)
{
    TNgTrackingConfig config;
    TNgAcquisitionConfig searchConfig;
    TNgStreamInfo stream = {0};
    TNgAcquisition *searches[2] = {NULL, NULL};
    TNgTrackingBank *banks[2] = {NULL, NULL};
    TNgChannel channels[2];
    TNgTrackingSnapshot slots[64];
    TNgIq8 samples[2048] = {{0}};
    TNgIqView view = {0};
    TNgError error = {{0}};
    const TNgAcquisitionStrategy strategy = {Search};
    unsigned delay[2] = {faster == 1 ? 0u : 150u, faster == 2 ? 0u : 150u};
    unsigned system, n;
    NgTrackingDefaults(&config);
    config.channelCount = 2;
    config.autoPriority = priority;
    config.workers = 2;
    NgAcquisitionDefaults(&searchConfig);
    searchConfig.lastPrn = 3;
    stream.sampleRateHz = 2048000;
    for (system = 0; system < 2; ++system) {
        CHECK(NgAcquisitionCreateWithStrategy(&searchConfig, &stream, &strategy, &delay[system],
                                              &searches[system], &channels[system],
                                              &error) == NG_OK);
        channels[system].id.constellation = system + 1;
        CHECK(NgTrackingBankCreate(&config, &stream, searches[system], &channels[system],
                                   &banks[system], &error) == NG_OK);
    }
    CHECK(NgTrackingBankLink(banks[0], banks[1], &error) == NG_OK);
    view.samples = samples;
    view.meta.count = 2048;
    view.meta.epoch = 1;
    for (n = 0; n < 120; ++n) {
        Window(&channels[0], &channels[1], &view);
        if (priority && n == 55)
            NgSleepMs(600); /* Both strategies finish before the next admission. */
    }
    NgTrackingBankReadAll(banks[0], slots);
    CHECK(slots[0].channelId == 1 && slots[1].channelId == 2 && !slots[2].channelId);
    CHECK(slots[0].constellation == (priority ? priority : faster) &&
          slots[1].constellation == (priority ? priority : faster));
    CHECK(slots[0].state == 2 && slots[1].state == 2 && slots[0].prn != slots[1].prn);
    NgTrackingBankDestroy(banks[0]);
    NgTrackingBankDestroy(banks[1]);
    NgAcquisitionDestroy(searches[0]);
    NgAcquisitionDestroy(searches[1]);
}

/** Fill slots, pause searches at capacity, invalidate on a gap and reuse the same identities. */
int main(void)
{
    TNgTrackingConfig config;
    TNgAcquisitionConfig searchConfig;
    TNgStreamInfo stream = {0};
    TNgAcquisition *acquisition = NULL, *galileoAcquisition = NULL;
    TNgTrackingBank *bank = NULL, *galileoBank = NULL;
    TNgChannel channel, galileo;
    TNgTrackingSnapshot slots[32];
    TNgAcquisitionSnapshot before, after;
    TNgIq8 samples[2048] = {{0}};
    TNgIqView view = {0};
    TNgError error = {{0}};
    const TNgAcquisitionStrategy strategy = {Search};
    unsigned n;
    stream.sampleRateHz = 2048000;
    NgTrackingDefaults(&config);
    config.channelCount = 3;
    config.gpsChannels = 2;
    config.galileoChannels = 1;
    config.workers = 2;
    NgAcquisitionDefaults(&searchConfig);
    searchConfig.lastPrn = 3;
    CHECK(NgAcquisitionCreateWithStrategy(&searchConfig, &stream, &strategy, NULL, &acquisition,
                                          &channel, &error) == NG_OK);
    CHECK(NgTrackingBankCreate(&config, &stream, acquisition, &channel, &bank, &error) == NG_OK);
    CHECK(NgAcquisitionCreateWithStrategy(&searchConfig, &stream, &strategy, NULL,
                                          &galileoAcquisition, &galileo, &error) == NG_OK);
    galileo.id.constellation = 2;
    CHECK(NgTrackingBankCreate(&config, &stream, galileoAcquisition, &galileo, &galileoBank,
                               &error) == NG_OK);
    CHECK(NgTrackingBankLink(bank, galileoBank, &error) == NG_OK);
    NgTrackingBankRead(bank, slots);
    CHECK(slots[0].channelId == 1 && slots[1].channelId == 2 && slots[2].channelId == 3 &&
          slots[3].channelId == 0);
    CHECK(slots[0].state == 0 && slots[1].state == 0);
    view.samples = samples;
    view.meta.count = 2048;
    view.meta.epoch = 1;
    for (n = 0; n < 100; n++)
        Window(&channel, &galileo, &view);
    NgTrackingBankRead(bank, slots);
    CHECK(slots[0].state == 2 && slots[1].state == 2);
    CHECK(slots[0].prn != slots[1].prn);
    NgAcquisitionRead(acquisition, &before);
    for (n = 0; n < 100; n++)
        Window(&channel, &galileo, &view);
    NgAcquisitionRead(acquisition, &after);
    CHECK(after.searches == before.searches);
    view.meta.epoch = 2;
    view.meta.firstSample = 0;
    view.meta.flags = NG_BLOCK_GAP;
    Window(&channel, &galileo, &view);
    NgTrackingBankRead(bank, slots);
    CHECK(slots[0].state != 2 && slots[1].state != 2);
    for (n = 0; n < 100; n++)
        Window(&channel, &galileo, &view);
    NgTrackingBankRead(bank, slots);
    CHECK(slots[0].channelId == 1 && slots[1].channelId == 2);
    CHECK(slots[0].state == 2 && slots[0].epoch == 2);
    CHECK(slots[1].state == 2 && slots[1].epoch == 2);
    NgTrackingBankRead(galileoBank, slots);
    CHECK(slots[2].channelId == 3 && slots[2].state == 2 && slots[2].epoch == 2 &&
          slots[2].constellation == 2);
    CHECK(slots[3].channelId == 0);
    NgTrackingBankDestroy(bank);
    NgTrackingBankDestroy(galileoBank);
    NgAcquisitionDestroy(acquisition);
    NgAcquisitionDestroy(galileoAcquisition);
    AutomaticPool(1, 0);
    AutomaticPool(2, 0);
    AutomaticPool(1, 2);
    AutomaticPool(2, 1);
    puts("shared channel allocation, reservations and gap reuse passed");
    return 0;
}
