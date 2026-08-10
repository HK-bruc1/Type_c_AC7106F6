#include "rtc_test.h"

#if RTC_TEST_COMPILED

#include "os/os_api.h"
#include "rtc/rtc_dev.h"
#include "system/generic/jiffies.h"
#include "system/timer.h"
#include "system/includes.h"
#include "driver/power/low_power.h"
#include "power/power_wakeup.h"

#if ((RTC_TEST_TIMING_SAMPLE_PERIOD_MS % 1000UL) != 0)
#error "RTC_TEST_TIMING_SAMPLE_PERIOD_MS must be a multiple of 1000"
#endif

#if (RTC_TEST_TIMING_SAMPLE_COUNT == 0)
#error "RTC_TEST_TIMING_SAMPLE_COUNT must be greater than zero"
#endif

#if ((RTC_TEST_ITEM_MASK & RTC_TEST_ITEM_ALARM) \
     && (RTC_TEST_ITEM_MASK & RTC_TEST_ITEM_SOFTOFF_WAKE))
#error "RTC alarm and softoff wake tests must run separately"
#endif

#define RTC_TEST_LOG(fmt, ...) \
    printf("[RTC_TEST] " fmt "\n", ##__VA_ARGS__)

static struct sys_time timing_start_time;
static struct sys_time expected_alarm_time;
static unsigned long timing_start_ms;
static u32 timing_sample_count;
static int timing_timer_id;
static volatile u8 alarm_task_posted;

static int rtc_test_item_enabled(u32 item)
{
    return (RTC_TEST_ITEM_MASK & item) != 0;
}

static u32 rtc_test_u32_abs_diff(u32 lhs, u32 rhs)
{
    return lhs >= rhs ? lhs - rhs : rhs - lhs;
}

static void rtc_test_print_time(const char *name, const struct sys_time *time)
{
    RTC_TEST_LOG("%s=%04u-%02u-%02u %02u:%02u:%02u",
                 name, time->year, time->month, time->day,
                 time->hour, time->min, time->sec);
}

static int rtc_test_time_valid(const struct sys_time *time)
{
    u32 days;

    if (time->year < 2000 || time->year > 2099
        || time->month < 1 || time->month > 12) {
        return 0;
    }
    days = month_to_day(time->year, time->month);
    if (time->day < 1 || time->day > days) {
        return 0;
    }
    return time->hour <= 23 && time->min <= 59 && time->sec <= 59;
}

static u32 rtc_test_abs_diff(const struct sys_time *lhs,
                             const struct sys_time *rhs)
{
    u64 lhs_sec = datetime_to_sec(lhs);
    u64 rhs_sec = datetime_to_sec(rhs);
    u64 diff = lhs_sec >= rhs_sec ? lhs_sec - rhs_sec : rhs_sec - lhs_sec;

    return diff > 0xffffffffULL ? 0xffffffffUL : (u32)diff;
}

static void rtc_test_snapshot(void)
{
    struct sys_time now = {0};

    rtc_read_time(&now);
    rtc_test_print_time("snapshot", &now);
    RTC_TEST_LOG("snapshot_result=%s",
                 rtc_test_time_valid(&now) ? "PASS" : "FAIL");
}

static void rtc_test_basic_read_write(void)
{
    struct sys_time set_time = {
        .year = 2025, .month = 6, .day = 15,
        .hour = 10, .min = 30, .sec = 0,
    };
    struct sys_time read_back = {0};
    u32 write_ret;
    u32 read_ret;
    u32 diff;

    write_ret = rtc_write_time(&set_time);
    read_ret = rtc_read_time(&read_back);
    diff = rtc_test_abs_diff(&set_time, &read_back);
    rtc_test_print_time("write", &set_time);
    rtc_test_print_time("read_back", &read_back);
    RTC_TEST_LOG("basic_rw_result=%s write_ret=%u read_ret=%u delta_sec=%u",
                 diff <= 1 ? "PASS" : "FAIL", write_ret, read_ret, diff);
}

static void rtc_test_timing_sample(void *priv)
{
    struct sys_time now = {0};
    u32 rtc_elapsed;
    u32 system_elapsed;
    u32 expected;
    u32 rtc_expected_diff;
    u32 system_expected_diff;
    u32 elapsed_diff;

    (void)priv;
    timing_sample_count++;
    rtc_read_time(&now);
    rtc_elapsed = time_diff_for_sec(&timing_start_time, &now);
    system_elapsed = (u32)(jiffies_msec() - timing_start_ms) / 1000;
    expected = timing_sample_count
               * (RTC_TEST_TIMING_SAMPLE_PERIOD_MS / 1000UL);
    RTC_TEST_LOG("timing_sample=%u/%u rtc=%u system=%u expected=%u",
                 timing_sample_count, RTC_TEST_TIMING_SAMPLE_COUNT,
                 rtc_elapsed, system_elapsed, expected);

    if (timing_sample_count >= RTC_TEST_TIMING_SAMPLE_COUNT) {
        rtc_expected_diff = rtc_test_u32_abs_diff(rtc_elapsed, expected);
        system_expected_diff = rtc_test_u32_abs_diff(system_elapsed, expected);
        elapsed_diff = rtc_test_u32_abs_diff(rtc_elapsed, system_elapsed);
        sys_timer_del(timing_timer_id);
        timing_timer_id = 0;
        RTC_TEST_LOG("timing_result=%s rtc_elapsed=%u system_elapsed=%u"
                     " expected=%u rtc_expected_delta_sec=%u"
                     " system_expected_delta_sec=%u delta_sec=%u tolerance_sec=%u",
                     rtc_elapsed && rtc_expected_diff <= RTC_TEST_TIMING_TOLERANCE_SEC
                     && system_expected_diff <= RTC_TEST_TIMING_TOLERANCE_SEC
                     && elapsed_diff <= RTC_TEST_TIMING_TOLERANCE_SEC
                     ? "PASS" : "FAIL",
                     rtc_elapsed, system_elapsed, expected,
                     rtc_expected_diff, system_expected_diff, elapsed_diff,
                     RTC_TEST_TIMING_TOLERANCE_SEC);
    }
}

static void rtc_test_start_timing(void)
{
    rtc_read_time(&timing_start_time);
    timing_start_ms = (unsigned long)jiffies_msec();
    timing_sample_count = 0;
    timing_timer_id = sys_timer_add(NULL, rtc_test_timing_sample,
                                    RTC_TEST_TIMING_SAMPLE_PERIOD_MS);
    RTC_TEST_LOG("timing_start timer_id=%d", timing_timer_id);
    if (!timing_timer_id) {
        RTC_TEST_LOG("timing_result=FAIL_TIMER_CREATE");
    }
}

static void rtc_test_alarm_in_task(void *priv)
{
    struct sys_time fired_time = {0};
    u8 reason = (u8)(u32)priv;
    u32 diff;

    rtc_alarm_en(0);
    rtc_read_time(&fired_time);
    diff = rtc_test_abs_diff(&expected_alarm_time, &fired_time);
    alarm_task_posted = 0;
    rtc_test_print_time("alarm_expected", &expected_alarm_time);
    rtc_test_print_time("alarm_fired", &fired_time);
    RTC_TEST_LOG("alarm_result=%s reason=0x%02x delta_sec=%u enabled=%u",
                 diff <= 1 ? "PASS" : "WARN",
                 reason, diff, rtc_get_alarm_en());
}

void rtc_test_alarm_callback(const struct rtc_event_t *event)
{
    int msg[3];
    u8 reason;

    if (!event || (event->event != RTC_ALARM_EVENT
                   && event->event != RTC_WKUP_EVENT)) {
        return;
    }
    reason = (u8)event->event;

    if (alarm_task_posted) {
        return;
    }

    alarm_task_posted = 1;
    msg[0] = (int)rtc_test_alarm_in_task;
    msg[1] = 1;
    msg[2] = reason;
    if (os_taskq_post_type("app_core", Q_CALLBACK, 3, msg)) {
        alarm_task_posted = 0;
    }
}

static void rtc_test_start_alarm(void *priv)
{
    struct sys_time now = {0};
    struct sys_time read_back = {0};
    u32 write_ret;
    u32 readback_diff;
    u32 enabled;
    int pass;

    (void)priv;
    rtc_read_time(&now);
    expected_alarm_time = now;
    time_add_sec(&expected_alarm_time, RTC_TEST_ALARM_DELAY_SEC);

    /* The BR56 LPTMR driver only accepts writes while alarm is enabled. */
    rtc_alarm_en(0);
    rtc_alarm_en(1);
    write_ret = rtc_write_alarm(&expected_alarm_time);
    rtc_read_alarm(&read_back);
    enabled = rtc_get_alarm_en();
    readback_diff = rtc_test_abs_diff(&expected_alarm_time, &read_back);
    pass = write_ret == RTC_SUCC && enabled && readback_diff == 0;

    rtc_test_print_time("alarm_now", &now);
    rtc_test_print_time("alarm_set", &expected_alarm_time);
    rtc_test_print_time("alarm_read_back", &read_back);
    RTC_TEST_LOG("alarm_program_result=%s write_ret=%u readback_delta_sec=%u enabled=%u",
                 pass ? "PASS" : "FAIL", write_ret, readback_diff, enabled);
    if (!pass) {
        rtc_alarm_en(0);
    }
}

static void rtc_test_calendar_utils(void)
{
    struct sys_time start = {
        .year = 2024, .month = 2, .day = 29,
        .hour = 23, .min = 59, .sec = 55,
    };
    struct sys_time result = start;
    struct sys_time round_trip = {0};
    u64 seconds;
    int pass;

    time_add_sec(&result, 10);
    seconds = datetime_to_sec(&result);
    sec_to_datetime(seconds, &round_trip);
    pass = result.year == 2024 && result.month == 3 && result.day == 1
           && result.hour == 0 && result.min == 0 && result.sec == 5
           && rtc_test_abs_diff(&result, &round_trip) == 0
           && month_to_day(2024, 2) == 29
           && month_to_day(2025, 2) == 28;
    rtc_test_print_time("utils_result_time", &result);
    RTC_TEST_LOG("utils_result=%s weekday=%u",
                 pass ? "PASS" : "FAIL",
                 caculate_weekday_by_time(&result));
}

static void rtc_test_enter_softoff(void *priv)
{
    (void)priv;
    RTC_TEST_LOG("softoff_enter: waiting for RTC alarm wake");
    power_set_soft_poweroff();
}

static void rtc_test_start_softoff_wakeup(void *priv)
{
    struct sys_time now = {0};
    struct sys_time alarm = {0};
    struct sys_time read_back = {0};
    u64 now_sec;
    u64 alarm_sec;
    u32 write_ret;
    u32 readback_diff;
    u32 enabled;
    int rtc_wakeup_source;
    u16 timer_id;

    (void)priv;

    rtc_read_time(&now);
    rtc_read_alarm(&alarm);
    now_sec = datetime_to_sec(&now);
    alarm_sec = rtc_test_time_valid(&alarm) ? datetime_to_sec(&alarm) : 0;
    rtc_wakeup_source = is_wakeup_source(PWR_RTC_WK_REASON_ALM);

    /* A RTC wake source plus an expired alarm proves this boot is the test wake. */
    if (rtc_wakeup_source && alarm_sec && alarm_sec <= now_sec + 5) {
        RTC_TEST_LOG("softoff_wakeup_result=PASS wake_source=RTC_ALARM");
        rtc_test_print_time("softoff_alarm", &alarm);
        rtc_test_print_time("softoff_wakeup_time", &now);
        rtc_alarm_en(0);
        return;
    }
    if (alarm_sec && alarm_sec <= now_sec + 5) {
        RTC_TEST_LOG("softoff_stale_alarm ignored wake_source=%d",
                     rtc_wakeup_source);
    }

    if (rtc_wakeup_source && !alarm_sec) {
        RTC_TEST_LOG("softoff_wakeup_result=FAIL wake_source=RTC_ALARM alarm_invalid");
        rtc_alarm_en(0);
        return;
    }

    alarm = now;
    time_add_sec(&alarm, RTC_TEST_SOFTOFF_ALARM_DELAY_SEC);

    rtc_alarm_en(0);
    rtc_alarm_en(1);
    write_ret = rtc_write_alarm(&alarm);
    rtc_read_alarm(&read_back);
    enabled = rtc_get_alarm_en();
    readback_diff = rtc_test_abs_diff(&alarm, &read_back);
    rtc_test_print_time("softoff_now", &now);
    rtc_test_print_time("softoff_alarm", &alarm);
    rtc_test_print_time("softoff_alarm_read_back", &read_back);
    RTC_TEST_LOG("softoff_program_result=%s write_ret=%u readback_delta_sec=%u enabled=%u",
                 write_ret == RTC_SUCC && enabled && readback_diff == 0
                 ? "PASS" : "FAIL",
                 write_ret, readback_diff, enabled);
    if (write_ret != RTC_SUCC || !enabled || readback_diff != 0) {
        rtc_alarm_en(0);
        return;
    }

    timer_id = sys_timeout_add(NULL, rtc_test_enter_softoff,
                               RTC_TEST_SOFTOFF_ENTER_DELAY_MS);
    RTC_TEST_LOG("softoff_armed delay_sec=%u timer_id=%u",
                 RTC_TEST_SOFTOFF_ALARM_DELAY_SEC, timer_id);
    if (!timer_id) {
        RTC_TEST_LOG("softoff_result=FAIL_TIMER_CREATE");
        rtc_alarm_en(0);
    }
}

void rtc_test_init(void)
{
    RTC_TEST_LOG("start mask=0x%08x clk=LRC",
                 (unsigned int)RTC_TEST_ITEM_MASK);

    if (rtc_test_item_enabled(RTC_TEST_ITEM_REG_DUMP)) {
        rtc_test_snapshot();
    }
    if (rtc_test_item_enabled(RTC_TEST_ITEM_BASIC_RW)) {
        rtc_test_basic_read_write();
    }
    if (rtc_test_item_enabled(RTC_TEST_ITEM_UTILS)) {
        rtc_test_calendar_utils();
    }
    if (rtc_test_item_enabled(RTC_TEST_ITEM_ALARM)) {
        int alarm_timer_id;

        alarm_timer_id = sys_timeout_add(NULL, rtc_test_start_alarm,
                                         RTC_TEST_ALARM_START_DELAY_MS);
        RTC_TEST_LOG("alarm_start_delay_ms=%u timer_id=%d",
                     RTC_TEST_ALARM_START_DELAY_MS, alarm_timer_id);
        if (!alarm_timer_id) {
            RTC_TEST_LOG("alarm_program_result=FAIL_TIMER_CREATE");
        }
    }
    if (rtc_test_item_enabled(RTC_TEST_ITEM_TIMING)) {
        rtc_test_start_timing();
    }
    if (rtc_test_item_enabled(RTC_TEST_ITEM_SOFTOFF_WAKE)) {
        int softoff_timer_id;

        softoff_timer_id = sys_timeout_add(NULL,
                                           rtc_test_start_softoff_wakeup,
                                           RTC_TEST_ALARM_START_DELAY_MS);
        RTC_TEST_LOG("softoff_start_delay_ms=%u timer_id=%d",
                     RTC_TEST_ALARM_START_DELAY_MS, softoff_timer_id);
    }
}

#endif /* RTC_TEST_COMPILED */
