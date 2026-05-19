#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".app_key_dut.data.bss")
#pragma data_seg(".app_key_dut.data")
#pragma const_seg(".app_key_dut.text.const")
#pragma code_seg(".app_key_dut.text")
#endif
#include "app_key_dut.h"
#include "app_msg.h"
#include "app_config.h"
#include "low_latency.h"

#if TCFG_USER_TWS_ENABLE
#include "bt_tws.h"
#endif

#if TCFG_APP_KEY_DUT_ENABLE

#if 1
#define key_dut_log     printf
#else
#define key_dut_log
#endif

/* 最大30字节，因为每次给上位机最大只能发送30字节数据)
 * 要求：建议上位机查询间隔小于1s，
 *       以保证查询间隔内出现的按键事件的字符串不大于KEY_LIST_ZISE*/
#define KEY_LIST_ZISE 30

//TWS区分左右按键表使能：用于TWS左右耳UI不同的情况
#define KEY_DUT_TWS_SEPARATE_LEFT_AND_RIGHT		0

typedef struct {
    u8 index; //记录当前存储的位置
    char key_list[KEY_LIST_ZISE];  //存储按键事件
} key_dut_hdl_t;

key_dut_hdl_t *key_dut_hdl = NULL;

typedef struct {
    u16 index;
    char name[15];  //最大14个字符
} key_mpt_self_list_t;

//按键表
const static key_mpt_self_list_t key_event_list[] = {
    {0x00, "music_pp"},
    {0x01, "music_prev"},
    {0x02, "music_next"},
    {0x03, "call_answer"},
    {0x04, "call_hangup"},
    {0x05, "InLowLatency"},
    {0x06, "OutLowLatency"},
    {0x07, "open_siri"},
    {0x08, "close_siri"},
    {0x09, "vol_up"},
    {0x0A, "vol_down"},
    {0x0B, "anc_on"},
    {0x0C, "anc_off"},
    {0x0D, "anc_trans"},
    {0x0E, "anc_next"},
    {0x0F, "mic_open"},
    {0x10, "mic_close"},
    {0x11, "ChangerUncle"},
    {0x12, "ChangerGoddess"},
    {0x13, "spatial_eff"},
    {0x14, "spk_char"},
    {0x15, "wat_click"},
    {0x16, "wind_det"},
    {0x17, "anc_adap"},
    //用户可在此继续扩展其他按键
};

//按键表-右耳
#if KEY_DUT_TWS_SEPARATE_LEFT_AND_RIGHT
const static key_mpt_self_list_t key_event_list_R[] = {
    /* {0x00, "music_pp"}, */
    //用户需完善右耳按键表
};
#endif

/*获取产测按键事件表的地址，返回数据长度*/
int get_key_dut_event_list(u8 **key_list)
{
    int len = sizeof(key_event_list);
#if KEY_DUT_TWS_SEPARATE_LEFT_AND_RIGHT && TCFG_USER_TWS_ENABLE
    if (bt_tws_get_local_channel() == 'R') {
        *key_list = (u8 *)key_event_list_R;
    } else {
        *key_list = (u8 *)key_event_list;
    }
#else
    *key_list = (u8 *)key_event_list;
#endif
    return len;
}

/*开始按键产测*/
int app_key_dut_start()
{
    key_dut_log("app_key_dut_start");
    if (key_dut_hdl) {
        key_dut_log("app key dut alreadly open !!!");
        return 0;
    }
    key_dut_hdl_t *hdl = zalloc(sizeof(key_dut_hdl_t));
    key_dut_log("hdl zise : %d", (int)sizeof(key_dut_hdl_t));
    if (hdl == NULL) {
        key_dut_log("key dut malloc fail !!!");
        return -1;
    }
    hdl->index = 0;
    key_dut_hdl = hdl;
    return 0;
}

/*结束按键产测*/
int app_key_dut_stop()
{
    key_dut_hdl_t *hdl = (key_dut_hdl_t *)key_dut_hdl;
    key_dut_log("app_key_dut_stop");
    if (hdl) {
        free(hdl);
        key_dut_hdl = NULL;
    }
    return 0;
}

/* 获取已经按下的按键列表
 * 要求：上位机查询间隔小于1s，
 *       以保证查询间隔内出现的按键事件的字符串不大于KEY_LIST_ZISE*/
