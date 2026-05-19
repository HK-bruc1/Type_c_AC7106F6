#include "dma_wrapper.h"
#include "dma_ota_wrapper.h"
#include "system/includes.h"
#include "os/os_api.h"
#include "dma_setting.h"
#include "btstack/third_party/app_protocol_event.h"
#include "btstack/avctp_user.h"
#include "system/timer.h"

#ifdef DMA_LIB_CODE_SIZE_CHECK
#pragma bss_seg(	".bt_dma_port_bss")
#pragma data_seg(	".bt_dma_port_data")
#pragma const_seg(	".bt_dma_port_const")
#pragma code_seg(	".bt_dma_port_code")
#endif

#define __this_cfg  (&dueros_global_cfg)
DUER_DMA_OPER g_dma_operation;
DMA_OTA_OPER  g_dma_ota_operation;

//#define DMA_HEAP_POOL_SIZE (4096)
//char dma_heap_pool[DMA_HEAP_POOL_SIZE];
u8 dma_use_lib_only_ble = 0;
u8 dma_role_switch_ble_disconnect_flag = 0;
#define DMA_QUEUE_BUFFER_SIZE (4096)
//uint8_t dma_queue_buffer_pool[DMA_QUEUE_BUFFER_SIZE];
uint8_t *dma_queue_buffer_pool = NULL;
/* osMutexId dma_mutex[DMA_MUTEX_NUM]; */

/* osSemaphoreId dma_sem_id = NULL; */
OS_MUTEX dma_mutex[DMA_MUTEX_NUM];
OS_SEM dma_sem_id;
int platform_system_init(void)
{
    dma_log("%s\n", __func__);

    // 初始化 dma_mutex[DMA_CMD_INPUT_MUTEX_ID];
    // 初始化 dma_mutex[DMA_CMD_OUTPUT_MUTEX_ID];
    // 初始化 dma_mutex[DMA_DATA_MUTEX_ID];
    // 初始化 dma_mutex[DMA_SYNC_DATA_MUTEX_ID];

    // 初始化 dma_heap_pool对应的heap
    // 初始化 dma_sem_id
    dma_queue_buffer_pool = malloc(DMA_QUEUE_BUFFER_SIZE);
    if (dma_queue_buffer_pool == NULL) {
        dma_log("=====why dma queue not enought buf\n");
        return -1;
    }
    os_sem_create(&dma_sem_id, 0);
    for (int i = 0; i < DMA_MUTEX_NUM; i++) {
        os_mutex_create(&dma_mutex[i]);
    }
    return 0;
}
__attribute__((weak)) u8 speak_type = 0;
__attribute__((weak)) u8 dma_ota    = false;
u8 app_ota_support = 1;
extern u8 use_triad_info;
bool platform_get_device_capability(DUER_DMA_CAPABILITY *device_capability)
{
    if (NULL == device_capability) {
        return false;
    }
    dma_log("%s\n", __func__);

    dma_role_switch_ble_disconnect_flag = 0x0;
    // 根据产品定义填充
    if (speak_type) {
        device_capability->device_type  = "SPEAKER";
        device_capability->is_earphone = false;
        device_capability->support_box_battery = false;
    } else {
        device_capability->device_type  = "HEADPHONE";
        device_capability->is_earphone = true;
        device_capability->support_box_battery = true;
    }
    set_dueros_pair_state(0, 0);
    device_capability->manufacturer = "JL";
    device_capability->support_spp  = true;
    device_capability->support_ble  = true;
    device_capability->support_a2dp = true;
    device_capability->support_at_command = true;
    device_capability->support_media = true;
    device_capability->support_sota = dma_ota;
    device_capability->support_dual_record = false;
    device_capability->support_local_voice_wake = false;
    device_capability->support_model_beamforming_asr = false;
    device_capability->support_log = false;
    device_capability->ble_role_switch_by_mobile = true;
    device_capability->support_tap_wakeup = true;
    device_capability->support_sign_method = DMA_SIGN_METHOD__SHA256;
    device_capability->support_local_wakeup_prompt_tone = true;
    device_capability->support_battery = true;

    return true;
}

extern u32 dma_software_version;
bool platform_get_firmeware_version(uint8_t *fw_rev_0, uint8_t *fw_rev_1, uint8_t *fw_rev_2, uint8_t *fw_rev_3)
{
    dma_log("%s-%d\n", __func__, dma_software_version);
    *fw_rev_3 = dma_software_version         & 0x000000ff;
    *fw_rev_2 = (dma_software_version >> 8)  & 0x000000ff;
    *fw_rev_1 = (dma_software_version >> 16) & 0x000000ff;
    *fw_rev_0 = (dma_software_version >> 24) & 0x000000ff;
    return true;
}

void platform_heap_free(void *ptr)
{
    //printf("platform_heap_free\n");
    free(ptr);
}

void *platform_heap_malloc(size_t size)
{
    //printf("platform_heap_malloc %d\n",size);
    return malloc(size);
}

bool platform_sem_wait(uint32_t timeout_ms)
{
    //dma_log("%s-%x\n", __func__, timeout_ms);
    os_sem_pend(&dma_sem_id, timeout_ms);
    return true;
}

bool platform_sem_signal()
{
    //dma_log("%s\n", __func__);
    os_sem_post(&dma_sem_id);
    return true;
}

bool platform_mutex_lock(DMA_MUTEX_ID mutex_id)
{
    //dma_log("%s\n", __func__);
    os_mutex_pend(&dma_mutex[mutex_id], 0);
    return true;
}

bool platform_mutex_unlock(DMA_MUTEX_ID mutex_id)
{
    //dma_log("%s\n", __func__);
    os_mutex_post(&dma_mutex[mutex_id]);
    return true;
}

