#include "app_config.h"
#include "mma_config.h"
#include "mma_platform_api.h"
#include "btstack/avctp_user.h"
#include "app_power_manage.h"
#include "bt_tws.h"
#include "battery_manager.h"
// #include "earphone.h"
#include "asm/charge.h"
#include "app_chargestore.h"

#if (THIRD_PARTY_PROTOCOLS_SEL & MMA_EN)

#if 0
#define log_info(x, ...)       printf("[MMA PORT]" x " ", ## __VA_ARGS__)
#define log_info_hexdump       put_buf
#else
#define log_info(...)
#define log_info_hexdump(...)
#endif

//////////////////////// DEV CONFIG ATTR SET/GET  OPCODE:0xF2&0xF3&0xF4 /////////////////////

u8 audio_mode = XM_AUDIO_MODE_LP;

u8 key_mapping[12] = {0x04, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x03, 0x03, 0x03, 0x06, 0x06};
u8 key_mapping_len = 12;

u8 anti_lost_status = XM_ANTI_LOST_L_WEARING | XM_ANTI_LOST_R_WEARING;

u8 anc_mode = XM_ANC_MODE_ANC;
u8 anc_mode_mode = XM_ANC_MODE_ANC_DEEP;

u8 spatial_audio = XM_SPATIAL_AUDIO_DISABLE;
u32 device_time;

u8 find_status[2] = {0};
u8 find_enable = 0;
u8 earbud_id = 1;

u8 eq_mode = XM_EQ_MODE_NORMAL;

static int fill_attr_idx(u16 attr_idx, u8 *buffer)
{
    buffer[0] = (attr_idx >> 8) & 0xFF;
    buffer[1] = attr_idx & 0xFF;
    return 2;
}