int get_key_press_list(void *key_list)
{
    key_dut_hdl_t *hdl = (key_dut_hdl_t *)key_dut_hdl;
    if (hdl) {
        /*记录按键列表字符长度*/
        local_irq_disable();
        int key_list_len = hdl->index;
        /*拷贝按键事件列表*/
        memcpy(key_list, hdl->key_list, key_list_len);
        /*将按键事件列表清零*/
        memset(hdl->key_list, '\0', (int)sizeof(hdl->key_list));
        hdl->index = 0;
        local_irq_enable();

        return key_list_len;
    }
    return 0;
}

/*(标准SDK情景配置流程)发送按键事件到key dut*/
int app_key_dut_send(int *msg, const char *key_name)
{
    key_dut_hdl_t *hdl = (key_dut_hdl_t *)key_dut_hdl;
    /*判断是否已经开按键产测*/
    if (!hdl) {
        return 0;
    }
    /*判断是否按键产生的事件*/
    if (msg) {
        if (!APP_MSG_FROM_KEY(msg[0])) {
            return 0;
        }
    }
    key_dut_log("[key_dut] key_event : %s", key_name);

    /*字符串长度(包括结束符'\0')*/
    int key_name_len = strlen(key_name) + 1;
    /* ASSERT((hdl->index + key_name_len) <= KEY_LIST_ZISE); */

    local_irq_disable();
    if ((hdl->index + key_name_len) > KEY_LIST_ZISE) {
        printf("[error][key_dut]  key_name_list size over %d !!!", KEY_LIST_ZISE);
        memset(hdl->key_list, '\0', (int)sizeof(hdl->key_list));
        hdl->index = 0;
    }

    /*将按键事件名字存起来*/
    memcpy(&hdl->key_list[hdl->index], key_name, key_name_len);
    /* key_dut_log("[key_dut] key_name : %s", &hdl->key_list[hdl->index]); */
    /*记录下一次存放起始位置*/
    hdl->index += key_name_len;
    local_irq_enable();

    return 0;
}

/*(用户自定义)发送按键事件到key dut*/
int app_key_dut_user_send(const char *key_name)
{
    return app_key_dut_send(NULL, key_name);
}

void app_key_dut_msg_handler(int app_msg)
{
    const char *str = NULL;

    switch (app_msg) {
    case APP_MSG_MUSIC_PP:
        str = "music_pp";
        break;
    case APP_MSG_MUSIC_NEXT:
        str = "music_next";
        break;
    case APP_MSG_MUSIC_PREV:
        str = "music_prev";
        break;
    case APP_MSG_CALL_ANSWER:
        str = "call_answer";
        break;
    case APP_MSG_CALL_HANGUP:
        str = "call_hangup";
        break;
    case APP_MSG_OPEN_SIRI:
        str = "open_siri";
        break;
    case APP_MSG_CLOSE_SIRI:
        str = "close_siri";
        break;
    case APP_MSG_LOW_LANTECY:
        if (bt_get_low_latency_mode() == 0) {
            str = "InLowLatency";
        } else {
            str = "OutLowLatency";
        }
        break;
    case APP_MSG_VOL_UP:
        str = "vol_up";
        break;
    case APP_MSG_VOL_DOWN:
        str = "vol_down";
        break;
    case APP_MSG_MIC_OPEN:
        str = "mic_open";
        break;
    case APP_MSG_MIC_CLOSE:
        str = "mic_close";
        break;
#if TCFG_VOICE_CHANGER_NODE_ENABLE
    case APP_MSG_ESCO_UNCLE_VOICE:
        str = "ChangerUncle";
        break;
    case APP_MSG_ESCO_GODDESS_VOICE:
        str = "ChangerGoddess";
        break;
#endif
#if TCFG_AUDIO_ANC_ENABLE
    case APP_MSG_ANC_SWITCH:
        str = "anc_next";
        break;
    case APP_MSG_ANC_ON:
        str = "anc_on";
        break;
    case APP_MSG_ANC_OFF:
        str = "anc_off";
        break;
    case APP_MSG_ANC_TRANS:
        str = "anc_trans";
        break;
    case APP_MSG_EAR_ADAPTIVE_OPEN:
        str = "anc_adap";
        break;
#endif
#if TCFG_AUDIO_SPATIAL_EFFECT_ENABLE
    case APP_MSG_SPATIAL_EFFECT_SWITCH:
        str = "spatial_eff";
        break;
#endif
    case APP_MSG_SPEAK_TO_CHAT_SWITCH:
        str = "spk_char";
        break;
    case APP_MSG_WAT_CLICK_SWITCH:
        str = "wat_click";
        break;
    case APP_MSG_WIND_DETECT_SWITCH:
        str = "wind_det";
        break;

    }
    if (str) {
        app_key_dut_send(NULL, str);
    }
}

#endif /*TCFG_APP_KEY_DUT_ENABLE*/
