#include "system/includes.h"
#include "media/includes.h"
#include "app_config.h"
#include "app_msg.h"
#include "bt_tws.h"
#include "app_main.h"
#include "battery_manager.h"
#include "app_power_manage.h"
#include "btstack/avctp_user.h"
#include "a2dp_player.h"
#include "esco_player.h"
#include "clock_manager/clock_manager.h"
#include "btstack/third_party/app_protocol_event.h"
#include "multi_protocol_main.h"
#include "app_ble_spp_api.h"
#include "asm/charge.h"
#include "dma_platform_api.h"
#include "sdfile.h"
#include "dma_use_lib/dma_wrapper.h"

#if (THIRD_PARTY_PROTOCOLS_SEL & DMA_EN)

#if 1
#define APP_DMA_LOG       printf
#define APP_DMA_DUMP      put_buf
#else
#define APP_DMA_LOG(...)
#define APP_DMA_DUMP(...)
#endif

bool dma_is_tws_master_role()
{
#if TCFG_USER_TWS_ENABLE
    return (tws_api_get_role() == TWS_ROLE_MASTER);
#endif
    return 1;
}

static int dma_get_tws_side()
{
    // CHECK_STATUS_TWS_SIDE:     /*0是单耳，1是左耳，2是右耳*/
#if TCFG_USER_TWS_ENABLE
    if (tws_api_get_local_channel() == 'R') {
        return 2;
    } else if (tws_api_get_local_channel() == 'L') {
        return 1;
    }
#endif
    return 0;
}
//=================================================================================//
//                             用户自定义配置项[1 ~ 49]                            //
//=================================================================================//
//#define 	CFG_USER_DEFINE_BEGIN		1
#define VM_DMA_NAME_EXT                  3
#define VM_DMA_REMOTE_LIC                4
//#define 	CFG_USER_DEFINE_END			49


#define APP_DMA_VERSION             0x00020004 //app十进制显示 xx.xx.xx.xx

#define BT_NAME_EXT_EN          0//使用sn码作为蓝牙后缀名，用于区分耳机和给app回连
//*********************************************************************************//
//                                 DMA认证信息                                     //
//*********************************************************************************//
#define DMA_PRODUCT_INFO_EN         1 //三元组使能
#define DMA_PRODUCT_INFO_TWS_SYNC   1 //三元组对耳同步使能
#define DMA_PRODUCT_INFO_TWS_CH     'L' //烧录了三元组的耳机
#define DMA_PRODUCT_INFO_TEST       1
#define DMA_PRODUCT_READ_FROM_FILE  0

#if DMA_PRODUCT_INFO_EN
u8 use_triad_info = 1;
#else
u8 use_triad_info = 0;
#endif // DMA_PRODUCT_INFO_EN

#define LIC_IN_FLASH 1
#define LIC_IN_VM    2
static u16 lic_data_len = 0;
static u8 lic_data_type = 0;
static u8 *lic_data = NULL;

/****************** DMA PRODUCT INFO TEST ********************/
// const char *dma_product_id  = "ojCAw7een53mBQ0abM2tSCxV8YqDbLR8";
// const char *dma_product_key = "jfyWVswabEAdq1hGoyZxjfztAxU9DSz3";
// const char *dma_triad_id    = "0005if4p0000000400000001";
// const char *dma_secret      = "a4e1cfdef78d401c";
// u8 dma_test_mac[] = {0xF4, 0x43, 0x8D, 0x29, 0x17, 0x02};
const char *dma_product_id  = "c2K6yffXMGAFNMeNEXFzgGcrtqYZKYSi";
const char *dma_product_key = "seXazCNK6CfAI0xlmEsScZ9Mo071I2oD";
const char *dma_triad_id    = "002aCDEy0000002700002833";
const char *dma_secret      = "b1cc860c49af8d0b";
u8 dma_test_mac[] = {0xF4, 0x43, 0x88, 0x28, 0x18, 0x08};
const char *dma_test_sn     = "1723846";

/**************** DMA PRODUCT INFO TEST END ******************/

#define DMA_PRODUCT_ID_LEN      65
#define DMA_PRODUCT_KEY_LEN     65
#define DMA_TRIAD_ID_LEN        32
#define DMA_SECRET_LEN          32

#define DMA_LEGAL_CHAR(c)       ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))

static u16 dma_get_one_info(const u8 *in, u8 *out)
{
    int read_len = 0;
    const u8 *p = in;

    while (DMA_LEGAL_CHAR(*p) && *p != ',') { //read Product ID
        *out++ = *p++;
        read_len++;
    }
    return read_len;
}

