#include "btstack/avctp_user.h"
#include "btstack/btstack_task.h"
#include "classic/tws_api.h"
#include "os/os_api.h"
#include "bt_slience_detect.h"
#include "a2dp_player.h"
#include "esco_player.h"
#include "app_tone.h"
#include "app_main.h"
#include "vol_sync.h"
#include "audio_config.h"
#include "btstack/a2dp_media_codec.h"
#include "earphone.h"
#include "audio_manager.h"
#include "clock_manager/clock_manager.h"
#include "dac_node.h"
#include "debug/audio_debug.h"
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_AURACAST_SINK_EN)
#include "le_audio_player.h"
#include "app_le_auracast.h"
#endif

#if TCFG_BT_DUAL_CONN_ENABLE == 0


enum {
    CMD_A2DP_PLAY = 1,
    CMD_A2DP_CLOSE,
    CMD_SET_A2DP_VOL,
#if TCFG_USER_TWS_ENABLE && TCFG_LOCAL_TWS_ENABLE
    CMD_A2DP_PLAY_REQ,
    CMD_A2DP_PLAY_RSP,
#endif
};


int g_avrcp_vol_chance_timer = 0;
u8 g_avrcp_vol_chance_data[8];
#if TCFG_USER_TWS_ENABLE && TCFG_LOCAL_TWS_ENABLE
static u8 SEND_A2DP_PLAY_REQ_FLAG = 0;
static u16 wait_enter_bt_timer = 0;
#endif

void tws_a2dp_play_send_cmd(u8 cmd, u8 *data, u8 len, u8 tx_do_action);

void tws_a2dp_player_close(u8 *bt_addr)
{
    puts("tws_a2dp_player_close\n");
    put_buf(bt_addr, 6);
    a2dp_player_close(bt_addr);
    a2dp_media_close(bt_addr);
}

#if TCFG_USER_TWS_ENABLE && TCFG_LOCAL_TWS_ENABLE
static void a2dp_wait_enter_bt(void *addr)
{
    if (app_in_mode(APP_MODE_BT) == 0) {
        wait_enter_bt_timer =  sys_timeout_add(addr, a2dp_wait_enter_bt, 100);
    } else {
        u8 buf[7];
        wait_enter_bt_timer = 0;
        memcpy(buf, (u8 *)addr, 6);
        tws_a2dp_play_send_cmd(CMD_A2DP_PLAY_RSP, buf, 6, 0);
    }
}
#endif

static void tws_a2dp_play_in_task(u8 *data)
{
    u8 btaddr[6];
    u8 dev_vol;
    u8 *bt_addr = data + 2;

    switch (data[0]) {
    case CMD_A2DP_PLAY:
        puts("app_msg_bt_a2dp_play\n");
        put_buf(bt_addr, 6);
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_AURACAST_SINK_EN)
        if (le_audio_player_is_playing()) {
            if (tws_api_get_role() != TWS_ROLE_SLAVE) {
                g_printf("a2dp_media_mute line:%d\n", __LINE__);
                a2dp_media_mute(bt_addr);
                break;
            }
        }
#endif
#if ((TCFG_BT_A2DP_PLAYER_ENABLE == 0) || BT_INTERFERE_WITH_AUDIO_DEBUG)
        y_printf("a2dp_player disable");
        break;
#endif
        dev_vol = data[8];
        //更新一下音量再开始播放
        app_audio_state_switch(APP_AUDIO_STATE_MUSIC,
                               app_audio_volume_max_query(AppVol_BT_MUSIC), NULL);
        set_music_device_volume(dev_vol);
        a2dp_player_low_latency_enable(bt_get_low_latency_mode());
        a2dp_player_open(bt_addr);
        break;
    case CMD_A2DP_CLOSE:
        tws_a2dp_player_close(bt_addr);
        break;
    case CMD_SET_A2DP_VOL:
        dev_vol = data[8];
        set_music_device_volume(dev_vol);
        break;
#if TCFG_USER_TWS_ENABLE && TCFG_LOCAL_TWS_ENABLE
    case CMD_A2DP_PLAY_REQ:
        r_printf("CMD_A2DP_PLAY_REQ tws_api_get_role() = %d", tws_api_get_role());
        if (app_in_mode(APP_MODE_BT)) {
            //如果已经在蓝牙模式了直接回复
            memcpy(g_play_addr, bt_addr, 6);
            tws_a2dp_play_send_cmd(CMD_A2DP_PLAY_RSP, g_play_addr, 6, 0);

        } else {
            //保存a2dp_addr, 后面进入蓝牙模式之后需要发回去
            memcpy(g_play_addr, bt_addr, 6);
            app_send_message(APP_MSG_GOTO_MODE, APP_MODE_BT);
            wait_enter_bt_timer = sys_timeout_add(g_play_addr, a2dp_wait_enter_bt, 100);
        }
        break;

    case CMD_A2DP_PLAY_RSP:
        r_printf("CMD_A2DP_PLAY_RSP");

        u8 buf[7];

        SEND_A2DP_PLAY_REQ_FLAG = 0;
        memcpy(buf, bt_addr, 6);

        buf[6] = bt_get_music_volume(bt_addr);
        tws_a2dp_play_send_cmd(CMD_A2DP_PLAY, buf, 7, 1);
        break;
#endif
    }
    if (data[1] != 2) {
        free(data);
    }
}

