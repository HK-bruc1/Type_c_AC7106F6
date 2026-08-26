#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "rtc/rtc_dev.h"
#include "rdx/ports/rdx_clock_port.h"

const struct sys_time def_sys_time = { 2020, 1, 1, 0, 0, 0 };

static struct sys_time rtc_current;
static struct sys_time rtc_last_write;
static u32 rtc_write_result;
static unsigned int rtc_read_count;
static unsigned int rtc_write_count;

static int leap_year(u32 year)
{
    return !(year % 4) && ((year % 100) || !(year % 400));
}

u32 days_in_month(u32 year, u32 month)
{
    static const u8 days[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
    };

    if (month < 1 || month > 12) {
        return 0;
    }
    return month == 2 && leap_year(year) ? 29 : days[month - 1];
}

u64 datetime_to_timestamp(const struct sys_time *time)
{
    u64 days = 0;
    u32 year;
    u32 month;

    for (year = 1970; year < time->year; year++) {
        days += leap_year(year) ? 366 : 365;
    }
    for (month = 1; month < time->month; month++) {
        days += days_in_month(time->year, month);
    }
    days += time->day - 1;
    return days * 86400u + time->hour * 3600u
           + time->min * 60u + time->sec;
}

void timestamp_to_datetime(u64 total_seconds, struct sys_time *time)
{
    u64 days = total_seconds / 86400u;
    u32 seconds = (u32)(total_seconds % 86400u);
    u32 year = 1970;
    u32 month = 1;
    u32 span;

    while (days >= (u64)(leap_year(year) ? 366 : 365)) {
        days -= leap_year(year) ? 366 : 365;
        year++;
    }
    while (days >= (span = days_in_month(year, month))) {
        days -= span;
        month++;
    }

    time->year = (u16)year;
    time->month = (u8)month;
    time->day = (u8)days + 1;
    time->hour = (u8)(seconds / 3600u);
    seconds %= 3600u;
    time->min = (u8)(seconds / 60u);
    time->sec = (u8)(seconds % 60u);
}

u32 rtc_read_time(struct sys_time *time)
{
    rtc_read_count++;
    *time = rtc_current;
    return 0;
}

u32 rtc_write_time(const struct sys_time *time)
{
    rtc_write_count++;
    rtc_last_write = *time;
    return rtc_write_result;
}

static void reset_rtc(void)
{
    memset(&rtc_current, 0, sizeof(rtc_current));
    memset(&rtc_last_write, 0, sizeof(rtc_last_write));
    rtc_write_result = 0;
    rtc_read_count = 0;
    rtc_write_count = 0;
}

static void test_read_normalizes_sdk_calendar(void)
{
    struct rdx_clock_snapshot snapshot;

    reset_rtc();
    rtc_current.year = 2024;
    rtc_current.month = 1;
    rtc_current.day = 1;
    assert(rdx_clock_read(&snapshot) == 0);
    assert(snapshot.timestamp == 1704067200u);
    assert(snapshot.is_placeholder == 0);
    assert(rtc_read_count == 1);

    rtc_current = def_sys_time;
    rtc_current.hour = 12;
    assert(rdx_clock_read(&snapshot) == 0);
    assert(snapshot.is_placeholder == 1);
}

static void test_read_rejects_invalid_or_unrepresentable_calendar(void)
{
    struct rdx_clock_snapshot snapshot;

    reset_rtc();
    assert(rdx_clock_read(NULL) == -1);
    rtc_current.year = 2024;
    rtc_current.month = 2;
    rtc_current.day = 30;
    assert(rdx_clock_read(&snapshot) == -1);

    rtc_current.year = 2200;
    rtc_current.month = 1;
    rtc_current.day = 1;
    assert(rdx_clock_read(&snapshot) == -1);
}

static void test_write_converts_and_normalizes_result(void)
{
    reset_rtc();
    assert(rdx_clock_write(1704067200u) == 0);
    assert(rtc_write_count == 1);
    assert(rtc_last_write.year == 2024);
    assert(rtc_last_write.month == 1);
    assert(rtc_last_write.day == 1);

    rtc_write_result = 7;
    assert(rdx_clock_write(1704067201u) == -1);
    assert(rtc_write_count == 2);
}

int main(void)
{
    test_read_normalizes_sdk_calendar();
    test_read_rejects_invalid_or_unrepresentable_calendar();
    test_write_converts_and_normalizes_result();
    puts("RDX BR56 clock adapter tests passed");
    return 0;
}
