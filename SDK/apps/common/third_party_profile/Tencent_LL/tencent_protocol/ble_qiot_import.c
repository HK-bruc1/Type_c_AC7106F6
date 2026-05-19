#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".ble_qiot_import.data.bss")
#pragma data_seg(".ble_qiot_import.data")
#pragma const_seg(".ble_qiot_import.text.const")
#pragma code_seg(".ble_qiot_import.text")
#endif
#include "ble_qiot_import.h"
/* #include "le/ble_api.h" */
/* #include "vm.h" */
#include "le_common.h"
#include "syscfg_id.h"
#include "user_cfg_id.h"
#include "ble_qiot_config.h"
#include "user_cfg.h"
#include "timer.h"
#include "ble_iot_power_manager.h"
#include "dual_bank_updata_api.h"
#include "classic/tws_api.h"
#include "update_tws_new.h"
#include "btstack/avctp_user.h"
#include "app_config.h"

#if (THIRD_PARTY_PROTOCOLS_SEL & LL_SYNC_EN)

// 杰理防丢器
/* #define PRODUCT_ID  "YSI2RECEHM" */
/* #define DEVICE_NAME "30AEA4DDC98A" */
/* #define SECRET_KEY  "bV9SYZl9vZzw/HfFvrph0g==" */

// 杰理耳机
#define PRODUCT_ID  "5IO900RHWV"
#define DEVICE_NAME "jl_earphone"
#define SECRET_KEY  "J294C9vj8azUVZo18ZQAvg=="

#define LOG_TAG             "[ble_qiot_import]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

#define ADV_INTERVAL_MIN          (160)

static u8 adv_data_len;
static u8 adv_data[ADV_RSP_PACKET_MAX];//max is 31
static u8 scan_rsp_data_len;
static u8 scan_rsp_data[ADV_RSP_PACKET_MAX];//max is 31

int ble_get_product_id(char *product_id)
{
    log_info("ble_get_product_id");
    memcpy(product_id, PRODUCT_ID, strlen(PRODUCT_ID));
    put_buf((const u8 *)product_id, strlen(product_id));
    return 0;
}

int ble_get_device_name(char *device_name)
{
    log_info("ble_get_device_name");
    memcpy(device_name, DEVICE_NAME, strlen(DEVICE_NAME));
    put_buf((const u8 *)device_name, strlen(device_name));
    return strlen(device_name);
}

int ble_get_psk(char *psk)
{
    log_info("ble_get_psk");
    memcpy(psk, SECRET_KEY, strlen(SECRET_KEY));
    put_buf((const u8 *)psk, strlen(psk));
    return 0;
}

extern const u8 *ble_get_mac_addr(void);
int ble_get_mac(char *mac)
{
    log_info("ble_get_mac");
    le_controller_get_mac(mac);
    put_buf((const u8 *)mac, 6);
    return 0;
}

static void (*app_set_adv_data)(u8 *adv_data, u8 adv_len) = NULL;
static void (*app_set_rsp_data)(u8 *rsp_data, u8 rsp_len) = NULL;
void app_set_adv_data_register(void (*handler)(u8 *adv_data, u8 adv_len))
{
    app_set_adv_data = handler;
}
void app_set_rsp_data_register(void (*handler)(u8 *rsp_data, u8 rsp_len))
{
    app_set_rsp_data = handler;
}

static int llsync_set_rsp_data(void)
{
    u8 offset = 0;
    u8 *buf = scan_rsp_data;

    const char *edr_name = bt_get_local_name();
    offset += make_eir_packet_data(&buf[offset], offset, HCI_EIR_DATATYPE_COMPLETE_LOCAL_NAME, (void *)edr_name, strlen(edr_name) > 26 ? 26 : strlen(edr_name));

    if (offset > ADV_RSP_PACKET_MAX) {
        printf("***rsp_data overflow!!!!!!\n");
        return -1;
    }

    //printf("rsp_data(%d):", offset);
    //printf_buf(buf, offset);
    scan_rsp_data_len = offset;
    if (app_set_rsp_data) {
        app_set_rsp_data(scan_rsp_data, scan_rsp_data_len);
    }
    ble_op_set_rsp_data(offset, buf);
    return 0;
}

