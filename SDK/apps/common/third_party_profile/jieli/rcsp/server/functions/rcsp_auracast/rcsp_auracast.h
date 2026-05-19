#ifndef __RCSP_AURACAST_H__
#define __RCSP_AURACAST_H__

#include "typedef.h"
#include "app_config.h"
#include "rcsp_config.h"
#include "rcsp.h"
#include "ble_rcsp_server.h"
#include "le/auracast_sink_api.h"
#include "app_le_auracast.h"

//#define AURACAST_APP_OPCODE_RECV_GET_DEVICE_INFO             (0x01)
#define AURACAST_APP_OPCODE_RECV_LOGIN_AUTHENTICATION        (0x20)
#define AURACAST_APP_OPCODE_RECV_LOGIN_PASSWORD_SET          (0x21)
#define AURACAST_APP_OPCODE_RECV_BROADCAST_SETTING           (0x22)
#define AURACAST_APP_OPCODE_RECV_DEVICE_RESET                (0x23)

#define AURACAST_APP_OPCODE_RECV_SCAN_CONTROL                (0x01)
#define AURACAST_APP_OPCODE_NOTIFY_SOURCE_LIST               (0x02)
#define AURACAST_APP_OPCODE_RECV_DEVICE_STATUS               (0x03)
#define AURACAST_APP_OPCODE_RECV_LISTENING_CONTROL           (0x04)
#define AURACAST_APP_OPCODE_NOTIFY_LISTENING_STATUS          (0x05)

struct auracast_source_item_t {
    char broadcast_name[32];
    u8   broadcast_id[3];
    u8   broadcast_features;
    u8   adv_address[6];
    u8   listening_state;   // 0:idle  1:to listen  2:listenig
    u8   listening_state_error;
};

#define AURACAST_NAME_MAX_LEN               32
#define AURACAST_PASSWORD_MAX_LEN           32
#define AURACAST_CODE_MAX_LEN               16

struct _auracast_tx_vm_config {
    u8 auracast_password[32];
    u8 auracast_name[32];
    u8 auracast_broadcast_code[16];
    u8 auracast_encryption_en;
    u8 auracast_password_len;
    u8 auracast_name_len;
    u8 auracast_audio_index;
    u8 auracast_tx_power_level;
    u8 reserve_1;
    u8 reserve_2;
    u8 reserve_3;
};


// common
extern int auracast_app_packet_receive(u8 opcode, u8 sn, u8 *payload, u32 payload_len, u16 ble_con_handle, u8 *spp_remote_addr);
extern int auracast_app_packet_response(u8 status, u8 opcode, u8 sn, u8 *payload, u32 payload_len, u16 ble_con_handle, u8 *spp_remote_addr);
extern int auracast_app_packet_cmd(u8 *payload, u32 payload_len);

// source
extern struct _auracast_tx_vm_config *tx_config;
extern int auracast_app_read_vm_config(struct _auracast_tx_vm_config *config);
extern void auracast_app_reset_vm_config(struct _auracast_tx_vm_config *config);
extern int auracast_app_login_authentication_deal(u8 opcode, u8 sn, u8 *payload, u32 payload_len);
extern int auracast_app_login_password_set_deal(u8 opcode, u8 sn, u8 *payload, u32 payload_len);
extern int auracast_app_broadcast_setting_deal(u8 opcode, u8 sn, u8 *payload, u32 payload_len);
extern void auracast_app_cpu_reset_do(void *priv);
extern int auracast_app_broadcast_reset_deal(u8 opcode, u8 sn, u8 *payload, u32 payload_len);

extern int auracast_app_source_api_init(void);
extern int auracast_app_source_api_uninit(void);



// sink
extern int auracast_app_notify_source_list(struct auracast_source_item_t *src);
extern int auracast_app_recv_scan_control_deal(u8 opcode, u8 sn, u8 *payload, u32 payload_len, u16 ble_con_handle, u8 *spp_remote_addr);
extern int auracast_app_recv_device_status_deal(u8 opcode, u8 sn, u8 *payload, u32 payload_len);
extern int auracast_app_recv_listening_control_deal(u8 opcode, u8 sn, u8 *payload, u32 payload_len, u16 ble_con_handle, u8 *spp_remote_addr);

// auracast_app_notify_listening_status
// notiry status
#define AURACAST_SINK_SYNC_STATE_IDLE                (0)
#define AURACAST_SINK_SYNC_STATE_SYNCING             (1)
#define AURACAST_SINK_SYNC_STATE_SYNC_COMPLETE       (2)
// notiry error
#define AURACAST_SINK_SYNC_NO_ERROR                  (0)
#define AURACAST_SINK_SYNC_BROADCAST_NAME_ERROR      (1)
#define AURACAST_SINK_SYNC_ADDRESS_ERROR             (2)
#define AURACAST_SINK_SYNC_BROADCAST_ID_ERROR        (3)
#define AURACAST_SINK_SYNC_BROADCAST_CODE_ERROR      (4)
#define AURACAST_SINK_SYNC_FAILD                     (5)
#define AURACAST_SINK_SYNC_TIMEOUT                   (6)
#define AURACAST_SINK_SYNC_LOST                      (7)
extern int auracast_app_notify_listening_status(u8 status, u8 error);

#endif
