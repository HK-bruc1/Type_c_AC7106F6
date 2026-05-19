/*
 ****************************************************************************
 *							Audio DAC Demo
 *
 *Description	: Audio DAC使用范例
 *Notes			: 本demo为开发测试范例，请不要修改该demo， 如有需求，请自行
 *				  复制再修改
 ****************************************************************************
 */
#include "audio_demo.h"
#include "media/includes.h"
#include "audio_config.h"
#include "audio_demo.h"
#include "gpio.h"
#include "pcm_data/sine_pcm.h"

#define KEY_TEST	(IO_PORTB_02)
#define KEY_TEST2	(IO_PORTB_04)

extern struct audio_dac_hdl dac_hdl;
static u8 dac_demo = 0;

/*
*********************************************************************
*                  AUDIO DAC OPEN
* Description: 打开 dac demo
* Arguments  : None
* Return	 : None.
* Note(s)    : dac 播放正弦波
*              将这个函数放audio_dec_init()后调用即可
*********************************************************************
*/

void audio_dac_demo_open(void)
{
    u8 *ptr;
    s16 *data_addr16;
    s32 *data_addr32;
    u32 data_len;

    app_audio_state_switch(APP_AUDIO_STATE_MUSIC, app_audio_volume_max_query(AppVol_BT_MUSIC), NULL);	// 音量状态设置
    audio_dac_set_volume(&dac_hdl, app_audio_volume_max_query(AppVol_BT_MUSIC));					// dac 音量设置
    audio_dac_set_sample_rate(&dac_hdl, 44100);							// 采样率设置
    audio_dac_start(&dac_hdl);											// dac 启动
    audio_dac_channel_start(NULL);
    JL_AUDDAC->DAC_VL0 = 0x40004000;
    SFR(JL_ADDA->DAA_CON1,   1, 3,  7);          //AUDDAC_LGAIN_SEL[2:0]_11v
    SFR(JL_ADDA->DAA_CON2,   1, 3,  7);          //AUDDAC_RGAIN_SEL[2:0]_11v
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

    dac_demo = 1;

#if KEY_TEST
    u8 mode = 0;
    gpio_set_mode(IO_PORT_SPILT(KEY_TEST), PORT_INPUT_PULLUP_10K);
#endif

#if KEY_TEST2
    u8 ana_vol_tab[4] = {0, 1, 3, 7};
    gpio_set_mode(IO_PORT_SPILT(KEY_TEST2), PORT_INPUT_PULLUP_10K);
#endif

    //	循环一直往dac写数据
    while (1) {

        // 这句是为了防止线程太久没有响应系统而产生异常，实际使用不需要
        int msg[16];
        os_taskq_accept(ARRAY_SIZE(msg), msg);

        if (dac_hdl.pd->bit_width) {
            ptr = (u8 *)data_addr32;
        } else {
            ptr = (u8 *)data_addr16;
        }
        int len = data_len;
        while (len) {
            // 往 dac 写数据
            int wlen = audio_dac_write(&dac_hdl, ptr, len);
#if KEY_TEST
            if (0 == gpio_read(KEY_TEST)) {
                while (0 == gpio_read(KEY_TEST)) {
                    os_time_dly(1);
                };
                mode++;
                if (mode >= 3) {
                    mode = 0;
                }
                printf(">> mode:%d\n", mode);
                if (mode == 0) {
                    JL_AUDDAC->DAC_VL0 = 0;
                    printf(">> silence\n");
                } else if (mode == 1) {
                    JL_AUDDAC->DAC_VL0 = 0x000F000F;
                    printf(">> -60dB\n");
                } else {
                    JL_AUDDAC->DAC_VL0 = 0x40004000;
                    printf(">> 0dB\n");
                }
            }

#endif

#if KEY_TEST2

            if (0 == gpio_read(KEY_TEST2)) {
                while (0 == gpio_read(KEY_TEST2)) {
                    os_time_dly(1);
                };
                static u8 analog_gain = 0;
                SFR(JL_ADDA->DAA_CON1, 5, 3, ana_vol_tab[analog_gain]);
                SFR(JL_ADDA->DAA_CON2, 5, 3, ana_vol_tab[analog_gain]);

                printf(">>> analog_gain: %d\n", ana_vol_tab[analog_gain]);

                analog_gain++;
                if (analog_gain > 3) {
                    analog_gain = 0;
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

/*
*********************************************************************
*                  AUDIO DAC CLOSE
* Description: 关闭 dac demo
* Arguments  : None
* Return	 : None.
* Note(s)    : dac 停止播放正弦波
*********************************************************************
*/
void audio_dac_demo_close(void)
{
    // 停止并关闭 DAC
    audio_dac_stop(&dac_hdl);
    audio_dac_close(&dac_hdl);
    dac_demo = 0;
}

#if AUDIO_DEMO_LP_REG_ENABLE
static u8 dac_demo_idle_query()
{
    return dac_demo ? 0 : 1;
}

REGISTER_LP_TARGET(dac_demo_lp_target) = {
    .name = "dac_demo",
    .is_idle = dac_demo_idle_query,
};
#endif/*AUDIO_DEMO_LP_REG_ENABLE*/


