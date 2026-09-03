#ifndef RDX_INTEGRATION_CONFIG_H
#define RDX_INTEGRATION_CONFIG_H

/*
 * RDX product-integration policy.
 * Include this file from app_config.h only after the final BLE and application
 * mode configuration, including TCFG_TYPEC_EARPHONE_CASE, has been resolved.
 */
#ifndef TCFG_TYPEC_EARPHONE_CASE
#error "rdx_integration_config.h requires the final application configuration"
#endif

#if TCFG_THIRD_PARTY_PROTOCOLS_ENABLE && (TCFG_THIRD_PARTY_PROTOCOLS_SEL & RDX_EN)

#include "rdx_protocol/integration/type_c_ac7106f6/rdx_product_compat_profile.h"
#include "rdx/rdx_capability.h"
#include "rdx_protocol/adapters/jl_br56/rdx_capabilities_jl_br56.h"

/*
 * Internal compile gates are derived from the product's advertised Ability.
 * Product code must only edit RDX_COMPAT_DEVICE_ABILITY; these gates are not
 * independent configuration knobs.
 */
#define RDX_CFG_RTC_ENABLE                     \
    ((RDX_COMPAT_DEVICE_ABILITY & RDX_ABILITY_RTC) != 0)
#define RDX_CFG_CONFERENCE_RECORDING_ENABLE    \
    ((RDX_COMPAT_DEVICE_ABILITY &              \
      RDX_ABILITY_CONFERENCE_RECORDING) != 0)
#define RDX_CFG_CALL_RECORDING_ENABLE          \
    ((RDX_COMPAT_DEVICE_ABILITY &              \
      RDX_ABILITY_CALL_RECORDING) != 0)
#define RDX_CFG_ONLINE_RECORDING_ENABLE        \
    (RDX_CFG_CONFERENCE_RECORDING_ENABLE       \
     || RDX_CFG_CALL_RECORDING_ENABLE)
#define RDX_CFG_RECORD_PAUSE_RESUME_ENABLE     \
    ((RDX_COMPAT_DEVICE_ABILITY &              \
      RDX_ABILITY_RECORD_PAUSE_RESUME) != 0)
#define RDX_CFG_RECORD_MARK_ENABLE             \
    ((RDX_COMPAT_DEVICE_ABILITY & RDX_ABILITY_RECORD_MARK) != 0)

/* Local storage follows the Product Profile.  The Type-C Product Profile now
 * enables the formal Stage-D ability and selects Native; other products may
 * still opt in through their own profile, but this shared header never picks
 * a backend silently. */
#define RDX_CFG_LOCAL_STORAGE_ENABLE            \
    ((RDX_COMPAT_DEVICE_ABILITY & RDX_ABILITY_LOCAL_STORAGE) != 0)
#ifndef RDX_LOCAL_STORAGE_DEV_TEST_ENABLE
#define RDX_LOCAL_STORAGE_DEV_TEST_ENABLE       0
#endif
#ifndef RDX_LOCAL_STORAGE_WIRE_DEV_TEST_ENABLE
#define RDX_LOCAL_STORAGE_WIRE_DEV_TEST_ENABLE  0
#endif
#define RDX_CFG_LOCAL_STORAGE_BUILD_ENABLE      \
    (RDX_CFG_LOCAL_STORAGE_ENABLE || RDX_LOCAL_STORAGE_DEV_TEST_ENABLE)
#define RDX_CFG_LOCAL_STORAGE_WIRE_ENABLE       \
    (RDX_CFG_LOCAL_STORAGE_ENABLE || RDX_LOCAL_STORAGE_WIRE_DEV_TEST_ENABLE)

/* Stage-C APP Wire development depends on the build-only Service/Backend
 * gate so a Wire-only image can never compile handlers which have no storage
 * worker behind them. */
#if RDX_LOCAL_STORAGE_WIRE_DEV_TEST_ENABLE && \
    !RDX_LOCAL_STORAGE_DEV_TEST_ENABLE
#error "RDX Local Storage Wire development requires the storage dev build"
#endif
#if RDX_CFG_LOCAL_STORAGE_WIRE_ENABLE && \
    !RDX_CFG_LOCAL_STORAGE_BUILD_ENABLE
#error "RDX Local Storage Wire requires the Local Storage Service"
#endif

/* The Product Compatibility Profile is the release Ability.  During the
 * historical Stage-C bring-up a Wire dev image temporarily overlaid the Local
 * bit so the real APP could exercise the entry.  Stage D now carries the bit
 * in the profile itself; retain the overlay only for legacy dev test images. */
#if RDX_LOCAL_STORAGE_WIRE_DEV_TEST_ENABLE
#define RDX_CFG_ADVERTISED_DEVICE_ABILITY       \
    (RDX_COMPAT_DEVICE_ABILITY | RDX_ABILITY_LOCAL_STORAGE)
