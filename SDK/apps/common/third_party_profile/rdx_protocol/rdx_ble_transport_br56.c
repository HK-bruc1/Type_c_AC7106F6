#include "app_config.h"

#if TCFG_RDX_ENABLE

#include "system/includes.h"
#include "app_ble_spp_api.h"
#include "btstack/btstack_event.h"
#include "btstack/le/att.h"
#include "rdx_ble_name.h"
#include "rdx_ble_transport_br56.h"
#include "rdx_identity.h"
#include "rdx_mvp0_compat_config.h"
#include "rdx_mvp0_protocol.h"

#define RDX_HANDLE_GAP_NAME                   0x0003
#define RDX_HANDLE_RX                         0x0006
#define RDX_HANDLE_TX                         0x0008
#define RDX_HANDLE_TX_CCC                     0x0009
#define RDX_HANDLE_IDENTITY                   0x000b
#define RDX_HANDLE_BATTERY                    0x000e
#define RDX_HANDLE_BATTERY_CCC                0x000f
#define RDX_HANDLE_OTA_RX                     0x0012
#define RDX_HANDLE_OTA_TX                     0x0014
#define RDX_HANDLE_OTA_TX_CCC                 0x0015

#define RDX_BLE_HANDLE_UUID                   0x52445830u
#define RDX_RECORD_CONN_INTERVAL_MIN          6
#define RDX_RECORD_CONN_INTERVAL_MAX          6
#define RDX_RECORD_DLE_TX_OCTETS              251
#define RDX_RECORD_DLE_TX_TIME                2120

static const struct conn_update_param_t rdx_record_conn_param = {
    RDX_RECORD_CONN_INTERVAL_MIN,
    RDX_RECORD_CONN_INTERVAL_MAX,
    0,
    600,
};

typedef struct {
    void *hdl;
    u16 con_handle;
    u16 mtu;
    u8 tx_ccc;
    u8 initialized;
    u8 record_streaming;
} rdx_ble_transport_t;

static rdx_ble_transport_t rdx_ble;

static int rdx_ble_request_record_conn_param(const char *reason)
{
    int ret;

    if (!rdx_ble.hdl || !rdx_ble.con_handle) {
        return -1;
    }

    ret = ble_op_conn_param_request(rdx_ble.con_handle,
                                    (void *)&rdx_record_conn_param);
    printf("[RDX][BLE] conn_param_request reason=%s interval=%u-%u"
           " latency=%u timeout=%u ret=%d\n",
           reason,
           RDX_RECORD_CONN_INTERVAL_MIN,
           RDX_RECORD_CONN_INTERVAL_MAX,
           rdx_record_conn_param.latency,
           rdx_record_conn_param.timeout,
           ret);
    return ret;
}

static void rdx_ble_request_high_throughput(const char *reason)
{
    int dle_ret;
    int phy_ret;

    if (!rdx_ble.hdl || !rdx_ble.con_handle) {
        return;
    }

    dle_ret = ble_op_set_data_length(rdx_ble.con_handle,
                                     RDX_RECORD_DLE_TX_OCTETS,
                                     RDX_RECORD_DLE_TX_TIME);
    phy_ret = ble_op_set_ext_phy(rdx_ble.con_handle, 0,
                                 CONN_SET_2M_PHY, CONN_SET_2M_PHY,
                                 CONN_SET_PHY_OPTIONS_NONE);
    printf("[RDX][BLE] throughput_request reason=%s dle=%u/%u ret=%d"
           " phy=2M ret=%d\n",
           reason,
           RDX_RECORD_DLE_TX_OCTETS,
           RDX_RECORD_DLE_TX_TIME,
           dle_ret,
           phy_ret);
}

