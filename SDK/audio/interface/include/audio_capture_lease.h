#ifndef AUDIO_CAPTURE_LEASE_H
#define AUDIO_CAPTURE_LEASE_H

#include "system/includes.h"

enum audio_capture_lease_result {
    AUDIO_CAPTURE_LEASE_OK = 0,
    AUDIO_CAPTURE_LEASE_BUSY = -1,
    AUDIO_CAPTURE_LEASE_PREEMPT_TIMEOUT = -2,
    AUDIO_CAPTURE_LEASE_INVALID = -3,
};

struct audio_capture_lease_client {
    const char *name;
    u8 priority;
    void (*revoke)(void *priv, u32 preempt_token, u32 lease_generation);
    void *priv;
};

struct audio_capture_lease {
    const struct audio_capture_lease_client *client;
    u32 generation;
    u8 held;
};

int audio_capture_lease_init(void);
int audio_capture_lease_acquire(
    const struct audio_capture_lease_client *client,
    struct audio_capture_lease *lease,
    u8 allow_preempt,
    u16 preempt_timeout_ticks);
void audio_capture_lease_release(struct audio_capture_lease *lease);
u8 audio_capture_lease_is_held_by(
    const struct audio_capture_lease_client *client);

#endif
