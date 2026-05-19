#include "sbox_core_config.h"
#include "sbox_user_app.h"
#include "classic/tws_api.h"
#include "audio_anc.h"
#include "btstack/bluetooth.h"
#include "audio_config.h"
#include "btstack/avctp_user.h"
#include "event.h"
#include "app_main.h"
// #include "le_multi_trans.h"
#include "system/includes.h"
#include "app_msg.h"
#include "app_tone.h"
#include "sbox_connect_emitter.h"
#include "sbox_eq_switch.h"
#include "user_video_ctr.h"
#include "battery_manager.h"
#include "app_power_manage.h"
#include "a2dp_player.h"
#include "bt_tws.h"
#include "bt_key_func.h"
#include "poweroff.h"

#if (THIRD_PARTY_PROTOCOLS_SEL & JL_SBOX_EN)

#if 1
#define log_info(x, ...)  printf("[sbox_app]" x " ", ## __VA_ARGS__)
#define log_info_hexdump  put_buf
#else
#define log_info(...)
#define log_info_hexdump(...)
#endif

struct user_time phone_time;
struct sbox_state_info sbox_user_info;
struct custom_music_info local_music_info;


// u16 check_handle=0;
void smartbox_tone_app_message_deal(int opcode, int err);
struct s_box_app_cb sbox_cb_func = {
    .sbox_sync_all_info = custom_sync_all_info_to_box,
    .sbox_sync_volume_info = custom_sync_volume_state,
    .sbox_sync_time_info = custom_sync_time_info,
    .sbox_sync_bt_con_state = custom_sync_bt_connect_state,
    .sbox_sync_ble_con_state = custom_sync_ble_connect_state,
    .sbox_sync_battery_info = custom_sync_vbat_percent_state,
    .sbox_sync_eq_info = custom_sync_eq_info_to_box,
    .sbox_sync_anc_info = custom_sync_anc_info_to_box,
    .sbox_sync_call_state = custom_sync_call_state,
    .sbox_sync_key_info = custom_sync_key_setting,
    .sbox_sync_edr_info = custom_sync_edr_info,
    .sbox_sync_phone_call_info = custom_sync_phone_call_info,
    .sbox_sync_lyric_info =  custom_sync_lyric_info,

    .sbox_control_volume = custom_control_volume,
    .sbox_control_music_state = custom_control_music_state,
    .sbox_control_anc = custom_control_anc_mode,
    .sbox_control_eq = custom_control_eq_mode_switch,
    .sbox_control_key = NULL,//custom_control_key_setting,
#if (!TCFG_BT_DUAL_CONN_ENABLE)
    .sbox_control_tiktok = custom_control_tiltok,
    .sbox_control_photo = custom_control_photo,
#endif
    // .sbox_control_playmode = custom_control_play_mode,
    .sbox_control_language = NULL,//custom_control_tone_language,
    .sbox_control_find_phone =  custom_control_find_ear_tone,
    .sbox_control_bt_name = NULL,//custom_control_bt_name,
    .sbox_control_phone_out = custom_control_phone_out,
    .sbox_control_phone_call = custom_control_phone_call,
    .sbox_control_ble_lantacy = custom_ble_into_no_latency,
    .sbox_control_alarm_tone_play = NULL,//custom_control_alarm_toneplay,
    .sbox_control_edr_conn = NULL, //custom_control_edr_conn,
};

void sbox_app_init(void)
{
    sbox_tr_cbuf_init();
    emitter_last_phone_mac_init();
    sbox_eq_init();
}


#if BLE_PROTECT_DEAL

bool user_ctrl_tws_role_switch;
int user_ctrl_tws_role_switch_timer = 0;
void user_check_ble_role_switch_succ(void)
{
    tws_api_auto_role_switch_enable();
    user_ctrl_tws_role_switch = 0;
}

#endif

//同步耳机所有的协议信息
__attribute__((weak))
void custom_sync_all_info_to_box(void)
{
    u8 all_info[7];
    u8 channel = tws_api_get_local_channel();

    all_info[0] = bt_get_total_connect_dev();
    all_info[1] = sbox_get_eq_index();
#if CONFIG_ANC_ENABLE
    all_info[2] = anc_mode_get();
#endif
    all_info[3] = a2dp_player_runing() ? 1 : 2; //sbox_user_info.music_states;
    all_info[4] = app_audio_get_volume(APP_AUDIO_STATE_MUSIC);
    if ('L' == channel) {
        all_info[5] = 10 * (battery_value_to_phone_level() + 1); //get_vbat_percent();
        all_info[6] = get_tws_sibling_bat_persent();
    } else if ('R' == channel) {
        all_info[5] = get_tws_sibling_bat_persent();
        all_info[6] = 10 * (battery_value_to_phone_level() + 1); //get_vbat_percent();
    } else {
        all_info[5] = 10 * (battery_value_to_phone_level() + 1); //get_vbat_percent();
        all_info[6] = 10 * (battery_value_to_phone_level() + 1);
    }
    // memcpy(all_info+6,&sbox_user_c_info.eq_mode_info,11);
    // memcpy(all_info+17,&sbox_user_c_info.default_key_set,8);
    log_info("custom_sync_all_info_to_box %c L :%d R:%d CON:%d anc:%d a2dp:%d vol:%d\n", channel, all_info[5], all_info[6], all_info[0], all_info[2], all_info[3], all_info[4]);
    sbox_ble_att_send_data(CUSTOM_ALL_INFO_CMD, all_info, sizeof(all_info));
}

//同步耳机播歌音量给仓
__attribute__((weak))
void custom_sync_volume_state(void)
{
    log_info("%s \n", __func__);
    u8 cur_vol = 0;
    cur_vol = app_audio_get_volume(APP_AUDIO_STATE_MUSIC);
    sbox_ble_att_send_data(CUSTOM_BLE_VOLUMEN_CMD, &cur_vol, sizeof(cur_vol)); //将当前音量发给手表
}