extern const u8 *multi_protocol_get_license_ptr(void);
u8 read_dma_product_info_from_flash(u8 *read_buf, u16 buflen)
{
    u8 *rp = read_buf;
    // const u8 *dma_ptr = NULL;
    const u8 *dma_ptr = (u8 *)multi_protocol_get_license_ptr();

    if (dma_ptr == NULL) {
        return FALSE;
    }

    if (dma_get_one_info(dma_ptr, rp) != 32) {
        return FALSE;
    }
    dma_ptr += 33;

    rp = read_buf + DMA_PRODUCT_ID_LEN;
    memcpy(rp, dma_product_key, strlen(dma_product_key));

    rp = read_buf + DMA_PRODUCT_ID_LEN + DMA_PRODUCT_KEY_LEN;
    if (dma_get_one_info(dma_ptr, rp) != 24) {
        return FALSE;
    }
    dma_ptr += 25;

    rp = read_buf + DMA_PRODUCT_ID_LEN + DMA_PRODUCT_KEY_LEN + DMA_TRIAD_ID_LEN;
    if (dma_get_one_info(dma_ptr, rp) != 16) {
        return FALSE;
    }

    return TRUE;
}

__attribute__((weak))
void bt_update_testbox_addr(u8 *addr)
{

}


#if DMA_PRODUCT_INFO_TEST
int get_vender_special_SN(char *serial_number)
{
    memcpy(serial_number, dma_test_sn, strlen(dma_test_sn));
    return 7;
}
#endif


bool dueros_dma_get_manufacturer_info(u8 *read_buf, u16 len)
{
    bool ret = FALSE;

    APP_DMA_LOG("%s\n", __func__);

#if DMA_PRODUCT_INFO_TEST
    memcpy(read_buf, dma_product_id, strlen(dma_product_id));
    memcpy(read_buf + DMA_PRODUCT_ID_LEN, dma_product_key, strlen(dma_product_key));
    memcpy(read_buf + DMA_PRODUCT_ID_LEN + DMA_PRODUCT_KEY_LEN, dma_triad_id, strlen(dma_triad_id));
    memcpy(read_buf + DMA_PRODUCT_ID_LEN + DMA_PRODUCT_KEY_LEN + DMA_TRIAD_ID_LEN, dma_secret, strlen(dma_secret));
    ret = TRUE;
#else
    ret = read_dma_product_info_from_flash(read_buf, sizeof(read_buf));
#endif
    return ret;
}

static bool dma_tws_get_manufacturer_info(u8 *read_buf, u16 len)
{
    u16 i = 0;
    bool ret = FALSE;

    // if (tws_api_get_local_channel() == DMA_PRODUCT_INFO_TWS_CH) {
    // return ret;
    // }
    if (syscfg_read(VM_DMA_REMOTE_LIC, read_buf, len) == len) {
        for (i = 0; i < len; i++) {
            if (read_buf[i] != 0xff) {
                ret = TRUE;
                break;
            }
        }
    }
    return ret;
}

u8 dma_get_hex(u8 cha)
{
    u8 num = 0;
    if (cha >= '0' && cha <= '9') {
        num = cha - '0';
    } else if (cha >= 'a' && cha <= 'f') {
        num = cha - 'a' + 10;
    } else if (cha >= 'A' && cha <= 'F') {
        num = cha - 'A' + 10;
    }
    return num;
}

extern u32 multi_protocol_read_cfg_file(void *buf, u16 len, char *path);
u8 dueros_dma_read_from_file(u8 *mac, u8 *read_buf, u16 len)
{
    u8 i = 0;
    u8 mac_tmp[6];
    u8 rbuf[256];
    u16 offset = 0;
    u8 *rp = read_buf;
    u8 *dma_ptr;
    u8 product_key[32];
    APP_DMA_LOG("read license from file\n");
    if (multi_protocol_read_cfg_file(rbuf, sizeof(rbuf), SDFILE_RES_ROOT_PATH"dma.txt") == TRUE) {
        printf("%s", rbuf);
        for (i = 0; i < sizeof(mac_tmp); i++) {
            mac_tmp[i] = (dma_get_hex(rbuf[offset]) << 4) | dma_get_hex(rbuf[offset + 1]);
            offset += 3;
        }
        put_buf(mac_tmp, sizeof(mac_tmp));
        dma_ptr = rbuf + offset;

        if (dma_get_one_info(dma_ptr, product_key) != 32) {
            return FALSE;
        }
        dma_ptr += 33;

        if (dma_get_one_info(dma_ptr, rp) != 32) {
            return FALSE;
        }
        dma_ptr += 33;

        rp = read_buf + DMA_PRODUCT_ID_LEN;
        memcpy(rp, product_key, sizeof(product_key));

        rp = read_buf + DMA_PRODUCT_ID_LEN + DMA_PRODUCT_KEY_LEN;
        if (dma_get_one_info(dma_ptr, rp) != 24) {
            return FALSE;
        }
        dma_ptr += 25;

        rp = read_buf + DMA_PRODUCT_ID_LEN + DMA_PRODUCT_KEY_LEN + DMA_TRIAD_ID_LEN;
        if (dma_get_one_info(dma_ptr, rp) != 16) {
            return FALSE;
        }
        memcpy(mac, mac_tmp, 6);
        return TRUE;
    }
    return FALSE;
}