u8 test_sn[] = "00000000000000000000";
int xm_device_config_attr_get(u16 attr_idx, u8 *buffer)
{
    log_info("xm_device_config_attr_get 0x%x\n", attr_idx);
    int pos = 0;
    int len_pos = 0;
    switch (attr_idx) {
    case XM_ATTR_IDX_AUDIO_MODE:
        buffer[pos++] = 3;
        pos += fill_attr_idx(attr_idx, &buffer[pos]);
        buffer[pos++] = audio_mode;
        break;
    case XM_ATTR_IDX_KEY_MAPPING:
        buffer[pos++] =  key_mapping_len + 2;
        pos += fill_attr_idx(attr_idx, &buffer[pos]);
        memcpy(&buffer[pos], key_mapping, key_mapping_len);
        pos += key_mapping_len;
        break;
    case XM_ATTR_IDX_AUTO_ANSWER:
        buffer[pos++] = 3;
        pos += fill_attr_idx(attr_idx, &buffer[pos]);
        buffer[pos++] = XM_AUTO_ANSWER_ON;
        break;
    case XM_ATTR_IDX_MULTI_POINTS:
        buffer[pos++] = 3;
        pos += fill_attr_idx(attr_idx, &buffer[pos]);
        buffer[pos++] = XM_MULTI_POINTS_NOT_SUPPORT;
        break;
    case XM_ATTR_IDX_COMPACTNESS_CHECK:
        break;
    case XM_ATTR_IDX_COMPACTNESS:
        break;
    case XM_ATTR_IDX_EQ_MODE:
        buffer[pos++] = 3;
        pos += fill_attr_idx(attr_idx, &buffer[pos]);
        buffer[pos++] = eq_mode;
        log_info("get eq mode %d\n", eq_mode);
        break;
    case XM_ATTR_IDX_DEVICE_NAME:
        break;
    case XM_ATTR_IDX_FIND:
        buffer[pos++] = 4;
        pos += fill_attr_idx(attr_idx, &buffer[pos]);
        if ((find_status[0] == 0) && (find_status[1] == 0)) {
            log_info("find all disable !\n");
            buffer[pos++] = 0;
            buffer[pos++] = 3;
        } else if ((find_status[0] == 1) && (find_status[1] == 1)) {
            log_info("find all enable !\n");
            buffer[pos++] = 1;
            buffer[pos++] = 3;
        } else if ((find_status[0] == 1) && (find_status[1] == 0)) {
            log_info("find left enable !\n");
            buffer[pos++] = 1;
            buffer[pos++] = 1;
        } else if ((find_status[0] == 0) && (find_status[1] == 1)) {
            log_info("find right enable !\n");
            buffer[pos++] = 1;
            buffer[pos++] = 2;
        }
        break;
#if 0
    case XM_ATTR_IDX_ANC_MODE_MAPPING:
        buffer[pos++] = 4;
        pos += fill_attr_idx(attr_idx, &buffer[pos]);
        buffer[pos++] = 0x07;
        buffer[pos++] = 0x07;
        break;
    case XM_ATTR_IDX_ANC_LEVEL:
        buffer[pos++] = 4;
        pos += fill_attr_idx(attr_idx, &buffer[pos]);
        buffer[pos++] = anc_mode;
        buffer[pos++] = anc_mode_mode;
        break;
#endif
    case XM_ATTR_IDX_ANTI_LOST:
        buffer[pos++] = 3;
        pos += fill_attr_idx(attr_idx, &buffer[pos]);
        buffer[pos++] = anti_lost_status;
        break;
    case XM_ATTR_IDX_LP_MODE_DETECT:
        buffer[pos++] = 3;
        pos += fill_attr_idx(attr_idx, &buffer[pos]);
        buffer[pos++] = XM_LP_MODE_DETECT_OFF;
        break;
    case XM_ATTR_IDX_APP_NAME:
        break;
    case XM_ATTR_IDX_WIND_NOISE:
        break;
    case XM_ATTR_IDX_SPATIAL_AUDIO_GET:
        buffer[pos++] = 3;
        pos += fill_attr_idx(attr_idx, &buffer[pos]);
        buffer[pos++] = spatial_audio;
        break;

    case XM_ATTR_IDX_SILENCE_OTA:
        buffer[pos++] = 3;
        pos += fill_attr_idx(attr_idx, &buffer[pos]);
        buffer[pos++] = -1;
        break;
    case XM_ATTR_IDX_AUTO_ANC:
        buffer[pos++] = 3;
        pos += fill_attr_idx(attr_idx, &buffer[pos]);
        buffer[pos++] = XM_AUTO_ANC_NOT_SUPPORT;
        break;
    case XM_ATTR_IDX_SN:
        buffer[pos++] = 22;
        pos += fill_attr_idx(attr_idx, &buffer[pos]);
        memcpy(&buffer[pos], test_sn, 20);
        pos += 20;
        break;
    default:
        return 0;
        break;
    }
    if (pos) {
        log_info_hexdump(buffer, pos);
    }
    return pos;
}