#else
#define RDX_CFG_ADVERTISED_DEVICE_ABILITY       RDX_COMPAT_DEVICE_ABILITY
#endif

#ifndef RDX_STORAGE_BACKEND_NATIVE
#define RDX_STORAGE_BACKEND_NATIVE              0
#endif
#ifndef RDX_STORAGE_BACKEND_LEGACY_UXFILE
#define RDX_STORAGE_BACKEND_LEGACY_UXFILE       0
#endif
#ifndef RDX_LIBRDX_UXFILE_LINKED
#define RDX_LIBRDX_UXFILE_LINKED                0
#endif
#ifndef RDX_LEGACY_UXFILE_ABI_READY
#define RDX_LEGACY_UXFILE_ABI_READY             0
#endif
#ifndef RDX_LEGACY_UXFILE_ADAPTER_IMPLEMENTED
#define RDX_LEGACY_UXFILE_ADAPTER_IMPLEMENTED   0
#endif

/* Local recording is the composition of the existing Online OPTIONAL
 * destination and the Native Local REQUIRED destination.  Derive this from
 * release/build capabilities so Stage-D production builds do not depend on
 * the historical Stage-C development gate. */
#define RDX_CFG_LOCAL_RECORDING_ENABLE          \
    (RDX_CFG_ONLINE_RECORDING_ENABLE &&         \
     RDX_CFG_LOCAL_STORAGE_BUILD_ENABLE &&      \
     RDX_STORAGE_BACKEND_NATIVE)

#if RDX_CFG_LOCAL_STORAGE_BUILD_ENABLE && \
    ((RDX_STORAGE_BACKEND_NATIVE + RDX_STORAGE_BACKEND_LEGACY_UXFILE) != 1)
#error "RDX local storage requires exactly one storage backend"
#endif
#if RDX_CFG_LOCAL_STORAGE_BUILD_ENABLE && \
    RDX_STORAGE_BACKEND_LEGACY_UXFILE && !RDX_LIBRDX_UXFILE_LINKED
#error "Legacy UXFILE backend requires an audited librdxApp UXFILE link"
#endif
#if RDX_CFG_LOCAL_STORAGE_BUILD_ENABLE && \
    RDX_STORAGE_BACKEND_LEGACY_UXFILE && !RDX_LEGACY_UXFILE_ABI_READY
#error "Legacy UXFILE backend requires an audited ABI adapter"
#endif
#if RDX_CFG_LOCAL_STORAGE_BUILD_ENABLE && \
    RDX_STORAGE_BACKEND_LEGACY_UXFILE && !RDX_LEGACY_UXFILE_ADAPTER_IMPLEMENTED
#error "Legacy UXFILE backend adapter is not implemented for this target"
#endif

#if RDX_CFG_LOCAL_STORAGE_BUILD_ENABLE
#if !TCFG_SD0_ENABLE
#error "RDX local storage requires the JL sd0 device"
#endif
#if !TCFG_DEV_MANAGER_ENABLE
#error "RDX local storage requires JL dev_manager"
#endif
#if !CONFIG_FATFS_ENABLE
#error "RDX local storage requires JL FAT/VFS"
#endif
#endif

/* Stage diagnostics can be disabled after BLE TX tuning is complete. */
#ifndef RDX_CFG_BLE_TX_DIAG_ENABLE
#define RDX_CFG_BLE_TX_DIAG_ENABLE             1
#endif

/* Key ownership and routing diagnostics can be disabled after bring-up. */
#ifndef RDX_CFG_KEY_DIAG_ENABLE
#define RDX_CFG_KEY_DIAG_ENABLE                1
#endif

/* Stage 1: wake the existing Record Engine task from ATT can-send events. */
#ifndef RDX_CFG_BLE_TX_CAN_SEND_ENABLE
#define RDX_CFG_BLE_TX_CAN_SEND_ENABLE         1
#endif
#ifndef RDX_CFG_BLE_TX_BURST_BUDGET
#define RDX_CFG_BLE_TX_BURST_BUDGET            4
#endif
#ifndef RDX_CFG_BLE_TX_QUEUE_DEPTH
#define RDX_CFG_BLE_TX_QUEUE_DEPTH             64
#endif
#ifndef RDX_CFG_BLE_TX_CAN_SEND_TIMEOUT_MS
#define RDX_CFG_BLE_TX_CAN_SEND_TIMEOUT_MS     100
#endif
#if RDX_CFG_BLE_TX_CAN_SEND_ENABLE && \
    ((RDX_CFG_BLE_TX_BURST_BUDGET < 1) || \
     (RDX_CFG_BLE_TX_BURST_BUDGET > 16))
