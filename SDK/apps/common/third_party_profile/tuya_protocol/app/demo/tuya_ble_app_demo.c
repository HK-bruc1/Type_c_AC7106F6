#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".tuya_ble_app_demo.data.bss")
#pragma data_seg(".tuya_ble_app_demo.data")
#pragma const_seg(".tuya_ble_app_demo.text.const")
#pragma code_seg(".tuya_ble_app_demo.text")
#endif
#include "tuya_ble_app_demo.h"
#include "tuya_ota.h"
#include "app_config.h"

#include "log.h"
#include "system/includes.h"
#include "btstack/avctp_user.h"
#include "user_cfg_id.h"
#include "tone_player.h"

#include "effects/audio_eq.h"
#include "bt_tws.h"
#include "user_cfg.h"
#include "key_event_deal.h"


#include "tuya_ble_api.h"
#include "tuya_ble_main.h"
#include "app_msg.h"
#include "app_tone.h"
#include "app_ble_spp_api.h"

//为了codecheck.sh check加个定义
#ifndef TUYA_BLE_PROTOCOL_VERSION_HIGN
#define TUYA_BLE_PROTOCOL_VERSION_HIGN   0x04
#endif

#if (THIRD_PARTY_PROTOCOLS_SEL&TUYA_DEMO_EN)

extern tuya_ble_parameters_settings_t tuya_ble_current_para;

extern void tuya_one_click_connect_state_set(uint8_t state);
extern void tuya_get_vm_id_register(uint8_t (*handler)(u32 addr));
extern void tuya_one_click_connect_init();
extern u32 sdfile_get_disk_capacity(void);
extern u32 sdfile_flash_addr2cpu_addr(u32 offset);
extern int le_controller_set_mac(void *addr);
extern void *tuya_ble_hdl;

static uint32_t sn = 0;

tuya_ble_device_param_t device_param = {0};

#define TWS_FUNC_ID_TUYA_AUTH_SYNC  TWS_FUNC_ID('T', 'U', 'A', 'U')

#define TUYA_TRIPLE_LENGTH      0x3E

#define TUYA_INFO_TEST 1

#if TUYA_INFO_TEST
static const char device_id_test[DEVICE_ID_LEN] = "tuya52c534229871";
static const char auth_key_test[AUTH_KEY_LEN] = "gqGPQQl4n540dc6sVPoGoh4fO7a8DzED";
static const uint8_t mac_test[6] = {0xDC, 0x23, 0x4E, 0x3E, 0xBD, 0x3D}; //The actual MAC address is : 66:55:44:33:22:11
#endif /* TUYA_INFO_TEST */

#define APP_CUSTOM_EVENT_1  1
#define APP_CUSTOM_EVENT_2  2
#define APP_CUSTOM_EVENT_3  3
#define APP_CUSTOM_EVENT_4  4
#define APP_CUSTOM_EVENT_5  5

#define LIC_PAGE_OFFSET     80

static uint8_t dp_data_array[255 + 3];
static uint16_t dp_data_len = 0;

static __tuya_info tuya_info;
tuya_tws_sync_info_t tuya_tws_sync_info;

static u8 ascii_to_hex(u8 in)
{
    if (in >= '0' && in <= '9') {
        return in - '0';
    } else if (in >= 'a' && in <= 'f') {
        return in - 'a' + 0x0a;
    } else if (in >= 'A' && in <= 'F') {
        return in - 'A' + 0x0a;
    } else {
        printf("tuya ascii to hex error, data:0x%x", in);
        return 0;
    }
}

static void parse_mac_data(u8 *in, u8 *out)
{
    for (int i = 0; i < 6; i++) {
        out[i] = (ascii_to_hex(in[2 * i]) << 4) + ascii_to_hex(in[2 * i + 1]);
    }
}

static u16 tuya_get_one_info(const u8 *in, u8 *out)
{
    int read_len = 0;
    const u8 *p = in;

    while (TUYA_LEGAL_CHAR(*p) && *p != ',') { //read product_uuid
        *out++ = *p++;
        read_len++;
    }
    return read_len;
}

static void tuya_sn_increase(void)
{
    sn += 1;
}

static void tuya_bt_name_indicate()
{
    __tuya_bt_name_data p_dp_data;

    uint8_t name_len = strlen(bt_get_local_name());

    p_dp_data.id = 43;
    p_dp_data.type = TUYA_SEND_DATA_TYPE_DT_RAW;
    p_dp_data.len = U16_TO_LITTLEENDIAN(name_len);
    memcpy(p_dp_data.data, bt_get_local_name(), name_len);

    printf("tuya_bt_name_indicate state:%s", p_dp_data.data);
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x03)
    tuya_ble_dp_data_report(&p_dp_data, 5);
#endif
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x04)
    tuya_ble_dp_data_send(sn, DP_SEND_TYPE_ACTIVE, DP_SEND_FOR_CLOUD_PANEL, DP_SEND_WITH_RESPONSE, (uint8_t *)&p_dp_data, 4 + name_len);
    tuya_sn_increase();
