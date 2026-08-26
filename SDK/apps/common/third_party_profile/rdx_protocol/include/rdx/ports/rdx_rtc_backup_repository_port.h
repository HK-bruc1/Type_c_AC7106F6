#ifndef RDX_RTC_BACKUP_REPOSITORY_PORT_H
#define RDX_RTC_BACKUP_REPOSITORY_PORT_H

#include <stdint.h>

/* The selected adapter owns the VM ID, persisted layout and verification. */
int rdx_rtc_backup_repository_load(uint32_t *timestamp);
int rdx_rtc_backup_repository_save(uint32_t timestamp);

#endif /* RDX_RTC_BACKUP_REPOSITORY_PORT_H */
