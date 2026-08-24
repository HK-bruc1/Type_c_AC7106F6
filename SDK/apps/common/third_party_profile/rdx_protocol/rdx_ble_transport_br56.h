#ifndef RDX_BLE_TRANSPORT_BR56_H
#define RDX_BLE_TRANSPORT_BR56_H

#include "system/includes.h"

#define RDX_BLE_APP_SEND_RET_SLOTS              4

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

int rdx_ble_transport_init(void);
void rdx_ble_transport_exit(void);
int rdx_ble_transport_send(const u8 *data, u16 len);
int rdx_ble_transport_request_can_send_now(
    u32 session_id, u32 capture_generation);
int rdx_ble_transport_set_record_streaming(u8 enabled);
void rdx_ble_transport_tx_stats_reset(void);
void rdx_ble_transport_tx_stats_get(
    struct rdx_ble_transport_tx_stats *stats);
int rdx_ble_transport_refresh_local_name(void);
void rdx_ble_transport_disconnect(void);
u8 rdx_ble_transport_is_connected(void);

#endif
