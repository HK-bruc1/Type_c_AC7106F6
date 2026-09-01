#include "app_config.h"

#if TCFG_RDX_ENABLE

#include "system/includes.h"
#include "spinlock.h"
#include "app_ble_spp_api.h"
#include "btstack/btstack_event.h"
#include "btstack/le/att.h"
#include "btstack/le/ble_api.h"
#include "rdx/ports/rdx_product_profile_port.h"
#include "rdx/rdx_ble_name_service.h"
#include "rdx/rdx_identity_service.h"
#include "rdx/ports/rdx_transport_port.h"

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
#define RDX_BLE_APP_SEND_RET_SLOTS               4

struct rdx_ble_app_send_ret_stat {
    int code;
    u32 count;
};

struct rdx_ble_transport_tx_stats {
    u32 unavailable_count;
    u32 valid_len_busy_count;
    u32 app_send_ok_count;
    u32 app_send_error_count;
    u32 app_send_ret_overflow_count;
    u32 can_send_request_count;
    u32 can_send_event_count;
    u32 can_send_ready_count;
    u32 can_send_coalesced_count;
    u32 can_send_timeout_rearm_count;
    u32 spurious_event_count;
    u32 valid_len_sample_count;
    u16 valid_len_min;
    u16 valid_len_max;
    u16 mtu;
    u16 conn_interval;
    u16 conn_latency;
    u16 dle_tx_octets;
    u16 dle_tx_time;
    u8 tx_phy;
    u8 rx_phy;
    u8 can_send_pending;
    struct rdx_ble_app_send_ret_stat
        app_send_ret[RDX_BLE_APP_SEND_RET_SLOTS];
};

static const struct conn_update_param_t rdx_record_conn_param = {
    RDX_RECORD_CONN_INTERVAL_MIN,
    RDX_RECORD_CONN_INTERVAL_MAX,
    0,
    600,
};

typedef struct {
    spinlock_t tx_lock;
    void *hdl;
    u16 con_handle;
    u16 mtu;
    u16 conn_interval;
    u16 conn_latency;
    u16 dle_tx_octets;
    u16 dle_tx_time;
    u8 tx_ccc;
    u8 ota_tx_ccc;
    u8 initialized;
    u8 record_streaming;
    u8 tx_phy;
    u8 rx_phy;
    u8 can_send_pending;
    u32 connection_generation;
    u32 can_send_connection_generation;
    u32 can_send_session_id;
    u32 can_send_capture_generation;
    u32 can_send_request_ms;
    rdx_transport_event_callback_t event_callback;
    void *event_context;
    struct rdx_ble_transport_tx_stats tx_stats;
} rdx_ble_transport_t;

static rdx_ble_transport_t rdx_ble;

static u32 rdx_ble_next_generation(u32 generation)
{
    generation++;
    return generation ? generation : 1;
}

static void rdx_ble_emit_event(struct rdx_transport_event *event)
{
    if (!event || !rdx_ble.event_callback) {
        return;
    }
    event->connection_generation = rdx_ble.connection_generation;
    rdx_ble.event_callback(event, rdx_ble.event_context);
}

static void rdx_ble_clear_can_send_locked(void)
{
    rdx_ble.can_send_pending = 0;
    rdx_ble.can_send_connection_generation = 0;
    rdx_ble.can_send_session_id = 0;
    rdx_ble.can_send_capture_generation = 0;
    rdx_ble.can_send_request_ms = 0;
}

static void rdx_ble_clear_can_send(void)
{
    spin_lock(&rdx_ble.tx_lock);
    rdx_ble_clear_can_send_locked();
    spin_unlock(&rdx_ble.tx_lock);
}