#if TCFG_USER_TWS_ENABLE
static void tws_a2dp_play_callback(u8 *data, u8 len)
{
    int msg[4];

    u8 *buf = malloc(len);
    if (!buf) {
        return;
    }
    memcpy(buf, data, len);

    msg[0] = (int)tws_a2dp_play_in_task;
    msg[1] = 1;
    msg[2] = (int)buf;

    os_taskq_post_type("app_core", Q_CALLBACK, 3, msg);
}

static void tws_a2dp_player_data_in_irq(void *_data, u16 len, bool rx)
{
    u8 *data = (u8 *)_data;

    if (data[1] == 0 && !rx) {
        return;
    }
    tws_a2dp_play_callback(data, len);
}
REGISTER_TWS_FUNC_STUB(tws_a2dp_player_stub) = {
    .func_id = 0x076AFE82,
    .func = tws_a2dp_player_data_in_irq,
};

void tws_a2dp_play_send_cmd(u8 cmd, u8 *_data, u8 len, u8 tx_do_action)
{
    u8 data[16];
    data[0] = cmd;
    data[1] = tx_do_action;
    memcpy(data + 2, _data, len);
    int err = tws_api_send_data_to_sibling(data, len + 2, 0x076AFE82);
    if (err) {
        data[1] = 2;
        tws_a2dp_play_in_task(data);
    }
}
#else
void tws_a2dp_play_send_cmd(u8 cmd, u8 *_data, u8 len, u8 tx_do_action)
{
    u8 data[16];

    if (!tx_do_action) {
        return;
    }
    data[0] = cmd;
    data[1] = 2;
    memcpy(data + 2, _data, len);
    tws_a2dp_play_in_task(data);
}
#endif

void tws_a2dp_sync_play(u8 *bt_addr, bool tx_do_action)
{
    u8 data[8];
    memcpy(data, bt_addr, 6);
    data[6] = bt_get_music_volume(bt_addr);
#if TCFG_USER_TWS_ENABLE && TCFG_LOCAL_TWS_ENABLE
    SEND_A2DP_PLAY_REQ_FLAG = 1;
    tws_a2dp_play_send_cmd(CMD_A2DP_PLAY_REQ, data, 7, 0);
#else
    if (data[6] > 127) {
        data[6] = app_audio_bt_volume_update(bt_addr, APP_AUDIO_STATE_MUSIC);
    }
    tws_a2dp_play_send_cmd(CMD_A2DP_PLAY, data, 7, tx_do_action);
#endif
}

static void avrcp_vol_chance_timeout(void *priv)
{
    g_avrcp_vol_chance_timer = 0;
    tws_a2dp_play_send_cmd(CMD_SET_A2DP_VOL, g_avrcp_vol_chance_data, 7, 1);
#if TCFG_TWS_AUDIO_SHARE_ENABLE
    bt_tws_share_master_sync_vol_to_share_slave();
#endif

}

static int a2dp_bt_status_event_handler(int *event)
{
    u8 data[8];
    u8 btaddr[6];
    struct bt_event *bt = (struct bt_event *)event;

    switch (bt->event) {
    case BT_STATUS_A2DP_MEDIA_START:
        g_printf("BT_STATUS_A2DP_MEDIA_START\n");
        put_buf(bt->args, 6);
        if (app_var.goto_poweroff_flag) {
            break;
        }
        dac_try_power_on_thread();//dac初始化耗时有120ms,此处提前将dac指定到独立任务内做初始化，优化蓝牙通路启动的耗时，减少时间戳超时的情况
        if (tws_api_get_role() == TWS_ROLE_MASTER &&
            bt_get_call_status_for_addr(bt->args) == BT_CALL_INCOMING) {
            //小米11来电挂断偶现没有hungup过来，hfp链路异常，重新断开hfp再连接
            puts("<<<<<<<<waring a2dp start hfp_incoming\n");
            bt_cmd_prepare_for_addr(bt->args, USER_CTRL_HFP_DISCONNECT, 0, NULL);
            bt_cmd_prepare_for_addr(bt->args, USER_CTRL_HFP_CMD_CONN, 0, NULL);
        }
        if (esco_player_runing()) {
            r_printf("esco_player_runing");
            a2dp_media_close(bt->args);
            break;
        }
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            break;
        }
        tws_a2dp_sync_play(bt->args, 1);
        break;
    case BT_STATUS_A2DP_MEDIA_STOP:
        g_printf("BT_STATUS_A2DP_MEDIA_STOP\n");
        put_buf(bt->args, 6);
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            break;
        }
        tws_a2dp_play_send_cmd(CMD_A2DP_CLOSE, bt->args, 6, 1);
        break;
    case BT_STATUS_AVRCP_VOL_CHANGE:
        puts("BT_STATUS_AVRCP_VOL_CHANGE\n");
        //判断是当前地址的音量值才更新
        clock_refurbish();
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            break;
        }
        data[6] = bt->value;
        memcpy(g_avrcp_vol_chance_data, data, 7);
        if (g_avrcp_vol_chance_timer) {
            sys_timer_modify(g_avrcp_vol_chance_timer, 100);
        } else {
            g_avrcp_vol_chance_timer = sys_timeout_add(NULL, avrcp_vol_chance_timeout, 100);
        }
        break;
    }
    return 0;
}
APP_MSG_HANDLER(a2dp_stack_msg_handler) = {
    .owner      = 0xff,
    .from       = MSG_FROM_BT_STACK,
    .handler    = a2dp_bt_status_event_handler,
};