static const u8 rdx_profile_data[] = {
    /* GAP service and Device Name, handles 0x0001 - 0x0003. */
    0x0a, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x28, 0x00, 0x18,
    0x0d, 0x00, 0x02, 0x00, 0x02, 0x00, 0x03, 0x28, 0x02, 0x03, 0x00, 0x00, 0x2a,
    0x08, 0x00, 0x02, 0x01, 0x03, 0x00, 0x00, 0x2a,

    /* RDX service 06068D0C-6B97-11EF-B864-0240AC120002. */
    0x18, 0x00, 0x02, 0x00, 0x04, 0x00, 0x00, 0x28, 0x02, 0x00, 0x12, 0xac, 0x40, 0x02, 0x64, 0xb8, 0xef, 0x11, 0x97, 0x6b, 0x0c, 0x8d, 0x06, 0x06,
    0x1b, 0x00, 0x02, 0x00, 0x05, 0x00, 0x03, 0x28, 0x04, 0x06, 0x00, 0x02, 0x00, 0x12, 0xac, 0x41, 0x02, 0x64, 0xb8, 0xef, 0x11, 0x97, 0x6b, 0x1c, 0x8d, 0x06, 0x06,
    0x16, 0x00, 0x04, 0x03, 0x06, 0x00, 0x02, 0x00, 0x12, 0xac, 0x41, 0x02, 0x64, 0xb8, 0xef, 0x11, 0x97, 0x6b, 0x1c, 0x8d, 0x06, 0x06,
    0x1b, 0x00, 0x02, 0x00, 0x07, 0x00, 0x03, 0x28, 0x10, 0x08, 0x00, 0x02, 0x00, 0x12, 0xac, 0x42, 0x02, 0x64, 0xb8, 0xef, 0x11, 0x97, 0x6b, 0x2c, 0x8d, 0x06, 0x06,
    0x16, 0x00, 0x10, 0x02, 0x08, 0x00, 0x02, 0x00, 0x12, 0xac, 0x42, 0x02, 0x64, 0xb8, 0xef, 0x11, 0x97, 0x6b, 0x2c, 0x8d, 0x06, 0x06,
    0x0a, 0x00, 0x0a, 0x01, 0x09, 0x00, 0x02, 0x29, 0x00, 0x00,
    0x1b, 0x00, 0x02, 0x00, 0x0a, 0x00, 0x03, 0x28, 0x02, 0x0b, 0x00, 0x02, 0x00, 0x12, 0xac, 0x43, 0x02, 0x64, 0xb8, 0xef, 0x11, 0x97, 0x6b, 0x3c, 0x8d, 0x06, 0x06,
    0x16, 0x00, 0x02, 0x03, 0x0b, 0x00, 0x02, 0x00, 0x12, 0xac, 0x43, 0x02, 0x64, 0xb8, 0xef, 0x11, 0x97, 0x6b, 0x3c, 0x8d, 0x06, 0x06,

    /* Battery service, handles 0x000c - 0x000f. */
    0x0a, 0x00, 0x02, 0x00, 0x0c, 0x00, 0x00, 0x28, 0x0f, 0x18,
    0x0d, 0x00, 0x02, 0x00, 0x0d, 0x00, 0x03, 0x28, 0x12, 0x0e, 0x00, 0x19, 0x2a,
    0x08, 0x00, 0x12, 0x01, 0x0e, 0x00, 0x19, 0x2a,
    0x0a, 0x00, 0x0a, 0x01, 0x0f, 0x00, 0x02, 0x29, 0x00, 0x00,

    /* OTA fingerprint only; MVP0 ignores OTA writes. */
    0x18, 0x00, 0x02, 0x00, 0x10, 0x00, 0x00, 0x28, 0xb3, 0xa7, 0x88, 0xf5, 0x5a, 0xf0, 0x74, 0x33, 0xbb, 0x89, 0x16, 0xc6, 0x6f, 0x9a, 0x23, 0x00,
    0x1b, 0x00, 0x02, 0x00, 0x11, 0x00, 0x03, 0x28, 0x04, 0x12, 0x00, 0xb3, 0xa7, 0x88, 0xf5, 0x5a, 0xf1, 0x74, 0x33, 0xbb, 0x89, 0x16, 0xc6, 0x7f, 0x9a, 0x23, 0x00,
    0x16, 0x00, 0x04, 0x03, 0x12, 0x00, 0xb3, 0xa7, 0x88, 0xf5, 0x5a, 0xf1, 0x74, 0x33, 0xbb, 0x89, 0x16, 0xc6, 0x7f, 0x9a, 0x23, 0x00,
    0x1b, 0x00, 0x02, 0x00, 0x13, 0x00, 0x03, 0x28, 0x10, 0x14, 0x00, 0xb3, 0xa7, 0x88, 0xf5, 0x5a, 0xf2, 0x74, 0x33, 0xbb, 0x89, 0x16, 0xc6, 0x8f, 0x9a, 0x23, 0x00,
    0x16, 0x00, 0x10, 0x02, 0x14, 0x00, 0xb3, 0xa7, 0x88, 0xf5, 0x5a, 0xf2, 0x74, 0x33, 0xbb, 0x89, 0x16, 0xc6, 0x8f, 0x9a, 0x23, 0x00,
    0x0a, 0x00, 0x0a, 0x01, 0x15, 0x00, 0x02, 0x29, 0x00, 0x00,
    0x00, 0x00,
};

