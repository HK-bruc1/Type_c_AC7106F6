/*
 * Product resource monitor independent of application and protocol modules.
 *
 * flash_code fields:
 * - used: start address of the automatically placed VM tail area. With the
 *   current zero-offset package layout, this is firmware and resource usage
 *   before VM.
 * - free: space before VM that remains after reserving TCFG_VM_SIZE.
 * - capacity: used + free. This is not the capacity of the whole flash.
 *
 * ram fields:
 * - total: allocator-managed RAM0 and optional RAM1 heap capacity.
 * - used/free: current allocated/free heap totals across RAM0 and RAM1.
 * - min_free: lowest sampled free total since boot. It is not an allocator
 *   low-water mark and may miss short peaks between samples.
 *
 * Values use integer KiB (bytes / 1024) although the log suffix is KB. Heap
 * fields are read through separate SDK calls and are not an atomic snapshot.
 */
#include "app_config.h"

#if TCFG_RESOURCE_MONITOR_ENABLE

#include "system/boot.h"
#include "system/init.h"
#include "system/timer.h"
#include "malloc.h"
#include "resource_monitor.h"

#define RESOURCE_BYTES_TO_KB(value)    ((u32)((value) / 1024))

static u16 resource_monitor_timer_id;
static u32 resource_monitor_min_ram_free = 0xffffffffUL;

static void resource_monitor_dump_flash_code(void)
{
    const BOOT_INFO *boot = get_boot_info();
    u32 vm_reserved = TCFG_VM_SIZE * 1024;
    u32 code_used = boot->vm.vm_saddr;
    u32 code_free = boot->vm.vm_size > vm_reserved
                    ? boot->vm.vm_size - vm_reserved : 0;
    u32 code_capacity = code_used + code_free;

    printf("[RES] flash_code capacity=%uKB used=%uKB free=%uKB\n",
           RESOURCE_BYTES_TO_KB(code_capacity),
           RESOURCE_BYTES_TO_KB(code_used),
           RESOURCE_BYTES_TO_KB(code_free));
}

static void resource_monitor_dump_ram(const char *reason)
{
    u32 ram_total = (u32)memory_get_size(P_HEAP_SIZE);
    u32 ram_used = (u32)memory_get_size(P_MEMORY_USED)
                   + (u32)memory_get_size(P_VLT_MEMORY_USED);
    u32 ram_free = (u32)memory_get_size(P_MEMORY_UNUSED)
                   + (u32)memory_get_size(P_VLT_MEMORY_UNUSED);

    if (ram_free < resource_monitor_min_ram_free) {
        resource_monitor_min_ram_free = ram_free;
    }

    printf("[RES] ram reason=%s total=%uKB used=%uKB"
           " free=%uKB min_free=%uKB\n",
           reason ? reason : "unknown",
           RESOURCE_BYTES_TO_KB(ram_total),
           RESOURCE_BYTES_TO_KB(ram_used),
           RESOURCE_BYTES_TO_KB(ram_free),
           RESOURCE_BYTES_TO_KB(resource_monitor_min_ram_free));
}

void resource_monitor_snapshot(const char *reason)
{
    resource_monitor_dump_ram(reason);
}

static void resource_monitor_timer(void *priv)
{
    resource_monitor_dump_ram("periodic");
}

static int resource_monitor_init(void)
{
    resource_monitor_dump_flash_code();
    resource_monitor_dump_ram("start");

    resource_monitor_timer_id = sys_timer_add(NULL,
                                              resource_monitor_timer,
                                              RESOURCE_MONITOR_PERIOD_MS);
    if (!resource_monitor_timer_id) {
        printf("[RES] timer_start_failed period_ms=%u\n",
               (unsigned int)RESOURCE_MONITOR_PERIOD_MS);
        return -1;
    }

    printf("[RES] timer_started period_ms=%u\n",
           (unsigned int)RESOURCE_MONITOR_PERIOD_MS);
    return 0;
}

late_initcall(resource_monitor_init);

#endif /* TCFG_RESOURCE_MONITOR_ENABLE */