bool platform_get_userdata_config(DMA_USER_DATA_CONFIG *dma_userdata_config)
{
//* @brief DMA协议栈使用的配置信息，保存于Flash
    dma_log("%s\n", __func__);
#if DMA_LIB_DEBUG
    if (dma_userdata_config == NULL) {
        dma_log("%s no buffer\n", __func__);
        return false;
    }
#endif
    //dma_dump(dma_userdata_config, DMA_USER_DATA_CONFIG_LEN);
    dma_message(APP_PROTOCOL_DMA_READ_RAND, (u8 *)dma_userdata_config, DMA_USER_DATA_CONFIG_LEN);
    return true;
}
u16 last_pack_crc = 0;
extern u16 CRC16(const void *ptr, u32 len);
bool platform_set_userdata_config(const DMA_USER_DATA_CONFIG *dma_userdata_config)
{
    u16 current_crc = 0;
    dma_log("%s\n", __func__);
#if DMA_LIB_DEBUG
    if (dma_userdata_config == NULL) {
        dma_log("%s no buffer\n", __func__);
        return false;
    }
#endif
    //dma_dump(dma_userdata_config, DMA_USER_DATA_CONFIG_LEN);
    if (dma_check_tws_is_master()) {
        current_crc = CRC16(dma_userdata_config, DMA_USER_DATA_CONFIG_LEN);
        dma_log("====dma vm data crc %x---%x\n", last_pack_crc, current_crc);
        if (last_pack_crc != current_crc) {
            last_pack_crc = current_crc;
            dma_message(APP_PROTOCOL_DMA_SAVE_RAND, (u8 *)dma_userdata_config, DMA_USER_DATA_CONFIG_LEN);
        }
    }
    return true;
}

void get_random_number(uint8_t *ptr, uint8_t len);
uint32_t platform_rand(void)
{
    uint8_t temp_buf[4];
    get_random_number(temp_buf, 4);
    //dma_log("%s\n", __func__);

    return ((temp_buf[0]) | (temp_buf[1] << 8) | (temp_buf[1] << 16) | (temp_buf[1] << 24));
}
u8 wait_master_ota_init = 0;
u8 dma_enter_ota_flag = 0;
bool platform_get_ota_state(void)
{
    dma_log("%s\n", __func__);
    return dma_enter_ota_flag;
}
extern u8 dma_ota_result;
uint8_t platform_get_upgrade_state(void)
{
    dma_log("%s\n", __func__);
    return /*dma_ota_result*/1;
}

extern u8 *get_cur_connect_phone_mac_addr(void);
int32_t platform_get_mobile_connect_type(void)
{
    dma_log("%s\n", __func__);
    int dev_type;
    u8 *tmp_addr = get_cur_connect_phone_mac_addr();
    if (tmp_addr) {
        dev_type = bt_remote_dev_company_ioctrl(tmp_addr, 0, 0);
    } else {
        return 0;
    }
    if (REMOTE_DEV_IOS == dev_type) {
        return 1;
    }
    return 2;
}

#define ADV_RSP_PACKET_MAX  31
extern u8 dma_adv_data[ADV_RSP_PACKET_MAX];//max is 31
extern u8 dma_adv_data_length;
// scan_rsp 内容
extern u8 dma_scan_rsp_data[ADV_RSP_PACKET_MAX];//max is 31
extern u8 dma_scan_rsp_data_len;

extern u8 dma_ibeacon_data[ADV_RSP_PACKET_MAX];//max is 31
extern u8 dma_ibeacon_data_len ;
extern void dma_ble_ibeacon_adv(u8 sw);
bool platform_set_ble_advertise_data(const char *t_adv_data, uint8_t adv_data_len,
                                     const char *scan_response, uint8_t scan_response_len,
                                     const char *ibeacon_adv_data, uint8_t ibeacon_adv_data_len)
{
    dma_log("%s\n", __func__);
    printf(">>>>>>>>> %s %d %d %d\n", __func__, adv_data_len, scan_response_len, ibeacon_adv_data_len);
#if 0
    if (adv_data_len != 0) {
        puts("dma_adv_data:");
        put_buf((const u8 *)t_adv_data, adv_data_len);
        if (adv_data_len < ADV_RSP_PACKET_MAX) {
            memcpy(dma_adv_data, t_adv_data, adv_data_len);
            if (dma_use_lib_only_ble) {
                dma_adv_data[2] = 0x06;
            }
            dma_adv_data_length = adv_data_len;
        }
    }
    if (scan_response_len != 0) {
        puts("scan_response:");
        put_buf((const u8 *)scan_response, scan_response_len);
        memcpy(dma_scan_rsp_data, scan_response, scan_response_len);
        dma_scan_rsp_data_len = scan_response_len;
    }
#endif
#if 0
    if (ibeacon_adv_data_len != 0) {
        puts("ibeacon_adv_data:");
        put_buf((const u8 *)ibeacon_adv_data, ibeacon_adv_data_len);
        memcpy(dma_ibeacon_data, ibeacon_adv_data, ibeacon_adv_data_len);
        dma_ibeacon_data_len = ibeacon_adv_data_len;
        dma_ble_ibeacon_adv(1);
    } else {
        dma_ibeacon_data_len = 0;
    }
#endif
    //later
    return true;
}

extern void dma_ble_adv_enable(u8 enable);
bool platform_set_ble_advertise_enable(bool on)
{
    dma_log("%s-%d--%d\n", __func__, on, dma_check_tws_is_master());
    if (dma_check_tws_is_master() == 0 || bt_get_esco_coder_busy_flag()
        || (dma_role_switch_ble_disconnect_flag != 0)) {
        dma_ble_adv_enable(0);
        dma_log("not allow dma lib open adv\n");
        return true;
    }
    dma_ble_adv_enable(on);
    return true;
}

extern const u8 *bt_get_mac_addr();
bool platform_get_bt_address(DMA_BT_ADDR_TYPE addr_type, uint8_t *bt_address)
{
    u8 temp_addr[6];
    dma_log("%s\n", __func__);
    if (NULL == bt_address) {
        return false;
    }
    swapX(bt_get_mac_addr(), temp_addr, 6);
    memcpy(bt_address, temp_addr, 6);
    dma_log("platform_get_bt_address---%d\n", addr_type);
    return true;
}

extern const char *bt_get_local_name(void);

bool platform_get_bt_local_name(char *bt_local_name)
{
    //dma_log("%s\n", __func__);
    const char *device_name = NULL;
    int device_name_len = 0;
    if (NULL == bt_local_name) {
        return false;
    }
    device_name = bt_get_local_name();
    device_name_len = strlen(device_name);
    memcpy(bt_local_name, device_name, device_name_len);
    return true;
}

