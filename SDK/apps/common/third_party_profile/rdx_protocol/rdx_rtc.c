#include "app_config.h"

#if TCFG_RDX_ENABLE

#include "system/includes.h"
#include "user_cfg_id.h"
#include "rtc/rtc_dev.h"
#include "rdx_rtc.h"

static u8 rdx_rtc_trusted;

extern const struct sys_time def_sys_time;

static int rdx_rtc_time_valid(const struct sys_time *time)
{
    u32 max_day;

    if (!time || time->year < RDX_RTC_VALID_YEAR_MIN
        || time->year > RDX_RTC_VALID_YEAR_MAX
        || time->month < 1 || time->month > 12) {
        return 0;
    }

    max_day = days_in_month(time->year, time->month);
    return time->day >= 1 && time->day <= max_day
           && time->hour <= 23 && time->min <= 59 && time->sec <= 59;
}

static int rdx_rtc_timestamp_to_time(u32 timestamp, struct sys_time *time)
{
    u64 round_trip;

    if (!time) {
        return 0;
    }

    memset(time, 0, sizeof(*time));
    timestamp_to_datetime(timestamp, time);
    if (!rdx_rtc_time_valid(time)) {
        return 0;
    }

    round_trip = datetime_to_timestamp(time);
    return round_trip == timestamp;
}

static int rdx_rtc_read_hw(u32 *timestamp, struct sys_time *time)
{
    struct sys_time current;

    if (!timestamp) {
        return 0;
    }

    memset(&current, 0, sizeof(current));
    rtc_read_time(&current);
    if (!rdx_rtc_time_valid(&current)) {
        return 0;
    }

    *timestamp = (u32)datetime_to_timestamp(&current);
    if (time) {
        *time = current;
    }
    return rdx_rtc_timestamp_to_time(*timestamp, &current);
}

static int rdx_rtc_read_vm(u32 *timestamp)
{
    struct sys_time time;
    u32 value = 0;
    int ret;

    if (!timestamp) {
        return 0;
    }

    ret = syscfg_read(CFG_RDX_RTC_BACKUP, &value, sizeof(value));
    if (ret != sizeof(value) || !rdx_rtc_timestamp_to_time(value, &time)) {
        return 0;
    }

    *timestamp = value;
    return 1;
}

static int rdx_rtc_write_vm_verified(u32 timestamp)
{
    u32 read_back = 0;
    int ret;

    ret = syscfg_read(CFG_RDX_RTC_BACKUP, &read_back, sizeof(read_back));
    if (ret == sizeof(read_back) && read_back == timestamp) {
        return 1;
    }

    ret = syscfg_write(CFG_RDX_RTC_BACKUP, &timestamp, sizeof(timestamp));
    if (ret != sizeof(timestamp)) {
        printf("[RDX][RTC] vm_write_failed vm_id=%u ret=%d\n",
               CFG_RDX_RTC_BACKUP, ret);
        return 0;
    }

    ret = syscfg_read(CFG_RDX_RTC_BACKUP, &read_back, sizeof(read_back));
    if (ret != sizeof(read_back) || read_back != timestamp) {
        printf("[RDX][RTC] vm_verify_failed vm_id=%u ret=%d expected=%u read=%u\n",
               CFG_RDX_RTC_BACKUP, ret, timestamp, read_back);
        return 0;
    }
    return 1;
}

static int rdx_rtc_is_placeholder(const struct sys_time *time)
{
    return time && time->year == def_sys_time.year
           && time->month == def_sys_time.month
           && time->day == def_sys_time.day;
}

int rdx_rtc_init(void)
{
    struct sys_time current;
    u32 hardware_timestamp;
    u32 backup_timestamp;

    rdx_rtc_trusted = 0;
    if (rdx_rtc_read_hw(&hardware_timestamp, &current)
        && !rdx_rtc_is_placeholder(&current)) {
        rdx_rtc_trusted = 1;
        printf("[RDX][RTC] init source=hardware timestamp=%u\n",
               hardware_timestamp);
        return 0;
    }

    if (rdx_rtc_read_vm(&backup_timestamp)) {
        int ret = rdx_rtc_set_timestamp(backup_timestamp);

        printf("[RDX][RTC] init source=vm timestamp=%u ret=%d\n",
               backup_timestamp, ret);
        return 0;
    }

    printf("[RDX][RTC] init source=hardware_default backup=empty\n");
    return 0;
}

int rdx_rtc_set_timestamp(u32 timestamp)
{
    struct sys_time set_time;
    struct sys_time read_time;
    u32 read_timestamp = 0;
    u32 write_ret;
    int hardware_ok;
    int vm_ok;

    if (!rdx_rtc_timestamp_to_time(timestamp, &set_time)) {
        printf("[RDX][RTC] set rejected timestamp=%u reason=out_of_range\n",
               timestamp);
        return -1;
    }

    write_ret = rtc_write_time(&set_time);
    hardware_ok = write_ret == 0
                  && rdx_rtc_read_hw(&read_timestamp, &read_time)
                  && read_timestamp == timestamp;
    if (hardware_ok) {
        rdx_rtc_trusted = 1;
    }

    vm_ok = hardware_ok && rdx_rtc_write_vm_verified(timestamp);
    printf("[RDX][RTC] set timestamp=%u hw_write_ret=%u hw=%s vm=%s\n",
           timestamp, write_ret, hardware_ok ? "ok" : "failed",
           vm_ok ? "ok" : "failed");
    return hardware_ok && vm_ok ? 0 : -1;
}

int rdx_rtc_store_backup(void)
{
    struct sys_time current;
    u32 timestamp;

    if (!rdx_rtc_trusted || !rdx_rtc_read_hw(&timestamp, &current)) {
        printf("[RDX][RTC] backup skipped trusted=%u\n", rdx_rtc_trusted);
        return 0;
    }

    if (!rdx_rtc_write_vm_verified(timestamp)) {
        printf("[RDX][RTC] backup failed timestamp=%u\n", timestamp);
        return -1;
    }

    printf("[RDX][RTC] backup timestamp=%u\n", timestamp);
    return 0;
}

#endif /* TCFG_RDX_ENABLE */
