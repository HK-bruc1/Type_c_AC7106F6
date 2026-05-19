/*********************************************************************************************
    *   Filename        : app_connected.c

    *   Description     :

    *   Author          : Weixin Liang

    *   Email           : liangweixin@zh-jieli.com

    *   Last modifiled  : 2022-12-15 14:30

    *   Copyright:(c)JIELI  2011-2022  @ , All Rights Reserved.
*********************************************************************************************/
#include "system/includes.h"
#include "cig.h"
#include "le_connected.h"
#include "app_le_connected.h"
#include "app_config.h"
#include "app_tone.h"
#include "btstack/avctp_user.h"
#include "audio_config.h"
#include "tone_player.h"
#include "app_main.h"
#include "ble_rcsp_server.h"
#include "wireless_trans.h"
#include "bt_common.h"
#include "user_cfg.h"
#include "btstack/bluetooth.h"
#include "btstack/le/ble_api.h"
#include "btstack/le/le_user.h"
#include "multi_protocol_main.h"
#include "earphone.h"
#include "vol_sync.h"
#include "a2dp_player.h"
#include "tws_a2dp_play.h"
#include "esco_player.h"
#include "clock_manager/clock_manager.h"
#include "dual_a2dp_play.h"
#include "ble/hci_ll.h"
#include "bt_tws.h"

#if TCFG_USER_TWS_ENABLE
#include "classic/tws_api.h"
#include "tws_dual_conn.h"
#else
#include "dual_conn.h"
#endif
#if (THIRD_PARTY_PROTOCOLS_SEL & RCSP_MODE_EN)
#include "ble_rcsp_server.h"
#endif
#include "vol_sync.h"
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_AURACAST_SINK_EN)
#include "app_le_auracast.h"
#endif
#if (THIRD_PARTY_PROTOCOLS_SEL & HID_ISO_EN)
#include "hid_iso.h"
#endif

#include "volume_node.h"

#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)))
struct le_audio_var {
    u32 cig_dongle_host_type;			// cig dongle主机类型
    u8 le_audio_profile_ok;				// cig 初始化成功标志
    u8 le_audio_en_config;				// cig 功能是否使能
    u8 cig_phone_conn_status;			// cig 当前tws耳机le_audio连接状态
    u8 cig_phone_other_conn_status;		// cig 另外一个tws耳机le_audio连接状态
    u8 peer_address[6];					// cig 当前连接的设备地址
    u8 le_audio_tws_role;				// 当前tws主从角色，0:tws主机，1:tws从机
    u8 le_audio_adv_connected;			// leaudio广播连接状态，0xAA:已连上，0:已断开
#if TCFG_JL_UNICAST_EDR_MODE_SWITCH_ENABLE
    u8 jl_unicast_mode;					// JL_UNICAST_MODE
#endif
};
static struct le_audio_var g_le_audio_hdl;

extern void ble_vendor_priv_cmd_handle_register(u16(*handle)(u16 hdl, u8 *cmd, u8 len, u8 *rsp));
extern int ll_hci_vendor_send_priv_cmd(u16 conn_handle, u8 *data, u16 size); //通过hci命令发
extern u8 lmp_get_esco_conn_statu(void);
extern void ll_set_param_aclMaxPduCToP(uint8_t aclMaxRxPdu);

enum {
    LE_AUDIO_CONFIG_EN = 1,
    LE_AUDIO_CONN_STATUES,
    LE_AUDIO_CONFIG_SIRK,
    LE_AUDIO_GET_SLAVE_VOL,  //when tws connect,if slave playing cis then it will send vol to master
    LE_AUDIO_ADV_MAC_INFO,
    LE_AUDIO_CONN_CHECK,
};

u8 cig_support_ce = 0; //cis音频加密功能，需要dongle同步开启，1开启、0关闭
#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_JL_UNICAST_SINK_EN | LE_AUDIO_UNICAST_SINK_EN)) && TCFG_USER_TWS_ENABLE)
u8 tws_witch_cis_power_check()
{
    return 1;  //开启jl_unicast tws省功耗策略
}
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/
#define LOG_TAG             "[APP_LE_CONNECTED]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"


#define VENDOR_PRIV_DEVICE_TYPE_REQ     0x01
//Central -> Peripheral
#define VENDOR_PRIV_LC3_INFO            0x03
#define VENDOR_PRIV_OPEN_MIC            0x04
#define VENDOR_PRIV_CLOSE_MIC           0x05
#define VENDOR_PRIV_ACL_MUSIC_VOLUME    0x07
#define VENDOR_PRIV_ACL_MIC_VOLUME      0x08
#define VENDOR_PRIV_HOST_TYPE           0x10   //主机告知从机dongle连接设备类型
//Peripheral -> Central
#define VENDOR_PRIV_DEVICE_TYPE_RSP     0x02
#define VENDOR_PRIV_ACL_OPID_CONTORL    0x06
#define VENDOR_PRIV_SET_DUAL_UAC_VOL    0x20   //从机告知主机调节对应声卡的音量值

/**************************************************************************************************
  Data Types
**************************************************************************************************/

struct app_cis_conn_info {
    u8 cis_status;						// cis连接断开的时候会被设置，详细见app_le_connected.h的APP_CONNECTED_STATUS说明
    u16 cis_hdl;						// cis句柄，cis连接成功的时候会被设置
    u16 acl_hdl;						// acl句柄，cis连接成功的时候会被设置
    u16 Max_PDU_C_To_P;
    u16 Max_PDU_P_To_C;					// 有值说明是LEA_SERVICE_CALL，否则LEA_SERVICE_MEDIA
};

struct app_cig_conn_info {
    u8 used;															// 是否开启cig, app_connected_open
    u8 cig_hdl;															// cig句柄，cis连接成功后会被设置
    u8 cig_status;														// cig功能开关关闭的时候会被设置, 详细见app_le_connected.h的APP_CONNECTED_STATUS说明
    u8 break_cig_hdl;
    u8 break_a2dp_by_le_audio_call;
    struct app_cis_conn_info cis_conn_info[CIG_MAX_CIS_NUMS];			// cig下的cis成员
};

/**************************************************************************************************
  Global Variables
**************************************************************************************************/
static OS_MUTEX mutex;
static u8 acl_connected_nums = 0;										// acl链路连接数
static u8 cis_connected_nums = 0;										// cis链路连接数
static struct app_cig_conn_info app_cig_conn_info[CIG_MAX_NUMS];		// cig对象

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/* --------------------------------------------------------------------------*/
/**
 * @brief 申请互斥量，用于保护临界区代码，与app_connected_mutex_post成对使用
 *
 * @param _mutex:已创建的互斥量指针变量
 */
/* ----------------------------------------------------------------------------*/
static inline void app_connected_mutex_pend(OS_MUTEX *_mutex, u32 line)
{
    int os_ret;
    os_ret = os_mutex_pend(_mutex, 0);
    if (os_ret != OS_NO_ERR) {
        log_error("%s err, os_ret:0x%x", __FUNCTION__, os_ret);
        ASSERT(os_ret != OS_ERR_PEND_ISR, "line:%d err, os_ret:0x%x", line, os_ret);
    }
}
enum {
    REMOVE_CIS_REASON_BY_ESCO = 1,//1:耳机端手机通话中，dongle不建立cis
    REMOVE_CIS_ESCO_MIX,  //2://手机通话声音和dongle 音乐声音叠加，dongle需要重新换新的参数播放
};



/* --------------------------------------------------------------------------*/
/**
 * @brief 释放互斥量，用于保护临界区代码，与app_connected_mutex_pend成对使用
 *
 * @param _mutex:已创建的互斥量指针变量
 */
/* ----------------------------------------------------------------------------*/
static inline void app_connected_mutex_post(OS_MUTEX *_mutex, u32 line)
{
    int os_ret;
    os_ret = os_mutex_post(_mutex);
    if (os_ret != OS_NO_ERR) {
        log_error("%s err, os_ret:0x%x", __FUNCTION__, os_ret);
        ASSERT(os_ret != OS_ERR_PEND_ISR, "line:%d err, os_ret:0x%x", line, os_ret);
    }
}

/* --------------------------------------------------------------------------*/
/**
 * @brief   le audio暂停播歌by a2dp
 *
 * @return 1:操作成功, 0:操作失败
 */
/* ----------------------------------------------------------------------------*/
int le_audio_unicast_play_stop_by_a2dp()
{
    int ret = 0;
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_JL_UNICAST_SINK_EN)
    if (is_cig_music_play()) {
        for (u8 i = 0; i < CIG_MAX_NUMS; i++) {
            if (app_cig_conn_info[i].used && app_cig_conn_info[i].cig_hdl != 0xff) {
                r_printf("le_audio_unicast_play_stop_by_a2dp");
                if (cis_audio_player_close(app_cig_conn_info[i].cig_hdl)) {
                    app_cig_conn_info[i].break_cig_hdl = app_cig_conn_info[i].cig_hdl;
#if TCFG_USER_TWS_ENABLE
                    if (tws_api_get_role() != TWS_ROLE_SLAVE)
#endif
                    {
                        u8 data[1];
                        data[0] = CIG_EVENT_OPID_PLAY;
                        le_audio_media_control_cmd(data, 1);
                        ret = 1;
                    }

                }
            }
        }
    }
#endif
    return ret;

}

/* --------------------------------------------------------------------------*/
/**
 * @brief   le audio恢复播歌by a2dp
 */
/* ----------------------------------------------------------------------------*/
void le_audio_unicast_play_resume_by_a2dp()
{
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_JL_UNICAST_SINK_EN)
    u8 data[2];
    if (is_cig_music_play()) {
        for (u8 i = 0; i < CIG_MAX_NUMS; i++) {
            if (app_cig_conn_info[i].used && app_cig_conn_info[i].cig_hdl != 0xff && app_cig_conn_info[i].break_cig_hdl != 0xff) {
                r_printf("le_audio_unicast_play_resume");
                if (cis_audio_player_resume(app_cig_conn_info[i].break_cig_hdl, is_cig_phone_call_play())) {

                    app_cig_conn_info[i].break_cig_hdl = 0xff;
                }
                data[0] = CIG_EVENT_OPID_PLAY;
                le_audio_media_control_cmd(data, 1);

            }
        }
    }

#endif
}

/* --------------------------------------------------------------------------*/
/**
 * @brief   le audio暂停播歌by phone call
 * 			LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_PLAY_PREEMPTEDK的时候使用
 */
/* ----------------------------------------------------------------------------*/
void le_audio_unicast_play_remove_by_phone_call()
{
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_JL_UNICAST_SINK_EN)
    u8 data[2];
    if (is_cig_acl_conn()) {
        for (u8 i = 0; i < CIG_MAX_NUMS; i++) {
            if (app_cig_conn_info[i].used) {
                r_printf("le_audio_unicast_play_remove_by_phone_call");
                if (is_cig_music_play()) {
                    cis_audio_player_close(app_cig_conn_info[i].cig_hdl);
                }
            }
            data[0] = CIG_EVENT_OPID_REQ_REMOVE_CIS;//fix dongle must remove cis
#if (LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_CONFIG & LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_PLAY_MIX)
            data[1] = REMOVE_CIS_ESCO_MIX;
#if (TCFG_BLE_HIGH_PRIORITY_ENABLE == 0)
#error "warning TCFG_BLE_HIGH_PRIORITY_ENABLE need = 1"
#endif
#else

            data[1] = REMOVE_CIS_REASON_BY_ESCO;
#endif
            le_audio_media_control_cmd(data, 2);
        }
    }
#endif

}

/* --------------------------------------------------------------------------*/
/**
 * @brief   le audio恢复播歌by phone hangup
 */
/* ----------------------------------------------------------------------------*/
void le_audio_unicast_try_resume_play_by_phone_call_remove()
{
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_JL_UNICAST_SINK_EN)
    u8 data[2];
    if (is_cig_acl_conn()) {
        for (u8 i = 0; i < CIG_MAX_NUMS; i++) {
            if (app_cig_conn_info[i].used) {
                r_printf("le_audio_unicast_try_resume_play_by_phone_call_remove");
                data[0] = CIG_EVENT_OPID_REQ_CREAT_CIS;
#if (LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_CONFIG & LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_PLAY_MIX)
                data[1] = REMOVE_CIS_ESCO_MIX;
#else
                data[1] = 0;
#endif
                le_audio_media_control_cmd(data, 2);
            }

        }
    }
#endif
}

/* --------------------------------------------------------------------------*/
/**
 * @brief   le audio暂停播歌by esco
 * 			LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_PLAY_MIX的时候使用
 *
 * @return 1:操作成功, 0:操作失败
 */