//同步经典蓝牙状态给仓
__attribute__((weak))
void custom_sync_bt_connect_state(u8 value)
{
    log_info("custom_sync_bt_connect_state%d\n", value);
    u8 bt_conn_state = value;
    sbox_ble_att_send_data(CUSTOM_BT_CONNECT_STATE_CMD, &bt_conn_state, sizeof(bt_conn_state)); //将当前蓝牙状态回复给手表
}
//同步BLE状态给仓
__attribute__((weak))
void custom_sync_ble_connect_state(u8 value)
{
    log_info("%s \n", __func__);
    u8 btl_conn_state = value;
    sbox_ble_att_send_data(CUSTOM_BLE_CONNECT_STATE_CMD, &btl_conn_state, sizeof(btl_conn_state)); //将当前BLE状态回复给手表
}
//同步EQ信息给仓
__attribute__((weak))
void custom_sync_eq_info_to_box(void)
{
    log_info("%s \n", __func__);
    u8 eq_info = sbox_get_eq_index();
    sbox_ble_att_send_data(CUSTOM_EQ_DATE_CMD, &eq_info, sizeof(eq_info));
}

//同步anc信息给仓
__attribute__((weak))
void custom_sync_anc_info_to_box(void)
{
    log_info("%s \n", __func__);
    u8 anc_info = 0; //anc_mode_get();
    sbox_ble_att_send_data(CUSTOM_ANC_DATE_CMD, &anc_info, sizeof(anc_info));
}

//同步耳机电量状态给仓
__attribute__((weak))
void custom_sync_vbat_percent_state(void)
{
    u8 bat_state[3];
    u8 channel = tws_api_get_local_channel();

    // byte0:L耳机电量---- byte1：R耳机电量
    if ('L' == channel) {
        bat_state[0] = 10 * (battery_value_to_phone_level() + 1); //get_vbat_percent();
        bat_state[1] = get_tws_sibling_bat_persent();
    } else if ('R' == channel) {
        bat_state[0] = get_tws_sibling_bat_persent();
        bat_state[1] = 10 * (battery_value_to_phone_level() + 1); //get_vbat_percent();
    } else {
        bat_state[0] = 10 * (battery_value_to_phone_level() + 1); //get_vbat_percent();
        bat_state[1] = 0xff;
    }
    bat_state[2] = bt_get_total_connect_dev();

    log_info("sscustom_sync_vbat_percent_state bat_states:%d %d, vbat_percent:%d, tws_vbat_percent:%d", bat_state[0], bat_state[1], get_vbat_percent(), get_tws_sibling_bat_persent());
    sbox_ble_att_send_data(CUSTOM_BLE_BATTERY_STATE_CMD, bat_state, sizeof(bat_state)); //将当前电量状态回复给手表
}

//同步耳机播歌状态给仓
__attribute__((weak))
void custom_sync_music_state(void)
{
    log_info("%s \n", __func__);
    u8 music_state = sbox_user_info.music_states;
    sbox_ble_att_send_data(CUSTOM_BLE_MUSIC_STATE_CONTROL_CMD, &music_state, sizeof(music_state)); //将当前音乐状态回复给手表
}

//同步耳机自定义按键状态给仓
__attribute__((weak))
void custom_sync_key_setting(void)
{
    log_info("%s \n", __func__);
}

//同步手机连接耳机后的通话信息给仓
__attribute__((weak))
void custom_sync_call_state(u8 states)
{
    // log_info("%s \n",__func__);
    u8 state = states;
    log_info("%s %d \n", __func__, state);
    sbox_ble_att_send_data(CUSTOM_CALL_STATEE_CMD, &state, sizeof(u8));
}

//同步耳机信息给给发射器
__attribute__((weak))
void custom_sync_edr_info(void)
{
    log_info("%s \n", __func__);
}

//同步歌词信息给仓
__attribute__((weak))
void custom_sync_lyric_info(void *data)
{
    log_info("%s \n", __func__);
    u8 music_buf[1024];
    struct custom_music_info *my_musics = (struct custom_music_info *)data;
    u8 offset = 2;
    music_buf[0] = 0xee;
    music_buf[1] = 0xbb;
    memcpy(music_buf + offset, my_musics, 8);
    offset += 8;
    memcpy(music_buf + offset, my_musics->artist_name, my_musics->name_len);
    offset += my_musics->name_len;
    memcpy(music_buf + offset, my_musics->album_name, my_musics->album_len);
    offset += my_musics->album_len;
    memcpy(music_buf + offset, my_musics->title, my_musics->title_len);
    offset += my_musics->title_len;
    // sbox_ble_att_send_data(CUSTOM_CALL_STATEE_CMD, music_buf, offset);
    ble_user_cmd_prepare(BLE_CMD_ATT_SEND_DATA, 4, sbox_func_hdl.sbox_ble_send_handle, music_buf, offset, sbox_func_hdl.sbox_handle_type);
    //  sbox_demo_ble_send(music_buf, offset);
    //custom_send_data_to_box(music_buf, offset);
    //app_ble_att_send_data(sbox_demo_ble_hdl, ATT_CHARACTERISTIC_ae99_01_CLIENT_CONFIGURATION_HANDLE, data, len, ATT_OP_NOTIFY);
}




/*---------------------------------------  功能控制类 -----------------------------------*/

//仓设置耳机播歌的EQ模式,   len:1byte
__attribute__((weak))
void custom_control_eq_mode(void *data)
{
    log_info("%s \n", __func__);
    // custom_eq_param_index_switch(*data);
}

