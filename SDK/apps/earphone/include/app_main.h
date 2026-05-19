#ifndef APP_MAIN_H
#define APP_MAIN_H

#include "app_msg.h"
#include "app_mode_manager/app_mode_manager.h"
#include "poweroff.h"
#include "app_config.h"
#include "bt_background.h"
#include "earphone.h"

enum {
    SYS_POWERON_BY_KEY = 1,
    SYS_POWERON_BY_OUT_BOX,
};

enum {
    SYS_POWEROFF_BY_KEY = 1,
    SYS_POWEROFF_BY_IN_BOX,
    SYS_POWEROFF_BY_TIMEOUT,
};

typedef struct {
    float talk;
    float ff;
    float fb;
} audio_mic_cmp_t;

typedef struct _APP_VAR {
    u8 volume_def_state;
    s16 bt_volume;
    s16 dev_volume;
    s16 music_volume;
    s16 call_volume;
    s16 wtone_volume;
    s16 ktone_volume;
    s16 ring_volume;
#if JL_UNICAST_DUAL_UAC_ENABLE
    u8 uac0_vol;   //双声卡模式下UAC0音量
    u8 uac1_vol;   //双声卡模式下UAC1音量
#endif
    u8 opid_play_vol_sync;
    u8 aec_dac_gain;
    u8 aec_mic_gain;
    u8 aec_mic1_gain;
    u8 aec_mic2_gain;
    u8 aec_mic3_gain;
    u8 rf_power;
    u8 goto_poweroff_flag;
    u16 goto_poweroff_cnt;
    u8 poweroff_sametime_flag;
    u8 play_poweron_tone;
    u8 remote_dev_company;
    u8 siri_stu;
    u8 cycle_mode;
    u8 poweron_reason;
    u8 poweroff_reason;
    u8 update_tone_end_flag;//升级完成后提示音播放结束标志位
    int auto_stop_page_scan_timer;     //用于1拖2时，有一台连接上后，超过三分钟自动关闭Page Scan
    u16 auto_off_time;
    u16 warning_tone_v;
    u16 poweroff_tone_v;
    u16 phone_dly_discon_time;
    u8 usb_mic_gain;
    int wait_timer_do;
    float audio_mic_array_diff_cmp;//麦克风阵列校准补偿值
    u8 audio_mic_array_trim_en; //麦克风阵列校准
    audio_mic_cmp_t audio_mic_cmp;
    float enc_degradation;//default:1,range[0:1]
    u32 start_time;
    s16 mic_eff_volume;
} APP_VAR;

struct bt_mode_var {
    //phone
    u8 phone_ring_flag;
    u8 phone_num_flag;
    u8 phone_income_flag;
    u8 phone_call_dec_begin;
    u8 phone_ring_sync_tws;
    u8 phone_ring_addr[6];

    u8 inband_ringtone;
    u8 phone_vol;
    u16 phone_timer_id;
    u16 dongle_check_a2dp_timer;
    u8 dongle_addr[6];
    u8 last_call_type;
    u8 income_phone_num[30];
    u8 income_phone_len;
    s32 auto_connection_counter;
    int auto_connection_timer;
    u8 auto_connection_addr[6];
    int tws_con_timer;
    u8 tws_start_con_cnt;
    u8 tws_conn_state;
    bool search_tws_ing;
    int sniff_timer;
    bool fast_test_mode;
    u16 exit_check_timer;
    u8 init_start; //蓝牙协议栈已经开始初始化标志位
    u8 init_ok; //蓝牙初始化完成标志
    u8 exiting; //蓝牙正在退出
    u8 wait_exit; //蓝牙等待退出
    u8 ignore_discon_tone;  // 1-退出蓝牙模式， 不响应discon提示音
    u8 bt_dual_conn_config;
    u8 control_device_type;  //用于区分1T2场景想要控制哪个连接类型的设备
    background_var background;  //蓝牙后台相关变量
    u16 get_music_player_timer;
};

typedef struct _BT_USER_COMM_VAR {
} BT_USER_COMM_VAR;

extern APP_VAR app_var;
extern struct bt_mode_var g_bt_hdl;

enum app_mode_t {
    APP_MODE_POWERON,
    APP_MODE_IDLE,
    APP_MODE_BT,
    APP_MODE_MUSIC,
    APP_MODE_LINEIN,
    APP_MODE_PC,
    APP_MODE_SINK,
};

enum app_mode_index {
    APP_MODE_BT_INDEX,
    APP_MODE_MUSIC_INDEX,
    APP_MODE_LINEIN_INDEX,
    APP_MODE_PC_INDEX,
};

enum {
    CONTROL_ALL,  //不限制连接类型进行控制
    CONTROL_EDR,  //控制edr设备
    CONTROL_CIS,
    CONTROL_BIS,
};

#define earphone (&bt_user_priv_var)

void app_power_off(void *priv);
void bt_bredr_enter_dut_mode(u8 mode, u8 inquiry_scan_en);
void bt_bredr_exit_dut_mode();
u8 get_charge_online_flag(void);
u8 check_local_not_accept_sniff_by_remote();

struct app_mode *app_mode_switch_handler(int *msg);

#endif