#endif
}

static void tuya_conn_state_indicate()
{
    __tuya_conn_state_data p_dp_data;

    p_dp_data.id = 33;
    p_dp_data.type = TUYA_SEND_DATA_TYPE_DT_ENUM;
    p_dp_data.len = U16_TO_LITTLEENDIAN(1);
    p_dp_data.data = tuya_info.tuya_conn_state;

    printf("tuya_conn_state_indicate state:%d", tuya_info.tuya_conn_state);
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x03)
    tuya_ble_dp_data_report(&p_dp_data, 5);
#endif
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x04)
    tuya_ble_dp_data_send(sn, DP_SEND_TYPE_ACTIVE, DP_SEND_FOR_CLOUD_PANEL, DP_SEND_WITH_RESPONSE, (uint8_t *)&p_dp_data, 5);
    tuya_sn_increase();
#endif
}

static bool license_para_head_check(u8 *para)
{
    _flash_of_lic_para_head *head;

    //fill head
    head = (_flash_of_lic_para_head *)para;

    ///crc check
    u8 *crc_data = (u8 *)(para + sizeof(((_flash_of_lic_para_head *)0)->crc));
    u32 crc_len = sizeof(_flash_of_lic_para_head) - sizeof(((_flash_of_lic_para_head *)0)->crc)/*head crc*/ + (head->string_len)/*content crc,include end character '\0'*/;
    s16 crc_sum = 0;

    crc_sum = CRC16(crc_data, crc_len);

    if (crc_sum != head->crc) {
        printf("license crc error !!! %x %x \n", (u32)crc_sum, (u32)head->crc);
        return false;
    }

    return true;
}

const u8 *tuya_get_license_ptr(void)
{
    u32 flash_capacity = sdfile_get_disk_capacity();
    u32 flash_addr = flash_capacity - 256 + LIC_PAGE_OFFSET;
    u8 *lic_ptr = NULL;
    _flash_of_lic_para_head *head;

    printf("flash capacity:%x \n", flash_capacity);
    lic_ptr = (u8 *)sdfile_flash_addr2cpu_addr(flash_addr);

    //head length check
    head = (_flash_of_lic_para_head *)lic_ptr;
    if (head->string_len >= 0xff) {
        printf("license length error !!! \n");
        return NULL;
    }

    ////crc check
    if (license_para_head_check(lic_ptr) == (false)) {
        printf("license head check fail\n");
        return NULL;
    }

    //put_buf(lic_ptr, 128);

    lic_ptr += sizeof(_flash_of_lic_para_head);
    return lic_ptr;
}

static uint8_t read_tuya_product_info_from_flash(uint8_t *read_buf, u16 buflen)
{
    uint8_t *rp = read_buf;
    const uint8_t *tuya_ptr = (uint8_t *)tuya_get_license_ptr();
    //printf("tuya_ptr:");
    //put_buf(tuya_ptr, 69);

    if (tuya_ptr == NULL) {
        return FALSE;
    }
    int data_len = 0;
    data_len = tuya_get_one_info(tuya_ptr, rp);
    //put_buf(rp, data_len);
    if (data_len != 16) {
        printf("read uuid err, data_len:%d", data_len);
        put_buf(rp, data_len);
        return FALSE;
    }
    tuya_ptr += 17;

    rp = read_buf + 16;

    data_len = tuya_get_one_info(tuya_ptr, rp);
    //put_buf(rp, data_len);
    if (data_len != 32) {
        printf("read key err, data_len:%d", data_len);
        put_buf(rp, data_len);
        return FALSE;
    }
    tuya_ptr += 33;

    rp = read_buf + 16 + 32;
    data_len = tuya_get_one_info(tuya_ptr, rp);
    //put_buf(rp, data_len);
    if (data_len != 12) {
        printf("read mac err, data_len:%d", data_len);
        put_buf(rp, data_len);
        return FALSE;
    }
    return TRUE;
}

void tuya_battry_indicate_case()
{
    __battery_indicate_data p_dp_data;

    p_dp_data.id = 2;
    p_dp_data.type = TUYA_SEND_DATA_TYPE_DT_VALUE;
    p_dp_data.len = U16_TO_LITTLEENDIAN(1);
    p_dp_data.data = tuya_info.battery_info.case_battery;

#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x03)
    tuya_ble_dp_data_report(&p_dp_data, 5);
#endif
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x04)
    tuya_ble_dp_data_send(sn, DP_SEND_TYPE_ACTIVE, DP_SEND_FOR_CLOUD_PANEL, DP_SEND_WITH_RESPONSE, (uint8_t *)&p_dp_data, 5);
    tuya_sn_increase();
#endif
}