//仓设置耳机anc模式         len:1byte
__attribute__((weak))
void custom_control_anc_mode(void *data)
{
#if CONFIG_ANC_ENABLE
    u8 *datas = (u8 *)data;
    log_info("%s \n", __func__);
    if (get_tws_sibling_connect_state()) {
        if (*datas == 0x01) {
            tws_api_sync_call_by_uuid(0xA2122623, SYNC_CMD_ANC_OFF, 200);
        } else if (*datas == 0x02) {
            tws_api_sync_call_by_uuid(0xA2122623, SYNC_CMD_ANC_ON, 200);
        } else if (*datas == 0x03) {
            tws_api_sync_call_by_uuid(0xA2122623, SYNC_CMD_ANC_TRANS, 200);
        }
    } else {
        if (*datas == 0x01) {
            anc_mode_switch(ANC_OFF, 1);
        } else if (*datas == 0x02) { //
            anc_mode_switch(ANC_ON, 1);
        } else if (*datas == 0x03) {
            anc_mode_switch(ANC_TRANSPARENCY, 1);
        }
    }
#endif
}

//仓设置耳机播歌音量
__attribute__((weak))
void custom_control_volume(void *datas)
{
    log_info("%s \n", __func__);
    u8 *data = (u8 *)datas;
    if (get_tws_sibling_connect_state()) {
        if (*data == 1) {
            tws_api_sync_call_by_uuid(0xA2122623, SYNC_CMD_VOLUME_UP, 100);
        } else {
            tws_api_sync_call_by_uuid(0xA2122623, SYNC_CMD_VOLUME_DOWN, 100);
        }
    } else {
        if (*data == 1) {
            bt_volume_up(1);
        } else {
            bt_volume_down(1);
        }
        sbox_cb_func.sbox_sync_volume_info();
        // set_lanugae_ver(data);

    }
}

//仓设置播歌状态，播放暂停、上下曲
__attribute__((weak))
void custom_control_music_state(void *data)
{
    u8 *bt_addr = NULL;
    u8 *datas = (u8 *)data;
#if TCFG_BT_DUAL_CONN_ENABLE

    u8 addr[6];
    if (a2dp_player_get_btaddr(addr)) {
        bt_addr = addr;
    } else {
        bt_addr = bt_get_current_remote_addr();
    }
    if (*datas == 0x01) { // 播放bt_cmd_prepare
        bt_cmd_prepare_for_addr(bt_addr, USER_CTRL_AVCTP_OPID_PLAY, 0, NULL);
    } else if (*datas == 0x02) { // 暂停
        bt_cmd_prepare_for_addr(bt_addr, USER_CTRL_AVCTP_OPID_PLAY, 0, NULL);
    } else if (*datas == 0x03) { // 上一曲
        bt_cmd_prepare_for_addr(bt_addr, USER_CTRL_AVCTP_OPID_PREV, 0, NULL);
    } else if (*datas == 0x04) { // 下一曲
        bt_cmd_prepare_for_addr(bt_addr, USER_CTRL_AVCTP_OPID_NEXT, 0, NULL);
    } else {
        log_info("CUSTOM_BLE_MUSIC_STATE_CONTROL_CMD volue:%x valid!!!!\n", *datas);
    }
#else
    if (*datas == 0x01) { // 播放bt_cmd_prepare
        bt_cmd_prepare(USER_CTRL_AVCTP_OPID_PLAY, 0, NULL);
        // bt_cmd_prepare_for_addr(bt_addr, USER_CTRL_AVCTP_OPID_PLAY, 0, NULL);
    } else if (*datas == 0x02) { // 暂停
        bt_cmd_prepare(USER_CTRL_AVCTP_OPID_PLAY, 0, NULL);
    } else if (*datas == 0x03) { // 上一曲
        bt_cmd_prepare(USER_CTRL_AVCTP_OPID_PREV, 0, NULL);
    } else if (*datas == 0x04) { // 下一曲
        bt_cmd_prepare(USER_CTRL_AVCTP_OPID_NEXT, 0, NULL);
    } else {
        log_info("CUSTOM_BLE_MUSIC_STATE_CONTROL_CMD volue:%x valid!!!!\n", *datas);
    }
#endif
}


//仓设置耳机自定义按键设置
__attribute__((weak))
void custom_control_key_setting(void *data)
{
    log_info("%s \n", __func__);
}

#if (!TCFG_BT_DUAL_CONN_ENABLE)
//设置手机的抖音点赞
__attribute__((weak))
void custom_control_tiltok(void *datas)
{
    log_info("%s \n", __func__);
    u8 *data = (u8 *)datas;
#if TCFG_USER_EDR_ENABLE
    sbox_ctrl_tiktok(data[0]);//抖音点赞操作接口
#endif
}

//仓设置手机是否拍照
__attribute__((weak))
void custom_control_photo(void *data)
{
    log_info("%s \n", __func__);
#if TCFG_USER_EDR_ENABLE
    key_take_photo();
#endif
}
#endif

// //仓设置耳机进出低延时模式
// __attribute__((weak))
// void custom_control_play_mode(void *datas)
// {
//     log_info("%s \n",__func__);
//     u8 *data =(u8 *)datas;
//      if(*data == 1){
//         bt_set_low_latency_mode(1, 1, 300);
//     }else if(*data==2) {
//         bt_set_low_latency_mode(0, 1, 300);
//     }
//     // custom_sync_game_mode_state();
// }

//仓设置耳机默认提示音语种
__attribute__((weak))
void custom_control_tone_language(void *data)
{
    log_info("%s \n", __func__);
    //to do
}
//仓设置查找耳机
__attribute__((weak))
void custom_control_find_ear_tone(void *datas)
{
    log_info("%s \n", __func__);
    u8 *data = (u8 *)datas;
    if (get_tws_sibling_connect_state()) {
        tws_api_sync_call_by_uuid(0x23091217, *data, 300);
    } else {
        smartbox_tone_app_message_deal(*data, 0);
    }
}

//仓设置耳机蓝牙名字
__attribute__((weak))
void custom_control_bt_name(void *data)
{
    log_info("%s \n", __func__);
    // sbox_set_bt_name_setting(*data);//todo
}

