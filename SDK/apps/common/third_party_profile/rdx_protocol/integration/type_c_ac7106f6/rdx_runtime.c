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
#if RDX_CFG_LOCAL_STORAGE_BUILD_ENABLE
#include "rdx/rdx_local_storage_service.h"
#if RDX_CFG_LOCAL_RECORDING_ENABLE
#include "rdx/rdx_record_coordinator.h"
#endif
#if RDX_STORAGE_BACKEND_NATIVE
#include "rdx_protocol/adapters/jl_br56/storage/rdx_storage_backend_sd0_jl_br56.h"
#elif RDX_STORAGE_BACKEND_LEGACY_UXFILE
#include "rdx_protocol/adapters/jl_br56/storage/rdx_storage_backend_legacy_uxfile.h"
#endif
#endif

#define RDX_RECORD_SHUTDOWN_TIMEOUT_MS          1000

static u8 rdx_runtime_started;
static u32 rdx_boot_structural_caps;

#if RDX_CFG_LOCAL_STORAGE_BUILD_ENABLE
static struct rdx_local_storage_service rdx_local_storage;
#if RDX_CFG_LOCAL_RECORDING_ENABLE
static struct rdx_record_coordinator rdx_record_coordinator;
#endif

static void rdx_runtime_local_storage_completion(
    void *priv, const struct rdx_local_storage_completion *completion)
{
    uint32_t artifact_high;
    uint32_t artifact_low;

    (void)priv;
    if (!completion) {
        return;
    }
    artifact_high = (uint32_t)(completion->artifact_id.value >> 32);
    artifact_low = (uint32_t)completion->artifact_id.value;
    if (completion->type
            == RDX_LOCAL_STORAGE_COMPLETION_ARTIFACT_COMMITTED
        || completion->type
            == RDX_LOCAL_STORAGE_COMPLETION_LEGACY_FILE_COMMITTED) {
        printf("[RDX][LOCAL] committed artifact=%08X%08X sn=%u"
               " produced=%u accepted=%u local=%u persisted=%u"
               " bytes_hi=%u bytes_lo=%u crc=%08X generation=%u\n",
               (unsigned int)artifact_high, (unsigned int)artifact_low,
               (unsigned int)completion->serial_number,
               (unsigned int)completion->produced_frame_count,
               (unsigned int)completion->destination_accepted_count,
               (unsigned int)completion->local_accepted_count,
               (unsigned int)completion->persisted_frame_count,
               (unsigned int)(completion->file_size >> 32),
               (unsigned int)completion->file_size,
               (unsigned int)completion->audio_crc,
               (unsigned int)completion->generation);
    } else {
        printf("[RDX][LOCAL] failed artifact=%08X%08X sn=%u"
               " result=%u cause=%u produced=%u local=%u persisted=%u"
               " rejected=%u dropped=%u\n",
               (unsigned int)artifact_high, (unsigned int)artifact_low,
               (unsigned int)completion->serial_number,
               completion->result, completion->cause,
               (unsigned int)completion->produced_frame_count,
               (unsigned int)completion->local_accepted_count,
               (unsigned int)completion->persisted_frame_count,
               (unsigned int)completion->rejected_frame_count,
               (unsigned int)completion->dropped_frame_count);
    }
}

static int rdx_runtime_check_storage_structure(void)
{
    int backend_ready = 0;

#if TCFG_PRODUCT_DIAGNOSTIC_BUILD_ENABLE
    printf("[RDX][O0] checkpoint=%u nonterminal_readback=%u"
           " local_only=%u diag=%u\n",
           (unsigned int)RDX_CFG_LOCAL_STORAGE_PERIODIC_CHECKPOINT_ENABLE,
           (unsigned int)RDX_CFG_LOCAL_STORAGE_NONTERMINAL_READBACK_ENABLE,
           (unsigned int)RDX_CFG_LOCAL_STORAGE_O0_LOCAL_ONLY,
           (unsigned int)RDX_CFG_LOCAL_STORAGE_O0_DIAG_ENABLE);
#endif
#if RDX_STORAGE_BACKEND_NATIVE
    backend_ready = rdx_storage_backend_sd0_jl_br56_init() == 0;
    if (!backend_ready) {
        /* sd0 is a runtime readiness condition, not a prerequisite for the
         * rest of RDX.  Keep the service registered so callers can report
         * Local Readiness=UNAVAILABLE and retry after media recovery. */
        printf("[RDX][CAP] local_storage=unavailable reason=sd0\n");
    }
    if (rdx_local_storage_service_init(
            &rdx_local_storage, RDX_LOCAL_STORAGE_BACKEND_NATIVE,
            rdx_storage_backend_sd0_jl_br56_media_ops(),
            rdx_storage_backend_sd0_jl_br56_native_ops(), NULL,
            rdx_runtime_local_storage_completion, NULL)) {
        printf("[RDX][CAP] boot=failed cap=local_storage reason=service\n");
        return -1;
    }
#elif RDX_STORAGE_BACKEND_LEGACY_UXFILE
    backend_ready = rdx_storage_backend_legacy_uxfile_init() == 0;
    if (!backend_ready) {
        printf("[RDX][CAP] local_storage=unavailable reason=legacy\n");
    }
    if (rdx_local_storage_service_init(
            &rdx_local_storage, RDX_LOCAL_STORAGE_BACKEND_LEGACY_UXFILE,
            rdx_storage_backend_legacy_uxfile_media_ops(), NULL,
            rdx_storage_backend_legacy_uxfile_ops(),
            rdx_runtime_local_storage_completion, NULL)) {
        printf("[RDX][CAP] boot=failed cap=local_storage reason=service\n");
        return -1;
    }
#endif
#if RDX_CFG_LOCAL_STORAGE_WIRE_ENABLE
    if (rdx_protocol_v23_local_storage_bind_service(&rdx_local_storage)) {
        printf("[RDX][CAP] boot=failed cap=local_storage_wire"
               " reason=bind\n");
        return -1;
    }
#endif
    if (!backend_ready) {
        rdx_local_storage.readiness = RDX_STORAGE_READINESS_UNAVAILABLE;
    }
#if RDX_STORAGE_BACKEND_NATIVE || RDX_STORAGE_BACKEND_LEGACY_UXFILE
#if RDX_STORAGE_BACKEND_NATIVE
    if (backend_ready) {
        enum rdx_storage_result recovery_result;

        recovery_result = rdx_local_storage_service_recover(
            &rdx_local_storage);
        if (recovery_result != RDX_STORAGE_RESULT_READY) {
            printf("[RDX][CAP] local_storage=recovery_required result=%u\n",
                   recovery_result);
        }
    }
#endif
#if RDX_CFG_LOCAL_RECORDING_ENABLE
    if (rdx_record_coordinator_init(
            &rdx_record_coordinator, &rdx_local_storage,
#if RDX_CFG_LOCAL_STORAGE_O0_LOCAL_ONLY
            NULL, NULL)
#else
            rdx_protocol_v23_record_online_destination_ops(), NULL)
#endif
        || rdx_protocol_v23_record_bind_coordinator(
               &rdx_record_coordinator)) {
        printf("[RDX][CAP] boot=failed cap=local_recording"
               " reason=coordinator\n");
#if RDX_CFG_LOCAL_STORAGE_WIRE_ENABLE
        rdx_protocol_v23_local_storage_bind_service(NULL);
#endif
        return -1;
    }
