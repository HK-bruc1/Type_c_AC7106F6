#include "app_config.h"

#if TCFG_RDX_ENABLE && RDX_CFG_RTC_ENABLE

#include "system/includes.h"
#include "rtc/rtc_dev.h"
#include "rdx_clock.h"

extern const struct sys_time def_sys_time;

static int rdx_clock_calendar_valid(const struct sys_time *time)
{
    u32 max_day;

    if (!time || time->year < 1970
        || time->month < 1 || time->month > 12) {
        return 0;
    }

    max_day = days_in_month(time->year, time->month);
    return time->day >= 1 && time->day <= max_day
           && time->hour <= 23 && time->min <= 59 && time->sec <= 59;
}

static int rdx_clock_timestamp_to_time(uint32_t timestamp,
                                       struct sys_time *time)
{
    u64 round_trip;

    if (!time) {
        return -1;
    }

    memset(time, 0, sizeof(*time));
    timestamp_to_datetime(timestamp, time);
    if (!rdx_clock_calendar_valid(time)) {
        return -1;
    }

    round_trip = datetime_to_timestamp(time);
    return round_trip == timestamp ? 0 : -1;
}

int rdx_clock_read(struct rdx_clock_snapshot *out)
{
    struct sys_time current;
    u64 timestamp;

    if (!out) {
        return -1;
    }

    memset(&current, 0, sizeof(current));
    rtc_read_time(&current);
    if (!rdx_clock_calendar_valid(&current)) {
        return -1;
    }

    timestamp = datetime_to_timestamp(&current);
    if (timestamp > UINT32_MAX
        || rdx_clock_timestamp_to_time((uint32_t)timestamp, &current)) {
        return -1;
    }

    out->timestamp = (uint32_t)timestamp;
    out->is_placeholder = current.year == def_sys_time.year
                          && current.month == def_sys_time.month
                          && current.day == def_sys_time.day;
    return 0;
}

int rdx_clock_write(uint32_t timestamp)
{
    struct sys_time set_time;

    if (rdx_clock_timestamp_to_time(timestamp, &set_time)) {
        return -1;
    }

    return rtc_write_time(&set_time) == 0 ? 0 : -1;
}

#endif /* TCFG_RDX_ENABLE && RDX_CFG_RTC_ENABLE */
