#ifndef _XMLY_PLATFORM_API_H_
#define _XMLY_PLATFORM_API_H_

#include "system/includes.h"

extern u8 xmly_pid[8];
extern char xmly_pid_str[];
extern u8 xmly_secret_key[16];
extern char xmly_sn[20];
extern char xmly_device_type[];
extern char xmly_fireware_version[];
extern char xmly_software_version[];
extern char xmly_device_model[];
extern char xmly_company[];

extern u8 xmly_adv_pid[4];
extern u8 xmly_adv_vid[4];
extern u8 xmly_adv_cid[2];
extern u8 xmly_adv_sv[2];
extern u8 xmly_adv_xpv;

#define XMLY_EVENT_BLE_CONNECTION       (0x00)
#define XMLY_EVENT_BLE_DISCONNECTION    (0x01)
#define XMLY_EVENT_AUTH_RESULT          (0x02)
#define XMLY_EVENT_TWS_SYNC             (0x03)
#define XMLY_EVENT_SPEECH_START         (0x10)
#define XMLY_EVENT_SPEECH_STOP          (0x11)
#define XMLY_EVENT_XIAOYA_TOUCH         (0x12)
extern void xmly_event_callback_register(int (*handler)(u8 *remote_addr, u32 event, u8 *data, u32 len));

extern void xmly_all_init(void);
extern void xmly_all_exit(void);
extern void xmly_cmd_recieve_callback(u8 *buffer, u32 buffer_size);
extern int xmly_speech_data_send(u8 *data, u16 len);
extern int xmly_adv_enable(u8 enable);
extern int xmly_ble_disconnect(void);
extern int xmly_adv_interval_set(u16 interval);
extern int xmly_adv_ble_connect_state(void);
extern int xmly_notify_xiaoya_touch(void);
extern int xmly_notify_battery(void);
extern int xmly_notify_error(char *json, u16 length);
extern int xmly_ble_status_get(void);
extern int xmly_ble_status_set(u8 status);
extern int xmly_auth_flag_get(void);
extern int xmly_tws_sync_size_get(void);
extern int xmly_tws_sync_data_get(u8 *data);
extern int xmly_tws_sync_data_set(u8 *data);

extern int xmly_is_tws_master_role(void);
extern void xmly_tws_sync_send_hdl(void);
extern void xmly_tws_sync_send_cmd(u8 *data, u16 size);

#endif

