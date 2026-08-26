#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "user_cfg_id.h"
#include "rdx/ports/rdx_rtc_backup_repository_port.h"

#define READ_PLAN_MAX 4

struct read_plan_item {
    int result;
    uint32_t value;
};

static struct read_plan_item read_plan[READ_PLAN_MAX];
static unsigned int read_plan_count;
static unsigned int read_count;
static int write_result;
static unsigned int write_count;
static uint32_t last_write;

static void reset_backend(void)
{
    memset(read_plan, 0, sizeof(read_plan));
    read_plan_count = 0;
    read_count = 0;
    write_result = sizeof(uint32_t);
    write_count = 0;
    last_write = 0;
}

static void push_read(int result, uint32_t value)
{
    assert(read_plan_count < READ_PLAN_MAX);
    read_plan[read_plan_count].result = result;
    read_plan[read_plan_count].value = value;
    read_plan_count++;
}

int syscfg_read(unsigned int id, void *buffer, unsigned int len)
{
    const struct read_plan_item *planned;

    assert(id == CFG_RDX_RTC_BACKUP);
    assert(len == sizeof(uint32_t));
    assert(read_count < read_plan_count);
    planned = &read_plan[read_count++];
    if (planned->result == sizeof(uint32_t)) {
        memcpy(buffer, &planned->value, sizeof(planned->value));
    }
    return planned->result;
}

int syscfg_write(unsigned int id, const void *buffer, unsigned int len)
{
    assert(id == CFG_RDX_RTC_BACKUP);
    assert(len == sizeof(uint32_t));
    write_count++;
    memcpy(&last_write, buffer, sizeof(last_write));
    return write_result;
}

static void test_load_contract(void)
{
    uint32_t timestamp = 0;

    reset_backend();
    assert(rdx_rtc_backup_repository_load(NULL) == -1);
    push_read(-1, 0);
    assert(rdx_rtc_backup_repository_load(&timestamp) == -1);
    push_read(sizeof(uint32_t), 1704067200u);
    assert(rdx_rtc_backup_repository_load(&timestamp) == 0);
    assert(timestamp == 1704067200u);
}

static void test_unchanged_value_skips_write(void)
{
    reset_backend();
    push_read(sizeof(uint32_t), 1704067200u);
    assert(rdx_rtc_backup_repository_save(1704067200u) == 0);
    assert(write_count == 0);
}

static void test_write_and_verify_failures(void)
{
    reset_backend();
    push_read(-1, 0);
    write_result = -1;
    assert(rdx_rtc_backup_repository_save(1704067200u) == -1);
    assert(write_count == 1);

    reset_backend();
    push_read(-1, 0);
    push_read(-1, 0);
    assert(rdx_rtc_backup_repository_save(1704067200u) == -1);

    reset_backend();
    push_read(sizeof(uint32_t), 1);
    push_read(sizeof(uint32_t), 2);
    assert(rdx_rtc_backup_repository_save(1704067200u) == -1);
}

static void test_success_preserves_raw_timestamp_layout(void)
{
    reset_backend();
    push_read(-1, 0);
    push_read(sizeof(uint32_t), 1704067200u);
    assert(rdx_rtc_backup_repository_save(1704067200u) == 0);
    assert(write_count == 1);
    assert(last_write == 1704067200u);
    assert(sizeof(last_write) == 4);
}

int main(void)
{
    test_load_contract();
    test_unchanged_value_skips_write();
    test_write_and_verify_failures();
    test_success_preserves_raw_timestamp_layout();
    puts("RDX BR56 RTC backup repository tests passed");
    return 0;
}
