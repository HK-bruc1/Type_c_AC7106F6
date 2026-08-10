#ifndef RTC_DEV_H
#define RTC_DEV_H

#include "typedef.h"
#include "utils/sys_time.h"

extern const bool control_rtc_enable;
extern const u32 control_rtc_clk_sel;

#define CLK_SEL_32K                         1
#define CLK_SEL_12M                         2
#define CLK_SEL_24M                         3
#define CLK_SEL_LRC                         4

enum {
    RTC_UNACCESSIBLE = 0,
    RTC_ACCESSIBLE_TIME_UNRELIABLE,
    RTC_ACCESSIBLE_TIME_RELIABLE,
    RTC_SUCC,
    RTC_ERROR_ALM_DIS,
    RTC_ERROR_ALM_NO_EFFECT,
};

enum rtc_event {
    RTC_ALARM_EVENT,
    RTC_WKUP_EVENT,
    RTC_1HZ_EVENT,
    RTC_TRIM_EVENT,
};

struct rtc_event_t {
    enum rtc_event event;
};

struct rtc_dev_platform_data {
    const struct sys_time *default_sys_time;
    void (*cbfun)(const struct rtc_event_t *event);
    u8 trim_interval;
};

#define RTC_DEV_PLATFORM_DATA_BEGIN(data) \
    const struct rtc_dev_platform_data data = {

#define RTC_DEV_PLATFORM_DATA_END() \
};

bool is_leap_year(u32 year);
u32 days_in_year(u32 year);
u32 days_in_month(u32 year, u32 month);
u64 datetime_to_timestamp(const struct sys_time *time);
u64 datetime_to_timestamp_us(const struct sys_time *time);
void timestamp_to_datetime(u64 total_seconds, struct sys_time *time);
u32 get_weekday_from_time(const struct sys_time *time);
s32 get_seconds_delta(const struct sys_time *old_time,
                      const struct sys_time *curr_time);
void datetime_add_seconds(struct sys_time *time, s32 sec);
void printf_datetime(const struct sys_time *time);
void day_to_ymd(u32 day, struct sys_time *time);
u32 ymd_to_day(const struct sys_time *time);

extern const struct device_operations rtc_dev_ops;

int rtc_port_pr_read(u32 port);
int rtc_port_pr_out(u32 port, u32 value);
int rtc_port_pr_dir(u32 port, u32 dir);
int rtc_port_pr_die(u32 port, u32 die);
int rtc_port_pr_pu(u32 port, u32 value);
int rtc_port_pr_pu1(u32 port, u32 value);
int rtc_port_pr_pd(u32 port, u32 value);
int rtc_port_pr_pd1(u32 port, u32 value);
int rtc_port_pr_hd0(u32 port, u32 value);
int rtc_port_pr_hd1(u32 port, u32 value);

int rtc_init(const struct rtc_dev_platform_data *arg);
u32 rtc_dev_deinit(void);
/* Low-level RTC IRQ entry. Do not invoke from task context. */
void rtc_wakup_source(void);
u32 rtc_read_time(struct sys_time *time);
u32 rtc_write_time(const struct sys_time *time);
u32 rtc_read_alarm(struct sys_time *time);
u32 rtc_write_alarm(const struct sys_time *time);
void rtc_debug_dump(void);
void rtc_alarm_switch(u32 en);
u32 rtc_is_alarm_en(void);
u32 rtc_is_soff_need_keep_clk(void);
void rtc_save_context_to_vm(void);
void rtc_lptmr_wakeup_enable(u32 wakeup_ms);
u64 rtc_sys_timestamp_us(void);

/* Compatibility names used by existing application and test code. */
static inline u32 month_to_day(u32 year, u32 month)
{
    return days_in_month(year, month);
}

static inline u32 caculate_weekday_by_time(const struct sys_time *time)
{
    return get_weekday_from_time(time);
}

static inline u32 time_diff_for_sec(struct sys_time *old_time,
                                    struct sys_time *curr_time)
{
    s32 delta = get_seconds_delta(old_time, curr_time);
    return delta < 0 ? 0 : (u32)delta;
}

static inline void time_add_sec(struct sys_time *time, s32 sec)
{
    datetime_add_seconds(time, sec);
}

static inline u64 datetime_to_sec(const struct sys_time *time)
{
    return datetime_to_timestamp(time);
}

static inline void sec_to_datetime(u64 seconds, struct sys_time *time)
{
    timestamp_to_datetime(seconds, time);
}

static inline void rtc_alarm_en(u32 en)
{
    rtc_alarm_switch(en);
}

static inline u32 rtc_get_alarm_en(void)
{
    return rtc_is_alarm_en();
}

static inline void poweroff_save_rtc_time(void)
{
    rtc_save_context_to_vm();
}

#endif /* RTC_DEV_H */
