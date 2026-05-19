#include "app_config.h"
#if ((defined TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN) && TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN && \
	 TCFG_AUDIO_ANC_ENABLE && TCFG_AUDIO_WIDE_AREA_TAP_ENABLE)
#include "system/includes.h"
#include "audio_anc_includes.h"
#include "tone_player.h"
#include "audio_config.h"
#include "sniff.h"
#include "btstack/avctp_user.h"
#include "bt_tws.h"

/*打开广域点击*/
int audio_wat_click_open()
{
    printf("%s", __func__);
    u8 adt_mode = ADT_WIDE_AREA_TAP_MODE;
    return audio_icsd_adt_sync_open(adt_mode);
}

/*关闭广域点击*/
int audio_wat_click_close()
{
    printf("%s", __func__);
    u8 adt_mode = ADT_WIDE_AREA_TAP_MODE;
    return audio_icsd_adt_sync_close(adt_mode, 0);
}

/*广域点击使用demo*/
void audio_wat_click_demo()
{
    printf("%s", __func__);
    if (audio_icsd_adt_open_permit(ADT_WIDE_AREA_TAP_MODE) == 0) {
        return;
    }

    if ((get_icsd_adt_mode() & ADT_WIDE_AREA_TAP_MODE) == 0) {
        /*打开提示音*/
        icsd_adt_tone_play(ICSD_ADT_TONE_WCLICK_ON);
        audio_wat_click_open();
    } else {
        /*关闭提示音*/
        icsd_adt_tone_play(ICSD_ADT_TONE_WCLICK_OFF);
        audio_wat_click_close();
    }
}

/*广域点击事件处理*/
void audio_wat_area_tap_event_handle(u8 wat_result)
{
    switch (wat_result) {
    case WIND_AREA_TAP_DOUBLE_CLICK:
        /*音乐暂停播放*/
        if ((bt_get_call_status() == BT_CALL_OUTGOING) ||
            (bt_get_call_status() == BT_CALL_ALERT)) {
            bt_cmd_prepare(USER_CTRL_HFP_CALL_HANGUP, 0, NULL);
        } else if (bt_get_call_status() == BT_CALL_INCOMING) {
            bt_cmd_prepare(USER_CTRL_HFP_CALL_ANSWER, 0, NULL);
        } else if (bt_get_call_status() == BT_CALL_ACTIVE) {
            bt_cmd_prepare(USER_CTRL_HFP_CALL_HANGUP, 0, NULL);
        } else {
            bt_cmd_prepare(USER_CTRL_AVCTP_OPID_PLAY, 0, NULL);
        }
        break;
    case WIND_AREA_TAP_THIRD_CLICK:
        /*anc切模式*/
        anc_mode_next();
        break;
    case WIND_AREA_TAP_MULTIPLE_CLICK:
        /* tone_play_index(IDEX_TONE_NUM_4, 0); */
        break;
    }
}

#endif /*(defined TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN) && TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN*/