static u16 rdx_ble_read_blob(const u8 *data, u16 data_len, u16 offset, u8 *buffer, u16 buffer_size)
{
    u16 copy_len;

    if (!buffer) {
        return data_len;
    }
    if (!data || offset >= data_len) {
        return 0;
    }

    copy_len = data_len - offset;
    if (copy_len > buffer_size) {
        copy_len = buffer_size;
    }
    memcpy(buffer, data + offset, copy_len);
    return copy_len;
}

static u8 rdx_ble_fill_adv_data(u8 *adv_data)
{
    u8 offset = 0;
    u8 name_len;
    const char *name = rdx_ble_name_get();

    name_len = strlen(name);
    if (name_len > ADV_RSP_PACKET_MAX - 5) {
        name_len = ADV_RSP_PACKET_MAX - 5;
    }

    offset += make_eir_packet_val(adv_data + offset, offset, HCI_EIR_DATATYPE_FLAGS, 0x0a, 1);
    offset += make_eir_packet_data(adv_data + offset, offset, HCI_EIR_DATATYPE_COMPLETE_LOCAL_NAME, (u8 *)name, name_len);
    return offset <= ADV_RSP_PACKET_MAX ? offset : 0;
}

static u8 rdx_ble_fill_rsp_data(u8 *rsp_data)
{
    u8 manufacturer_data[ADV_RSP_PACKET_MAX - 2];
    u8 manufacturer_len;
    u8 offset = 0;

    manufacturer_len = rdx_identity_fill_manufacturer_data(manufacturer_data, sizeof(manufacturer_data));
    if (!manufacturer_len) {
        return 0;
    }

    offset += make_eir_packet_data(rsp_data + offset, offset,
                                   HCI_EIR_DATATYPE_MANUFACTURER_SPECIFIC_DATA,
                                   manufacturer_data, manufacturer_len);
    return offset <= ADV_RSP_PACKET_MAX ? offset : 0;
}

