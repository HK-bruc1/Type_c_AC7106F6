#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".audio_demo.data.bss")
#pragma data_seg(".audio_demo.data")
#pragma const_seg(".audio_demo.text.const")
#pragma code_seg(".audio_demo.text")
#endif

#include "system/includes.h"
#include "audio_config.h"
#include "media/includes.h"
#include "audio_demo.h"
#include "gpio.h"
#include "pcm_data/sine_pcm.h"

#define DAC_DEMO_ENABLE         0   // DAC_DEMO使能

#if ((defined BT_DUT_INTERFERE) || (DAC_DEMO_ENABLE))
#define DAC_DEMO_TASK_ENABLE	1   // 0:while住不跑后续流程 1:创建任务跑DAC_DEMO(可正常跑默认流程)
#define DAC_FLASH_INTERFERE     0   // 开启FLASH干扰
#define KEY_TEST1	(IO_PORTB_01)   // 按键切换DAC音量 0dB/-60dB/silence
#if (CONFIG_CPU_BR27 || CONFIG_CPU_BR28 || CONFIG_CPU_BR29 || CONFIG_CPU_BR36 || CONFIG_CPU_BR42)
#define JL_AUDIO_REG    JL_AUDIO
#else
#define JL_AUDIO_REG    JL_AUDDAC
#endif

extern struct audio_dac_hdl dac_hdl;

static void dac_test_task_handle(void *p)
{
    printf(">>>>>>>>>>>>>>>>>>>>>audio_dac_demo>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
    app_audio_state_switch(APP_AUDIO_STATE_MUSIC, app_audio_volume_max_query(AppVol_BT_MUSIC), NULL);	// 音量状态设置
    audio_dac_set_volume(&dac_hdl, app_audio_volume_max_query(AppVol_BT_MUSIC));					// dac 音量设置
    audio_dac_set_sample_rate(&dac_hdl, 44100);							// 采样率设置
    audio_dac_start(&dac_hdl);											// dac 启动
    audio_dac_channel_start(NULL);
    u8 *ptr;
    s16 *data_addr16;
    s32 *data_addr32;
    u32 data_len;
    //	判断声道数，双声道需要复制多一个声道的数据
    if (dac_hdl.channel == 2) {
        if (dac_hdl.pd->bit_width) {
            data_addr32 = zalloc(441 * 4 * 2);
            if (!data_addr32) {
                printf("demo dac malloc err !!\n");
                return;
            }
            for (int i = 0; i < 882; i++) {
                data_addr32[i] = sin1k_44100_32bit[i / 2];
            }
            data_len = 441 * 4 * 2;
        } else {
            data_addr16 = zalloc(441 * 2 * 2);
            if (!data_addr16) {
                printf("demo dac malloc err !!\n");
                return;
            }
            for (int i = 0; i < 882; i++) {
                data_addr16[i] = sin1k_44100_16bit[i / 2];
            }
            data_len = 441 * 2 * 2;
        }
    } else {
        if (dac_hdl.pd->bit_width) {
            data_addr32 = (s32 *)sin1k_44100_32bit;
            data_len = 441 * 4;
        } else {
            data_addr16 = (s16 *)sin1k_44100_16bit;
            data_len = 441 * 2;
        }
    }

#if KEY_TEST1
    u8 mode = 0;
    gpio_set_mode(IO_PORT_SPILT(KEY_TEST1), PORT_INPUT_PULLUP_10K);
#endif
    //	循环一直往dac写数据
    while (1) {
        if (dac_hdl.pd->bit_width) {
            ptr = (u8 *)data_addr32;
        } else {
            ptr = (u8 *)data_addr16;
        }
        int len = data_len;
        while (len) {
            // 往 dac 写数据
            /* putchar('w'); */
            int wlen = audio_dac_write(&dac_hdl, ptr, len);
#if KEY_TEST1
            if (0 == gpio_read(KEY_TEST1)) {
                while (0 == gpio_read(KEY_TEST1)) {
                    os_time_dly(1);
                };
                mode++;
                if (mode >= 3) {
                    mode = 0;
                }
                printf(">> mode:%d\n", mode);
                if (mode == 0) {
                    JL_AUDIO_REG->DAC_VL0 = 0;
                    printf(">> silence\n");
                } else if (mode == 1) {
                    JL_AUDIO_REG->DAC_VL0 = 0x000F000F;
                    printf(">> -60dB\n");
                } else {
                    JL_AUDIO_REG->DAC_VL0 = 0x40004000;
                    printf(">> 0dB\n");
                }
            }

#endif
            if (wlen != len) {	// dac缓存满了，延时 10ms 后再继续写
                os_time_dly(1);
            }
            ptr += wlen;
            len -= wlen;
        }
    }
}

static int audio_demo(void)
{
#if DAC_DEMO_TASK_ENABLE
    int ret = os_task_create(dac_test_task_handle, NULL, 31, 512, 0, "dac_task");
#else
    dac_test_task_handle(NULL);
#endif
    return 0;
}
late_initcall(audio_demo);

#if DAC_FLASH_INTERFERE
void idle_hook(void)
{
    while (1) {
        wdt_close();
        //cpu 测试
        /* void coremark_main(void); */
        /* coremark_main(); */
        void debug_random_read(u32 size);
        debug_random_read(32 * 1024);
        local_irq_disable();
        putchar('c');
        putchar('o');
        putchar('r');
        putchar('e');
        local_irq_enable();
    }
}
#endif

#endif
