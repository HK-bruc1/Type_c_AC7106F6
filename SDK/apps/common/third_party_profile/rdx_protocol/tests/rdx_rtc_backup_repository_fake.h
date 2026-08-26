#ifndef RDX_RTC_BACKUP_REPOSITORY_FAKE_H
#define RDX_RTC_BACKUP_REPOSITORY_FAKE_H

#include <stdint.h>

struct rdx_rtc_backup_repository_fake_state {
    unsigned int load_count;
    unsigned int save_count;
    uint32_t last_saved_timestamp;
};

void rdx_rtc_backup_repository_fake_reset(void);
void rdx_rtc_backup_repository_fake_set_load(int result,
                                             uint32_t timestamp);
void rdx_rtc_backup_repository_fake_set_save_result(int result);
const struct rdx_rtc_backup_repository_fake_state *
rdx_rtc_backup_repository_fake_get_state(void);

#endif /* RDX_RTC_BACKUP_REPOSITORY_FAKE_H */
