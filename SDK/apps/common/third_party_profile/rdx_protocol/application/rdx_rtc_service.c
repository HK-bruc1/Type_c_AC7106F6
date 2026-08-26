#include <stdint.h>
#include <stdio.h>

#include "app_config.h"

#if TCFG_RDX_ENABLE && RDX_CFG_RTC_ENABLE
#include "rdx/ports/rdx_clock_port.h"
#include "rdx/rdx_rtc_service.h"
#include "rdx/ports/rdx_rtc_backup_repository_port.h"

#define RDX_RTC_SECONDS_PER_DAY    86400u

static uint8_t rdx_rtc_trusted;

static int rdx_rtc_leap_year(uint32_t year)
{
    return !(year % 4) && ((year % 100) || !(year % 400));
}

static uint32_t rdx_rtc_timestamp_year(uint32_t timestamp)
{
    uint32_t days = timestamp / RDX_RTC_SECONDS_PER_DAY;
    uint32_t year = 1970;

    for (;;) {
        uint32_t days_in_year = rdx_rtc_leap_year(year) ? 366 : 365;

        if (days < days_in_year) {
            return year;
        }
        days -= days_in_year;
        year++;
    }
}

static int rdx_rtc_timestamp_valid(uint32_t timestamp)
{
    uint32_t year = rdx_rtc_timestamp_year(timestamp);

    return year >= RDX_RTC_VALID_YEAR_MIN
           && year <= RDX_RTC_VALID_YEAR_MAX;
}

static int rdx_rtc_read_valid_clock(struct rdx_clock_snapshot *snapshot)
{
    return !rdx_clock_read(snapshot)
           && rdx_rtc_timestamp_valid(snapshot->timestamp);
}

int rdx_rtc_init(void)
{
    struct rdx_clock_snapshot current;
    uint32_t backup_timestamp;

    rdx_rtc_trusted = 0;
    if (rdx_rtc_read_valid_clock(&current) && !current.is_placeholder) {
        rdx_rtc_trusted = 1;
        printf("[RDX][RTC] init source=hardware timestamp=%u\n",
               current.timestamp);
        return 0;
    }

    if (!rdx_rtc_backup_repository_load(&backup_timestamp)
        && rdx_rtc_timestamp_valid(backup_timestamp)) {
        int ret = rdx_rtc_set_timestamp(backup_timestamp);

        printf("[RDX][RTC] init source=backup timestamp=%u ret=%d\n",
               backup_timestamp, ret);
        return 0;
    }

    printf("[RDX][RTC] init source=hardware_default backup=empty_or_invalid\n");
    return 0;
}

int rdx_rtc_set_timestamp(uint32_t timestamp)
{
    struct rdx_clock_snapshot read_back;
    int write_ret;
    int hardware_ok;
    int repository_ok;

    if (!rdx_rtc_timestamp_valid(timestamp)) {
        printf("[RDX][RTC] set rejected timestamp=%u reason=out_of_range\n",
               timestamp);
        return -1;
    }

    write_ret = rdx_clock_write(timestamp);
    hardware_ok = !write_ret
                  && rdx_rtc_read_valid_clock(&read_back)
                  && read_back.timestamp == timestamp;
    if (hardware_ok) {
        rdx_rtc_trusted = 1;
    }

    repository_ok = hardware_ok
                    && !rdx_rtc_backup_repository_save(timestamp);
    printf("[RDX][RTC] set timestamp=%u clock_write_ret=%d clock=%s backup=%s\n",
           timestamp, write_ret, hardware_ok ? "ok" : "failed",
           repository_ok ? "ok" : "failed");
    return hardware_ok && repository_ok ? 0 : -1;
}

int rdx_rtc_store_backup(void)
{
    struct rdx_clock_snapshot current;

    if (!rdx_rtc_trusted || !rdx_rtc_read_valid_clock(&current)) {
        printf("[RDX][RTC] backup skipped trusted=%u\n", rdx_rtc_trusted);
        return 0;
    }

    if (rdx_rtc_backup_repository_save(current.timestamp)) {
        printf("[RDX][RTC] backup failed timestamp=%u\n",
               current.timestamp);
        return -1;
    }

    printf("[RDX][RTC] backup timestamp=%u\n", current.timestamp);
    return 0;
}

#endif /* TCFG_RDX_ENABLE && RDX_CFG_RTC_ENABLE */