#error "RDX BLE TX burst budget must be between 1 and 16"
#endif
#if RDX_CFG_BLE_TX_CAN_SEND_ENABLE && \
    ((RDX_CFG_BLE_TX_QUEUE_DEPTH < 16) || \
     (RDX_CFG_BLE_TX_QUEUE_DEPTH > 128))
#error "RDX BLE TX queue depth must be between 16 and 128"
#endif
#if RDX_CFG_BLE_TX_CAN_SEND_ENABLE && \
    ((RDX_CFG_BLE_TX_CAN_SEND_TIMEOUT_MS < 20) || \
     (RDX_CFG_BLE_TX_CAN_SEND_TIMEOUT_MS > 1000))
#error "RDX BLE TX can-send timeout must be between 20 and 1000 ms"
#endif

/* Reject product abilities that this Product Integration does not support. */
#define RDX_PRODUCT_SUPPORTED_ABILITY          \
    (RDX_ABILITY_RTC                          | \
     RDX_ABILITY_CONFERENCE_RECORDING         | \
     RDX_ABILITY_CALL_RECORDING               | \
     RDX_ABILITY_RECORD_PAUSE_RESUME           | \
     RDX_ABILITY_RECORD_MARK                   | \
     RDX_ABILITY_LOCAL_STORAGE)

#if (RDX_COMPAT_DEVICE_ABILITY & ~RDX_PRODUCT_SUPPORTED_ABILITY)
#error "RDX Device Ability contains a feature not implemented by this target"
#endif

/*
 * Required Capability is derived from Product Ability. Compiled Capability
 * describes the Adapter sources selected into this exact firmware build.
 */
#define RDX_REQUIRED_PORT_CAPS                 \
    RDX_REQUIRED_CAPS_FROM_ABILITY(RDX_COMPAT_DEVICE_ABILITY)

#if RDX_CFG_RTC_ENABLE
#define RDX_CFG_RTC_COMPILED_CAPS              RDX_CAP_RTC
#else
#define RDX_CFG_RTC_COMPILED_CAPS              RDX_CAP_NONE
#endif
#if RDX_CFG_CONFERENCE_RECORDING_ENABLE
#define RDX_CFG_MEETING_COMPILED_CAPS          RDX_CAP_MEETING_CAPTURE
#else
#define RDX_CFG_MEETING_COMPILED_CAPS          RDX_CAP_NONE
#endif
#if RDX_CFG_CALL_RECORDING_ENABLE
#define RDX_CFG_CALL_COMPILED_CAPS             RDX_CAP_CALL_CAPTURE
#else
#define RDX_CFG_CALL_COMPILED_CAPS             RDX_CAP_NONE
#endif
#if RDX_CFG_LOCAL_STORAGE_BUILD_ENABLE && RDX_STORAGE_BACKEND_NATIVE
#define RDX_CFG_LOCAL_STORAGE_COMPILED_CAPS    \
    (RDX_CAP_LOCAL_STORAGE | RDX_CAP_STORAGE_BACKEND_NATIVE | \
     RDX_CAP_STORAGE_SERVICE)
#elif RDX_CFG_LOCAL_STORAGE_BUILD_ENABLE && RDX_STORAGE_BACKEND_LEGACY_UXFILE
#define RDX_CFG_LOCAL_STORAGE_COMPILED_CAPS    \
    (RDX_CAP_LOCAL_STORAGE | RDX_CAP_STORAGE_BACKEND_LEGACY_UXFILE | \
     RDX_CAP_STORAGE_SERVICE)
#else
#define RDX_CFG_LOCAL_STORAGE_COMPILED_CAPS    RDX_CAP_NONE
#endif

#define RDX_PORT_COMPILED_CAPS                 \
    (RDX_REQUIRED_BASE_CAPS                   | \
     RDX_CFG_RTC_COMPILED_CAPS                | \
     RDX_CFG_MEETING_COMPILED_CAPS            | \
     RDX_CFG_CALL_COMPILED_CAPS                | \
     RDX_CFG_LOCAL_STORAGE_COMPILED_CAPS)

#if (RDX_REQUIRED_PORT_CAPS & ~RDX_JL_BR56_AVAILABLE_CAPS)
#error "RDX Product Ability requires a capability unavailable in JL BR56 Adapter"
#endif
#if (RDX_REQUIRED_PORT_CAPS & ~RDX_PORT_COMPILED_CAPS)
#error "RDX required capability source is not selected into this build"
#endif

#if TCFG_THIRD_PARTY_PROTOCOLS_SEL != RDX_EN
#error "RDX cannot coexist with other third-party protocols"
#endif

#if !TCFG_USER_BLE_ENABLE
#error "RDX requires final TCFG_USER_BLE_ENABLE"
#endif

#if !TCFG_TYPEC_EARPHONE_CASE
#error "RDX requires the Type-C dedicated PC application mode"
#endif

#if RDX_CFG_RTC_ENABLE && !TCFG_APP_RTC_EN
#error "RDX requires the on-chip hardware RTC"
#endif

