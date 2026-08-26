#include "app_config.h"

#if TCFG_RDX_ENABLE && RDX_CFG_RTC_ENABLE

#include "system/includes.h"
#include "user_cfg_id.h"
#include "rdx/ports/rdx_rtc_backup_repository_port.h"

int rdx_rtc_backup_repository_load(uint32_t *timestamp)
{
    uint32_t stored = 0;
    int ret;

    if (!timestamp) {
        return -1;
    }

    ret = syscfg_read(CFG_RDX_RTC_BACKUP, &stored, sizeof(stored));
    if (ret != sizeof(stored)) {
        return -1;
    }

    *timestamp = stored;
    return 0;
}

int rdx_rtc_backup_repository_save(uint32_t timestamp)
{
    uint32_t read_back = 0;
    int ret;

    ret = syscfg_read(CFG_RDX_RTC_BACKUP, &read_back, sizeof(read_back));
    if (ret == sizeof(read_back) && read_back == timestamp) {
        return 0;
    }

    ret = syscfg_write(CFG_RDX_RTC_BACKUP, &timestamp, sizeof(timestamp));
    if (ret != sizeof(timestamp)) {
        printf("[RDX][RTC_REPO] save=failed reason=write vm_id=%u vm_ret=%d\n",
               CFG_RDX_RTC_BACKUP, ret);
        return -1;
    }

    read_back = 0;
    ret = syscfg_read(CFG_RDX_RTC_BACKUP, &read_back, sizeof(read_back));
    if (ret != sizeof(read_back) || read_back != timestamp) {
        printf("[RDX][RTC_REPO] save=failed reason=verify vm_id=%u vm_ret=%d expected=%u read=%u\n",
               CFG_RDX_RTC_BACKUP, ret, timestamp, read_back);
        return -1;
    }

    return 0;
}

#endif /* TCFG_RDX_ENABLE && RDX_CFG_RTC_ENABLE */
