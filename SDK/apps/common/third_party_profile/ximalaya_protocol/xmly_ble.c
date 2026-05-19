#include "system/includes.h"
#include "media/includes.h"
#include "sdk_config.h"
#include "app_msg.h"
#include "earphone.h"
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
#include "xmly_platform_api.h"
#include "xmly_protocol.h"

#if (THIRD_PARTY_PROTOCOLS_SEL & XIMALAYA_EN)

void *xmly_ble_hdl = NULL;
extern int (*xmly_event_callback)(u8 *remote_addr, u32 event, u8 *data, u32 len);

/*************************************************
                  BLE 相关内容
*************************************************/

const uint8_t xmly_profile_data[] = {
    //////////////////////////////////////////////////////
    //
    // 0x0001 PRIMARY_SERVICE  0xAA62
    //
    //////////////////////////////////////////////////////
    0x0a, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x28, 0x62, 0xaa,

    /* CHARACTERISTIC, 0xAAD5,  READ|WRITE_WITHOUT_RESPONSE | DYNAMIC, */
    // 0x0002 CHARACTERISTIC 0xAAD5 READ|WRITE_WITHOUT_RESPONSE | DYNAMIC
    0x0d, 0x00, 0x02, 0x00, 0x02, 0x00, 0x03, 0x28, 0x06, 0x03, 0x00, 0xd5, 0xaa,
    // 0x0003 VALUE 0xAAD5 READ|WRITE_WITHOUT_RESPONSE | DYNAMIC
    0x08, 0x00, 0x06, 0x01, 0x03, 0x00, 0xd5, 0xaa,

    /* CHARACTERISTIC, 0xAAD6,  READ|NOTIFY, */
    // 0x0004 CHARACTERISTIC 0xAAD6 READ|NOTIFY
    0x0d, 0x00, 0x02, 0x00, 0x04, 0x00, 0x03, 0x28, 0x12, 0x05, 0x00, 0xd6, 0xaa,
    // 0x0005 VALUE 0xAAD6 READ|NOTIFY
    0x08, 0x00, 0x12, 0x00, 0x05, 0x00, 0xd6, 0xaa,
    // 0x0006 CLIENT_CHARACTERISTIC_CONFIGURATION
    0x0a, 0x00, 0x0a, 0x01, 0x06, 0x00, 0x02, 0x29, 0x00, 0x00,

    // END
    0x00, 0x00,
};

//
// characteristics <--> handles
//
#define ATT_CHARACTERISTIC_2a00_01_VALUE_HANDLE 0x0003
#define ATT_CHARACTERISTIC_AAD5_01_VALUE_HANDLE 0x0003
#define ATT_CHARACTERISTIC_AAD6_01_VALUE_HANDLE 0x0005
#define ATT_CHARACTERISTIC_AAD6_01_CLIENT_CONFIGURATION_HANDLE 0x0006

static u16 xmly_adv_interval_min = 150;

static void xmly_swapX(const uint8_t *src, uint8_t *dst, int32_t len)
{
    int32_t i;
    for (i = 0; i < len; i++) {
        dst[len - 1 - i] = src[i];
    }
}

static void xmly_cbk_packet_handler(void *hdl, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
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
                if (xmly_event_callback != NULL) {
                    xmly_event_callback(NULL, XMLY_EVENT_BLE_CONNECTION, NULL, 0);
                }
                xmly_ble_status_set(1);
                break;
            default:
                break;
            }
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            printf("HCI_EVENT_DISCONNECTION_COMPLETE: %0x", packet[5]);
            if (xmly_event_callback != NULL) {
                xmly_event_callback(NULL, XMLY_EVENT_BLE_DISCONNECTION, NULL, 0);
            }
            xmly_ble_status_set(0);
            break;
        default:
            break;
        }
        break;
    }
    return;
}