//仓设置耳机呼出指定电话
__attribute__((weak))
void custom_control_phone_out(void *data, u8 len)
{
    log_info("%s \n", __func__);
    u8 *number = (u8 *)data;
    if (len < 3 || len > 30) {
        log_info("custom_phone_out len error %d\n", len);
        return;
    }
    put_buf(number, len);
    bt_cmd_prepare(USER_CTRL_HFP_DIAL_NUMBER, len, number);
}


void custom_sync_wait_emitter_conn(void)
{
    log_info("%s \n", __func__);
    u8 data = 1;
    sbox_ble_att_send_data(CUSTOM_EDR_CLEAR_COMP, &data, sizeof(data)); //将当前音乐状态回复给手表
}

//仓设置耳机播放闹钟提示音
__attribute__((weak))
void custom_control_alarm_toneplay(void *data)
{
    log_info("%s \n", __func__);
}

//仓设置断开手机连接，回连仓播歌
__attribute__((weak))
void custom_control_edr_conn(void *data)
{
    log_info("%s \n", __func__);
}
//仓设置断开手机连接，回连仓播歌
__attribute__((weak))
void custom_sync_time_info(void *data)
{
    struct user_time *my_times = (struct user_time *)data;
    sbox_ble_att_send_data(CUSTOM_BLE_TIME_DATE_CMD, (u8 *)my_times, sizeof(struct user_time));
}

//仓控制耳机通话接听挂断
__attribute__((weak))
void custom_control_phone_call(void *datas)
{
    log_info("%s \n", __func__);
    u8 *data = (u8 *)datas;
    u8 state = bt_get_call_status();
    if (state >= BT_CALL_INCOMING && state < BT_CALL_HANGUP) {
        switch (*data) {
        case 1:
            bt_cmd_prepare(USER_CTRL_HFP_CALL_ANSWER, 0, NULL);
            break;
        case 2:
            bt_cmd_prepare(USER_CTRL_HFP_CALL_HANGUP, 0, NULL);
            break;
        case 3:
            if (get_tws_sibling_connect_state()) {
                tws_api_sync_call_by_uuid(0xA2122623, SYNC_CMD_CALL_MUTE, 200);
            } else {
                sbox_user_info.phone_call_mute = (!sbox_user_info.phone_call_mute);
            }
            break;
        case 4:
            if (get_tws_sibling_connect_state()) {
                tws_api_sync_call_by_uuid(0xA2122623, SYNC_CMD_CALL_MUTE, 200);
            } else {
                sbox_user_info.phone_call_mute = (!sbox_user_info.phone_call_mute);
            }
            break;
        default:
            break;
        }
    }
}
//设置BLE不进入低功耗模式，从而提高响应速度，但会增加设备的功耗。
__attribute__((weak))
void custom_ble_into_no_latency(void *enable)
{
    log_info("%s \n", __func__);
    u8 *state = (u8 *)enable;
    // if(get_ble_conn_handle_state()){
    //     log_info("custom_ble_into_no_latency %d\n",enable);
    if (state[0]) {
        ble_op_latency_skip(0x50, 0xffff);
    } else {
        ble_op_latency_skip(0x50, 0);
    }
    // }
}


/*-------------------------------------------查找耳机------------------------------------------------------*/

void find_ear_tone_play(u8 tws)
{
    // todo 播查找耳机提示音
    log_info("find_ear_tone_play\n");
    u8 i = 0;
    ring_player_stop();
    if (tws) {
        //tws_play_ring_file_alone(get_tone_files()->find_ear,400);
    } else {
        //play_ring_file_alone(get_tone_files()->find_ear);
    }

}
void real_alarm_tone_play(u8 tws)
{
    // todo 播闹钟提示音
    log_info("alarm_tone_play\n");
    u8 i = 0;
    ring_player_stop();
    if (tws) {
        tws_play_ring_file_alone(get_tone_files()->power_on, 400);
    } else {
        play_ring_file_alone(get_tone_files()->power_on);
    }
}

static u8 sbox_tone_ctrl_l_en = 0;
static u8 sbox_tone_ctrl_r_en = 0;

#define VOLUME_NODE_CMD_SET_VOL     (1<<4)
static void sbox_tone_set_volume()
{
    printf("func:%s:%d", __FUNCTION__, app_audio_volume_max_query(SysVol_TONE));
    u32 param = VOLUME_NODE_CMD_SET_VOL | app_audio_volume_max_query(SysVol_TONE);
    jlstream_set_node_param(NODE_UUID_VOLUME_CTRLER, "Vol_BtcRing", &param, sizeof(param));
}

static void tws_sbox_tone_callback(int priv, enum stream_event event)
{
    printf("func:%s, event:%d", __FUNCTION__, event);
    if (event == STREAM_EVENT_START) {
        int msg[] = {(int)sbox_tone_set_volume, 0};
        os_taskq_post_type("app_core", Q_CALLBACK, ARRAY_SIZE(msg), msg);
    }
}
REGISTER_TWS_TONE_CALLBACK(tws_sbox_tone_stub) = {
    .func_uuid  = 0x2EC850AF,
    .callback   = tws_sbox_tone_callback,
};

static int sbox_tone_play_callback(void *priv, enum stream_event event)
{
    printf("func:%s, event:%d", __FUNCTION__, event);
    if (event == STREAM_EVENT_START) {
        int msg[] = {(int)sbox_tone_set_volume, 0};
        os_taskq_post_type("app_core", Q_CALLBACK, ARRAY_SIZE(msg), msg);
    }
    return 0;
}


