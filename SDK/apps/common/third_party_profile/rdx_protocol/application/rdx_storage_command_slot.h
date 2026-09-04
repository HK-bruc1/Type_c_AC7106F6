#ifndef RDX_STORAGE_COMMAND_SLOT_H
#define RDX_STORAGE_COMMAND_SLOT_H

#include <stdint.h>

enum rdx_storage_command_slot_state {
    RDX_STORAGE_COMMAND_SLOT_IDLE = 0,
    RDX_STORAGE_COMMAND_SLOT_PENDING,
    RDX_STORAGE_COMMAND_SLOT_RUNNING,
    RDX_STORAGE_COMMAND_SLOT_COMPLETION_READY,
    RDX_STORAGE_COMMAND_SLOT_COMPLETION_READING,
    RDX_STORAGE_COMMAND_SLOT_ABANDONED,
};

enum rdx_storage_command_deadline_result {
    RDX_STORAGE_COMMAND_DEADLINE_STALE = 0,
    RDX_STORAGE_COMMAND_DEADLINE_COMPLETED,
    RDX_STORAGE_COMMAND_DEADLINE_CANCELLED_PENDING,
    RDX_STORAGE_COMMAND_DEADLINE_ABANDONED_PENDING,
    RDX_STORAGE_COMMAND_DEADLINE_ABANDONED_RUNNING,
};

enum rdx_storage_command_complete_result {
    RDX_STORAGE_COMMAND_COMPLETE_STALE = 0,
    RDX_STORAGE_COMMAND_COMPLETE_READY,
    RDX_STORAGE_COMMAND_COMPLETE_LATE_DROPPED,
};

struct rdx_storage_command_slot {
    uint32_t next_token;
    uint32_t active_token;
    uint32_t completed_token;
    enum rdx_storage_command_slot_state state;
};

void rdx_storage_command_slot_init(struct rdx_storage_command_slot *slot);
uint8_t rdx_storage_command_slot_busy(
    const struct rdx_storage_command_slot *slot);
uint32_t rdx_storage_command_slot_submit(
    struct rdx_storage_command_slot *slot);
uint8_t rdx_storage_command_slot_start(
    struct rdx_storage_command_slot *slot, uint32_t token);
enum rdx_storage_command_deadline_result
rdx_storage_command_slot_deadline(struct rdx_storage_command_slot *slot,
                                  uint32_t token,
                                  uint8_t cancel_pending);
enum rdx_storage_command_complete_result
rdx_storage_command_slot_complete(struct rdx_storage_command_slot *slot,
                                  uint32_t token);
uint8_t rdx_storage_command_slot_claim(
    struct rdx_storage_command_slot *slot, uint32_t token);
uint8_t rdx_storage_command_slot_release(
    struct rdx_storage_command_slot *slot, uint32_t token);

#endif /* RDX_STORAGE_COMMAND_SLOT_H */