void tuya_battry_indicate_left()
{
    __battery_indicate_data p_dp_data;

    p_dp_data.id = 3;
    p_dp_data.type = TUYA_SEND_DATA_TYPE_DT_VALUE;
    p_dp_data.len = U16_TO_LITTLEENDIAN(1);
    p_dp_data.data = tuya_info.battery_info.left_battery;

#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x03)
    tuya_ble_dp_data_report(&p_dp_data, 5);
#endif
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x04)
    tuya_ble_dp_data_send(sn, DP_SEND_TYPE_ACTIVE, DP_SEND_FOR_CLOUD_PANEL, DP_SEND_WITH_RESPONSE, (uint8_t *)&p_dp_data, 5);
    tuya_sn_increase();
#endif
}

void tuya_battry_indicate_right()
{
    __battery_indicate_data p_dp_data;

    p_dp_data.id = 4;
    p_dp_data.type = TUYA_SEND_DATA_TYPE_DT_VALUE;
    p_dp_data.len = U16_TO_LITTLEENDIAN(1);
    p_dp_data.data = tuya_info.battery_info.right_battery;

#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x03)
    tuya_ble_dp_data_report(&p_dp_data, 5);
#endif
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x04)
    tuya_ble_dp_data_send(sn, DP_SEND_TYPE_ACTIVE, DP_SEND_FOR_CLOUD_PANEL, DP_SEND_WITH_RESPONSE, (uint8_t *)&p_dp_data, 5);
    tuya_sn_increase();
#endif
}

void tuya_key_info_indicate()
{
    __key_indicate_data p_dp_data;

    uint8_t key_buf[30];
    uint8_t key_func;
    for (int key_idx = 0; key_idx < 6; key_idx++) {
        p_dp_data.id = key_idx + 19;
        p_dp_data.type = TUYA_SEND_DATA_TYPE_DT_ENUM;
        p_dp_data.len = U16_TO_LITTLEENDIAN(1);
        switch (key_idx) {
        case 0:
            key_func = tuya_info.key_info.left1;
            break;
        case 1:
            key_func = tuya_info.key_info.right1;
            break;
        case 2:
            key_func = tuya_info.key_info.left2;
            break;
        case 3:
            key_func = tuya_info.key_info.right2;
            break;
        case 4:
            key_func = tuya_info.key_info.left3;
            break;
        case 5:
            key_func = tuya_info.key_info.right3;
            break;
        }
        p_dp_data.data = key_func;

        memcpy(&key_buf[5 * key_idx], &p_dp_data, 5);
    }
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x03)
    tuya_ble_dp_data_report(&p_dp_data, 5);
#endif
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x04)
    tuya_ble_dp_data_send(sn, DP_SEND_TYPE_ACTIVE, DP_SEND_FOR_CLOUD_PANEL, DP_SEND_WITH_RESPONSE, key_buf, 30);
    tuya_sn_increase();
#endif
}

void tuya_battery_indicate(u8 left, u8 right, u8 chargebox)
{
    //tuya_led_state_indicate();
    tuya_info.battery_info.left_battery = left;
    tuya_info.battery_info.right_battery = right;
    tuya_info.battery_info.case_battery = chargebox;
    tuya_battry_indicate_right();
    tuya_battry_indicate_left();
    tuya_battry_indicate_case();
}

/* 设备音量数据上报 */
void tuya_valume_indicate(u8 valume)
{
    printf("tuya_valume_indicate, sn:%x", sn);
    __valume_indicate_data p_dp_data;

    p_dp_data.id = 5;
    p_dp_data.type = TUYA_SEND_DATA_TYPE_DT_VALUE;
    p_dp_data.len = U16_TO_LITTLEENDIAN(1);
    p_dp_data.data = valume;

#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x03)
    tuya_ble_dp_data_report(&p_dp_data, 5);
#endif
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x04)
    tuya_ble_dp_data_send(sn, DP_SEND_TYPE_ACTIVE, DP_SEND_FOR_CLOUD_PANEL, DP_SEND_WITH_RESPONSE, (uint8_t *)&p_dp_data, 5);
    tuya_sn_increase();
#endif
}
/* 音乐播放状态上报 */
void tuya_play_status_indicate(u8 status)
{
    printf("tuya_play_status_indicate:%d, sn:%x", status, sn);
    __play_status_indicate_data p_dp_data;

    p_dp_data.id = 7;
    p_dp_data.type = TUYA_SEND_DATA_TYPE_DT_BOOL;
    p_dp_data.len = U16_TO_LITTLEENDIAN(1);
    p_dp_data.data = status;

#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x03)
    tuya_ble_dp_data_report(&p_dp_data, 5);
#endif
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x04)
    tuya_ble_dp_data_send(sn, DP_SEND_TYPE_ACTIVE, DP_SEND_FOR_CLOUD_PANEL, DP_SEND_WITH_RESPONSE, (uint8_t *)&p_dp_data, 5);
    tuya_sn_increase();
#endif
}