int xm_device_config_attr_set(u16 attr_idx, u8 *buffer, int len)
{
    log_info("xm_device_config_attr_set 0x%x %d\n", attr_idx, len);
    log_info_hexdump(buffer, len);
    switch (attr_idx) {
    case XM_ATTR_IDX_AUDIO_MODE:
        audio_mode = buffer[0];
        break;
    case XM_ATTR_IDX_KEY_MAPPING:
        if (buffer[1] != 0xFF) {
            key_mapping[(buffer[0] % 4) * 3 + 1] = buffer[1];
        }
        if (buffer[2] != 0xFF) {
            key_mapping[(buffer[0] % 4) * 3 + 2] = buffer[2];
        }
        return 0;
        break;
    case XM_ATTR_IDX_AUTO_ANSWER:
        break;
    case XM_ATTR_IDX_MULTI_POINTS:
        break;
    case XM_ATTR_IDX_COMPACTNESS_CHECK:
        break;
    case XM_ATTR_IDX_COMPACTNESS:
        break;
    case XM_ATTR_IDX_EQ_MODE:
        eq_mode = buffer[0];
        log_info("set eq mode %d\n", eq_mode);
        break;
    case XM_ATTR_IDX_DEVICE_NAME:
        break;
    case XM_ATTR_IDX_FIND:
        log_info("find:%s side:%d\n", buffer[0] ? "enable" : "disable", buffer[1]);
        if (buffer[1] == 1) {   // left
            find_status[0] = buffer[0];
        } else if (buffer[1] == 2) {    // right
            find_status[1] = buffer[0];
        } else if (buffer[1] == 3) {    // all
            find_status[0] = buffer[0];
            find_status[1] = buffer[0];
        }
        log_info("left:%d right:%d\n", find_status[0], find_status[1]);
        xm_device_config_attr_notify(XM_ATTR_IDX_FIND);
        break;
    case XM_ATTR_IDX_ANC_MODE_MAPPING:
        break;
    case XM_ATTR_IDX_ANC_LEVEL:
        anc_mode = buffer[0];
        anc_mode_mode = buffer[1];
        break;
    case XM_ATTR_IDX_ANTI_LOST:
        break;
    case XM_ATTR_IDX_LP_MODE_DETECT:
        break;
    case XM_ATTR_IDX_APP_NAME:
        break;
    case XM_ATTR_IDX_SPATIAL_AUDIO_SET:
        spatial_audio = buffer[0];
        break;
    case XM_ATTR_IDX_DEVICE_TIME:
        device_time = buffer[0];
        device_time += buffer[1] << 8;
        device_time += buffer[2] << 16;
        device_time += buffer[3] << 24;
        log_info("XM_ATTR_IDX_DEVICE_TIME set %x\n", device_time);
        break;
    default:
        break;
    }
    return 0;
}

extern int XM_notify_device_config(u16 attr_idx);
extern void XM_f2a_report_device_status(void);
int xm_device_config_attr_notify(u16 attr_idx)
{
    return XM_notify_device_config(attr_idx);
}

int xm_device_status_notify(void)
{
    XM_f2a_report_device_status();
    return 0;
}

//////////////////////// OTHERS  /////////////////////

int xm_bt_name_set(u8 *name, u32 length)
{
    puts((const char *)name);
    return 0;
}

///// api set end

bool xm_left_page_scan_status_get(void)
{
    return 1;
}

bool xm_right_page_scan_status_get(void)
{
    return 1;
}

bool xm_left_inquiry_scan_status_get(void)
{
    return 1;
}

bool xm_right_inquiry_scan_status_get(void)
{
    return 1;
}

// 1:left       0:right
bool xm_tws_side_get(void)
{
    bool tws_l_or_r = 0;
#if TCFG_USER_TWS_ENABLE
    if (bt_tws_get_local_channel() == 'L') {
        tws_l_or_r = 1;
    }
#endif
    return tws_l_or_r;
}

// 1:connecting   0:not connecting
bool xm_tws_connecting_status_get(void)
{
    return 0;
}

// 1:connected   0:not connected
bool xm_tws_connected_status_get(void)
{
    return 1;
}

// 0:not leave box   1:leave box
bool xm_left_leave_box_status_get(void)
{
    return 0;
}

// 0:not leave box   1:leave box
bool xm_right_leave_box_status_get(void)
{
    return 0;
}

// 0:not all leave box   1:all leave box
bool xm_all_leave_box_status_get(void)
{
    return (xm_left_leave_box_status_get() && xm_right_leave_box_status_get());
}

// 0:not paired   1:paired
extern u8 *get_mac_memory_by_index(u8 index);
bool xm_edr_paired_status_get(void)
{
    if (get_mac_memory_by_index(1) != NULL) {
        return 1;
    }
    return 0;
}

// 0:not connected   1:connected
bool xm_edr_connencted_status_get(void)
{
    return ((bt_get_curr_channel_state() != 0) ? 1 : 0);
}