static uint16_t xmly_att_read_callback(void *hdl, hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t *buffer, uint16_t buffer_size)
{
    uint16_t  att_value_len = 0;
    uint16_t handle = att_handle;
    printf("<-------------read_callback, handle= 0x%04x,buffer= %08x", handle, (u32)buffer);
    switch (handle) {
    /* case ATT_CHARACTERISTIC_2a00_01_VALUE_HANDLE: */
    /* const char *gap_name = bt_get_local_name(); */
    /* att_value_len = strlen(gap_name); */
    /* if ((offset >= att_value_len) || (offset + buffer_size) > att_value_len) { */
    /* break; */
    /* } */
    /* if (buffer) { */
    /* memcpy(buffer, &gap_name[offset], buffer_size); */
    /* att_value_len = buffer_size; */
    /* printf("\n------read gap_name: %s", gap_name); */
    /* } */
    /* break; */
    case ATT_CHARACTERISTIC_AAD6_01_CLIENT_CONFIGURATION_HANDLE:
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

static int xmly_att_write_callback(void *hdl, hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size)
{
    int result = 0;
    u16 tmp16;
    u16 handle = att_handle;
    printf("<-------------write_callback, handle= 0x%04x,size = %d", handle, buffer_size);
    switch (handle) {
    /* case ATT_CHARACTERISTIC_2a00_01_VALUE_HANDLE: */
    /* break; */
    case ATT_CHARACTERISTIC_AAD5_01_VALUE_HANDLE:
        printf("rx(%d):\n", buffer_size);
        put_buf(buffer, buffer_size);
        xmly_cmd_recieve_callback(buffer, buffer_size);
        break;
    case ATT_CHARACTERISTIC_AAD6_01_CLIENT_CONFIGURATION_HANDLE:
        printf("\nwrite ccc:%04x, %02x\n", handle, buffer[0]);
        att_set_ccc_config(handle, buffer[0]);
        break;
    default:
        break;
    }
    return 0;
}


__attribute__((weak)) u8 xmly_user_fill_adv_data(u8 *rsp_data)
{
    return 0;
}

static u8 xmly_fill_adv_data(u8 *adv_data)
{
    u8 offset = 0;
    u8 i;
    u8 *buf = adv_data;

    offset = xmly_user_fill_adv_data(adv_data);
    if (offset) {
        return offset;
    }

    buf[offset++] = 11;
    buf[offset++] = 0xFF;
    buf[offset++] = 0x9;   // length
    buf[offset++] = xmly_adv_cid[0];	// CID
    buf[offset++] = xmly_adv_cid[1];
    buf[offset++] = 0x0;   // EXT
    xmly_swapX(bt_get_mac_addr(), &buf[offset], 6);   // MAC
    offset += 6;

    buf[offset++] = 14;
    buf[offset++] = 0x16;
    buf[offset++] = 0x62;
    buf[offset++] = 0xAA;
    memcpy(&buf[offset], xmly_adv_pid, 4);
    offset += 4;
    memcpy(&buf[offset], xmly_adv_vid, 4);
    offset += 4;
    memcpy(&buf[offset], xmly_adv_sv, 2);
    offset += 2;
    buf[offset++] = xmly_adv_xpv;

    if (offset > ADV_RSP_PACKET_MAX) {
        puts("***rsp_data overflow!!!!!!\n");
        return 0;
    }
    return offset;
}

__attribute__((weak)) u8 xmly_user_fill_rsp_data(u8 *rsp_data)
{
    return 0;
}

static u8 xmly_fill_rsp_data(u8 *rsp_data)
{

    u8 offset = 0;
    u8 *buf = rsp_data;
    const char *edr_name = bt_get_local_name();

    offset = xmly_user_fill_rsp_data(rsp_data);
    if (offset) {
        return offset;
    }

    offset += make_eir_packet_val(&buf[offset], offset, HCI_EIR_DATATYPE_MORE_16BIT_SERVICE_UUIDS, 0xAA62, 2);
    offset += make_eir_packet_val(&buf[offset], offset, HCI_EIR_DATATYPE_FLAGS, 0x1A, 1);
    offset += make_eir_packet_data(&buf[offset], offset, HCI_EIR_DATATYPE_COMPLETE_LOCAL_NAME, (void *)edr_name, strlen(edr_name));
    if (offset > ADV_RSP_PACKET_MAX) {
        puts("***rsp_data overflow!!!!!!\n");
        return 0;
    }
    return offset;
}

u8 *xmly_ble_get_addr(void)
{
    return app_ble_adv_addr_get(xmly_ble_hdl);
}

int xmly_adv_interval_set(u16 interval)
{
    xmly_adv_interval_min = interval;
    return 0;
}

int xmly_adv_ble_connect_state(void)
{
    if (app_ble_get_hdl_con_handle(xmly_ble_hdl)) {
        return 1;
    }
    return 0;
}

int xmly_adv_enable(u8 enable)
{
    uint8_t adv_type = ADV_IND;
    uint8_t adv_channel = ADV_CHANNEL_ALL;
    uint8_t advData[ADV_RSP_PACKET_MAX] = {0};
    uint8_t rspData[ADV_RSP_PACKET_MAX] = {0};
    uint8_t len = 0;

    printf("xmly_adv_enable %d\n", enable);

    if (enable == app_ble_adv_state_get(xmly_ble_hdl)) {
        printf("xmly adv state: %d\n", enable);
        return 0;
    }

    if (enable && !xmly_is_tws_master_role()) {
        printf("slave no adv\n");
        return 0;
    }

    if (enable) {
        app_ble_set_adv_param(xmly_ble_hdl, xmly_adv_interval_min, adv_type, adv_channel);
        len = xmly_fill_adv_data(advData);
        if (len) {
            app_ble_adv_data_set(xmly_ble_hdl, advData, len);
        }
        len = xmly_fill_rsp_data(rspData);
        if (len) {
            app_ble_rsp_data_set(xmly_ble_hdl, rspData, len);
        }
    }
    app_ble_adv_enable(xmly_ble_hdl, enable);
    return 0;
}

int xmly_ble_send(u8 *data, u32 len)
{
    int ret = 0;
    int i;

    if (!xmly_is_tws_master_role()) {
        printf("slave dont send\n");
        return 0;
    }

    ret = app_ble_att_send_data(xmly_ble_hdl, ATT_CHARACTERISTIC_AAD6_01_VALUE_HANDLE, data, len, ATT_OP_AUTO_READ_CCC);
    if (ret) {
        printf("send fail\n");
    }
    return ret;
}

int xmly_ble_disconnect(void)
{
    // BLE exit
    if (app_ble_get_hdl_con_handle(xmly_ble_hdl)) {
        app_ble_disconnect(xmly_ble_hdl);
    }
    return 0;
}

#define XMLY_BLE_HDL_UUID \
	(((u8)('X' + 'M') << (3 * 8)) | \
	 ((u8)('L' + 'Y') << (2 * 8)) | \
	 ((u8)('B' + 'L' + 'E') << (1 * 8)) | \
	 ((u8)('H' + 'D' + 'L') << (0 * 8)))


__attribute__((weak)) void xmly_user_set_mac_addr_callback(u8 *edr_addr)
{
}

void xmly_ble_init(void)
{
    printf("xmly_ble_init\n");
    u8 edr_addr[6];
    memcpy(edr_addr, bt_get_mac_addr(), 6);
    printf("edr addr:");
    put_buf((uint8_t *)edr_addr, 6);

    // BLE init
    if (xmly_ble_hdl == NULL) {
        xmly_ble_hdl = app_ble_hdl_alloc();
        if (xmly_ble_hdl == NULL) {
            printf("xmly_ble_hdl alloc err !\n");
            return;
        }
        app_ble_hdl_uuid_set(xmly_ble_hdl, XMLY_BLE_HDL_UUID);
        xmly_user_set_mac_addr_callback(edr_addr);
        app_ble_set_mac_addr(xmly_ble_hdl, (void *)edr_addr);
        app_ble_profile_set(xmly_ble_hdl, xmly_profile_data);
        app_ble_att_read_callback_register(xmly_ble_hdl, xmly_att_read_callback);
        app_ble_att_write_callback_register(xmly_ble_hdl, xmly_att_write_callback);
        app_ble_att_server_packet_handler_register(xmly_ble_hdl, xmly_cbk_packet_handler);
        app_ble_hci_event_callback_register(xmly_ble_hdl, xmly_cbk_packet_handler);
        app_ble_l2cap_packet_handler_register(xmly_ble_hdl, xmly_cbk_packet_handler);
    }
    // BLE init end
}

void xmly_ble_exit(void)
{
    printf("xmly_ble_exit\n");

    // BLE exit
    if (app_ble_get_hdl_con_handle(xmly_ble_hdl)) {
        app_ble_disconnect(xmly_ble_hdl);
    }
    app_ble_hdl_free(xmly_ble_hdl);
    xmly_ble_hdl = NULL;
}

#endif