void tuya_eq_onoff_indicate(uint8_t status)
{
    printf("tuya_eq_onoff_indicate:%d,sn:%x", status, sn);
    __eq_onoff_indicate_data p_dp_data;

    p_dp_data.id = 44;
    p_dp_data.type = TUYA_SEND_DATA_TYPE_DT_BOOL;
    p_dp_data.len = U16_TO_LITTLEENDIAN(1);
    p_dp_data.data = status;

#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x03)
    tuya_ble_dp_data_report(&p_dp_data, 5);
#endif
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x04)
    tuya_ble_dp_data_send(sn, DP_SEND_TYPE_ACTIVE, DP_SEND_FOR_CLOUD_PANEL, DP_SEND_WITH_RESPONSE, (uint8_t *)&p_dp_data, 5);
    tuya_sn_increase();
#endif
}

void tuya_eq_info_indicate(char *eq_setting)
{
    __eq_indicate_data p_dp_data;
    if (tuya_info.eq_info.eq_onoff == 0) {
        tuya_info.eq_info.eq_onoff = 1;
        tuya_eq_onoff_indicate(tuya_info.eq_info.eq_onoff);
    }
    p_dp_data.id = 18;
    p_dp_data.type = TUYA_SEND_DATA_TYPE_DT_RAW;
    p_dp_data.len = U16_TO_LITTLEENDIAN(13);
    p_dp_data.version = 0x00;
    p_dp_data.eq_num = 0x0a;
    p_dp_data.eq_mode = eq_setting[10];
    for (int i = 0; i < 10; i++) {
        p_dp_data.eq_data[i] = eq_setting[i] + 0xC0;
    }

#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x03)
    tuya_ble_dp_data_report(&p_dp_data, 17);
#endif
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x04)
    tuya_ble_dp_data_send(sn, DP_SEND_TYPE_ACTIVE, DP_SEND_FOR_CLOUD_PANEL, DP_SEND_WITH_RESPONSE, (uint8_t *)&p_dp_data, 17);
    tuya_sn_increase();
#endif
}

// 经典蓝牙连接传1,断开传0
void tuya_conn_state_set_and_indicate(uint8_t state)
{
    if (state == 1) {
        tuya_info.tuya_conn_state = TUYA_CONN_STATE_CONNECTED;
    } else {
        tuya_info.tuya_conn_state = TUYA_CONN_STATE_DISCONNECT;
    }
    tuya_conn_state_indicate();
    tuya_one_click_connect_state_set(state);
}

/********************tuya demo api**********************/
u8 key_table_l[KEY_ACTION_MAX] = {
    APP_MSG_MUSIC_PP,       //短按
    APP_MSG_NULL,    //长按
    APP_MSG_VOL_UP,           //hold
    APP_MSG_NULL,           //长按抬起
    APP_MSG_MUSIC_NEXT,     //双击
    APP_MSG_TWS_START_PAIR,    //三击
};
u8 key_table_r[KEY_ACTION_MAX] = {
    APP_MSG_MUSIC_PP,       //短按
    APP_MSG_NULL,    //长按
    APP_MSG_VOL_UP,           //hold
    APP_MSG_NULL,           //长按抬起
    APP_MSG_MUSIC_NEXT,     //双击
    APP_MSG_TWS_START_PAIR,    //三击
};

void tuya_key_reset_indicate()
{
    tuya_info.key_info.right1 = 0;
    tuya_info.key_info.right2 = 1;
    tuya_info.key_info.right3 = 2;
    tuya_info.key_info.left1 = 0;
    tuya_info.key_info.left2 = 1;
    tuya_info.key_info.left3 = 2;
    tuya_key_info_indicate();
}