void dma_set_local_version(u32 version);
void dueros_dma_manufacturer_info_init()
{
    dma_set_local_version(APP_DMA_VERSION);
#if DMA_PRODUCT_INFO_EN
    u8 *mac = dma_test_mac;

    u8 read_buf[DMA_PRODUCT_ID_LEN + DMA_PRODUCT_KEY_LEN + DMA_TRIAD_ID_LEN + DMA_SECRET_LEN + 1] = {0};
    bool ret = FALSE;

    APP_DMA_LOG("dueros_dma_manufacturer_info_init\n");

    ret = dma_tws_get_manufacturer_info(read_buf, sizeof(read_buf));
    if (ret == FALSE) {
        ret = dueros_dma_get_manufacturer_info(read_buf, sizeof(read_buf));
        if (ret == true) {
            lic_data_type = LIC_IN_FLASH;
        }
    } else {
        lic_data_type = LIC_IN_VM;
    }
#if DMA_PRODUCT_READ_FROM_FILE
    if (ret == FALSE) {
        ret = dueros_dma_read_from_file(mac, read_buf, sizeof(read_buf));
    }
#endif
    if (ret == TRUE) {
        APP_DMA_LOG("read license success\n");
        APP_DMA_LOG("product id: %s\n", read_buf);
        APP_DMA_LOG("product key: %s\n", read_buf + DMA_PRODUCT_ID_LEN);
        APP_DMA_LOG("triad id: %s\n", read_buf + DMA_PRODUCT_ID_LEN + DMA_PRODUCT_KEY_LEN);
        APP_DMA_LOG("secret: %s\n", read_buf + DMA_PRODUCT_ID_LEN + DMA_PRODUCT_KEY_LEN + DMA_TRIAD_ID_LEN);
        u8 pid_len = strlen((const char *)read_buf);
        u8 key_len = strlen((const char *)read_buf + DMA_PRODUCT_ID_LEN);
        u8 tid_len = strlen((const char *)read_buf + DMA_PRODUCT_ID_LEN + DMA_PRODUCT_KEY_LEN);
        u8 sec_len = strlen((const char *)read_buf + DMA_PRODUCT_ID_LEN + DMA_PRODUCT_KEY_LEN + DMA_TRIAD_ID_LEN);
        lic_data_len = pid_len + key_len + tid_len + sec_len + 4;
        if (lic_data == NULL) {
            lic_data = zalloc(lic_data_len);
        }
        if (lic_data) {
            memcpy(lic_data, read_buf, pid_len);
            memcpy(lic_data + 1 + pid_len, read_buf + DMA_PRODUCT_ID_LEN, key_len);
            memcpy(lic_data + 2 + pid_len + key_len, read_buf + DMA_PRODUCT_ID_LEN + DMA_PRODUCT_KEY_LEN, tid_len);
            memcpy(lic_data + 3 + pid_len + key_len + tid_len, read_buf + DMA_PRODUCT_ID_LEN + DMA_PRODUCT_KEY_LEN + DMA_TRIAD_ID_LEN, sec_len);
        }
        dma_set_product_id_key(read_buf);
    } else {
        dma_set_product_id_key(NULL);
    }

#if DMA_PRODUCT_INFO_TEST
    u8 ble_mac[6];
    void bt_update_mac_addr(u8 * addr);
    void lmp_hci_write_local_address(const u8 * addr);
    void bt_update_testbox_addr(u8 * addr);
    // extern int le_controller_set_mac(void *addr);
    //extern void bt_make_ble_address(u8 * ble_address, u8 * edr_address);
    bt_update_mac_addr(mac);
    lmp_hci_write_local_address(mac);
    bt_update_testbox_addr(mac);
    // bt_make_ble_address(ble_mac, mac);
    memcpy(ble_mac, mac, 6);
    dma_ble_set_mac_addr(mac); //修改BLE地址
    APP_DMA_DUMP(mac, 6);
    APP_DMA_DUMP(ble_mac, 6);
#endif
#else
    u8 read_buf[DMA_PRODUCT_ID_LEN + DMA_PRODUCT_KEY_LEN + DMA_TRIAD_ID_LEN + DMA_SECRET_LEN + 1] = {0};
    APP_DMA_LOG("dueros_dma_manufacturer_info_init\n");
    dueros_dma_get_manufacturer_info(read_buf, sizeof(read_buf));
    APP_DMA_LOG("read license success\n");
    APP_DMA_LOG("product id: %s\n", read_buf);
    APP_DMA_LOG("product key: %s\n", read_buf + DMA_PRODUCT_ID_LEN);
    dma_set_product_id_key(read_buf);
#endif
}