#if RDX_CFG_BLE_TX_DIAG_ENABLE
static void rdx_ble_track_app_send_ret(int ret)
{
    u8 i;

    for (i = 0; i < RDX_BLE_APP_SEND_RET_SLOTS; i++) {
        if (rdx_ble.tx_stats.app_send_ret[i].count
            && rdx_ble.tx_stats.app_send_ret[i].code == ret) {
            rdx_ble.tx_stats.app_send_ret[i].count++;
            return;
        }
    }
    for (i = 0; i < RDX_BLE_APP_SEND_RET_SLOTS; i++) {
        if (!rdx_ble.tx_stats.app_send_ret[i].count) {
            rdx_ble.tx_stats.app_send_ret[i].code = ret;
            rdx_ble.tx_stats.app_send_ret[i].count = 1;
            return;
        }
    }
    rdx_ble.tx_stats.app_send_ret_overflow_count++;
}
#endif

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

    /* RDX V23 OTA service. */
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

        ret = app_ble_set_adv_param(
                  rdx_ble.hdl,
                  rdx_product_profile_get()->ble_adv_interval,
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
    struct rdx_transport_event transport_event;
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
            spin_lock(&rdx_ble.tx_lock);
            rdx_ble.connection_generation = rdx_ble_next_generation(
                rdx_ble.connection_generation);
            rdx_ble_clear_can_send_locked();
            spin_unlock(&rdx_ble.tx_lock);
            rdx_ble.con_handle = con_handle;
            rdx_ble.mtu = 23;
            rdx_ble.conn_interval =
                hci_subevent_le_connection_complete_get_conn_interval(packet);
            rdx_ble.conn_latency =
                hci_subevent_le_connection_complete_get_conn_latency(packet);
            rdx_ble.dle_tx_octets = 27;
            rdx_ble.dle_tx_time = 328;
            rdx_ble.tx_phy = 1;
            rdx_ble.rx_phy = 1;
            rdx_ble.tx_ccc = 0;
            rdx_ble.ota_tx_ccc = 0;
            memset(&transport_event, 0, sizeof(transport_event));
            transport_event.type = RDX_TRANSPORT_EVENT_CONNECTED;
            rdx_ble_emit_event(&transport_event);
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
            if (!status) {
                rdx_ble.conn_interval = interval;
                rdx_ble.conn_latency = latency;
            }
            if (!status
                && (interval > RDX_RECORD_CONN_INTERVAL_MAX || latency)) {
                rdx_ble_request_record_conn_param("slow_update");
            }
        } else if (subevent == HCI_SUBEVENT_LE_DATA_LENGTH_CHANGE) {
            rdx_ble.dle_tx_octets =
                hci_subevent_le_data_length_change_get_max_tx_octets(packet);
            rdx_ble.dle_tx_time =
                hci_subevent_le_data_length_change_get_max_tx_time(packet);
            printf("[RDX][BLE] data_length tx_octets=%u tx_time=%u"
                   " rx_octets=%u rx_time=%u\n",
                   hci_subevent_le_data_length_change_get_max_tx_octets(packet),
                   hci_subevent_le_data_length_change_get_max_tx_time(packet),
                   hci_subevent_le_data_length_change_get_max_rx_octets(packet),
                   hci_subevent_le_data_length_change_get_max_rx_time(packet));
        } else if (subevent == HCI_SUBEVENT_LE_PHY_UPDATE_COMPLETE) {
            if (!hci_event_le_meta_get_phy_update_complete_status(packet)) {
                rdx_ble.tx_phy =
                    hci_event_le_meta_get_phy_update_complete_tx_phy(packet);
                rdx_ble.rx_phy =
                    hci_event_le_meta_get_phy_update_complete_rx_phy(packet);
            }
            printf("[RDX][BLE] phy_update status=%u tx=%u rx=%u\n",
                   hci_event_le_meta_get_phy_update_complete_status(packet),
                   hci_event_le_meta_get_phy_update_complete_tx_phy(packet),
                   hci_event_le_meta_get_phy_update_complete_rx_phy(packet));
        }
        break;

    case ATT_EVENT_CAN_SEND_NOW: {
#if RDX_CFG_BLE_TX_CAN_SEND_ENABLE && RDX_CFG_ONLINE_RECORDING_ENABLE
        u32 session_id = 0;
        u32 capture_generation = 0;
        u8 ready = 0;
#endif

        spin_lock(&rdx_ble.tx_lock);
#if RDX_CFG_BLE_TX_DIAG_ENABLE
        if (rdx_ble.record_streaming) {
            rdx_ble.tx_stats.can_send_event_count++;
        }
#endif
#if RDX_CFG_BLE_TX_CAN_SEND_ENABLE && RDX_CFG_ONLINE_RECORDING_ENABLE
        if (rdx_ble.record_streaming
            && rdx_ble.can_send_pending
            && rdx_ble.can_send_connection_generation ==
                rdx_ble.connection_generation) {
            session_id = rdx_ble.can_send_session_id;
            capture_generation = rdx_ble.can_send_capture_generation;
            ready = 1;
#if RDX_CFG_BLE_TX_DIAG_ENABLE
            rdx_ble.tx_stats.can_send_ready_count++;
#endif
        } else {
#if RDX_CFG_BLE_TX_DIAG_ENABLE
            if (rdx_ble.record_streaming) {
                rdx_ble.tx_stats.spurious_event_count++;
            }
#endif
        }
#else
#if RDX_CFG_BLE_TX_DIAG_ENABLE
        if (rdx_ble.record_streaming && !rdx_ble.can_send_pending) {
            rdx_ble.tx_stats.spurious_event_count++;
        }
#endif
#endif
        rdx_ble_clear_can_send_locked();
        spin_unlock(&rdx_ble.tx_lock);
#if RDX_CFG_BLE_TX_CAN_SEND_ENABLE && RDX_CFG_ONLINE_RECORDING_ENABLE
        if (ready) {
            memset(&transport_event, 0, sizeof(transport_event));
            transport_event.type = RDX_TRANSPORT_EVENT_CAN_SEND_NOW;
            transport_event.data.can_send.session_id = session_id;
            transport_event.data.can_send.capture_generation =
                capture_generation;
            rdx_ble_emit_event(&transport_event);
        }
#endif
        break;
    }

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
            rdx_ble_clear_can_send();
            rdx_ble.tx_ccc = 0;
            rdx_ble.ota_tx_ccc = 0;
            memset(&transport_event, 0, sizeof(transport_event));
            transport_event.type = RDX_TRANSPORT_EVENT_LINK_LOST;
            rdx_ble_emit_event(&transport_event);
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
    struct rdx_transport_event transport_event;
    const u8 *data = NULL;
    const char *name;
    u16 data_len = 0;
    u16 ccc;
    u8 value[2];

    if (!rdx_ble.con_handle || connection_handle != rdx_ble.con_handle) {
        return 0;
    }

    switch (att_handle) {
    case RDX_HANDLE_GAP_NAME:
        name = rdx_ble_name_get();
        data = (const u8 *)name;
        data_len = strlen(name);
        break;

    case RDX_HANDLE_IDENTITY:
        data_len = rdx_identity_get_read_value(&data);
        memset(&transport_event, 0, sizeof(transport_event));
        transport_event.type = RDX_TRANSPORT_EVENT_IDENTITY_READ;
        rdx_ble_emit_event(&transport_event);
        break;

    case RDX_HANDLE_BATTERY:
        value[0] = rdx_product_profile_get()->fixed_battery_level;
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
    struct rdx_transport_event transport_event;
    u16 ccc;

    if (!buffer || !buffer_size) {
        return 0;
    }
    if (!rdx_ble.con_handle || connection_handle != rdx_ble.con_handle) {
        return 0;
    }

    switch (att_handle) {
    case RDX_HANDLE_RX:
        memset(&transport_event, 0, sizeof(transport_event));
        transport_event.type = RDX_TRANSPORT_EVENT_RX;
        transport_event.data.rx.data = buffer;
        transport_event.data.rx.len = buffer_size;
        rdx_ble_emit_event(&transport_event);
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
            if (!rdx_ble.tx_ccc) {
                rdx_ble_clear_can_send();
            }
            memset(&transport_event, 0, sizeof(transport_event));
            transport_event.type = RDX_TRANSPORT_EVENT_CCC_CHANGED;
            transport_event.data.ccc_enabled = rdx_ble.tx_ccc;
            rdx_ble_emit_event(&transport_event);
            printf("[RDX] tx ccc=%u\n", rdx_ble.tx_ccc);
        } else if (att_handle == RDX_HANDLE_OTA_TX_CCC) {
            rdx_ble.ota_tx_ccc = !!(ccc & 0x0001);
            memset(&transport_event, 0, sizeof(transport_event));
            transport_event.type = RDX_TRANSPORT_EVENT_OTA_CCC_CHANGED;
            transport_event.data.ccc_enabled = rdx_ble.ota_tx_ccc;
            rdx_ble_emit_event(&transport_event);
            printf("[RDX][OTA] tx ccc=%u\n", rdx_ble.ota_tx_ccc);
        }
        break;

    case RDX_HANDLE_OTA_RX:
        if (transaction_mode || offset) {
            printf("[RDX][OTA] write_rejected transaction=%u"
                   " offset=%u len=%u\n",
                   transaction_mode, offset, buffer_size);
            break;
        }
        memset(&transport_event, 0, sizeof(transport_event));
        transport_event.type = RDX_TRANSPORT_EVENT_OTA_RX;
        transport_event.data.rx.data = buffer;
        transport_event.data.rx.len = buffer_size;
        rdx_ble_emit_event(&transport_event);
        break;

    default:
        break;
    }
    return 0;
}

