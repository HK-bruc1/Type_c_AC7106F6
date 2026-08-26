#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "rdx_clock_fake.h"
#include "rdx_rtc.h"
#include "rdx_rtc_backup_repository_fake.h"

#define TIMESTAMP_1999_12_31    946598400u
#define TIMESTAMP_2000_01_01    946684800u
#define TIMESTAMP_2024_01_01    1704067200u
#define TIMESTAMP_2099_12_31    4102358400u
#define TIMESTAMP_2100_01_01    4102444800u

static void reset_fakes(void)
{
    rdx_clock_fake_reset();
    rdx_rtc_backup_repository_fake_reset();
}

static void test_hardware_time_is_trusted_and_backed_up(void)
{
    const struct rdx_clock_fake_state *clock;
    const struct rdx_rtc_backup_repository_fake_state *repository;

    reset_fakes();
    rdx_clock_fake_push_read(0, TIMESTAMP_2024_01_01, 0);
    assert(rdx_rtc_init() == 0);

    clock = rdx_clock_fake_get_state();
    repository = rdx_rtc_backup_repository_fake_get_state();
    assert(clock->read_count == 1);
    assert(clock->write_count == 0);
    assert(repository->load_count == 0);

    rdx_clock_fake_push_read(0, TIMESTAMP_2024_01_01 + 5, 0);
    assert(rdx_rtc_store_backup() == 0);
    assert(repository->save_count == 1);
    assert(repository->last_saved_timestamp == TIMESTAMP_2024_01_01 + 5);
}

static void test_placeholder_restores_valid_backup(void)
{
    const struct rdx_clock_fake_state *clock;
    const struct rdx_rtc_backup_repository_fake_state *repository;

    reset_fakes();
    rdx_clock_fake_push_read(0, TIMESTAMP_2000_01_01, 1);
    rdx_clock_fake_push_read(0, TIMESTAMP_2024_01_01, 0);
    rdx_rtc_backup_repository_fake_set_load(0, TIMESTAMP_2024_01_01);
    assert(rdx_rtc_init() == 0);

    clock = rdx_clock_fake_get_state();
    repository = rdx_rtc_backup_repository_fake_get_state();
    assert(clock->read_count == 2);
    assert(clock->write_count == 1);
    assert(clock->last_write_timestamp == TIMESTAMP_2024_01_01);
    assert(repository->load_count == 1);
    assert(repository->save_count == 1);
}

static void test_invalid_sources_remain_untrusted(void)
{
    const struct rdx_clock_fake_state *clock;
    const struct rdx_rtc_backup_repository_fake_state *repository;

    reset_fakes();
    rdx_clock_fake_push_read(0, TIMESTAMP_2100_01_01, 0);
    rdx_rtc_backup_repository_fake_set_load(0, TIMESTAMP_1999_12_31);
    assert(rdx_rtc_init() == 0);
    assert(rdx_rtc_store_backup() == 0);

    clock = rdx_clock_fake_get_state();
    repository = rdx_rtc_backup_repository_fake_get_state();
    assert(clock->read_count == 1);
    assert(clock->write_count == 0);
    assert(repository->load_count == 1);
    assert(repository->save_count == 0);
}

static void test_set_rejects_invalid_time_and_clock_failures(void)
{
    const struct rdx_clock_fake_state *clock;
    const struct rdx_rtc_backup_repository_fake_state *repository;

    reset_fakes();
    rdx_clock_fake_push_read(0, TIMESTAMP_2024_01_01, 0);
    assert(rdx_rtc_init() == 0);
    assert(rdx_rtc_set_timestamp(TIMESTAMP_1999_12_31) == -1);
    assert(rdx_rtc_set_timestamp(TIMESTAMP_2100_01_01) == -1);

    rdx_clock_fake_set_write_result(-1);
    assert(rdx_rtc_set_timestamp(TIMESTAMP_2024_01_01 + 10) == -1);
    rdx_clock_fake_set_write_result(0);

    rdx_clock_fake_push_read(-1, 0, 0);
    assert(rdx_rtc_set_timestamp(TIMESTAMP_2024_01_01 + 20) == -1);
    rdx_clock_fake_push_read(0, TIMESTAMP_2024_01_01 + 29, 0);
    assert(rdx_rtc_set_timestamp(TIMESTAMP_2024_01_01 + 30) == -1);

    clock = rdx_clock_fake_get_state();
    repository = rdx_rtc_backup_repository_fake_get_state();
    assert(clock->write_count == 3);
    assert(repository->save_count == 0);
}

static void test_repository_failure_preserves_hardware_trust(void)
{
    const struct rdx_rtc_backup_repository_fake_state *repository;

    reset_fakes();
    rdx_clock_fake_push_read(0, TIMESTAMP_2024_01_01, 0);
    assert(rdx_rtc_init() == 0);

    rdx_rtc_backup_repository_fake_set_save_result(-1);
    rdx_clock_fake_push_read(0, TIMESTAMP_2099_12_31, 0);
    assert(rdx_rtc_set_timestamp(TIMESTAMP_2099_12_31) == -1);

    rdx_rtc_backup_repository_fake_set_save_result(0);
    rdx_clock_fake_push_read(0, TIMESTAMP_2099_12_31 - 1, 0);
    assert(rdx_rtc_store_backup() == 0);
    repository = rdx_rtc_backup_repository_fake_get_state();
    assert(repository->save_count == 2);
    assert(repository->last_saved_timestamp == TIMESTAMP_2099_12_31 - 1);
}

static void test_trusted_clock_read_failure_skips_backup(void)
{
    const struct rdx_rtc_backup_repository_fake_state *repository;

    reset_fakes();
    rdx_clock_fake_push_read(0, TIMESTAMP_2024_01_01, 0);
    assert(rdx_rtc_init() == 0);
    rdx_clock_fake_push_read(-1, 0, 0);
    assert(rdx_rtc_store_backup() == 0);
    repository = rdx_rtc_backup_repository_fake_get_state();
    assert(repository->save_count == 0);
}

int main(void)
{
    test_hardware_time_is_trusted_and_backed_up();
    test_placeholder_restores_valid_backup();
    test_invalid_sources_remain_untrusted();
    test_set_rejects_invalid_time_and_clock_failures();
    test_repository_failure_preserves_hardware_trust();
    test_trusted_clock_read_failure_skips_backup();
    puts("RDX RTC service tests passed");
    return 0;
}