//*********************************************************************************//
//                                 DMA私有消息处理                                 //
//*********************************************************************************//
#if TCFG_USER_TWS_ENABLE
//固定使用左耳的三元组

static void dma_tws_conn_sync_lic()
{
    u8 read_buf[DMA_PRODUCT_ID_LEN + DMA_PRODUCT_KEY_LEN + DMA_TRIAD_ID_LEN + DMA_SECRET_LEN + 1] = {0};
    bool ret = FALSE;
    // if (tws_api_get_local_channel() == DMA_PRODUCT_INFO_TWS_CH) {
    ret = dueros_dma_get_manufacturer_info(read_buf, sizeof(read_buf));
    // }
    if (ret) {
        APP_DMA_LOG("dueros_dma start sync manufacturer info\n");
        dma_protocol_tws_send_to_sibling(DMA_TWS_CMD_SYNC_LIC, read_buf, sizeof(read_buf));
    }
}
#endif

static void dma_tws_rx_license_deal(u8 *license, u16 len)
{
    printf("dma_tws_rx_license_deal\n");
    syscfg_write(VM_DMA_REMOTE_LIC, license, len);
    if (dma_is_tws_master_role()) {
        dma_set_product_id_key(license);
        // dma_ble_disconnect();
        dma_ble_adv_enable(0);
        if (0 == bt_get_esco_coder_busy_flag()) {
            dma_ble_adv_enable(1);
        }
    }
}

static void dma_update_ble_addr()
{
    u8 comm_addr[6];
    printf("%s\n", __func__);
    tws_api_get_local_addr(comm_addr);
    dma_ble_set_mac_addr(comm_addr); //地址发生变化，更新地址
    dma_ble_adv_enable(0);
    if (0 == bt_get_esco_coder_busy_flag()) {
        //esco在用的时候开广播会影响质量
        dma_ble_adv_enable(1);
    }
}

extern void dueros_dma_set_calling_asr_state(uint8_t en);
static int dma_protocol_bt_status_event_handler(struct bt_event *bt)
{
    switch (bt->event) {
    case BT_STATUS_INIT_OK:
        break;
    case BT_STATUS_SECOND_CONNECTED:
    case BT_STATUS_FIRST_CONNECTED:
    case BT_STATUS_THIRD_CONNECTED:
        break;
    case BT_STATUS_FIRST_DISCONNECT:
    case BT_STATUS_SECOND_DISCONNECT:
    case BT_STATUS_THIRD_DISCONNECT:
        dma_update_tws_state_to_lib(DMA_NOTIFY_STATE_MOBILE_DISCONNECTED);
        dueros_dma_set_calling_asr_state(0);
        break;
    case BT_STATUS_PHONE_INCOME:
    case BT_STATUS_PHONE_OUT:
    case BT_STATUS_PHONE_ACTIVE:
        dma_app_speech_stop();
        dma_ble_adv_enable(0);
        dueros_dma_set_calling_asr_state(1);
        break;
    case BT_STATUS_PHONE_HANGUP:
        dma_ble_adv_enable(1);
        dueros_dma_set_calling_asr_state(0);
        break;

    case BT_STATUS_PHONE_MANUFACTURER:
        if (!dma_pair_state()) {
            if (bt_get_call_status() != BT_CALL_HANGUP) {
                dma_ble_adv_enable(0);
            }
        }
        break;
    case BT_STATUS_SIRI_OPEN:
    case BT_STATUS_SIRI_CLOSE:
    case BT_STATUS_SIRI_GET_STATE:
        /* case BT_STATUS_VOICE_RECOGNITION: */
        if ((app_var.siri_stu == 1) || (app_var.siri_stu == 2)) {
            dma_app_speech_stop();
            dma_ble_adv_enable(0);
        } else if (app_var.siri_stu == 0) {
            dma_ble_adv_enable(1);
        }
        break;
    case BT_STATUS_CONN_A2DP_CH:
        dma_update_tws_state_to_lib(DMA_NOTIFY_STATE_MOBILE_CONNECTED);
        break;
    }
    return 0;
}