bool platform_get_ble_local_name(char *ble_local_name)
{
    //dma_log("%s\n", __func__);
    if (NULL == ble_local_name) {
        return false;
    }
    return platform_get_bt_local_name(ble_local_name);
}

bool platform_get_mobile_bt_address(uint8_t *bt_address)
{
    dma_log("%s\n", __func__);
    if (NULL == bt_address) {
        dma_log("%s no buf\n", __func__);
        return false;
    }
    if (get_cur_connect_phone_mac_addr()) {
        u8 temp_addr[6];

        //swapX(get_cur_connect_phone_mac_addr(), temp_addr, 6);
        //memcpy(bt_address, temp_addr, 6);
        memcpy(bt_address, get_cur_connect_phone_mac_addr(), 6);
        dma_dump(bt_address, 6);
        return true;
    } else {
        dma_log("%s no connect address\n", __func__);
        return false;
    }
}

extern int get_link_key(u8 *bd_addr, u8 *link_key, u8 id);
bool platform_get_linkkey_exist_state(const uint8_t *bt_address)
{
    u8 link_key[16] = {0};
    dma_log("%s\n", __func__);
    dma_dump((u8 *)bt_address, 6);
    if (get_link_key((u8 *)bt_address, (u8 *)&link_key[0],  0)) {
        dma_log("dma have link key\n");
        return true;
    } else {
        dma_log("dma no link key\n");
        return false;
    }
};
extern char dma_serial_number[];
__attribute__((weak))
int get_vender_special_SN(char *serial_number)
{
    return 0;
}
bool platform_get_serial_number(DMA_SN_TYPE sn_type, uint8_t *serial_number)
{
    u8 temp_addr[6];
    u8 value_1 = 0, value_2 = 0;
    char serial_number_int[33];
    int SN_len = 0;
    SN_len = get_vender_special_SN(serial_number_int);
    if (SN_len > 0) {
        memcpy(serial_number, serial_number_int, SN_len);
        dma_log("%s---%d==%s\n", __func__, sn_type, serial_number);
        return true;
    }
    dma_log("%s---%d==%s\n", __func__, sn_type, dma_serial_number);
    memcpy(serial_number, dma_serial_number, strlen((const char *)dma_serial_number));
    return true;
}

bool platform_get_sco_state(void)
{
    dma_log("%s\n", __func__);
    return bt_get_esco_coder_busy_flag();
}

bool platform_get_reconnect_state(void)
{
    dma_log("%s\n", __func__);
    bool flag = 0;
    flag = (bool)bt_get_auto_connect_state(get_cur_connect_phone_mac_addr());
    return flag;
}

bool platform_set_voice_mic_stream_state(DMA_VOICE_STREAM_CTRL_TYPE cmd, DMA_VOICE_STREAM_CODEC_TYPE codec_type)
{
    dma_log("set_voice_mic_stream---%d-%d\n", cmd, codec_type);
    if (DMA_VOICE_STREAM_START == cmd) {
        dma_message(APP_PROTOCOL_SPEECH_START, NULL, 0);
    } else {
        dma_message(APP_PROTOCOL_SPEECH_STOP, NULL, 0);
    }
    return true;
}

u8 dma_stream_upload_enable = 1;
bool platform_set_stream_upload_enable(bool on)
{
    dma_log("%s---%d\n", __func__, on);
    dma_stream_upload_enable = on;
    return true;
}

bool platform_get_stream_upload_state(void)
{
    dma_log("%s\n", __func__);
    return dma_stream_upload_enable;
}

extern u16 global_local_ble_mtu;
bool platform_get_mobile_mtu(uint32_t *mtu)
{
    if (mtu == NULL) {
        return false;
    }
    if (bt_get_curr_channel_state() & SPP_CH) {
        *mtu = 500;
    } else {
        *mtu = global_local_ble_mtu;
    }
    dma_log("%s-%d\n", __func__, *mtu);
    return true;
}

bool platform_get_peer_mtu(uint32_t *mtu)
{
    dma_log("%s\n", __func__);
    if (mtu == NULL) {
        return false;
    }
    *mtu = 320;
    return true;
}

bool platform_set_wakeup_enable(bool on)
{
    dma_log("platform_set_wakeup_enable %d\n", on);
    return true;
}

extern int sha256Compute(const void *data, int length, u8 *digest);
bool platform_get_check_summary(const void *input_data, uint32_t len, uint8_t *output_string)
{
    dma_log("%s\n", __func__);
    sha256Compute(input_data, len, (u8 *)output_string);
    set_dueros_pair_state(2, 0);
    return true;
}

bool platform_get_prepare_state(void)
{
    // 获取当前是否处于可发送DMA数据状态
    //dma_log("%s--%d\n", __func__, check_can_send_data_or_not());
    return check_can_send_data_or_not();
}

bool platform_process_cmd(DMA_OPERATION_CMD cmd, void *param_buf, uint32_t  param_size)
{
    dma_log("platform_process_cmd======%d-%d\n", cmd, param_size);
    switch (cmd) {
    case DMA_OPERATION_NO_CMD:
        break;
    case DMA_AUDIO_GET_PLAY_PAUSE:
        if (param_buf == NULL) {
            break;
        }
        if (1 == bt_a2dp_get_status()) {
            *(u8 *)param_buf = true;
        } else {
            *(u8 *)param_buf = false;
        }
        break;
    case DMA_AUDIO_PLAY:
        bt_cmd_prepare(USER_CTRL_AVCTP_OPID_PLAY, 0, NULL);
        break;
    case DMA_AUDIO_PAUSE:
        bt_cmd_prepare(USER_CTRL_AVCTP_OPID_PAUSE, 0, NULL);
        break;
    case DMA_AUDIO_PLAY_BACKWARD:
        bt_cmd_prepare(USER_CTRL_AVCTP_OPID_PREV, 0, NULL);
        break;
    case DMA_AUDIO_PLAY_FORWARD:
        bt_cmd_prepare(USER_CTRL_AVCTP_OPID_NEXT, 0, NULL);
        break;
    case DMA_SEND_CMD:
        dueros_sent_raw_cmd(param_buf, param_size);
        break;
    case DMA_SEND_DATA:
        dueros_sent_raw_audio_data(param_buf, param_size);
        break;
    case DMA_SEND_ATCMD:
        //put_buf(param_buf, param_size);
        bt_cmd_prepare(USER_CTRL_HFP_SEND_USER_AT_CMD, param_size, param_buf);
        break;
    case DMA_SEND_VOICE_COMMAND:
        bt_cmd_prepare(USER_CTRL_HFP_GET_SIRI_OPEN, 0, NULL);
        break;
    case DMA_DISCONNECT_MOBILE:
        break;
    case DMA_CONNECT_MOBILE:
        break;
    default:
        break;
    }
    return true;
}

