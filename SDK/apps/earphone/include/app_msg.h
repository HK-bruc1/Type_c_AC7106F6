#ifndef APP_MSG_H
#define APP_MSG_H


#include "os/os_api.h"

enum {
    MSG_FROM_KEY =  Q_MSG + 1,

    MSG_FROM_TWS,
    MSG_FROM_BT_STACK,
    MSG_FROM_BT_HCI,

    MSG_FROM_EARTCH,

    MSG_FROM_BATTERY,
    MSG_FROM_CHARGE_STORE,
    MSG_FROM_TESTBOX,
    MSG_FROM_ANCBOX,
    MSG_FROM_CHARGE_CLIBRATION,

    MSG_FROM_PWM_LED,

    MSG_FROM_TONE,

    MSG_FROM_APP,

    MSG_FROM_AUDIO,

    MSG_FROM_OTA,

    MSG_FROM_CI_UART,
    MSG_FROM_CDC,
    MSG_FROM_CDC_DATA,
    MSG_FROM_CFGTOOL_TWS_SYNC,

    MSG_FROM_DEVICE,

    MSG_FROM_RCSP,
    MSG_FROM_RCSP_BT,
    MSG_FROM_TWS_UPDATE_NEW,

    MSG_FROM_IOT,
    MSG_FROM_BIG,
    MSG_FROM_CIG,
    MSG_FROM_NET_BT,

    MSG_FROM_LOCAL_TWS,
};


struct app_msg_handler {
    int owner;
    int from;
    int (*handler)(int *msg);
};


#define APP_MSG_PROB_HANDLER(msg_handler) \
    const struct app_msg_handler msg_handler sec(.app_msg_prob_handler)

extern const struct app_msg_handler app_msg_prob_handler_begin[];
extern const struct app_msg_handler app_msg_prob_handler_end[];

#define for_each_app_msg_prob_handler(p) \
    for (p = app_msg_prob_handler_begin; p < app_msg_prob_handler_end; p++)


#define APP_MSG_HANDLER(msg_handler) \
    const struct app_msg_handler msg_handler sec(.app_msg_handler)

extern const struct app_msg_handler app_msg_handler_begin[];
extern const struct app_msg_handler app_msg_handler_end[];

#define for_each_app_msg_handler(p) \
    for (p = app_msg_handler_begin; p < app_msg_handler_end; p++)



#define APP_KEY_MSG_FROM_TWS    1
#define APP_KEY_MSG_FROM_CIS    2

#define APP_MSG_KEY    0x010000

#define APP_MSG_FROM_KEY(msg)   (msg & APP_MSG_KEY)

#define APP_MSG_KEY_VALUE(msg)  ((msg >> 8) & 0xff)

#define APP_MSG_KEY_ACTION(msg)  (msg & 0xff)


#define APP_KEY_MSG_REMAP(key_value, key_action) \
            (APP_MSG_KEY | (key_value << 8) | key_action)


#define APP_EARTCH_MSG_FROM_TWS    1

enum {
    APP_MSG_NULL = 0,

    APP_MSG_WRITE_RESFILE_START,
    APP_MSG_WRITE_RESFILE_STOP,

    APP_MSG_POWER_KEY_LONG,
    APP_MSG_POWER_KEY_HOLD,
    APP_MSG_SYS_TIMER,
    APP_MSG_POWER_ON,
    APP_MSG_POWER_OFF,
    APP_MSG_SOFT_POWEROFF,
    APP_MSG_GOTO_MODE,
    APP_MSG_GOTO_NEXT_MODE,
    APP_MSG_CHANGE_MODE,
    APP_MSG_ENTER_MODE,
    APP_MSG_EXIT_MODE,

    APP_MSG_STATUS_INIT_OK,
    APP_MSG_BT_GET_CONNECT_ADDR,
    APP_MSG_BT_OPEN_PAGE_SCAN,
    APP_MSG_BT_CLOSE_PAGE_SCAN,
    APP_MSG_BT_ENTER_SNIFF,
    APP_MSG_BT_EXIT_SNIFF,
    APP_MSG_BT_A2DP_PAUSE,
    APP_MSG_BT_A2DP_STOP,
    APP_MSG_BT_A2DP_PLAY,
    APP_MSG_BT_A2DP_START,
    APP_MSG_BT_PAGE_DEVICE,
    APP_MSG_BT_IN_PAGE_MODE,
    APP_MSG_BT_IN_PAIRING_MODE,
    APP_MSG_BT_A2DP_OPEN_MEDIA_SUCCESS,
    APP_MSG_BT_STOP_A2DP_SLIENCE,
    APP_MSG_LE_AUDIO_MODE,
    APP_MSG_LE_AUDIO_CALL_OPEN,
    APP_MSG_LE_AUDIO_CALL_CLOSE,
    APP_MSG_BT_OPEN_DUT,
    APP_MSG_BT_CLOSE_DUT,
    APP_MSG_CALL_ANSWER,						// 接听电话
    APP_MSG_HID_CONTROL,						// 拍照
    APP_MSG_CALL_THREE_WAY_ANSWER1,
    APP_MSG_CALL_THREE_WAY_ANSWER2,
    APP_MSG_CALL_SWITCH,
    APP_MSG_CALL_HANGUP,
    APP_MSG_CALL_LAST_NO,
    APP_MSG_OPEN_SIRI,
    APP_MSG_CLOSE_SIRI,
    APP_MSG_LOW_LANTECY,
    APP_MSG_SEND_A2DP_PLAY_CMD,
    APP_MSG_A2DP_NO_ENERGY,

