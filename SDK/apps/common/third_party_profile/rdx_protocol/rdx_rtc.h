#ifndef RDX_RTC_H
#define RDX_RTC_H

#include "typedef.h"

/*
 * RDX RTC uses the BR56 hardware RTC as its only time source.  A zero return
 * value means that the requested operation was completed and verified.
 */
int rdx_rtc_init(void);
int rdx_rtc_set_timestamp(u32 timestamp);
int rdx_rtc_store_backup(void);

#endif /* RDX_RTC_H */
