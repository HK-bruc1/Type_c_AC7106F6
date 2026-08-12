#include "audio_capture_lease.h"
#include "spinlock.h"
#include "system/init.h"

struct audio_capture_lease_manager {
    spinlock_t lock;
    OS_SEM released_sem;
    const struct audio_capture_lease_client *owner;
    const struct audio_capture_lease_client *pending_owner;
    u32 generation;
    u32 pending_token;
    u16 references;
    u8 initialized;
};

static struct audio_capture_lease_manager capture_lease_manager;

static u32 audio_capture_next_token(u32 token)
{
    token++;
    return token ? token : 1;
}

int audio_capture_lease_init(void)
{
    if (capture_lease_manager.initialized) {
        return 0;
    }

    memset(&capture_lease_manager, 0, sizeof(capture_lease_manager));
    spin_lock_init(&capture_lease_manager.lock);
    if (os_sem_create(&capture_lease_manager.released_sem, 0)) {
        return -1;
    }
    capture_lease_manager.initialized = 1;
    return 0;
}

static void audio_capture_lease_commit(
    const struct audio_capture_lease_client *client,
    struct audio_capture_lease *lease)
{
    if (capture_lease_manager.owner == client) {
        capture_lease_manager.references++;
    } else {
        capture_lease_manager.owner = client;
        capture_lease_manager.references = 1;
        capture_lease_manager.generation =
            audio_capture_next_token(capture_lease_manager.generation);
    }
    lease->client = client;
    lease->generation = capture_lease_manager.generation;
    lease->held = 1;
}

int audio_capture_lease_acquire(
    const struct audio_capture_lease_client *client,
    struct audio_capture_lease *lease,
    u8 allow_preempt,
    u16 preempt_timeout_ticks)
{
    const struct audio_capture_lease_client *owner;
    void (*revoke)(void *, u32, u32);
    void *revoke_priv;
    u32 owner_generation;
    u32 preempt_token;
    int pend_ret;

    if (!client || !lease || !client->name || lease->held
        || !capture_lease_manager.initialized) {
        return AUDIO_CAPTURE_LEASE_INVALID;
    }

    spin_lock(&capture_lease_manager.lock);
    owner = capture_lease_manager.owner;
    if (!owner) {
        if (capture_lease_manager.pending_owner
            && capture_lease_manager.pending_owner != client) {
            spin_unlock(&capture_lease_manager.lock);
            return AUDIO_CAPTURE_LEASE_BUSY;
        }
        audio_capture_lease_commit(client, lease);
        spin_unlock(&capture_lease_manager.lock);
        return AUDIO_CAPTURE_LEASE_OK;
    }
    if (owner == client) {
        audio_capture_lease_commit(client, lease);
        spin_unlock(&capture_lease_manager.lock);
        return AUDIO_CAPTURE_LEASE_OK;
    }
    if (!allow_preempt || !owner->revoke
        || client->priority <= owner->priority
        || capture_lease_manager.pending_owner) {
        spin_unlock(&capture_lease_manager.lock);
        return AUDIO_CAPTURE_LEASE_BUSY;
    }

    preempt_token = audio_capture_next_token(
        capture_lease_manager.pending_token);
    capture_lease_manager.pending_token = preempt_token;
    capture_lease_manager.pending_owner = client;
    owner_generation = capture_lease_manager.generation;
    revoke = owner->revoke;
    revoke_priv = owner->priv;
    os_sem_set(&capture_lease_manager.released_sem, 0);
    spin_unlock(&capture_lease_manager.lock);

    revoke(revoke_priv, preempt_token, owner_generation);
    pend_ret = os_sem_pend(&capture_lease_manager.released_sem,
                           preempt_timeout_ticks);

    spin_lock(&capture_lease_manager.lock);
    if (!pend_ret && !capture_lease_manager.owner
        && capture_lease_manager.pending_owner == client
        && capture_lease_manager.pending_token == preempt_token) {
        capture_lease_manager.pending_owner = NULL;
        audio_capture_lease_commit(client, lease);
        spin_unlock(&capture_lease_manager.lock);
        printf("[AUDIO_LEASE] grant owner=%s generation=%u preempt=%u\n",
               client->name, (unsigned int)lease->generation,
               (unsigned int)preempt_token);
        return AUDIO_CAPTURE_LEASE_OK;
    }
    if (capture_lease_manager.pending_owner == client
        && capture_lease_manager.pending_token == preempt_token) {
        capture_lease_manager.pending_owner = NULL;
    }
    spin_unlock(&capture_lease_manager.lock);

    printf("[AUDIO_LEASE] preempt_failed requester=%s token=%u pend=%d\n",
           client->name, (unsigned int)preempt_token, pend_ret);
    return AUDIO_CAPTURE_LEASE_PREEMPT_TIMEOUT;
}

void audio_capture_lease_release(struct audio_capture_lease *lease)
{
    u8 wake_waiter = 0;

    if (!lease || !lease->held || !capture_lease_manager.initialized) {
        return;
    }

    spin_lock(&capture_lease_manager.lock);
    if (capture_lease_manager.owner == lease->client
        && capture_lease_manager.generation == lease->generation
        && capture_lease_manager.references) {
        capture_lease_manager.references--;
        if (!capture_lease_manager.references) {
            capture_lease_manager.owner = NULL;
            wake_waiter = capture_lease_manager.pending_owner != NULL;
        }
    }
    lease->client = NULL;
    lease->generation = 0;
    lease->held = 0;
    spin_unlock(&capture_lease_manager.lock);

    if (wake_waiter) {
        os_sem_post(&capture_lease_manager.released_sem);
    }
}

u8 audio_capture_lease_is_held_by(
    const struct audio_capture_lease_client *client)
{
    u8 held;

    if (!client || !capture_lease_manager.initialized) {
        return 0;
    }
    spin_lock(&capture_lease_manager.lock);
    held = capture_lease_manager.owner == client;
    spin_unlock(&capture_lease_manager.lock);
    return held;
}

platform_initcall(audio_capture_lease_init);