// 1:open       0:close
bool xm_box_open_status_get(void)
{
    return 1;
}

u8 xm_left_battery_get(void)
{
    u8 bat;
    u8 sibling_val = get_tws_sibling_bat_persent();
    u8 self_val = get_charge_online_flag() << 7 | get_vbat_percent();

    bat = self_val;
#if TCFG_USER_TWS_ENABLE
    if (tws_api_get_local_channel() == 'R') {
        bat = sibling_val;
    }
#endif
    return bat;
}

u8 xm_right_battery_get(void)
{
    u8 bat;
    u8 sibling_val = get_tws_sibling_bat_persent();
    u8 self_val = get_charge_online_flag() << 7 | get_vbat_percent();

    bat = self_val;
#if TCFG_USER_TWS_ENABLE
    if (tws_api_get_local_channel() == 'L') {
        bat = sibling_val;
    }
#endif
    return bat;
}

u8 xm_box_battery_get(void)
{
    u8 bat = 0xFF;
    u8 sibling_val = get_tws_sibling_bat_persent();
    u8 self_val = get_charge_online_flag() << 7 | get_vbat_percent();
#if TCFG_CHARGESTORE_ENABLE
    if (self_val >> 7 || sibling_val >> 7) { //有任意一直耳机充电时才显示充电仓电量
        bat = chargestore_get_power_level(); //充电仓电量
    }
#endif
    return bat;
}

//0：正常1：降噪2：通透 3:抗风噪模式
u8 xm_anc_mode_get(void)
{
    return 0;
}

int xm_bt_name_get(u8 *name)
{
    return 0;
}

extern const u8 *bt_get_mac_addr();
void xm_get_edr_address(u8 *addr_buf)
{
    //有个格式要对,need swap
#if 0   //TCFG_USER_TWS_ENABLE
    u8 conn_addr[6];
    extern void bt_get_tws_local_addr(u8 * addr);
    bt_get_tws_local_addr(conn_addr);
    reverse_bd_addr(conn_addr, addr_buf);
#else
    reverse_bd_addr(bt_get_mac_addr(), addr_buf);
#endif
    log_info("edr_addr:");
    log_info_hexdump(addr_buf, 6);
}

void xm_edr_last_paired_addr_get(u8 *addr)
{
    memcpy(addr, get_mac_memory_by_index(1), 6);
}


///// api get end

int xm_left_page_scan_status_update(bool status)
{
    xm_adv_update_notify();
    xm_at_cmd_status_update(XM_AT_IDX_ADV_RSP_DATA, NULL, 0);
    return 0;
}

int xm_right_page_scan_status_update(bool status)
{
    xm_adv_update_notify();
    xm_at_cmd_status_update(XM_AT_IDX_ADV_RSP_DATA, NULL, 0);
    return 0;
}

int xm_left_inquiry_scan_status_update(bool status)
{
    xm_adv_update_notify();
    xm_at_cmd_status_update(XM_AT_IDX_ADV_RSP_DATA, NULL, 0);
    return 0;
}

int xm_right_inquiry_scan_status_update(bool status)
{
    xm_adv_update_notify();
    xm_at_cmd_status_update(XM_AT_IDX_ADV_RSP_DATA, NULL, 0);

    return 0;
}

int xm_tws_side_update(bool status)
{
    xm_adv_update_notify();
    xm_at_cmd_status_update(XM_AT_IDX_ADV_RSP_DATA, NULL, 0);
    return 0;
}

int xm_tws_connecting_status_update(bool status)
{
    xm_adv_update_notify();
    xm_at_cmd_status_update(XM_AT_IDX_ADV_RSP_DATA, NULL, 0);
    xm_device_status_notify();
    return 0;
}

int xm_tws_connected_status_update(bool status)
{
    xm_adv_update_notify();
    xm_at_cmd_status_update(XM_AT_IDX_ADV_RSP_DATA, NULL, 0);
    xm_device_status_notify();
    return 0;
}

