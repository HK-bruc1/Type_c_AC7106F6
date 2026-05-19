#include "sdk_config.h"
#include "app_msg.h"
// #include "earphone.h"
#include "cig.h"
#include "bt_tws.h"
#include "app_main.h"
#include "btstack/avctp_user.h"
#include "multi_protocol_main.h"
#include "hid_iso.h"
#include "le_connected.h"

#if (THIRD_PARTY_PROTOCOLS_SEL & HID_ISO_EN)

void *hid_iso_ble_hdl = NULL;

typedef struct {
    u8 features;
    u16 report_intervals;
    u8 input_max_SDU;
    u8 input_prefs_SDU;
    u8 output_max_SDU;
    u8 output_prefs_SDU;
    u8 iso_reports[0];
} hid_iso_char_format;

typedef struct {
    u8 opcode;
    u8 cig_id;
    u8 cis_id;
    u16 report_interval;
    u8 input_SDU;
    u8 output_SDU;
    u8 iso_reports_ensble[0];
} _GNU_PACKED_ hid_iso_op_char_format;

static const u8 hid_field[] = {
    0x01,
    0xff, 0x01, //Supported Report Intervals
    0x0f, //Max SDU Size for Input Reports
    0x0f, //Preferred SDU Size for Input Reports
    0x0f, //Max SDU Size for Output Reports
    0x0f, //Preferred SDU Size for Output Reports
    0x00, 0x02, //Hybrid Mode ISO Reports  2to12
    0x01, 0x03, //Hybrid Mode ISO Reports  2to12
};

#define ERROR_OPCODE_OUTSIDE_RANGE              0x81
#define ERROR_DEVICE_ALREADY_IN_REQUESTED_STATE 0x82
#define ERROR_UNSUPPORTED_FEATURE               0x83

/*************************************************
                  BLE 相关内容
*************************************************/

