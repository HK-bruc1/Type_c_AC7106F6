#include "app_config.h"

#if TCFG_RDX_ENABLE

#include "system/includes.h"
#include "rdx_platform_br56.h"

int rdx_platform_get_auto_off_time_minutes(u32 *minutes)
{
    if (!minutes) {
        return -1;
    }

    /* Type-C USB Audio keeps automatic power-off disabled. */
    *minutes = 0;
    return 0;
}

#endif