int rdx_transport_init(rdx_transport_event_callback_t event_callback,
                       void *context)
{
    const uint8_t *ble_mac;
    int ret;

    if (!event_callback) {
        return -1;
    }
    if (rdx_ble.hdl) {
        rdx_ble.event_callback = event_callback;
        rdx_ble.event_context = context;
        return 0;
    }

    memset(&rdx_ble, 0, sizeof(rdx_ble));
    spin_lock_init(&rdx_ble.tx_lock);
    rdx_ble.event_callback = event_callback;
    rdx_ble.event_context = context;

    ble_mac = rdx_identity_get_ble_mac();
    ret = le_controller_set_mac((void *)ble_mac);
    if (ret) {
        printf("[RDX][BLE] controller_mac_set_failed ret=%d\n", ret);
        return RDX_TRANSPORT_ERR_CONTROLLER_MAC;
    }

    rdx_ble.hdl = app_ble_hdl_alloc();
    if (!rdx_ble.hdl) {
        printf("[RDX] BLE handle alloc failed\n");
        return RDX_TRANSPORT_ERR_HANDLE_ALLOC;
    }

    app_ble_hdl_uuid_set(rdx_ble.hdl, RDX_BLE_HANDLE_UUID);
    ret = app_ble_set_mac_addr(rdx_ble.hdl, (void *)ble_mac);
    if (ret) {
        printf("[RDX][BLE] handle_mac_set_failed ret=%d\n", ret);
        app_ble_hdl_free(rdx_ble.hdl);
        rdx_ble.hdl = NULL;
        return RDX_TRANSPORT_ERR_HANDLE_MAC;
    }
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
        printf("[RDX][BLE] advertising_start_failed ret=%d\n", ret);
        return RDX_TRANSPORT_ERR_ADVERTISING;
    }
    return 0;
}