const uint8_t hid_iso_profile_data[] = {
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

    /* CHARACTERISTIC,  2a05, INDICATE, */
    // 0x0004 CHARACTERISTIC 2a05 INDICATE
    0x0d, 0x00, 0x02, 0x00, 0x04, 0x00, 0x03, 0x28, 0x20, 0x05, 0x00, 0x05, 0x2a,
    // 0x0005 VALUE 2a05 INDICATE
    0x08, 0x00, 0x20, 0x00, 0x05, 0x00, 0x05, 0x2a,
    // 0x0006 CLIENT_CHARACTERISTIC_CONFIGURATION
    0x0a, 0x00, 0x0a, 0x01, 0x06, 0x00, 0x02, 0x29, 0x00, 0x00,

    //////////////////////////////////////////////////////
    //
    // 0x0007 PRIMARY_SERVICE  1812
    //
    //////////////////////////////////////////////////////
    0x0a, 0x00, 0x02, 0x00, 0x07, 0x00, 0x00, 0x28, 0x12, 0x18,

    /* CHARACTERISTIC,  2a4a, READ | DYNAMIC, */
    // 0x0008 CHARACTERISTIC 2a4a READ | DYNAMIC
    0x0d, 0x00, 0x02, 0x00, 0x08, 0x00, 0x03, 0x28, 0x02, 0x09, 0x00, 0x4a, 0x2a,
    // 0x0009 VALUE 2a4a READ | DYNAMIC
    0x08, 0x00, 0x02, 0x01, 0x09, 0x00, 0x4a, 0x2a,

    /* CHARACTERISTIC,  2a4b, READ | DYNAMIC, */
    // 0x000a CHARACTERISTIC 2a4b READ | DYNAMIC
    0x0d, 0x00, 0x02, 0x00, 0x0a, 0x00, 0x03, 0x28, 0x02, 0x0b, 0x00, 0x4b, 0x2a,
    // 0x000b VALUE 2a4b READ | DYNAMIC
    0x08, 0x00, 0x02, 0x01, 0x0b, 0x00, 0x4b, 0x2a,

    /* CHARACTERISTIC,  2a4c, WRITE_WITHOUT_RESPONSE | DYNAMIC, */
    // 0x000c CHARACTERISTIC 2a4c WRITE_WITHOUT_RESPONSE | DYNAMIC
    0x0d, 0x00, 0x02, 0x00, 0x0c, 0x00, 0x03, 0x28, 0x04, 0x0d, 0x00, 0x4c, 0x2a,
    // 0x000d VALUE 2a4c WRITE_WITHOUT_RESPONSE | DYNAMIC
    0x08, 0x00, 0x04, 0x01, 0x0d, 0x00, 0x4c, 0x2a,

    /* CHARACTERISTIC,  2a4d, READ | WRITE | NOTIFY | DYNAMIC, */
    // 0x000e CHARACTERISTIC 2a4d READ | WRITE | NOTIFY | DYNAMIC
    0x0d, 0x00, 0x02, 0x00, 0x0e, 0x00, 0x03, 0x28, 0x1a, 0x0f, 0x00, 0x4d, 0x2a,
    // 0x000f VALUE 2a4d READ | WRITE | NOTIFY | DYNAMIC
    0x08, 0x00, 0x1a, 0x01, 0x0f, 0x00, 0x4d, 0x2a,
    // 0x0010 CLIENT_CHARACTERISTIC_CONFIGURATION
    0x0a, 0x00, 0x0a, 0x01, 0x10, 0x00, 0x02, 0x29, 0x00, 0x00,

    /* CHARACTERISTIC,  2a4e, READ | WRITE_WITHOUT_RESPONSE,01 */
    // 0x0011 CHARACTERISTIC 2a4e READ | WRITE_WITHOUT_RESPONSE
    0x0d, 0x00, 0x02, 0x00, 0x11, 0x00, 0x03, 0x28, 0x06, 0x12, 0x00, 0x4e, 0x2a,
    // 0x0012 VALUE 2a4e READ | WRITE_WITHOUT_RESPONSE 01
    0x09, 0x00, 0x06, 0x00, 0x12, 0x00, 0x4e, 0x2a, 0x01,

    /* CHARACTERISTIC,  2a33, READ | WRITE | NOTIFY, */
    // 0x0013 CHARACTERISTIC 2a33 READ | WRITE | NOTIFY
    0x0d, 0x00, 0x02, 0x00, 0x13, 0x00, 0x03, 0x28, 0x1a, 0x14, 0x00, 0x33, 0x2a,
    // 0x0014 VALUE 2a33 READ | WRITE | NOTIFY
    0x08, 0x00, 0x1a, 0x00, 0x14, 0x00, 0x33, 0x2a,
    // 0x0015 CLIENT_CHARACTERISTIC_CONFIGURATION
    0x0a, 0x00, 0x0a, 0x01, 0x15, 0x00, 0x02, 0x29, 0x00, 0x00,

    //////////////////////////////////////////////////////
    //
    // 0x0016 PRIMARY_SERVICE  185c
    //
    //////////////////////////////////////////////////////
    0x0a, 0x00, 0x02, 0x00, 0x16, 0x00, 0x00, 0x28, 0x5c, 0x18,

    /* CHARACTERISTIC,  2c23, READ|DYNAMIC, */
    // 0x0017 CHARACTERISTIC 2c23 READ|DYNAMIC
    0x0d, 0x00, 0x02, 0x00, 0x17, 0x00, 0x03, 0x28, 0x02, 0x18, 0x00, 0x23, 0x2c,
    // 0x0018 VALUE 2c23 READ|DYNAMIC
    0x08, 0x00, 0x02, 0x01, 0x18, 0x00, 0x23, 0x2c,

    /* CHARACTERISTIC,  2c24, WRITE | DYNAMIC, */
    // 0x0019 CHARACTERISTIC 2c24 WRITE | DYNAMIC
    0x0d, 0x00, 0x02, 0x00, 0x19, 0x00, 0x03, 0x28, 0x08, 0x1a, 0x00, 0x24, 0x2c,
    // 0x001a VALUE 2c24 WRITE | DYNAMIC
    0x08, 0x00, 0x08, 0x01, 0x1a, 0x00, 0x24, 0x2c,

    // END
    0x00, 0x00,
};