extern void anc_mode_switch(u8 mode, u8 tone_play);
void tuya_data_parse(tuya_ble_cb_evt_param_t *event)
{
    uint32_t get_sn = event->dp_received_data.sn;
    printf("tuya_data_parse, p_data:0x%x, len:%d", (int)event->dp_received_data.p_data, event->dp_received_data.data_len);
    put_buf(event->dp_received_data.p_data, event->dp_received_data.data_len);
    uint16_t buf_len = event->dp_received_data.data_len;

    uint8_t dp_id = event->dp_received_data.p_data[0];
    uint8_t type = event->dp_received_data.p_data[1];
    uint16_t data_len = TWO_BYTE_TO_DATA((&event->dp_received_data.p_data[2]));
    uint8_t *data = &event->dp_received_data.p_data[4];
    printf("<--------------  tuya_data_parse  -------------->");
    printf("get_sn = %x, id = %d, type = %d, data_len = %d, data:", get_sn, dp_id, type, data_len);
    u8 key_value_record[2][6] = {0};
    put_buf(data, data_len);
    int value = 0;
    switch (dp_id) {
    case 1:
        //iot播报模式
        printf("tuya iot broadcast set to: %d\n", data[0]);
        break;
    case 5:
        //音量设置
        printf("tuya voice set to :%d\n", data[3]);
        tuya_set_music_volume(data[3]);
        tuya_sync_info_send(&data[3], APP_TWS_TUYA_SYNC_VOLUME);
        break;
    case 6:
        //切换控制
        printf("tuya change_control: %d\n", data[0]);
        tuya_ctrl_music_state(data[0]);
        break;
    case 7:
        //播放/暂停
        printf("tuya play state:%d\n", data[0]);
        /* tuya_post_key_event(TUYA_MUSIC_PP); */
        bt_cmd_prepare(USER_CTRL_AVCTP_OPID_PLAY, 0, NULL);
        break;
    case 8:
        //设置降噪模式
#if TCFG_AUDIO_ANC_ENABLE
        anc_mode_switch(data[0], 1);
#endif
        tuya_info.noise_info.noise_mode = data[0];
        printf("tuya noise_mode: %d\n", tuya_info.noise_info.noise_mode);
        break;
    case 9:
        //设置降噪场景
        tuya_info.noise_info.noise_scenes = data[0];
        printf("tuya noise_scenes:%d\n", tuya_info.noise_info.noise_scenes);
        break;
    case 10:
        //设置通透模式
        tuya_info.noise_info.transparency_scenes = data[0];
        printf("tuya transparency_scenes: %d\n", tuya_info.noise_info.transparency_scenes);
        break;
    case 11:
        //设置降噪强度
        tuya_info.noise_info.noise_set = data[3];
        printf("tuya noise_set: %d\n", tuya_info.noise_info.noise_set);
        break;
    case 12:
        //寻找设备
        printf("tuya find device set:%d\n", data[0]);
        tuya_sync_info_send(&data[0], APP_TWS_TUYA_SYNC_FIND_DEVICE);
        tuya_find_device(data[0]);
        break;
    case 13:
        //设备断连提醒
        printf("tuya device disconnect notify set:%d", data[0]);
        tuya_sync_info_send(&data[0], APP_TWS_TUYA_SYNC_DEVICE_DISCONN_FLAG);
        if (data[0]) {
            play_tone_file(get_tone_files()->bt_disconnect);
        }
        break;
    case 14:
        //设备重连提醒
        printf("tuya device reconnect notify set:%d", data[0]);
        tuya_sync_info_send(&data[0], APP_TWS_TUYA_SYNC_DEVICE_CONN_FLAG);
        if (data[0]) {
            play_tone_file(get_tone_files()->bt_connect);
        }
        break;
    case 15:
        //手机断连提醒
        printf("tuya phone disconnect notify set:%d", data[0]);
        tuya_sync_info_send(&data[0], APP_TWS_TUYA_SYNC_PHONE_DISCONN_FLAG);
        if (data[0]) {
            play_tone_file(get_tone_files()->bt_disconnect);
        }
        break;
    case 16:
        //手机重连提醒
        printf("tuya phone reconnect notify set:%d", data[0]);
        tuya_sync_info_send(&data[0], APP_TWS_TUYA_SYNC_PHONE_CONN_FLAG);
        if (data[0]) {
            play_tone_file(get_tone_files()->bt_connect);
        }
        break;
    case 17:
        //设置eq模式
        printf("tuya eq_mode set:0x%x", data[0]);
        break;
    case 18:
        // 设置eq参数.此处也会设置eq模式
        if (tuya_info.eq_info.eq_onoff == 0) {
            printf("tuya eq_data set fail, eq_onoff is 0!");
            return;
        }
        tuya_eq_info_deal(tuya_info, (char *)data);
        break;
    case 19:
        //左按键1
        key_table_l[0] = tuya_key_event_swith(data[0]);
        tuya_update_vm_key_info(key_value_record);
        value = syscfg_write(TUYA_SYNC_KEY_INFO, key_value_record, sizeof(key_value_record));
        tuya_sync_info_send(&key_table_l[0], APP_TWS_TUYA_SYNC_KEY_L1);
        break;
    case 20:
        //右按键1
        key_table_r[0] = tuya_key_event_swith(data[0]);
        tuya_update_vm_key_info(key_value_record);
        value = syscfg_write(TUYA_SYNC_KEY_INFO, key_value_record, sizeof(key_value_record));
        tuya_sync_info_send(&key_table_r[0], APP_TWS_TUYA_SYNC_KEY_R1);
        break;
    case 21:
        //左按键2
        key_table_l[2] = tuya_key_event_swith(data[0]);
        tuya_update_vm_key_info(key_value_record);
        value = syscfg_write(TUYA_SYNC_KEY_INFO, key_value_record, sizeof(key_value_record));
        tuya_sync_info_send(&key_table_l[2], APP_TWS_TUYA_SYNC_KEY_L2);
        break;
    case 22:
        //右按键2
        key_table_r[2] = tuya_key_event_swith(data[0]);
        tuya_update_vm_key_info(key_value_record);
        value = syscfg_write(TUYA_SYNC_KEY_INFO, key_value_record, sizeof(key_value_record));
        tuya_sync_info_send(&key_table_r[2], APP_TWS_TUYA_SYNC_KEY_R2);
        break;
    case 23:
        //左按键3
        key_table_l[4] = tuya_key_event_swith(data[0]);
        tuya_update_vm_key_info(key_value_record);
        value = syscfg_write(TUYA_SYNC_KEY_INFO, key_value_record, sizeof(key_value_record));
        tuya_sync_info_send(&key_table_l[4], APP_TWS_TUYA_SYNC_KEY_L3);
        break;
    case 24:
        //右按键3
        key_table_r[4] = tuya_key_event_swith(data[0]);
        tuya_update_vm_key_info(key_value_record);
        value = syscfg_write(TUYA_SYNC_KEY_INFO, key_value_record, sizeof(key_value_record));
        tuya_sync_info_send(&key_table_r[4], APP_TWS_TUYA_SYNC_KEY_R3);
        break;
    case 25:
        //按键重置
        printf("tuya key reset, sn:%x", sn);
        tuya_reset_key_info();
        for (int i = 0; i < 6; i++) {
            key_value_record[0][i] = key_table_l[i];
            key_value_record[1][i] = key_table_r[i];
        }
        syscfg_write(TUYA_SYNC_KEY_INFO, key_value_record, sizeof(key_value_record));
        tuya_sync_info_send(NULL, APP_TWS_TUYA_SYNC_KEY_RESET);
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x03)
        tuya_ble_dp_data_report(data, data_len); //1
#endif
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x04)
        tuya_ble_dp_data_send(sn, DP_SEND_TYPE_ACTIVE, DP_SEND_FOR_CLOUD_PANEL, DP_SEND_WITH_RESPONSE, data, data_len);