void rdx_transport_exit(void)
{
    if (!rdx_ble.hdl) {
        return;
    }

    rdx_ble.initialized = 0;
    rdx_ble_adv_enable(0);
    rdx_transport_disconnect();
    app_ble_hdl_free(rdx_ble.hdl);
    memset(&rdx_ble, 0, sizeof(rdx_ble));
}

enum rdx_transport_result rdx_transport_send(
    const uint8_t *data, uint16_t len)
{
    int valid_len;
    int ret;

    if (!data || !len) {
        return RDX_TRANSPORT_FATAL;
    }
    if (!rdx_ble.hdl || !rdx_ble.con_handle || !rdx_ble.tx_ccc) {
#if RDX_CFG_BLE_TX_DIAG_ENABLE
        if (rdx_ble.record_streaming) {
            rdx_ble.tx_stats.unavailable_count++;
        }
#endif
        return RDX_TRANSPORT_UNAVAILABLE;
    }

    valid_len = app_ble_att_vaild_len_get(rdx_ble.hdl);
#if RDX_CFG_BLE_TX_DIAG_ENABLE
    if (rdx_ble.record_streaming && valid_len > 0) {
        u16 sample = valid_len > 0xffff ? 0xffff : valid_len;

        rdx_ble.tx_stats.valid_len_sample_count++;
        if (!rdx_ble.tx_stats.valid_len_min
            || sample < rdx_ble.tx_stats.valid_len_min) {
            rdx_ble.tx_stats.valid_len_min = sample;
        }
        if (sample > rdx_ble.tx_stats.valid_len_max) {
            rdx_ble.tx_stats.valid_len_max = sample;
        }
    }
#endif
    if (valid_len <= 0 || len > valid_len) {
#if RDX_CFG_BLE_TX_DIAG_ENABLE
        if (rdx_ble.record_streaming) {
            rdx_ble.tx_stats.valid_len_busy_count++;
        }
#endif
        return RDX_TRANSPORT_RETRYABLE;
    }

    ret = app_ble_att_send_data(rdx_ble.hdl, RDX_HANDLE_TX, (u8 *)data,
                                len, ATT_OP_AUTO_READ_CCC);
#if RDX_CFG_BLE_TX_DIAG_ENABLE
    if (rdx_ble.record_streaming) {
        if (ret) {
            rdx_ble.tx_stats.app_send_error_count++;
            rdx_ble_track_app_send_ret(ret);
        } else {
            rdx_ble.tx_stats.app_send_ok_count++;
        }
    }
#endif
    if (!ret) {
        return RDX_TRANSPORT_ACCEPTED;
    }
    if (ret == BLE_CMD_RET_BUSY || ret == BLE_BUFFER_FULL) {
        return RDX_TRANSPORT_RETRYABLE;
    }
    if (ret == BLE_CMD_STACK_NOT_RUN || ret == BLE_CMD_CCC_FAIL) {
        return RDX_TRANSPORT_UNAVAILABLE;
    }
    return RDX_TRANSPORT_FATAL;
}