static int dma_app_status_event_handler(int *msg)
{
    struct bt_event *bt = (struct bt_event *)msg;
    if (tws_api_get_role() == TWS_ROLE_SLAVE) {
        return 0;
    }
    dma_protocol_bt_status_event_handler(bt);
    return 0;
}
APP_MSG_HANDLER(dma_protocol_msg_handler) = {
    .owner      = 0xff,
    .from       = MSG_FROM_BT_STACK,
    .handler    = dma_app_status_event_handler,
};

#if TCFG_USER_TWS_ENABLE

extern void dma_ota_peer_recieve_data(u8 *buf, u16 len);
extern void dma_peer_recieve_data(u8 *buf, u16 len);
static void dma_rx_tws_data_deal(u16 opcode, u8 *data, u16 len)
{
    switch (opcode) {
#if DMA_PRODUCT_INFO_TWS_SYNC
    case DMA_TWS_CMD_SYNC_LIC:
        APP_DMA_LOG(">>> DMA_TWS_CMD_SYNC_LIC \n");
        if (lic_data_type != LIC_IN_FLASH || tws_api_get_role() == TWS_ROLE_SLAVE) {
            //没有三元组则用对耳的三元组，或者从机有三元组则以主机的为准
            dma_tws_rx_license_deal(data, len);
        }
        break;
#endif
    case DMA_TWS_LIB_INFO_SYNC:
        dma_peer_recieve_data(data, len);
        break;
    case DMA_TWS_TONE_INFO_SYNC:
        int index = -1;
        memcpy(&index, data, len);
        printf("slave sync tone index:%d", index);
        //dma_tone_status_update(index, 1);
        break;
    case DMA_TWS_OTA_CMD_SYNC:
        dma_ota_peer_recieve_data(data, len);
        break;
    case DMA_TWS_OTA_INFO_SYNC:
        printf("APP_PROTOCOL_DMA_SAVE_OTA_INFO");
        put_buf(data, len);
        syscfg_write(VM_TME_AUTH_COOKIE, data, len);
        break;
    }
}

static void __tws_rx_from_sibling(u8 *data)
{
    u16 opcode = (data[1] << 8) | data[0];
    u16 len = (data[3] << 8) | data[2];
    u8 *rx_data = data + 4;
    switch (opcode) {
    case DMA_TWS_FOR_LIB_SYNC:
        printf("DMA_TWS_FOR_LIB_SYNC");
        dma_tws_data_deal(rx_data, len);
        break;
    case DMA_TWS_PAIR_STATE_SYNC:
        printf("DMA_TWS_PAIR_STATE_SYNC");
        set_dueros_pair_state_2(rx_data[0]);
        break;
    default:
        dma_rx_tws_data_deal(opcode, rx_data, len);
        break;
    }
    free(data);
}

static void dma_protocol_rx_from_sibling(void *_data, u16 len, bool rx)
{
    int err = 0;
    if (rx) {
        printf(">>>%s \n", __func__);
        printf("len :%d\n", len);
        put_buf(_data, len);
        u8 *rx_data = malloc(len);
        memcpy(rx_data, _data, len);

        int msg[4];
        msg[0] = (int)__tws_rx_from_sibling;
        msg[1] = 1;
        msg[2] = (int)rx_data;
        err = os_taskq_post_type("app_core", Q_CALLBACK, 3, msg);
        if (err) {
            printf("tws rx post fail\n");
        }
    }
}

#define DMA_PROTOCOL_TWS_SEND_ID    \
	(((u8)('D' + 'M' + 'A') << (3 * 8)) | \
	 ((u8)('P' + 'R' + 'T' + 'C') << (2 * 8)) | \
	 ((u8)('T' + 'W' + 'S') << (1 * 8)) | \
	 ((u8)('S' + 'E' + 'N' + 'D') << (0 * 8)))

//发送给对耳
REGISTER_TWS_FUNC_STUB(dma_app_tws_rx_from_sibling) = {
    .func_id = DMA_PROTOCOL_TWS_SEND_ID,
    .func    = dma_protocol_rx_from_sibling,
};

int dma_protocol_tws_send_to_sibling(u16 opcode, u8 *data, u16 len)
{
    u8 send_data[len + 4];
    printf("app protocol send data to sibling \n");
    send_data[0] = opcode & 0xff;
    send_data[1] = opcode >> 8;
    send_data[2] = len & 0xff;
    send_data[3] = len >> 8;
    memcpy(send_data + 4, data, len);
    return tws_api_send_data_to_sibling(send_data, sizeof(send_data), DMA_PROTOCOL_TWS_SEND_ID);
}

