#include <string.h>

#include "rdx/ports/rdx_rtc_backup_repository_port.h"
#include "rdx_rtc_backup_repository_fake.h"

static struct rdx_rtc_backup_repository_fake_state fake_state;
static int fake_load_result;
static uint32_t fake_load_timestamp;
static int fake_save_result;

void rdx_rtc_backup_repository_fake_reset(void)
{
    memset(&fake_state, 0, sizeof(fake_state));
    fake_load_result = -1;
    fake_load_timestamp = 0;
    fake_save_result = 0;
}

void rdx_rtc_backup_repository_fake_set_load(int result,
                                             uint32_t timestamp)
{
    fake_load_result = result;
    fake_load_timestamp = timestamp;
}

void rdx_rtc_backup_repository_fake_set_save_result(int result)
{
    fake_save_result = result;
}

const struct rdx_rtc_backup_repository_fake_state *
rdx_rtc_backup_repository_fake_get_state(void)
{
    return &fake_state;
}

int rdx_rtc_backup_repository_load(uint32_t *timestamp)
{
    fake_state.load_count++;
    if (!timestamp || fake_load_result) {
        return -1;
    }
    *timestamp = fake_load_timestamp;
    return 0;
}

int rdx_rtc_backup_repository_save(uint32_t timestamp)
{
    fake_state.save_count++;
    fake_state.last_saved_timestamp = timestamp;
    return fake_save_result;
}