enum rdx_transport_result rdx_transport_ota_send(
    const uint8_t *data, uint16_t len,
    uint32_t connection_generation)
{
    int valid_len;
    int ret;

    if (!data || !len || !rdx_ble.hdl || !rdx_ble.con_handle
        || !rdx_ble.ota_tx_ccc
        || !connection_generation
        || connection_generation != rdx_ble.connection_generation) {
        return RDX_TRANSPORT_UNAVAILABLE;
    }
    valid_len = app_ble_att_vaild_len_get(rdx_ble.hdl);
    if (valid_len <= 0 || len > valid_len) {
        return RDX_TRANSPORT_RETRYABLE;
    }
    ret = app_ble_att_send_data(rdx_ble.hdl, RDX_HANDLE_OTA_TX,
                                (u8 *)data, len,
                                ATT_OP_AUTO_READ_CCC);
    if (!ret) {
        return RDX_TRANSPORT_ACCEPTED;
    }
    if (ret == BLE_CMD_RET_BUSY || ret == BLE_BUFFER_FULL) {
        return RDX_TRANSPORT_RETRYABLE;
    }
    if (ret == BLE_CMD_STACK_NOT_RUN || ret == BLE_CMD_CCC_FAIL) {
        return RDX_TRANSPORT_UNAVAILABLE;
    }
    return RDX_TRANSPORT_FATAL;
}

int rdx_transport_request_can_send_now(
    uint32_t session_id, uint32_t capture_generation)
{
#if RDX_CFG_BLE_TX_CAN_SEND_ENABLE && RDX_CFG_ONLINE_RECORDING_ENABLE
    u16 con_handle;
    u32 now_ms;

    now_ms = sys_timer_get_ms();
    spin_lock(&rdx_ble.tx_lock);
    if (!rdx_ble.hdl || !rdx_ble.con_handle || !rdx_ble.tx_ccc
        || !rdx_ble.record_streaming || !session_id
        || !capture_generation) {
        spin_unlock(&rdx_ble.tx_lock);
        return -1;
    }
    if (rdx_ble.can_send_pending) {
        if (rdx_ble.can_send_connection_generation ==
                rdx_ble.connection_generation
            && rdx_ble.can_send_session_id == session_id
            && rdx_ble.can_send_capture_generation == capture_generation
            && (u32)(now_ms - rdx_ble.can_send_request_ms) <
                RDX_CFG_BLE_TX_CAN_SEND_TIMEOUT_MS) {
#if RDX_CFG_BLE_TX_DIAG_ENABLE
            rdx_ble.tx_stats.can_send_coalesced_count++;
#endif
            spin_unlock(&rdx_ble.tx_lock);
            return 0;
        }
#if RDX_CFG_BLE_TX_DIAG_ENABLE
        rdx_ble.tx_stats.can_send_timeout_rearm_count++;
#endif
        rdx_ble_clear_can_send_locked();
    }

    con_handle = rdx_ble.con_handle;
    rdx_ble.can_send_pending = 1;
    rdx_ble.can_send_connection_generation =
        rdx_ble.connection_generation;
    rdx_ble.can_send_session_id = session_id;
    rdx_ble.can_send_capture_generation = capture_generation;
    rdx_ble.can_send_request_ms = now_ms;
#if RDX_CFG_BLE_TX_DIAG_ENABLE
    rdx_ble.tx_stats.can_send_request_count++;
#endif
    spin_unlock(&rdx_ble.tx_lock);
    att_server_request_can_send_now_event(con_handle);
    return 0;
#else
    (void)session_id;
    (void)capture_generation;
    return -1;
#endif
}

