/* Explicit development target; excluded from normal builds. */
#include "../../src/frontend/sige.c"

int main(void)
{
    TNgConfig config;
    TNgFrontend frontend = {0};
    TNgError error = {{0}};
    Sige *s;
    double start;
    unsigned calls = 0;
    uint64_t pairs = 0;
    NgConfigDefaults(&config);
    NgConfigSige(&config);
    config.windowSamples = 8184;
    if (NgSigeOpen(&config, &frontend, &error) != NG_OK) {
        fprintf(stderr, "%s\n", error.message);
        return 1;
    }
    s = frontend.context;
    printf("libusb=%s transfers=%d bytes=%d\n", libusb_get_version()->describe, TRANSFERS, BYTES);
    if (Start(s, &error) != NG_OK) {
        fprintf(stderr, "%s\n", error.message);
        Destroy(s);
        return 1;
    }
    start = NgMonotonicSeconds();
    while (NgMonotonicSeconds() - start < 3) {
        TNgIqView view = {0};
        int rc = Acquire(s, &view, 100, &error);
        unsigned ready = 0, flight = 0, k;
        ++calls;
        if (rc == NG_OK) {
            pairs += view.meta.count;
        }
        for (k = 0; k < TRANSFERS; k++) {
            ready += (unsigned)s->slots[k].ready;
            flight += (unsigned)s->slots[k].flight;
        }
        if (calls < 6 || rc != NG_OK) {
            printf("call=%u rc=%d pairs=%llu current=%u offset=%zu ready=%u flight=%u "
                   "currentLength=%d currentStatus=%d %s\n",
                   calls, rc, (unsigned long long)pairs, s->current, s->offset, ready, flight,
                   s->slots[s->current].transfer->actual_length,
                   (int)s->slots[s->current].transfer->status, error.message);
        }
        if (rc == NG_OK) {
            Release(s);
        } else if (rc != NG_TIMEOUT) {
            break;
        }
    }
    printf("total=%llu elapsed=%.3f\n", (unsigned long long)pairs, NgMonotonicSeconds() - start);
    Stop(s, &error);
    Destroy(s);
    return 0;
}