static int a2dp_bt_hci_event_handler(int *event)
{
    struct bt_event *bt = (struct bt_event *)event;

    switch (bt->event) {
    case HCI_EVENT_DISCONNECTION_COMPLETE:
        tws_a2dp_player_close(bt->args);
        break;
    }

    return 0;
}
APP_MSG_HANDLER(a2dp_hci_msg_handler) = {
    .owner      = 0xff,
    .from       = MSG_FROM_BT_HCI,
    .handler    = a2dp_bt_hci_event_handler,
};

static int a2dp_app_msg_handler(int *msg)
{
    u8 *bt_addr = (u8 *)(msg +  1);
    u8 addr[6];

    switch (msg[0]) {
    case APP_MSG_BT_A2DP_START:
        tws_a2dp_sync_play(bt_addr, 1);
        break;
    case APP_MSG_BT_A2DP_STOP:
        puts("APP_MSG_BT_A2DP_STOP\n");
        tws_a2dp_play_send_cmd(CMD_A2DP_CLOSE, bt_addr, 6, 1);
        break;
    case APP_MSG_BT_A2DP_PLAY:
        puts("APP_MSG_BT_A2DP_PLAY\n");
        tws_a2dp_sync_play(bt_addr, 1);
        break;
    case APP_MSG_SEND_A2DP_PLAY_CMD:
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            break;
        }
        void *device = btstack_get_conn_device(bt_addr);
        if (device) {
            btstack_device_control(device, USER_CTRL_AVCTP_OPID_PLAY);
        }
        break;
    case APP_MSG_EXIT_MODE:
        puts("APP_MSG_EXIT_MODE\n");
#if TCFG_USER_TWS_ENABLE && TCFG_LOCAL_TWS_ENABLE
        if (wait_enter_bt_timer && (msg[1] == APP_MODE_BT)) {
            sys_timeout_del(wait_enter_bt_timer);
            wait_enter_bt_timer = 0;
        }
#endif
        break;
    }
    return 0;
}
APP_MSG_HANDLER(a2dp_app_msg_handler_stub) = {
    .owner      = 0xff,
    .from       = MSG_FROM_APP,
    .handler    = a2dp_app_msg_handler,
};

#if TCFG_USER_TWS_ENABLE
static int a2dp_tws_msg_handler(int *msg)
{
    struct tws_event *evt = (struct tws_event *)msg;
    u8 role = evt->args[0];
    u8 state = evt->args[1];
    u8 *bt_addr = evt->args + 3;
    u8 addr[6];

    switch (evt->event) {
    case TWS_EVENT_MONITOR_START:
        if (role == TWS_ROLE_SLAVE) {
            break;
        }
        if (a2dp_player_is_playing(bt_addr)) {
            tws_a2dp_sync_play(bt_addr, 0);
        }
        break;
    case TWS_EVENT_PHONE_LINK_DETACH:
        tws_a2dp_player_close(evt->args + 3);
        break;
    case TWS_EVENT_ROLE_SWITCH:
        break;
    case TWS_EVENT_CONNECTION_DETACH:
#if TCFG_USER_TWS_ENABLE && TCFG_LOCAL_TWS_ENABLE
        r_printf("__func__ = %s   TWS_EVT_CONNECT_DETACH   SEND_A2DP_PLAY_REQ_FLAG = %d    wait_enter_bt_timer = %d", __func__, SEND_A2DP_PLAY_REQ_FLAG, wait_enter_bt_timer);
        if (SEND_A2DP_PLAY_REQ_FLAG) {
            //发了A2DP_PLAY_REQ从机无响应,且断开tws连接，那么主机自己播
            u8 buf[7];
            memcpy(buf, g_play_addr, 6);
            put_buf(g_play_addr, 6);
            buf[6] = bt_get_music_volume(g_play_addr);
            tws_a2dp_play_send_cmd(CMD_A2DP_PLAY, g_play_addr, 7, 1);
            SEND_A2DP_PLAY_REQ_FLAG = 0;
        }
        if (wait_enter_bt_timer) {
            sys_timeout_del(wait_enter_bt_timer);
            wait_enter_bt_timer = 0;
        }
#endif
        break;
    }
    return 0;
}
APP_MSG_HANDLER(a2dp_tws_msg_handler_stub) = {
    .owner      = 0xff,
    .from       = MSG_FROM_TWS,
    .handler    = a2dp_tws_msg_handler,
};
#endif



#endif