void smartbox_ring_set(u8 en, u8 ch)
{
    if (strcmp(os_current_task(), "app_core")) {
        printf("%s, task:%s", __func__, os_current_task());
        int msg[] = {(int)smartbox_ring_set, 0};
        os_taskq_post_type("app_core", Q_CALLBACK, ARRAY_SIZE(msg), msg);
        return;
    }

    // if(sbox_tone_ctrl_l_en ==0 && sbox_tone_ctrl_r_en ==0 && tws_api_get_role()==1)
    // {
    //     printf("all tone stop slave\n");
    //     return;
    // }
    /* tone_player_stop(); */
    ring_player_stop();

    u8 l_en = sbox_tone_ctrl_l_en;
    u8 r_en = sbox_tone_ctrl_r_en;

    printf("%s, l_en:%d, r_en:%d", __func__, l_en, r_en);

    if (l_en && r_en) {
        if (get_tws_sibling_connect_state()) {
            if (tws_api_get_role() == TWS_ROLE_SLAVE) { //对耳连上后，从机同步播。主机同步播的话，由于从机收到响铃慢，导致停止了提示音
                tws_play_ring_file_alone_callback("tone_en/find_ear.*", 300, 0x2EC850AF);
            }
        } else {
            play_ring_file_alone_with_callback("tone_en/find_ear.*", NULL, sbox_tone_play_callback);
        }
    } else if ((l_en && tws_api_get_local_channel() == 'L') ||
               (r_en && tws_api_get_local_channel() == 'R')) {
        play_ring_file_alone_with_callback("tone_en/find_ear.*", NULL, sbox_tone_play_callback);
    }
}

void smartbox_tone_app_message_deal(int opcode, int err)
{
    switch (opcode) {
    case RING_CH_LR_ALARM:
        log_info("SMARTBOX_RING_ALL");
        sbox_tone_ctrl_l_en = 1;
        sbox_tone_ctrl_r_en = 1;
        smartbox_ring_set(1, RING_CH_LR_ALARM);
        break;
    case RING_CH_LR:
        log_info("SMARTBOX_RING_ALL");
        sbox_tone_ctrl_l_en = 1;
        sbox_tone_ctrl_r_en = 1;
        smartbox_ring_set(1, RING_CH_LR);
        break;
    case RING_CH_R:
        log_info("SMARTBOX_RING_RIGHT");
        sbox_tone_ctrl_l_en = 0;
        sbox_tone_ctrl_r_en = 1;
        smartbox_ring_set(1, RING_CH_R);
        // smartbox_ring_set(0, RING_CH_L);
        break;
    case RING_CH_L:
        log_info("SMARTBOX_RING_LEFT");
        sbox_tone_ctrl_l_en = 1;
        sbox_tone_ctrl_r_en = 0;
        smartbox_ring_set(1, RING_CH_L);
        // smartbox_ring_set(0, RING_CH_R);
        break;
    case RING_CH_LR_ALL_OFF:
        printf("GFPS_RING_STOP_ALL");
        sbox_tone_ctrl_l_en = 0;
        sbox_tone_ctrl_r_en = 0;
        smartbox_ring_set(0, RING_CH_LR);
        break;
    }
}

TWS_SYNC_CALL_REGISTER(tws_smartbox_sync_tone_play) = {
    .uuid = 0x23091217,
    .task_name = "app_core",
    .func = smartbox_tone_app_message_deal,
};

/**
 * @brief 响铃函数
 *
 * @param en 1为响铃，0为不响铃
 * @param ch 设置的通道号
 */
static void smartbox_tone_play_at_same_time(int event, int err)
{
    log_info("%s\n", __func__);
    if (!get_tws_sibling_connect_state()) {
        log_info("%s tws_disconn, ignore", __func__);
        return;
    }
    if (tws_api_get_role() == TWS_ROLE_MASTER && err != TWS_SYNC_CALL_TX) { //主机发送失败，重新发送
        tws_api_sync_call_by_uuid(0x23081717, event, 300);
        return;
    }
    if (event == 2) {
        real_alarm_tone_play(1);
        return;
    }
    //  real_alarm_tone_play(1);
    find_ear_tone_play(event);
}

TWS_SYNC_CALL_REGISTER(tws_smartbox_tone_play) = {
    .uuid = 0x23081717,
    .task_name = "app_core",
    .func = smartbox_tone_play_at_same_time,
};
/*-------------------------------------------查找耳机------------------------------------------------------*/



/*-------------------------------------------获取手机时间------------------------------------------------------*/
#if TCFG_BT_SUPPORT_MAP
char *strtok(char *str, const char *delim)
{
    static char *last;
    if (str == NULL) {
        str = last;
    }
    if (str == NULL) {
        return NULL;
    }
    char *p = str;
    while (*p != '\0' && strchr(delim, *p) == NULL) {
        p++;
    }
    if (*p == '\0') {
        last = NULL;
        return NULL;
    }
    *p = '\0';
    last = p + 1;
    return str;
}
void ios_ascill_to_data(u8 *data)
{
    u8 p[20];
    memset(p, 0, 20);
    sprintf((char *)p, "%s%s", data, "\"");
    data = p;
    put_buf(data, 20);
    char *year = strtok((char *)data, "/");
    char *month = strtok(NULL, "/");
    char *day = strtok(NULL, ",");
    char *hour = strtok(NULL, ":");
    char *min = strtok(NULL, ":");
    char *second = strtok(NULL, "\"");
    memset(&phone_time, 0, sizeof(phone_time));
    printf("ios_ascill_to_data %s %s %s %s %s %s \n", year, month, day, hour, min, second);
    phone_time.years = 2000 + 10 * (year[0] - '0') + (year[1] - '0');
    phone_time.months = 10 * (month[0] - '0') + (month[1] - '0');
    phone_time.days = 10 * (day[0] - '0') + (day[1] - '0');
    phone_time.hours = 10 * (hour[1] - '0') + (hour[2] - '0');
    phone_time.min = 10 * (min[0] - '0') + (min[1] - '0');
    phone_time.second = 10 * (second[0] - '0') + (second[1] - '0');
    printf("ios_ascill_to_data %d %d %d %d %d %d \n", phone_time.years, phone_time.months, phone_time.days, phone_time.hours, phone_time.min, phone_time.second);
    sbox_cb_func.sbox_sync_time_info(&phone_time);
}
void android_ascill_to_data(char *data)
{
    memset(&phone_time, 0, sizeof(phone_time));
    phone_time.years = 1000 * (data[0] - '0') + 100 * (data[1] - '0') + 10 * (data[2] - '0') + (data[3] - '0');
    phone_time.months = 10 * (data[4] - '0') + (data[5] - '0');
    phone_time.days = 10 * (data[6] - '0') + (data[7] - '0');
    phone_time.hours = 10 * (data[9] - '0') + (data[10] - '0');
    phone_time.min = 10 * (data[11] - '0') + (data[12] - '0');
    phone_time.second = 10 * (data[13] - '0') + (data[14] - '0');
    printf("android_ascill_to_data %d %d %d %d %d %d \n", phone_time.years, phone_time.months, phone_time.days, phone_time.hours, phone_time.min, phone_time.second);
    sbox_cb_func.sbox_sync_time_info(&phone_time);
}