static int rdx_ble_adv_enable(u8 enable)
{
    u8 adv_data[ADV_RSP_PACKET_MAX] = {0};
    u8 rsp_data[ADV_RSP_PACKET_MAX] = {0};
    u8 adv_len;
    u8 rsp_len;
    int ret;

    if (!rdx_ble.hdl) {
        return -1;
    }
    if (enable == app_ble_adv_state_get(rdx_ble.hdl)) {
        return 0;
    }

    if (enable) {
        adv_len = rdx_ble_fill_adv_data(adv_data);
        rsp_len = rdx_ble_fill_rsp_data(rsp_data);
        if (!adv_len || !rsp_len) {
            return -1;
        }

        ret = app_ble_set_adv_param(rdx_ble.hdl, RDX_BLE_ADV_INTERVAL,
                                    APP_ADV_IND, APP_ADV_CHANNEL_ALL);
        if (ret) {
            return ret;
        }
        ret = app_ble_adv_data_set(rdx_ble.hdl, adv_data, adv_len);
        if (ret) {
            return ret;
        }
        ret = app_ble_rsp_data_set(rdx_ble.hdl, rsp_data, rsp_len);
        if (ret) {
            return ret;
        }
    }

    ret = app_ble_adv_enable(rdx_ble.hdl, enable);
    printf("[RDX] adv=%u ret=%d\n", enable, ret);
    return ret;
}

static void rdx_ble_packet_handler(void *hdl, u8 packet_type, u16 channel, u8 *packet, u16 size)
{
    u8 event_type;
    u8 subevent;
    u16 con_handle;

    if (packet_type != HCI_EVENT_PACKET || !packet || !size) {
        return;
    }

    event_type = hci_event_packet_get_type(packet);
    switch (event_type) {
    case HCI_EVENT_LE_META:
        subevent = hci_event_le_meta_get_subevent_code(packet);
        if (subevent == HCI_SUBEVENT_LE_CONNECTION_COMPLETE
            && !hci_subevent_le_connection_complete_get_status(packet)) {
            con_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
            rdx_ble.con_handle = con_handle;
            rdx_ble.mtu = 23;
            rdx_ble.tx_ccc = 0;
            rdx_mvp0_protocol_set_connected(1);
            att_server_set_exchange_mtu(con_handle);
            printf("[RDX] connected handle=%04x interval=%u latency=%u"
                   " timeout=%u\n",
                   con_handle,
                   hci_subevent_le_connection_complete_get_conn_interval(packet),
                   hci_subevent_le_connection_complete_get_conn_latency(packet),
                   hci_subevent_le_connection_complete_get_supervision_timeout(packet));
            rdx_ble_request_record_conn_param("connected");
            rdx_ble_request_high_throughput("connected");
        } else if (subevent == HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE) {
            u8 status = hci_subevent_le_connection_update_complete_get_status(packet);
            u16 interval = hci_subevent_le_connection_update_complete_get_conn_interval(packet);
            u16 latency = hci_subevent_le_connection_update_complete_get_conn_latency(packet);
            u16 timeout = hci_subevent_le_connection_update_complete_get_supervision_timeout(packet);

            printf("[RDX][BLE] conn_param_update status=%u interval=%u"
                   " latency=%u timeout=%u streaming=%u\n",
                   status, interval, latency, timeout,
                   rdx_ble.record_streaming);
            if (!status
                && (interval > RDX_RECORD_CONN_INTERVAL_MAX || latency)) {
                rdx_ble_request_record_conn_param("slow_update");
            }
        } else if (subevent == HCI_SUBEVENT_LE_DATA_LENGTH_CHANGE) {
            printf("[RDX][BLE] data_length tx_octets=%u tx_time=%u"
                   " rx_octets=%u rx_time=%u\n",
                   hci_subevent_le_data_length_change_get_max_tx_octets(packet),
                   hci_subevent_le_data_length_change_get_max_tx_time(packet),
                   hci_subevent_le_data_length_change_get_max_rx_octets(packet),
                   hci_subevent_le_data_length_change_get_max_rx_time(packet));
        } else if (subevent == HCI_SUBEVENT_LE_PHY_UPDATE_COMPLETE) {
            printf("[RDX][BLE] phy_update status=%u tx=%u rx=%u\n",
                   hci_event_le_meta_get_phy_update_complete_status(packet),
                   hci_event_le_meta_get_phy_update_complete_tx_phy(packet),
                   hci_event_le_meta_get_phy_update_complete_rx_phy(packet));
        }
        break;

    case ATT_EVENT_MTU_EXCHANGE_COMPLETE:
        rdx_ble.mtu = att_event_mtu_exchange_complete_get_MTU(packet);
        printf("[RDX] mtu=%u\n", rdx_ble.mtu);
        break;

    case HCI_EVENT_DISCONNECTION_COMPLETE:
        con_handle = hci_event_disconnection_complete_get_connection_handle(packet);
        if (!rdx_ble.con_handle || con_handle == rdx_ble.con_handle) {
            printf("[RDX] disconnected reason=%02x\n",
                   hci_event_disconnection_complete_get_reason(packet));
            rdx_ble.con_handle = 0;
            rdx_ble.mtu = 0;
            rdx_ble.tx_ccc = 0;
            rdx_mvp0_protocol_set_connected(0);
            if (rdx_ble.initialized) {
                rdx_ble_adv_enable(1);
            }
        }
        break;

    default:
        break;
    }
}

