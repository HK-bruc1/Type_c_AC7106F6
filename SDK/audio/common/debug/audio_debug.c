#include "audio_debug.h"
#include "system/timer.h"
#include "audio_config.h"
#include "system/task.h"
#include "jlstream.h"
#include "classic/hci_lmp.h"
#include "media_memory.h"
#include "audio_dac.h"


const int CONFIG_MEDIA_MEM_DEBUG = AUD_MEM_INFO_DUMP_ENABLE;

static void flash_random_read_debug(u32 size)
{
    u32 ali_start = rand32();
    u32 ali_end;
    volatile u8 tmp;
    extern u32 CODE_BEG;
    ali_start %= 64 * 1024;
    ali_start += (u32)&CODE_BEG;
    ali_end = ali_start + size;

    for (u32 i = ali_start;  i <  ali_end; i += 32) {
        tmp = *(u8 *)i;
    }

    local_irq_disable();
    extern void sfc_drop_cache(void *ptr, u32 len);
    sfc_drop_cache((void *)ali_start, size);
    local_irq_enable();
}

static void audio_config_trace(void *priv)
{
    printf(">>Audio_Config_Trace:\n");
#if AUD_BT_DELAY_INFO_DUMP_ENABLE
    a2dp_delay_dump();
#endif

#if AUD_CFG_DUMP_ENABLE
    audio_config_dump();
#endif

#if AUD_REG_DUMP_ENABLE
    audio_adda_dump();
#endif

#if AUD_CACHE_INFO_DUMP_ENABLE
    extern void CacheReport(void);
    CacheReport();
#endif

#if AUD_TASK_INFO_DUMP_ENABLE
    task_info_output(0);
#endif

#if AUD_JLSTREAM_MEM_DUMP_ENABLE
    stream_mem_unfree_dump();
#endif

#if AUD_BT_INFO_DUMP_ENABLE
    printf("ESCO Tx Packet Num:%d", lmp_private_get_esco_tx_packet_num());
#endif

#if SYS_MEM_INFO_DUMP_ENABLE
    mem_stats();
#endif

#if AUD_MEM_INFO_DUMP_ENABLE
    media_mem_unfree_dump();
#endif

#if FLASH_INTERFERE_WITH_AUDIO_DEBUG
    flash_random_read_debug(1024 * 4);
#endif

}

void audio_config_trace_setup(int interval)
{
    sys_timer_add(NULL, audio_config_trace, interval);
}

#define DAC_POP_UP_NOISE_DEBUG_DISABLE	0
#define DAC_POWER_ON_POP_DBG			1
#define DAC_MUTE_POP_DBG				2
#define DAC_POP_UP_NOISE_DEBUG_CFG		DAC_POP_UP_NOISE_DEBUG_DISABLE
static void dac_power_on_pop_debug(void *priv)
{
    static u8 dac_en;
    dac_en ^= 1;
    if (dac_en) {
        audio_dac_try_power_on(&dac_hdl);
    } else {
        audio_dac_stop(&dac_hdl);
        audio_dac_close(&dac_hdl);
    }
}

static void dac_mute_pop_debug(void *priv)
{
    static u8 mute_en = 0;
    mute_en ^= 1;
    audio_dac_ch_mute(&dac_hdl, 0b11, mute_en);
}

void audio_dac_pop_up_noise_debug(void)
{
#if (DAC_POP_UP_NOISE_DEBUG_CFG ==  DAC_POWER_ON_POP_DBG)
    sys_timer_add(NULL, dac_power_on_pop_debug, 3000);
#elif (DAC_POP_UP_NOISE_DEBUG_CFG ==  DAC_MUTE_POP_DBG)
    sys_timer_add(NULL, dac_mute_pop_debug, 3000);
#endif
}
