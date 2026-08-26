#include "app_config.h"

#if TCFG_RDX_ENABLE

#include "system/includes.h"
#include "rdx/rdx_runtime.h"
#include "rdx/rdx_ble_name_service.h"
#include "rdx/rdx_device_state_service.h"
#include "rdx/rdx_identity_service.h"
#include "rdx_protocol/protocol/v23/rdx_protocol_v23.h"
#include "rdx/ports/rdx_transport_port.h"
#if RDX_CFG_ONLINE_RECORDING_ENABLE
#include "rdx/rdx_record_engine.h"
#endif
#include "system/init.h"
#if RDX_CFG_RTC_ENABLE
#include "rdx/rdx_rtc_service.h"
#endif

#define RDX_RECORD_SHUTDOWN_TIMEOUT_MS          1000

static u8 rdx_runtime_started;

int rdx_runtime_start(void)
{
    int ret;

    if (rdx_runtime_started) {
        return 0;
    }

    ret = rdx_device_state_init();
    if (ret) {
        return ret;
    }
    ret = rdx_ble_name_init();
    if (ret) {
        rdx_device_state_exit();
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
    ret = rdx_protocol_v23_init();
    if (ret) {
        rdx_device_state_exit();
        return ret;
    }
    ret = rdx_transport_init(rdx_protocol_v23_handle_transport_event,
                             NULL);
    if (ret) {
        rdx_protocol_v23_exit();
#if RDX_CFG_ONLINE_RECORDING_ENABLE
        rdx_record_engine_shutdown(RDX_RECORD_SHUTDOWN_TIMEOUT_MS);
#endif
        rdx_device_state_exit();
        return ret;
    }

    rdx_runtime_started = 1;
    printf("[RDX] started profile=V23\n");
    return 0;
}

void rdx_runtime_stop(void)
{
    if (!rdx_runtime_started) {
        return;
    }

#if RDX_CFG_RTC_ENABLE
    rdx_rtc_store_backup();
#endif
    rdx_protocol_v23_exit();
#if RDX_CFG_ONLINE_RECORDING_ENABLE
    if (rdx_record_engine_shutdown(RDX_RECORD_SHUTDOWN_TIMEOUT_MS)) {
        printf("[RDX] record engine shutdown incomplete\n");
        return;
    }
#endif
    rdx_transport_exit();
    rdx_device_state_exit();
    rdx_runtime_started = 0;
    printf("[RDX] stopped profile=V23\n");
}

static void rdx_protocol_poweroff_backup(void)
{
    if (rdx_runtime_started) {
#if RDX_CFG_RTC_ENABLE
        rdx_rtc_store_backup();
#endif
    }
}

platform_uninitcall(rdx_protocol_poweroff_backup);

#endif