static int llsync_set_adv_data(adv_info_s *adv)
{
    u8 offset = 0;
    u8 *buf = adv_data;
    u8 manufacturer_buf[20];

    memcpy(&manufacturer_buf[0], &adv->manufacturer_info.company_identifier, 2);
    memcpy(&manufacturer_buf[2], adv->manufacturer_info.adv_data, adv->manufacturer_info.adv_data_len);
    printf("manufacturer_buf\n");
    put_buf((const u8 *)manufacturer_buf, adv->manufacturer_info.adv_data_len + 2);

    offset += make_eir_packet_val(&buf[offset], offset, HCI_EIR_DATATYPE_FLAGS, 0x06, 1);
    offset += make_eir_packet_val(&buf[offset], offset, HCI_EIR_DATATYPE_COMPLETE_16BIT_SERVICE_UUIDS, IOT_BLE_UUID_SERVICE, 2);
    offset += make_eir_packet_data(&buf[offset], offset, HCI_EIR_DATATYPE_MANUFACTURER_SPECIFIC_DATA, manufacturer_buf, adv->manufacturer_info.adv_data_len + 2);

    if (offset > ADV_RSP_PACKET_MAX) {
        puts("***adv_data overflow!!!!!!\n");
        printf("offset = %d, limite is %d\n", offset, ADV_RSP_PACKET_MAX);
        return -1;
    }

    //log_info("adv_data(%d):", offset);
    //log_info_hexdump(buf, offset);
    adv_data_len = offset;
    if (app_set_adv_data) {
        app_set_adv_data(adv_data, adv_data_len);
    }
    ble_op_set_adv_data(offset, buf);
    return 0;
}

static void (*llsync_ble_module_enable)(u8 en) = NULL;
ble_qiot_ret_status_t ble_advertising_start(adv_info_s *adv, uint8_t start)
{
    log_info("ble_advertising_start\n");
    //log_info_hexdump(adv->manufacturer_info.adv_data, adv->manufacturer_info.adv_data_len);

    llsync_set_adv_data(adv);
    llsync_set_rsp_data();

    if (start) {
        if (llsync_ble_module_enable) {
            llsync_ble_module_enable(1);
        }
    }
    return 0;
}

void llsync_ble_module_enable_register(void (*handler)(u8 en))
{
    llsync_ble_module_enable = handler;
}

ble_qiot_ret_status_t ble_advertising_stop(void)
{
    log_info("ble_advertising_stop\n");
    if (llsync_ble_module_enable) {
        llsync_ble_module_enable(0);
    }
    return 0;
}

static int (*llsync_send_data)(void *priv, void *data, u16 len) = NULL;
void llsync_send_data_register(int (*handler)(void *priv, void *buf, u16 len))
{
    llsync_send_data = handler;
}

ble_qiot_ret_status_t ble_send_notify(uint8_t *buf, uint8_t len)
{
    if (llsync_send_data) {
        llsync_send_data(NULL, buf, len);
    } else {
        log_info("llsync_send_data no register");
    }
    return 0;
}

uint16_t ble_get_user_data_mtu_size(void)
{
    return BLE_QIOT_PACKAGE_LENGTH + 6;
}


// 小程序升级的时候会优先调用这个函数判断是否可以升级
uint8_t ble_ota_is_enable(const char *version)
{
    log_info("remote version: %s, current version: %s\n", version, BLE_QIOT_USER_DEVELOPER_VERSION);
    u8 lefttear_battery_percentage = iot_get_leftear_battery_percentage();
    u8 righttear_battery_percentage = iot_get_rightear_battery_percentage();
    if (((lefttear_battery_percentage > 0) && (lefttear_battery_percentage < 30)) || ((righttear_battery_percentage > 0) && (righttear_battery_percentage < 30))) {
        return BLE_OTA_DISABLE_LOW_POWER;
    }
    return BLE_OTA_ENABLE;
}

uint32_t ble_ota_get_download_addr(void)
{
    uint32_t download_addr = dual_bank_passive_update_get_target_update_addr();
    /* log_info("%s - 0x%x!\n", __FUNCTION__, download_addr); */
    return download_addr;
}