bool platform_get_peer_connect_state(void)
{
    dma_log("%s\n", __func__);
    if (dma_check_some_status(CHECK_STATUS_TWS_PAIR_STA)) {
        return true;
    } else {
        return false;
    }
}

bool platform_send_custom_info_to_peer(uint8_t *param_buf, uint16_t param_size)
{
    dma_log("%s\n", __func__);
    dma_tws_peer_send(DMA_LIB_DATA_TYPE_DMA, param_buf, param_size);
    return true;
}

bool platform_get_tws_role(DMA_TWS_ROLE_TYPE *role_type)
{
    if (NULL == role_type) {
        return false;
    }
    if (dma_check_tws_is_master()) {
        *role_type = DMA_TWS_MASTER;
    } else {
        *role_type = DMA_TWS_SLAVE;
    }
    dma_log("%s %d---%d\n", __func__, *role_type, dma_check_tws_is_master());
    return true;
}
bool platform_get_tws_side(DMA_TWS_SIDE_TYPE *side_type)
{
    int side_info = dma_check_some_status(CHECK_STATUS_TWS_SIDE);
    if (side_info == 2) {
        *side_type = DMA_SIDE_RIGHT;
    } else if (side_info == 1) {
        *side_type = DMA_SIDE_LEFT;
    } else {
        *side_type = DMA_SIDE_RIGHT;
    }
    dma_log("%s %d\n", __func__, *side_type);
    return true;
}

bool platform_set_role_switch_enable(bool on)
{
    dma_log("%s\n", __func__);
    return true;
}

bool platform_get_box_state(DMA_BOX_STATE_TYPE *box_state)
{
    dma_log("%s\n", __func__);
    *box_state = DMA_BOX_STATE_OPEN;
    return true;
}

bool platform_get_wearing_state(void)
{
    dma_log("%s\n", __func__);
    return true;
}

static bool (*dma_get_battery_value)(u8 battery_type, u8 *value) = NULL;
void dma_get_battery_callback_register(bool (*handler)(u8 battery_type, u8 *value))
{
    dma_get_battery_value = handler;
}
u8 dma_get_bat_type_value(u8 type)
{
    u8 battery_value[APP_PROTOCOL_BAT_T_MAX] = {0};
    if (dma_get_battery_value) {
        dma_get_battery_value(BIT(type), battery_value);
    }
    dma_log("dma battery :%d %d\n", type, battery_value[type]);
    return battery_value[type];
}
uint8_t platform_get_battery_level(void)
{
    dma_log("%s\n", __func__);
    int flag = 0;;
    DMA_TWS_SIDE_TYPE tws_side = 0;
    platform_get_tws_side(&tws_side);
    if (tws_side == DMA_SIDE_LEFT) {
        flag = APP_PROTOCOL_BAT_T_TWS_LEFT;
    } else {
        flag = APP_PROTOCOL_BAT_T_TWS_RIGHT;
    }
    return dma_get_bat_type_value(flag);
}

uint8_t platform_get_box_battery_level(void)
{
    dma_log("%s\n", __func__);
    return dma_get_bat_type_value(APP_PROTOCOL_BAT_T_BOX);
}
bool platform_get_triad_info(char *triad_id, char *triad_secret)
{
    dma_log("%s\n", __func__);
    if (!use_triad_info) {
        return false;
    }
    u8 copy_len = 0;
    copy_len = strlen(__this_cfg->dma_cfg_file_triad_id);
    memcpy(triad_id, (uint8_t *)__this_cfg->dma_cfg_file_triad_id, copy_len);
    copy_len = strlen(__this_cfg->dma_cfg_file_secret);
    memcpy(triad_secret, (uint8_t *)__this_cfg->dma_cfg_file_secret, copy_len);
    return true;
}

bool platform_play_local_tts(DMA_TTS_ID tts_id, bool both_side)
{
    u8 temp_tts_id = (u8)tts_id;
    dma_log("%s---tts_id:%d--both-side-%d\n", __func__, temp_tts_id, both_side);

    dma_message(APP_PROTOCOL_DMA_TTS_TYPE, &temp_tts_id, 1);

    return true;
}

bool platform_get_device_name(char *device_name)
{
    dma_log("%s\n", __func__);

    const char *local_name = NULL;
    int local_name_len = 0;
    if (NULL == device_name) {
        return false;
    }
    local_name = bt_get_local_name();
    local_name_len = strlen(device_name);
    if (local_name_len > 31) {
        local_name_len = 31;
    }
    memset(device_name, 0, 32);
    memcpy(device_name, local_name, local_name_len);
    return true;
}

bool platform_hfp_call_hangup(void)
{
    dma_log("%s\n", __func__);
    return true;
}

int platform_get_call_phone_number(char *number_list)
{
    dma_log("%s\n", __func__);
    return true;
}

bool platform_get_wakeup_enable_default(void)
{
    dma_log("%s\n", __func__);
    return true;
}

