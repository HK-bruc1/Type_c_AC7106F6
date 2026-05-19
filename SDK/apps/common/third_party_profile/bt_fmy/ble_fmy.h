// binary representation
// attribute size in bytes (16), flags(16), handle (16), uuid (16/128), value(...)

#ifndef _BLE_FMY_H
#define _BLE_FMY_H

#include <stdint.h>
#include "app_config.h"
#include "ble_user.h"
#include "gatt_common/le_gatt_common.h"
#include "btstack/third_party/fmna/fmna_api.h"

enum {
    FY_STATE_UNPAIR = 0,
    FY_STATE_NEARBY,
    FY_STATE_SEPARATED,
    FY_STATE_CONNECTED,
};

enum {
    PROFILE_MODE_UNPAIR = 0,
    PROFILE_MODE_OWNER,
    PROFILE_MODE_NON_OWNER,
    PROFILE_MODE_NEARBY,
    PROFILE_MODE_SEPARATED,
};

typedef struct {
    u8  adv_fmna_state;
    u8  profile_mode;
    u8  make_new_mac_flag;
    u8  sound_onoff;
    u16 sound_ctrl_timer_id;
    u16 pairing_mode_timer_id;
    u8 pairing_mode_enable;
    u8 fmna_state;
} fmy_glb_t;
extern fmy_glb_t  fmy_global_data;
#define __fydata  (&fmy_global_data)

typedef struct {
    u8  head_tag;
    u8  reset_config;//flag
    u8  is_open;
} fmy_vm_t;

typedef struct {
    u16   conn_hdl;
    u16   timer_id;
    u16   interval;
    u8    update_flag;
} fmy_conn_info_t;


extern fmy_vm_t fmy_vm_info;
#define __fy_vm  (&fmy_vm_info)

extern adv_cfg_t fmy_server_adv_config;
extern uint8_t fmy_battery_level;
//------------------------------------------------------------------
extern const gatt_client_cfg_t fmy_client_init_cfg;
void fmy_client_init(void);
void fmy_client_exit(void);
int fmy_client_search_remote_profile(u16 conn_handle);
int fmy_client_search_remote_stop(u16 conn_handle);

void fmy_ios_services_init(void);
void fmy_ios_services_exit(void);
bool ble_set_make_random_address(u8 random_type);
int fmna_main_start(void);
int fmna_main_exit(void);
void swapX(const uint8_t *src, uint8_t *dst, int len);
bool fmy_check_capabilities_is_enalbe(u8 cap);
void fmy_state_idle_set_active(u8 active);
bool fmy_vm_deal(fmy_vm_t *info, u8 rw_flag);
void fmy_pairing_timeout_start(void);
void fmy_pairing_timeout_stop(void);
void fmy_key_do_in_task(int key_event);

//--------------------------------------------------------------------------------

#endif
