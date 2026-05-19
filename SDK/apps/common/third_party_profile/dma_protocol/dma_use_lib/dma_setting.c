#include <stdlib.h>
#include <string.h>
#include "typedef.h"
#include "btstack/avctp_user.h"
///#include "system/timer.h"
#include "btstack/third_party/app_protocol_event.h"
#include "dma_wrapper.h"
#include "dma_setting.h"
#include "dma_ota_wrapper.h"
#include "app_ble_spp_api.h"
#include "btstack/bluetooth.h"
#include "dma_le_port.h"
#include "dma_platform_api.h"

#ifdef DMA_LIB_CODE_SIZE_CHECK
#pragma bss_seg(	".bt_dma_port_bss")
#pragma data_seg(	".bt_dma_port_data")
#pragma const_seg(	".bt_dma_port_const")
#pragma code_seg(	".bt_dma_port_code")
#endif

extern const char *dma_product_id;
extern const char *dma_product_key;
extern const char *dma_triad_id;
extern const char *dma_secret;

//项目ID，不需要三元组
extern const char *no_triad_product_id_lib;
extern const char *no_triad_product_key_lib;

struct dueros_cfg_t dueros_global_cfg;
#define __this_cfg  (&dueros_global_cfg)
void dma_set_product_id_key(void *data)
{
#ifndef CONFIG_RELEASE_ENABLE
    return;
#endif
    log_info(" dma_set_product_id_key\n");
    struct dueros_cfg_t *info = (struct dueros_cfg_t *)data;
    memset(__this_cfg, 0, sizeof(struct dueros_cfg_t));
    if (data) {
        memcpy(__this_cfg->dma_cfg_file_product_id, info->dma_cfg_file_product_id, PRODUCT_ID_LEN);
        memcpy(__this_cfg->dma_cfg_file_product_key, info->dma_cfg_file_product_key, PRODUCT_KEY_LEN);
        memcpy(__this_cfg->dma_cfg_file_triad_id,   info->dma_cfg_file_triad_id, TRIAD_ID_LEN);
        memcpy(__this_cfg->dma_cfg_file_secret,     info->dma_cfg_file_secret, SECRET_LEN);
        log_info("\n%s\n", __this_cfg->dma_cfg_file_product_id);
        log_info("\n%s\n", __this_cfg->dma_cfg_file_product_key);
        __this_cfg->dma_cfg_read_file_flag = 1;
    }
}

#ifndef CONFIG_RELEASE_ENABLE
void dma_default_special_info_init()
{
    u8 copy_len = 0;
    if (!__this_cfg->dma_cfg_read_file_flag) {
        memset(__this_cfg, 0, sizeof(struct dueros_cfg_t));
        copy_len = strlen(dma_product_id);
        memcpy(__this_cfg->dma_cfg_file_product_id, dma_product_id, copy_len);
        copy_len = strlen(dma_product_key);
        memcpy(__this_cfg->dma_cfg_file_product_key, dma_product_key, copy_len);
        copy_len = strlen(dma_triad_id);
        memcpy(__this_cfg->dma_cfg_file_triad_id,  dma_triad_id, copy_len);
        copy_len = strlen(dma_secret);
        memcpy(__this_cfg->dma_cfg_file_secret,   dma_secret, copy_len);
        printf("\ndefault info\n");
        printf("\n%s\n", __this_cfg->dma_cfg_file_product_id);
        printf("\n%s\n", __this_cfg->dma_cfg_file_product_key);
        printf("\n%s\n", __this_cfg->dma_cfg_file_triad_id);
        printf("\n%s\n", __this_cfg->dma_cfg_file_secret);
    }
}
#endif
/**获取查询当前是不是TWS master状态。默认就是master端*/
static int (*dma_check_status)(int flag) = NULL;
void dma_check_status_callback_register(int (*handler)(int state_flag))
{
    dma_check_status = handler;
}
//默认认为是主
bool dma_check_tws_is_master()
{
    if (dma_check_status) {
        return dma_check_status(CHECK_STATUS_TWS_MASTER);
    }
    return 1;
}
int dma_check_some_status(int flag)
{
    if (dma_check_status) {
        return dma_check_status(flag);
    }
    return 0;
}