#endif

#if TCFG_BT_SUPPORT_MAP
//#define PROFILE_CMD_TRY_AGAIN_LATER 	    -1004
//void bt_get_time_date()
//{
//    int error = bt_cmd_prepare(USER_CTRL_HFP_GET_PHONE_DATE_TIME, 0, NULL);
//    log_info(">>>>>error = %d\n", error);
//    if (error == PROFILE_CMD_TRY_AGAIN_LATER) {
//        sys_timeout_add(NULL, bt_get_time_date, 100);
//    }
//}
//void phone_date_and_time_feedback(u8 *data, u16 len)
//{
//    log_info("%d time：%s ", len,data);
//    put_buf(data,strlen((char *)data));
//    ios_ascill_to_data(data);
//}
void sbox_phone_date_and_time_feedback(u8 *data, u16 len)
{
    ios_ascill_to_data(data);
}
//void map_get_time_data(char *time, int status)
//{
//    if (status == 0) {
//        log_info("time：%s ", time);
//        android_ascill_to_data(time);
//    } else {
//        log_info(">>>map get fail\n");
//        sys_timeout_add(NULL, bt_get_time_date, 100);
//    }
//}
void sbox_map_get_time_data(char *time)
{
    android_ascill_to_data(time);
}
#endif
/*-------------------------------------------获取手机时间------------------------------------------------------*/

/*-------------------------------------------歌词同步-----------------------------------------------------*/
//歌词同步需要more_avctp_cmd_support 置1,并且TCFG_DEC_ID3_V2_ENABLE TCFG_DEC_ID3_V1_ENABLE 都需要打开;同时在bredr_handle_register()里面注册回调函数bt_music_info_handle_register(user_get_bt_music_info);


double powerOfTen(int exponent)
{
    double result = 1.0;
    if (exponent >= 0) {
        for (int i = 0; i < exponent; i++) {
            result *= 10.0;
        }
    } else {
        for (int i = 0; i > exponent; i--) {
            result /= 10.0;
        }
    }
    return result;
}
void sbox_phone_music_info_deal(u8 type, u32 time, u8 *info, u16 len)
{
    printf("phone_music_info_deal:%d", type);
    switch (type) {
    case 1:
        local_music_info.type_title = type;
        local_music_info.title_len = len;
        if (len > 256) {
            return;
        }
        memcpy(local_music_info.title, info, len);
        break;
    case 2:
        local_music_info.type_artist_name = type;
        local_music_info.name_len = len;
        if (len > 256) {
            return;
        }
        memcpy(local_music_info.artist_name, info, len);
        break;
    case 3:
        local_music_info.type_album_name = type;
        local_music_info.album_len = len;
        if (len > 256) {
            return;
        }
        memcpy(local_music_info.album_name, info, len);
        break;
    case 7:
        local_music_info.time = 0;
        for (int i = 0; i < len - 3; i++) {
            local_music_info.time += (info[i] - '0') * powerOfTen(len - i - 4);
        }
        custom_sync_lyric_info(&local_music_info);
        break;
    default :
        break;
    }
}
// void user_get_bt_music_info(u8 type, u32 time, u8 *info, u16 len)
// {
//     //profile define type: 1-title 2-artist name 3-album names 4-track number 5-total number of tracks 6-genre  7-playing time
//     //JL define 0x10-total time , 0x11 current play position
//     u8  min, sec;
//     printf("type %d %d \n", type,len);
//     if ((info != NULL) && (len != 0)) {
//         printf(" %s \n", info);
//     }
//     if (time != 0) {
//         min = time / 1000 / 60;
//         sec = time / 1000 - (min * 60);
//         log_debug(" time %d %d\n ", min, sec);
//     }
//     phone_music_info_deal(type,time,info,len);
// }



//void user_get_bt_music_info(u8 type, u32 time, u8 *info, u16 len)
//{
//    //profile define type:
//    //1-title 2-artist name 3-album names 4-track number
//    //5-total number of tracks 6-genre  7-playing time
//    //JL define 0x10-total time , 0x11 current play position
//    u8  min, sec;
//    //printf("type %d\n", type );
//    if ((info != NULL) && (len != 0) && (type != 7)) {
//        printf(" %s \n", info);
//    }
//
//    if (type == 7) {
//        // ms_to_time(info, len);
//    }
//    if (time != 0) {
//        min = time / 1000 / 60;
//
//        sec = time / 1000 - (min * 60);
//        printf(" time %02d : %02d\n ", min, sec);
//    }
//    phone_music_info_deal(type,time,info,len);
//}
/*-------------------------------------------歌词同步-----------------------------------------------------*/