static u16 rdx_ble_att_read_callback(void *hdl, u16 connection_handle, u16 att_handle,
                                     u16 offset, u8 *buffer, u16 buffer_size)
{
    const u8 *data = NULL;
    const char *name;
    u16 data_len = 0;
    u16 ccc;
    u8 value[2];

    switch (att_handle) {
    case RDX_HANDLE_GAP_NAME:
        name = rdx_ble_name_get();
        data = (const u8 *)name;
        data_len = strlen(name);
        break;

    case RDX_HANDLE_IDENTITY:
        data_len = rdx_identity_get_read_value(&data);
        rdx_mvp0_protocol_set_identity_read();
        break;

    case RDX_HANDLE_BATTERY:
        value[0] = RDX_MVP0_FIXED_BATTERY_LEVEL;
        data = value;
        data_len = 1;
        break;

    case RDX_HANDLE_TX_CCC:
    case RDX_HANDLE_BATTERY_CCC:
    case RDX_HANDLE_OTA_TX_CCC:
        ccc = multi_att_get_ccc_config(connection_handle, att_handle);
        value[0] = ccc & 0xff;
        value[1] = ccc >> 8;
        data = value;
        data_len = sizeof(value);
        break;

    default:
        break;
    }

    return rdx_ble_read_blob(data, data_len, offset, buffer, buffer_size);
}

static int rdx_ble_att_write_callback(void *hdl, u16 connection_handle, u16 att_handle,
                                      u16 transaction_mode, u16 offset, u8 *buffer, u16 buffer_size)
{
    u16 ccc;

    if (!buffer || !buffer_size) {
        return 0;
    }

    switch (att_handle) {
    case RDX_HANDLE_RX:
        rdx_mvp0_protocol_receive(buffer, buffer_size);
        break;

    case RDX_HANDLE_TX_CCC:
    case RDX_HANDLE_BATTERY_CCC:
    case RDX_HANDLE_OTA_TX_CCC:
        ccc = buffer[0];
        if (buffer_size > 1) {
            ccc |= (u16)buffer[1] << 8;
        }
        multi_att_set_ccc_config(connection_handle, att_handle, ccc);
        if (att_handle == RDX_HANDLE_TX_CCC) {
            rdx_ble.tx_ccc = !!(ccc & 0x0001);
            rdx_mvp0_protocol_set_ccc(rdx_ble.tx_ccc);
            printf("[RDX] tx ccc=%u\n", rdx_ble.tx_ccc);
        }
        break;

    case RDX_HANDLE_OTA_RX:
        printf("[RDX] OTA write ignored len=%u\n", buffer_size);
        break;

    default:
        break;
    }
    return 0;
}