static void (*dma_tx_resume)(void) = NULL;
void dma_tx_resume_register(void (*handler)(void))
{
    dma_tx_resume = handler;
}
static void (*dma_rx_resume)(void) = NULL;
void dma_rx_resume_register(void (*handler)(void))
{
    dma_rx_resume = handler;
}
void dma_send_process_resume(void)
{
    if (dma_tx_resume) {
        dma_tx_resume();
    }
}
void dma_cmd_analysis_resume(void)
{
    if (dma_rx_resume) {
        dma_rx_resume();
    }
}

/**一个函数更新状态到上层使用*/
static int (*dma_message_update)(int id, int opcode, u8 *data, u32 len) = NULL;
void dma_message_callback_register(int (*handler)(int id, int opcode, u8 *data, u32 len))
{
    dma_message_update = handler;
}
int dma_message(int opcode, u8 *data, u32 len)
{
    if (dma_message_update) {
        return dma_message_update(DMA_HANDLER_ID, opcode, data, len);
    }
    return -1;
}

static int (*dueros_ota_send_cmd_data)(void *priv, u8 *buf, u16 len) = NULL;
void server_register_dueros_ota_send_callbak(int (*send_callbak)(void *priv, u8 *buf, u16 len))
{
    dueros_ota_send_cmd_data = send_callbak;
}
extern int dma_update_tws_state_to_lib(int state);
static int (*dueros_send_cmd_data)(void *priv, u8 *buf, u16 len) = NULL;
void server_register_dueros_send_callbak(int (*send_callbak)(void *priv, u8 *buf, u16 len))
{
    u8 update_flag = 0;
    if (dueros_send_cmd_data == NULL) {
        //重复注册就不需要更新这个DMA状态了
        update_flag = 1;
    }
    dueros_send_cmd_data = send_callbak;
    if (update_flag) {
        set_dueros_pair_state(1, 0);
        dma_update_tws_state_to_lib(DMA_NOTIFY_STATE_DMA_CONNECTED);
    }
    if (send_callbak) {
        dma_update_tws_state_to_lib(DMA_NOTIFY_STATE_SEND_PREPARE_DONE);
    } else {
        set_dueros_pair_state(0, 0);
        dma_update_tws_state_to_lib(DMA_NOTIFY_STATE_DMA_DISCONNECTED);
    }
}

int dueros_common_send_cmd_data(uint8_t *data, uint32_t len)
{
    int ret = 0;
    if (NULL != app_spp_get_hdl_remote_addr(dma_app_spp_hdl)) {
        ret = app_spp_data_send(dma_app_spp_hdl, data, len);
    }
    if (0 != app_ble_get_hdl_con_handle(dma_app_ble_hdl)) {
        ret = dma_ble_send_user_data(ATT_CHARACTERISTIC_b84ac9c6_29c5_46d4_bba1_9d534784330f_01_VALUE_HANDLE, data, len, ATT_OP_AUTO_READ_CCC);
    }
    return ret;
}

int dueros_ota_common_send_cmd_data(uint8_t *data, uint32_t len)
{
    int ret = 0;
#if DMA_LIB_OTA_EN
    //if (NULL != app_spp_get_hdl_remote_addr(dma_app_spp_hdl)) {
    //    ret = app_spp_data_send(dma_app_spp_hdl, data, len);
    //}
#endif
    if (0 != app_ble_get_hdl_con_handle(dma_app_ble_hdl)) {
        ret = dma_ble_send_user_data(ATT_CHARACTERISTIC_0000FF00_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE, data, len, ATT_OP_AUTO_READ_CCC);
    }
    return ret;
}

int dueros_sent_raw_audio_data(uint8_t *data, uint32_t len)
{
    return dueros_common_send_cmd_data(data, len);
}
int dueros_sent_raw_cmd(uint8_t *data,  uint32_t len)
{
    return dueros_common_send_cmd_data(data, len);
}
int dueros_ota_sent_raw_cmd(uint8_t *data,  uint32_t len)
{
    if (dueros_ota_send_cmd_data) {
        return dueros_ota_common_send_cmd_data(data, len);
    } else {
        return dueros_sent_raw_cmd(data, len);
    }
    return 1;
}
bool check_can_send_data_or_not()
{
    if ((NULL != app_spp_get_hdl_remote_addr(dma_app_spp_hdl)) \
        || (0 != app_ble_get_hdl_con_handle(dma_app_ble_hdl))) {
        return true;
    } else {
        return false;
    }
}
/**获取查询当前是不是ble update状态。*/
static bool (*ble_update_jump_flag)(void) = NULL;
void dma_ble_update_flag_callback_register(bool (*handler)(void))
{
    ble_update_jump_flag = handler;
}
bool dma_get_ble_update_ready_jump_flag()
{
    if (ble_update_jump_flag) {
        return ble_update_jump_flag();
    }
    return 0;
}

