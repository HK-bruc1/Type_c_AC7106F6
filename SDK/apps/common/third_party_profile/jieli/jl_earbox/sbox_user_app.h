#ifndef SBOX_USER_APP_H
#define SBOX_USER_APP_H

#include "btstack/le/att.h"
#include "sbox_core_api.h"

struct sbox_state_info {
    bool ble_conn_state;
    bool bt_conn_state;
    bool ble_into_no_lantacy;
    bool phone_call_mute;
    bool music_states;
    bool game_mode;
    u8 box_addr[6];
};
struct user_time {
    u16 years;
    u8 months;
    u8 days;
    u8 hours;
    u8 min;
    u8 second;
};
struct custom_music_info {
    u16 time;
    u8 type_artist_name;
    u8 name_len;
    u8 type_album_name;
    u8 album_len;
    u8 type_title;
    u8 title_len;
    u8 artist_name[256];
    u8 album_name[256];
    u8 title[256];
};

struct s_box_app_cb {
    void (*sbox_sync_all_info)(void);               //同步所有需要显示状态的耳机信息给仓
    void (*sbox_sync_bt_con_state)(u8);           //同步的经典蓝牙状态给仓
    void (*sbox_sync_ble_con_state)(u8);          //同步BLE状态给仓
    void (*sbox_sync_battery_info)(void);           //同步电量信息给仓
    void (*sbox_sync_volume_info)(void);            //同步音乐状态音量给仓
    void (*sbox_sync_time_info)(void *data);        //同步手机时间给仓
    void (*sbox_sync_eq_info)(void);                //同步当前的eq信息给仓
    void (*sbox_sync_anc_info)(void);               //同步当前ANC状态给仓
    void (*sbox_sync_call_state)(u8 data);          //同步当前通话状态给仓
    void (*sbox_sync_phone_call_info)(void);        //同步当前联系人的信息给仓
    void (*sbox_sync_key_info)(void);               //同步按键信息给仓
    void (*sbox_sync_edr_info)(void);               //同步仓发射器蓝牙状态信息
    void (*sbox_sync_lyric_info)(void *);             //同步手机歌词信息
    void (*sbox_sync_play_mode_info)(void);         //同步低延时模式信息

    void (*sbox_control_volume)(void *data);                //仓设置播歌音量
    void (*sbox_control_music_state)(void *data);           //仓设置音乐播放暂停
    void (*sbox_control_anc)(void *data);                   //仓设置anc状态
    void (*sbox_control_eq)(void *data);                    //仓设置eq信息
    void (*sbox_control_key)(void *data);                   //仓设置自定义按键
    void (*sbox_control_siri)(void *data);                  //仓设置语音助手
    void (*sbox_control_playmode)(void *data);              //仓设置低延时模式
    void (*sbox_control_phone_call)(void *data);            //仓设置通话状态，接听挂断
    void (*sbox_control_phone_out)(void *data, u8 len);            //仓设置电话呼出
    void (*sbox_control_tiktok)(void *data);                //抖音点赞，上下滑
    void (*sbox_control_photo)(void *data);                 //仓控制拍照
    void (*sbox_control_edr_conn)(void *data);              //仓同步发射器信息
    void (*sbox_control_language)(void *data);              //仓设置提示音语种
    void (*sbox_control_find_phone)(void *data);            //播提示音查找耳机
    void (*sbox_control_ble_lantacy)(void *data);           //仓设置耳机BLE忽略包，用于快速响应
    void (*sbox_control_alarm_tone_play)(void *data);       //仓设置闹钟提示音
    void (*sbox_control_bt_name)(void *data);               //仓设置耳机经典蓝牙
};

typedef struct __custom_edr_info {
    u8 emitter_addr[6];

} custom_edr_info __attribute__((aligned(4)));



extern struct sbox_state_info sbox_user_info;
extern struct user_time phone_time;
extern struct custom_music_info local_music_info;



/*-------------------CMD start----------------------*/
#define CUSTOM_ALL_INFO_CMD                             0xff  // 接受仓的所有信息，或同步耳机的所有信息的CMD
#define CUSTOM_BT_CONNECT_STATE_CMD                     0x1  // bt连接状态
#define CUSTOM_BLE_CONNECT_STATE_CMD                    0x2  // ble连接状态
#define CUSTOM_BLE_BATTERY_STATE_CMD                    0x3  // 电量信息
#define CUSTOM_BLE_VOLUMEN_CMD                          0x4  // 音量信息
#define CUSTOM_BLE_TIME_DATE_CMD                        0x5  // 时间信息
#define CUSTOM_EQ_DATE_CMD                              0x6  // EQ信息
#define CUSTOM_ANC_DATE_CMD                             0x7  // ANC信息
#define CUSTOM_CALL_STATEE_CMD                          0X08    //通话状态
#define CUSTOM_PHONE_CALL_INFO_CMD                      0X09    //通话记录信息同步
#define CUSTOM_KEY_INFO_CMD                             0X0A    //自定义按键设置同步

