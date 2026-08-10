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

#if TCFG_THIRD_PARTY_PROTOCOLS_SEL != RDX_EN
#error "RDX BLE MVP0 cannot coexist with other third-party protocols"
#endif

#if !TCFG_USER_BLE_ENABLE
#error "RDX requires final TCFG_USER_BLE_ENABLE"
#endif

#if !TCFG_TYPEC_EARPHONE_CASE
#error "RDX BLE MVP0 requires the Type-C dedicated PC application mode"
#endif

#if !TCFG_APP_RTC_EN
#error "RDX requires the on-chip hardware RTC"
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

#ifndef TCFG_RDX_RESOURCE_MONITOR_ENABLE
/* Development diagnostics are enabled for this MVP product by default. */
#define TCFG_RDX_RESOURCE_MONITOR_ENABLE  1
#endif

#if TCFG_RDX_RESOURCE_MONITOR_ENABLE
#ifndef RDX_RESOURCE_MONITOR_PERIOD_MS
#define RDX_RESOURCE_MONITOR_PERIOD_MS    (10 * 1000UL)
#endif
#if (RDX_RESOURCE_MONITOR_PERIOD_MS == 0)
#error "RDX resource monitor period must be greater than zero"
#endif
#endif

#define TCFG_RDX_ENABLE            1
#else
#define TCFG_RDX_ENABLE            0
#endif

#endif