/*配置全局变量和配置接口*/
u32 dma_software_version = 0x00000001;
void dma_set_local_version(u32 version)
{
    dma_software_version = version;
}

u8 dma_ota_result = 0;
void dma_set_ota_result(u8 flag)
{
    dma_ota_result = flag;
}

static u8 dueros_pair_state = 0;    // 1:BLE/SPP connected   2:APP connected
bool set_dueros_pair_state(u8 new_state, u8 init_flag)
{
    dueros_pair_state = new_state;
    //log_info("\n!!!---pair_steps new= %d\n\n", dueros_pair_state);
    dma_protocol_tws_sync_pair_state(dueros_pair_state);
    return TRUE;
}

bool set_dueros_pair_state_2(u8 new_state)
{
    dueros_pair_state = new_state;
    return TRUE;
}

void dma_tws_sync_pair_state()
{
    if (dueros_pair_state == 2) {
        dma_protocol_tws_sync_pair_state(dueros_pair_state);
    }
}

int dma_pair_state()
{
    return dueros_pair_state ? true : false;
}

DMA_ERROR_CODE dma_app_notify_state(DMA_NOTIFY_STATE state, void *param_buf, uint32_t  param_size)
{
    DMA_ERROR_CODE ret;
    ret = dma_notify_state(state, param_buf, param_size);
    dma_send_process_resume();
    return ret;
}

const u8 *bt_get_mac_addr();
char dma_serial_number[20] = "12345667";
static void serial_number_init()
{
    memset(dma_serial_number, 0, sizeof(dma_serial_number));
    u8 addr_buf[6];
    swapX(bt_get_mac_addr(), addr_buf, 6);
    for (u8 i = 0; i < 4; i++) {
        sprintf(&dma_serial_number[2 * i], "%02x", addr_buf[i]);
    }

    log_info("serial_number = %s\n", dma_serial_number);
}
int dma_tws_peer_send(u8 type, u8 *data, int len)
{
    int ret = 0;
    u8 *data_send = malloc(len + 4);
    data_send[0] = DMA_TWS_LIB_SEND_MAGIC_NUMBER;
    data_send[1] = type;
    memcpy(&data_send[4], data, len);
    ret = dma_message(APP_PROTOCOL_LIB_TWS_DATA_SYNC, data_send, len + 4);
    free(data_send);
    return ret;

}
int dma_tws_data_deal(u8 *data, int len)
{
    //r_printf("%s",__func__);
    if (data[0] == DMA_TWS_LIB_SEND_MAGIC_NUMBER) {
        if (data[1] == DMA_LIB_DATA_TYPE_DMA) {
            dma_recv_peer_data((const char *)data + 4, len - 4);
        } else if (data[1] == DMA_LIB_DATA_TYPE_OTA) {
#if DMA_LIB_OTA_EN
            dma_ota_peer_recieve_data(data + 4, len - 4);
#endif
        }
    }
    return 0;
}
extern uint32_t platform_dma_init();
extern void dma_spp_init(void);
extern void dma_ble_init(void);
extern void dma_ble_profile_init(void);
int dma_all_init(void)
{
    u8 flag = true;
    serial_number_init();
    set_dueros_pair_state(0, 0);
#ifndef CONFIG_RELEASE_ENABLE
    dma_default_special_info_init();
#endif
    platform_dma_init();

    dma_ble_profile_init();
    dma_spp_init();
    dma_ble_init();
    dma_app_notify_state(DMA_NOTIFY_STATE_BOX_OPEN, &flag, 1);
    return 0;
}

int dma_all_exit(void)
{
    return 0;
}

extern void dma_thread(void const *argument);
int dueros_process()
{
    dma_thread(NULL);
    return 0;
}
int dueros_send_process(void)
{
    dma_thread(NULL);
    return 0;
}
