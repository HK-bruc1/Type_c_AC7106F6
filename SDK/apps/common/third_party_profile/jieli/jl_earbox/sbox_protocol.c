#include "app_config.h"
#include "app_msg.h"
#include "earphone.h"
#include "bt_tws.h"
#include "app_main.h"
#include "btstack/avctp_user.h"
#include "multi_protocol_main.h"
#include "sbox_core_config.h"
#include "sbox_protocol.h"
#include "sbox_user_app.h"

#if (THIRD_PARTY_PROTOCOLS_SEL & JL_SBOX_EN)

void *sbox_demo_ble_hdl = NULL;
void *sbox_demo_ble_hdl2 = NULL;
void *sbox_demo_spp_hdl = NULL;

extern u16 ble_att_server_get_link_mtu(u16 con_handle);
extern void att_server_set_exchange_mtu(u16 con_handle);

/*************************************************
                  BLE 相关内容
*************************************************/

const uint8_t sbox_profile_data[] = {
    //////////////////////////////////////////////////////
    //
    // 0x0001 PRIMARY_SERVICE  1800
    //
    //////////////////////////////////////////////////////
    0x0a, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x28, 0x00, 0x18,

    /* CHARACTERISTIC,  2a00, READ | WRITE | DYNAMIC, */
    // 0x0002 CHARACTERISTIC 2a00 READ | WRITE | DYNAMIC
    0x0d, 0x00, 0x02, 0x00, 0x02, 0x00, 0x03, 0x28, 0x0a, 0x03, 0x00, 0x00, 0x2a,
    // 0x0003 VALUE 2a00 READ | WRITE | DYNAMIC
    0x08, 0x00, 0x0a, 0x01, 0x03, 0x00, 0x00, 0x2a,

    //////////////////////////////////////////////////////
    //
    // 0x0004 PRIMARY_SERVICE  ae00
    //
    //////////////////////////////////////////////////////
    0x0a, 0x00, 0x02, 0x00, 0x04, 0x00, 0x00, 0x28, 0x00, 0xae,

    /* CHARACTERISTIC,  ae01, WRITE_WITHOUT_RESPONSE | DYNAMIC, */
    // 0x0005 CHARACTERISTIC ae01 WRITE_WITHOUT_RESPONSE | DYNAMIC
    0x0d, 0x00, 0x02, 0x00, 0x05, 0x00, 0x03, 0x28, 0x04, 0x06, 0x00, 0x01, 0xae,
    // 0x0006 VALUE ae01 WRITE_WITHOUT_RESPONSE | DYNAMIC
    0x08, 0x00, 0x04, 0x01, 0x06, 0x00, 0x01, 0xae,

    /* CHARACTERISTIC,  ae02, NOTIFY, */
    // 0x0007 CHARACTERISTIC ae02 NOTIFY
    0x0d, 0x00, 0x02, 0x00, 0x07, 0x00, 0x03, 0x28, 0x10, 0x08, 0x00, 0x02, 0xae,
    // 0x0008 VALUE ae02 NOTIFY
    0x08, 0x00, 0x10, 0x00, 0x08, 0x00, 0x02, 0xae,
    // 0x0009 CLIENT_CHARACTERISTIC_CONFIGURATION
    0x0a, 0x00, 0x0a, 0x01, 0x09, 0x00, 0x02, 0x29, 0x00, 0x00,

    /* CHARACTERISTIC,  ae98, WRITE_WITHOUT_RESPONSE | DYNAMIC, */
    // 0x000a CHARACTERISTIC ae98 WRITE_WITHOUT_RESPONSE | DYNAMIC
    0x0d, 0x00, 0x02, 0x00, 0x0a, 0x00, 0x03, 0x28, 0x04, 0x0b, 0x00, 0x98, 0xae,
    // 0x000b VALUE ae98 WRITE_WITHOUT_RESPONSE | DYNAMIC
    0x08, 0x00, 0x04, 0x01, 0x0b, 0x00, 0x98, 0xae,

    /* CHARACTERISTIC,  ae99, NOTIFY, */
    // 0x000c CHARACTERISTIC ae99 NOTIFY
    0x0d, 0x00, 0x02, 0x00, 0x0c, 0x00, 0x03, 0x28, 0x10, 0x0d, 0x00, 0x99, 0xae,
    // 0x000d VALUE ae99 NOTIFY
    0x08, 0x00, 0x10, 0x00, 0x0d, 0x00, 0x99, 0xae,
    // 0x000e CLIENT_CHARACTERISTIC_CONFIGURATION
    0x0a, 0x00, 0x0a, 0x01, 0x0e, 0x00, 0x02, 0x29, 0x00, 0x00,

    // END
    0x00, 0x00,
};