static void dma_protocol_bt_tws_event_handler(struct tws_event *tws)
{
    int role = tws->args[0];
    int phone_link_connection = tws->args[1];
    int reason = tws->args[2];
    u8 phone_addr[6];
    u8 btaddr[6];

    switch (tws->event) {
    case TWS_EVENT_CONNECTED:
        if (!dma_is_tws_master_role()) {
            dma_ble_adv_enable(0);
        }
        dma_update_tws_state_to_lib(DMA_NOTIFY_STATE_TWS_CONNECT);
        dma_update_ble_addr();
#if DMA_PRODUCT_INFO_TWS_SYNC
        dma_tws_conn_sync_lic();
#endif
        dma_tws_sync_pair_state();
        break;
    case TWS_EVENT_CONNECTION_DETACH:
        dma_update_tws_state_to_lib(DMA_NOTIFY_STATE_BATTERY_LEVEL_UPDATE); //主动上报电量
        dma_update_tws_state_to_lib(DMA_NOTIFY_STATE_TWS_DISCONNECT);
        break;
    case TWS_EVENT_ROLE_SWITCH:
        dma_update_tws_state_to_lib(DMA_NOTIFY_STATE_ROLE_SWITCH_FINISH);
        if (role == TWS_ROLE_MASTER && (0 == bt_get_esco_coder_busy_flag())) {
            dma_ble_adv_enable(1);
        } else {
            dma_ble_adv_enable(0);
        }
        break;
    }
}

static int bt_dma_tws_status_event_handler(int *msg)
{
    struct tws_event *evt = (struct tws_event *)msg;
    dma_protocol_bt_tws_event_handler(evt);
    return 0;
}
APP_MSG_HANDLER(bt_dma_tws_msg_handler_stub) = {
    .owner      = 0xff,
    .from       = MSG_FROM_TWS,
    .handler    = bt_dma_tws_status_event_handler,
};

#else
int dma_protocol_tws_send_to_sibling(u16 opcode, u8 *data, u16 len)
{
    return 0;
}
#endif

void dma_protocol_tws_sync_pair_state(u8 state)
{
    dma_protocol_tws_send_to_sibling(DMA_TWS_PAIR_STATE_SYNC, &state, 1);
}