int ble_read_flash(uint32_t flash_addr, char *read_buf, uint16_t read_len)
{
    u16 vm_id = 0;
    log_info("\n\nble read flash idx:%d len:%d", flash_addr, read_len);
    switch (flash_addr) {
    case BLE_QIOT_RECORD_FLASH_ADDR:
        vm_id = CFG_LLSYNC_RECORD_ID;
        break;
#if BLE_QIOT_SUPPORT_RESUMING
    case BLE_QIOT_OTA_INFO_FLASH_ADDR:
        vm_id = CFG_LLSYNC_OTA_INFO_ID;
        break;
#endif
    default:
        log_info("\n\nread flash addr error\n");
        return read_len;
        break;
    }

    if (read_len == syscfg_read(vm_id, read_buf, read_len)) {
        put_buf((const u8 *)read_buf, read_len);
    } else {
        log_info("\n\nread flash error\n");
    }
    return read_len;
}

extern void bt_tws_sync_ll_sync_state();
int ble_write_flash(uint32_t flash_addr, const char *write_buf, uint16_t write_len)
{
    u16 vm_id = 0;
    log_info("\n\nble write flash idx:%d len:%d", flash_addr, write_len);
    put_buf((const u8 *)write_buf, write_len);
    switch (flash_addr) {
    case BLE_QIOT_RECORD_FLASH_ADDR:
        vm_id = CFG_LLSYNC_RECORD_ID;
        break;
#if BLE_QIOT_SUPPORT_RESUMING
    case BLE_QIOT_OTA_INFO_FLASH_ADDR:
        vm_id = CFG_LLSYNC_OTA_INFO_ID;
        break;
#endif
    default:
        log_info("\n\nwrite flash addr error\n");
        return write_len;
        break;
    }

    if (write_len == syscfg_write(vm_id, write_buf, write_len)) {
        log_info("\n\nwrite flash succes\n");
#if TCFG_USER_TWS_ENABLE
        bt_tws_sync_ll_sync_state();
#endif
    } else {
        log_info("\n\nwrite flash error\n");
    }
    return write_len;
}


typedef struct ble_jl_timer_id_ {
    uint8_t       type;
    ble_timer_cb  handle;
    int           timer;
    char         *name;
} ble_jl_timer_id;

ble_timer_t ble_timer_create(uint8_t type, ble_timer_cb timeout_handle, char *name)
{
    ble_jl_timer_id *p_timer = zalloc(sizeof(ble_jl_timer_id));
    if (NULL == p_timer) {
        return NULL;
    }

    p_timer->type   = type;
    p_timer->handle = timeout_handle;
    p_timer->name   = name;
    p_timer->timer  = 0;

    return (ble_timer_t)p_timer;
}

ble_qiot_ret_status_t ble_timer_start(ble_timer_t timer_id, uint32_t period)
{
    ble_jl_timer_id *p_timer = (ble_jl_timer_id *)timer_id;

    if (0 == p_timer->timer) {
        if (p_timer->type == BLE_TIMER_PERIOD_TYPE) {
            p_timer->timer = sys_timer_add(NULL, p_timer->handle, period);
        } else if (p_timer->type == BLE_TIMER_ONE_SHOT_TYPE) {
            p_timer->timer = sys_timeout_add(NULL, p_timer->handle, period);
        }
    }
    return BLE_QIOT_RS_OK;
}

ble_qiot_ret_status_t ble_timer_stop(ble_timer_t timer_id)
{
    ble_jl_timer_id *p_timer = (ble_jl_timer_id *)timer_id;
    if (p_timer->timer) {
        if (0 != p_timer->timer) {
            if (p_timer->type == BLE_TIMER_PERIOD_TYPE) {
                sys_timer_del(p_timer->timer);
            } else if (p_timer->type == BLE_TIMER_ONE_SHOT_TYPE) {
                sys_timeout_del(p_timer->timer);
            }
            p_timer->timer = 0;
        }
    }
    return BLE_QIOT_RS_OK;
}

ble_qiot_ret_status_t ble_timer_delete(ble_timer_t timer_id)
{
    ble_jl_timer_id *p_timer = (ble_jl_timer_id *)timer_id;

    if (p_timer->timer) {
        if (0 != p_timer->timer) {
            if (p_timer->type == BLE_TIMER_PERIOD_TYPE) {
                sys_timer_del(p_timer->timer);
            } else if (p_timer->type == BLE_TIMER_ONE_SHOT_TYPE) {
                sys_timeout_del(p_timer->timer);
            }
            p_timer->timer = 0;
        }
        free(p_timer);
    }

    return BLE_QIOT_RS_OK;
}

#endif // (THIRD_PARTY_PROTOCOLS_SEL == LL_SYNC_EN)
