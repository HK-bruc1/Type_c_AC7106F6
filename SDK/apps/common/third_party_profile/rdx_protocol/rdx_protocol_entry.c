#include "app_config.h"

#if TCFG_RDX_ENABLE

#include "system/includes.h"
#include "rdx_protocol_entry.h"
#include "rdx_ble_transport_br56.h"
#include "rdx_mvp0_core.h"

static u8 rdx_protocol_started;

int rdx_protocol_start(void)
{
    int ret;

    if (rdx_protocol_started) {
        return 0;
    }

    rdx_mvp0_core_init(rdx_ble_transport_send);
    ret = rdx_ble_transport_init();
    if (ret) {
        rdx_mvp0_core_exit();
        return ret;
    }

    rdx_protocol_started = 1;
    printf("[RDX] MVP0 started\n");
    return 0;
}

void rdx_protocol_stop(void)
{
    if (!rdx_protocol_started) {
        return;
    }

    rdx_ble_transport_exit();
    rdx_mvp0_core_exit();
    rdx_protocol_started = 0;
    printf("[RDX] MVP0 stopped\n");
}

#endif
