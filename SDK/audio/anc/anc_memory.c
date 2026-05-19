#include "malloc.h"
#include "typedef.h"

#pragma   code_seg(".audio.text.cache.L1")

#define ANC_DEBUG_MAX_MEMORY_USAGE		0	//统计ANC最大内存使用量, 刷新最大值时会默认dump

const u8 CONFIG_ANC_MEM_DEBUG = 0;      	//ANC内存管理使能

void anc_mem_unfree_dump();             	//未释放的ANC内存打印

/*Format:[0]addr [1]size [2]name [3]rets*/
static u32 anc_unfree_rets[4][64];


static void anc_unfree_rets_push(const char *name, u32 rets_addr, void *ptr, u32 size)
{
    if (CONFIG_ANC_MEM_DEBUG) {
        for (int i = 0; i < ARRAY_SIZE(anc_unfree_rets[0]); i++) {
            if (anc_unfree_rets[0][i]) {
                continue;
            }
            anc_unfree_rets[0][i] = (u32)ptr;
            anc_unfree_rets[1][i] = size;
            anc_unfree_rets[2][i] = (u32)name;
            anc_unfree_rets[3][i] = rets_addr;
            return;
        }
        r_printf("anc mem can't recode");
    }
}

static void anc_unfree_rets_pop(void *ptr)
{
    if (CONFIG_ANC_MEM_DEBUG) {
        for (int i = 0; i < ARRAY_SIZE(anc_unfree_rets[0]); i++) {
            if (anc_unfree_rets[0][i] == (u32)ptr) {
                anc_unfree_rets[0][i] = 0;
                anc_unfree_rets[1][i] = 0;
                return;
            }
        }
    }
}

void anc_mem_unfree_dump()
{
    if (CONFIG_ANC_MEM_DEBUG) {
        int j = 0;

        int mm_unfree_total = 0;
        for (int i = 0; i < ARRAY_SIZE(anc_unfree_rets[0]); i++) {
            if (anc_unfree_rets[0][i] == 0) {
                continue;
            }
            mm_unfree_total += anc_unfree_rets[1][i];
            printf("anc_unfree: [%d], %s, %x, %x, %d", j++, (char *)anc_unfree_rets[2][i], anc_unfree_rets[3][i], anc_unfree_rets[0][i], anc_unfree_rets[1][i]);
        }
        printf("anc_unfree_total:%d(bytes)", mm_unfree_total);
    }
}

//统计ANC申请的最大消耗
void anc_mem_unfree_dump_max(u8 clear)
{
    if (CONFIG_ANC_MEM_DEBUG) {
        static int max_mm_unfree_total = 0;
        if (clear) {
            max_mm_unfree_total = 0;
        }
        int j = 0;
        int mm_unfree_total = 0;
        for (int i = 0; i < ARRAY_SIZE(anc_unfree_rets[0]); i++) {
            if (anc_unfree_rets[0][i] == 0) {
                continue;
            }
            mm_unfree_total += anc_unfree_rets[1][i];
        }
        if (max_mm_unfree_total < mm_unfree_total) {
            max_mm_unfree_total = mm_unfree_total;
            g_printf("max_anc_unfree_total %d\n", max_mm_unfree_total);
            anc_mem_unfree_dump();
        }
    }
}

void *anc_malloc(const char *name, size_t size)
{
    u32 rets_addr;
    __asm__ volatile("%0 = rets ;" : "=r"(rets_addr));
    if (CONFIG_ANC_MEM_DEBUG) {
        //printf("[w]anc_malloc,module:%s,size:%d\n", name, (int)size);
        void *ptr = zalloc(size);
        anc_unfree_rets_push(name, rets_addr, ptr, size);
#if ANC_DEBUG_MAX_MEMORY_USAGE
        anc_mem_unfree_dump_max(0);
#endif
        return ptr;
    }
    return zalloc(size);
}

void anc_mem_clear(void *pv)
{
    if (CONFIG_ANC_MEM_DEBUG) {
        anc_unfree_rets_pop(pv);
    }
}

void anc_free(void *pv)
{
    if (CONFIG_ANC_MEM_DEBUG) {
        anc_unfree_rets_pop(pv);
    }
    free(pv);
}