//
// characteristics <--> handles
//
#define ATT_CHARACTERISTIC_2a00_01_VALUE_HANDLE 0x0003
#define ATT_CHARACTERISTIC_2a05_01_VALUE_HANDLE 0x0005
#define ATT_CHARACTERISTIC_2a05_01_CLIENT_CONFIGURATION_HANDLE 0x0006
#define ATT_CHARACTERISTIC_2a4a_01_VALUE_HANDLE 0x0009
#define ATT_CHARACTERISTIC_2a4b_01_VALUE_HANDLE 0x000b
#define ATT_CHARACTERISTIC_2a4c_01_VALUE_HANDLE 0x000d
#define ATT_CHARACTERISTIC_2a4d_01_VALUE_HANDLE 0x000f
#define ATT_CHARACTERISTIC_2a4d_01_CLIENT_CONFIGURATION_HANDLE 0x0010
#define ATT_CHARACTERISTIC_2a4e_01_VALUE_HANDLE 0x0012
#define ATT_CHARACTERISTIC_2a33_01_VALUE_HANDLE 0x0014
#define ATT_CHARACTERISTIC_2a33_01_CLIENT_CONFIGURATION_HANDLE 0x0015
#define ATT_CHARACTERISTIC_2c23_01_VALUE_HANDLE 0x0018
#define ATT_CHARACTERISTIC_2c24_01_VALUE_HANDLE 0x001a

static u16 hid_adv_interval_min = 150;

u8 hid_iso_cig_hdl = 0;
u8 hid_mode = 0;
static void hid_cbk_packet_handler(void *hdl, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
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
                hid_mode = 0;
                break;
            default:
                break;
            }
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            printf("custom  HCI_EVENT_DISCONNECTION_COMPLETE: %0x", packet[5]);
            hid_iso_cig_hdl  = 0;
            hid_iso_adv_enable(1);
            break;
        default:
            break;
        }
        break;
    }
    return;
}

static uint16_t hid_att_read_callback(void *hdl, hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t *buffer, uint16_t buffer_size)
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
    case ATT_CHARACTERISTIC_2c23_01_VALUE_HANDLE:
        att_value_len = sizeof(hid_field);
        if ((offset >= att_value_len) || (offset + buffer_size) > att_value_len) {
            break;
        }
        if (buffer) {
            memcpy(buffer, &hid_field[offset], buffer_size);
            att_value_len = buffer_size;
        }
        break;

    case ATT_CHARACTERISTIC_2a4b_01_VALUE_HANDLE:
        att_value_len = sizeof(keypage_report_map_ios);
        if ((offset >= att_value_len) || (offset + buffer_size) > att_value_len) {
            break;
        }
        if (buffer) {
            memcpy(buffer, &keypage_report_map_ios[offset], buffer_size);
            att_value_len = buffer_size;
        }
        break;

    case ATT_CHARACTERISTIC_2a4d_01_CLIENT_CONFIGURATION_HANDLE:
    case ATT_CHARACTERISTIC_2a33_01_CLIENT_CONFIGURATION_HANDLE:
        if (buffer) {
            buffer[0] = multi_att_get_ccc_config(connection_handle, handle);
            buffer[1] = 0;
        }
        att_value_len = 2;
        break;
    default:
        break;
    }
    printf("att_value_len= %d", att_value_len);
    return att_value_len;
}

