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