/* ----------------------------------------------------------------------------*/
int le_audio_unicast_play_stop_by_esco()
{
    int ret = 0;
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_JL_UNICAST_SINK_EN)
    if (is_cig_music_play()) {
        for (u8 i = 0; i < CIG_MAX_NUMS; i++) {
            if (app_cig_conn_info[i].used && app_cig_conn_info[i].cig_hdl != 0xff) {
                r_printf("le_audio_unicast_play_stop_by_esco");
                if (cis_audio_player_close(app_cig_conn_info[i].cig_hdl)) {
                    app_cig_conn_info[i].break_cig_hdl = app_cig_conn_info[i].cig_hdl;
                    ret = 1;
                }
            }
        }
    }
#endif
    return ret;

}

/* --------------------------------------------------------------------------*/
/**
 * @brief   le audio恢复播歌by esco
 * 			LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_PLAY_MIX的时候使用
 */
/* ----------------------------------------------------------------------------*/
void le_audio_unicast_play_resume_by_esco()
{
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_JL_UNICAST_SINK_EN)
    u8 data[2];
    if (is_cig_music_play()) {
        for (u8 i = 0; i < CIG_MAX_NUMS; i++) {
            if (app_cig_conn_info[i].used && app_cig_conn_info[i].cig_hdl != 0xff && app_cig_conn_info[i].break_cig_hdl != 0xff) {
                r_printf("le_audio_unicast_play_esco_resume");
                if (cis_audio_player_resume(app_cig_conn_info[i].break_cig_hdl, is_cig_phone_call_play())) {

                    app_cig_conn_info[i].break_cig_hdl = 0xff;
                }

            }
        }
    }

#endif
}

/* --------------------------------------------------------------------------*/
/**
 * @brief CIG连接状态事件处理函数
 *
 * @param msg:状态事件附带的返回参数
 *
 * @return
 */
/* ----------------------------------------------------------------------------*/
static int app_connected_conn_status_event_handler(int *msg)
{
    u8 i, j;
    u8 find = 0;
    int ret = 0;
    cig_hdl_t *hdl;
    cis_acl_info_t *acl_info;
    int *event = msg;
    int result = 0;
    log_info("app_connected_conn_status_event_handler=%d", event[0]);

    switch (event[0]) {
    case CIG_EVENT_PERIP_CONNECT:
        hdl = (cig_hdl_t *)&event[1];
        g_printf("CIG_EVENT_PERIP_CONNECT 0x%x, 0x%x", hdl->cig_hdl, hdl->cis_hdl);
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_AURACAST_SINK_EN)
        if (hdl->Max_PDU_P_To_C) {
            // 通话暂停auracast播歌
            le_auracast_stop(1);
        } else {
            // leaudio播歌暂停auracast播歌，且暂停leaudio播歌后不会恢复auracast播歌（参考三星手机逻辑）
            le_auracast_stop(0);
        }
#endif

#if TCFG_USER_TWS_ENABLE
        tws_api_tx_unsniff_req();
#endif
        bt_cmd_prepare(USER_CTRL_ALL_SNIFF_EXIT, 0, NULL);
        //由于是异步操作需要加互斥量保护，避免connected_close的代码与其同时运行,添加的流程请放在互斥量保护区里面
        app_connected_mutex_pend(&mutex, __LINE__);

        //记录设备的cig_hdl等信息
        for (i = 0; i < CIG_MAX_NUMS; i++) {
            if (app_cig_conn_info[i].used) {
                if (app_cig_conn_info[i].cig_hdl == hdl->cig_hdl) {
                    app_cig_conn_info[i].cig_hdl = hdl->cig_hdl;
                    app_cig_conn_info[i].break_cig_hdl = 0xff;
                    for (j = 0; j < CIG_MAX_CIS_NUMS; j++) {
                        if (!app_cig_conn_info[i].cis_conn_info[j].cis_hdl) {
                            app_cig_conn_info[i].cis_conn_info[j].cis_hdl = hdl->cis_hdl;
                            app_cig_conn_info[i].cis_conn_info[j].acl_hdl = hdl->acl_hdl;
                            app_cig_conn_info[i].cis_conn_info[j].cis_status = APP_CONNECTED_STATUS_CONNECT;
                            app_cig_conn_info[i].cis_conn_info[j].Max_PDU_C_To_P = hdl->Max_PDU_C_To_P;
                            app_cig_conn_info[i].cis_conn_info[j].Max_PDU_P_To_C = hdl->Max_PDU_P_To_C;
                            cis_connected_nums++;
                            ASSERT(cis_connected_nums <= CIG_MAX_CIS_NUMS && cis_connected_nums >= 0, "cis_connected_nums:%d", cis_connected_nums);
                            find = 0;

                            log_info("Record acl hangle:0x%x", app_cig_conn_info[i].cis_conn_info[j].acl_hdl);
                            break;
                        }
                    }
                } else if (app_cig_conn_info[i].cig_hdl == 0xFF) {
                    app_cig_conn_info[i].cig_hdl = hdl->cig_hdl;
                    app_cig_conn_info[i].break_cig_hdl = 0xff;
                    for (j = 0; j < CIG_MAX_CIS_NUMS; j++) {
                        if (!app_cig_conn_info[i].cis_conn_info[j].cis_hdl) {
                            app_cig_conn_info[i].cis_conn_info[j].cis_hdl = hdl->cis_hdl;
                            app_cig_conn_info[i].cis_conn_info[j].acl_hdl = hdl->acl_hdl;
                            app_cig_conn_info[i].cis_conn_info[j].cis_status = APP_CONNECTED_STATUS_CONNECT;
                            app_cig_conn_info[i].cis_conn_info[j].Max_PDU_C_To_P = hdl->Max_PDU_C_To_P;
                            app_cig_conn_info[i].cis_conn_info[j].Max_PDU_P_To_C = hdl->Max_PDU_P_To_C;
                            cis_connected_nums++;
                            ASSERT(cis_connected_nums <= CIG_MAX_CIS_NUMS && cis_connected_nums >= 0, "cis_connected_nums:%d", cis_connected_nums);
                            find = 1;

                            log_info("Record acl hangleFF:0x%x", app_cig_conn_info[i].cis_conn_info[j].acl_hdl);
                            break;
                        }
                    }
                }
            }
        }

        if (find) {
            g_le_audio_hdl.cig_phone_conn_status |= APP_CONNECTED_STATUS_MUSIC;
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_UNICAST_SINK_EN)
            if (hdl->Max_PDU_P_To_C) {
                u8 now_call_vol  = vcs_server_get_volume(hdl->acl_hdl) * 15 / 255;
                //set call volume
                app_audio_set_volume(APP_AUDIO_STATE_CALL, now_call_vol, 1);
                g_le_audio_hdl.cig_phone_conn_status &= ~APP_CONNECTED_STATUS_MUSIC;
                g_le_audio_hdl.cig_phone_conn_status |= APP_CONNECTED_STATUS_PHONE_CALL;
                log_info("cis call to stop tone");
                tone_player_stop();
            }
#endif
        }
#if TCFG_USER_TWS_ENABLE
        tws_sync_le_audio_conn_info();
#endif

#if (LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_CONFIG & LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_PLAY_PREEMPTEDK)
        a2dp_suspend_by_le_audio();
#elif (LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_CONFIG & LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_PLAY_MIX)
        clock_refurbish();
        if (hdl->Max_PDU_P_To_C) {
            //dongle通话不叠加手机播歌，关闭a2dp,dongle cis 通话+手机a2dp+tws声音叠,帧长需要12.5ms,
            /* if(a2dp_suspend_by_le_audio()) */
            if (0) {
                for (i = 0; i < CIG_MAX_NUMS; i++) {
                    if (app_cig_conn_info[i].used) {
                        if (app_cig_conn_info[i].cig_hdl == hdl->cig_hdl) {
                            app_cig_conn_info[i].break_a2dp_by_le_audio_call = 1;
                        }
                    }
                }
            }
        }
#endif
        ret = connected_perip_connect_deal((void *)hdl);
        if (ret < 0) {
            log_debug("connected_perip_connect_deal fail");
        }

#if (LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_CONFIG & LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_PLAY_PREEMPTEDK)
        if (esco_player_runing()) {
            log_info("esco runing, stop cis");
            le_audio_unicast_play_remove_by_phone_call();
        }
#endif
        //释放互斥量
        app_connected_mutex_post(&mutex, __LINE__);
        break;

    case CIG_EVENT_PERIP_DISCONNECT:
        //由于是异步操作需要加互斥量保护，避免connected_close的代码与其同时运行,添加的流程请放在互斥量保护区里面
        app_connected_mutex_pend(&mutex, __LINE__);

        hdl = (cig_hdl_t *)&event[1];
        g_printf("CIG_EVENT_PERIP_DISCONNECT 0x%x", hdl->cig_hdl);
        u8  dis_reason = hdl->flush_timeout_C_to_P;     //  复用了flush_timeout_C_to_P参数上传断开错误码
        u16 acl_handle_for_disconnect_cis = 0;
        for (i = 0; i < CIG_MAX_NUMS; i++) {
            if (app_cig_conn_info[i].used && (app_cig_conn_info[i].cig_hdl == hdl->cig_hdl)) {
                if (app_cig_conn_info[i].used) {
                    if (app_cig_conn_info[i].cig_hdl == hdl->cig_hdl) {
#if TCFG_BT_DUAL_CONN_ENABLE
#if (LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_CONFIG & LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_PLAY_MIX)
                        if (app_cig_conn_info[i].break_a2dp_by_le_audio_call) {
                            app_cig_conn_info[i].break_a2dp_by_le_audio_call = 0;
                            try_a2dp_resume_by_le_audio_preempted();
                        }
#endif
#endif
                        for (j = 0; j < CIG_MAX_CIS_NUMS; j++) {
                            // if (app_cig_conn_info[i].cis_conn_info[j].cis_hdl == hdl->cis_hdl) {
                            app_cig_conn_info[i].cis_conn_info[j].cis_hdl = 0;
                            acl_handle_for_disconnect_cis = app_cig_conn_info[i].cis_conn_info[j].acl_hdl;
                            /* app_cig_conn_info[i].cis_conn_info[j].acl_hdl = 0; */
                            app_cig_conn_info[i].cis_conn_info[j].cis_status = APP_CONNECTED_STATUS_DISCONNECT;
                            app_cig_conn_info[i].cig_hdl = 0xFF;
                            //break;
                            // }
                            if (cis_connected_nums > 0) {
                                cis_connected_nums--;
                            }
                        }

                        find = 1;
                    }
                }
            }
        }

        if (!find) {
            //释放互斥量
            app_connected_mutex_post(&mutex, __LINE__);
            break;
        }


        ret = connected_perip_disconnect_deal((void *)hdl);
        g_le_audio_hdl.cig_phone_conn_status &= ~APP_CONNECTED_STATUS_MUSIC;
        g_le_audio_hdl.cig_phone_conn_status &= ~APP_CONNECTED_STATUS_PHONE_CALL;
#if TCFG_USER_TWS_ENABLE
        tws_sync_le_audio_conn_info();
#endif
#if TCFG_BT_DUAL_CONN_ENABLE
#if (LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_CONFIG & LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_PLAY_PREEMPTEDK)
        try_a2dp_resume_by_le_audio_preempted();
#endif
#endif
        if (ret < 0) {
            log_debug("connected_perip_disconnect_deal fail");
        }

#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_UNICAST_SINK_EN)
        if (dis_reason == ERROR_CODE_CONNECTION_TIMEOUT) {
            //测试播歌超距的时候，有一种状态是CIG超时了，ACL还没断开，
            //这个时候靠近手机没有重新建立CIG的。---主动断开等手机重连
            log_info("CIG disconnect for timeout\n");
            //le_audio_disconn_le_audio_link();
            ll_hci_disconnect(acl_handle_for_disconnect_cis, CONNECTION_TERMINATED_BY_LOCAL_HOST);
        }
#endif
        //释放互斥量
        app_connected_mutex_post(&mutex, __LINE__);


#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_AURACAST_SINK_EN)
        le_auracast_audio_recover();
#endif

        break;

    case CIG_EVENT_ACL_CONNECT:
        log_info("CIG_EVENT_ACL_CONNECT");
#if (TCFG_LE_AUDIO_RCSP_USE_SAME_ACL)
        rcsp_bt_ble_adv_enable(0);
#endif
#if (THIRD_PARTY_PROTOCOLS_SEL & HID_ISO_EN)
        hid_iso_adv_enable(0);
#endif
        acl_info = (cis_acl_info_t *)&event[1];
        if (acl_info->conn_type) {
            log_info("connect test box ble");
            return -EPERM;
        }
        g_le_audio_hdl.cig_phone_conn_status |= APP_CONNECTED_STATUS_START;

        //由于是异步操作需要加互斥量保护，避免connected_close的代码与其同时运行,添加的流程请放在互斥量保护区里面
        app_connected_mutex_pend(&mutex, __LINE__);
        ble_op_latency_close(acl_info->acl_hdl);
        log_info("remote device addr:");
        put_buf((u8 *)&acl_info->pri_ch, sizeof(acl_info->pri_ch));
