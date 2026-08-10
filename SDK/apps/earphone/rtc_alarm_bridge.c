#include "app_config.h"

#if TCFG_APP_RTC_EN

#include "gpio_config.h"
#include "os/os_api.h"
#include "power/power_wakeup.h"
#include "rtc/rtc_dev.h"
#include "rtc_alarm_bridge.h"

static volatile u8 rtc_alarm_pending;
static volatile u8 rtc_alarm_task_posted;
static volatile u8 rtc_alarm_reason;
static volatile u8 rtc_alarm_wakeup_replayed;

__attribute__((weak))
void rtc_alarm_event_notify(u8 reason,
                            const struct sys_time *fired_time,
                            const struct sys_time *alarm_time)
{
    (void)reason;
    (void)fired_time;
    (void)alarm_time;
}

static void rtc_alarm_handle_in_task(void *priv)
{
    struct sys_time fired_time = {0};
    struct sys_time alarm_time = {0};
    u8 reason = (u8)(u32)priv;

    /* The product contract treats the hardware alarm as one-shot. */
    rtc_alarm_en(0);
    rtc_read_time(&fired_time);
    rtc_read_alarm(&alarm_time);

    rtc_alarm_pending = 0;
    rtc_alarm_task_posted = 0;
    rtc_alarm_event_notify(reason, &fired_time, &alarm_time);
}

static int rtc_alarm_post_to_app_core(u8 reason)
{
    int msg[3];
    int ret;

    if (rtc_alarm_task_posted) {
        return 0;
    }

    rtc_alarm_task_posted = 1;
    msg[0] = (int)rtc_alarm_handle_in_task;
    msg[1] = 1;
    msg[2] = reason;
    ret = os_taskq_post_type("app_core", Q_CALLBACK, 3, msg);
    if (ret) {
        rtc_alarm_task_posted = 0;
    }
    return ret;
}

void rtc_alarm_isr_callback(const struct rtc_event_t *event)
{
    u8 reason;

    if (!event || (event->event != RTC_ALARM_EVENT
                   && event->event != RTC_WKUP_EVENT)) {
        return;
    }
    reason = (u8)event->event;
    rtc_alarm_reason = reason;
    rtc_alarm_pending = 1;
    rtc_alarm_post_to_app_core(reason);
}

void rtc_alarm_wakeup_event_replay(void)
{
    /*
     * rtc_wakup_source() is the low-level IRQ entry in this BR56 RTC
     * library.  It ends with an interrupt return and must never be called
     * from app_core.  If the IRQ arrived before app_core was ready, the
     * callback above has already latched the event in rtc_alarm_pending.
     * A cold softoff return may clear BSS before the callback can latch it,
     * so recover that case from the retained hardware wake source.
     */
    if (!rtc_alarm_wakeup_replayed
        && is_wakeup_source(PWR_RTC_WK_REASON_ALM)) {
        rtc_alarm_wakeup_replayed = 1;
        if (!rtc_alarm_pending) {
            rtc_alarm_reason = RTC_WKUP_EVENT;
            rtc_alarm_pending = 1;
        }
    }
    if (rtc_alarm_pending && !rtc_alarm_task_posted) {
        rtc_alarm_post_to_app_core(rtc_alarm_reason);
    }
}

#endif /* TCFG_APP_RTC_EN */