    APP_MSG_TWS_PAIRED,
    APP_MSG_TWS_UNPAIRED,
    APP_MSG_TWS_PAIR_SUSS,
    APP_MSG_TWS_CONNECTED,
    APP_MSG_TWS_DISCONNECTED,
    APP_MSG_TWS_WAIT_PAIR,                      // 等待配对
    APP_MSG_TWS_START_PAIR,                     // 手动发起配对
    APP_MSG_TWS_START_CONN,                     // 开始回连TWS
    APP_MSG_TWS_WAIT_CONN,                      // 等待TWS连接
    APP_MSG_TWS_START_PAIR_AND_CONN_PROCESS,    // 执行配对和连接的默认流程
    APP_MSG_TWS_POWERON_PAIR_TIMEOUT,           // TWS开机配对超时
    APP_MSG_TWS_POWERON_CONN_TIMEOUT,           // TWS开机回连超时
    APP_MSG_TWS_START_PAIR_TIMEOUT,
    APP_MSG_TWS_START_CONN_TIMEOUT,
    APP_MSG_TWS_POWER_OFF,

    //音频相关消息
    APP_MSG_VOL_UP,
    APP_MSG_VOL_DOWN,
    APP_MSG_UAC0_VOL_DOWN,                      //声卡0音量减
    APP_MSG_UAC0_VOL_UP,                        //声卡0音量加
    APP_MSG_UAC1_VOL_DOWN,                      //声卡1音量减
    APP_MSG_UAC1_VOL_UP,                        //声卡1音量加
    APP_MSG_MAX_VOL,
    APP_MSG_MIN_VOL,
    APP_MSG_MIC_OPEN,
    APP_MSG_MIC_CLOSE,
    APP_MSG_ESCO_UNCLE_VOICE,                   //通话上行变男声
    APP_MSG_ESCO_GODDESS_VOICE,                 //通话上行变女声
    APP_MSG_ANC_SWITCH,                         //ANC循环切换
    APP_MSG_ANC_ON,
    APP_MSG_ANC_OFF,
    APP_MSG_ANC_TRANS,                          //通透模式
    APP_MSG_EAR_ADAPTIVE_OPEN,                  //耳道自适应
    APP_MSG_SPATIAL_EFFECT_SWITCH,              //空间音效
    APP_MSG_SPEAK_TO_CHAT_SWITCH,               //智能免摘
    APP_MSG_WAT_CLICK_SWITCH,                   //广域点击
    APP_MSG_WIND_DETECT_SWITCH,                 //风噪检测
    APP_MSG_ADAPTIVE_VOL_SWITCH,                 //音量自适应
    APP_MSG_FIT_DET_SWITCH,
    APP_MSG_IMU_TRIM_START,                     //开始陀螺仪校准
    APP_MSG_IMU_TRIM_STOP,                      //关闭陀螺仪校准
    APP_MSG_SMART_VOICE_EVENT,
    APP_MSG_VOL_CHANGED,
    APP_MSG_SWITCH_SOUND_EFFECT,
    APP_MSG_SPK_EQ_TEST,
    APP_MSG_VOCAL_REMOVE,
    APP_MSG_EQ_CHANGED,
    APP_MSG_MUTE_CHANGED,
    APP_MSG_SYS_MUTE,
    APP_MSG_MIC_EFFECT_ON_OFF,                  //麦克风音效开关
    APP_MSG_SWITCH_MIC_EFFECT,                  //麦克风音效场景切换
    APP_MSG_EARTCH_IN_EAR,                      //耳机佩戴
    APP_MSG_EARTCH_OUT_EAR,                     //耳机摘下

    APP_MSG_KEY_TONE,                           //播放按键音
    APP_MSG_KEY_POWER_OFF,                      //按键关机
    APP_MSG_KEY_POWER_OFF_HOLD,
    APP_MSG_KEY_POWER_OFF_RELEASE,
    APP_MSG_KEY_POWER_OFF_INSTANTLY,

