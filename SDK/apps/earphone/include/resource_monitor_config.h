#ifndef RESOURCE_MONITOR_CONFIG_H
#define RESOURCE_MONITOR_CONFIG_H

/*
 * Submission/release builds keep diagnostic instrumentation compiled out.
 * Set this umbrella switch to 1 for a troubleshooting image; individual
 * monitors can still be overridden below when a narrower image is required.
 */
#ifndef TCFG_PRODUCT_DIAGNOSTIC_BUILD_ENABLE
#define TCFG_PRODUCT_DIAGNOSTIC_BUILD_ENABLE  0
#endif

/* Product diagnostics maintained outside the tool-generated sdk_config.h. */
#ifndef TCFG_RESOURCE_MONITOR_ENABLE
#define TCFG_RESOURCE_MONITOR_ENABLE  TCFG_PRODUCT_DIAGNOSTIC_BUILD_ENABLE
#endif

#if (TCFG_PRODUCT_DIAGNOSTIC_BUILD_ENABLE != 0) && \
    (TCFG_PRODUCT_DIAGNOSTIC_BUILD_ENABLE != 1)
#error "Product diagnostic build policy must be 0 or 1"
#endif

#if (TCFG_RESOURCE_MONITOR_ENABLE != 0) && \
    (TCFG_RESOURCE_MONITOR_ENABLE != 1)
#error "Resource monitor policy must be 0 or 1"
#endif

#if TCFG_RESOURCE_MONITOR_ENABLE
#define RESOURCE_MONITOR_PERIOD_MS      (10 * 1000UL)

#if (RESOURCE_MONITOR_PERIOD_MS == 0)
#error "Resource monitor period must be greater than zero"
#endif
#endif

#endif /* RESOURCE_MONITOR_CONFIG_H */