#if TCFG_JL_UNICAST_BOUND_PAIR_EN
        u8 le_com_addr_new[6] = {0};
        ret = syscfg_read(CFG_USER_COMMON_ADDR, le_com_addr_new, 6);
        log_info("read binding common addr\n");
        put_buf(le_com_addr_new, 6);

        if (memcmp(le_com_addr_new, "\0\0\0\0\0\0", 6) != 0) { //防止空地址触发读零异常
            if (memcmp(&acl_info->pri_ch, le_com_addr_new, 6) != 0) { //地址不匹配
                log_info("Device mismatched, connection denied!!!\n");
                ll_hci_disconnect(acl_info->acl_hdl, CONNECTION_TERMINATED_BY_LOCAL_HOST);
                app_connected_mutex_post(&mutex, __LINE__);
                break;
            }
        }
#endif
        acl_connected_nums++;
        ASSERT(acl_connected_nums <= CIG_MAX_CIS_NUMS && acl_connected_nums >= 0, "acl_connected_nums:%d", acl_connected_nums);

        //释放互斥量
        app_connected_mutex_post(&mutex, __LINE__);

#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_JL_UNICAST_SINK_EN)
        g_le_audio_hdl.cig_phone_conn_status |= APP_CONNECTED_STATUS_CONNECT;
        le_audio_adv_api_enable(0);
        int edr_total_connect_dev = bt_get_total_connect_dev();
        if (edr_total_connect_dev == 1) {
            clr_device_in_page_list();
            bt_cmd_prepare(USER_CTRL_PAGE_CANCEL, 0, NULL);
        } else if (edr_total_connect_dev == 2) {
            u8 active_addr[6];
            u8 *tmp_addr = NULL;
            if (esco_player_get_btaddr(active_addr) || a2dp_player_get_btaddr(active_addr)) { //断开非当前播歌通话的设备
                tmp_addr = btstack_get_other_dev_addr(active_addr);
                bt_cmd_prepare_for_addr(tmp_addr, USER_CTRL_DISCONNECTION_HCI, 0, NULL);
            } else { //断开非活跃的设备
                unactice_device_cmd_prepare(USER_CTRL_DISCONNECTION_HCI, 0, NULL);
            }

        }
#if TCFG_AUTO_SHUT_DOWN_TIME
        sys_auto_shut_down_disable();
#endif
#if TCFG_USER_TWS_ENABLE
        tws_sync_le_audio_conn_info();
        tws_dual_conn_state_handler();
#else
        dual_conn_page_device();
#endif
#endif

        //提前不进power down，page scan时间太少可能影响手机连接---lihaihua
#if TCFG_USER_TWS_ENABLE
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            puts("role slave break\n");
            break;
        }
#endif
#if LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_SWITCH
#else
        int connect_device      = bt_get_total_connect_dev();
        log_info("app_le_connected connect_device=%d\n", connect_device);
        if (connect_device == 0) {
            bt_cmd_prepare(USER_CTRL_WRITE_SCAN_DISABLE, 0, NULL);
            bt_cmd_prepare(USER_CTRL_WRITE_CONN_DISABLE, 0, NULL);
            clr_device_in_page_list();
            bt_cmd_prepare(USER_CTRL_PAGE_CANCEL, 0, NULL);
            bt_cmd_prepare(USER_CTRL_WRITE_CONN_ENABLE, 0, NULL);
        }
#endif
#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_JL_UNICAST_SINK_EN)))
        for (i = 0; i < CIG_MAX_NUMS; i++) {
            for (j = 0; j < CIG_MAX_CIS_NUMS; j++) {
                if (!app_cig_conn_info[i].cis_conn_info[j].acl_hdl) {
                    app_cig_conn_info[i].cis_conn_info[j].acl_hdl = acl_info->acl_hdl;
                }
            }
        }

#if TCFG_USER_TWS_ENABLE
        if (is_cig_other_music_play() || is_cig_other_phone_call_play()) {
            //主机入仓，主机出仓测试，播歌的时候又播提示音有点变调，先屏蔽。
            break;
        }
        tws_play_tone_file(get_tone_files()->cis_connect, 400);
#else
        play_tone_file(get_tone_files()->cis_connect);
#endif

#if (THIRD_PARTY_PROTOCOLS_SEL & HID_ISO_EN)
        hid_iso_adv_enable(1);
#endif

#if JL_UNICAST_DUAL_UAC_ENABLE
        u8 uac_vol[2] = {0};
        int ret = syscfg_read(CFG_JL_CIS_DUAL_UAC_VOL, uac_vol, 2);
        //log_info("syscfg_read, uac0_vol:%d , uac1_vol:%d\n", uac_vol[0], uac_vol[1]);
        if (ret != 2) {
            log_info("no uac_vol\n");
            app_var.uac0_vol = uac_vol[0] = 50;
            app_var.uac1_vol = uac_vol[1] = 50;
        }
        log_info("acl conn, set_dual_uac_vol");
        le_audio_set_dual_uac_vol(uac_vol[0], uac_vol[1]);
#endif

#endif
        break;

    case CIG_EVENT_ACL_DISCONNECT:
        log_info("CIG_EVENT_ACL_DISCONNECT");
#if (TCFG_LE_AUDIO_RCSP_USE_SAME_ACL)
#if TCFG_USER_TWS_ENABLE
        if (tws_api_get_role() != TWS_ROLE_SLAVE) {
            rcsp_bt_ble_adv_enable(1);
        }
#else
        rcsp_bt_ble_adv_enable(1);
#endif
#endif
#if (THIRD_PARTY_PROTOCOLS_SEL & HID_ISO_EN)
        hid_iso_adv_enable(1);
        void le_audio_adv_open_discover_mode();
        le_audio_adv_open_discover_mode();
#endif
        g_le_audio_hdl.cig_phone_conn_status = 0;
        acl_info = (cis_acl_info_t *)&event[1];

        for (i = 0; i < CIG_MAX_NUMS; i++) {
            for (j = 0; j < CIG_MAX_CIS_NUMS; j++) {
                if (app_cig_conn_info[i].cis_conn_info[j].acl_hdl == acl_info->acl_hdl) {
                    log_info("Clear acl handle...\n");
                    app_cig_conn_info[i].cis_conn_info[j].acl_hdl = 0;
                }
            }
        }

#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_JL_UNICAST_SINK_EN)
        if (get_le_audio_jl_dongle_device_type()) {
            cig_event_to_user(CIG_EVENT_JL_DONGLE_DISCONNECT, (void *)&acl_info->acl_hdl, 2);
        }
#endif
        set_le_audio_jl_dongle_device_type(0);
        if (acl_info->conn_type) {
            log_info("disconnect test box ble");
            return -EPERM;
        }

        //由于是异步操作需要加互斥量保护，避免connected_close的代码与其同时运行,添加的流程请放在互斥量保护区里面
        app_connected_mutex_pend(&mutex, __LINE__);

        if (acl_connected_nums > 0) { // TCFG_JL_UNICAST_BOUND_PAIR_EN拒绝连接，会导致实际还没设备连接就触发断言
            acl_connected_nums--;
            ASSERT(acl_connected_nums <= CIG_MAX_CIS_NUMS && acl_connected_nums >= 0, "acl_connected_nums:%d", acl_connected_nums);
        }

        //释放互斥量
        app_connected_mutex_post(&mutex, __LINE__);
#if TCFG_USER_TWS_ENABLE
        tws_dual_conn_state_handler();
#else
        dual_conn_page_device();
#endif
        break;
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_JL_UNICAST_SINK_EN)
    case CIG_EVENT_JL_DONGLE_CONNECT:
        if (g_le_audio_hdl.cig_phone_conn_status & APP_CONNECTED_STATUS_CONNECT) {
            break;
        }
        /* app_cig_conn_info[i].cis_conn_info[j].acl_hdl = hdl->acl_hdl; */
#if (LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_SWITCH == 0)
        bt_cmd_prepare(USER_CTRL_WRITE_SCAN_DISABLE, 0, NULL);
        bt_cmd_prepare(USER_CTRL_WRITE_CONN_DISABLE, 0, NULL);
        clr_device_in_page_list();
#endif
#endif
        break;
    case CIG_EVENT_PHONE_CONNECT:
        log_info("CIG_EVENT_PHONE_CONNECT");
        clr_device_in_page_list();
        bt_cmd_prepare(USER_CTRL_PAGE_CANCEL, 0, NULL);
        g_le_audio_hdl.cig_phone_conn_status |= APP_CONNECTED_STATUS_CONNECT;
        int ret = get_sm_peer_address(g_le_audio_hdl.peer_address);
        put_buf(g_le_audio_hdl.peer_address, 6);
        u8 *cur_conn_addr = bt_get_current_remote_addr();
        if (cur_conn_addr && memcmp(cur_conn_addr, g_le_audio_hdl.peer_address, 6)) {
            bt_cmd_prepare_for_addr(cur_conn_addr, USER_CTRL_DISCONNECTION_HCI, 0, NULL);
        }
        updata_last_link_key(g_le_audio_hdl.peer_address, get_remote_dev_info_index());

#if TCFG_AUTO_SHUT_DOWN_TIME
        sys_auto_shut_down_disable();
#endif
#if TCFG_USER_TWS_ENABLE
        tws_sync_le_audio_conn_info();
        tws_dual_conn_state_handler();
#else
        dual_conn_page_device();
#endif
#if TCFG_USER_TWS_ENABLE
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            break;
        }
        if (is_cig_other_music_play() || is_cig_other_phone_call_play()) {
            //主机入仓，主机出仓测试，播歌的时候又播提示音有点变调，先屏蔽。
            break;
        }
        tws_play_tone_file(get_tone_files()->cis_connect, 400);
#else
        play_tone_file(get_tone_files()->cis_connect);
#endif

#if (TCFG_LE_AUDIO_RCSP_USE_SAME_ACL)
#if TCFG_USER_TWS_ENABLE
        if (tws_api_get_role() != TWS_ROLE_SLAVE) {
            rcsp_bt_ble_adv_enable(1);
        }
#else
        rcsp_bt_ble_adv_enable(1);
#endif
#endif
#if (THIRD_PARTY_PROTOCOLS_SEL & HID_ISO_EN)
        hid_iso_adv_enable(1);
#endif

#if JL_UNICAST_DUAL_UAC_ENABLE
        u8 uac_vol[2] = {0};
        int ret = syscfg_read(CFG_JL_CIS_DUAL_UAC_VOL, uac_vol, 2);
        //log_info("syscfg_read, uac0_vol:%d , uac1_vol:%d\n", uac_vol[0], uac_vol[1]);
        if (ret != 2) {
            log_info("no uac_vol\n");
            app_var.uac0_vol = uac_vol[0] = 50;
            app_var.uac1_vol = uac_vol[1] = 50;
        }
        log_info("acl conn, set_dual_uac_vol");
        le_audio_set_dual_uac_vol(uac_vol[0], uac_vol[1]);
#endif
        break;
    case CIG_EVENT_JL_DONGLE_DISCONNECT:
    case CIG_EVENT_PHONE_DISCONNECT:
        log_info("CIG_EVENT_PHONE_DISCONNECT");
        g_le_audio_hdl.cig_phone_conn_status = 0;
        memset(g_le_audio_hdl.peer_address, 0xff, 6);
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_AURACAST_SINK_EN)
        le_auracast_stop(0);
#endif
#if TCFG_USER_TWS_ENABLE
        tws_sync_le_audio_conn_info();
        tws_sync_le_audio_conn_check();
        tws_dual_conn_state_handler();
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            break;
        }
        if (app_var.goto_poweroff_flag == 0) {
            tws_play_tone_file(get_tone_files()->cis_disconnect, 400);
        }
#else
        if (app_var.goto_poweroff_flag == 0) {
            play_tone_file(get_tone_files()->cis_disconnect);
        }
#endif
#if TCFG_AUTO_SHUT_DOWN_TIME && TCFG_JL_UNICAST_EDR_MODE_SWITCH_ENABLE
        if (cis_connected_nums == 0) {
            sys_auto_shut_down_enable();
        }
#endif
        if (!g_le_audio_hdl.le_audio_profile_ok) {
            log_info("cis disconnect, app_connected_close\n");
            app_connected_close_in_other_mode();
        }
        break;

    default:
        result = -ESRCH;
        break;
    }

    return result;
}
APP_MSG_PROB_HANDLER(app_le_connected_msg_entry) = {
    .owner = 0xff,
    .from = MSG_FROM_CIG,
    .handler = app_connected_conn_status_event_handler,
};


/* --------------------------------------------------------------------------*/
/**
 * @brief   le audio状态查询函数， 是否在音乐播放状态，
 *
 * @return 1：在le audio音乐播放  0：不在音乐播放
 */
/* ----------------------------------------------------------------------------*/
u8 is_cig_music_play()
{
    log_debug("is_cig_music_play=%x\n", g_le_audio_hdl.cig_phone_conn_status);
    if (g_le_audio_hdl.cig_phone_conn_status & APP_CONNECTED_STATUS_MUSIC) {
        return 1;
    }
    return 0;
}
/* --------------------------------------------------------------------------*/
/**
 * @brief   le audio状态查询函数， tws另一个耳机是否在音乐播放状态
 *
 * @return  1：在le audio音乐播放  0：不在音乐播放
 */