//同步执行对应功能
static void sbox_tws_app_info_sync(int cmd, int err)
{
    log_info("%s cmd:%d\n", __func__, cmd);
    switch (cmd) {
#if CONFIG_ANC_ENABLE
    case SYNC_CMD_ANC_ON:
        anc_mode_switch(ANC_ON, 1);
        break;
    case SYNC_CMD_ANC_OFF:
        anc_mode_switch(ANC_OFF, 1);
        break;
    case SYNC_CMD_ANC_TRANS:
        anc_mode_switch(ANC_TRANSPARENCY, 1);
#endif
        break;
    case SYNC_CMD_VOLUME_UP:
        bt_volume_up(1);
        if (tws_api_get_role() == 0) {
            sbox_cb_func.sbox_sync_volume_info();
            // custom_sync_volume_state();
        }
        break;
    case SYNC_CMD_VOLUME_DOWN:
        bt_volume_down(1);
        if (tws_api_get_role() == 0) {
            sbox_cb_func.sbox_sync_volume_info();
        }
        break;
    case SYNC_CMD_CALL_MUTE:
        sbox_user_info.phone_call_mute = (!sbox_user_info.phone_call_mute);
        break;
    case SYNC_CMD_SBOX_POWER_OFF_TOGETHER:
        sys_enter_soft_poweroff(POWEROFF_NORMAL_TWS);
        break;
    default:

        break;
    }
}

TWS_SYNC_CALL_REGISTER(sbox_app_info_sync) = {
    .uuid = 0xA2122623,
    .task_name = "app_core",
    .func = sbox_tws_app_info_sync,
};


/*----------------------------------------------------功能应用相关------------------------------------------------------------------------*/



/*-------------------------------------------------状态同步相关------------------------------------------------------------------------*/

static int bt_call_status_event_handler(int *msg)
{
    u8 *phone_number = NULL;
    struct bt_event *bt = (struct bt_event *)msg;

    if (tws_api_get_role_async() == TWS_ROLE_SLAVE) {
        return 0;
    }

    switch (bt->event) {
    case BT_STATUS_PHONE_INCOME:
        sbox_cb_func.sbox_sync_call_state(SBOX_CALL_INCOME);
        // custom_sync_call_state(SBOX_CALL_INCOME);
        break;
    case BT_STATUS_PHONE_HANGUP:
        sbox_cb_func.sbox_sync_call_state(SBOX_CALL_HANDUP);
        // custom_sync_call_state(SBOX_CALL_HANDUP);
        sbox_user_info.phone_call_mute = 0;
        break;
    case BT_STATUS_PHONE_ACTIVE:
        sbox_cb_func.sbox_sync_call_state(SBOX_CALL_ACTIVE);
        // custom_sync_call_state(SBOX_CALL_ACTIVE);
        break;
    default:
        break;
    }
    return 0;
}

APP_MSG_HANDLER(bt_call_msg_handler_stub) = {
    .owner      = 0xff,
    .from       = MSG_FROM_BT_STACK,
    .handler    = bt_call_status_event_handler,
};


//蓝牙事件回调  用于同步一些状态信息
static int sbox_btstack_event_handler(int *_event)
{
    struct bt_event *event = (struct bt_event *)_event;
    log_info("sbox_btstack_event_handler:%d\n", event->event);
    put_buf(event->args, 6);
    int a2dp_addr_is_same = 0;
    static u8 last_addr[6] = {0xff};
    put_buf(last_addr, 6);
    u8 tmpbuf[6] = {0xff};
    switch (event->event) {
    case BT_STATUS_RECONN_OR_CONN:
#if TCFG_BT_SUPPORT_MAP
        if (tws_api_get_role() == 0) {
            bt_cmd_prepare(USER_CTRL_MAP_READ_TIME, 0, NULL);
        }
#endif
        break;
    case BT_STATUS_INIT_OK:
        sbox_app_init();
        break;
    case BT_STATUS_FIRST_CONNECTED:
    case BT_STATUS_SECOND_CONNECTED:

        break;
    case BT_STATUS_CONN_A2DP_CH:
        if (tws_api_get_role() == 0) {
            sys_timeout_add(NULL, (void (*)(void *))(sbox_cb_func.sbox_sync_all_info), 1000);
        }

    case BT_STATUS_CONN_HFP_CH:

        //  sbox_cb_func.sbox_sync_all_info();
        break;
    case BT_STATUS_FIRST_DISCONNECT:
        sys_timeout_add(NULL, (void (*)(void *))(sbox_cb_func.sbox_sync_all_info), 1000);
    case BT_STATUS_SECOND_DISCONNECT:
        break;
    case BT_STATUS_AVRCP_INCOME_OPID:
        u8 connect_num = bt_get_total_connect_dev();
        log_info("sbox_btstack_event_handler:%d connect_num%d value%d sbox_user_info.music_states%d\n", event->event, connect_num, event->value, sbox_user_info.music_states);
#if TCFG_BT_DUAL_CONN_ENABLE
        if (connect_num == 1) {
            if (event->value == AVC_PLAY) {
                sbox_user_info.music_states = 1;
                custom_sync_music_state();
            } else if (event->value == AVC_PAUSE) {
                sbox_user_info.music_states = 0;
                custom_sync_music_state();
            }
        } else if (connect_num == 2) {
            if (memcmp(last_addr, tmpbuf, 6) == 0) {
                if (event->value == AVC_PLAY) {
                    memcpy(last_addr, event->args, 6);
                    sbox_user_info.music_states = 1;
                    custom_sync_music_state();
                } else if (event->value == AVC_PAUSE) {
                    sbox_user_info.music_states = 0;
                    memset(last_addr, 0, 6);
                    custom_sync_music_state();
                }
            } else {
                if (memcmp(last_addr, event->args, 6) == 0) {
                    a2dp_addr_is_same = 1;
                } else {
                    a2dp_addr_is_same = 0;
                }

                if ((event->value == AVC_PLAY) && sbox_user_info.music_states && (a2dp_addr_is_same == 0)) { //一拖二播歌抢断的情况
                    putchar('F');
                    memcpy(last_addr, event->args, 6);
                } else if ((event->value == AVC_PLAY) && (sbox_user_info.music_states == 0)) {
                    putchar('G');
                    sbox_user_info.music_states = 1;
                    memcpy(last_addr, event->args, 6);
                    custom_sync_music_state();
                } else if ((event->value == AVC_PAUSE) &&  sbox_user_info.music_states && (a2dp_addr_is_same == 0)) {
                    putchar('H');
                    // sbox_user_info.music_states=0;
                    // custom_sync_music_state();
                    // memset(last_addr,0,6);
                } else if ((event->value == AVC_PAUSE) &&  sbox_user_info.music_states && (a2dp_addr_is_same == 1)) {
                    putchar('J');
                    sbox_user_info.music_states = 0;
                    custom_sync_music_state();
                    memset(last_addr, 0, 6);
                }
            }
        }


        // memcpy(last_addr,event->args,6);
#else
        if (event->value == AVC_PLAY) {
            sbox_user_info.music_states = 1;
            custom_sync_music_state();
        } else if (event->value == AVC_PAUSE) {
            sbox_user_info.music_states = 2;
            custom_sync_music_state();
        }
#endif
        break;
    }
    return 0;
}