int xm_left_leave_box_status_update(bool status)
{
    xm_adv_update_notify();
    xm_at_cmd_status_update(XM_AT_IDX_ANTI_LOST, NULL, 0);
    xm_at_cmd_status_update(XM_AT_IDX_ADV_RSP_DATA, NULL, 0);
    xm_device_config_attr_notify(XM_ATTR_IDX_ANTI_LOST);
    return 0;
}

int xm_right_leave_box_status_update(bool status)
{
    xm_adv_update_notify();
    xm_at_cmd_status_update(XM_AT_IDX_ANTI_LOST, NULL, 0);
    xm_at_cmd_status_update(XM_AT_IDX_ADV_RSP_DATA, NULL, 0);
    xm_device_config_attr_notify(XM_ATTR_IDX_ANTI_LOST);
    return 0;
}

int xm_all_leave_box_status_update(bool status)
{
    xm_adv_update_notify();
    xm_at_cmd_status_update(XM_AT_IDX_ADV_RSP_DATA, NULL, 0);
    return 0;
}

int xm_edr_paired_status_update(bool status)
{
    xm_adv_update_notify();
    xm_at_cmd_status_update(XM_AT_IDX_ADV_RSP_DATA, NULL, 0);
    return 0;
}

int xm_edr_connencted_status_update(bool status)
{
    xm_adv_update_notify();
    xm_at_cmd_status_update(XM_AT_IDX_ADV_RSP_DATA, NULL, 0);
    return 0;
}

int xm_box_open_status_update(bool status)
{
    xm_adv_update_notify();
    xm_at_cmd_status_update(XM_AT_IDX_ADV_RSP_DATA, NULL, 0);
    return 0;
}

int xm_left_battery_update(u8 status)
{
    xm_adv_update_notify();
    xm_at_cmd_status_update(XM_AT_IDX_ADV_RSP_DATA, NULL, 0);
    xm_device_status_notify();
    return 0;
}

int xm_right_battery_update(u8 status)
{
    xm_adv_update_notify();
    xm_at_cmd_status_update(XM_AT_IDX_ADV_RSP_DATA, NULL, 0);
    xm_device_status_notify();
    return 0;
}

int xm_box_battery_update(u8 status)
{
    xm_adv_update_notify();
    xm_at_cmd_status_update(XM_AT_IDX_ADV_RSP_DATA, NULL, 0);
    xm_device_status_notify();
    return 0;
}

int xm_anc_mode_update(u8 status)
{
    /* xm_at_cmd_status_update(XM_AT_IDX_DENOISE, NULL, 0); */
    /* xm_device_status_notify(); */
    xm_device_config_attr_notify(XM_ATTR_IDX_ANC_LEVEL);
    return 0;
}

int xm_key_mapping_update(u8 status)
{
    /* xm_at_cmd_status_update(XM_AT_IDX_KEY_MAPPING, NULL, 0); */
    xm_device_config_attr_notify(XM_ATTR_IDX_KEY_MAPPING);
    return 0;
}

int xm_eq_mode_update(u8 status)
{
    /* xm_at_cmd_status_update(XM_AT_IDX_EQUALIZER, NULL, 0); */
    xm_device_config_attr_notify(XM_ATTR_IDX_EQ_MODE);
    return 0;
}

int xm_game_mode_update(u8 status)
{
    /* xm_at_cmd_status_update(XM_AT_IDX_GAME_MODE, NULL, 0); */
    return 0;
}

///// api update end


//////////////////////// DEV INFO ATTR GET  OPCODE:0x02 /////////////////////