#if RDX_CFG_RECORD_PAUSE_RESUME_ENABLE && \
    !RDX_CFG_ONLINE_RECORDING_ENABLE
#error "RDX pause/resume ability requires an online recording ability"
#endif

#if RDX_CFG_RECORD_MARK_ENABLE && !RDX_CFG_ONLINE_RECORDING_ENABLE
#error "RDX record mark ability requires an online recording ability"
#endif

/* RDX RTC calendar policy; products may override these before this include. */
#ifndef RDX_RTC_VALID_YEAR_MIN
#define RDX_RTC_VALID_YEAR_MIN     2000
#endif
#ifndef RDX_RTC_VALID_YEAR_MAX
#define RDX_RTC_VALID_YEAR_MAX     2099
#endif

#if (RDX_RTC_VALID_YEAR_MIN < 1970) || \
    (RDX_RTC_VALID_YEAR_MAX < RDX_RTC_VALID_YEAR_MIN)
#error "RDX RTC year range is invalid"
#endif

/* RDX recording consumes tool-generated nodes; never enable them here. */
#if RDX_CFG_CONFERENCE_RECORDING_ENABLE
#if !TCFG_ADC_NODE_ENABLE
#error "RDX recording requires TCFG_ADC_NODE_ENABLE"
#endif
#if !TCFG_AGC_NODE_ENABLE
#error "RDX recording requires TCFG_AGC_NODE_ENABLE"
#endif
#endif
#if RDX_CFG_ONLINE_RECORDING_ENABLE
#if !TCFG_ENCODER_NODE_ENABLE
#error "RDX recording requires TCFG_ENCODER_NODE_ENABLE"
#endif
#if !TCFG_AI_TX_NODE_ENABLE
#error "RDX recording requires TCFG_AI_TX_NODE_ENABLE"
#endif
#if !TCFG_ENC_OPUS_ENABLE
#error "RDX recording requires TCFG_ENC_OPUS_ENABLE"
#endif
#if (TCFG_ENCODER_CHANNEL_NUM != 1)
#error "RDX MEETING_V1 requires a mono Encoder"
#endif
#endif

#if RDX_CFG_CALL_RECORDING_ENABLE
#if !TCFG_SOURCE_DEV1_NODE_ENABLE
#error "RDX call recording requires TCFG_SOURCE_DEV1_NODE_ENABLE"
#endif
#if TCFG_USB_CALL_AUDIO_ENCODER_DIAG_ENABLE
#error "RDX call recording and the Encoder diagnostic cannot share SourceDev1"
#endif
#endif

#define TCFG_RDX_ENABLE            1
#else
#define TCFG_RDX_ENABLE            0
#define RDX_CFG_RTC_ENABLE                     0
#define RDX_CFG_CONFERENCE_RECORDING_ENABLE    0
#define RDX_CFG_CALL_RECORDING_ENABLE          0
#define RDX_CFG_ONLINE_RECORDING_ENABLE        0
#define RDX_CFG_RECORD_PAUSE_RESUME_ENABLE     0
#define RDX_CFG_RECORD_MARK_ENABLE             0
#define RDX_CFG_LOCAL_STORAGE_ENABLE            0
#define RDX_CFG_LOCAL_STORAGE_BUILD_ENABLE      0
#define RDX_CFG_LOCAL_STORAGE_WIRE_ENABLE       0
#define RDX_CFG_LOCAL_RECORDING_ENABLE          0
#define RDX_LOCAL_STORAGE_DEV_TEST_ENABLE       0
#define RDX_LOCAL_STORAGE_WIRE_DEV_TEST_ENABLE  0
#define RDX_STORAGE_BACKEND_NATIVE              0
#define RDX_STORAGE_BACKEND_LEGACY_UXFILE       0
#define RDX_LIBRDX_UXFILE_LINKED                0
#define RDX_LEGACY_UXFILE_ABI_READY             0
#define RDX_LEGACY_UXFILE_ADAPTER_IMPLEMENTED   0
#define RDX_CFG_LOCAL_STORAGE_COMPILED_CAPS     0
#define RDX_PRODUCT_SUPPORTED_ABILITY          0
#define RDX_REQUIRED_PORT_CAPS                 0
#define RDX_PORT_COMPILED_CAPS                 0
#define RDX_CFG_BLE_TX_DIAG_ENABLE             0
#define RDX_CFG_KEY_DIAG_ENABLE                0
#define RDX_CFG_BLE_TX_CAN_SEND_ENABLE         0
#define RDX_CFG_BLE_TX_BURST_BUDGET            1
#define RDX_CFG_BLE_TX_QUEUE_DEPTH             16
#define RDX_CFG_BLE_TX_CAN_SEND_TIMEOUT_MS     100
#endif

#endif