bool platform_dma_configure(void)
{
    if (platform_system_init() < 0) {
        return false;
    }

    g_dma_operation.get_device_capability = platform_get_device_capability;
    g_dma_operation.get_firmeware_version = platform_get_firmeware_version;
    g_dma_operation.dma_heap_malloc = platform_heap_malloc;
    g_dma_operation.dma_heap_free   = platform_heap_free;
    g_dma_operation.dma_sem_wait   = platform_sem_wait;
    g_dma_operation.dma_sem_signal = platform_sem_signal;
    g_dma_operation.dma_mutex_lock = platform_mutex_lock;
    g_dma_operation.dma_mutex_unlock = (void *)platform_mutex_unlock;
    g_dma_operation.dma_get_userdata_config = platform_get_userdata_config;
    g_dma_operation.dma_set_userdata_config = platform_set_userdata_config;
    g_dma_operation.dma_rand = platform_rand;
    g_dma_operation.get_ota_state = platform_get_ota_state;
    g_dma_operation.get_upgrade_state = platform_get_upgrade_state;
    g_dma_operation.get_mobile_bt_address = platform_get_mobile_bt_address;
    g_dma_operation.get_mobile_connect_type = platform_get_mobile_connect_type;
    g_dma_operation.set_ble_advertise_data = platform_set_ble_advertise_data;
    g_dma_operation.set_ble_advertise_enable = platform_set_ble_advertise_enable;
    g_dma_operation.get_bt_address = platform_get_bt_address;
    g_dma_operation.get_bt_local_name = platform_get_bt_local_name;
    g_dma_operation.get_ble_local_name = platform_get_ble_local_name;
    g_dma_operation.get_linkkey_exist_state = platform_get_linkkey_exist_state;
    g_dma_operation.get_serial_number = platform_get_serial_number;
    g_dma_operation.get_sco_state = platform_get_sco_state;
    g_dma_operation.get_reconnect_state = platform_get_reconnect_state;
    g_dma_operation.set_voice_mic_stream_state = platform_set_voice_mic_stream_state;
    g_dma_operation.set_stream_upload_enable = platform_set_stream_upload_enable;
    g_dma_operation.get_stream_upload_state = platform_get_stream_upload_state;
    g_dma_operation.get_mobile_mtu = platform_get_mobile_mtu;
    g_dma_operation.get_peer_mtu = platform_get_peer_mtu;
    g_dma_operation.set_wakeup_enable = platform_set_wakeup_enable;
    g_dma_operation.get_check_summary = platform_get_check_summary;
    g_dma_operation.get_prepare_state = platform_get_prepare_state;
    g_dma_operation.dma_process_cmd = platform_process_cmd;
    g_dma_operation.get_peer_connect_state = platform_get_peer_connect_state;
    g_dma_operation.send_custom_info_to_peer = platform_send_custom_info_to_peer;
    g_dma_operation.get_tws_role = platform_get_tws_role;
    g_dma_operation.get_tws_side = platform_get_tws_side;
    g_dma_operation.set_role_switch_enable = platform_set_role_switch_enable;
    g_dma_operation.get_box_state = platform_get_box_state;
    g_dma_operation.get_wearing_state = platform_get_wearing_state;
    g_dma_operation.get_battery_level = platform_get_battery_level;
    g_dma_operation.get_box_battery_level = platform_get_box_battery_level;
    g_dma_operation.get_triad_info = platform_get_triad_info;
    g_dma_operation.play_local_tts = platform_play_local_tts;
    g_dma_operation.get_device_name = platform_get_device_name;
    g_dma_operation.hfp_call_hangup = platform_hfp_call_hangup;
    g_dma_operation.get_call_phone_number = platform_get_call_phone_number;
    g_dma_operation.get_wakeup_enable_default = platform_get_wakeup_enable_default;
    return true;
}
extern u8 dma_ota_connect_flag;
#if DMA_LIB_OTA_EN
/**
 * @brief DMA OTA ms毫秒之后调用回调函数
 *
 * @param[in] cb      回调函数
 * @param[in] data    调用回调函数时传给回调函数的参数
 * @param[in] ms      多少毫秒后调用此函数
 *
 * @return 返回说明
 *        -true 创建timer成功
 *        -false 创建timer失败
 * @note ms毫秒之后调用cb回调函数，参数传data
 */
bool platform_ota_callback_by_timeout(dma_ota_timeout_cb cb, void *data, uint32_t ms)
{
    dma_log("%s---%x---%d\n", __func__, data, ms);
    int ota_timer = sys_timeout_add(data, cb, ms);
    return true;
}
/**
* @brief 发送数据给手机
* @param[in] size    发送数据的大小
* @param[in] data    发送的数据
* @note OTA SDK与手机之间传输数据，只有主耳机调用 \n
*/
extern int dueros_ota_sent_raw_cmd(uint8_t *data,  uint32_t len);
void platform_ota_send_data_to_mobile(uint16_t size, uint8_t *data)
{
    dma_log("%s\n", __func__);
    dma_dump(data, size);
    dueros_ota_sent_raw_cmd(data, size);

}

/**
* @brief 存储OTA相关的数据到flash，必须同步写入
* @param[in] size    写入数据的大小
* @param[in] data    写入的数据
* @note OTA过程中存储OTA相关的数据
*/
void platform_ota_save_ota_info(uint16_t size, const uint8_t *data)
{
    dma_log("%s\n", __func__);
    if (dma_check_tws_is_master()) {
        dma_message(APP_PROTOCOL_DMA_SAVE_OTA_INFO, data, size);
    }
}
/**
* @brief 从flash读取OTA SDK存储的数据
* @param[in] size    要读取数据的大小，等于写入数据的大小
* @param[in] buffer  存放数据的buffer
* @note 读取OTA过程中存储的OTA相关数据
*/
void platform_ota_read_ota_info(uint16_t size, uint8_t *buffer)
{
    dma_log("%s\n", __func__);
    dma_message(APP_PROTOCOL_DMA_READ_OTA_INFO, buffer, size);
}

/**
* @brief 获取缓存Image数据的的buffer
* @param[out] buffer      buffer的地址
* @param[in] buffer       需要的buffer大小(会传入于flash sector大小4096Bytes)
* @return 返回说明
*        实际buffer大小(最好等于flash sector大小)
* @note 用于缓存收到的image数据，buffer满后调用write_image_data_to_flash回调一次写入到flash
*/
u8 *update_data_buf = NULL;
uint32_t platform_ota_get_flash_data_buffer(uint8_t **buffer, uint32_t size)
{
    dma_log("%s\n", __func__);
    if (update_data_buf == NULL) {
        update_data_buf = malloc(size);
        *buffer = update_data_buf;
    } else {
        dma_log("%s=====why not null\n", __func__);
        while (1);
    }
    return size;
}