int rdx_ble_transport_init(void)
{
    int ret;

    if (rdx_ble.hdl) {
        return 0;
    }

    memset(&rdx_ble, 0, sizeof(rdx_ble));

    rdx_ble.hdl = app_ble_hdl_alloc();
    if (!rdx_ble.hdl) {
        printf("[RDX] BLE handle alloc failed\n");
        return -1;
    }

    app_ble_hdl_uuid_set(rdx_ble.hdl, RDX_BLE_HANDLE_UUID);
    app_ble_set_mac_addr(rdx_ble.hdl, (void *)rdx_identity_get_ble_mac());
    app_ble_profile_set(rdx_ble.hdl, rdx_profile_data);
    app_ble_att_read_callback_register(rdx_ble.hdl, rdx_ble_att_read_callback);
    app_ble_att_write_callback_register(rdx_ble.hdl, rdx_ble_att_write_callback);
    app_ble_att_server_packet_handler_register(rdx_ble.hdl, rdx_ble_packet_handler);
    app_ble_hci_event_callback_register(rdx_ble.hdl, rdx_ble_packet_handler);
    app_ble_l2cap_packet_handler_register(rdx_ble.hdl, rdx_ble_packet_handler);

    rdx_ble.initialized = 1;
    ret = rdx_ble_adv_enable(1);
    if (ret) {
        rdx_ble.initialized = 0;
        app_ble_hdl_free(rdx_ble.hdl);
        rdx_ble.hdl = NULL;
        return ret;
    }
    return 0;
}

void rdx_ble_transport_exit(void)
{
    if (!rdx_ble.hdl) {
        return;
    }

    rdx_ble.initialized = 0;
    rdx_ble_adv_enable(0);
    rdx_ble_transport_disconnect();
    app_ble_hdl_free(rdx_ble.hdl);
    memset(&rdx_ble, 0, sizeof(rdx_ble));
}

int rdx_ble_transport_send(const u8 *data, u16 len)
{
    int valid_len;

    if (!rdx_ble.hdl || !rdx_ble.con_handle || !rdx_ble.tx_ccc || !data || !len) {
        return -1;
    }

    valid_len = app_ble_att_vaild_len_get(rdx_ble.hdl);
    if (valid_len <= 0 || len > valid_len) {
        return -2;
    }

    return app_ble_att_send_data(rdx_ble.hdl, RDX_HANDLE_TX, (u8 *)data,
                                 len, ATT_OP_AUTO_READ_CCC);
}

int rdx_ble_transport_set_record_streaming(u8 enabled)
{
    rdx_ble.record_streaming = !!enabled;
    if (!rdx_ble.record_streaming) {
        return 0;
    }
    rdx_ble_request_high_throughput("record_start");
    return rdx_ble_request_record_conn_param("record_start");
}

int rdx_ble_transport_refresh_local_name(void)
{
    int adv_enabled;
    int ret;

    if (!rdx_ble.hdl || !rdx_ble.initialized) {
        printf("[RDX][BLE] name_refresh state=inactive ret=-1\n");
        return -1;
    }
    if (rdx_ble.con_handle) {
        printf("[RDX][BLE] name_refresh state=connected action=defer_until_disconnect ret=0\n");
        return 0;
    }

    adv_enabled = app_ble_adv_state_get(rdx_ble.hdl);
    if (adv_enabled <= 0) {
        printf("[RDX][BLE] name_refresh state=off action=defer_until_enable ret=0\n");
        return 0;
    }

    ret = rdx_ble_adv_enable(0);
    if (!ret) {
        ret = rdx_ble_adv_enable(1);
    }
    printf("[RDX][BLE] name_refresh state=advertising action=restart ret=%d\n",
           ret);
    return ret;
}

void rdx_ble_transport_disconnect(void)
{
    if (rdx_ble.hdl && app_ble_get_hdl_con_handle(rdx_ble.hdl)) {
        app_ble_disconnect(rdx_ble.hdl);
    }
}

u8 rdx_ble_transport_is_connected(void)
{
    return rdx_ble.con_handle != 0;
}

#endif
