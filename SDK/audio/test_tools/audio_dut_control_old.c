#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".audio_dut_control_old.data.bss")
#pragma data_seg(".audio_dut_control_old.data")
#pragma const_seg(".audio_dut_control_old.text.const")
#pragma code_seg(".audio_dut_control_old.text")
#endif
/*
 ****************************************************************************
 *							Audio DUT Control Old
 *音频产测旧命令控制流程
 *
 ****************************************************************************
 */

#include "audio_dut_control.h"
#include "online_db_deal.h"
#include "system/includes.h"
#include "app_config.h"
#include "audio_cvp.h"
#include "audio_adc.h"
#include "app_main.h"
#include "audio_config.h"
#include "esco_recoder.h"
#include "adc_file.h"
#if TCFG_AUDIO_ANC_ENABLE
#include "audio_anc.h"
#endif

#if 1
#define audio_dut_log	printf
#else
#define audio_dut_log(...)
#endif

#define AUDIO_DUT_BIG_ENDDIAN 		1		//大端数据输出
#define AUDIO_DUT_LITTLE_ENDDIAN	2		//小端数据输出
#define AUDIO_DUT_OUTPUT_WAY			AUDIO_DUT_LITTLE_ENDDIAN

#if TCFG_AUDIO_DUT_ENABLE

extern audio_dut_t *audio_dut_hdl;

int cvp_dut_bypass_mic_ch_sel(u8 mic_num);

int audio_dut_event_deal(u8 *dat)
{
    u8 last_mode, reset_flag;
    int ret = AUDIO_DUT_ACK_ERR_UNKNOW;
    if (audio_dut_hdl) {
__again:
        reset_flag = 0;
        last_mode = audio_dut_hdl->mode;
        ret = audio_dut_common_event_deal(dat[3], &reset_flag);
        if (last_mode != audio_dut_hdl->mode) {
            reset_flag = 1;		//模式被修改，则复位通话
        }
        if (reset_flag) {
            audio_dut_hdl->reset_flag = 1;
            esco_recoder_reset();
            audio_dut_hdl->reset_flag = 0;
            //由于算法相关的命令在esco_recoder_reset后会失效，因此需要重新跑一遍
            if (audio_dut_hdl->mode == CVP_DUT_MODE_ALGORITHM) {
                goto __again;
            }
        }
    }
    return ret;
}

int audio_dut_spp_tx_packet(u8 command)
{
    if (audio_dut_hdl) {
        audio_dut_hdl->tx_buf.magic = AUDIO_DUT_SPP_MAGIC;
        audio_dut_hdl->tx_buf.dat[0] = 0;
        audio_dut_hdl->tx_buf.dat[1] = 0;
        audio_dut_hdl->tx_buf.dat[2] = 0;
        audio_dut_hdl->tx_buf.dat[3] = command;
        audio_dut_log("tx dat");
#if AUDIO_DUT_OUTPUT_WAY == AUDIO_DUT_LITTLE_ENDDIAN	//	小端格式
        audio_dut_hdl->tx_buf.crc = CRC16((&audio_dut_hdl->tx_buf.dat), AUDIO_DUT_PACK_NUM - 4);
        put_buf((u8 *)&audio_dut_hdl->tx_buf.magic, AUDIO_DUT_PACK_NUM);
        app_online_db_send(DB_PKT_TYPE_DMS, (u8 *)&audio_dut_hdl->tx_buf.magic, AUDIO_DUT_PACK_NUM);
#else
        u16 crc_temp;
        int i;
        u8 dat[AUDIO_DUT_PACK_NUM];
        u8 dat_temp;
        memcpy(dat, &(audio_dut_hdl->tx_buf), AUDIO_DUT_PACK_NUM);
        crc_temp = CRC16(dat + 4, 6);
        printf("crc0x%x,0x%x", crc_temp, audio_dut_hdl->tx_buf.crc);
        for (i = 0; i < 6; i += 2) {	//小端数据转大端
            dat_temp = dat[i];
            dat[i] = dat[i + 1];
            dat[i + 1] = dat_temp;
        }
        crc_temp = CRC16(dat + 4, AUDIO_DUT_PACK_NUM - 4);
        dat[2] = crc_temp >> 8;
        dat[3] = crc_temp & 0xff;
        put_buf(dat, AUDIO_DUT_PACK_NUM);
        app_online_db_send(DB_PKT_TYPE_DMS, dat, AUDIO_DUT_PACK_NUM);
#endif

    }
    return 0;
}


int audio_dut_spp_rx_packet(u8 *dat, u8 len)
{
    if (audio_dut_hdl) {
        u8 dat_packet[AUDIO_DUT_PACK_NUM];
        if (len > AUDIO_DUT_PACK_NUM) {
            return 1;
        }
        /* audio_dut_log("rx dat,%d\n", AUDIO_DUT_PACK_NUM); */
        put_buf(dat, len);
        memcpy(dat_packet, dat, len);
        u16 crc = CRC16(dat + 4, len - 4);
#if AUDIO_DUT_OUTPUT_WAY == AUDIO_DUT_BIG_ENDDIAN	//	大端格式
        for (int i = 0; i < 6; i += 2) {		//	大端数据转小端
            u8 dat_temp = dat_packet[i];
            dat_packet[i] = dat_packet[i + 1];
            dat_packet[i + 1] = dat_temp;
        }
#endif
        audio_dut_log("rx dat_packet");
        memcpy(&(audio_dut_hdl->rx_buf), dat_packet, 4);
        if (audio_dut_hdl->rx_buf.magic == AUDIO_DUT_SPP_MAGIC) {
            audio_dut_log("crc %x,audio_dut_hdl->rx_buf.crc %x\n", crc, audio_dut_hdl->rx_buf.crc);
            if (audio_dut_hdl->rx_buf.crc == crc || audio_dut_hdl->rx_buf.crc == 0x1) {
                memcpy(&(audio_dut_hdl->rx_buf), dat_packet, len);
                int ret = audio_dut_event_deal(audio_dut_hdl->rx_buf.dat);
                audio_dut_spp_tx_packet(ret);		//反馈收到命令
                return 0;
            }
        }
    }
    return 1;
}
#endif/*TCFG_AUDIO_DUT_ENABLE*/