/**
* @brief 写入OTA Image数据到flash
* @param[in] size           写入数据的大小
* @param[in] data           写入的数据
* @param[in] image_offset   从此image offset处开始写入
* @param[in] sync           数据是否同步写入flash
* @note OTA Image数据写入到flash
*       get_flash_data_buffer 的buffer满了会写入
*       image_offset不一定是累加的，可能会从较小的image_offset重新写入数据
*       若get_flash_data_buffer是flash sector 4KB大小
*           此接口的image_offset会是4KB对齐
*           每次写入大小会是4KB
*/
__attribute__((weak))
void platform_ota_write_image_data_to_flash(uint16_t size, uint8_t *data, uint32_t image_offset, bool sync)
{
    //dma_log("%s\n", __func__);
    putchar('@');
}

/**
* @brief 从flash内读取OTA Image数据
* @param[in] size           要读取数据的大小
* @param[in] buffer         存放数据的buffer
* @param[in] image_offset   从此image offset处开始读取
* @note 从flash内读取OTA Image数据
*/

u8 crc_os_time_dly_flag = 0;
__attribute__((weak))
void platform_ota_read_image_data_from_flash(uint16_t size, uint8_t *buffer, uint32_t image_offset)
{
    static cnt = 0;
    //dma_log("%s-size-%d-img_offset-%d\n", __func__, size, image_offset);
    putchar('#');
    if (crc_os_time_dly_flag) {
        if (++cnt == 10) {
            cnt = 0;
            r_printf("delay");
            os_time_dly(10);
        }
    }
}

/**
* @brief 从flash内擦除OTA Image数据
* @param[in] size           要擦除数据的大小
* @param[in] image_offset   从此image offset处开始擦除
* @note 从flash内擦除OTA Image数据
*           擦除时4KB对齐的，DMA_OTA_FLASH_SECTOR_SIZE 的倍数
*           size是 DMA_OTA_FLASH_SECTOR_SIZE 的倍数(image的最后一部分不是)
*           image_offset 是 DMA_OTA_FLASH_SECTOR_SIZE 的倍数
*/
__attribute__((weak))
void platform_ota_erase_image_data_from_flash(uint16_t size, uint32_t image_offset)
{
    dma_log("%s-size-%d-img_offset-%d\n", __func__, size, image_offset);
}

/**
* @brief 进入OTA模式
* @note 开始OTA后会回调此函数
*/
__attribute__((weak))
void app_protocol_enter_ota_state(void)
{
    dma_message(APP_PROTOCOL_OTA_BEGIN, NULL, 0);
}
__attribute__((weak))
void app_protocol_exit_ota_state(void)
{

}
void platform_ota_enter_ota_state(void)
{
    dma_enter_ota_flag = 1;
    app_protocol_enter_ota_state();
    dma_log("%s\n", __func__);
}
__attribute__((weak))
void platform_ota_set_new_image_size(u32 total_image_size)
{
    dma_log("%s==%d\n", __func__, total_image_size);
}

/**
* @brief 退出OTA模式
* @param[in] error      是否异常退出
*           -true   异常退出
*           -false  正常退出
* @note OTA异常退出或者正常结束后会回调此函数
*/
void disconnect_free_ota_buffer(u8 flag);
void platform_ota_exit_ota_state(bool error)
{
    dma_enter_ota_flag = 0;
    dma_log("%s--flag-%d\n", __func__, error);
    disconnect_free_ota_buffer(0);
}

/**
* @brief OTA数据传输完成
* @param[in] total_image_size      固件总大小
* @return 返回说明
*           -true Image固件没有问题
*           -false Image固件有问题
* @note OTA数据传输完成后回调此函数，可以在此函数内做固件的校验
*/
__attribute__((weak))
bool platform_ota_data_transmit_complete_and_check(uint32_t total_image_size)
{
    dma_log("%s--size-%d\n", __func__, total_image_size);
    return true;
}
/**
* @brief OTA数据check完成后的回调
* @param[in] check_pass      耳机固件校验成功与否
*               - true 固件校验没问题（若为TWS耳机，两边都没问题）
*               - false 固件校验有问题（若为TWS耳机，至少一边有问题）
* @note OTA数据传输完成后回调此函数，可以在此函数内做固件的校验
*/
__attribute__((weak))
void platform_ota_image_check_complete(bool check_pass)
{
    dma_log("%s--flag-%d\n", __func__, check_pass);
}

/**
* @brief 应用新的固件，系统重启，使用新的固件启动
* @return 返回说明
*           -true   应用新的固件成功
*           -false  应用新的固件失败
* @note 应用新的固件，系统重启，使用新的固件启动。只有固件校验通过，才会走到这个调用。
*/
__attribute__((weak))
bool platform_ota_image_apply(void)
{
    dma_log("%s\n", __func__);

    return true;
}

__attribute__((weak))
void platform_ota_send_custom_info_to_peer(uint16_t param_size, uint8_t *param_buf)
{
    dma_log("%s\n", __func__);
}

__attribute__((weak))
void dma_ota_disconnect(void)
{
    dma_log("%s\n", __func__);
}


void disconnect_free_ota_buffer(u8 flag)
{
    r_printf("%s\n", __func__);
    if (update_data_buf == NULL) {
        return ;
    }
    dma_ota_disconnect();
    if (dma_check_tws_is_master()) {
        platform_ota_send_custom_info_to_peer(3, "JLE");
    }
    if (flag) {
        dma_ota_stop_ota();
    }
    if (update_data_buf) {
        printf("===================free+++++++++++++++++\n");
        free(update_data_buf);
        update_data_buf = NULL;
    }
}