static bool is_single_bit_set(u16 value)
{
    return value && !(value & (value - 1));
}
static int hid_att_write_callback(void *hdl, hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size)
{
    int result = 0;
    u16 tmp16;
    u16 handle = att_handle;
    printf("<-------------write_callback, handle= 0x%04x,size = %d", handle, buffer_size);
    switch (handle) {
    case ATT_CHARACTERISTIC_2a00_01_VALUE_HANDLE:
        break;
    case ATT_CHARACTERISTIC_2a4c_01_VALUE_HANDLE:
        printf("rx(%d):\n", buffer_size);
        put_buf(buffer, buffer_size);
        // test
        break;
    case ATT_CHARACTERISTIC_2c24_01_VALUE_HANDLE:
        printf("rx(%d):\n", buffer_size);
        put_buf(buffer, buffer_size);
        if (buffer[0] == 1) {
            hid_iso_op_char_format *op_char_format = (hid_iso_op_char_format *)malloc(buffer_size);
            //cpy数据，防止数据不对齐。
            memcpy(op_char_format, buffer, buffer_size);
            hid_iso_cig_hdl  = op_char_format->cig_id;
            if (hid_mode == 1) {
                return ERROR_DEVICE_ALREADY_IN_REQUESTED_STATE;
            }
            printf("interval 0x%x\n", op_char_format->report_interval);
            if (!is_single_bit_set(op_char_format->report_interval)) {
                free(op_char_format);
                return ERROR_UNSUPPORTED_FEATURE;
            }
            if (/* (op_char_format->iso_reports_ensble[0] > 0x7) || */ buffer_size > 9) {
                free(op_char_format);
                return ERROR_UNSUPPORTED_FEATURE;
            }
            hid_mode = 1;
            free(op_char_format);
        } else if (buffer[0] == 2) {
            if (hid_mode == 0) {
                return ERROR_DEVICE_ALREADY_IN_REQUESTED_STATE;
            }
            hid_mode = 0;
        } else {
            return ERROR_OPCODE_OUTSIDE_RANGE;
        }
        break;
    case ATT_CHARACTERISTIC_2a4d_01_CLIENT_CONFIGURATION_HANDLE:
    case ATT_CHARACTERISTIC_2a33_01_CLIENT_CONFIGURATION_HANDLE:
        printf("\nwrite ccc:%04x, %02x\n", handle, buffer[0]);
        multi_att_set_ccc_config(connection_handle, handle, buffer[0]);
        break;
    default:
        break;
    }
    return 0;
}

static u8 hid_fill_adv_data(u8 *adv_data)
{
    u8 offset = 0;
    u16 HID_UUID = 0x1812;
    u16 HID_ISO_UUID = 0x185c;
    const char *name_p = bt_get_local_name();
    int name_len = strlen(name_p);
    offset += make_eir_packet_data(&adv_data[offset], offset, HCI_EIR_DATATYPE_COMPLETE_16BIT_SERVICE_UUIDS, (u8 *)&HID_UUID, 2);
    offset += make_eir_packet_data(&adv_data[offset], offset, HCI_EIR_DATATYPE_COMPLETE_16BIT_SERVICE_UUIDS, (u8 *)&HID_ISO_UUID, 2);
    offset += make_eir_packet_data(&adv_data[offset], offset, HCI_EIR_DATATYPE_COMPLETE_LOCAL_NAME, (void *)name_p, name_len);
    if (offset > ADV_RSP_PACKET_MAX) {
        puts("***rsp_data overflow!!!!!!\n");
        return 0;
    }
    return offset;
}

static u8 hid_fill_rsp_data(u8 *rsp_data)
{
    return 0;
}

extern u8 is_cig_phone_conn();
int hid_iso_adv_enable(u8 enable)
{
    uint8_t adv_type = ADV_IND;
    uint8_t adv_channel = ADV_CHANNEL_ALL;
    uint8_t advData[ADV_RSP_PACKET_MAX] = {0};
    uint8_t rspData[ADV_RSP_PACKET_MAX] = {0};
    uint8_t len = 0;

#if !TCFG_RCSP_DUAL_CONN_ENABLE && !TCFG_THIRD_PARTY_PROTOCOLS_SIMPLIFIED

#if (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN))
    if (enable) {
        if ((0 == is_cig_phone_conn()) && (0 == bt_get_total_connect_dev())) {
            printf("cig[%d] or edr[%d] is not connected\n", is_cig_phone_conn(), bt_get_total_connect_dev());
            return 0;
        }
    }
#endif
#endif

    if (enable == app_ble_adv_state_get(hid_iso_ble_hdl)) {
        return 0;
    }
    if (enable) {
#if (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN))
        if (is_cig_phone_conn()) {
            adv_type = APP_ADV_SCAN_IND;
        }
#endif

        app_ble_set_adv_param(hid_iso_ble_hdl, hid_adv_interval_min, adv_type, adv_channel);
        len = hid_fill_adv_data(advData);
        if (len) {
            app_ble_adv_data_set(hid_iso_ble_hdl, advData, len);
        }
        len = hid_fill_rsp_data(rspData);
        if (len) {
            app_ble_rsp_data_set(hid_iso_ble_hdl, rspData, len);
        }
    }
    app_ble_adv_enable(hid_iso_ble_hdl, enable);
    return 0;
}