/* ----------------------------------------------------------------------------*/
u8 is_cig_other_music_play()
{
    log_debug("is_cig_other_music_play=%x\n", g_le_audio_hdl.cig_phone_other_conn_status);
    if (g_le_audio_hdl.cig_phone_other_conn_status & APP_CONNECTED_STATUS_MUSIC) {
        return 1;
    }
    return 0;
}
/* --------------------------------------------------------------------------*/
/**
 * @brief   le audio状态查询函数, 是否在通话状态
 *
 * @return 1：在le audio通话状态  0：不在通话状态
 */
/* ----------------------------------------------------------------------------*/
u8 is_cig_phone_call_play()
{
    log_debug("is_cig_phone_call_play=%x\n", g_le_audio_hdl.cig_phone_conn_status);
    if (g_le_audio_hdl.cig_phone_conn_status & APP_CONNECTED_STATUS_PHONE_CALL) {
        return 1;
    }
    return 0;
}
/* --------------------------------------------------------------------------*/
/**
 * @brief   le audio状态查询函数, tws另一个耳机是否在通话状态
 *
 * @return  1：在le audio通话状态  0：不在通话状态
 */
/* ----------------------------------------------------------------------------*/
u8 is_cig_other_phone_call_play()
{
    log_debug("is_cig_other_phone_call_play=%x\n", g_le_audio_hdl.cig_phone_other_conn_status);
    if (g_le_audio_hdl.cig_phone_other_conn_status & APP_CONNECTED_STATUS_PHONE_CALL) {
        return 1;
    }
    return 0;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 检测当前是否处于挂起状态
 *
 * @return true:处于挂起状态，false:处于非挂起状态
 */
/* ----------------------------------------------------------------------------*/
static bool is_need_resume_connected()
{
    u8 i;
    u8 find = 0;
    for (i = 0; i < CIG_MAX_NUMS; i++) {
        if (app_cig_conn_info[i].cig_status == APP_CONNECTED_STATUS_SUSPEND) {
            find = 1;
            break;
        }
    }
    if (find) {
        return true;
    } else {
        return false;
    }
}

/* --------------------------------------------------------------------------*/
/**
 * @brief CIG从挂起状态恢复
 */
/* ----------------------------------------------------------------------------*/
static void app_connected_resume()
{
    cig_hdl_t temp_connected_hdl;

    if (!g_bt_hdl.init_ok) {
        return;
    }

    if (!is_need_resume_connected()) {
        return;
    }

    app_connected_open();
}

/* --------------------------------------------------------------------------*/
/**
 * @brief CIG进入挂起状态
 */
/* ----------------------------------------------------------------------------*/
static void app_connected_suspend()
{
    u8 i;
    u8 find = 0;
    for (i = 0; i < CIG_MAX_NUMS; i++) {
        if (app_cig_conn_info[i].used) {
            find = 1;
            break;
        }
    }
    if (find) {
        app_connected_close_all(APP_CONNECTED_STATUS_SUSPEND);
    }
}

int tws_check_user_conn_open_quick_type()
{
#if (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN))
    /* log_debug("tws_check_user_conn_open_quick_type=%d\n",check_le_audio_tws_conn_role() ); */
    if (is_cig_phone_conn()) {
        return g_le_audio_hdl.le_audio_tws_role;
    }
#endif
    return 0xff;
}

/* ----------------------------------------------------------------------------*/
/**
 * @brief 判断cig acl是否已被连接
 *
 * @return 1:已连接 0:未连接
 */
/* ----------------------------------------------------------------------------*/
u8 is_cig_acl_conn()
{
    /* log_debug("is_cig_acl_conn=%x\n", g_le_audio_hdl.cig_phone_conn_status); */
    if (g_le_audio_hdl.cig_phone_conn_status & APP_CONNECTED_STATUS_START) {
        return 1;
    }
    return 0;
}

/* ----------------------------------------------------------------------------*/
/**
 * @brief 判断tws另一端cig acl是否已被连接
 *
 * @return 1:已连接 0:未连接
 */
/* ----------------------------------------------------------------------------*/
u8 is_cig_other_acl_conn()
{
    /* log_debug("is_cig_other_acl_conn=%x\n", g_le_audio_hdl.cig_phone_other_conn_status); */
    if (g_le_audio_hdl.cig_phone_other_conn_status & APP_CONNECTED_STATUS_START) {
        return 1;
    }
    return 0;
}

/* ----------------------------------------------------------------------------*/
/**
 * @brief 判断cig是否已被连接
 *
 * @return 1:已连接 0:未连接
 */
/* ----------------------------------------------------------------------------*/
u8 is_cig_phone_conn()
{
    log_debug("is_cig_phone_conn=%d, %x\n", bt_get_total_connect_dev(), g_le_audio_hdl.cig_phone_conn_status);
    if (g_le_audio_hdl.cig_phone_conn_status & APP_CONNECTED_STATUS_CONNECT) {
        return 1;
    }
    return 0;
}


/* ----------------------------------------------------------------------------*/
/**
 * @brief 判断tws另一端cig是否已被连接
 *
 * @return 1:已连接 0:未连接
 */
/* ----------------------------------------------------------------------------*/
u8 is_cig_other_phone_conn()
{
    log_debug("is_cig_other_phone_conn=%d, %x\n", bt_get_total_connect_dev(), g_le_audio_hdl.cig_phone_other_conn_status);
    if (g_le_audio_hdl.cig_phone_other_conn_status & APP_CONNECTED_STATUS_CONNECT) {
        return 1;
    }
    return 0;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 开启CIG
 */
/* ----------------------------------------------------------------------------*/
void app_connected_open(void)
{
    u8 i;
    u8 role = 0;
    u8 cig_available_num = 0;
    cig_hdl_t temp_connected_hdl;
    cig_parameter_t *params;
    int ret;
    u64 pair_addr;
    struct app_mode *mode;

    if (!g_bt_hdl.init_ok) {
        return;
    }

    for (i = 0; i < CIG_MAX_NUMS; i++) {
        if (!app_cig_conn_info[i].used) {
            cig_available_num++;
        }
    }

    if (!cig_available_num) {
        return;
    }

    log_info("connected_open");

    //初始化cig接收端参数
    params = set_cig_params();
    //打开cig，打开成功后会在函数app_connected_conn_status_event_handler做后续处理
    temp_connected_hdl.cig_hdl = connected_perip_open(params);

#if 0
    extern void wireless_trans_auto_test3_init(void);
    extern void wireless_trans_auto_test4_init(void);
    //不定时切换模式
    wireless_trans_auto_test3_init();
    //不定时暂停播放
    wireless_trans_auto_test4_init();
#endif

    if (temp_connected_hdl.cig_hdl >= 0) {
        for (i = 0; i < CIG_MAX_NUMS; i++) {
            if (!app_cig_conn_info[i].used) {
                app_cig_conn_info[i].cig_hdl = temp_connected_hdl.cig_hdl;
                app_cig_conn_info[i].cig_status = APP_CONNECTED_STATUS_START;
                app_cig_conn_info[i].used = 1;
                break;
            }
        }
    }
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 关闭对应cig_hdl的CIG连接
 *
 * @param cig_hdl:需要关闭的CIG连接对应的hdl
 * @param status:关闭后CIG进入的suspend还是stop状态
 */
/* ----------------------------------------------------------------------------*/
void app_connected_close(u8 cig_hdl, u8 status)
{
    u8 i;
    u8 find = 0;
    log_info("connected_close");
    //由于是异步操作需要加互斥量保护，避免和开启的流程同时运行,添加的流程请放在互斥量保护区里面
    app_connected_mutex_pend(&mutex, __LINE__);

    for (i = 0; i < CIG_MAX_NUMS; i++) {
        if (app_cig_conn_info[i].used && (app_cig_conn_info[i].cig_hdl == cig_hdl)) {
            memset(&app_cig_conn_info[i], 0, sizeof(struct app_cig_conn_info));
            app_cig_conn_info[i].cig_status = status;
            find = 1;
            break;
        }
    }

    if (find) {
        connected_close(cig_hdl);
        acl_connected_nums -= CIG_MAX_CIS_NUMS;
        ASSERT(acl_connected_nums <= CIG_MAX_CIS_NUMS && acl_connected_nums >= 0, "acl_connected_nums:%d", acl_connected_nums);
        cis_connected_nums -= CIG_MAX_CIS_NUMS;
        ASSERT(cis_connected_nums <= CIG_MAX_CIS_NUMS && cis_connected_nums >= 0, "cis_connected_nums:%d", cis_connected_nums);
    }


    //释放互斥量
    app_connected_mutex_post(&mutex, __LINE__);
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 关闭所有cig_hdl的CIG连接
 *
 * @param status:关闭后CIG进入的suspend还是stop状态
 */
/* ----------------------------------------------------------------------------*/
void app_connected_close_all(u8 status)
{
    log_info("connected_close");
    //由于是异步操作需要加互斥量保护，避免和开启的流程同时运行,添加的流程请放在互斥量保护区里面
    app_connected_mutex_pend(&mutex, __LINE__);

    for (u8 i = 0; i < CIG_MAX_NUMS; i++) {
        if (app_cig_conn_info[i].used && app_cig_conn_info[i].cig_hdl) {
            connected_close(app_cig_conn_info[i].cig_hdl);
            memset(&app_cig_conn_info[i], 0, sizeof(struct app_cig_conn_info));
            app_cig_conn_info[i].cig_status = status;
        }
    }

    acl_connected_nums = 0;
    cis_connected_nums = 0;

    //释放互斥量
    app_connected_mutex_post(&mutex, __LINE__);
}

/* --------------------------------------------------------------------------*/
/**
 * @brief CIG开关切换
 *
 * @return 0：操作成功
 */
/* ----------------------------------------------------------------------------*/
int app_connected_switch(void)
{
    u8 i;
    u8 find = 0;
    cig_hdl_t temp_connected_hdl;
    struct app_mode *mode;

    if (!g_bt_hdl.init_ok) {
        return -EPERM;
    }

    for (i = 0; i < CIG_MAX_NUMS; i++) {
        if (app_cig_conn_info[i].used) {
            find = 1;
            break;
        }
    }

    if (find) {
        app_connected_close_all(APP_CONNECTED_STATUS_STOP);
    } else {
        app_connected_open();
    }
    return 0;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief CIG资源初始化
 */
/* ----------------------------------------------------------------------------*/
void app_connected_init(void)
{
    int os_ret = os_mutex_create(&mutex);
    if (os_ret != OS_NO_ERR) {
        log_error("%s %d err, os_ret:0x%x", __FUNCTION__, __LINE__, os_ret);
        ASSERT(0);
    }
}

/* --------------------------------------------------------------------------*/
/**
 * @brief CIG资源卸载
 */
/* ----------------------------------------------------------------------------*/
void app_connected_uninit(void)
{
    int os_ret = os_mutex_del(&mutex, OS_DEL_NO_PEND);
    if (os_ret != OS_NO_ERR) {
        log_error("%s %d err, os_ret:0x%x", __FUNCTION__, __LINE__, os_ret);
    }
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 非蓝牙后台情况下，在其他音源模式开启CIG，前提要先开蓝牙协议栈
 */
/* ----------------------------------------------------------------------------*/
void app_connected_open_in_other_mode()
{
    app_connected_init();
    app_connected_open();
}

/* --------------------------------------------------------------------------*/
/**
 * @brief
 * @brief 非蓝牙后台情况下，在其他音源模式关闭CIG
 */
/* ----------------------------------------------------------------------------*/

void app_connected_close_in_other_mode_in_app_core()
{
    if (is_cis_connected_init()) {
        app_connected_close_all(APP_CONNECTED_STATUS_STOP);
        app_connected_uninit();
    }
}
void app_connected_close_in_other_mode()
{
    int argv[2];
    argv[0] = (int)app_connected_close_in_other_mode_in_app_core;
    argv[1] = 0;
    int ret = os_taskq_post_type("app_core", Q_CALLBACK, 2, argv);
    if (ret) {
        r_printf("app_connected_close_in_other_mode taskq post err %d!\n", __LINE__);
    }
}

u8 le_audio_need_requesting_phone_connection()  //20250618 优化操作进配对断开LE后不需要手机回连
{
    printf("le_audio_need_requesting_phone_connection\n");
    return 0;   // 1：需要手机回连    0：不需要手机回连
}

/**
 * @brief le_audio广播开关设置
 * @param en 1：开启；0：关闭
 */
void le_audio_adv_api_enable(u8 en)
{
    if (!get_bt_le_audio_config()) {
        r_printf("le_audio_en_config = 0");
        return;
    }
    if (g_le_audio_hdl.le_audio_profile_ok == 0) {
        r_printf("le_audio_profile_ok = 0");
        //退出状态不操作LEA广播
        return;
    }
#if TCFG_JL_UNICAST_EDR_MODE_SWITCH_ENABLE
    if (jl_unicast_edr_mode_get() != JL_UNICAST_MODE_DEFAULT) {
        en = 0;
    }
#endif
    if (en) {
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_JL_UNICAST_SINK_EN)
        if (is_cig_phone_conn()) {
            r_printf("have cig_phone_conn");
            return;
        }
#endif
    }
    if (app_var.goto_poweroff_flag || g_bt_hdl.exiting) {
        printf("le_audio_adv_api_enable:%d,%d\n", app_var.goto_poweroff_flag, g_bt_hdl.exiting);
        return;
    }
    r_printf("le_audio_adv_api_enable=%d\n", en);
#if TCFG_USER_TWS_ENABLE
    if (tws_api_get_role() == TWS_ROLE_SLAVE) {
        bt_le_audio_adv_enable(en);
    } else
#endif
    {
        bt_le_audio_adv_enable(en);
    }

}

/*提供使用参考用户设置le audio的广播包配置可连接不可发现状态*/
void le_audio_adv_close_discover_mode()
{
    le_audio_adv_api_enable(0);
    le_audio_set_discover_mode(0);
    le_audio_adv_api_enable(1);
}
void le_audio_adv_open_discover_mode()
{
    le_audio_adv_api_enable(0);
    le_audio_set_discover_mode(1);
    le_audio_adv_api_enable(1);
}

/*le audio profile lib check tws ear side to do something different*/
#define LE_AUDIO_LEFT_EAR                                       1
#define LE_AUDIO_RIGHT_EAR                                      2
#define LE_AUDIO_BOTH_EAR                                       3
/*
 * 实现le_audio_profile.a里面的weak函数，给库获取当前耳机的左右情况
 * */
u8 le_audio_get_tws_ear_side()
{
#if TCFG_USER_TWS_ENABLE
#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_LR)
    return LE_AUDIO_BOTH_EAR;
#else
    if (tws_api_get_local_channel()  == 'R') {
        return LE_AUDIO_RIGHT_EAR;
    } else {
        return LE_AUDIO_LEFT_EAR;
    }
#endif
#else
#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_MONO_L)
    return LE_AUDIO_LEFT_EAR;
#elif (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_MONO_R)
    return LE_AUDIO_LEFT_EAR;
#else
    return LE_AUDIO_BOTH_EAR;
#endif
#endif
}
/*
 * 实现le_audio_profile.a里面的weak函数，给库获取当前耳机的主从情况
 * le audio profile lib check tws role to do something different
*/
static void tws_sync_le_audio_sirk();
static void tws_sync_le_audio_adv_mac_to_slave();
static void tws_sync_le_audio_adv_mac_to_master();
/*le audio profile lib check tws role to do something different*/
u8 le_audio_get_tws_role()
{
#if TCFG_USER_TWS_ENABLE
    if (tws_api_get_role() == TWS_ROLE_SLAVE) {
        g_le_audio_hdl.le_audio_tws_role = TWS_ROLE_SLAVE;
        return 0;
    }
    g_le_audio_hdl.le_audio_tws_role = TWS_ROLE_MASTER;
#endif
    return 1;
}
/*这个是16个byte长度的值，可以是随机值再保存下来使用，
 * 主从一样的时候说明主从是一个设备组
 *tws配对之后，主机要发送这个值给从机
 * */
static u8 default_sirk[16] = {0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};
u8 le_audio_adv_local_mac[6] = {0xff};
u8 le_audio_adv_slave_mac[6] = {0xff};
/*
 * 实现le_audio_profile.a里面的weak函数，给库获取当前耳机的sirk配置值
 * */
u8 le_audio_get_user_sirk(u8 *sirk)
{
    memcpy(sirk, default_sirk, 16);
    return 0;
}
void le_audio_adv_open_success(void *le_audio_ble_hdl, u8 *addr)
{
    log_info("le_audio_adv_open_success\n");
    memcpy(le_audio_adv_local_mac, addr, 6);
}

u8 *le_audio_adv_local_mac_get(void)
{
    return le_audio_adv_local_mac;
}

enum {
    UNICAST_INDXT = 1,
    UNICAST_FOR_JL_HEADSET_INDXT,
    UNICAST_FOR_JL_TWS_INDXT,

};

static u32 ear_version[2] = {0x00, 0x04};           // 临时定义耳机版本号，后面可以考虑存在VM，小端对齐
static u32 dongle_common_version[2] = {0x00, 0x00}; // 耳机和dongle协商共用用双方中的低版本

static u32 dongle_codec_type = 0;
/* --------------------------------------------------------------------------*/
/* @brief   设置dongle耳机的编解码类型 */
/* ----------------------------------------------------------------------------*/
void le_audio_set_dongle_codec_type(u8 codec_type)
{
    printf("codec_type = %d\n", codec_type);
    if (codec_type == 0) {
        dongle_codec_type = AUDIO_CODING_LC3;       // 默认使能LC3编解码
    } else if (codec_type == 1) {
        dongle_codec_type = AUDIO_CODING_JLA_V2;
#if (!TCFG_DEC_JLA_V2_ENABLE) || (!TCFG_ENC_JLA_V2_ENABLE)
        ASSERT(0, "Encoder and decoder undefined!!!");    // 对应编解码没使能，断言处理
#endif
    } else {
        r_f_printf("ERROR This format has not been discussed yet!!!");   // 其他编解码格式，dongle还没和耳机协商，暂未添加。
        r_f_printf("Please use other encoder and decoder formats or manually add it");
    }
    printf("dongle_codec_type = 0x%08X\n", dongle_codec_type);
}

/* --------------------------------------------------------------------------*/
/* @brief   获取dongle耳机的编解码类型 */
/* ----------------------------------------------------------------------------*/
u32 le_audio_get_dongle_codec_type(void)
{
    printf("get dongle_codec_type = 0x%08X\n", dongle_codec_type);
    return dongle_codec_type;
}

u8 pri_data[30];
void le_audio_send_priv_cmd(u16 conn_handle, u8 cmd, u8 *data, u8 len)
{
    int send_len = 1;
    pri_data[0] = cmd;

    memcpy(&pri_data[1], data, len);
    send_len += len;
    log_info_hexdump(pri_data, send_len);
    /* if (cmd == VENDOR_PRIV_LC3_INFO) { */
    /*     send_len += get_unicast_lc3_info(&pri_data[1]); */
    /* } */
    if (cmd == VENDOR_PRIV_DEVICE_TYPE_REQ) {
        log_info("VENDOR_PRIV_DEVICE_TYPE_REQ\n");
    }
    log_info("le_audio_send_priv_cmd, handle:0x%x , cmd:0x%x\n", conn_handle, cmd);
    ll_hci_vendor_send_priv_cmd(conn_handle, pri_data, send_len);
}

static u16 get_conn_handle(void)
{
    int i, j;
    u16 con_handle = 0;
    //TODO 暂时没有添加获取实时有音频流的链路进行控制,选择刚连上的链路控制媒体行为

    for (i = 0; i < CIG_MAX_NUMS; i++) {
        if (app_cig_conn_info[i].used) {
            for (j = 0; j < CIG_MAX_CIS_NUMS; j++) {
                if (app_cig_conn_info[i].cis_conn_info[j].acl_hdl) {
                    con_handle = app_cig_conn_info[i].cis_conn_info[j].acl_hdl;
                }
            }
        }
    }

    return con_handle;
}

#if JL_UNICAST_DUAL_UAC_ENABLE
void le_audio_set_dual_uac_vol(u8 _uac0_vol, u8 _uac1_vol)
{
    u8 uac_vol_max = 100;
    u8 uac_vol_min = 0;
    u8 valid_uac0_vol = (_uac0_vol > uac_vol_max) ? uac_vol_max :
                        (_uac0_vol < uac_vol_min) ? uac_vol_min : _uac0_vol;
    u8 valid_uac1_vol = (_uac1_vol > uac_vol_max) ? uac_vol_max :
                        (_uac1_vol < uac_vol_min) ? uac_vol_min : _uac1_vol;
    u16 con_handle = get_conn_handle();
    if (con_handle) {
        log_info("le_audio_set_dual_uac_vol, valid_uac0_vol:%d , valid_uac1_vol:%d\n", valid_uac0_vol, valid_uac1_vol);
        u8 uac_vol[2] = {valid_uac0_vol, valid_uac1_vol};
        syscfg_write(CFG_JL_CIS_DUAL_UAC_VOL, uac_vol, 2);
        le_audio_send_priv_cmd(con_handle, VENDOR_PRIV_SET_DUAL_UAC_VOL, uac_vol, 2);
    } else {
        log_error("no cis conn\n");
    }
}
#endif
/* --------------------------------------------------------------------------*/
/**
 * @brief   le audio媒体控制接口
 *
 * @return
 */
/* ----------------------------------------------------------------------------*/
void le_audio_media_control_cmd(u8 *data, u8 len)
{
    u16 con_handle = get_conn_handle();

    if (con_handle) {
        log_info("le_audio_media_control_cmd, handle:0x%x , data:\n", con_handle);
        put_buf(data, len);
#if TCFG_BT_VOL_SYNC_ENABLE
        switch (data[0]) {
        case CIG_EVENT_OPID_VOLUME_UP:
            log_info("sync vol to master up\n");
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_UNICAST_SINK_EN)
            bt_cmd_prepare(USER_CTRL_CMD_SYNC_VOL_INC, 0, NULL);
            le_audio_send_priv_cmd(con_handle, VENDOR_PRIV_ACL_MUSIC_VOLUME, &(app_var.opid_play_vol_sync), sizeof(app_var.opid_play_vol_sync));
#elif (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_JL_UNICAST_SINK_EN)
            le_audio_send_priv_cmd(con_handle, VENDOR_PRIV_ACL_OPID_CONTORL, data, len);
#endif
            break;
        case CIG_EVENT_OPID_VOLUME_DOWN:
            log_info("sync vol to master down\n");
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_UNICAST_SINK_EN)
            bt_cmd_prepare(USER_CTRL_CMD_SYNC_VOL_DEC, 0, NULL);
            le_audio_send_priv_cmd(con_handle, VENDOR_PRIV_ACL_MUSIC_VOLUME, &(app_var.opid_play_vol_sync), sizeof(app_var.opid_play_vol_sync));
#elif (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_JL_UNICAST_SINK_EN)
            le_audio_send_priv_cmd(con_handle, VENDOR_PRIV_ACL_OPID_CONTORL, data, len);
#endif
            break;
        default:
            le_audio_send_priv_cmd(con_handle, VENDOR_PRIV_ACL_OPID_CONTORL, data, len);
            break;
        }
#else
        le_audio_send_priv_cmd(con_handle, VENDOR_PRIV_ACL_OPID_CONTORL, data, len);
#endif
    } else {
        log_info("No handle...,send fail");
    }
}

static u16 ble_user_priv_cmd_handle(u16 handle, u8 *cmd, u8 len, u8 *rsp)
{
    u8 cmd_opcode = cmd[0];
    u16 rsp_len = 0;
    log_info("ble_user_priv_cmd_handle:%x, len:%d\n", cmd[0], len);
    put_buf(cmd, len);
    switch (cmd_opcode) {
    case VENDOR_PRIV_DEVICE_TYPE_REQ:
        rsp[0] = VENDOR_PRIV_DEVICE_TYPE_RSP;

#if (TCFG_USER_TWS_ENABLE)
        rsp[1] = UNICAST_FOR_JL_TWS_INDXT;
#else
        rsp[1] = UNICAST_FOR_JL_HEADSET_INDXT;
#endif
#if (TCFG_USER_TWS_ENABLE)
        log_info("get tws state:%d, %d\n", tws_api_get_tws_state(), tws_api_get_role());
        //主从均可获取adv_mac
        if (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED) {
            /* if (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED && tws_api_get_role() == TWS_ROLE_MASTER) { */
            memcpy(rsp + 2, le_audio_adv_slave_mac, 6);
            log_info("RSP, le_audio_adv_slave_mac:");
            put_buf(le_audio_adv_slave_mac, 6);
        } else
#endif
        {
            log_info("RSP, le_audio_adv_slave_mac null");
            memset(rsp + 2, 0xff, 6);
        }
        if (lmp_get_esco_conn_statu()) {
            r_printf("le_audio_conn esco_conn have");
            rsp[8] = 1 ;
        } else {
            rsp[8] = 0 ;
        }
#if TCFG_USER_TWS_ENABLE
        if (bt_tws_get_local_channel() == 'L') {
            rsp[9] = LE_AUDIO_LEFT_EAR;
        } else if (bt_tws_get_local_channel() == 'R') {
            rsp[9] = LE_AUDIO_RIGHT_EAR;
        } else
#endif
        {
            rsp[9] = 0;
        }

        memcpy(dongle_common_version, cmd + 1, 2);   // 先用dongle的版本号做公共版本号
        u16 dongle_current_version = (u16)(cmd[1] << 8 | cmd[2]);
        u16 ear_current_version = (u16)(ear_version[0] << 8 | ear_version[1]);
        log_info("dongle_version:%04x, ear_version:%04x\n", dongle_current_version, ear_current_version);
        if (!dongle_current_version) {
            // 耳机和dongle的版本号都为空，临时用个最低的版本号，或者报错
            r_f_printf("ERROR dongle version are NULL!!\n");
            r_f_printf("ERROR dongle version are NULL!!\n");
            r_f_printf("ERROR dongle version are NULL!!\n");
        } else if ((ear_current_version != 0 && (dongle_current_version > ear_current_version))) {
            // 耳机版本比dongle版本低，或者dongle无版本号，就用耳机的版本
            printf("use earphone version!!\n");
            memcpy(dongle_common_version, ear_version, 2);
        } else {
            // dongle版本比耳机版本低，或者耳机无版本号，或者两者相同，就用dongle的版本
            printf("use dongle version!!\n");
        }

        rsp[10] = ear_version[0];
        rsp[11] = ear_version[1];

        // 命令类型	远端设备类型 Tws对耳地址 手机通话状态 Tws左右声道 从机版本号
        rsp_len = 1 + 1 + 6 + 1 + 1 + 2;
        le_audio_set_dongle_codec_type(cmd[3]);
        put_buf(&rsp[2], 6);
        set_le_audio_jl_dongle_device_type(1);
        cig_event_to_user(CIG_EVENT_JL_DONGLE_CONNECT, (void *)&handle, 2);

        break;
    case VENDOR_PRIV_LC3_INFO:
        set_unicast_lc3_info(&cmd[1], len - 1);
        break;
    case VENDOR_PRIV_OPEN_MIC:
        app_send_message(APP_MSG_LE_AUDIO_CALL_OPEN, handle);
        break;
    case VENDOR_PRIV_CLOSE_MIC:
        app_send_message(APP_MSG_LE_AUDIO_CALL_CLOSE, handle);
        break;
    case VENDOR_PRIV_ACL_MUSIC_VOLUME:
        log_info("Get master music vol:%d", cmd[1]);
        struct volume_cfg music_cfg;
        music_cfg.bypass = VOLUME_NODE_CMD_SET_VOL;
        //要注意原先dongle发下来的总等级，是0~127，要根据实际最高等级填入，
        //下面是将127级转换成16级，这里的16，是MIC的音量控制器节点填入的等级
        printf("cmd[1]:%d\n", cmd[1]);
        music_cfg.cur_vol = cmd[1] * 16 / 127;    // mic实际音量登记 = dongle发下来的实际等级 * music音量总等级 / dongle发下来的总等级
        int err1 = jlstream_set_node_param(NODE_UUID_VOLUME_CTRLER, "LEA_Media", (void *)&music_cfg, sizeof(struct volume_cfg));
        printf(">>>lea music vol:%d, ret:%d", music_cfg.cur_vol, err1);
        break;
    case VENDOR_PRIV_ACL_MIC_VOLUME:
        log_info("Get master mic vol:%d", cmd[1]);
        // set_music_device_volume(cmd[1]);
        struct volume_cfg cfg;
        cfg.bypass = VOLUME_NODE_CMD_SET_VOL;
        //要注意原先dongle发下来的总等级，是0~127，要根据实际最高等级填入，
        //下面是将127级转换成16级，这里的16，是MIC的音量控制器节点填入的等级 -- 国斌
        printf("cmd[1]:%d\n", cmd[1]);
        cfg.cur_vol = cmd[1] * 16 / 127;    // mic实际音量登记 = dongle发下来的实际等级 * mic音量总等级 / dongle发下来的总等级
        int err = jlstream_set_node_param(NODE_UUID_VOLUME_CTRLER, "LEA_CallMic", (void *)&cfg, sizeof(struct volume_cfg));
        printf(">>>lea mic vol:%d, ret:%d", cfg.cur_vol, err);
        break;
    case VENDOR_PRIV_HOST_TYPE:
        g_le_audio_hdl.cig_dongle_host_type = (uint32_t)cmd[1] << 24 |
                                              (uint32_t)cmd[2] << 16 |
                                              (uint32_t)cmd[3] << 8  |
                                              (uint32_t)cmd[4];
        log_info("Get dongle host type:%d", g_le_audio_hdl.cig_dongle_host_type);
        break;
    }
    log_info("rsp_date len:%d\n", rsp_len); // debug用的
    put_buf(rsp, rsp_len); // debug用的
    return rsp_len;
}
/*
 * le audio功能总初始化函数
 * */
void le_audio_profile_init()
{
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_JL_UNICAST_SINK_EN)
    printf("JL le_audio_profile_init:%d\n", g_le_audio_hdl.le_audio_profile_ok);
#else
    printf("le_audio_profile_init:%d\n", g_le_audio_hdl.le_audio_profile_ok);
#endif
    if (get_bt_le_audio_config() && (g_le_audio_hdl.le_audio_profile_ok == 0)) {
#if (TCFG_LE_AUDIO_RCSP_USE_SAME_ACL)
        le_audio_user_server_profile_init(rcsp_profile_data);
#endif
#if (THIRD_PARTY_PROTOCOLS_SEL & HID_ISO_EN)
        le_audio_user_server_profile_init(hid_iso_profile_data);
#endif
        g_le_audio_hdl.le_audio_profile_ok = 1;
        char le_audio_name[LOCAL_NAME_LEN] = {0};     //le_audio蓝牙名
        memcpy(le_audio_name, (u8 *)bt_get_local_name(), LOCAL_NAME_LEN);
        le_audio_name_reset((u8 *)le_audio_name, strlen(le_audio_name));
        le_audio_init(1);
        app_connected_init();
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_JL_UNICAST_SINK_EN)
        ble_vendor_priv_cmd_handle_register(ble_user_priv_cmd_handle);
#endif
        app_connected_open();
        le_audio_ops_register(0);

        make_rand_num(default_sirk);
    }
#if TCFG_JL_UNICAST_EDR_MODE_SWITCH_ENABLE
    if (jl_unicast_edr_mode_get() != JL_UNICAST_MODE_DEFAULT) {
        printf("close acl callback register\n");
        ll_set_param_aclMaxPduCToP(0);
        ll_conn_rx_acl_callback_register(0); //关闭私有cis回调注册，rcsp才能连接
    }
#endif
}
/*
 * le audio功能总退出函数
 * */
void le_audio_profile_exit()
{
    printf("le_audio_profile_exit:%d\n", g_le_audio_hdl.le_audio_profile_ok);
    if (g_le_audio_hdl.le_audio_profile_ok == 0) {
        return;
    }
    le_audio_adv_api_enable(0);
    g_le_audio_hdl.le_audio_profile_ok = 0;
    if (is_cig_phone_conn()) {
        local_irq_disable();
        log_info("le_hci_disconnect_all_connections\n");
        le_hci_disconnect_all_connections();
        local_irq_enable();
    } else {
        log_info("app_connected_close\n");
        app_connected_close_in_other_mode();
    }
}

/*
 * 一些公共消息按需处理
 * */
static int le_audio_app_msg_handler(int *msg)
{
    u8 comm_addr[6];

    switch (msg[0]) {
    case APP_MSG_STATUS_INIT_OK:
        log_info("APP_MSG_STATUS_INIT_OK");
#if (TCFG_USER_TWS_ENABLE == 0)
        le_audio_profile_init();
        le_audio_adv_open_discover_mode();
#endif
        break;
    case APP_MSG_ENTER_MODE://1
        log_info("APP_MSG_ENTER_MODE");
        break;
    case APP_MSG_BT_GET_CONNECT_ADDR://1
        log_info("APP_MSG_BT_GET_CONNECT_ADDR");
        break;
    case APP_MSG_BT_OPEN_PAGE_SCAN://1
        log_info("APP_MSG_BT_OPEN_PAGE_SCAN");
        break;
    case APP_MSG_BT_CLOSE_PAGE_SCAN://1
        log_info("APP_MSG_BT_CLOSE_PAGE_SCAN");
        break;
    case APP_MSG_BT_ENTER_SNIFF:
        break;
    case APP_MSG_BT_EXIT_SNIFF:
        break;
    case APP_MSG_TWS_PAIRED://1
        log_info("APP_MSG_TWS_PAIRED");
        le_audio_profile_init();
        break;
    case APP_MSG_TWS_UNPAIRED://1
        le_audio_profile_init();
        le_audio_adv_api_enable(1);
        log_info("APP_MSG_TWS_UNPAIRED");
        break;
    case APP_MSG_TWS_POWERON_CONN_TIMEOUT:
        puts("APP_MSG_TWS_POWERON_CONN_TIMEOUT\n");
        le_audio_adv_api_enable(1);
        break;
    case APP_MSG_TWS_PAIR_SUSS://1
        log_info("APP_MSG_TWS_PAIR_SUSS");
    case APP_MSG_TWS_CONNECTED://1
        log_info("APP_MSG_TWS_CONNECTED--in app le connect");
#if TCFG_USER_TWS_ENABLE
        tws_sync_le_audio_conn_info();
        //配对之后主从广播的情况不一样
        if (tws_api_get_role() == TWS_ROLE_MASTER) {
            y_printf("APP_MSG_TWS_CONNECTED,sync adv mac");
            le_audio_adv_api_enable(0);
            le_audio_adv_api_enable(1);
            tws_sync_le_audio_adv_mac_to_slave();  //主机同步地址给从机
            tws_sync_le_audio_sirk();   //从机同步地址给主机
        } else {
            //if slave cis is playing,then send vol to master
            if (is_cig_music_play() || is_cig_phone_call_play()) {
                void bt_tws_slave_sync_volume_to_master();
                bt_tws_slave_sync_volume_to_master();
            }

        }
#endif

        break;
    case APP_MSG_TWS_DISCONNECTED:
        log_info("APP_MSG_TWS_DISCONNECTED -in app le connect");
#if TCFG_USER_TWS_ENABLE
        puts("to master le_audio_adv_mac null\n");
        u16 con_handle = get_conn_handle();
        if (con_handle) {
            le_audio_send_priv_cmd(con_handle, VENDOR_PRIV_DEVICE_TYPE_REQ, NULL, 0);
        }
        break;
#endif
    case APP_MSG_POWER_OFF://1
        log_info("APP_MSG_POWER_OFF");
        break;
    case APP_MSG_LE_AUDIO_MODE:
        log_debug("APP_MSG_LE_AUDIO_MODE=%d\n", msg[1]);
#if TCFG_USER_TWS_ENABLE
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            break;
        }
#endif
        le_audio_surport_config(msg[1]);
        break;
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_JL_UNICAST_SINK_EN)
    case APP_MSG_LE_AUDIO_CALL_OPEN:
        g_le_audio_hdl.cig_phone_conn_status |= APP_CONNECTED_STATUS_PHONE_CALL;
        connected_perip_connect_recoder(1, msg[1]);
        break;
    case APP_MSG_LE_AUDIO_CALL_CLOSE:
        g_le_audio_hdl.cig_phone_conn_status &= ~APP_CONNECTED_STATUS_PHONE_CALL;
        connected_perip_connect_recoder(0, msg[1]);
#if TCFG_BT_DUAL_CONN_ENABLE
#if (LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_CONFIG & LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_PLAY_MIX)
        for (int i = 0; i < CIG_MAX_NUMS; i++) {
            if (app_cig_conn_info[i].used && app_cig_conn_info[i].break_a2dp_by_le_audio_call) {
                app_cig_conn_info[i].break_a2dp_by_le_audio_call = 0;
                try_a2dp_resume_by_le_audio_preempted();
            }
        }
#endif
#endif
        break;
#endif
    default:
        break;
    }
    return 0;
}
/*
 * 一些蓝牙线程消息按需处理
 * */