    APP_MSG_LINEIN_START,

    APP_MSG_MUSIC_PP,
    APP_MSG_MUSIC_PREV,
    APP_MSG_MUSIC_NEXT,
    APP_MSG_MUSIC_CHANGE_DEV,
    APP_MSG_MUSIC_AUTO_NEXT_DEV,
    APP_MSG_MUSIC_PLAYE_NEXT_FOLDER,
    APP_MSG_MUSIC_PLAYE_PREV_FOLDER,
    APP_MSG_MUSIC_PLAY_FIRST,
    APP_MSG_MUSIC_PLAY_REC_FOLDER_SWITCH,
    APP_MSG_MUSIC_MOUNT_PLAY_START,
    APP_MSG_MUSIC_PLAY_START,
    APP_MSG_MUSIC_PLAY_START_BY_SCLUST,
    APP_MSG_MUSIC_PLAY_START_BY_DEV,
    APP_MSG_MUSIC_FR,
    APP_MSG_MUSIC_FF,
    APP_MSG_MUSIC_DEC_ERR,
    APP_MSG_MUSIC_PLAYER_END,
    APP_MSG_MUSIC_CHANGE_REPEAT,
    APP_MSG_MUSIC_SPEED_UP,
    APP_MSG_MUSIC_SPEED_DOWN,
    APP_MSG_MUSIC_PLAYER_AB_REPEAT_SWITCH,
    APP_MSG_MUSIC_MUTE,
    APP_MSG_MUSIC_CHANGE_EQ,
    APP_MSG_MUSIC_PLAY_BY_NUM,
    APP_MSG_MUSIC_PLAY_START_AT_DEST_TIME,
    APP_MSG_MUSIC_FILE_NUM_CHANGED,
    APP_MSG_MUSIC_PLAY_STATUS,

    APP_MSG_REPEAT_MODE_CHANGED,
    APP_MSG_PITCH_UP,
    APP_MSG_PITCH_DOWN,

    APP_MSG_REC_PP,
    APP_MSG_ENC_START,
    APP_MSG_REVERB_OPNE,
    APP_MSG_MIC_VOL_UP,
    APP_MSG_MIC_VOL_DOWN,
    APP_MSG_MIC_BASS_UP,
    APP_MSG_MIC_BASS_DOWN,
    APP_MSG_MIC_TREBLE_UP,
    APP_MSG_MIC_TREBLE_DOWN,

    APP_MSG_PC_AUDIO_PLAY_OPEN,
    APP_MSG_PC_AUDIO_PLAY_CLOSE,
    APP_MSG_PC_AUDIO_MIC_OPEN,
    APP_MSG_PC_AUDIO_MIC_CLOSE,

    //IR_NUM中间不允许插入msg
    APP_MSG_IR_NUM0,
    APP_MSG_IR_NUM1,
    APP_MSG_IR_NUM2,
    APP_MSG_IR_NUM3,
    APP_MSG_IR_NUM4,
    APP_MSG_IR_NUM5,
    APP_MSG_IR_NUM6,
    APP_MSG_IR_NUM7,
    APP_MSG_IR_NUM8,
    APP_MSG_IR_NUM9,
    //IR_NUM中间不允许插入msg

    APP_MSG_INPUT_FILE_NUM,
    APP_MSG_DEL_ALL_REMOTE_DEV, //清除远程配对记录

    APP_MSG_TWS_OPEN_LOCAL_DEC,
    APP_MSG_TWS_CLOSE_LOCAL_DEC,

    //Type-C耳机按键消息
    APP_MSG_PP_KEY_PRESS,
    APP_MSG_UP_KEY_PRESS,
    APP_MSG_DOWN_KEY_PRESS,

    APP_MSG_PP_KEY_LIFT,
    APP_MSG_UP_KEY_LIFT,
    APP_MSG_DOWN_KEY_LIFT,
};


struct key_remap_table {
    u8 key_value;
    const int *remap_table;
    int (*remap_func)(int *event);
};

extern const struct key_remap_table bt_mode_key_table[];
extern const struct key_remap_table idle_mode_key_table[];
extern const struct key_remap_table linein_mode_key_table[];
extern const struct key_remap_table music_mode_key_table[];
extern const struct key_remap_table pc_mode_key_table[];

void app_send_message(int msg, int arg);

void app_send_message2(int _msg, int arg1, int arg2);

void app_send_message_from(int from, int argc, int *msg);

int app_key_event_remap(const struct key_remap_table *table, int *event);

int app_get_message(int *msg, int max_num, const struct key_remap_table *key_table);


#endif