APP_MSG_HANDLER(sbox_stack_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_BT_STACK,
    .handler    = sbox_btstack_event_handler,
};



//仓协议事件具体执行
void sys_smartstore_event_handle(struct box_info *boxinfo)
{
    u8 addr[6];
    u32 len = 0;
    u8 cmd = 0;
    int ret = sbox_tr_cbuf_read(sbox_tr_rbuf, &len, &cmd);//栈变量取值可能会死机
    if (ret < 0) {
        log_info("%s[line:%d] read cbuf error!!!\n", __func__, __LINE__);
        return;
    }
__recmd:
    log_info("sys_smartstore_event_handle %x %d\n", cmd, len);
    put_buf(sbox_tr_rbuf, len);
    switch (cmd) {
    case CUSTOM_ALL_INFO_CMD:
        log_info("case CUSTOM_ALL_INFO_CMD\n");

        break;
    case CUSTOM_BLE_PLAY_MODE_CONTROL_CMD:
        log_info("CUSTOM_BLE_PLAY_MODE_CONTROL_CMD \n");
        sbox_cb_func.sbox_control_playmode(&sbox_tr_rbuf[0]);
        break;
    case CUSTOM_BLE_FINE_EARPHONE_CMD://0x38
        log_info("CUSTOM_BLE_FINE_EARPHONE_CMD \n");
        sbox_cb_func.sbox_control_find_phone(&sbox_tr_rbuf[0]);
        break;
    case CUSTOM_BLE_VOL_CONTROL_CMD:
        log_info("CUSTOM_BLE_VOL_CONTROL_CMD %d\n", sbox_tr_rbuf[0]);
        sbox_cb_func.sbox_control_volume(&sbox_tr_rbuf[0]);
        break;
    case CUSTOM_BLE_MUSIC_STATE_CONTROL_CMD:
        log_info("CUSTOM_BLE_MUSIC_STATE_CONTROL_CMD\n");
        sbox_cb_func.sbox_control_music_state(&sbox_tr_rbuf[0]);
        break;
    case CUSTOM_BLE_ANC_MODE_CONTROL_CMD:
        log_info("CUSTOM_BLE_ANC_MODE_CONTROL_CMD\n");
        sbox_cb_func.sbox_control_anc(&sbox_tr_rbuf[0]);
        break;
    case CUSTOM_BLE_EQ_MODE_CONTROL_CMD:
        log_info("CUSTOM_BLE_EQ_MODE_CONTROL_CMD\n");
        sbox_cb_func.sbox_control_eq(&sbox_tr_rbuf[0]);
        break;
    case CUSTOM_BLE_CONTRAL_TILTOK:
        log_info("CUSTOM_BLE_CONTRAL_TILTOK\n");
        sbox_cb_func.sbox_control_tiktok(&sbox_tr_rbuf[0]);
        break;
    case CUSTOM_BLE_CONTRAL_PHOTO:
        log_info("CUSTOM_BLE_CONTRAL_PHOTO\n");
        // custom_ctrl_phone_photo();
        sbox_cb_func.sbox_control_photo(NULL);
        break;
    case CUSTOM_EDR_CONTRAL_CONN:
        log_info("CUSTOM_EDR_CONTRAL_CONN\n");
        custom_ctrle_edr_conn(sbox_tr_rbuf[0]);
        break;
    case CUSTOM_EDR_SYNC_INFO:
        log_info("CUSTOM_EDR_CONTRAL_CONN\n");
        custom_ctrle_emitter_info(sbox_tr_rbuf, len);
        break;
    case CUSTOM_BLE_CONTRAL_CALL:
        sbox_cb_func.sbox_control_phone_call(&sbox_tr_rbuf[0]);
        break;
    case CUSTOM_SLEEP_CTRL_CMD:
        sbox_cb_func.sbox_control_ble_lantacy(&sbox_tr_rbuf[0]);
        break;
    default:
        break;
    }
    ret = 0;
    ret = sbox_tr_cbuf_read(sbox_tr_rbuf, &len, &cmd);//栈变量取值可能会死机
    if (ret < 0) {
        log_info("%s[line:%d] read cbuf return\n", __func__, __LINE__);
        return;
    } else {
        log_info("%s[line:%d] read cbuf recmd\n", __func__, __LINE__);
        goto __recmd;
    }
}

static int sbox_app_power_event_handler(int *msg)
{
    switch (msg[0]) {
    case POWER_EVENT_SYNC_TWS_VBAT_LEVEL:
        printf("update sbox bat");
        sbox_cb_func.sbox_sync_battery_info();
        break;
    case POWER_EVENT_POWER_CHANGE:
        printf("update sbox bat");
        sbox_cb_func.sbox_sync_battery_info();
        break;
    }
    return 0;
}

APP_MSG_HANDLER(sbox_level_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_BATTERY,
    .handler    = sbox_app_power_event_handler,
};

#endif


