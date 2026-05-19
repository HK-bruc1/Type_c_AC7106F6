#include "system/timer.h"
#include "system/init.h"
#include "malloc.h"

extern const u32 CONFIG_HEAP_MEMORY_TRACE;

static u8 timer_cnt = 0;

void memory_debug_timer(void *p)
{
    int ret = memory_trace_dump_minimal();
    if (ret) {
        timer_cnt = 0;
        return;
    }
    if (++timer_cnt == 5) {
        timer_cnt = 0;
        memory_trace_dump();
    }
}


int memory_debug_init()
{
    if (CONFIG_HEAP_MEMORY_TRACE == 0) {
        return 0;
    }
    sys_timer_add(NULL, memory_debug_timer, 200);
    return 0;
}
late_initcall(memory_debug_init);