#endif
    rdx_boot_structural_caps |= RDX_CAP_LOCAL_STORAGE
#if RDX_STORAGE_BACKEND_NATIVE
                               | RDX_CAP_STORAGE_BACKEND_NATIVE
#else
                               | RDX_CAP_STORAGE_BACKEND_LEGACY_UXFILE
#endif
                               | RDX_CAP_STORAGE_SERVICE;
    return 0;
#else
    /* The integration header normally rejects this combination.  Keep this
     * branch defensive for build-system probes and do not take RDX down. */
    printf("[RDX][CAP] local_storage=unavailable reason=no_backend\n");
    return 0;
#endif
}
#endif

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
#if RDX_CFG_LOCAL_STORAGE_BUILD_ENABLE
    ret = rdx_runtime_check_storage_structure();
    if (ret) {
#if RDX_CFG_LOCAL_STORAGE_WIRE_ENABLE
        rdx_protocol_v23_local_storage_bind_service(NULL);
#endif
        rdx_local_storage_service_shutdown(
            &rdx_local_storage, RDX_RECORD_SHUTDOWN_TIMEOUT_MS);
        rdx_device_state_exit();
        rdx_boot_structural_caps = RDX_CAP_NONE;
        return ret;
    }
#endif
    ret = rdx_protocol_v23_init();
    if (ret) {
#if RDX_CFG_LOCAL_RECORDING_ENABLE
        rdx_protocol_v23_record_bind_coordinator(NULL);
#endif
#if RDX_CFG_LOCAL_STORAGE_BUILD_ENABLE
#if RDX_CFG_LOCAL_STORAGE_WIRE_ENABLE
        rdx_protocol_v23_local_storage_bind_service(NULL);
#endif
        rdx_local_storage_service_shutdown(
            &rdx_local_storage, RDX_RECORD_SHUTDOWN_TIMEOUT_MS);
#endif
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
#if RDX_CFG_LOCAL_STORAGE_BUILD_ENABLE
#if RDX_CFG_LOCAL_STORAGE_WIRE_ENABLE
        rdx_protocol_v23_local_storage_bind_service(NULL);
#endif
        rdx_local_storage_service_shutdown(
            &rdx_local_storage, RDX_RECORD_SHUTDOWN_TIMEOUT_MS);
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
#if RDX_CFG_LOCAL_STORAGE_BUILD_ENABLE
#if RDX_CFG_LOCAL_STORAGE_WIRE_ENABLE
        rdx_protocol_v23_local_storage_bind_service(NULL);
#endif
        rdx_local_storage_service_shutdown(
            &rdx_local_storage, RDX_RECORD_SHUTDOWN_TIMEOUT_MS);
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
#if RDX_CFG_ONLINE_RECORDING_ENABLE
    if (rdx_record_engine_shutdown(RDX_RECORD_SHUTDOWN_TIMEOUT_MS)) {
        printf("[RDX] record engine shutdown incomplete\n");
        /* Keep Protocol/Transport objects alive: the Engine callback may
         * still reference them until its worker reaches IDLE. */
        return;
    }
#endif
    /* Protocol record_exit only marks STOP_PENDING; the Engine must be
     * stopped while its callback context is still valid. */
    rdx_protocol_v23_exit();
#if RDX_CFG_LOCAL_STORAGE_WIRE_ENABLE
    rdx_protocol_v23_local_storage_bind_service(NULL);
#endif
#if RDX_CFG_LOCAL_RECORDING_ENABLE
    rdx_protocol_v23_record_bind_coordinator(NULL);
#endif
#if RDX_CFG_LOCAL_STORAGE_BUILD_ENABLE
    if (rdx_local_storage_service_shutdown(
            &rdx_local_storage, RDX_RECORD_SHUTDOWN_TIMEOUT_MS)) {
        printf("[RDX] local storage shutdown incomplete\n");
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