#endif
        tuya_key_reset_indicate();
        return;
    case 26:
        //入耳检测
        printf("tuya inear_detection set:%d\n", data[0]);
        break;
    case 27:
        //贴合度检测
        printf("tuya fit_detection set:%d\n", data[0]);
        break;
    case 31:
        //倒计时
        u32 count_time = FOUR_BYTE_TO_DATA(data);
        printf("tuya count down:%d\n", count_time);
        break;
    case 43:
        //蓝牙名字
        tuya_bt_name_deal(data, data_len);
        break;
    case 44:
        // 设置eq开关
        tuya_eq_info_reset(tuya_info, data);
        printf("tuya eq_onoff set:%d\n", tuya_info.eq_info.eq_onoff);
        break;
    case 45:
        //设置通透强度
        tuya_info.noise_info.trn_set = data[3];
        printf("tuya trn_set:%d\n", tuya_info.noise_info.trn_set);
        break;
    default:
        printf("unknow control msg len = %d\n, data:", data_len);
        break;
    }
    if (value != 0) {
        printf("tuya syscfg_write error = %d, please check\n", value);
    }
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x03)
    tuya_ble_dp_data_report(event->dp_received_data.p_data, data_len + 4); //1
#endif
#if (TUYA_BLE_PROTOCOL_VERSION_HIGN == 0x04)
    tuya_ble_dp_data_send(sn, DP_SEND_TYPE_ACTIVE, DP_SEND_FOR_CLOUD_PANEL, DP_SEND_WITH_RESPONSE, event->dp_received_data.p_data, data_len + 4);
#endif
}

void tuya_tws_bind_info_sync()
{
    printf("tuya_tws_bind_info_sync");
    if (tws_api_get_role() == TWS_ROLE_MASTER) {
        memcpy(tuya_tws_sync_info.login_key, tuya_ble_current_para.sys_settings.login_key, LOGIN_KEY_LEN);
        memcpy(tuya_tws_sync_info.device_virtual_id, tuya_ble_current_para.sys_settings.device_virtual_id, DEVICE_VIRTUAL_ID_LEN);
        tuya_tws_sync_info.bound_flag = tuya_ble_current_para.sys_settings.bound_flag;

        tws_api_send_data_to_sibling(&tuya_tws_sync_info, sizeof(tuya_tws_sync_info), TWS_FUNC_ID_TUYA_AUTH_SYNC);
    } else {
        printf("slaver don't sync pair info");
    }
}

void tuya_info_indicate()
{
    tuya_info.eq_info.eq_onoff = 1;
    tuya_eq_onoff_indicate(1);
    if (bt_a2dp_get_status() == 1) {
        tuya_play_status_indicate(1);
    } else {
        tuya_play_status_indicate(0);
    }
    tuya_conn_state_indicate();
    tuya_bt_name_indicate();
}