int rdx_transport_set_record_streaming(uint8_t enabled)
{
    spin_lock(&rdx_ble.tx_lock);
    rdx_ble.record_streaming = !!enabled;
    if (!rdx_ble.record_streaming) {
        rdx_ble_clear_can_send_locked();
        spin_unlock(&rdx_ble.tx_lock);
        return 0;
    }
    spin_unlock(&rdx_ble.tx_lock);
    rdx_ble_request_high_throughput("record_start");
    return rdx_ble_request_record_conn_param("record_start");
}

void rdx_transport_tx_diagnostics_reset(void)
{
    spin_lock(&rdx_ble.tx_lock);
    memset(&rdx_ble.tx_stats, 0, sizeof(rdx_ble.tx_stats));
    rdx_ble_clear_can_send_locked();
    spin_unlock(&rdx_ble.tx_lock);
}

#if RDX_CFG_BLE_TX_DIAG_ENABLE
static void rdx_ble_tx_stats_get(
    struct rdx_ble_transport_tx_stats *stats)
{
    if (!stats) {
        return;
    }

    spin_lock(&rdx_ble.tx_lock);
    memcpy(stats, &rdx_ble.tx_stats, sizeof(*stats));
    stats->can_send_pending = rdx_ble.can_send_pending;
    spin_unlock(&rdx_ble.tx_lock);
    stats->mtu = rdx_ble.mtu;
    stats->conn_interval = rdx_ble.conn_interval;
    stats->conn_latency = rdx_ble.conn_latency;
    stats->dle_tx_octets = rdx_ble.dle_tx_octets;
    stats->dle_tx_time = rdx_ble.dle_tx_time;
    stats->tx_phy = rdx_ble.tx_phy;
    stats->rx_phy = rdx_ble.rx_phy;
}
#endif

void rdx_transport_tx_diagnostics_log(void)
{
#if RDX_CFG_BLE_TX_DIAG_ENABLE
    struct rdx_ble_transport_tx_stats stats;
    u8 i;

    rdx_ble_tx_stats_get(&stats);
    printf("[RDX][TXQ] ble unavailable=%u busy_valid_len=%u"
           " app_ok=%u app_error=%u valid_samples=%u"
           " valid_min=%u valid_max=%u\n",
           (unsigned int)stats.unavailable_count,
           (unsigned int)stats.valid_len_busy_count,
           (unsigned int)stats.app_send_ok_count,
           (unsigned int)stats.app_send_error_count,
           (unsigned int)stats.valid_len_sample_count,
           stats.valid_len_min, stats.valid_len_max);
    printf("[RDX][TXQ] can_req=%u can_evt=%u spurious_evt=%u"
           " can_ready=%u can_coalesced=%u timeout_rearm=%u pending=%u"
           "\n",
           (unsigned int)stats.can_send_request_count,
           (unsigned int)stats.can_send_event_count,
           (unsigned int)stats.spurious_event_count,
           (unsigned int)stats.can_send_ready_count,
           (unsigned int)stats.can_send_coalesced_count,
           (unsigned int)stats.can_send_timeout_rearm_count,
           stats.can_send_pending);
    printf("[RDX][TXQ] mtu=%u interval=%u latency=%u phy=%u/%u"
           " dle=%u/%u ret_overflow=%u\n",
           stats.mtu, stats.conn_interval,
           stats.conn_latency, stats.tx_phy, stats.rx_phy,
           stats.dle_tx_octets, stats.dle_tx_time,
           (unsigned int)stats.app_send_ret_overflow_count);
    for (i = 0; i < RDX_BLE_APP_SEND_RET_SLOTS; i++) {
        if (stats.app_send_ret[i].count) {
            printf("[RDX][TXQ] app_ret code=%d count=%u\n",
                   stats.app_send_ret[i].code,
                   (unsigned int)stats.app_send_ret[i].count);
        }
    }
#endif
}

int rdx_transport_refresh_local_name(void)
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

void rdx_transport_disconnect(void)
{
    if (rdx_ble.hdl && app_ble_get_hdl_con_handle(rdx_ble.hdl)) {
        app_ble_disconnect(rdx_ble.hdl);
    }
}

#endif
