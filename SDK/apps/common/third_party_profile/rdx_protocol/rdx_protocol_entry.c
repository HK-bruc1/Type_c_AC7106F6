#include "app_config.h"

#if TCFG_RDX_ENABLE

#include "system/includes.h"
#include "rdx_protocol_entry.h"
#include "rdx_ble_transport_br56.h"
#include "rdx_device_state.h"
#include "rdx_identity.h"
#include "rdx_mvp0_protocol.h"
#if RDX_CFG_CONFERENCE_RECORDING_ENABLE
#include "rdx_record_engine.h"
#endif
#include "system/init.h"
#if RDX_CFG_RTC_ENABLE
#include "rdx_rtc.h"
#endif
#if TCFG_RDX_RESOURCE_MONITOR_ENABLE
#include "rdx_resource_monitor.h"
#endif

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
#if RDX_CFG_RTC_ENABLE
    ret = rdx_rtc_init();
    if (ret) {
        rdx_device_state_exit();
        return ret;
    }
#endif
    ret = rdx_mvp0_protocol_init(rdx_ble_transport_send,
                                 rdx_ble_transport_disconnect);
    if (ret) {
        rdx_device_state_exit();
        return ret;
    }
    ret = rdx_ble_transport_init();
    if (ret) {
        rdx_mvp0_protocol_exit();
#if RDX_CFG_CONFERENCE_RECORDING_ENABLE
        rdx_record_engine_shutdown(100);
#endif
        rdx_device_state_exit();
        return ret;
    }

    rdx_protocol_started = 1;
#if TCFG_RDX_RESOURCE_MONITOR_ENABLE
    rdx_resource_monitor_start();
#endif
    printf("[RDX] MVP0 started\n");
    return 0;
}

void rdx_protocol_stop(void)
{
    if (!rdx_protocol_started) {
        return;
    }

#if TCFG_RDX_RESOURCE_MONITOR_ENABLE
    rdx_resource_monitor_stop();
#endif
#if RDX_CFG_RTC_ENABLE
    rdx_rtc_store_backup();
#endif
    rdx_mvp0_protocol_exit();
#if RDX_CFG_CONFERENCE_RECORDING_ENABLE
    if (rdx_record_engine_shutdown(100)) {
        printf("[RDX] record engine shutdown incomplete\n");
        return;
    }
#endif
    rdx_ble_transport_exit();
    rdx_device_state_exit();
    rdx_protocol_started = 0;
    printf("[RDX] MVP0 stopped\n");
}

static void rdx_protocol_poweroff_backup(void)
{
    if (rdx_protocol_started) {
#if TCFG_RDX_RESOURCE_MONITOR_ENABLE
        rdx_resource_monitor_stop();
#endif
#if RDX_CFG_RTC_ENABLE
        rdx_rtc_store_backup();
#endif
    }
}

platform_uninitcall(rdx_protocol_poweroff_backup);

#endif
