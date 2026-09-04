#include <string.h>

#include "rdx_storage_command_slot.h"

static void rdx_storage_command_slot_clear(
    struct rdx_storage_command_slot *slot)
{
    slot->active_token = 0;
    slot->completed_token = 0;
    slot->state = RDX_STORAGE_COMMAND_SLOT_IDLE;
}

void rdx_storage_command_slot_init(struct rdx_storage_command_slot *slot)
{
    if (slot) {
        memset(slot, 0, sizeof(*slot));
    }
}

uint8_t rdx_storage_command_slot_busy(
    const struct rdx_storage_command_slot *slot)
{
    return slot && slot->state != RDX_STORAGE_COMMAND_SLOT_IDLE;
}

uint32_t rdx_storage_command_slot_submit(
    struct rdx_storage_command_slot *slot)
{
    uint32_t token;

    if (!slot || rdx_storage_command_slot_busy(slot)) {
        return 0;
    }
    token = ++slot->next_token;
    if (!token) {
        token = ++slot->next_token;
    }
    slot->active_token = token;
    slot->completed_token = 0;
    slot->state = RDX_STORAGE_COMMAND_SLOT_PENDING;
    return token;
}

uint8_t rdx_storage_command_slot_start(
    struct rdx_storage_command_slot *slot, uint32_t token)
{
    if (!slot || !token || slot->active_token != token
        || (slot->state != RDX_STORAGE_COMMAND_SLOT_PENDING
            && slot->state != RDX_STORAGE_COMMAND_SLOT_ABANDONED)) {
        return 0;
    }
    if (slot->state == RDX_STORAGE_COMMAND_SLOT_PENDING) {
        slot->state = RDX_STORAGE_COMMAND_SLOT_RUNNING;
    }
    return 1;
}

enum rdx_storage_command_deadline_result
rdx_storage_command_slot_deadline(struct rdx_storage_command_slot *slot,
                                  uint32_t token,
                                  uint8_t cancel_pending)
{
    if (!slot || !token || slot->active_token != token) {
        return RDX_STORAGE_COMMAND_DEADLINE_STALE;
    }
    if (slot->state == RDX_STORAGE_COMMAND_SLOT_COMPLETION_READY) {
        return RDX_STORAGE_COMMAND_DEADLINE_COMPLETED;
    }
    if (slot->state == RDX_STORAGE_COMMAND_SLOT_PENDING) {
        if (cancel_pending) {
            rdx_storage_command_slot_clear(slot);
            return RDX_STORAGE_COMMAND_DEADLINE_CANCELLED_PENDING;
        }
        /* Cleanup/barrier commands are accepted once submitted.  The caller
         * may abandon its wait, but the executor must still run the command
         * and reconcile the late completion before permitting slot reuse. */
        slot->state = RDX_STORAGE_COMMAND_SLOT_ABANDONED;
        return RDX_STORAGE_COMMAND_DEADLINE_ABANDONED_PENDING;
    }
    if (slot->state == RDX_STORAGE_COMMAND_SLOT_RUNNING) {
        slot->state = RDX_STORAGE_COMMAND_SLOT_ABANDONED;
        return RDX_STORAGE_COMMAND_DEADLINE_ABANDONED_RUNNING;
    }
    return RDX_STORAGE_COMMAND_DEADLINE_STALE;
}

enum rdx_storage_command_complete_result
rdx_storage_command_slot_complete(struct rdx_storage_command_slot *slot,
                                  uint32_t token)
{
    if (!slot || !token || slot->active_token != token) {
        return RDX_STORAGE_COMMAND_COMPLETE_STALE;
    }
    if (slot->state == RDX_STORAGE_COMMAND_SLOT_RUNNING) {
        slot->completed_token = token;
        slot->state = RDX_STORAGE_COMMAND_SLOT_COMPLETION_READY;
        return RDX_STORAGE_COMMAND_COMPLETE_READY;
    }
    if (slot->state == RDX_STORAGE_COMMAND_SLOT_ABANDONED) {
        /* Keep the slot owned until the executor has reconciled any late
         * side effect.  release() acknowledges that reconciliation finished;
         * a newer command must not overwrite its payload before that cut. */
        slot->completed_token = token;
        slot->state = RDX_STORAGE_COMMAND_SLOT_COMPLETION_READING;
        return RDX_STORAGE_COMMAND_COMPLETE_LATE_DROPPED;
    }
    return RDX_STORAGE_COMMAND_COMPLETE_STALE;
}

uint8_t rdx_storage_command_slot_claim(
    struct rdx_storage_command_slot *slot, uint32_t token)
{
    if (!slot || !token || slot->active_token != token
        || slot->completed_token != token
        || slot->state != RDX_STORAGE_COMMAND_SLOT_COMPLETION_READY) {
        return 0;
    }
    slot->state = RDX_STORAGE_COMMAND_SLOT_COMPLETION_READING;
    return 1;
}

uint8_t rdx_storage_command_slot_release(
    struct rdx_storage_command_slot *slot, uint32_t token)
{
    if (!slot || !token || slot->active_token != token
        || slot->completed_token != token
        || slot->state != RDX_STORAGE_COMMAND_SLOT_COMPLETION_READING) {
        return 0;
    }
    rdx_storage_command_slot_clear(slot);
    return 1;
}