typedef enum __XM_ATTR_TYPE {
    XM_ATTR_TYPE_NAME                   = 0,
    XM_ATTR_TYPE_VERSION,
    XM_ATTR_TYPE_BATTERY,
    XM_ATTR_TYPE_VID_AND_PID,
    XM_ATTR_TYPE_EDR_CONNECTION_STATUS,
    XM_ATTR_TYPE_FW_RUN_TYPE            = 5,
    XM_ATTR_TYPE_UBOOT_VERSION,
    XM_ATTR_TYPE_MULT_BATTERY,
    XM_ATTR_TYPE_CODEC_TYPE,             // 0：speex(默认值), 1:opus, 2:no_decode(无需解码)
    XM_ATTR_TYPE_POWER_SAVE,
    XM_ATTR_TYPE_FUNC_KEY              = 10,
    XM_ATTR_TYPE_HOTWORD,
    XM_ATTR_TYPE_CHARSET_TYPE,
    XM_ATTR_TYPE_COLOR,
    XM_ATTR_TYPE_SPATIAL_AUDIO         = 14,
    XM_ATTR_TYPE_VIRTUAL_SURROUND      = 15,
    XM_ATTR_TYPE_MAX,
} XM_ATTR_TYPE;

int xm_device_info_attr_get(u16 attr_idx, u8 *buffer)
{
    log_info("xm_device_info_attr_get 0x%x\n", attr_idx);
    int rlen = 0;
    switch (attr_idx) {
    case XM_ATTR_TYPE_NAME:
        /* rlen = strlen(bt_get_local_name()); */
        /* if (buffer != NULL) { */
        /* memcpy(buffer, bt_get_local_name(), rlen); */
        /* } */
        break;
    case XM_ATTR_TYPE_VERSION:
        rlen = 4;
        if (buffer != NULL) {
            buffer[0] = (XM_FIREWARE_VERSION >> 8) & 0xFF;
            buffer[1] = XM_FIREWARE_VERSION & 0xFF;
            buffer[2] = (XM_FIREWARE_VERSION >> 8) & 0xFF;
            buffer[3] = XM_FIREWARE_VERSION & 0xFF;
        }
        break;
    case XM_ATTR_TYPE_BATTERY:
        break;
    case XM_ATTR_TYPE_VID_AND_PID:
        rlen = 4;
        if (buffer != NULL) {
            buffer[0] = (XIAO_AI_VID >> 8) & 0xFF;
            buffer[1] = XIAO_AI_VID & 0xFF;
            buffer[2] = (XIAO_AI_PID >> 8) & 0xFF;
            buffer[3] = XIAO_AI_PID & 0xFF;
        }
        break;
    case XM_ATTR_TYPE_EDR_CONNECTION_STATUS:
        rlen = 1;
        if (buffer != NULL) {
            buffer[0] = xm_edr_connencted_status_get();
        }
        break;
    case XM_ATTR_TYPE_FW_RUN_TYPE:
        rlen = 1;
        if (buffer != NULL) {
            buffer[0] = 0;
        }
        break;
    case XM_ATTR_TYPE_UBOOT_VERSION:
        rlen = 2;
        if (buffer != NULL) {
            buffer[0] = (XM_FIREWARE_VERSION >> 8) & 0xFF;
            buffer[1] = XM_FIREWARE_VERSION & 0xFF;
        }
        break;
    case XM_ATTR_TYPE_MULT_BATTERY:
        rlen = 3;
        if (buffer != NULL) {
            buffer[0] = xm_left_battery_get();
            buffer[1] = xm_right_battery_get();
            buffer[2] = xm_box_battery_get();
        }
        break;
    case XM_ATTR_TYPE_CODEC_TYPE:
        // 0:speex 1:opus 2:no_decode
        rlen = 1;
        if (buffer != NULL) {
            buffer[0] = 1;
        }
        break;
    case XM_ATTR_TYPE_POWER_SAVE:
        rlen = 1;
        if (buffer != NULL) {
            buffer[0] = 0;
        }
        break;
    case XM_ATTR_TYPE_FUNC_KEY:
        break;
    case XM_ATTR_TYPE_HOTWORD:
        break;
    case XM_ATTR_TYPE_CHARSET_TYPE:
        // 0:utf-8 1:utf-16 2:unicode 3:gbk2312 当前仅支持UTF-8
        rlen = 1;
        if (buffer != NULL) {
            buffer[0] = 0;
        }
        break;
    case XM_ATTR_TYPE_COLOR:
        rlen = 1;
        if (buffer != NULL) {
            buffer[0] = 0x04;
        }
        break;
    case XM_ATTR_TYPE_SPATIAL_AUDIO:
        rlen = 1;
        if (buffer != NULL) {
            buffer[0] = 0x02;
        }
        break;
    case XM_ATTR_TYPE_VIRTUAL_SURROUND:
        break;
    default:
        break;
    }
    return rlen;
}