int hid_iso_ble_send(u8 *data, u32 len)
{
    int ret = 0;
    int i;
    printf("hid_iso_ble_send len = %d", len);
    put_buf(data, len);
    ret = app_ble_att_send_data(hid_iso_ble_hdl, ATT_CHARACTERISTIC_2c23_01_VALUE_HANDLE, data, len, ATT_OP_AUTO_READ_CCC);
    if (ret) {
        printf("send fail\n");
    }
    return ret;
}

/*************************************************
                  BLE 相关内容 end
*************************************************/

#define HID_ISO_BLE_HDL_UUID \
    (((u8)('H' + 'I' + 'D') << (3 * 8)) | \
     ((u8)('I' + 'S' + 'O') << (2 * 8)) | \
     ((u8)('B' + 'L' + 'E') << (1 * 8)) | \
     ((u8)('H' + 'D' + 'L') << (0 * 8)))

void hid_iso_init(void)
{
    printf("hid_iso_all_init\n");
    const uint8_t *edr_addr = bt_get_mac_addr();
    printf("edr addr:");
    put_buf((uint8_t *)edr_addr, 6);

    // BLE init
    if (hid_iso_ble_hdl == NULL) {
        hid_iso_ble_hdl = app_ble_hdl_alloc();
        if (hid_iso_ble_hdl == NULL) {
            printf("hid_iso_ble_hdl alloc err !\n");
            return;
        }
        app_ble_hdl_uuid_set(hid_iso_ble_hdl, HID_ISO_BLE_HDL_UUID);
        app_ble_set_mac_addr(hid_iso_ble_hdl, (void *)edr_addr);
        app_ble_profile_set(hid_iso_ble_hdl, hid_iso_profile_data);
        app_ble_att_read_callback_register(hid_iso_ble_hdl, hid_att_read_callback);
        app_ble_att_write_callback_register(hid_iso_ble_hdl, hid_att_write_callback);
        app_ble_att_server_packet_handler_register(hid_iso_ble_hdl, hid_cbk_packet_handler);
        app_ble_hci_event_callback_register(hid_iso_ble_hdl, hid_cbk_packet_handler);
        app_ble_l2cap_packet_handler_register(hid_iso_ble_hdl, hid_cbk_packet_handler);

        hid_iso_adv_enable(1);
    }
    // BLE init end

}

void hid_iso_exit(void)
{
    printf("hid_iso_all_exit\n");

    // BLE exit
    if (app_ble_get_hdl_con_handle(hid_iso_ble_hdl)) {
        app_ble_disconnect(hid_iso_ble_hdl);
    }
    app_ble_hdl_free(hid_iso_ble_hdl);
    hid_iso_ble_hdl = NULL;

}

int get_hid_iso_conn_hdl_en(u16 cig_hdl)
{
    if (cig_hdl == hid_iso_cig_hdl) {
        return 1;
    }
    return 0;
}

void hid_connected_iso_callback(const void *const buf, size_t length, void *priv)
{
    printf(">>>hid data:\n");
    put_buf(buf, length);
}

int connected_key_event_sync(int key_msg)
{
    int slen = 0;
    u8 len = 15;
    u8 *buffer = zalloc(len);
    u8 device_channel = 0x1;
    ASSERT(buffer);
    buffer[0] = key_msg;//CONNECTED_KEY_SYNC_TYPE;
    buffer[1] = 0x0a;//CONNECTED_KEY_SYNC_TYPE;

    if (buffer) {
        slen = connected_send_cis_data(device_channel, (void *)buffer, len);
    }
    free(buffer);
    return slen;
}

#endif

