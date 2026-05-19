#ifndef __RTC_DEV_H__
#define __RTC_DEV_H__

#include "typedef.h"
#include "utils/sys_time.h"

enum RTC_CLK {
    CLK_SEL_32K = 1,
    CLK_SEL_BTOSC_DIV1 = 2,
    CLK_SEL_12M = 2,
    CLK_SEL_BTOSC_DIV2 = 3,
    CLK_SEL_24M = 3,
    CLK_SEL_LRC = 4,
};

enum RTC_SEL {
    HW_RTC,
    VIR_RTC,
};

enum {
    RTC_UNACCESSIBLE = 0,
    RTC_ACCESSIBLE_TIME_UNRELIABLE,
    RTC_ACCESSIBLE_TIME_RELIABLE,
};

struct rtc_dev_platform_data {
    const struct sys_time *default_sys_time;
    const struct sys_time *default_alarm;
    enum RTC_CLK rtc_clk;
    enum RTC_SEL rtc_sel;
    /* u8 clk_sel; */
    u8 x32xs;
    void (*cbfun)(u8);
};

struct _rtc_trim {
    u64 sfr_time_to_sec;
    u64 now_time_to_sec;
    u64 last_time_remain;
};
struct p11_sys_time {
    u32 mask;
    struct sys_time ram_time;
    struct _rtc_trim ram_lrc_trim;
};

void get_lrc_rtc_trim(struct _rtc_trim *lrc_trim);
int write_p11_sys_time(int param);
static void write_p11_sys_time_by_timer(void *priv);
bool read_p11_sys_time(struct sys_time *t, struct _rtc_trim *lrc_trim);
void p11_sys_time_init();



#define RTC_DEV_PLATFORM_DATA_BEGIN(data) \
	const struct rtc_dev_platform_data data = {

#define RTC_DEV_PLATFORM_DATA_END()  \
    .x32xs = 0, \
};

extern const struct device_operations rtc_dev_ops;
extern const u8 control_rtc_enable;
int rtc_init(const struct rtc_dev_platform_data *arg);
int rtc_ioctl(u32 cmd, u32 arg);
void rtc_wakup_source();
void set_alarm_ctrl(u32 set_alarm);
void write_sys_time(const struct sys_time *curr_time);
void read_sys_time(struct sys_time *curr_time);
void write_alarm(const struct sys_time *alarm_time);
void read_alarm(struct sys_time *alarm_time);

u32 month_to_day(u32 year, u32 month);
void day_to_ymd(u32 day, struct sys_time *sys_time);
u32 ymd_to_day(const struct sys_time *time);
u32 caculate_weekday_by_time(const struct sys_time *r_time);
u32 time_diff_for_sec(struct sys_time *old_time, struct sys_time *curr_time);
void time_add_sec(struct sys_time *time, s64 sec);
void copy_time(struct sys_time *old_time, struct sys_time *new_time);
u64 datetime_to_sec(const struct sys_time *curr_time);
void sec_to_datetime(u64 total_seconds, struct sys_time *curr_time);

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


struct rtc_dev_fun_cfg {
    void (*rtc_init)(const struct rtc_dev_platform_data *arg);
    void (*rtc_get_time)(struct sys_time *curr_time);
    void (*rtc_set_time)(const struct sys_time *curr_time);
    void (*rtc_get_alarm)(struct sys_time *alarm_time);
    void (*rtc_set_alarm)(const struct sys_time *alarm_time);
    void (*rtc_time_dump)(void);
    void (*rtc_alm_en)(u32 set_alarm);
    u32(*get_rtc_alm_en)(void);

};

void rtc_config_init(const struct rtc_dev_platform_data *arg);
void rtc_read_time(struct sys_time *curr_time);
void rtc_write_time(const struct sys_time *curr_time);
void rtc_read_alarm(struct sys_time *alarm_time);
void rtc_write_alarm(const struct sys_time *alarm_time);
void rtc_debug_dump(void);
void rtc_alarm_en(u32 set_alarm);
u32 rtc_get_alarm_en(void);

void poweroff_save_rtc_time();

#endif // __RTC_API_H__
