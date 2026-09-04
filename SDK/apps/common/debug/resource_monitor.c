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
 * ram fields are reported separately for RAM0 and RAM1.  aggregate_* is only
 * a dashboard value; it must not be used to decide whether either allocator
 * pool can satisfy a request.  min_free is the lowest value observed by this
 * module and is not an allocator-native low-water mark.
 *
 * Flash values use integer KiB.  Heap values use exact bytes so small pool
 * changes remain visible.  Heap fields are read through separate SDK calls
 * and are not an atomic snapshot.
 */
#include "app_config.h"
#include "resource_monitor.h"

#if TCFG_RESOURCE_MONITOR_ENABLE

#include <string.h>

#include "system/boot.h"
#include "system/init.h"
#include "system/timer.h"
#include "malloc.h"
#include "task.h"

#define RESOURCE_BYTES_TO_KB(value)    ((u32)((value) / 1024))

static u16 resource_monitor_timer_id;
static u32 resource_monitor_min_ram0_free = 0xffffffffUL;
static u32 resource_monitor_min_ram1_free = 0xffffffffUL;
static u32 resource_monitor_min_aggregate_free = 0xffffffffUL;
static SIMPLE_TASK_INFO resource_monitor_task_snapshot[
    RESOURCE_MONITOR_TASK_SNAPSHOT_CAPACITY];

typedef struct {
    const char *name;
    u8 prefix;
} resource_monitor_task_filter_t;

#define RESOURCE_MONITOR_EXACT_TASK(name)   { name, 0 },
#define RESOURCE_MONITOR_PREFIX_TASK(name)  { name, 1 },
static const resource_monitor_task_filter_t resource_monitor_task_filters[] = {
    RESOURCE_MONITOR_KEY_TASK_LIST(RESOURCE_MONITOR_EXACT_TASK,
                                   RESOURCE_MONITOR_PREFIX_TASK)
};
#undef RESOURCE_MONITOR_EXACT_TASK
#undef RESOURCE_MONITOR_PREFIX_TASK

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

static void resource_monitor_sample_ram(const char *reason, u8 emit)
{
    u32 ram0_total = (u32)memory_get_size(P_MEMORY_TOTAL);
    u32 ram0_used = (u32)memory_get_size(P_MEMORY_USED);
    u32 ram0_free = (u32)memory_get_size(P_MEMORY_UNUSED);
    u32 ram1_total = (u32)memory_get_size(P_VLT_MEMORY_TOTAL);
    u32 ram1_used = (u32)memory_get_size(P_VLT_MEMORY_USED);
    u32 ram1_free = (u32)memory_get_size(P_VLT_MEMORY_UNUSED);
    u32 aggregate_total = ram0_total + ram1_total;
    u32 aggregate_used = ram0_used + ram1_used;
    u32 aggregate_free = ram0_free + ram1_free;

    if (ram0_free < resource_monitor_min_ram0_free) {
        resource_monitor_min_ram0_free = ram0_free;
    }
    if (ram1_free < resource_monitor_min_ram1_free) {
        resource_monitor_min_ram1_free = ram1_free;
    }
    if (aggregate_free < resource_monitor_min_aggregate_free) {
        resource_monitor_min_aggregate_free = aggregate_free;
    }
    if (!emit) {
        return;
    }

    printf("[RES] ram reason=%s"
           " ram0_total=%u ram0_used=%u ram0_free=%u ram0_min_free=%u"
           " ram1_total=%u ram1_used=%u ram1_free=%u ram1_min_free=%u"
           " aggregate_total=%u aggregate_used=%u aggregate_free=%u"
           " aggregate_min_free=%u unit=B\n",
           reason ? reason : "unknown",
           (unsigned int)ram0_total, (unsigned int)ram0_used,
           (unsigned int)ram0_free,
           (unsigned int)resource_monitor_min_ram0_free,
           (unsigned int)ram1_total, (unsigned int)ram1_used,
           (unsigned int)ram1_free,
           (unsigned int)resource_monitor_min_ram1_free,
           (unsigned int)aggregate_total, (unsigned int)aggregate_used,
           (unsigned int)aggregate_free,
           (unsigned int)resource_monitor_min_aggregate_free);
}

void resource_monitor_snapshot(const char *reason)
{
    resource_monitor_sample_ram(reason, 1);
}

void resource_monitor_observe(void)
{
    resource_monitor_sample_ram(NULL, 0);
}

static u8 resource_monitor_is_key_task(const char *name)
{
    u16 i;

    if (!name) {
        return 0;
    }
    for (i = 0;
         i < sizeof(resource_monitor_task_filters)
             / sizeof(resource_monitor_task_filters[0]);
         i++) {
        const resource_monitor_task_filter_t *filter =
            &resource_monitor_task_filters[i];

        if (filter->prefix) {
            if (!strncmp(name, filter->name, strlen(filter->name))) {
                return 1;
            }
        } else if (!strcmp(name, filter->name)) {
            return 1;
        }
    }
    return 0;
}

void resource_monitor_dump_key_tasks(const char *reason)
{
    u16 count;
    u16 i;

    memset(resource_monitor_task_snapshot, 0,
           sizeof(resource_monitor_task_snapshot));
    count = simple_task_info_list_get(
        resource_monitor_task_snapshot,
        sizeof(resource_monitor_task_snapshot)
            / sizeof(resource_monitor_task_snapshot[0]));
    for (i = 0; i < count; i++) {
        const SIMPLE_TASK_INFO *task = &resource_monitor_task_snapshot[i];

        if (!resource_monitor_is_key_task(task->task_name)) {
            continue;
        }
        printf("[RES] task reason=%s name=%s stack_used=%u stack_max=%u"
               " stack_hwm=%u cpu=%u\n",
               reason ? reason : "unknown", task->task_name,
               (unsigned int)task->stack_used,
               (unsigned int)task->stack_max,
               (unsigned int)task->stack_hwm,
               (unsigned int)task->task_usage);
    }
}

static void resource_monitor_timer(void *priv)
{
    resource_monitor_sample_ram("periodic", 1);
}

static int resource_monitor_init(void)
{
    resource_monitor_dump_flash_code();
    resource_monitor_sample_ram("start", 1);

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