void tuya_app_cb_handler(tuya_ble_cb_evt_param_t *event)
{
    printf("tuya_app_cb_handler, evt:0x%x, task:%s\n", event->evt, os_current_task());
    int16_t result = 0;
    switch (event->evt) {
    case TUYA_BLE_CB_EVT_CONNECTE_STATUS:
        printf("received tuya ble conncet status update event,current connect status = %d", event->connect_status);
        break;
    case TUYA_BLE_CB_EVT_DP_DATA_RECEIVED:
        tuya_data_parse(event);
        break;
    case TUYA_BLE_CB_EVT_DP_DATA_REPORT_RESPONSE:
        printf("received dp data report response result code =%d", event->dp_response_data.status);
        break;
    case TUYA_BLE_CB_EVT_DP_DATA_WTTH_TIME_REPORT_RESPONSE:
        printf("received dp data report response result code =%d", event->dp_response_data.status);
        break;
    case TUYA_BLE_CB_EVT_DP_DATA_WITH_FLAG_REPORT_RESPONSE:
        printf("received dp data with flag report response sn = %d , flag = %d , result code =%d", event->dp_with_flag_response_data.sn, event->dp_with_flag_response_data.mode
               , event->dp_with_flag_response_data.status);
        break;
    case TUYA_BLE_CB_EVT_DP_DATA_WITH_FLAG_AND_TIME_REPORT_RESPONSE:
        printf("received dp data with flag and time report response sn = %d , flag = %d , result code =%d", event->dp_with_flag_and_time_response_data.sn,
               event->dp_with_flag_and_time_response_data.mode, event->dp_with_flag_and_time_response_data.status);
        break;
    case TUYA_BLE_CB_EVT_UNBOUND:
        printf("received unbound req");
        break;
    case TUYA_BLE_CB_EVT_ANOMALY_UNBOUND:
        printf("received anomaly unbound req");
        break;
    case TUYA_BLE_CB_EVT_DEVICE_RESET:
        printf("received device reset req");
        break;
    case TUYA_BLE_CB_EVT_DP_QUERY:
        printf("received TUYA_BLE_CB_EVT_DP_QUERY event");
        //printf("dp_query len:", event->dp_query_data.data_len);
        put_buf(event->dp_query_data.p_data, event->dp_query_data.data_len);
        tuya_info_indicate();
        break;
    case TUYA_BLE_CB_EVT_OTA_DATA:
        tuya_ota_proc(event->ota_data.type, event->ota_data.p_data, event->ota_data.data_len);
        break;
    case TUYA_BLE_CB_EVT_NETWORK_INFO:
        printf("received net info : %s", event->network_data.p_data);
        tuya_ble_net_config_response(result);
        break;
    case TUYA_BLE_CB_EVT_WIFI_SSID:
        break;
    case TUYA_BLE_CB_EVT_TIME_STAMP:
        printf("received unix timestamp : %s ,time_zone : %d", event->timestamp_data.timestamp_string, event->timestamp_data.time_zone);
        break;
    case TUYA_BLE_CB_EVT_TIME_NORMAL:
        break;
    case TUYA_BLE_CB_EVT_DP_DATA_SEND_RESPONSE:
        printf("TUYA_BLE_CB_EVT_DP_DATA_SEND_RESPONSE, sn:%d, type:0x%x, mode:0x%x, ack:0x%x, status:0x%x", event->dp_send_response_data.sn, event->dp_send_response_data.type, event->dp_send_response_data.mode, event->dp_send_response_data.ack, event->dp_send_response_data.status);
        break;
    case TUYA_BLE_CB_EVT_DATA_PASSTHROUGH:
        tuya_ble_data_passthrough(event->ble_passthrough_data.p_data, event->ble_passthrough_data.data_len);
        break;
    case TUYA_BLE_CB_EVT_UPDATE_LOGIN_KEY_VID:
        printf("TUYA_BLE_CB_EVT_UPDATE_LOGIN_KEY_VID");
        break;
    default:
        printf("tuya_app_cb_handler msg: unknown event type 0x%04x", event->evt);
        break;
    }
    tuya_ble_inter_event_response(event);
}

typedef struct {
    u8  flag;
    u8  data[0x50];
} tuya_triple_info;

tuya_triple_info triple_info;

typedef struct {
    u8  flag;
    u8  data[0x50];
} vm_tuya_triple_info;

vm_tuya_triple_info vm_triple_info;

enum {
    TRIPLE_NULL = 0,
    TRIPLE_FLASH,
    TRIPLE_VM,
};

int flash_ret = 0;
typedef struct {
    u8  data[TUYA_TRIPLE_LENGTH];
} vm_tuya_triple;

extern vm_tuya_triple vm_triple;
int vm_read_result;

static uint8_t app_tuya_get_vm_id(u32 addr)
{
    uint8_t index;
    switch (addr) {
    case TUYA_BLE_AUTH_FLASH_ADDR:
        index = CFG_USER_TUYA_INFO_AUTH;
        break;
    case TUYA_BLE_AUTH_FLASH_BACKUP_ADDR:
        index = CFG_USER_TUYA_INFO_AUTH_BK;
        break;
    case TUYA_BLE_SYS_FLASH_ADDR:
        index = CFG_USER_TUYA_INFO_SYS;
        break;
    case TUYA_BLE_SYS_FLASH_BACKUP_ADDR:
        index = CFG_USER_TUYA_INFO_SYS_BK;
        break;
    default:
        return TUYA_BLE_ERR_INVALID_ADDR;
    }
    return index;
}

