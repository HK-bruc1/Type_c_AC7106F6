#ifndef AUDIO_CAPTURE_LEASE_H
#define AUDIO_CAPTURE_LEASE_H

#include <stdint.h>

enum audio_capture_lease_result {
    AUDIO_CAPTURE_LEASE_OK = 0,
    AUDIO_CAPTURE_LEASE_BUSY = -1,
    AUDIO_CAPTURE_LEASE_PREEMPT_TIMEOUT = -2,
    AUDIO_CAPTURE_LEASE_INVALID = -3,
};

struct audio_capture_lease_client {
    const char *name;
    uint8_t priority;
    void (*revoke)(void *priv, uint32_t preempt_token,
                   uint32_t lease_generation);
    void *priv;
};

struct audio_capture_lease {
    const struct audio_capture_lease_client *client;
    uint32_t generation;
    uint8_t held;
};

int audio_capture_lease_init(void);
int audio_capture_lease_acquire(
    const struct audio_capture_lease_client *client,
    struct audio_capture_lease *lease,
    uint8_t allow_preempt,
    uint16_t preempt_timeout_ticks);
void audio_capture_lease_release(struct audio_capture_lease *lease);
uint8_t audio_capture_lease_is_held_by(
    const struct audio_capture_lease_client *client);

#endif