#define CUSTOM_BLE_VOL_CONTROL_CMD                      0x32 // 音量控制
#define CUSTOM_BLE_MUSIC_STATE_CONTROL_CMD              0x33 // 音乐状态控制
#define CUSTOM_BLE_ANC_MODE_CONTROL_CMD                 0x34 // ANC 模式控制
#define CUSTOM_BLE_EQ_MODE_CONTROL_CMD                  0x35 // EQ              0-4
#define CUSTOM_BLE_PLAY_MODE_CONTROL_CMD                0x36 // 设置播放模式
#define CUSTOM_BLE_ALARM_CLOCK_CONTROL_CMD              0x37 // 设置闹钟
#define CUSTOM_BLE_FINE_EARPHONE_CMD                    0x38 // 查找手机
#define CUSTOM_BLE_FLASHLIGHT_CONTROL_CMD               0x39 // 手电筒
#define CUSTOM_BLE_SWITCH_LANGUAGE                      0x40 //切换中英文
#define CUSTOM_BLE_CONTRAL_CALL                         0x41 //控制通话状态
#define CUSTOM_BLE_USER_ADD_CMD                         0X42 //用户自定义命令
#define CUSTOM_BLE_CONTRAL_TILTOK                       0X43  //控制抖音操作
#define CUSTOM_BLE_CONTRAL_PHOTO                        0X44  //控制拍照操作

#define CUSTOM_EDR_CONTRAL_CONN                         0X45  //控制edr连接
#define CUSTOM_EDR_SYNC_INFO                            0X46  //同步经典蓝牙信息
#define CUSTOM_EDR_SIRI_CTRL                            0X47  //控制语音助手
#define CUSTOM_BLE_CONTRAL_PHONEOUT                     0X48    //拨出电话
#define CUSTOM_BLE_CONTRAL_KEY                          0X49    //按键同步设置
#define CUSTOM_SLEEP_CTRL_CMD                           0x4A    //屏幕亮灭状态
#define CUSTOM_EDR_CLEAR_COMP                           0x4B    //br等待仓连接
#define CUSTOM_BLE_CONTRAL_BTNAME                       0X50    //蓝牙名设置
// #define CUSTOM_BLE_EQ_CUSTOM_CONTROL_CMD                0x51   // EQ自定义信息设置、切换


//响铃设置
#define RING_CH_L   0x10                        //左耳响铃
#define RING_CH_R   0x01                        //右耳响铃
#define RING_CH_LR   0x11                       //对耳响铃
#define RING_CH_LR_ALL_OFF   0x03               //关闭响铃
#define RING_CH_LR_ALARM   0x12                 //对耳闹钟响

//响铃和闹钟提示音
#define TONE_FIDN_EAR			SDFILE_RES_ROOT_PATH"tone/find_tone.*"
#define TONE_ALARM			    SDFILE_RES_ROOT_PATH"tone/alarm.*"
// #define SOFT_VERSION            "1.0.2"

//通话状态同步标志
#define     SBOX_CALL_HANDUP        1
#define     SBOX_CALL_INCOME        2
#define     SBOX_CALL_ACTIVE        3

#define AVC_PLAY			0x44
#define AVC_STOP			0x45
#define AVC_PAUSE			0x46

//tws同步执行指令
enum {
    SYNC_CMD_EQ_SWITCH_0,
    SYNC_CMD_EQ_SWITCH_1,
    SYNC_CMD_EQ_SWITCH_2,
    SYNC_CMD_EQ_SWITCH_3,
    SYNC_CMD_EQ_SWITCH_4,

    SYNC_CMD_ANC_ON,
    SYNC_CMD_ANC_OFF,
    SYNC_CMD_ANC_TRANS,
    SYNC_CMD_VOLUME_UP,
    SYNC_CMD_VOLUME_DOWN,
    SYNC_CMD_CALL_MUTE,
    SYNC_CMD_SBOX_POWER_OFF_TOGETHER,
    SYNC_CMD_RESET_MODE,
};

enum {
    SYNC_CMD_HID_CHANNEL_ID,
    SYNC_CMD_EMITTER_ADDR,
    SYNC_CMD_CHANGE_LMP_ADDR,
    SYNC_CMD_ADDR_CHANGE_FLAG,
    SYNC_CMD_CLEAN_EMITTER_LINKKEY,
    SYNC_CMD_REMOTE_TYPE,
    SYNC_CMD_BOX_BLE_INFO,
    SYNC_CMD_REMOTE_ADDR,
};


void custom_sync_all_info_to_box(void);
void custom_sync_eq_info_to_box(void);
void custom_sync_anc_info_to_box(void);
void custom_sync_bt_connect_state(u8 value);
void custom_sync_ble_connect_state(u8 value);
void custom_sync_vbat_percent_state(void);
void custom_sync_volume_state(void);
void custom_sync_music_state(void);
void custom_sync_key_setting(void);
void custom_sync_phone_call_info(void);
void custom_sync_edr_info(void);
void custom_sync_time_info(void *data);

void custom_control_eq_mode(void *mode);
void custom_control_anc_mode(void *mode);
void custom_control_volume(void *data);
void custom_control_phone_call(void *datas);
// void custom_control_play_mode(void *datas);
void custom_control_find_ear_tone(void *datas);
void custom_sync_call_state(u8 data);
void custom_set_find_phone(void *datas);
void custom_ble_into_no_latency(void *datas);
void custom_control_phone_out(void *data, u8 len);
void custom_sync_wait_emitter_conn(void);
void custom_control_music_state(void *data);
void custom_ble_into_no_latency(void *data);
void custom_sync_lyric_info(void *data);
void custom_control_photo(void *data);
void custom_control_tiltok(void *data);

void sbox_phone_music_info_deal(u8 type, u32 time, u8 *info, u16 len);
void sbox_phone_date_and_time_feedback(u8 *data, u16 len);
void sbox_map_get_time_data(char *time);
void sys_smartstore_event_handle(struct box_info *boxinfo);

extern struct s_box_app_cb sbox_cb_func;

#endif
