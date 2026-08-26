#ifndef RDX_RTC_SERVICE_H
#define RDX_RTC_SERVICE_H

#include <stdint.h>

/*
 * A zero return value means that the requested operation was completed and
 * verified by the RTC service.
 */
int rdx_rtc_init(void);
int rdx_rtc_set_timestamp(uint32_t timestamp);
int rdx_rtc_store_backup(void);

#endif /* RDX_RTC_SERVICE_H */