//
// characteristics <--> handles
//
#define ATT_CHARACTERISTIC_2a00_01_VALUE_HANDLE 0x0003
#define ATT_CHARACTERISTIC_ae01_01_VALUE_HANDLE 0x0006
#define ATT_CHARACTERISTIC_ae02_01_VALUE_HANDLE 0x0008
#define ATT_CHARACTERISTIC_ae02_01_CLIENT_CONFIGURATION_HANDLE 0x0009
#define ATT_CHARACTERISTIC_ae98_01_VALUE_HANDLE 0x000b
#define ATT_CHARACTERISTIC_ae99_01_VALUE_HANDLE 0x000d
#define ATT_CHARACTERISTIC_ae99_01_CLIENT_CONFIGURATION_HANDLE 0x000e

static u16 sbox_adv_interval_min = 150;
static const uint8_t connection_update_enable = 1; ///0--disable, 1--enable
static uint8_t connection_update_cnt = 0; //
static const struct conn_update_param_t connection_param_table[] = {
    {264, 264, 3, 600},
    {12, 28, 14, 600},//11
    {8,  20, 20, 600},//3.7
    /* {12, 28, 4, 600},//3.7 */
    /* {12, 24, 30, 600},//3.05 */
};

static const struct conn_update_param_t connection_param_table_update[] = {
    {96, 120, 0,  600},
    {60,  80, 0,  600},
    {60,  80, 0,  600},
    /* {8,   20, 0,  600}, */
    {6, 12, 0, 400},/*ios 提速*/
};

static u8 conn_update_param_index = 0;
#define CONN_PARAM_TABLE_CNT      (sizeof(connection_param_table)/sizeof(struct conn_update_param_t))
#define CONN_TRY_NUM			  10 // 重复尝试次数


static void send_request_connect_parameter(hci_con_handle_t connection_handle, u8 table_index)
{
    struct conn_update_param_t *param = NULL; //static ram
    switch (conn_update_param_index) {
    case 0:
        param = (void *)&connection_param_table[table_index];
        break;
    case 1:
        param = (void *)&connection_param_table_update[table_index];
        break;
    default:
        break;
    }

    printf("update_request:-%d-%d-%d-%d-\n", param->interval_min, param->interval_max, param->latency, param->timeout);
    if (connection_handle) {
        ble_user_cmd_prepare(BLE_CMD_REQ_CONN_PARAM_UPDATE, 2, connection_handle, param);
    }
}