DMA_OTA_TWS_ROLE_TYPE platform_ota_get_tws_role()
{
    DMA_OTA_TWS_ROLE_TYPE role_type = DMA_OTA_TWS_UNKNOWN;
    if (dma_check_tws_is_master()) {
        role_type = DMA_OTA_TWS_MASTER;
    } else {
        role_type = DMA_OTA_TWS_SLAVE;
    }
    //dma_log("%s %d\n", __func__, role_type);
    return role_type;
}
bool platform_ota_get_mobile_mtu(uint32_t *mtu)
{
    if (mtu == NULL) {
        return false;
    }
    if (bt_get_curr_channel_state() & SPP_CH) {
        *mtu = 500;
    } else {
        *mtu = global_local_ble_mtu - 2;
    }
    dma_log("%s-%d\n", __func__, *mtu);
    return true;
}
void platform_dma_ota_configure(void)
{
    g_dma_ota_operation.callback_by_timeout       = platform_ota_callback_by_timeout;
    g_dma_ota_operation.get_firmeware_version     = platform_get_firmeware_version;
    g_dma_ota_operation.dma_ota_heap_malloc       = platform_heap_malloc;
    g_dma_ota_operation.dma_ota_heap_free     = platform_heap_free;
    g_dma_ota_operation.get_mobile_mtu        = platform_get_mobile_mtu;
    g_dma_ota_operation.get_peer_mtu          = platform_get_peer_mtu;
    g_dma_ota_operation.get_tws_role         = platform_ota_get_tws_role;
    g_dma_ota_operation.is_tws_link_connected = platform_get_peer_connect_state;
    g_dma_ota_operation.send_data_to_mobile      = platform_ota_send_data_to_mobile;
    g_dma_ota_operation.send_data_to_tws_peer    = platform_ota_send_custom_info_to_peer;
    g_dma_ota_operation.save_ota_info            = platform_ota_save_ota_info;
    g_dma_ota_operation.read_ota_info            = platform_ota_read_ota_info;
    g_dma_ota_operation.get_flash_data_buffer         = platform_ota_get_flash_data_buffer;
    g_dma_ota_operation.write_image_data_to_flash     = platform_ota_write_image_data_to_flash;
    g_dma_ota_operation.read_image_data_from_flash    = platform_ota_read_image_data_from_flash;
    g_dma_ota_operation.erase_image_data_from_flash   = platform_ota_erase_image_data_from_flash;
    g_dma_ota_operation.enter_ota_state          =  platform_ota_enter_ota_state;
    g_dma_ota_operation.set_new_image_size       =  platform_ota_set_new_image_size;
    g_dma_ota_operation.exit_ota_state           =  platform_ota_exit_ota_state;
    g_dma_ota_operation.data_transmit_complete_and_check = platform_ota_data_transmit_complete_and_check;
    g_dma_ota_operation.image_check_complete     = platform_ota_image_check_complete;
    g_dma_ota_operation.image_apply              = platform_ota_image_apply;
}
#endif
static uint8_t vendor_id[] = {0xB0, 0x02};
static uint8_t JL_vendor_id[] = {0xD6, 0x05};

void dma_thread(void const *argument)
{
    /* printf("dma task..."); */
    //while (true) {
    dma_process(0xffffffff);
    //}
}

uint32_t platform_dma_init()
{
    int init_error  = 0;
    u8  device_type = 1;
    platform_dma_configure();
    dma_register_operation(&g_dma_operation);
    if (dma_use_lib_only_ble) {
        device_type = 0;
    }
    init_error = dma_protocol_init(
                     (uint8_t *)__this_cfg->dma_cfg_file_product_id,
                     (uint8_t *)__this_cfg->dma_cfg_file_product_key,
                     JL_vendor_id,
                     device_type,
                     3,
                     dma_queue_buffer_pool
                 );

    dma_log(" platform_dma_init %d----need size %d\n", init_error, (int)(sizeof(DUER_DMA_OPER) + DMA_QUEUE_BUFFER_SIZE));


    return 0;
}

void platform_dma_ota_init()
{
#if DMA_LIB_OTA_EN
    if (update_data_buf) {
        return ;
    }
    wait_master_ota_init = 1;
    if (dma_check_tws_is_master()) {
        platform_ota_send_custom_info_to_peer(3, "JLB");
    }
    platform_dma_ota_configure();
    dma_ota_register_operation(&g_dma_ota_operation);
    dma_log("=====dma_ota_init====\n");
    dma_ota_init(560 * 1024);
    app_protocol_enter_ota_state();
#endif
}
extern int dma_disconnect(void *addr);
extern void cpu_assert_debug();
extern const int config_asser;

void dma_assert(int cond, ...)
{
#ifdef CONFIG_RELEASE_ENABLE
    if (!(cond)) {
        //cpu_reset();
        dma_disconnect(NULL);
    }
#else
    int argv[8];
    int argv_cnt = 0;
    va_list argptr;

    va_start(argptr, cond);
    for (int i = 0; i < 5; i++) {
        argv[i] = va_arg(argptr, int);
        if (argv[i]) {
            argv_cnt++;
        } else {
            break;
        }
    }
    va_end(argptr);
    if (!(cond)) {
        dma_log("================dma assert ==%d============\n", argv_cnt);
        if (argv_cnt == 2) {
            dma_log("[%s]", (char *)argv[0]);
        }
        if (argv_cnt > 3) {
            dma_log("[%s %s %d]", (char *)argv[0], (char *)argv[1], (int)argv[2]);
        }
        dma_disconnect(NULL);
        //while (1);
    }
#endif
}

#if 0   // enable lib debug

int dma_lib_printf(const char *sFormat, ...)
{
    int len = 0;
    printf("[DMA_LIB_LOG] ");
    va_list ap;
    va_start(ap, sFormat);
    len = print(NULL, NULL, sFormat, ap);
    va_end(ap);
    return len;
}
#endif

