#ifndef RESOURCE_MONITOR_H
#define RESOURCE_MONITOR_H

/*
 * 本项目在 JL 平台上主动增加的公共资源监测模块，并非 JL SDK 原生模块。
 *
 * 公共监测配置与接口统一归口到本头文件。协议或业务专用的计数与追踪
 * （例如 RDX FAT、录音及 BLE TX 监测）仍由对应业务配置管理，不在此定义。
 */
#ifndef TCFG_PRODUCT_DIAGNOSTIC_BUILD_ENABLE
#define TCFG_PRODUCT_DIAGNOSTIC_BUILD_ENABLE              0
#endif

/* Flash/RAM 周期采样，以及按需触发的任务资源快照。 */
#ifndef TCFG_RESOURCE_MONITOR_ENABLE
#define TCFG_RESOURCE_MONITOR_ENABLE  \
    TCFG_PRODUCT_DIAGNOSTIC_BUILD_ENABLE
#endif
#ifndef RESOURCE_MONITOR_PERIOD_MS
#define RESOURCE_MONITOR_PERIOD_MS                       (10 * 1000UL)
#endif
#ifndef RESOURCE_MONITOR_TASK_SNAPSHOT_CAPACITY
#define RESOURCE_MONITOR_TASK_SNAPSHOT_CAPACITY          24
#endif
#ifndef RESOURCE_MONITOR_KEY_TASK_LIST
#define RESOURCE_MONITOR_KEY_TASK_LIST(EXACT, PREFIX)  \
    EXACT("rdx_record")                                \
    EXACT("rdx_storage")                               \
    EXACT("btstack")                                   \
    PREFIX("jlstream_")
#endif

/* 异常回溯属于本项目的公共平台诊断能力，不受 RDX 业务开关控制。 */
#ifndef TCFG_RESOURCE_MONITOR_EXCEPTION_BACKTRACE_ENABLE
#define TCFG_RESOURCE_MONITOR_EXCEPTION_BACKTRACE_ENABLE  \
    TCFG_PRODUCT_DIAGNOSTIC_BUILD_ENABLE
#endif

/*
 * 内存分配追踪会显著增加 RAM、CPU 与日志开销，因此即使构建诊断镜像，
 * 也必须单独显式开启。
 */
#ifndef RESOURCE_MONITOR_HEAP_TRACE_RECORDS
#define RESOURCE_MONITOR_HEAP_TRACE_RECORDS              0
#endif
#ifndef RESOURCE_MONITOR_HEAP_TRACE_LOW_WATER_BYTES
#define RESOURCE_MONITOR_HEAP_TRACE_LOW_WATER_BYTES      (20 * 1024)
#endif
#ifndef RESOURCE_MONITOR_HEAP_TRACE_POLL_PERIOD_MS
#define RESOURCE_MONITOR_HEAP_TRACE_POLL_PERIOD_MS       200
#endif
#ifndef RESOURCE_MONITOR_HEAP_TRACE_FULL_DUMP_POLLS
#define RESOURCE_MONITOR_HEAP_TRACE_FULL_DUMP_POLLS      5
#endif

#if (TCFG_PRODUCT_DIAGNOSTIC_BUILD_ENABLE != 0) && \
    (TCFG_PRODUCT_DIAGNOSTIC_BUILD_ENABLE != 1)
#error "Product diagnostic build policy must be 0 or 1"
#endif

#if (TCFG_RESOURCE_MONITOR_ENABLE != 0) && \
    (TCFG_RESOURCE_MONITOR_ENABLE != 1)
#error "Resource monitor policy must be 0 or 1"
#endif

#if (TCFG_RESOURCE_MONITOR_EXCEPTION_BACKTRACE_ENABLE != 0) && \
    (TCFG_RESOURCE_MONITOR_EXCEPTION_BACKTRACE_ENABLE != 1)
#error "Resource monitor exception backtrace policy must be 0 or 1"
#endif

#if (RESOURCE_MONITOR_PERIOD_MS == 0)
#error "Resource monitor period must be greater than zero"
#endif

#if (RESOURCE_MONITOR_TASK_SNAPSHOT_CAPACITY < 1) || \
    (RESOURCE_MONITOR_TASK_SNAPSHOT_CAPACITY > 64)
#error "Resource monitor task snapshot capacity must be between 1 and 64"
#endif

#if (RESOURCE_MONITOR_HEAP_TRACE_RECORDS != 0) && \
    ((RESOURCE_MONITOR_HEAP_TRACE_RECORDS < 16) || \
     (RESOURCE_MONITOR_HEAP_TRACE_RECORDS > 512))
#error "Resource monitor heap trace count must be 0 or between 16 and 512"
#endif

#if RESOURCE_MONITOR_HEAP_TRACE_RECORDS && \
    (RESOURCE_MONITOR_HEAP_TRACE_POLL_PERIOD_MS == 0)
#error "Resource monitor heap trace poll period must be greater than zero"
#endif

#if RESOURCE_MONITOR_HEAP_TRACE_RECORDS && \
    ((RESOURCE_MONITOR_HEAP_TRACE_FULL_DUMP_POLLS < 1) || \
     (RESOURCE_MONITOR_HEAP_TRACE_FULL_DUMP_POLLS > 255))
#error "Resource monitor heap trace full-dump polls must be between 1 and 255"
#endif

#ifndef __LD__
void resource_monitor_snapshot(const char *reason);
void resource_monitor_observe(void);
void resource_monitor_dump_key_tasks(const char *reason);
#endif

#endif /* RESOURCE_MONITOR_H */
