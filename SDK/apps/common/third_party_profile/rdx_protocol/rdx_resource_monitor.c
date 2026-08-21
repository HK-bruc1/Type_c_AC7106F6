/*
 * RDX 资源监测指标口径：
 *
 * 数据来源：
 * - memory_get_size()、get_boot_info() 和 sys_timer_*() 均为 JL SDK
 *   原生接口；本模块自行实现指标汇总、定时采样、最低值记录和日志输出。
 *
 * flash_code 日志字段：
 * - used：boot_info.vm.vm_saddr，即当前自动放置 VM 尾区的起始地址；
 *   在当前从 Flash 零偏移开始打包的布局中，表示 VM 前固件及资源占用空间。
 * - free：max(boot_info.vm.vm_size - TCFG_VM_SIZE * 1024, 0)，表示保留
 *   产品最小 VM 后，VM 前固件及资源还可增长的空间。
 * - capacity：used + free，表示保留最小 VM 后 VM 前区域的最大容量。
 * 上述字段不是整颗 Flash 的容量/空闲量，也不是 ELF 纯代码段大小；其含义
 * 依赖当前 VM 自动放置和从零偏移打包的产品布局。
 *
 * ram 日志字段：
 * - reason：采样原因；start 为监测启动，periodic 为周期采样，其他值由
 *   rdx_resource_monitor_snapshot() 的调用方传入，空指针显示 unknown。
 * - total：memory_get_size(P_HEAP_SIZE)，即分配器管理的 RAM0/可选 RAM1
 *   动态堆总容量；不是芯片 SRAM 总量，不包含链接期静态区和系统保留区。
 * - used：RAM0 与可选 RAM1 动态堆当前已分配空间之和。
 * - free：RAM0 与可选 RAM1 动态堆当前空闲空间之和；是空闲总量，不代表
 *   最大连续空闲块，不能据此保证同等大小的连续内存申请一定成功。
 * - min_free：本模块从本次开机首次采样以来观测到的最小 free；stop/start
 *   不会重置。它不是分配器维护的真实历史最低水位，可能漏掉采样间隔内的
 *   短时内存峰值。
 *
 * timer 日志字段：
 * - period_ms：周期采样间隔，单位毫秒，由 RDX_RESOURCE_MONITOR_PERIOD_MS
 *   配置，当前默认值为 10000 ms。
 *
 * 日志沿用后缀 KB，但计算方式为 bytes / 1024，并向下取整；数值实际表示
 * 整数 KiB。RAM 各字段由多次接口调用依次取得，并非同一原子快照。
 */
#include "app_config.h"

#if TCFG_RDX_ENABLE && TCFG_RDX_RESOURCE_MONITOR_ENABLE

#include "system/boot.h"
#include "system/timer.h"
#include "malloc.h"
#include "rdx_resource_monitor.h"

#define RDX_RESOURCE_BYTES_TO_KB(value)    ((u32)((value) / 1024))

static u16 rdx_resource_monitor_timer_id;
static u8 rdx_resource_monitor_running;
static u8 rdx_resource_monitor_flash_reported;
static u32 rdx_resource_monitor_min_ram_free = 0xffffffffUL;

static void rdx_resource_monitor_dump_flash_code(void)
{
    const BOOT_INFO *boot = get_boot_info();
    /* The tail VM area is reserved; its configured minimum is not code space. */
    u32 vm_reserved = TCFG_VM_SIZE * 1024;
    u32 code_used = boot->vm.vm_saddr;
    u32 code_free = boot->vm.vm_size > vm_reserved
                    ? boot->vm.vm_size - vm_reserved : 0;
    u32 code_capacity = code_used + code_free;

    printf("[RDX][RES] flash_code capacity=%uKB used=%uKB free=%uKB\n",
           RDX_RESOURCE_BYTES_TO_KB(code_capacity),
           RDX_RESOURCE_BYTES_TO_KB(code_used),
           RDX_RESOURCE_BYTES_TO_KB(code_free));
}

static void rdx_resource_monitor_dump_ram(const char *reason)
{
    u32 ram_total = (u32)memory_get_size(P_HEAP_SIZE);
    u32 ram_used = (u32)memory_get_size(P_MEMORY_USED)
                   + (u32)memory_get_size(P_VLT_MEMORY_USED);
    u32 ram_free = (u32)memory_get_size(P_MEMORY_UNUSED)
                   + (u32)memory_get_size(P_VLT_MEMORY_UNUSED);

    if (ram_free < rdx_resource_monitor_min_ram_free) {
        rdx_resource_monitor_min_ram_free = ram_free;
    }

    printf("[RDX][RES] ram reason=%s total=%uKB used=%uKB"
           " free=%uKB min_free=%uKB\n",
           reason ? reason : "unknown",
           RDX_RESOURCE_BYTES_TO_KB(ram_total),
           RDX_RESOURCE_BYTES_TO_KB(ram_used),
           RDX_RESOURCE_BYTES_TO_KB(ram_free),
           RDX_RESOURCE_BYTES_TO_KB(rdx_resource_monitor_min_ram_free));
}

void rdx_resource_monitor_snapshot(const char *reason)
{
    rdx_resource_monitor_dump_ram(reason);
}

static void rdx_resource_monitor_timer(void *priv)
{
    rdx_resource_monitor_dump_ram("periodic");
}

void rdx_resource_monitor_start(void)
{
    if (rdx_resource_monitor_running) {
        return;
    }

    if (!rdx_resource_monitor_flash_reported) {
        rdx_resource_monitor_dump_flash_code();
        rdx_resource_monitor_flash_reported = 1;
    }
    rdx_resource_monitor_dump_ram("start");

    rdx_resource_monitor_timer_id = sys_timer_add(NULL,
                                                   rdx_resource_monitor_timer,
                                                   RDX_RESOURCE_MONITOR_PERIOD_MS);
    if (!rdx_resource_monitor_timer_id) {
        printf("[RDX][RES] timer_start_failed period_ms=%u\n",
               (unsigned int)RDX_RESOURCE_MONITOR_PERIOD_MS);
        return;
    }

    rdx_resource_monitor_running = 1;
    printf("[RDX][RES] timer_started period_ms=%u\n",
           (unsigned int)RDX_RESOURCE_MONITOR_PERIOD_MS);
}

void rdx_resource_monitor_stop(void)
{
    if (!rdx_resource_monitor_running) {
        return;
    }

    sys_timer_del(rdx_resource_monitor_timer_id);
    rdx_resource_monitor_timer_id = 0;
    rdx_resource_monitor_running = 0;
    printf("[RDX][RES] timer_stopped\n");
}

#endif /* TCFG_RDX_ENABLE && TCFG_RDX_RESOURCE_MONITOR_ENABLE */