static int le_audio_conn_btstack_event_handler(int *_event)
{
    struct bt_event *event = (struct bt_event *)_event;
    log_info("le_audio_conn_btstack_event_handler:%d\n", event->event);
    switch (event->event) {
    case BT_STATUS_FIRST_CONNECTED:
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_JL_UNICAST_SINK_EN)
#if LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_SWITCH
        le_audio_adv_api_enable(1);//手机连接成功后，继续开广播给dongle连接
#else
        le_audio_adv_api_enable(0);
#endif
#else
        if (get_bt_le_audio_config()) {
            clr_device_in_page_list();
            bt_cmd_prepare(USER_CTRL_PAGE_CANCEL, 0, NULL);
#if TCFG_USER_TWS_ENABLE
            if (tws_api_get_role() == TWS_ROLE_SLAVE) {
                break;
            }
#endif
            le_audio_adv_api_enable(0);
        }
#endif
        break;
    case BT_STATUS_FIRST_DISCONNECT:
        if (get_bt_le_audio_config()) {
            le_audio_adv_api_enable(1);
        }
        break;
    }
    return 0;

}

static int le_audio_app_hci_event_handler(int *_event)
{
    struct bt_event *event = (struct bt_event *)_event;
    printf("le_audio_app_hci_event_handler:%d\n", event->event);
    switch (event->event) {
    case HCI_EVENT_CONNECTION_COMPLETE:
        break;
    case HCI_EVENT_DISCONNECTION_COMPLETE :
        printf("app_le_audio HCI_EVENT_DISCONNECTION_COMPLETE: %0x\n", event->value);
        if (event->value == ERROR_CODE_CONNECTION_TIMEOUT) {
            //超时断开设置上请求回连标记
            le_audio_adv_api_enable(0);
            le_audio_set_requesting_a_connection_flag(1);
            le_audio_adv_api_enable(1);
        } else {
            le_audio_adv_api_enable(0);
            le_audio_adv_api_enable(1);
        }
        break;
    }
    return 0;
}



