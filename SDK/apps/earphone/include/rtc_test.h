#ifndef RTC_TEST_H
#define RTC_TEST_H

#include "app_config.h"
#include "typedef.h"

struct rtc_event_t;

#define RTC_TEST_COMPILED   (TCFG_APP_RTC_EN && RTC_TEST_ENABLE)

#if RTC_TEST_COMPILED
void rtc_test_init(void);
void rtc_test_alarm_callback(const struct rtc_event_t *event);
#else
static inline void rtc_test_init(void)
{
}

static inline void rtc_test_alarm_callback(const struct rtc_event_t *event)
{
    (void)event;
}
#endif

#endif /* RTC_TEST_H */
