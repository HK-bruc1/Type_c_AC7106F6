#include "app_config.h"

#if TCFG_RDX_ENABLE

#include "system/includes.h"
#include "rdx/rdx_runtime.h"
#include "rdx/rdx_capability.h"
#include "rdx/rdx_ble_name_service.h"
#include "rdx/rdx_device_state_service.h"
#include "rdx/rdx_identity_service.h"
#include "rdx_protocol/protocol/v23/rdx_protocol_v23.h"
#include "rdx/ports/rdx_transport_port.h"
#if RDX_CFG_ONLINE_RECORDING_ENABLE
#include "rdx/ports/rdx_audio_capture_port.h"
#include "rdx/rdx_record_engine.h"
#endif
#include "system/init.h"
#if RDX_CFG_RTC_ENABLE
#include "rdx/rdx_rtc_service.h"
#endif

#define RDX_RECORD_SHUTDOWN_TIMEOUT_MS          1000

static u8 rdx_runtime_started;
static u32 rdx_boot_structural_caps;

#if RDX_CFG_ONLINE_RECORDING_ENABLE
static int rdx_runtime_check_audio_structure(void)
{
    enum rdx_audio_capture_result result;

#if RDX_CFG_CONFERENCE_RECORDING_ENABLE
    result = rdx_audio_capture_check_structure(
        RDX_RECORD_SCENE_MEETING, RDX_RECORD_FORMAT_MEETING_V1);
    if (result != RDX_AUDIO_CAPTURE_OK) {
        printf("[RDX][CAP] boot=failed cap=meeting_capture result=%u\n",
               result);
        return -1;
    }
    rdx_boot_structural_caps |= RDX_CAP_MEETING_CAPTURE;
#endif
#if RDX_CFG_CALL_RECORDING_ENABLE
    result = rdx_audio_capture_check_structure(
        RDX_RECORD_SCENE_CALL, RDX_RECORD_FORMAT_CALL_V1);
    if (result != RDX_AUDIO_CAPTURE_OK) {
        printf("[RDX][CAP] boot=failed cap=call_capture result=%u\n",
               result);
        return -1;
    }
    rdx_boot_structural_caps |= RDX_CAP_CALL_CAPTURE;
#endif
    return 0;
}
#endif

int rdx_runtime_start(void)
{
    int ret;

    if (rdx_runtime_started) {
        return 0;
    }

    rdx_boot_structural_caps = RDX_CAP_NONE;
    ret = rdx_device_state_init();
    if (ret) {
        return ret;
    }
    rdx_boot_structural_caps |= RDX_CAP_DEVICE_STATE_REPOSITORY;
    ret = rdx_identity_init();
    if (ret) {
        rdx_device_state_exit();
        rdx_boot_structural_caps = RDX_CAP_NONE;
        return ret;
    }
    rdx_boot_structural_caps |= RDX_CAP_FACTORY_IDENTITY;
    rdx_boot_structural_caps |= RDX_CAP_DEVICE_IDENTITY;
    ret = rdx_ble_name_init();
    if (ret) {
        rdx_device_state_exit();
        rdx_boot_structural_caps = RDX_CAP_NONE;
        return ret;
    }
#if RDX_CFG_RTC_ENABLE
    ret = rdx_rtc_init();
    if (ret) {
        rdx_device_state_exit();
        rdx_boot_structural_caps = RDX_CAP_NONE;
        return ret;
    }
    rdx_boot_structural_caps |= RDX_CAP_RTC;
#endif
#if RDX_CFG_ONLINE_RECORDING_ENABLE
    ret = rdx_runtime_check_audio_structure();
    if (ret) {
        rdx_device_state_exit();
        rdx_boot_structural_caps = RDX_CAP_NONE;
        return ret;
    }
#endif
    ret = rdx_protocol_v23_init();
    if (ret) {
        rdx_device_state_exit();
        rdx_boot_structural_caps = RDX_CAP_NONE;
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
        rdx_boot_structural_caps = RDX_CAP_NONE;
        return ret;
    }
    rdx_boot_structural_caps |= RDX_CAP_TRANSPORT;
    if ((rdx_boot_structural_caps & RDX_REQUIRED_PORT_CAPS)
        != RDX_REQUIRED_PORT_CAPS) {
        printf("[RDX][CAP] boot=failed required=0x%x compiled=0x%x boot=0x%x\n",
               (unsigned int)RDX_REQUIRED_PORT_CAPS,
               (unsigned int)RDX_PORT_COMPILED_CAPS,
               (unsigned int)rdx_boot_structural_caps);
        rdx_protocol_v23_exit();
#if RDX_CFG_ONLINE_RECORDING_ENABLE
        rdx_record_engine_shutdown(RDX_RECORD_SHUTDOWN_TIMEOUT_MS);
#endif
        rdx_transport_exit();
        rdx_device_state_exit();
        rdx_boot_structural_caps = RDX_CAP_NONE;
        return -1;
    }

    rdx_runtime_started = 1;
    printf("[RDX] started profile=V23 required_caps=0x%x"
           " compiled_caps=0x%x boot_caps=0x%x\n",
           (unsigned int)RDX_REQUIRED_PORT_CAPS,
           (unsigned int)RDX_PORT_COMPILED_CAPS,
           (unsigned int)rdx_boot_structural_caps);
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
    rdx_boot_structural_caps = RDX_CAP_NONE;
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
