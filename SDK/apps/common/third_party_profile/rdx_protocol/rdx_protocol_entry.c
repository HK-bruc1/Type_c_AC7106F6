#include "app_config.h"

#if TCFG_RDX_ENABLE

#include "system/includes.h"
#include "rdx_protocol_entry.h"
#include "rdx_ble_transport_br56.h"
#include "rdx_device_state.h"
#include "rdx_identity.h"
#include "rdx_mvp0_protocol.h"
#include "system/init.h"
#include "rdx_rtc.h"

static u8 rdx_protocol_started;

int rdx_protocol_start(void)
{
    int ret;

    if (rdx_protocol_started) {
        return 0;
    }

    ret = rdx_device_state_init();
    if (ret) {
        return ret;
    }
    ret = rdx_identity_init();
    if (ret) {
        rdx_device_state_exit();
        return ret;
    }
    ret = rdx_rtc_init();
    if (ret) {
        rdx_device_state_exit();
        return ret;
    }
    rdx_mvp0_protocol_init(rdx_ble_transport_send, rdx_ble_transport_disconnect);
    ret = rdx_ble_transport_init();
    if (ret) {
        rdx_mvp0_protocol_exit();
        rdx_device_state_exit();
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

    rdx_rtc_store_backup();
    rdx_mvp0_protocol_exit();
    rdx_ble_transport_exit();
    rdx_device_state_exit();
    rdx_protocol_started = 0;
    printf("[RDX] MVP0 stopped\n");
}

static void rdx_protocol_poweroff_backup(void)
{
    if (rdx_protocol_started) {
        rdx_rtc_store_backup();
    }
}

platform_uninitcall(rdx_protocol_poweroff_backup);

#endif
