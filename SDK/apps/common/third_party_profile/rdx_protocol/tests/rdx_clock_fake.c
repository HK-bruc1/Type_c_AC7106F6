#include <assert.h>
#include <string.h>

#include "rdx_clock.h"
#include "rdx_clock_fake.h"

struct rdx_clock_fake_read {
    int result;
    struct rdx_clock_snapshot snapshot;
};

static struct rdx_clock_fake_state fake_state;
static struct rdx_clock_fake_read fake_reads[RDX_CLOCK_FAKE_READ_PLAN_MAX];
static unsigned int fake_read_plan_count;
static int fake_write_result;

void rdx_clock_fake_reset(void)
{
    memset(&fake_state, 0, sizeof(fake_state));
    memset(fake_reads, 0, sizeof(fake_reads));
    fake_read_plan_count = 0;
    fake_write_result = 0;
}

void rdx_clock_fake_push_read(int result, uint32_t timestamp,
                              uint8_t is_placeholder)
{
    struct rdx_clock_fake_read *planned;

    assert(fake_read_plan_count < RDX_CLOCK_FAKE_READ_PLAN_MAX);
    planned = &fake_reads[fake_read_plan_count++];
    planned->result = result;
    planned->snapshot.timestamp = timestamp;
    planned->snapshot.is_placeholder = is_placeholder;
}

void rdx_clock_fake_set_write_result(int result)
{
    fake_write_result = result;
}

const struct rdx_clock_fake_state *rdx_clock_fake_get_state(void)
{
    return &fake_state;
}

int rdx_clock_read(struct rdx_clock_snapshot *out)
{
    const struct rdx_clock_fake_read *planned;

    assert(out);
    assert(fake_state.read_count < fake_read_plan_count);
    planned = &fake_reads[fake_state.read_count++];
    if (!planned->result) {
        *out = planned->snapshot;
    }
    return planned->result;
}

int rdx_clock_write(uint32_t timestamp)
{
    fake_state.write_count++;
    fake_state.last_write_timestamp = timestamp;
    return fake_write_result;
}
