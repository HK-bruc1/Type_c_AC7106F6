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

#include "rdx_mvp0_compat_config.h"

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
#define RDX_CFG_RECORD_PAUSE_RESUME_ENABLE     \
    ((RDX_COMPAT_DEVICE_ABILITY &              \
      RDX_ABILITY_RECORD_PAUSE_RESUME) != 0)
#define RDX_CFG_RECORD_MARK_ENABLE             \
    ((RDX_COMPAT_DEVICE_ABILITY & RDX_ABILITY_RECORD_MARK) != 0)

/* Reject advertised abilities that this target integration cannot provide. */
#define RDX_CFG_IMPLEMENTED_ABILITY            \
    (RDX_ABILITY_RTC                          | \
     RDX_ABILITY_CONFERENCE_RECORDING         | \
     RDX_ABILITY_RECORD_PAUSE_RESUME           | \
     RDX_ABILITY_RECORD_MARK)

#if (RDX_COMPAT_DEVICE_ABILITY & ~RDX_CFG_IMPLEMENTED_ABILITY)
#error "RDX Device Ability contains a feature not implemented by this target"
#endif

#if TCFG_THIRD_PARTY_PROTOCOLS_SEL != RDX_EN
#error "RDX BLE MVP0 cannot coexist with other third-party protocols"
#endif

#if !TCFG_USER_BLE_ENABLE
#error "RDX requires final TCFG_USER_BLE_ENABLE"
#endif

#if !TCFG_TYPEC_EARPHONE_CASE
#error "RDX BLE MVP0 requires the Type-C dedicated PC application mode"
#endif

#if RDX_CFG_RTC_ENABLE && !TCFG_APP_RTC_EN
#error "RDX requires the on-chip hardware RTC"
#endif

#if RDX_CFG_RECORD_PAUSE_RESUME_ENABLE && \
    !RDX_CFG_CONFERENCE_RECORDING_ENABLE
#error "RDX pause/resume ability requires conference recording ability"
#endif

#if RDX_CFG_RECORD_MARK_ENABLE && !RDX_CFG_CONFERENCE_RECORDING_ENABLE
#error "RDX record mark ability requires conference recording ability"
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

#define TCFG_RDX_ENABLE            1
#else
#define TCFG_RDX_ENABLE            0
#define RDX_CFG_RTC_ENABLE                     0
#define RDX_CFG_CONFERENCE_RECORDING_ENABLE    0
#define RDX_CFG_RECORD_PAUSE_RESUME_ENABLE     0
#define RDX_CFG_RECORD_MARK_ENABLE             0
#define RDX_CFG_IMPLEMENTED_ABILITY            0
#endif

#endif
