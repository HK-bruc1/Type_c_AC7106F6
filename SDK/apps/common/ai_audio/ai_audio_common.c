#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".ai_audio_comm.data.bss")
#pragma data_seg(".ai_audio_comm.data")
#pragma const_seg(".ai_audio_comm.text.const")
#pragma code_seg(".ai_audio_comm.text")
#endif
#include "system/includes.h"
#include "app_config.h"
#include "app_msg.h"
#include "sniff.h"
#include "classic/tws_api.h"
#include "reference_time.h"
#include "avctp_user.h"
#include "system/task.h"
#include "ai_audio_common.h"

#define LOG_TAG             "[AI_COMM]"
#define LOG_ERROR_ENABLE
//#define LOG_DEBUG_ENABLE
//#define LOG_INFO_ENABLE
//#define LOG_DUMP_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"

#define TWS_FUNC_ID_AI_AUDIO                TWS_FUNC_ID('A', 'I', 'A', 'U')

struct ai_audio_common_info {
    u16 ble_con_handle;
    u8 ble_remote_addr[6];
    u8 spp_remote_addr[6];
};
static struct ai_audio_common_info ai_audio_comm;

enum {
    TWS_TRANS_CMD_SYNC_REMOTE_ADDR,
};

void ai_audio_common_set_remote_bt_addr(u8 *spp_remote_addr, u16 ble_con_hdl)
{
    memcpy(ai_audio_comm.spp_remote_addr, spp_remote_addr, 6);
    memcpy(&ai_audio_comm.ble_con_handle, &ble_con_hdl, 2);
    log_info("set remote info:");
    log_info("spp_addr:");
    log_info_hexdump(ai_audio_comm.spp_remote_addr, 6);
    log_info("ble_con_hdl: %d", ai_audio_comm.ble_con_handle);
}

u8 *ai_audio_common_get_remote_bt_addr()
{
    u8 *remote_bt_addr = NULL;
    u8 zero_addr[6] = {0};

    if ((tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            if (ai_audio_comm.ble_con_handle) {
                remote_bt_addr = ai_audio_comm.ble_remote_addr;
                log_info("get remote addr:  LINE %d", __LINE__);
                log_info_hexdump(ai_audio_comm.ble_remote_addr, 6);
            } else {
                remote_bt_addr = ai_audio_comm.spp_remote_addr;
                log_info("get remote addr:  LINE %d", __LINE__);
                log_info_hexdump(ai_audio_comm.spp_remote_addr, 6);
            }
            return remote_bt_addr;
        }
    }
    if (ai_audio_comm.ble_con_handle) {
        if (!memcmp(ai_audio_comm.ble_remote_addr, zero_addr, 6)) {
            //TODO
            //if (ai_audio_comm.ble_con_handle >= 0x50) {  //ble
            //   remote_bt_addr = rcsp_get_ble_hdl_remote_mac_addr(ai_audio_comm.ble_con_handle);
            //} else {  //att over edr
            remote_bt_addr = bt_get_current_remote_addr();
            //}
            if (remote_bt_addr) {
                memcpy(ai_audio_comm.ble_remote_addr, remote_bt_addr, 6);
            }
        } else {
            remote_bt_addr = ai_audio_comm.ble_remote_addr;
        }
        log_info("get remote addr:  LINE %d", __LINE__);
        log_info_hexdump(ai_audio_comm.ble_remote_addr, 6);
    } else {  //spp
        remote_bt_addr = ai_audio_comm.spp_remote_addr;
        log_info("get remote addr:  LINE %d", __LINE__);
        log_info_hexdump(ai_audio_comm.spp_remote_addr, 6);
    }
    return remote_bt_addr;
}

void ai_audio_common_sync_remote_bt_addr()
{
    u8 bt_addr_sync[14];

    if (tws_api_get_role() == TWS_ROLE_SLAVE) {
        return;
    }
    ai_audio_common_get_remote_bt_addr();
    memcpy(&bt_addr_sync[0], ai_audio_comm.spp_remote_addr, 6);
    memcpy(&bt_addr_sync[6], &ai_audio_comm.ble_con_handle, 2);
    memcpy(&bt_addr_sync[8], ai_audio_comm.ble_remote_addr, 6);
    ai_audio_common_tws_send_cmd(TWS_FUNC_ID_AI_AUDIO, TWS_TRANS_CMD_SYNC_REMOTE_ADDR, 0, bt_addr_sync, 14);
    log_info("sync remote info:");
    log_info("spp_addr:");
    log_info_hexdump(ai_audio_comm.spp_remote_addr, 6);
    log_info("ble_con_hdl: %d", ai_audio_comm.ble_con_handle);
    log_info("ble_addr:");
    log_info_hexdump(ai_audio_comm.ble_remote_addr, 6);
}

int ai_audio_common_tws_send_cmd(u32 func_id, u8 cmd, u8 auxiliary, void *param, u32 len)
{
    u8 *buf;
    int ret = 0;

    buf = malloc(len + 2);
    if (!buf) {
        return -ENOMEM;
    }
    buf[0] = cmd;
    buf[1] = auxiliary;
    if (param) {
        memcpy(buf + 2, param, len);
    }
    ret = tws_api_send_data_to_sibling(buf, len + 2, func_id);
    free(buf);
    return ret;
}

static void ai_audio_common_tws_msg_handler(u8 *_data, u32 len)
{
    int err = 0;
    u8 cmd;
    //u32 param_len;
    u8 *param;

    cmd = _data[0];
    switch (cmd) {
    case TWS_TRANS_CMD_SYNC_REMOTE_ADDR:
        param = _data + 2;
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            memcpy(ai_audio_comm.spp_remote_addr, param, 6);
            memcpy(&ai_audio_comm.ble_con_handle, param + 6, 2);
            memcpy(ai_audio_comm.ble_remote_addr, param + 8, 6);
            log_info("recv remote info:");
            log_info("spp_addr:");
            log_info_hexdump(ai_audio_comm.spp_remote_addr, 6);
            log_info("ble_con_hdl: %d", ai_audio_comm.ble_con_handle);
            log_info("ble_addr:");
            log_info_hexdump(ai_audio_comm.ble_remote_addr, 6);
        }
        break;
    }

    free(_data);
}

static void ai_audio_common_tws_msg_from_sibling(void *_data, u16 len, bool rx)
{
    u8 *rx_data;
    int msg[4];
    if (!rx) {
        //是否需要限制只有rx才能收到消息
        //return;
    }
    rx_data = malloc(len);
    if (!rx_data) {
        return;
    }
    memcpy(rx_data, _data, len);
    msg[0] = (int)ai_audio_common_tws_msg_handler;
    msg[1] = 2;
    msg[2] = (int)rx_data;
    msg[3] = len;
    int err = os_taskq_post_type("app_core", Q_CALLBACK, 4, msg);
    if (err) {
        printf("tws rx post fail\n");
    }
    //cppcheck-suppress memleak
}

REGISTER_TWS_FUNC_STUB(ai_audio_common_tws_sibling_stub) = {
    .func_id = TWS_FUNC_ID_AI_AUDIO,
    .func = ai_audio_common_tws_msg_from_sibling,
};

