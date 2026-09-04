#ifndef RDX_DIAGNOSTIC_CONFIG_H
#define RDX_DIAGNOSTIC_CONFIG_H

/*
 * 本项目 RDX 模块的统一诊断配置入口。
 *
 * 本头文件只管理不会改变协议结果和业务语义的可裁剪观测能力。RDX Ability、
 * 存储策略、队列深度、写入批量及超时等运行策略仍由
 * rdx_integration_config.h 管理；JL 公共 RAM/Flash、任务、异常回溯和内存分配
 * 追踪仍由 resource_monitor.h 管理。
 */
#ifndef TCFG_RDX_ENABLE
#error "rdx_diagnostic_config.h requires rdx_integration_config.h first"
#endif

#if TCFG_RDX_ENABLE

#ifndef TCFG_PRODUCT_DIAGNOSTIC_BUILD_ENABLE
#error "RDX diagnostics require the product diagnostic build policy"
#endif

/* 普通 RDX 诊断集合的总开关，默认跟随产品诊断镜像。 */
#ifndef RDX_CFG_DIAGNOSTIC_ENABLE
#define RDX_CFG_DIAGNOSTIC_ENABLE  \
    TCFG_PRODUCT_DIAGNOSTIC_BUILD_ENABLE
#endif

/* 本地存储事件快照、FAT 时延统计及公共资源快照触发。 */
#ifndef RDX_CFG_LOCAL_STORAGE_O0_DIAG_ENABLE
#define RDX_CFG_LOCAL_STORAGE_O0_DIAG_ENABLE  \
    RDX_CFG_DIAGNOSTIC_ENABLE
#endif

/* Record Engine 唤醒、合并及投递失败统计。 */
#ifndef RDX_CFG_RECORD_PERF_DIAG_ENABLE
#define RDX_CFG_RECORD_PERF_DIAG_ENABLE  RDX_CFG_DIAGNOSTIC_ENABLE
#endif

/* BLE ATT/TX 队列、背压及 can-send 详细统计。 */
#ifndef RDX_CFG_BLE_TX_DIAG_ENABLE
#define RDX_CFG_BLE_TX_DIAG_ENABLE  RDX_CFG_DIAGNOSTIC_ENABLE
#endif

/* 产品按键所有权与路由明细。 */
#ifndef RDX_CFG_KEY_DIAG_ENABLE
#define RDX_CFG_KEY_DIAG_ENABLE  RDX_CFG_DIAGNOSTIC_ENABLE
#endif

/*
 * 完整 AppKey、AuthKey、BLE MAC 与 Label SN 属于敏感数据。
 * 即使普通诊断镜像已开启，也必须为受控联调单独显式开启。
 */
#ifndef RDX_CFG_SENSITIVE_DATA_DIAG_ENABLE
#define RDX_CFG_SENSITIVE_DATA_DIAG_ENABLE  0
#endif

#else

#ifndef RDX_CFG_DIAGNOSTIC_ENABLE
#define RDX_CFG_DIAGNOSTIC_ENABLE                 0
#endif
#ifndef RDX_CFG_LOCAL_STORAGE_O0_DIAG_ENABLE
#define RDX_CFG_LOCAL_STORAGE_O0_DIAG_ENABLE      0
#endif
#ifndef RDX_CFG_RECORD_PERF_DIAG_ENABLE
#define RDX_CFG_RECORD_PERF_DIAG_ENABLE           0
#endif
#ifndef RDX_CFG_BLE_TX_DIAG_ENABLE
#define RDX_CFG_BLE_TX_DIAG_ENABLE                0
#endif
#ifndef RDX_CFG_KEY_DIAG_ENABLE
#define RDX_CFG_KEY_DIAG_ENABLE                   0
#endif
#ifndef RDX_CFG_SENSITIVE_DATA_DIAG_ENABLE
#define RDX_CFG_SENSITIVE_DATA_DIAG_ENABLE        0
#endif

#endif

#if (RDX_CFG_DIAGNOSTIC_ENABLE != 0) && \
    (RDX_CFG_DIAGNOSTIC_ENABLE != 1)
#error "RDX diagnostics policy must be 0 or 1"
#endif
#if (RDX_CFG_LOCAL_STORAGE_O0_DIAG_ENABLE != 0) && \
    (RDX_CFG_LOCAL_STORAGE_O0_DIAG_ENABLE != 1)
#error "RDX Local Storage diagnostics policy must be 0 or 1"
#endif
#if (RDX_CFG_RECORD_PERF_DIAG_ENABLE != 0) && \
    (RDX_CFG_RECORD_PERF_DIAG_ENABLE != 1)
#error "RDX Record performance diagnostics policy must be 0 or 1"
#endif
#if (RDX_CFG_BLE_TX_DIAG_ENABLE != 0) && \
    (RDX_CFG_BLE_TX_DIAG_ENABLE != 1)
#error "RDX BLE TX diagnostics policy must be 0 or 1"
#endif
#if (RDX_CFG_KEY_DIAG_ENABLE != 0) && \
    (RDX_CFG_KEY_DIAG_ENABLE != 1)
#error "RDX key diagnostics policy must be 0 or 1"
#endif
#if (RDX_CFG_SENSITIVE_DATA_DIAG_ENABLE != 0) && \
    (RDX_CFG_SENSITIVE_DATA_DIAG_ENABLE != 1)
#error "RDX sensitive-data diagnostics policy must be 0 or 1"
#endif

#if !TCFG_RDX_ENABLE && \
    (RDX_CFG_DIAGNOSTIC_ENABLE || \
     RDX_CFG_LOCAL_STORAGE_O0_DIAG_ENABLE || \
     RDX_CFG_RECORD_PERF_DIAG_ENABLE || \
     RDX_CFG_BLE_TX_DIAG_ENABLE || \
     RDX_CFG_KEY_DIAG_ENABLE || \
     RDX_CFG_SENSITIVE_DATA_DIAG_ENABLE)
#error "RDX diagnostics require TCFG_RDX_ENABLE"
#endif

#endif /* RDX_DIAGNOSTIC_CONFIG_H */