APP_MSG_HANDLER(le_audio_hci_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_BT_HCI,
    .handler    = le_audio_app_hci_event_handler,
};
APP_MSG_HANDLER(le_audio_app_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_APP,
    .handler    = le_audio_app_msg_handler,
};
APP_MSG_HANDLER(conn_stack_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_BT_STACK,
    .handler    = le_audio_conn_btstack_event_handler,
};


//le audio vol control , weak function redo
/*conn_handle(2),volume_setting(1),change_step_size(1),mute(1)*/
#define LEA_VCS_SERVER_VOLUME_STATE   0x00
/*conn_handle(2),volume_flags(1)*/
#define LEA_VCS_SERVER_VOLUME_FLAGS   0x01
//Microphone Control Service
/*conn_handle(2),mute_state(1)*/
#define LEA_MICS_SERVER_MUTE_STATE    0x02

/**
 * 实现le_audio_profile.a里面的weak函数，获取到了手机配置的音量消息，音量值更新
 */
void le_audio_profile_event_to_user(u16 type, u8 *data, u16 len)
{
    log_info("le_audio_profile_event in app");
    put_buf(data, len);
    if (type == LEA_VCS_SERVER_VOLUME_STATE) {
        u8 phone_vol = data[2]; //value is (0 -> 255)
        //change to 0-127
        set_music_device_volume(phone_vol / 2);
    }
}
/**
 * 有些手机连接过同一个地址之后会记忆耳机的服务，所以动态配置le audio的功能的时候，
 * 仅经典蓝牙功能和经典蓝牙+le audio功能的地址要不一样
 * */
