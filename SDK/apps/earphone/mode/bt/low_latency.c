#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".low_latency.data.bss")
#pragma data_seg(".low_latency.data")
#pragma const_seg(".low_latency.text.const")
#pragma code_seg(".low_latency.text")
#endif
#include "system/includes.h"
#include "btstack/a2dp_media_codec.h"
#include "btstack/avctp_user.h"
#include "a2dp_player.h"
#include "app_tone.h"
#include "low_latency.h"
#include "app_config.h"
#include "bt_tws.h"



#if (THIRD_PARTY_PROTOCOLS_SEL & (TME_EN | DMA_EN | GMA_EN))
#include "app_protocol_api.h"
#endif

#if RCSP_MODE && RCSP_ADV_WORK_SET_ENABLE
#include "adv_work_setting.h"
#endif


#if TCFG_USER_TWS_ENABLE
#include "tws_dual_conn.h"

static void tws_low_latency_tone_cb(int fname_uuid, enum stream_event event)
{
    if (event == STREAM_EVENT_START) {
        int uuid = tone_player_get_fname_uuid(get_tone_files()->low_latency_in);
        if (uuid == fname_uuid) {
            tws_api_low_latency_enable(1);
            a2dp_player_low_latency_enable(1);
            g_printf("tws_api_low_latency_enable 1");
        } else {
            tws_api_low_latency_enable(0);
            a2dp_player_low_latency_enable(0);
            g_printf("tws_api_low_latency_enable 0");
        }
    }
};
REGISTER_TWS_TONE_CALLBACK(tws_low_latency_entry) = {
    .func_uuid = 0x6F90E37B,
    .callback = tws_low_latency_tone_cb,
};

void bt_set_low_latency_mode(int enable, u8 tone_play_enable, int delay_ms)
{
    /*
     * 未连接手机,操作无效
     */
    int state = tws_api_get_tws_state();
    if (state & TWS_STA_PHONE_DISCONNECTED) {
        return;
    }

    g_printf("bt_set_low_latency_mode=%d\n", enable);
#if TCFG_USER_TWS_ENABLE
    if (state & TWS_STA_SIBLING_CONNECTED) {
        if (tone_play_enable) {
            if (delay_ms < 100) {
                delay_ms = 100;
            }
            const char *fname = enable ? get_tone_files()->low_latency_in : get_tone_files()->low_latency_out;
            tws_play_tone_file_alone_callback(fname, delay_ms, 0x6F90E37B);
        }
    } else
#endif
    {
        if (tone_play_enable) {
            const char *fname = enable ? get_tone_files()->low_latency_in : get_tone_files()->low_latency_out;
            play_tone_file_alone(fname);
        }
        tws_api_low_latency_enable(enable);
        a2dp_player_low_latency_enable(enable);
    }
    if (enable) {
        if (bt_get_total_connect_dev()) {
            lmp_hci_write_scan_enable(0);
        }

    } else {
#if TCFG_USER_TWS_ENABLE
        tws_dual_conn_state_handler();
#endif

    }
}

int bt_get_low_latency_mode()
{
    return tws_api_get_low_latency_state();
}

#else
#include "dual_conn.h"

static u8 low_latency_mode = 0;

void bt_set_low_latency_mode(int enable, u8 tone_play_enable, int delay_ms)
{
    g_printf("bt_set_low_latency_mode=%d\n", enable);
    if (tone_play_enable) {
        const char *fname = enable ? get_tone_files()->low_latency_in :
                            get_tone_files()->low_latency_out;
        play_tone_file_alone(fname);
    }
    a2dp_player_low_latency_enable(enable);
    low_latency_mode = enable;
    if (enable) {
        if (bt_get_total_connect_dev()) {
            lmp_hci_write_scan_enable(0);
        }
    } else {
        dual_conn_state_handler();
    }
}

int bt_get_low_latency_mode()
{
    return low_latency_mode;
}
#endif


void bt_enter_low_latency_mode()
{
    puts("enter_low_latency\n");
#if RCSP_MODE && RCSP_ADV_WORK_SET_ENABLE
    rcsp_set_work_mode(RCSPWorkModeGame);
#else
    bt_set_low_latency_mode(1, 1, 400);	// a2dp淡出延时较大，这里调大到400ms可以处理杂音问题
#endif
}

void bt_exit_low_latency_mode()
{
    puts("exit_low_latency\n");
#if RCSP_MODE && RCSP_ADV_WORK_SET_ENABLE
    rcsp_set_work_mode(RCSPWorkModeNormal);
#else
    bt_set_low_latency_mode(0, 1, 400);	// a2dp淡出延时较大，这里调大到400ms可以处理杂音问题
#endif
}