void tuya_ble_app_init(void)
{
    device_param.device_id_len = 16;    //If use the license stored by the SDK,initialized to 0, Otherwise 16 or 20.

    int ret = 0;
    tuya_earphone_key_init();

    device_param.use_ext_license_key = 1;
    if (device_param.device_id_len == 16) {
#if TUYA_INFO_TEST
        memcpy(device_param.auth_key, (void *)auth_key_test, AUTH_KEY_LEN);
        memcpy(device_param.device_id, (void *)device_id_test, DEVICE_ID_LEN);
        memcpy(device_param.mac_addr.addr, mac_test, 6);
#else
        uint8_t read_buf[16 + 32 + 12 + 1] = {0};
        flash_ret = read_tuya_product_info_from_flash(read_buf, sizeof(read_buf));
        if (flash_ret == TRUE) {
            uint8_t mac_data[6];
            memcpy(device_param.device_id, read_buf, 16);
            memcpy(device_param.auth_key, read_buf + 16, 32);
            parse_mac_data(read_buf + 16 + 32, mac_data);
            memcpy(device_param.mac_addr.addr, mac_data, 6);
            memcpy((u8 *)&triple_info.data, read_buf, TUYA_TRIPLE_LENGTH);
            triple_info.flag = 1;
        } else {
            triple_info.flag = 2;
            vm_read_result = syscfg_read(VM_TUYA_TRIPLE, (u8 *)&vm_triple, TUYA_TRIPLE_LENGTH);
            printf("vm result: %d\n", vm_read_result);
            printf("read vm data:");
            put_buf((u8 *)&vm_triple, 0x40);
            memcpy((u8 *)&triple_info.data, (u8 *)&vm_triple, TUYA_TRIPLE_LENGTH);
            if (vm_read_result == TUYA_TRIPLE_LENGTH) {
                uint8_t mac_data[6];
                memcpy(device_param.device_id, (u8 *)&vm_triple, 16);
                memcpy(device_param.auth_key, (u8 *)&vm_triple + 16, 32);
                parse_mac_data((u8 *)&vm_triple + 16 + 32, mac_data);
                memcpy(device_param.mac_addr.addr, mac_data, 6);
                printf("read after write vm data:");
                put_buf((u8 *)&vm_triple, 0x40);
            } else  {
                printf("tripe  null");
                put_buf((u8 *)&vm_triple_info, TUYA_TRIPLE_LENGTH);
                triple_info.flag = 0;
            }

        }
#endif /* TUYA_INFO_TEST */
        device_param.mac_addr.addr_type = TUYA_BLE_ADDRESS_TYPE_RANDOM;
    }
    printf("device_id:");
    put_buf(device_param.device_id, 16);
    printf("auth_key:");
    put_buf(device_param.auth_key, 32);
    printf("mac:");
    put_buf(device_param.mac_addr.addr, 6);
    app_ble_set_mac_addr(tuya_ble_hdl, (void *)device_param.mac_addr.addr);

    device_param.p_type = TUYA_BLE_PRODUCT_ID_TYPE_PID;
    device_param.product_id_len = 8;
    memcpy(device_param.product_id, APP_PRODUCT_ID, 8);
    device_param.firmware_version = TY_APP_VER_NUM;
    device_param.hardware_version = TY_HARD_VER_NUM;
    tuya_get_vm_id_register(app_tuya_get_vm_id);
    tuya_one_click_connect_init();
    tuya_ble_sdk_init(&device_param);
    ret = tuya_ble_callback_queue_register(tuya_app_cb_handler);
    y_printf("tuya_ble_callback_queue_register,ret=%d\n", ret);

    //tuya_ota_init();

    //printf("demo project version : "TUYA_BLE_DEMO_VERSION_STR);
    printf("app version : "TY_APP_VER_STR);
}
#if (THIRD_PARTY_PROTOCOLS_SEL & TUYA_DEMO_EN)

int get_triple_info_result()
{
    if (vm_read_result != TUYA_TRIPLE_LENGTH) {
        vm_read_result = 0;
    }

    return flash_ret | vm_read_result;
}


u16 get_triple_data(u8 *data)
{
    if (flash_ret) {
        memcpy(data, (u8 *)&triple_info.data, TUYA_TRIPLE_LENGTH - 2);

    } else if (vm_read_result == TUYA_TRIPLE_LENGTH) {
        memcpy(data, (u8 *)&vm_triple, TUYA_TRIPLE_LENGTH - 2);

    }
    return TUYA_TRIPLE_LENGTH ;
}
extern void tws_conn_send_event(int code, const char *format, ...);
u8 tuya_data[TUYA_TRIPLE_LENGTH] = {0};
void set_triple_info(u8 *data)
{
    printf("--------------------------------------------");
    printf("Tuya triple function is no available now!!!");
    printf("--------------------------------------------");
    memset((u8 *)&tuya_data, 0, 0x3e);
    memcpy((u8 *)&tuya_data, data, TUYA_TRIPLE_LENGTH - 2);
    //tws_conn_send_event(TWS_EVENT_CHANGE_TRIPLE, "111", 0, 1, 0);

}
#endif

#endif