//////////////////////// DEV RUN INFO ATTR GET  OPCODE:0x09 /////////////////////

//XM_OPCODE_GET_DEVICE_RUN_INFO
enum {
    RUN_INFO_ATTR_GET_EDR_MAC = 0x0,
    RUN_INFO_ATTR_GET_BLE_MAC,
    RUN_INFO_ATTR_GET_MTU,
    RUN_INFO_ATTR_GET_EDR_CONNECT_STATUS,
    RUN_INFO_ATTR_GET_POWER_MODE,
    RUN_INFO_ATTR_GET_VENDOR_DATA,  // 0x5
    RUN_INFO_ATTR_TWS_STATUS,
    RUN_INFO_ATTR_DEVICE_VIRTUAL_ADDRESS,
    RUN_INFO_ATTR_TYPE_DONGLE_STATUS,
    RUN_INFO_ATTR_TYPE_GET_ANC_STATUS,
    RUN_INFO_ATTR_DEVICE_AUTO_PLAY,     // 0xA
    RUN_INFO_ATTR_DEVICE_GAME_MODE,
    RUN_INFO_ATTR_MAX,
};

int xm_device_run_info_attr_get(u16 attr_idx, u8 *buffer)
{
    log_info("xm_device_run_info_attr_get 0x%x\n", attr_idx);
    int rlen = 0;
    switch (attr_idx) {
    case RUN_INFO_ATTR_GET_EDR_CONNECT_STATUS:
        // 获取经典蓝牙状态 0：未连接 1：已连接
        if (buffer != NULL) {
            buffer[0] = xm_edr_connencted_status_get();
        }
        rlen = 1;
        break;
    case RUN_INFO_ATTR_TWS_STATUS:
        // 0：主从连接完成 1：主从连接中 2:主从连接
        if (buffer != NULL) {
            buffer[0] = !xm_tws_connected_status_get();
        }
        rlen = 1;
        break;
    case RUN_INFO_ATTR_TYPE_DONGLE_STATUS:
        // 获取辅助设备与目标设备连接状态
        // 0：连接断开 1：连接中 2：连接完成
        break;
    case RUN_INFO_ATTR_TYPE_GET_ANC_STATUS:
        //获取耳机的主动降噪状态
        //0：正常1：降噪2：通透 3:抗风噪模式
        if (buffer != NULL) {
            buffer[0] = xm_anc_mode_get();
        }
        rlen = 1;
        break;
    case RUN_INFO_ATTR_DEVICE_AUTO_PLAY:
        // 耳机入耳自动续播
        if (buffer != NULL) {
            buffer[0] = 0;
        }
        rlen = 1;
        break;
    case RUN_INFO_ATTR_DEVICE_GAME_MODE:
        // 查询耳机是否开启游戏模式
        if (buffer != NULL) {
            buffer[0] = 0;
        }
        rlen = 1;
        break;
    default:
        break;
    }
    return rlen;
}

//////////////////// GAME MODE ////////////////////

void xm_game_mode_callback(u8 game_mode)
{
    printf("################## game mode %d\n", game_mode);
}

#endif

