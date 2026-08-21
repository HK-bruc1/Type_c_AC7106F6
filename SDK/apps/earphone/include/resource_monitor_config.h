#ifndef RESOURCE_MONITOR_CONFIG_H
#define RESOURCE_MONITOR_CONFIG_H

/* Product diagnostics maintained outside the tool-generated sdk_config.h. */
#define TCFG_RESOURCE_MONITOR_ENABLE    1

#if TCFG_RESOURCE_MONITOR_ENABLE
#define RESOURCE_MONITOR_PERIOD_MS      (10 * 1000UL)

#if (RESOURCE_MONITOR_PERIOD_MS == 0)
#error "Resource monitor period must be greater than zero"
#endif
#endif

#endif /* RESOURCE_MONITOR_CONFIG_H */
