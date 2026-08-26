#ifndef RDX_HOST_TEST_RTC_DEV_H
#define RDX_HOST_TEST_RTC_DEV_H

#include "system/includes.h"

struct sys_time {
    u16 year;
    u8 month;
    u8 day;
    u8 hour;
    u8 min;
    u8 sec;
};

u32 days_in_month(u32 year, u32 month);
u64 datetime_to_timestamp(const struct sys_time *time);
void timestamp_to_datetime(u64 total_seconds, struct sys_time *time);
u32 rtc_read_time(struct sys_time *time);
u32 rtc_write_time(const struct sys_time *time);

#endif /* RDX_HOST_TEST_RTC_DEV_H */