void le_audio_surport_config_change_addr(u8 ramdom)
{
    u8 mac_buf[6];
    u8 comm_mac_buf[12];
    int ret = 0;
    ret = syscfg_read(CFG_TWS_COMMON_ADDR, mac_buf, 6);
    if (ret == 6) {
        memcpy(&comm_mac_buf[6], mac_buf, 6);
        for (int i = 0; i < 6; i++) {
            mac_buf[i] = mac_buf[5 - i] + ramdom;
        }
        memcpy(comm_mac_buf, mac_buf, 6);
        log_debug("le_audio_surport_config_change_addr comm=%x", ramdom);
        put_buf(comm_mac_buf, 12);
        syscfg_write(CFG_TWS_COMMON_ADDR, comm_mac_buf, 12);

    } else {
        ret = syscfg_read(CFG_BT_MAC_ADDR, mac_buf, 6);
        if (ret == 6) {
            for (int i = 0; i < 6; i++) {
                mac_buf[i] += mac_buf[5 - i] + ramdom;
            }
            log_debug("le_audio_surport_config_change_addr bt_addr");
            put_buf(mac_buf, 6);
            syscfg_write(CFG_BT_MAC_ADDR, mac_buf, 6);

        }

    }
}
/**
 *实现le_audio_profile.a里面的weak函数，这个回调表示le audio广播刚连上，加密之前，协议还没开始连
 * */
void le_audio_adv_conn_success(u8 adv_id)
{
    log_info("le_audio_adv_conn_success,adv id:%d\n", adv_id);
    g_le_audio_hdl.le_audio_adv_connected = 0xAA;
}
/**
 *实现le_audio_profile.a里面的weak函数，这个回调表示le audio的广播连接断开回调
 * */
void le_audio_adv_disconn_success(u8 adv_id)
{
    log_info("le_audio_adv_disconn_success,adv id:%d\n", adv_id);
    g_le_audio_hdl.le_audio_adv_connected = 0;
}
/*定义接口获取le audio广播的连接状态*/
u8  le_audio_get_adv_conn_success()
{
    return !(g_le_audio_hdl.le_audio_adv_connected == 0xAA);
}
/*
 * 实现le_audio_profile.a里面的weak函数,传递出来更多参数可用于功能扩展
 * 这个回调表示le audio广播刚连上，加密之前，协议还没开始连
 * */
void le_audio_profile_connected_for_cig_peripheral(u8 status, u16 acl_handle, u8 *addr)
{
}

#if TCFG_USER_TWS_ENABLE
/*一些tws线程消息按需处理*/
int le_audio_tws_connction_status_event_handler(int *msg)
{
    struct tws_event *evt = (struct tws_event *)msg;
    int role = evt->args[0];
    int phone_link_connection = evt->args[1];
    int reason = evt->args[2];

    log_info("le_audo tws-user: role= %d, phone_link_connection %d, reason=%d,event= %d\n",
             role, phone_link_connection, reason, evt->event);

    switch (evt->event) {
    case TWS_EVENT_CONNECTED:
        break;
    case TWS_EVENT_CONNECTION_DETACH:
        g_le_audio_hdl.cig_phone_other_conn_status = 0;
        memset(le_audio_adv_slave_mac, 0xff, 6);
        break;
    }
    return 0;
}
APP_MSG_HANDLER(le_audio_tws_msg_handler) = {
    .owner      = APP_MODE_BT,
    .from       = MSG_FROM_TWS,
    .handler    = le_audio_tws_connction_status_event_handler,
};

/*
 *有些le audio信息TWS之间的同步流程
 * */
static void tws_sync_le_audio_config_func(u8 *data, int len)
{
    log_debug("tws_sync_le_audio_config_func: %d, %d\n", data[0], data[1]);
    switch (data[0]) {
    case LE_AUDIO_CONFIG_EN:
        if (data[1]) {
            g_le_audio_hdl.le_audio_en_config = 1;
        } else {
            g_le_audio_hdl.le_audio_en_config = 0;
        }
#if TCFG_JL_UNICAST_EDR_MODE_SWITCH_ENABLE
        g_le_audio_hdl.jl_unicast_mode = data[3];
        log_debug("set_le_audio_surport_config cpu_reset=%d, jl_unicast_mode=%d\n", g_le_audio_hdl.le_audio_en_config, g_le_audio_hdl.jl_unicast_mode);
        syscfg_write(CFG_JL_UNICAST_EDR_MODE, &(g_le_audio_hdl.jl_unicast_mode), 1);
#else
        log_debug("set_le_audio_surport_config cpu_reset=%d\n", g_le_audio_hdl.le_audio_en_config);
#endif
        syscfg_write(CFG_LE_AUDIO_EN, &(g_le_audio_hdl.le_audio_en_config), 1);
        le_audio_surport_config_change_addr(data[2]);
        bt_cmd_prepare(USER_CTRL_DEL_ALL_REMOTE_INFO, 0, NULL);
        bt_cmd_prepare(USER_CTRL_POWER_OFF, 0, NULL);
        os_time_dly(10);
        cpu_reset();//开关le_audio之后重新reset重启,初始化相关服务
        break;
    case LE_AUDIO_CONN_STATUES:
        puts("LE_AUDIO_CONN_STATUES\n");
        g_le_audio_hdl.cig_phone_other_conn_status = data[1];
        log_info("cig_phone_other_conn_status:%d\n", g_le_audio_hdl.cig_phone_other_conn_status);
#if TCFG_AUTO_SHUT_DOWN_TIME
        if (g_le_audio_hdl.cig_phone_other_conn_status & APP_CONNECTED_STATUS_CONNECT) {
            sys_auto_shut_down_disable();
        }
#endif
#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN)))
        if ((g_le_audio_hdl.cig_phone_other_conn_status & APP_CONNECTED_STATUS_CONNECT) || (g_le_audio_hdl.cig_phone_conn_status & APP_CONNECTED_STATUS_CONNECT)) {
            puts("le_audio connect, close page\n");
            clr_device_in_page_list();
            bt_cmd_prepare(USER_CTRL_PAGE_CANCEL, 0, NULL);
        }
        if (tws_api_get_role() == TWS_ROLE_MASTER) {
            tws_dual_conn_state_handler();
        }
#endif
        break;
    case LE_AUDIO_CONFIG_SIRK:
        //单向主更新给从,此处为从机处理
        memcpy(default_sirk, &data[1], 16);
        log_info("slave get SIRK");
        put_buf(default_sirk, 16);
        le_audio_adv_api_enable(0);
        le_audio_adv_api_enable(1);
        tws_sync_le_audio_adv_mac_to_master();
        break;
    case LE_AUDIO_GET_SLAVE_VOL:
        if (is_cig_music_play() || is_cig_phone_call_play()) {
            break ;
        }
        log_info("master get slave vol while slave playing cis");
        app_audio_set_volume(APP_AUDIO_STATE_MUSIC, data[1], 1);
        app_audio_set_volume(APP_AUDIO_STATE_CALL, data[2], 1);
        break;
    case LE_AUDIO_ADV_MAC_INFO:
        puts("LE_AUDIO_ADV_MAC_INFO\n");
        memcpy(le_audio_adv_slave_mac, &data[1], 6);
        put_buf(le_audio_adv_slave_mac, 6);
        u16 con_handle = get_conn_handle();
        if (con_handle) {
            //主机连上后同步从机的adv mac给dongle
            log_info("req master update addr\n");
            le_audio_send_priv_cmd(con_handle, VENDOR_PRIV_DEVICE_TYPE_REQ, NULL, 0);
        }
        break;
    case LE_AUDIO_CONN_CHECK:
#if TCFG_USER_TWS_ENABLE
        if ((is_cig_phone_conn() == 0) && (is_cig_other_phone_conn() == 0)) {
            log_info("LE_AUDIO_CONN_CHECK, tws_dual_conn_state_handler\n");
            tws_dual_conn_state_handler();
        } else if ((is_cig_phone_conn() == 1) && (is_cig_other_phone_conn() == 0) && get_bt_tws_connect_status()) {
            log_info("LE_AUDIO_CONN_CHECK, need dongle creat cis conn\n");
            u16 con_handle = get_conn_handle();
            if (con_handle) {
                y_printf("LE_AUDIO_CONN_CHECK, tws_sync_le_audio_sirk\n");
                tws_sync_le_audio_sirk();
            }
        }
        break;
#endif
    }
    free(data);

}
static void tws_sync_le_audio_info_func(void *_data, u16 len, bool rx)
{
    log_debug("tws_sync_le_audio_info_func:%d", rx);
    if (rx) {
        u8 *data = malloc(len);
        memcpy(data, _data, len);
        int msg[4] = { (int)tws_sync_le_audio_config_func, 2, (int)data, len};
        os_taskq_post_type("app_core", Q_CALLBACK, 4, msg);
    } else {
        u8 *data = _data;
        if (data[0] == LE_AUDIO_CONFIG_EN) {
            log_debug("tx set_le_audio_surport_config cpu_reset=%d\n", g_le_audio_hdl.le_audio_en_config);
            cpu_reset();//开关le_audio之后重新reset重启,初始化相关服务

        }

    }
}
REGISTER_TWS_FUNC_STUB(app_vol_sync_stub) = {
    .func_id = 0x23782C5B,
    .func    = tws_sync_le_audio_info_func,
};

/*
 * le audio的使能配置主从之间信息同步
 * */
void tws_sync_le_audio_en_info(u8 random)
{
    u8 data[4];
    data[0] = LE_AUDIO_CONFIG_EN;
    data[1] = g_le_audio_hdl.le_audio_en_config;
    data[2] = random;
#if TCFG_JL_UNICAST_EDR_MODE_SWITCH_ENABLE
    data[3] = g_le_audio_hdl.jl_unicast_mode;
#else
    data[3] = 0;
#endif
    tws_api_send_data_to_slave(data, 4, 0x23782C5B);

}
/*
 * le audio的连接状态主从之间信息同步
 * */
void tws_sync_le_audio_conn_info()
{
    u8 data[2];
    data[0] = LE_AUDIO_CONN_STATUES;
    data[1] = g_le_audio_hdl.cig_phone_conn_status;
    tws_api_send_data_to_sibling(data, 2, 0x23782C5B);
}
/*
 * le audio的sirk主从之间信息同步
 * */
static void tws_sync_le_audio_sirk()
{
    u8 data[17];
    data[0] = LE_AUDIO_CONFIG_SIRK;
    memcpy(&data[1], default_sirk, 16);
    log_info("master get SIRK");
    put_buf(default_sirk, 16);
    tws_api_send_data_to_slave(data, 17, 0x23782C5B);

}

