#include "app_config.h"

#if TCFG_RDX_ENABLE

#include "rdx_system_actions.h"

int rdx_system_action_get_auto_off_time_minutes(uint32_t *minutes)
{
    if (!minutes) {
        return -1;
    }

    /* Type-C USB Audio keeps automatic power-off disabled. */
    *minutes = 0;
    return 0;
}

#endif