extern u32 lmp_private_get_tx_remain_buffer();
void platform_ota_send_custom_info_to_peer(uint16_t param_size, uint8_t *param_buf)
{
    u8 retry_cnt = 0;
    int error_code = 0;
    if (!(tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
        putchar('r');
        return;
    }
__try_again:
    error_code = dma_protocol_tws_send_to_sibling(DMA_TWS_OTA_CMD_SYNC, param_buf, param_size);
    printf("%s--%d , error %d\n", __func__, param_size, error_code);
    printf("OTA remain_buffer = 0x%x", lmp_private_get_tx_remain_buffer());
    if (error_code != 0) {
        os_time_dly(5);
        retry_cnt ++;

        if (retry_cnt > 35) {
            printf(">>>>>>>>>>tws send slave faile");
            //bt_tws_dma_ota_fail();
            return;
        }
        goto __try_again;
    }
}

static int dma_app_special_message(int id, int opcode, u8 *data, u32 len)
{
    int ret = 0;
    switch (opcode) {
    case APP_PROTOCOL_DMA_SAVE_RAND:
        APP_DMA_LOG("APP_PROTOCOL_DMA_SAVE_RAND");
        //APP_DMA_DUMP(data, len);
        ret = syscfg_write(VM_DMA_RAND, data, len);
        // dma_protocol_tws_send_to_sibling(DMA_TWS_ALL_INFO_SYNC, data, len);
        break;
    case APP_PROTOCOL_DMA_READ_RAND:
        APP_DMA_LOG("APP_PROTOCOL_DMA_READ_RAND");
        ret = syscfg_read(VM_DMA_RAND, data, len);
        //APP_DMA_DUMP(data, len);
        break;
    case APP_PROTOCOL_DMA_SAVE_OTA_INFO:
        APP_DMA_LOG("APP_PROTOCOL_DMA_SAVE_OTA_INFO");
        APP_DMA_DUMP(data, len);
        //不支持断点续传
        /* ret = syscfg_write(VM_TME_AUTH_COOKIE, data, len); */
        /* dma_protocol_tws_send_to_sibling(DMA_TWS_OTA_INFO_SYNC, data, len); */
        break;
    case APP_PROTOCOL_DMA_READ_OTA_INFO:
        APP_DMA_LOG("APP_PROTOCOL_DMA_READ_OTA_INFO");
        //ret = syscfg_read(VM_TME_AUTH_COOKIE, data, len);
        //不支持断点续传
        memset(data, 0, len);
        APP_DMA_DUMP(data, len);
        break;
    case APP_PROTOCOL_DMA_TTS_TYPE:
        printf("app_protocol_dma_tone_play %d\n", data[0]);
        //app_protocol_dma_tone_play(data[0], 1);
        break;
    case APP_PROTOCOL_SPEECH_START:
        APP_DMA_LOG("APP_PROTOCOL_SPEECH_START");
        dma_app_speech_start();
        break;
    case APP_PROTOCOL_SPEECH_STOP:
        APP_DMA_LOG("APP_PROTOCOL_SPEECH_STOP");
        dma_app_speech_stop();
        break;
    case APP_PROTOCOL_LIB_TWS_DATA_SYNC:
        dma_protocol_tws_send_to_sibling(DMA_TWS_FOR_LIB_SYNC, data, len);
        break;
    }
    return ret;
}

int dma_app_check_status(int status_flag)
{
#if TCFG_USER_TWS_ENABLE
    switch (status_flag) {
    case CHECK_STATUS_TWS_MASTER:
        return dma_is_tws_master_role();
    case CHECK_STATUS_TWS_SLAVE:
        return !dma_is_tws_master_role();
    case CHECK_STATUS_TWS_PAIR_STA: /*1是tws已经配对了，0是未配对*/
        if (get_bt_tws_connect_status()) {
            return true;
        } else {
            return false;
        }
    case CHECK_STATUS_TWS_SIDE:     /*0是单耳，1是左耳，2是右耳*/
        return dma_get_tws_side();
    }
#endif
    return 1;
}

extern bool vbat_is_low_power(void);
static u8 app_protocal_get_bat_by_type(u8 type)
{
    u8 value = 0;
    extern u8 get_tws_sibling_bat_persent(void);
    u8 sibling_val = 0xff;
#if TCFG_USER_TWS_ENABLE
    extern u8 get_tws_sibling_bat_persent(void);
    sibling_val = get_tws_sibling_bat_persent();
#endif
#if TCFG_CHARGESTORE_ENABLE
    if (sibling_val == 0xff) {
        sibling_val = chargestore_get_sibling_power_level();
    }
#endif
    switch (type) {
    case APP_PROTOCOL_BAT_T_CHARGE_FLAG:
        value = get_charge_online_flag();
        break;
    case APP_PROTOCOL_BAT_T_MAIN:
        value = get_vbat_percent();
        break;
#if TCFG_CHARGESTORE_ENABLE
    case APP_PROTOCOL_BAT_T_BOX:
        value = chargestore_get_power_level();
        break;
#endif
#if TCFG_USER_TWS_ENABLE
    case APP_PROTOCOL_BAT_T_TWS_LEFT:
        value = (tws_api_get_local_channel() != 'R') ? get_vbat_percent() : sibling_val;
        break;
    case APP_PROTOCOL_BAT_T_TWS_RIGHT:
        value = (tws_api_get_local_channel() != 'L') ? get_vbat_percent() : sibling_val;
        break;
#endif
    case APP_PROTOCOL_BAT_T_TWS_SIBLING:
        value = sibling_val;
        break;
    case APP_PROTOCOL_BAT_T_LOW_POWER:
        value = vbat_is_low_power();
        break;
    default:
        break;
    }
    if (value == 0xff) { //获取不到电量返回0
        value = 0;
    }
    printf("%s %d %d\n", __func__, type, value);
    return value;
}

bool dma_get_battery(u8 type, u8 *value)
{
    for (int i = 0; i < APP_PROTOCOL_BAT_T_MAX; i++) {
        if (type & BIT(i)) {
            value[i] = app_protocal_get_bat_by_type(i);
        }
    }
    return 0;
}

#define APP_PROTOCOL_MIC_TIMEOUT    (10000)
static u16 mic_timer = 0;
void __dma_app_speech_stop(void)
{
    if (ai_mic_is_busy()) {
        APP_DMA_LOG("app_protocol_speech_stop\n");
        ai_mic_rec_close();
        // send stop_speech_cmd;
    }
}
static void app_protocol_speech_timeout(void *priv)
{
    mic_timer = 0;
    printf(" speech timeout !!! \n");
    __dma_app_speech_stop();
    dma_start_voice_recognition(0);
}

void dma_speech_timeout_start(void)
{
#if APP_PROTOCOL_MIC_TIMEOUT
    if (mic_timer == 0) {
        mic_timer = sys_timeout_add(NULL, app_protocol_speech_timeout, APP_PROTOCOL_MIC_TIMEOUT);
    }
#endif
}

int dma_app_speech_start(void)
{
    if (strcmp(os_current_task(), "app_core")) {
        printf("%s, task:%s", __func__, os_current_task());
        int msg[] = {(int)dma_app_speech_start, 0};
        os_taskq_post_type("app_core", Q_CALLBACK, ARRAY_SIZE(msg), msg);
        return 0;
    }
    if (!dma_is_tws_master_role()) {
        return -1;
    }
    // if (BT_STATUS_TAKEING_PHONE == bt_get_connect_status()) {
    //     APP_DMA_LOG("phone ing...\n");
    //     return -1;
    // }
    if (ai_mic_is_busy()) {
        APP_DMA_LOG("mic activing...\n");
        return -1;
    }

    ai_mic_rec_start();

    return 0;
}

void dma_app_speech_stop(void)
{
    if (strcmp(os_current_task(), "app_core")) {
        printf("%s, task:%s", __func__, os_current_task());
        int msg[] = {(int)dma_app_speech_stop, 0};
        os_taskq_post_type("app_core", Q_CALLBACK, ARRAY_SIZE(msg), msg);
        return;
    }
    if (ai_mic_is_busy()) {
        APP_DMA_LOG("app_protocol_speech_stop\n");
        ai_mic_rec_close();
        // send stop_speech_cmd;
    }
    if (mic_timer) {
        sys_timeout_del(mic_timer);
        mic_timer = 0;
    }
}

extern void dma_send_process_resume(void);
u16 dma_app_speech_data_send(u8 *buf, u16 len)
{
    printf("dma_app_speech_data_send %d\n", len);
    u16 ret = dma_speech_data_send(buf, len);
    dma_send_process_resume();
    return ret;
}

uint8_t *custom_dma_get_state_callback(uint32_t profile, char **ext)
{
    printf("custom_dma_get_state_callback 0x%x\n", profile);
    // ret format
    // Byte 0 : value case
    // STATE__VALUE_BOOLEAN = 2
    // STATE__VALUE_INTEGER = 3
    // STATE__VALUE_STR     = 4
    // Byte 1~n: value
    return NULL;
}

int custom_dma_set_state_callback(uint32_t profile, int value_case, uint8_t *value, uint8_t *ext)
{
    printf("custom_dma_set_state_callback 0x%x %d\n", profile, value_case);
    // ret format
    // value case
    // STATE__VALUE_BOOLEAN = 2
    // STATE__VALUE_INTEGER = 3
    // STATE__VALUE_STR     = 4
    return 0;
}

int dma_protocol_all_init(void)
{
    printf(">>>>>>>>>>>>>>>>>>> dma_protocol_init");
    dma_message_callback_register(dma_app_special_message);
    dma_check_status_callback_register(dma_app_check_status);
    dma_get_battery_callback_register(dma_get_battery);
    //dma_user_set_dev_info(&dma_device_info);
    dueros_dma_manufacturer_info_init();
    dma_all_init();

    mic_rec_pram_init(AUDIO_CODING_OPUS, 0, dma_app_speech_data_send, 4, 1024);

    dma_ble_adv_enable(0);
    dma_ble_adv_enable(1);
    return 0;
}

int dma_protocol_all_exit(void)
{
    dma_all_exit();
    return 0;
}

int get_lic_data_len(void)
{
    return lic_data_len;
}

u8 get_lic_data_type(void)
{
    return lic_data_type;
}

int get_lic_data(u8 *buff)
{
    if (lic_data) {
        memcpy(buff, lic_data, lic_data_len);
        return 0;
    }
    return -1;
}

int set_lic_data(u8 *buff, u16 len)
{
    if (lic_data == NULL) {
        lic_data = zalloc(len);
    }
    lic_data_len = len;
    lic_data_type = LIC_IN_VM;
    if (lic_data) {
        memcpy(lic_data, buff, len);
        put_buf(lic_data, lic_data_len);
        u8 read_buf[DMA_PRODUCT_ID_LEN + DMA_PRODUCT_KEY_LEN + DMA_TRIAD_ID_LEN + DMA_SECRET_LEN + 1] = {0};
        u8 pid_len = strlen((const char *)lic_data);
        u8 key_len = strlen((const char *)(lic_data + pid_len + 1));
        u8 tid_len = strlen((const char *)(lic_data + pid_len + key_len + 2));
        u8 sec_len = strlen((const char *)(lic_data + pid_len + key_len + tid_len + 3));
        memcpy(read_buf, lic_data, pid_len);
        memcpy(read_buf + DMA_PRODUCT_ID_LEN, lic_data + pid_len + 1, key_len);
        memcpy(read_buf + DMA_PRODUCT_ID_LEN + DMA_PRODUCT_KEY_LEN, lic_data + pid_len + key_len + 2, tid_len);
        memcpy(read_buf + DMA_PRODUCT_ID_LEN + DMA_PRODUCT_KEY_LEN + DMA_TRIAD_ID_LEN, lic_data + pid_len + key_len + tid_len + 3, sec_len);
        dma_tws_rx_license_deal(read_buf, sizeof(read_buf));
        return len;
    }
    return len;
}

#endif