void tws_sync_le_audio_conn_check()
{
    log_info("tws_sync_le_audio_conn_check");
    u8 data[1];
    data[0] = LE_AUDIO_CONN_CHECK;
    tws_api_send_data_to_sibling(data, 1, 0x23782C5B);
}

void bt_tws_slave_sync_volume_to_master()
{
    u8 data[4];
    //原从机正在播le audio，另一个出仓后是主机，
    //这个时候从不接收主机的音量，并且要同步自己的音量给主
    data[0] = LE_AUDIO_GET_SLAVE_VOL;
    data[1] = app_audio_get_volume(APP_AUDIO_STATE_MUSIC);
    data[2] = app_audio_get_volume(APP_AUDIO_STATE_CALL);

    tws_api_send_data_to_sibling(data, 3, 0x23782C5B);
}
static void tws_sync_le_audio_adv_mac_to_slave()
{
    u8 data[7];
    data[0] = LE_AUDIO_ADV_MAC_INFO;
    memcpy(&data[1], le_audio_adv_local_mac, 6);
    log_info("to slave le_audio_adv_mac");
    put_buf(le_audio_adv_local_mac, 6);
    tws_api_send_data_to_slave(data, 7, 0x23782C5B);
}
static void tws_sync_le_audio_adv_mac_to_master()
{
    u8 data[7];
    data[0] = LE_AUDIO_ADV_MAC_INFO;
    memcpy(&data[1], le_audio_adv_local_mac, 6);
    log_info("to master le_audio_adv_mac");
    put_buf(le_audio_adv_local_mac, 6);
    tws_api_send_data_to_sibling(data, 7, 0x23782C5B);
}
#endif

#if TCFG_JL_UNICAST_EDR_MODE_SWITCH_ENABLE

/**
 * @brief 获取当前JLUNICAST模式的状态
 *
 * @return JL_UNICAST_MODE
 */
u8 jl_unicast_edr_mode_get()
{
    int ret = syscfg_read(CFG_JL_UNICAST_EDR_MODE, &(g_le_audio_hdl.jl_unicast_mode), 1);
    if (ret != 1) {
        g_le_audio_hdl.jl_unicast_mode = JL_UNICAST_MODE_EDR;
        printf("jl_unicast_edr_mode_get fail:%d\n", g_le_audio_hdl.jl_unicast_mode);
    } else {
        printf("jl_unicast_edr_mode_get:%d\n", g_le_audio_hdl.jl_unicast_mode);
    }
    return g_le_audio_hdl.jl_unicast_mode;
}

static void jl_unicast_edr_mode_switch_in_app_core()
{
#if TCFG_USER_TWS_ENABLE
    if (!(tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
        printf("jl_unicast_edr_mode_switch_in_app_core fail, tws disconnect\n");
        return;
    }
#endif
    jl_unicast_edr_mode_get();
    u8 _jl_unicast_mode = g_le_audio_hdl.jl_unicast_mode + 1;     // dongle模式和手机模式轮流切换
    if (_jl_unicast_mode >= JL_UNICAST_MODE_UNKNOW) {
        _jl_unicast_mode = JL_UNICAST_MODE_DEFAULT;
    }
    y_printf("jl_unicast_edr_mode_switch_in_app_core = %d\n", _jl_unicast_mode);

    if (_jl_unicast_mode == JL_UNICAST_MODE_DEFAULT) {    // jlunicast模式
        y_printf("JL_UNICAST_MODE_DEFAULT\n");
#if TCFG_USER_TWS_ENABLE
        if (tws_api_get_role() == TWS_ROLE_MASTER) {
            tws_play_tone_file(get_tone_files()->cis_mode, 400);
        }
#else
        play_tone_file(get_tone_files()->cis_mode);
#endif
        le_audio_surport_config(1);
        int connect_device = bt_get_total_connect_dev();
        printf("bt_get_total_connect_dev = %d\n", connect_device);
        if (connect_device) {                                       // 切换前如果有连接，断开当前所有蓝牙连接
            bt_cmd_prepare(USER_CTRL_DISCONNECTION_HCI, 0, NULL);   // 模式切换时需要清理现有连接，避免新旧模式的连接冲突。
        }
        write_scan_conn_enable(0, 0);
        bt_cmd_prepare(USER_CTRL_PAGE_CANCEL, 0, NULL);
        clr_device_in_page_list();
#if TCFG_USER_TWS_ENABLE
        tws_sync_le_audio_conn_info();
        if (tws_api_get_role() == TWS_ROLE_MASTER) {
#if (THIRD_PARTY_PROTOCOLS_SEL & RCSP_MODE_EN)
            ble_module_enable(0);
#endif
            tws_sync_le_audio_sirk();
            le_audio_adv_api_enable(0);
            le_audio_adv_api_enable(1);
            tws_sync_le_audio_adv_mac_to_slave();//主机同步地址给tws从机
        } else {
            //if slave cis is playing,then send vol to master
            if (is_cig_music_play() || is_cig_phone_call_play()) {
                void bt_tws_slave_sync_volume_to_master();
                bt_tws_slave_sync_volume_to_master();
            }
        }
#else
#if (THIRD_PARTY_PROTOCOLS_SEL & RCSP_MODE_EN)
        ble_module_enable(0);   //关闭rcsp广播
#endif
        le_audio_adv_api_enable(1);
#endif
    } else { // edr模式
        y_printf("JL_UNICAST_MODE_EDR\n");
#if TCFG_USER_TWS_ENABLE
        if (tws_api_get_role() == TWS_ROLE_MASTER) {
            tws_play_tone_file(get_tone_files()->edr_mode, 400);
        }
#else
        play_tone_file(get_tone_files()->edr_mode);
#endif
        le_audio_surport_config(0);
        if (is_cig_phone_conn() || is_cig_other_phone_conn()) {
            local_irq_disable();
            y_printf("le_hci_disconnect_all_connections\n");
            le_hci_disconnect_all_connections();
            local_irq_enable();
        }
        le_audio_disconn_le_audio_link_no_reconnect();
        printf("close acl callback register\n");
        ll_conn_rx_acl_callback_register(0); //关闭私有cis回调注册，rcsp才能连接
        extern void dual_conn_page_devices_init();
        dual_conn_page_devices_init();
#if TCFG_USER_TWS_ENABLE
        tws_dual_conn_state_handler();
#endif
    }

}

/**
 * @brief 控制dongle模式和手机模式切换
 */
void jl_unicast_edr_mode_switch(void)
{
    printf("jl_unicast_edr_mode_switch\n");
#if TCFG_USER_TWS_ENABLE
    int ret = tws_api_sync_call_by_uuid(0X1979EF3B, 0, 400);
    if (ret != 0) {
        printf("jl_unicast_edr_mode_switch fail, ret=%d\n", ret);
    }
#else
    jl_unicast_edr_mode_switch_in_app_core();
#endif
}

#if TCFG_USER_TWS_ENABLE

static void jl_unicast_edr_mode_switch_tws_sync_in_app_core(int args, int err)
{
    printf("jl_unicast_edr_mode_switch_tws_sync_in_app_core\n");
    jl_unicast_edr_mode_switch_in_app_core();
}

TWS_SYNC_CALL_REGISTER(jl_unicast_edr_mode_switch_tws_sync) = {
    .uuid = 0X1979EF3B,
    .task_name = "app_core",
    .func = jl_unicast_edr_mode_switch_tws_sync_in_app_core,
};

#endif

#endif // TCFG_JL_UNICAST_EDR_MODE_SWITCH_ENABLE


/* ----------------------------------------------------------------------------*/
/**
 * @brief  一开始设计是用于app动态配置le audio的功能开关，配置会记录在VM
 *
 * @param   le_audio_en  1-enable   0-disable
 * @return  void
 */
/* ----------------------------------------------------------------------------*/
void le_audio_surport_config(u8 le_audio_en)
{
#if TCFG_USER_TWS_ENABLE
    if (!(tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
        printf("le_audio_surport_config fail, tws disconnect\n");
        return;
    }
#endif
    u8 addr[6] = {0};
#if TCFG_JL_UNICAST_EDR_MODE_SWITCH_ENABLE
    if (le_audio_en) {
        g_le_audio_hdl.jl_unicast_mode = JL_UNICAST_MODE_DEFAULT;
        ll_set_param_aclMaxPduCToP(JL_UNICAST_ACL_MAX_PDU_CTOP);
    } else {
        g_le_audio_hdl.jl_unicast_mode = JL_UNICAST_MODE_EDR;
        ll_set_param_aclMaxPduCToP(0);
    }
    syscfg_write(CFG_JL_UNICAST_EDR_MODE, &(g_le_audio_hdl.jl_unicast_mode), 1);
    log_debug("le_audio_surport_config jl_unicast_mode=%d\n", g_le_audio_hdl.jl_unicast_mode);
#endif
#if TCFG_BT_DUAL_CONN_ENABLE
    set_dual_conn_config(addr, !le_audio_en);//le_audio en close dual_conn
#endif
    if (le_audio_en) {
        g_le_audio_hdl.le_audio_en_config = 1;
    } else {
        g_le_audio_hdl.le_audio_en_config = 0;
    }
    syscfg_write(CFG_LE_AUDIO_EN, &(g_le_audio_hdl.le_audio_en_config), 1);
    log_debug("le_audio_surport_config le_audio_en_config=%d\n", g_le_audio_hdl.le_audio_en_config);
#if (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN)) //如果是公有cis的话，要换蓝牙地址，否则手机那边继续用原来的地址点击连接
    bt_cmd_prepare(USER_CTRL_DEL_ALL_REMOTE_INFO, 0, NULL);
    bt_cmd_prepare(USER_CTRL_POWER_OFF, 0, NULL);
    u8 random = (u8)rand32();
    le_audio_surport_config_change_addr(random);
#if TCFG_USER_TWS_ENABLE
    tws_sync_le_audio_en_info(random);
#endif
#endif

}

void set_le_audio_surport_config(u8 le_audio_en)
{
    app_send_message(APP_MSG_LE_AUDIO_MODE, le_audio_en);
}

/**
 * @brief 蓝牙底层库调用，不允许开广播判断
 */
u8 le_audio_enable_adv_when_disconnect()
{
    printf("le_audio_enable_adv_when_disconnect\n");
#if TCFG_JL_UNICAST_EDR_MODE_SWITCH_ENABLE
    if (g_le_audio_hdl.jl_unicast_mode == 1) {
        y_printf("jl_unicast_edr_mode, close adv page\n");
        //独立配对模式为edr模式不回连
        return 0;
    }
#endif
    if (app_var.goto_poweroff_flag || g_bt_hdl.exiting || (g_le_audio_hdl.le_audio_profile_ok == 0)) {
        //退出状态不操作LEA广播
        return 0;
    } else {
        return 1;
    }
}

/* ----------------------------------------------------------------------------*/
/**
 * @brief 是否支持le_audio功能
 *
 * @return 1:支持 0:不支持
 */
/* ----------------------------------------------------------------------------*/
u8 get_bt_le_audio_config()
{
    return g_le_audio_hdl.le_audio_en_config;
}

/* ----------------------------------------------------------------------------*/
/**
 * @brief 从vm读取是否支持le_audio功能
 *
 * @return 1:支持 0:不支持
 */
/* ----------------------------------------------------------------------------*/
u8 get_bt_le_audio_config_for_vm()
{
#if 1
    //default support le_audio
    g_le_audio_hdl.le_audio_en_config = 1;
    return g_le_audio_hdl.le_audio_en_config;
#else
    int ret = syscfg_read(CFG_LE_AUDIO_EN, &(g_le_audio_hdl.le_audio_en_config), 1);
    if (ret == 1) {
        log_debug("get_bt_le_audio_config_for_vm=%d\n", g_le_audio_hdl.le_audio_en_config);
        return g_le_audio_hdl.le_audio_en_config;
    }
    log_debug("get_bt_le_audio_config_for_vm fail\n");
    return 0;
#endif
}
u8 edr_conn_memcmp_filterate_for_addr(u8 *addr)
{
#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN)))
    if (g_le_audio_hdl.le_audio_en_config) {
        if (g_le_audio_hdl.cig_phone_conn_status & APP_CONNECTED_STATUS_CONNECT) {
            if (memcmp(addr, g_le_audio_hdl.peer_address, 6)) {
                return 1;
            }
        }
    }
#endif
    return 0;
}
//le_audio phone con可以进入power down
static u8 le_audio_idle_query(void)
{
    if ((g_le_audio_hdl.cig_phone_conn_status & APP_CONNECTED_STATUS_CONNECT)
        || (g_le_audio_hdl.cig_phone_conn_status & APP_CONNECTED_STATUS_START)) {
        return 1;
    }
    return 1;
}

REGISTER_LP_TARGET(le_audio_lp_target) = {
    .name = "le_audio_play",
    .is_idle = le_audio_idle_query,
};

#endif

