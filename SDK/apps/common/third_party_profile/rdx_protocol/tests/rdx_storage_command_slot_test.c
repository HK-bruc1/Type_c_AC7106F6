#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "rdx_protocol/application/rdx_storage_command_slot.h"

static void test_completion_must_be_claimed_before_reuse(void)
{
    struct rdx_storage_command_slot slot;
    uint32_t token;

    rdx_storage_command_slot_init(&slot);
    token = rdx_storage_command_slot_submit(&slot);
    assert(token != 0);
    assert(rdx_storage_command_slot_start(&slot, token));
    assert(rdx_storage_command_slot_complete(&slot, token)
           == RDX_STORAGE_COMMAND_COMPLETE_READY);
    assert(rdx_storage_command_slot_submit(&slot) == 0);
    assert(rdx_storage_command_slot_claim(&slot, token));
    assert(rdx_storage_command_slot_submit(&slot) == 0);
    assert(rdx_storage_command_slot_release(&slot, token));
    assert(rdx_storage_command_slot_submit(&slot) != 0);
}

static void test_pending_deadline_cancels_without_late_completion(void)
{
    struct rdx_storage_command_slot slot;
    uint32_t cancelled;
    uint32_t current;

    rdx_storage_command_slot_init(&slot);
    cancelled = rdx_storage_command_slot_submit(&slot);
    assert(rdx_storage_command_slot_deadline(&slot, cancelled, 1)
           == RDX_STORAGE_COMMAND_DEADLINE_CANCELLED_PENDING);
    current = rdx_storage_command_slot_submit(&slot);
    assert(current != 0 && current != cancelled);
    assert(rdx_storage_command_slot_complete(&slot, cancelled)
           == RDX_STORAGE_COMMAND_COMPLETE_STALE);
    assert(slot.active_token == current);
    assert(slot.state == RDX_STORAGE_COMMAND_SLOT_PENDING);
}

static void test_running_deadline_drops_only_matching_late_completion(void)
{
    struct rdx_storage_command_slot slot;
    uint32_t abandoned;
    uint32_t current;

    rdx_storage_command_slot_init(&slot);
    abandoned = rdx_storage_command_slot_submit(&slot);
    assert(rdx_storage_command_slot_start(&slot, abandoned));
    assert(rdx_storage_command_slot_deadline(&slot, abandoned, 1)
           == RDX_STORAGE_COMMAND_DEADLINE_ABANDONED_RUNNING);
    assert(rdx_storage_command_slot_submit(&slot) == 0);
    assert(rdx_storage_command_slot_complete(&slot, abandoned)
           == RDX_STORAGE_COMMAND_COMPLETE_LATE_DROPPED);
    assert(rdx_storage_command_slot_busy(&slot));
    assert(rdx_storage_command_slot_release(&slot, abandoned));
    current = rdx_storage_command_slot_submit(&slot);
    assert(current != 0 && current != abandoned);
    assert(rdx_storage_command_slot_complete(&slot, abandoned)
           == RDX_STORAGE_COMMAND_COMPLETE_STALE);
    assert(slot.active_token == current);
}

static void test_completion_wins_deadline_race(void)
{
    struct rdx_storage_command_slot slot;
    uint32_t token;

    rdx_storage_command_slot_init(&slot);
    token = rdx_storage_command_slot_submit(&slot);
    assert(rdx_storage_command_slot_start(&slot, token));
    assert(rdx_storage_command_slot_complete(&slot, token)
           == RDX_STORAGE_COMMAND_COMPLETE_READY);
    assert(rdx_storage_command_slot_deadline(&slot, token, 1)
           == RDX_STORAGE_COMMAND_DEADLINE_COMPLETED);
    assert(rdx_storage_command_slot_claim(&slot, token));
    assert(rdx_storage_command_slot_release(&slot, token));
}

static void test_token_wrap_skips_zero(void)
{
    struct rdx_storage_command_slot slot;

    rdx_storage_command_slot_init(&slot);
    slot.next_token = UINT32_MAX;
    assert(rdx_storage_command_slot_submit(&slot) == 1);
}

static void test_must_run_pending_survives_caller_deadline(void)
{
    struct rdx_storage_command_slot slot;
    uint32_t token;

    rdx_storage_command_slot_init(&slot);
    token = rdx_storage_command_slot_submit(&slot);
    assert(rdx_storage_command_slot_deadline(&slot, token, 0)
           == RDX_STORAGE_COMMAND_DEADLINE_ABANDONED_PENDING);
    assert(rdx_storage_command_slot_busy(&slot));
    assert(rdx_storage_command_slot_start(&slot, token));
    assert(slot.state == RDX_STORAGE_COMMAND_SLOT_ABANDONED);
    assert(rdx_storage_command_slot_complete(&slot, token)
           == RDX_STORAGE_COMMAND_COMPLETE_LATE_DROPPED);
    assert(rdx_storage_command_slot_release(&slot, token));
}

int main(void)
{
    test_completion_must_be_claimed_before_reuse();
    test_pending_deadline_cancels_without_late_completion();
    test_running_deadline_drops_only_matching_late_completion();
    test_completion_wins_deadline_race();
    test_token_wrap_skips_zero();
    test_must_run_pending_survives_caller_deadline();
    puts("RDX storage command slot tests passed");
    return 0;
}