static void check_connetion_updata_deal(hci_con_handle_t connection_handle)
{
    //cppcheck-suppress knownConditionTrueFalse
#if (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN))
    extern u8 le_audio_get_adv_conn_success();
    //le audio 连接之后不允许其他位置更新参数
    if (connection_update_enable && le_audio_get_adv_conn_success()) {
#else
    if (connection_update_enable) {
#endif
        if (connection_update_cnt < (CONN_PARAM_TABLE_CNT * CONN_TRY_NUM)) {
            send_request_connect_parameter(connection_handle, connection_update_cnt / CONN_TRY_NUM);
        }
    }
}


static void connection_update_complete_success(u8 *packet)
{
#if 0
    int con_handle_t, conn_interval, conn_latency, conn_timeout;

    con_handle_t = hci_subevent_le_connection_update_complete_get_connection_handle(packet);
    conn_interval = hci_subevent_le_connection_update_complete_get_conn_interval(packet);
    conn_latency = hci_subevent_le_connection_update_complete_get_conn_latency(packet);
    conn_timeout = hci_subevent_le_connection_update_complete_get_supervision_timeout(packet);

    printf("conn_interval = %d\n", conn_interval);
    printf("conn_latency = %d\n", conn_latency);
    printf("conn_timeout = %d\n", conn_timeout);
#endif
}

int sbox_demo_adv_enable(u8 enable);
static void sbox_cbk_packet_handler(void *hdl, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    u16 con_handle;
    // printf("cbk packet_type:0x%x, packet[0]:0x%x, packet[2]:0x%x", packet_type, packet[0], packet[2]);
    switch (packet_type) {
    case HCI_EVENT_PACKET:
        switch (hci_event_packet_get_type(packet)) {
        case ATT_EVENT_CAN_SEND_NOW:
            printf("ATT_EVENT_CAN_SEND_NOW");
            break;

        case HCI_EVENT_LE_META:
            switch (hci_event_le_meta_get_subevent_code(packet)) {
            case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                con_handle = little_endian_read_16(packet, 4);
                printf("HCI_SUBEVENT_LE_CONNECTION_COMPLETE: %0x", con_handle);
                // reverse_bd_addr(&packet[8], addr);
                put_buf(&packet[8], 6);
                // sbox_cb_func.sbox_sync_all_info();
                break;
            default:
                break;
            }
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            printf("HCI_EVENT_DISCONNECTION_COMPLETE: %0x", packet[5]);
            sbox_demo_adv_enable(1);
            break;

        case L2CAP_EVENT_CONNECTION_PARAMETER_UPDATE_RESPONSE:
            u16 temp_con_hdl = little_endian_read_16(packet, 2);
            u32 tmp = little_endian_read_16(packet, 4);
            printf("-update_rsp: %02x\n", tmp);
            if (tmp) {
                connection_update_cnt++;
                printf("remoter reject!!!\n");
                check_connetion_updata_deal(temp_con_hdl);
            } else {
                connection_update_cnt = (CONN_PARAM_TABLE_CNT * CONN_TRY_NUM);
            }
            break;

        case HCI_EVENT_ENCRYPTION_CHANGE:
            u16 temp_con_handle = little_endian_read_16(packet, 3);

            printf("HCI_EVENT_ENCRYPTION_CHANGE= %d\n", packet[2]);
            if (packet[2] == 0) {
                /*ENCRYPTION is ok */
                check_connetion_updata_deal(temp_con_handle);
            }

            break;
        case HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE:
            u16 tmp16 = little_endian_read_16(packet, 4); // con_handle
            printf("conn_update_handle   = %04x\n", tmp16);
            printf("conn_update_interval = %d\n", hci_subevent_le_connection_update_complete_get_conn_interval(packet));
            printf("conn_update_latency  = %d\n", hci_subevent_le_connection_update_complete_get_conn_latency(packet));
            printf("conn_update_timeout  = %d\n", hci_subevent_le_connection_update_complete_get_supervision_timeout(packet));

            // fmna_connection_conn_param_update_handler(tmp16, little_endian_read_16(packet, 6 + 0));
            if (tmp16) {
                ble_op_set_ext_phy(tmp16, 0, CONN_SET_CODED_PHY, CONN_SET_CODED_PHY, CONN_SET_PHY_OPTIONS_S2);    // 向对端发起phy通道更改
            }

            break;
        case ATT_EVENT_MTU_EXCHANGE_COMPLETE:
            u16 mtu = att_event_mtu_exchange_complete_get_MTU(packet) - 3;
            printf("con_handle= %02x, ATT MTU = %u", little_endian_read_16(packet, 2), mtu);
            if (mtu > 517) {
                mtu = 23;
            }
            ble_vendor_set_default_att_mtu(mtu);
            sbox_cb_func.sbox_sync_all_info();
            break;
        default:
            break;
        }
        break;
    }
    return;
}

static uint16_t sbox_att_read_callback(void *hdl, hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t *buffer, uint16_t buffer_size)
{
    uint16_t  att_value_len = 0;
    uint16_t handle = att_handle;
    printf("<-------------read_callback, handle= 0x%04x,buffer= %08x", handle, (u32)buffer);
    switch (handle) {
    case ATT_CHARACTERISTIC_2a00_01_VALUE_HANDLE:
        const char *gap_name = bt_get_local_name();
        att_value_len = strlen(gap_name);
        if ((offset >= att_value_len) || (offset + buffer_size) > att_value_len) {
            break;
        }
        if (buffer) {
            memcpy(buffer, &gap_name[offset], buffer_size);
            att_value_len = buffer_size;
            printf("\n------read gap_name: %s", gap_name);
        }
        break;
    case ATT_CHARACTERISTIC_ae02_01_CLIENT_CONFIGURATION_HANDLE:
        if (buffer) {
            buffer[0] = att_get_ccc_config(handle);
            buffer[1] = 0;
        }
        att_value_len = 2;
        break;
    default:
        break;
    }
    printf("att_value_len= %d", att_value_len);
    return att_value_len;
    return 0;
}

static int sbox_att_write_callback(void *hdl, hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size)
{
    int result = 0;
    u16 tmp16;
    u16 handle = att_handle;
    printf("<-------------write_callback, handle= 0x%04x,size = %d", handle, buffer_size);
    switch (handle) {
    case ATT_CHARACTERISTIC_2a00_01_VALUE_HANDLE:
        break;
    case ATT_CHARACTERISTIC_ae01_01_VALUE_HANDLE:
        printf("rx(%d):\n", buffer_size);
        put_buf(buffer, buffer_size);
        // test
        // sbox_demo_ble_send(buffer, buffer_size);
        break;
    case ATT_CHARACTERISTIC_ae02_01_CLIENT_CONFIGURATION_HANDLE:
        printf("\nwrite ccc:%04x, %02x\n", handle, buffer[0]);
        att_set_ccc_config(handle, buffer[0]);
        break;
    case ATT_CHARACTERISTIC_ae98_01_VALUE_HANDLE:
        printf("rx(%d):\n", buffer_size);
        put_buf(buffer, buffer_size);
        sbox_att_write_without_rsp_handler(buffer, buffer_size);
        break;
    case ATT_CHARACTERISTIC_ae99_01_VALUE_HANDLE:

        break;
    case ATT_CHARACTERISTIC_ae99_01_CLIENT_CONFIGURATION_HANDLE:
        printf("\nwrite ccc:%04x, %02x\n", handle, buffer[0]);
        check_connetion_updata_deal(connection_handle);
        att_set_ccc_config(att_handle, buffer[0]);

        if (ble_att_server_get_link_mtu(connection_handle) == 23 && buffer[0] == 1) {
            att_server_set_exchange_mtu(connection_handle);
        }
        sbox_cb_func.sbox_sync_all_info();

        break;
    default:
        break;
    }
    return 0;
}

static u8 sbox_fill_adv_data(u8 *adv_data)
{
    u8 offset = 0;
    const char *name_p = bt_get_local_name();
    int name_len = strlen(name_p);
    offset += make_eir_packet_data(&adv_data[offset], offset, HCI_EIR_DATATYPE_COMPLETE_LOCAL_NAME, (void *)name_p, name_len);
    if (offset > ADV_RSP_PACKET_MAX) {
        puts("***rsp_data overflow!!!!!!\n");
        return 0;
    }
    return offset;
}

static u8 sbox_fill_rsp_data(u8 *rsp_data)
{
    return 0;
}


int sbox_demo_adv_enable(u8 enable)
{
    uint8_t adv_type = ADV_IND;
    uint8_t adv_channel = ADV_CHANNEL_ALL;
    uint8_t advData[ADV_RSP_PACKET_MAX] = {0};
    uint8_t rspData[ADV_RSP_PACKET_MAX] = {0};
    uint8_t len = 0;

    if (enable == app_ble_adv_state_get(sbox_demo_ble_hdl)) {
        return 0;
    }
    if (enable) {
        app_ble_set_adv_param(sbox_demo_ble_hdl, sbox_adv_interval_min, adv_type, adv_channel);
        len = sbox_fill_adv_data(advData);
        if (len) {
            app_ble_adv_data_set(sbox_demo_ble_hdl, advData, len);
        }
        len = sbox_fill_rsp_data(rspData);
        if (len) {
            app_ble_rsp_data_set(sbox_demo_ble_hdl, rspData, len);
        }
    }
    app_ble_adv_enable(sbox_demo_ble_hdl, enable);
    return 0;
}

int sbox_demo_ble_send(u8 *data, u32 len)
{
    int ret = 0;
    int i;
    printf("sbox_demo_ble_send len = %d", len);
    // put_buf(data, len);
    ret = app_ble_att_send_data(sbox_demo_ble_hdl, ATT_CHARACTERISTIC_ae99_01_VALUE_HANDLE, data, len, ATT_OP_NOTIFY);
    if (ret) {
        printf("send fail\n");
    }
    return ret;
}
/*************************************************
                  BLE 相关内容 end
*************************************************/

/*************************************************
                  SPP 相关内容
*************************************************/
static void sbox_spp_state_callback(void *hdl, void *remote_addr, u8 state)
{
    int i;
    int bond_flag = 0;
    switch (state) {
    case SPP_USER_ST_CONNECT:
        printf("custom spp connect#########\n");
        // 将 sbox_demo_spp_hdl 绑定到连接上的设备地址，否则后续会收到所有已连接设备地址的事件和数据
        app_spp_set_filter_remote_addr(sbox_demo_spp_hdl, remote_addr);
        break;
    case SPP_USER_ST_DISCONN:
        printf("custom spp disconnect#########\n");
        break;
    };
}

int sbox_demo_spp_send(u8 *data, u32 len)
{
    return app_spp_data_send(sbox_demo_spp_hdl, data, len);
}

static void sbox_spp_recieve_callback(void *hdl, void *remote_addr, u8 *buf, u16 len)
{
    printf("sbox_spp_recieve_callback len=%d\n", len);
    put_buf(buf, len);

    // test send
    sbox_demo_spp_send(buf, len);
}

/*************************************************
                  SPP 相关内容 end
*************************************************/
#define SBOX_BLE_HDL_UUID \
    (((u8)('S' + 'B' + 'O') << (3 * 8)) | \
     ((u8)('X' + 'O' + 'M') << (2 * 8)) | \
     ((u8)('B' + 'L' + 'E') << (1 * 8)) | \
     ((u8)('H' + 'D' + 'L') << (0 * 8)))

#define SBOX_SPP_HDL_UUID \
    (((u8)('S' + 'B' + 'O') << (3 * 8)) | \
     ((u8)('X' + 'O' + 'M') << (2 * 8)) | \
     ((u8)('S' + 'P' + 'P') << (1 * 8)) | \
     ((u8)('H' + 'D' + 'L') << (0 * 8)))

void sbox_demo_all_init(void)
{
    printf("sbox_demo_all_init\n");
    const uint8_t *edr_addr = bt_get_mac_addr();
    printf("edr addr:");
    put_buf((uint8_t *)edr_addr, 6);

    // BLE init
    if (sbox_demo_ble_hdl == NULL) {
        sbox_demo_ble_hdl = app_ble_hdl_alloc();
        if (sbox_demo_ble_hdl == NULL) {
            printf("sbox_demo_ble_hdl alloc err !\n");
            return;
        }
        app_ble_hdl_uuid_set(sbox_demo_ble_hdl, SBOX_BLE_HDL_UUID);
        app_ble_set_mac_addr(sbox_demo_ble_hdl, (void *)edr_addr);
        app_ble_profile_set(sbox_demo_ble_hdl, sbox_profile_data);
        app_ble_att_read_callback_register(sbox_demo_ble_hdl, sbox_att_read_callback);
        app_ble_att_write_callback_register(sbox_demo_ble_hdl, sbox_att_write_callback);
        app_ble_att_server_packet_handler_register(sbox_demo_ble_hdl, sbox_cbk_packet_handler);
        app_ble_hci_event_callback_register(sbox_demo_ble_hdl, sbox_cbk_packet_handler);
        app_ble_l2cap_packet_handler_register(sbox_demo_ble_hdl, sbox_cbk_packet_handler);

        sbox_demo_adv_enable(1);
    }
    // BLE init end

    // SPP init
    if (sbox_demo_spp_hdl == NULL) {
        sbox_demo_spp_hdl = app_spp_hdl_alloc(0x0);
        if (sbox_demo_spp_hdl == NULL) {
            printf("sbox_demo_spp_hdl alloc err !\n");
            return;
        }
        app_spp_hdl_uuid_set(sbox_demo_ble_hdl, SBOX_SPP_HDL_UUID);
        app_spp_recieve_callback_register(sbox_demo_spp_hdl, sbox_spp_recieve_callback);
        app_spp_state_callback_register(sbox_demo_spp_hdl, sbox_spp_state_callback);
        app_spp_wakeup_callback_register(sbox_demo_spp_hdl, NULL);
    }
    // SPP init end
}

void sbox_demo_all_exit(void)
{
    printf("sbox_demo_all_exit\n");

    // BLE exit
    if (app_ble_get_hdl_con_handle(sbox_demo_ble_hdl)) {
        app_ble_disconnect(sbox_demo_ble_hdl);
    }
    app_ble_hdl_free(sbox_demo_ble_hdl);
    sbox_demo_ble_hdl = NULL;

    // SPP init
    if (NULL != app_spp_get_hdl_remote_addr(sbox_demo_spp_hdl)) {
        app_spp_disconnect(sbox_demo_spp_hdl);
    }
    app_spp_hdl_free(sbox_demo_spp_hdl);
    sbox_demo_spp_hdl = NULL;
}

int sbox_ble_disconnect(void)
{
    // BLE exit
    if (app_ble_get_hdl_con_handle(sbox_demo_ble_hdl)) {
        app_ble_disconnect(sbox_demo_ble_hdl);
    }
    return 0;
}

static int sbox_tws_connction_status_event_handler(int *msg)
{
    struct tws_event *evt = (struct tws_event *)msg;
    switch (evt->event) {
    case TWS_EVENT_CONNECTED:

        if (tws_api_get_role() == TWS_ROLE_MASTER) {
            //master enable
            sbox_demo_adv_enable(1);
        } else {
            //slave disable
            printf("\nConnect Slave!!!\n\n");
            /*从机ble关掉*/
            sbox_ble_disconnect();
            sbox_demo_adv_enable(0);
        }

        break;
    default:
        break;
    }
    return 0;
}

APP_MSG_HANDLER(sbox_tws_msg_handler) = {
    .owner      = 0xff,
    .from       = MSG_FROM_TWS,
    .handler    = sbox_tws_connction_status_event_handler,
};


#endif


