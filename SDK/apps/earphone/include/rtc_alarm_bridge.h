#ifndef RTC_ALARM_BRIDGE_H
#define RTC_ALARM_BRIDGE_H

#include "typedef.h"
#include "utils/sys_time.h"

struct rtc_event_t;

/* RTC driver callback. It may run in interrupt context. */
void rtc_alarm_isr_callback(const struct rtc_event_t *event);

/* Retry an alarm event that could not be queued during early startup. */
void rtc_alarm_wakeup_event_replay(void);

/* Product code may override this weak task-context notification hook. */
void rtc_alarm_event_notify(u8 reason,
                            const struct sys_time *fired_time,
                            const struct sys_time *alarm_time);

#endif /* RTC_ALARM_BRIDGE_H */