u16 dma_speech_data_send(u8 *buf, u16 len)
{
//DMA_ERROR_CODE dma_feed_compressed_data(const char* input_data, uint32_t date_len);
    u16 res = 0;
    if (dma_ota_connect_flag) {
        return DMA_SUCCESS;
        if (dma_role_switch_ble_disconnect_flag) {
            putchar('K');
            //需要主从切换的流程不响应mic数据发送
            return res;
        }
    }
    res = dma_feed_compressed_data((const char *)buf, len);
    if (DMA_SUCCESS != res) {
        dma_log("send error %d\n", res);
    }
    return res;
}
int dma_start_voice_recognition(u8 en)
{
    u8 flag = true;
    if (dma_enter_ota_flag) {
        return -1;
    }
    if (en == 2) {
        flag = true;
    } else {
        flag = false;
    }
    //DMA_ERROR_CODE dma_notify_state(DMA_NOTIFY_STATE state);
    dma_log("dma_notify_state---%d---%d\n", DMA_NOTIFY_STATE_DOUBLE_CLICK, flag);
    if (en) {
        dma_app_notify_state(DMA_NOTIFY_STATE_DOUBLE_CLICK, &flag, 1);
    }
    return 0;
}
extern bool check_dma_le_handle(void);
extern void dma_ble_disconnect(void);
u8 tws_role_switch_wait_cnt = 0;
u8 tws_role_switch_check_ble_status()
{
    if (check_dma_le_handle()) {
        if ((tws_role_switch_wait_cnt == 10) && (dma_role_switch_ble_disconnect_flag == 0xAA)) {
            dma_role_switch_ble_disconnect_flag = 0xAB;
            dma_ble_disconnect();
        }
        if (tws_role_switch_wait_cnt++ > 12) {
            tws_role_switch_wait_cnt = 0;
            y_printf("tws_role_switch_wait timeout\n");
            return 0;
        }
        putchar('@');
        return 1;
    }
    return 0;
}
int dma_update_tws_state_to_lib(int state)
{
    u8 flag = true;
    if (strcmp(os_current_task(), "app_core")) {
        printf("%s, task:%s", __func__, os_current_task());
        int msg[] = {(int)dma_update_tws_state_to_lib, 1, state};
        os_taskq_post_type("app_core", Q_CALLBACK, ARRAY_SIZE(msg), msg);
        return 0;
    }
    dma_log("dma_notify_state---%d\n", state);
    if (state == JL_NOTIFY_STATE_STOP_OTA) {
        if (dma_enter_ota_flag) {
            r_printf("user___stop ota===\n");
            dma_ota_stop_ota();
            //dma_ble_disconnect();
        }
    }
    if (state == DMA_NOTIFY_STATE_TWS_DISCONNECT) {
#if DMA_LIB_OTA_EN
        if (update_data_buf) {
            r_printf("user___stop ota 2222===\n");
            disconnect_free_ota_buffer(1);
        }
#endif
    }
    if (state == DMA_NOTIFY_STATE_TWS_CONNECT) {
        //初始化判断的主从不准，主从连上，从机要关了
        if (!dma_check_tws_is_master()) {
            dma_log("ble connect slave before tws pair\n");
            dma_ble_disconnect();
        }
        dma_ble_adv_enable(0);
    }
    if (state == DMA_NOTIFY_STATE_ROLE_SWITCH_START) {
        //主从切换断开BLE，如果是超时会比较慢
        dma_ble_adv_enable(0);
        if (dma_check_tws_is_master()) {
            dma_role_switch_ble_disconnect_flag = 0xAA;
            tws_role_switch_wait_cnt = 0;
        }
    }
    if (state < JL_NOTIFY_STATE_ONE_CLICK) {
        dma_app_notify_state(state, &flag, 1);
    }
    if (state == DMA_NOTIFY_STATE_ROLE_SWITCH_FINISH) {
        if (!dma_check_tws_is_master()) {
            dma_ble_adv_enable(0);
        }
    }
    if (state == DMA_NOTIFY_STATE_DMA_DISCONNECTED) {
        dma_role_switch_ble_disconnect_flag = 0x0;
    }
    return 0;
}
void dma_ota_recieve_data(u8 *buf, u16 len)
{
#if DMA_LIB_OTA_EN
    if (len < 32) {
        put_buf(buf, len);
    }
    //DMA_OTA_CMD_IMAGE_VERIFY 事件要开启定时校验延时
    if ((len == 1) && (buf[0] == 0x90)) {
        crc_os_time_dly_flag = 1;
    }
    if (bt_get_curr_channel_state() & SPP_CH) {
        dma_ota_recv_mobile_data(len, buf, false, false);
    } else {
        dma_ota_recv_mobile_data(len, buf, true, true);
    }
#endif
}
void dma_recieve_data(u8 *buf, u16 len)
{
    //DMA_ERROR_CODE dma_recv_mobile_data(const char *input_data, uint32_t date_len);
    dma_recv_mobile_data((const char *)buf, len);
    printf("dma_recv_mobile_data");
    put_buf(buf, len);
    //发了通知app 切换之后收到数据包认为通知成功，断开ble
    if (dma_role_switch_ble_disconnect_flag == 0xAA) {
        dma_role_switch_ble_disconnect_flag = 0xAB;
        dma_ble_disconnect();
    }
}

void dma_ota_peer_recieve_data(u8 *buf, u16 len)
{
#if DMA_LIB_OTA_EN
    if (dma_check_tws_is_master()) {
        //主机如果没有处理第一步数据就处理TWS发过来的数据包，可能会导致开始OTA就失败
        while (wait_master_ota_init) {
            putchar('@');
            os_time_dly(10);
        }
    }
    dma_log("%s -- %d\n", __func__, len);
    if (!(get_curr_channel_state() & SPP_CH)) {
        //BLE的时候所有包都是TWS直接转发过来的。这个dly标识要在这里做
        if ((len == 2) && (buf[0] == 0x09) && (buf[1] == 0x90)) {
            dma_log("set crc_os_time_dly_flag\n");
            crc_os_time_dly_flag = 1;
        }
        //BLE从机没有连接和断开信息，所以有些操作要做处理
        if ((len == 3) && (buf[0] == 'J') && (buf[1] == 'L') && (buf[2] == 'B')) {
            platform_dma_ota_init();
            return ;
        }
        if ((len == 3) && (buf[0] == 'J') && (buf[1] == 'L') && (buf[2] == 'E')) {
            disconnect_free_ota_buffer(1);
            return ;
        }
    }
    dma_ota_recv_tws_data(len, buf);
#endif
}
void dma_peer_recieve_data(u8 *buf, u16 len)
{
    dma_log("%s -- %d\n", __func__, len);
    dma_recv_peer_data((const char *)buf, len);
}

extern int dma_pair_state();
int update_tws_battary_to_app()
{
    u8 flag = true;
    DMA_TWS_SIDE_TYPE tws_side = 0;
    if (!dma_pair_state()) {
        return -1;
    }
    platform_get_tws_side(&tws_side);
    if (tws_side == DMA_SIDE_LEFT) {
        flag = true;
    } else {
        flag = false;
    }
    dma_log("dma_notify_bat_state--side:%d\n", flag);
    dma_app_notify_state(DMA_NOTIFY_STATE_BATTERY_LEVEL_UPDATE, &flag, 1);
    return 0;
}

