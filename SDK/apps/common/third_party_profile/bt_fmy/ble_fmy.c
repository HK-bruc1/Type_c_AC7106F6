/*********************************************************************************************
    *   Filename        : .c

    *   Description     :

    *   Author          : JM

    *   Email           : zh-jieli.com

    *   Last modifiled  : 2017-01-17 11:14

    *   Copyright:(c)JIELI  2011-2016  @ , All Rights Reserved.
*********************************************************************************************/
/* #include "system/app_core.h" */
#include "system/includes.h"

#include "app_config.h"
#include "app_action.h"

#include "btstack/btstack_task.h"
#include "btstack/bluetooth.h"
#include "user_cfg.h"
#include "vm.h"
#include "btcontroller_modules.h"
#include "bt_common.h"
#include "3th_profile_api.h"
#include "le_common.h"
#include "JL_rcsp_api.h"
#include "custom_cfg.h"
#include "btstack/btstack_event.h"
#include "gatt_common/le_gatt_common.h"
#include "ble_fmy.h"
#include "ble_fmy_profile.h"
#include "system/malloc.h"
#include "ble_fmy_cfg.h"
#include "ble_fmy_fmna.h"
#include "app_ble_spp_api.h"
#include "clock_manager/clock_manager.h"
#include "ble_fmy_ota.h"

#if (THIRD_PARTY_PROTOCOLS_SEL & FMNA_EN)

unsigned int fmna_lib_debug_enable = 0x1;

#if 1
#define log_info(x, ...)  printf("[BLE_FMY]" x "\r\n", ## __VA_ARGS__)
/* #define log_info(x, ...)  r_printf("[BLE_FMY]" x "\r\n", ## __VA_ARGS__) */
#define log_info_hexdump  put_buf

#else
#define log_info(...)
#define log_info_hexdump(...)
#endif

//测试NRF连接,工具不会主动发起交换流程,需要手动操作; 但设备可配置主动发起MTU长度交换请求
#define ATT_MTU_REQUEST_ENALBE     0    /*配置1,就是设备端主动发起交换*/

//ATT发送的包长,    note: 23 <=need >= MTU
#define ATT_LOCAL_MTU_SIZE        (247) /*一般是主机发起交换,如果主机没有发起,设备端也可以主动发起(ATT_MTU_REQUEST_ENALBE set 1)*/

//ATT缓存的buffer支持缓存数据包个数
#define ATT_PACKET_NUMS_MAX       (4)  //need 1k

//ATT缓存的buffer大小,  note: need >= 23,可修改
#define ATT_SEND_CBUF_SIZE        (ATT_PACKET_NUMS_MAX * (ATT_PACKET_HEAD_SIZE + ATT_LOCAL_MTU_SIZE))

// 广播周期 (unit:0.625ms)
#define ADV_INTERVAL_MIN          (160 * 5)//

// FMY最大连接数，不可修改
#define  FMY_MAX_CONNECTIONS          2
#define  FMY_UPDATE_PARAM_PERIOD      30000  // FMY连接参数更新周期

//******************************************************************************************
//---------------
//连接参数更新请求设置
//是否使能参数请求更新,0--disable, 1--enable
static uint8_t fmy_connection_update_enable = 1; ///0--disable, 1--enable

//用户可以选择在空闲的时候 主动请求手机更新连接参数 快速降低功耗
static const struct conn_update_param_t fmy_connection_param_table = {792, 792, 0, 800};


static u16 fmy_cur_con_handle;//记录最新的连接handle
static u8  fmy_cur_remote_address_info[7];//remote's address
void *fmy_ble_hdl = NULL;
void *fmy_second_ble_hdl = NULL;
ble_state_e fmy_ble_state;

//-------------------------------------------------------------------------------------
fmy_glb_t  fmy_global_data;
fmy_vm_t fmy_vm_info;
adv_cfg_t fmy_server_adv_config;
uint8_t fmy_battery_level = BAT_STATE_FULL;
static fmy_conn_info_t fmy_conn_info[FMY_MAX_CONNECTIONS];

static const char *FMY_cur_name = NULL;
static const char the_original_Name[] = "Find My Accessory";//"Accessory- Find My.";
static const char the_suffix_Name[] = "Acce- Find My.";
static const char FMY_ManufacturerName[64] = "Zhuhai Jieli Technology Co.,Ltd.";
static const char FMY_ModelName[64] = "JLtag";
static const uint8_t FMY_AccessoryCategory[8] = {FMY_CATEGORY_Finder, 0, 0, 0, 0, 0, 0, 0};
static const uint8_t read_tx_power_level = 4;//The Bluetooth LE transmit power level of the accessory shall be fixed at ≥ +4dBm

static const uint8_t fmy_battery_type = FMNA_BAT_NON_RECHARGEABLE;

//set capability
static const uint8_t FMY_AccessoryCapabilities[4] = {
    (FMY_CAPABILITY_SUPPORTS_PLAY_SOUND

#if FMY_OTA_SUPPORT_CONFIG
    | FMY_CAPABILITY_SUPPORTS_FW_UPDATE_SERVICE
#endif

#if TCFG_GSENSOR_ENABLE && TCFG_P11GSENSOR_EN
    | FMY_CAPABILITY_SUPPORTS_MOTION_DETECTOR_UT
#endif

    | FMY_CAPABILITY_SUPPORTS_SN_LOOKUP_BY_BLE),
    0x00, 0x00, 0x00
};

#define FMY_CHECK_CAPABILITIES(bit)   ((FMY_AccessoryCapabilities[0] & bit) != 0)

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
static uint16_t fmy_att_read_callback(void *ble_hdl, hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t *buffer, uint16_t buffer_size);
static int fmy_att_write_callback(void *ble_hdl, hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t fmyaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size);
static int fmy_event_packet_handler(int event, u8 *packet, u16 size, u8 *ext_param);
extern int fmy_set_adv_enable(u8 enable);
static int fmy_set_adv_mode(uint8_t mode);
static int fmy_get_static_mac(uint8_t *mac);
static int fmy_set_static_mac(uint8_t *mac);
static int fmy_get_battery_level(void);
static int fmy_disconnect(uint16_t conn_handle, uint8_t reason);
//-------------------------------------------------------------------------------------
static const fmna_att_handle_t fmna_att_handle_table = {
    .pairing_control_point_handle = ATT_CHARACTERISTIC_4F860001_943B_49EF_BED4_2F730304427A_01_VALUE_HANDLE,
    .owner_cfg_control_point_handle = ATT_CHARACTERISTIC_4F860002_943B_49EF_BED4_2F730304427A_01_VALUE_HANDLE,
    .owner_info_porint_handle = ATT_CHARACTERISTIC_4F860004_943B_49EF_BED4_2F730304427A_01_VALUE_HANDLE,
    .non_owner_control_point_handle = ATT_CHARACTERISTIC_4F860003_943B_49EF_BED4_2F730304427A_01_VALUE_HANDLE,
    .debug_control_point_handle = ATT_CHARACTERISTIC_4F860005_943B_49EF_BED4_2F730304427A_01_VALUE_HANDLE,
    .firmware_update_handle = ATT_CHARACTERISTIC_94110001_6D9B_4225_A4F1_6A4A7F01B0DE_01_VALUE_HANDLE,
};

//-------------------------------------------------------------------------------------
//输入passkey 加密
#define PASSKEY_ENABLE                     0

static const sm_cfg_t fmy_sm_init_config = {
    .slave_security_auto_req = 0,
    .slave_set_wait_security = 0,

#if PASSKEY_ENABLE
    .io_capabilities = IO_CAPABILITY_DISPLAY_ONLY,
#else
    .io_capabilities = IO_CAPABILITY_NO_INPUT_NO_OUTPUT,
#endif

    .authentication_req_flags = SM_AUTHREQ_BONDING,// | SM_AUTHREQ_MITM_PROTECTION,
    .min_key_size = 7,
    .max_key_size = 16,
    .sm_cb_packet_handler = NULL,
};

//============================================================================================
bool fmy_check_capabilities_is_enalbe(u8 cap)
{
    if (FMY_CHECK_CAPABILITIES(cap)) {
        return true;
    } else {
        return false;
    }
}


static void fmy_timeout_add(u16 *id, void *func, u32 time)
{
    if (*id) {
        sys_timeout_del(*id);
    }
    *id = sys_timeout_add(0, func, time);
}

static void fmy_timeout_del(u16 *id)
{
    if (*id) {
        sys_timeout_del(*id);
        *id = 0;
    }
}

#define	FMY_VM_HEAD_TAG (0xA1)
bool fmy_vm_deal(fmy_vm_t *info, u8 rw_flag)
{
    int ret;
    int vm_len = sizeof(fmy_vm_t);

    log_info("-fmy_info vm_do:%d", rw_flag);

    if (rw_flag == 0) {
        ret = syscfg_read(CFG_FMY_INFO, (u8 *)info, vm_len);
        if (!ret) {
            log_info("-null--");
        } else {
            if (FMY_VM_HEAD_TAG == info->head_tag) {
                log_info("-exist--");
                log_info_hexdump((u8 *)info, vm_len);
                return true;
            }
        }
        memset(info, 0, vm_len);
        info->head_tag = FMY_VM_HEAD_TAG;
        return false;

    } else {
        syscfg_write(CFG_FMY_INFO, (u8 *)info, vm_len);
        log_info("-write--");
        log_info_hexdump((u8 *)info, vm_len);
        return true;
    }
}

static void fmy_set_profile_switch(void *ble_hdl, u8 mode)
{
    log_info("%s: %d", __FUNCTION__, mode);
    __fydata->profile_mode = mode;
    u8 fw_update_enable = 0;

    if (FMY_CHECK_CAPABILITIES(FMY_CAPABILITY_SUPPORTS_FW_UPDATE_SERVICE)) {
        fw_update_enable = 1;
    }

    switch (mode) {
    case PROFILE_MODE_UNPAIR:
        log_info("profile unpair");
        app_ble_att_handle_clear(ble_hdl);
        app_ble_att_handle_enable(ble_hdl, UNPAIR_CONTROL_START_HANDLE, UNPAIR_CONTROL_END_HANDLE, true);
        app_ble_att_handle_enable(ble_hdl, CFG_CONTROL_START_HANDLE, CFG_CONTROL_END_HANDLE, true);
        app_ble_att_handle_enable(ble_hdl, NON_OWNER_CONTROL_START_HANDLE, NON_OWNER_CONTROL_END_HANDLE, false);
        app_ble_att_handle_enable(ble_hdl, PAIRED_OWNER_INFO_START_HANDLE, PAIRED_OWNER_INFO_END_HANDLE, false);
        app_ble_att_handle_enable(ble_hdl, DEBUG_CONTROL_START_HANDLE, DEBUG_CONTROL_END_HANDLE, FMY_DEBUG_SERVICE_ENABLE);
        app_ble_att_handle_enable(ble_hdl, FW_UPDATE_START_HANDLE, FW_UPDATE_END_HANDLE, fw_update_enable);
        break;

    case PROFILE_MODE_OWNER:
        log_info("profile owner");
        app_ble_att_handle_clear(ble_hdl);
        app_ble_att_handle_enable(ble_hdl, UNPAIR_CONTROL_START_HANDLE, UNPAIR_CONTROL_END_HANDLE, false);
        app_ble_att_handle_enable(ble_hdl, CFG_CONTROL_START_HANDLE, CFG_CONTROL_END_HANDLE, true);
        app_ble_att_handle_enable(ble_hdl, NON_OWNER_CONTROL_START_HANDLE, NON_OWNER_CONTROL_END_HANDLE, true);
        app_ble_att_handle_enable(ble_hdl, PAIRED_OWNER_INFO_START_HANDLE, PAIRED_OWNER_INFO_END_HANDLE, true);
        app_ble_att_handle_enable(ble_hdl, DEBUG_CONTROL_START_HANDLE, DEBUG_CONTROL_END_HANDLE, FMY_DEBUG_SERVICE_ENABLE);
        app_ble_att_handle_enable(ble_hdl, FW_UPDATE_START_HANDLE, FW_UPDATE_END_HANDLE, fw_update_enable);
        break;

    case PROFILE_MODE_NON_OWNER:
        log_info("profile no owner");
        app_ble_att_handle_clear(ble_hdl);
        app_ble_att_handle_enable(ble_hdl, UNPAIR_CONTROL_START_HANDLE, UNPAIR_CONTROL_END_HANDLE, false);
        app_ble_att_handle_enable(ble_hdl, CFG_CONTROL_START_HANDLE, CFG_CONTROL_END_HANDLE, false);
        app_ble_att_handle_enable(ble_hdl, NON_OWNER_CONTROL_START_HANDLE, NON_OWNER_CONTROL_END_HANDLE, true);
        app_ble_att_handle_enable(ble_hdl, PAIRED_OWNER_INFO_START_HANDLE, PAIRED_OWNER_INFO_END_HANDLE, true);
        app_ble_att_handle_enable(ble_hdl, DEBUG_CONTROL_START_HANDLE, DEBUG_CONTROL_END_HANDLE, FMY_DEBUG_SERVICE_ENABLE);
        app_ble_att_handle_enable(ble_hdl, FW_UPDATE_START_HANDLE, FW_UPDATE_END_HANDLE, false);
        break;

    case PROFILE_MODE_SEPARATED:
        log_info("profile separated");
        app_ble_att_handle_clear(ble_hdl);
        app_ble_att_handle_enable(ble_hdl, UNPAIR_CONTROL_START_HANDLE, UNPAIR_CONTROL_END_HANDLE, false);
        app_ble_att_handle_enable(ble_hdl, CFG_CONTROL_START_HANDLE, CFG_CONTROL_END_HANDLE, false);
        app_ble_att_handle_enable(ble_hdl, NON_OWNER_CONTROL_START_HANDLE, NON_OWNER_CONTROL_END_HANDLE, true);
        app_ble_att_handle_enable(ble_hdl, PAIRED_OWNER_INFO_START_HANDLE, PAIRED_OWNER_INFO_END_HANDLE, false);
        app_ble_att_handle_enable(ble_hdl, DEBUG_CONTROL_START_HANDLE, DEBUG_CONTROL_END_HANDLE, FMY_DEBUG_SERVICE_ENABLE);
        app_ble_att_handle_enable(ble_hdl, FW_UPDATE_START_HANDLE, FW_UPDATE_END_HANDLE, fw_update_enable);
        break;

    case PROFILE_MODE_NEARBY:
    default:
        log_info("profile nearby");
        app_ble_att_handle_clear(ble_hdl);
        app_ble_att_handle_enable(ble_hdl, UNPAIR_CONTROL_START_HANDLE, UNPAIR_CONTROL_END_HANDLE, false);
        app_ble_att_handle_enable(ble_hdl, CFG_CONTROL_START_HANDLE, CFG_CONTROL_END_HANDLE, false);
        app_ble_att_handle_enable(ble_hdl, NON_OWNER_CONTROL_START_HANDLE, NON_OWNER_CONTROL_END_HANDLE, false);
        app_ble_att_handle_enable(ble_hdl, PAIRED_OWNER_INFO_START_HANDLE, PAIRED_OWNER_INFO_END_HANDLE, false);
        app_ble_att_handle_enable(ble_hdl, DEBUG_CONTROL_START_HANDLE, DEBUG_CONTROL_END_HANDLE, FMY_DEBUG_SERVICE_ENABLE);
        app_ble_att_handle_enable(ble_hdl, FW_UPDATE_START_HANDLE, FW_UPDATE_END_HANDLE, fw_update_enable);
        break;
    }
}

extern u8 get_cur_battery_level(void);
static void fmy_update_battery_level(void)
{
#if TCFG_SYS_LVD_EN
    u8  battery_level = get_cur_battery_level();//0~9
    log_info("read vbat:%d\n", battery_level);
#else
    u8  battery_level = 9;
#endif

    if (battery_level > 8) {
        fmy_battery_level = BAT_STATE_FULL;
    } else if (battery_level < 2) {
        fmy_battery_level = BAT_STATE_CRITICALLY_LOW;
    } else if (battery_level < 4) {
        fmy_battery_level = BAT_STATE_LOW;
    } else {
        fmy_battery_level = BAT_STATE_MEDIUM;
    }
    log_info("%s,bat_lev= %d", __FUNCTION__, fmy_battery_level);
}


static void fmy_check_connect_remote(void *ble_hdl, u8 addr_type, u8 *addr)
{
    u8 temp_addr[6];
    swapX(addr, temp_addr, 6);
    if (fmna_pm_peer_count() && fmna_connection_is_fmna_paired()) {
        if (ble_list_check_addr_is_exist(temp_addr, addr_type)) {
            log_info("remote in list");
        } else {
            log_info("remote not in list");
        }
        fmy_set_profile_switch(ble_hdl, PROFILE_MODE_NON_OWNER);
    } else {
        log_info("device no pair");
        fmy_set_profile_switch(ble_hdl, PROFILE_MODE_UNPAIR);
    }
}


// *************************fmy connect table curd *********************************//
void fmy_print_conn_info(const fmy_conn_info_t *conn_info_array, int array_size)
{
    log_info(">>>>>>>>print fmy curretn connect info");
    for (int i = 0; i < array_size; i++) {
        log_info("Index: %d, conn_hdl: %u, timer_id: %u, interval: %d, update_flag: %u",
                 i, conn_info_array[i].conn_hdl, conn_info_array[i].timer_id,
                 conn_info_array[i].interval, conn_info_array[i].update_flag);
    }
}

int fmy_get_index_by_conn_handle(u16 conn_handle)
{
    for (int i = 0; i < FMY_MAX_CONNECTIONS; i++) {
        if (fmy_conn_info[i].conn_hdl == conn_handle) {
            return i;  // 返回匹配的索引
        }
    }
    // 未找到匹配的 conn_handle
    return -1;
}

static void fmy_send_request_connect_parameter(void *connection_handle)
{
    log_info("fmy_send_request_connect_parameter");
    struct conn_update_param_t param = fmy_connection_param_table; //static ram
    int index = fmy_get_index_by_conn_handle((u16) connection_handle);
    if (index != -1) {
        if (fmy_conn_info[index].update_flag && fmy_conn_info[index].interval < param.interval_min) {
            fmy_conn_info[index].update_flag = 0;
            log_info("fmy update connect parameter request success:-%d-%d-%d-%d-\n", param.interval_min, param.interval_max, param.latency, param.timeout);
            ble_op_conn_param_request(fmy_conn_info[index].conn_hdl, &fmy_connection_param_table);
            return;
        } else {
            fmy_conn_info[index].update_flag = 1;
        }
    }
}

static void fmy_send_connect_update_task_deal(u16 conn_handle)
{
    u32 t_conn_handle = conn_handle;
    int i = fmy_get_index_by_conn_handle(conn_handle);
    if (i != -1) {
        u16 _timer_id = fmy_conn_info[i].timer_id;
        if (!_timer_id) {
            fmy_conn_info[i].timer_id = sys_timer_add((void *)t_conn_handle, fmy_send_request_connect_parameter, FMY_UPDATE_PARAM_PERIOD);
        } else {
            sys_timer_del(_timer_id);
            fmy_conn_info[i].timer_id = 0;
        }
    }
}

static void fmy_connect_info_update(u16 conn_handle, u16 interval)
{
    log_info("fmy connect_info_update");
    int empty_slot = -1;  // 用于记录空闲的槽位索引

    // 遍历 fmy_conn_info 数组，查找空闲槽位或已存在的 conn_handle
    for (int i = 0; i < FMY_MAX_CONNECTIONS; i++) {
        if (fmy_conn_info[i].conn_hdl == conn_handle) {
            // 已存在的 conn_handle，更新 interval 值
            fmy_conn_info[i].interval = interval;
            fmy_print_conn_info(fmy_conn_info, FMY_MAX_CONNECTIONS);
            return;
        } else if (fmy_conn_info[i].conn_hdl == 0 && empty_slot == -1) {
            // 空闲槽位，记录空闲槽位索引
            empty_slot = i;
        }
    }

    if (empty_slot != -1) {
        // 存入元素到空闲槽位
        fmy_conn_info[empty_slot].conn_hdl = conn_handle;
        fmy_conn_info[empty_slot].interval = interval;
        fmy_print_conn_info(fmy_conn_info, FMY_MAX_CONNECTIONS);
    } else {
        // 没有空闲槽位，无法存入新元素
        log_info("fmy_conn_info is full");
    }
}

static void fmy_connect_info_delete(u16 conn_handle)
{
    log_info("fmy connect_info_delete");
    int i = fmy_get_index_by_conn_handle(conn_handle);
    if (i != -1) {
        // 找到匹配的 conn_handle，执行删除操作
        fmy_conn_info[i].conn_hdl = 0;
        fmy_conn_info[i].interval = 0;
        fmy_conn_info[i].timer_id = 0;
        fmy_conn_info[i].update_flag = 0;
        fmy_print_conn_info(fmy_conn_info, FMY_MAX_CONNECTIONS);
        return;
    }
    // 未找到匹配的 conn_handle
    log_info("No matching conn_handle found in fmy_conn_info");
}
// *************************fmy connect table curd *********************************//


static void fmy_sm_event_cbk_handler(void *ble_hdl, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    sm_just_event_t *event = (void *)packet;
    u32 tmp32;
    switch (packet_type) {
    case HCI_EVENT_PACKET:
        switch (hci_event_packet_get_type(packet)) {
        case SM_EVENT_JUST_WORKS_REQUEST:
            //发送接受配对命令sm_just_works_confirm,否则不发
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            log_info("%04x->Just Works Confirmed.\n", event->con_handle);
            if (!fmna_connection_pair_request_check(event->con_handle)) {
                //设备已被绑定，则不允许再次走配对绑定流程
                log_info("reject new pair request!!!");
            }
            break;

        case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
            log_info_hexdump(packet, size);
            memcpy(&tmp32, event->data, 4);
            log_info("%04x->Passkey display: %06u.\n", event->con_handle, tmp32);
            break;

        case SM_EVENT_PASSKEY_INPUT_NUMBER:
            tmp32 = 888888;
            log_info("%04x->Passkey input: %06u.\n", event->con_handle, tmp32);
            sm_passkey_input(event->con_handle, tmp32); /*update passkey*/
            break;

        case SM_EVENT_PAIR_PROCESS:
            log_info("%04x->===Pair_process,sub= %02x\n", event->con_handle, event->data[0]);
            put_buf(event->data, 4);
            break;
        }
        break;
    }
}

static void fmy_event_cbk_packet_handler(void *ble_hdl, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    int mtu;
    uint16_t tmp16;
    uint32_t tmp32;
    uint16_t con_handle;

    switch (packet_type) {
    case HCI_EVENT_PACKET:
        switch (hci_event_packet_get_type(packet)) {
        case ATT_EVENT_CAN_SEND_NOW:
            break;
        case ATT_EVENT_HANDLE_VALUE_INDICATION_COMPLETE:
            if (app_ble_get_hdl_con_handle(ble_hdl) != little_endian_read_16(packet, 3)) {
                break;
            }
            log_info("ATT_EVENT_HANDLE_VALUE_INDICATION_COMPLETE\n");
            log_info("INDICATION_COMPLETE:con_handle= %04x,att_handle= %04x", \
                     little_endian_read_16(packet, 3), little_endian_read_16(packet, 5));
            fmna_gatt_platform_recieve_indication_response(little_endian_read_16(packet, 3), little_endian_read_16(packet, 5));
            break;
        case HCI_EVENT_LE_META:
            switch (hci_event_le_meta_get_subevent_code(packet)) {
            case HCI_SUBEVENT_LE_CONNECTION_COMPLETE: {
                if (app_ble_get_hdl_con_handle(ble_hdl) != little_endian_read_16(packet, 4)) {
                    break;
                }
                if (!hci_subevent_le_enhanced_connection_complete_get_role(packet)) {
                    break;
                }

                if (BT_OP_SUCCESS != packet[3]) {
                    switch (packet[3]) {
                    case BT_ERR_ADVERTISING_TIMEOUT:
                        log_info("DIRECT_ADV TO!\n");
                        fmy_ble_state = BLE_ST_IDLE;
                        break;

                    default:
                        log_info("CONNECTION FAIL!!! %0x\n", packet[3]);
                        fmy_ble_state = BLE_ST_IDLE;
                        break;
                    }
                    // __gatt_server_check_auto_adv();
                    break;
                }

                fmy_cur_con_handle = little_endian_read_16(packet, 4);
                tmp16 = little_endian_read_16(packet, 14 + 0);//interval
                __fydata->adv_fmna_state = FMNA_SM_CONNECTING;

                log_info("connection_handle:%04x, rssi= %d", fmy_cur_con_handle, ble_vendor_get_peer_rssi(fmy_cur_con_handle));
                log_info("peer_address_info:");
                log_info_hexdump(&packet[7], 7);

                log_info("con_interval = %d", little_endian_read_16(packet, 14 + 0));
                log_info("con_latency = %d", little_endian_read_16(packet, 14 + 2));
                log_info("cnn_timeout = %d", little_endian_read_16(packet, 14 + 4));

                //ble_comm_set_config_name(the_suffix_Name, 0);
                FMY_cur_name = the_suffix_Name;
                memcpy(fmy_cur_remote_address_info, &packet[7], 7);
                fmy_check_connect_remote(ble_hdl, packet[7], &packet[8]);
                fmna_connection_connected_handler(fmy_cur_con_handle, tmp16);
                if (fmy_connection_update_enable) {
                    fmy_connect_info_update(fmy_cur_con_handle, tmp16);
                    fmy_send_connect_update_task_deal(fmy_cur_con_handle);
                }
#if ATT_MTU_REQUEST_ENALBE
                att_server_set_exchange_mtu(fmy_cur_con_handle);/*主动请求MTU长度交换*/
#endif
#if TCFG_SYS_LVD_EN && FMY_FMCA_TEST_MODE == 0
                fmy_update_battery_level();
#endif
                fmy_ble_state = BLE_ST_CONNECT;
                break;
            }

            case HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE: {

                // 过滤除了fmy链路的其他链路更新操作
                if (app_ble_get_hdl_con_handle(ble_hdl) != little_endian_read_16(packet, 4)) {
                    break;
                }

                log_info_hexdump(packet, size);
                log_info("conn_param update_complete:%04x", little_endian_read_16(packet, 4));
                log_info("update_interval = %d", little_endian_read_16(packet, 6 + 0));
                log_info("update_latency = %d", little_endian_read_16(packet, 6 + 2));
                log_info("update_timeout = %d", little_endian_read_16(packet, 6 + 4));
                fmna_connection_conn_param_update_handler(little_endian_read_16(packet, 4), little_endian_read_16(packet, 6 + 0));

                if (fmy_connection_update_enable) {
                    fmy_connect_info_update(little_endian_read_16(packet, 4), little_endian_read_16(packet, 6 + 0));
                }
                break;
            }
            }
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            if (app_ble_get_hdl_con_handle(ble_hdl) != little_endian_read_16(packet, 3)) {
                break;
            }
            log_info("disconnect_handle:%04x,reason= %02x", little_endian_read_16(packet, 3), little_endian_read_16(packet, 5));
            if (fmy_cur_con_handle == little_endian_read_16(packet, 3)) {
                fmy_cur_con_handle = 0;
            }

            if (fmy_connection_update_enable) {
                fmy_send_connect_update_task_deal(little_endian_read_16(packet, 3));
                fmy_connect_info_delete(little_endian_read_16(packet, 3));
            }
            fmna_connection_disconnected_handler(little_endian_read_16(packet, 3), little_endian_read_16(packet, 5));
            fmy_ble_state = BLE_ST_DISCONN;
            break;
        case HCI_EVENT_ENCRYPTION_CHANGE:
            if (app_ble_get_hdl_con_handle(ble_hdl) != little_endian_read_16(packet, 3)) {
                break;
            }
            con_handle = little_endian_read_16(packet, 3);
            log_info("ENCRYPTION_CHANGE:handle=%04x,state=%d,process =%d", con_handle, packet[2], packet[3]);
            if (packet[2] == 0) {
                log_info("BOND OK");
                /* ble_comm_set_config_name(the_original_Name, 0); */
                FMY_cur_name = the_original_Name;
                if (fmna_connection_is_fmna_paired()) {
                    log_info("the same apple_id connect");
                    //绑定后的配置，默认进入owner访问,不同手机用绑定的apple id，也可以加密成功的
                    fmy_set_profile_switch(ble_hdl, PROFILE_MODE_OWNER);
                }
                fmna_connection_encryption_change_complete(con_handle, true);
                fmy_pairing_timeout_stop();
            } else {
                log_info("BOND fail!!!");
                fmy_set_profile_switch(ble_hdl, PROFILE_MODE_NON_OWNER);
            }
            break;

        case ATT_EVENT_MTU_EXCHANGE_COMPLETE:
            if (app_ble_get_hdl_con_handle(ble_hdl) != little_endian_read_16(packet, 2)) {
                break;
            }
            mtu = att_event_mtu_exchange_complete_get_MTU(packet) - 3;
            log_info("con_handle= %02x, ATT MTU = %u", little_endian_read_16(packet, 2), mtu);
            if (mtu > ATT_LOCAL_MTU_SIZE) {
                mtu = ATT_LOCAL_MTU_SIZE;
            }
            fmna_gatt_set_mtu_size(little_endian_read_16(packet, 2), mtu);
            break;

        case HCI_EVENT_VENDOR_REMOTE_TEST:
            log_info("--- HCI_EVENT_VENDOR_REMOTE_TEST\n");
            break;

        case L2CAP_EVENT_CONNECTION_PARAMETER_UPDATE_RESPONSE:
            break;

        }
        break;
    }
}

static uint16_t att_read_callback_ack_handle(uint16_t offset, uint8_t *buffer, uint16_t buffer_size, uint8_t *ack_data, uint16_t ack_data_len)
{
    uint16_t ret_att_value_len = ack_data_len;

    if ((offset >= ret_att_value_len) || (offset + buffer_size) > ret_att_value_len) {
        return 0;
    }

    if (buffer && ack_data_len) {
        memcpy(buffer, &ack_data[offset], buffer_size);
        ret_att_value_len = buffer_size;
//        log_info("------read data: %d", ret_att_value_len);
//        log_info_hexdump(buffer, buffer_size);
    }
    return ret_att_value_len;
}

/*************************************************************************************************/
/*!
 *  \brief      处理client 读操作
 *
 *  \param      [in]
 *
 *  \return
 *
 *  \note      profile的读属性uuid 有配置 DYNAMIC 关键字，就有read_callback 回调
 */
/*************************************************************************************************/
// ATT Client Read Callback for Dynamic Data
// - if buffer == NULL, don't copy data, just return size of value
// - if buffer != NULL, copy data and return number bytes copied
// @param con_handle of hci le connection
// @param attribute_handle to be read
// @param offset defines start of attribute value
// @param buffer
// @param buffer_size
static uint16_t fmy_att_read_callback(void *ble_hdl, hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t *buffer, uint16_t buffer_size)
{
    uint16_t  att_value_len = 0;
    uint16_t handle = att_handle;

    log_info("read_callback,conn_handle =%04x, handle=%04x,buffer=%08x", connection_handle, handle, (u32)buffer);

    switch (handle) {
    case ATT_CHARACTERISTIC_2a00_01_VALUE_HANDLE: {
        /* char *gap_name = ble_comm_get_gap_name(); */
        char *gap_name = (char *)FMY_cur_name;
        att_value_len = strlen(gap_name);

        if ((offset >= att_value_len) || (offset + buffer_size) > att_value_len) {
            break;
        }

        if (buffer) {
            memcpy(buffer, &gap_name[offset], buffer_size);
            att_value_len = buffer_size;
            log_info("\n------read gap_name: %s", gap_name);
        }
    }
    break;

    case ATT_CHARACTERISTIC_2a07_01_VALUE_HANDLE:
        log_info("read_tx_power = %d", read_tx_power_level);
        att_value_len = att_read_callback_ack_handle(offset, buffer, buffer_size, (uint8_t *)&read_tx_power_level, sizeof(read_tx_power_level));
        break;


    case ATT_CHARACTERISTIC_6AA50001_6352_4D57_A7B4_003A416FBB0B_01_VALUE_HANDLE:
        att_value_len = att_read_callback_ack_handle(offset, buffer, buffer_size, (uint8_t *)fmna_get_product_data(), 8);
        break;

    case ATT_CHARACTERISTIC_6AA50002_6352_4D57_A7B4_003A416FBB0B_01_VALUE_HANDLE:
        att_value_len = att_read_callback_ack_handle(offset, buffer, buffer_size, (uint8_t *)FMY_ManufacturerName, sizeof(FMY_ManufacturerName));
        break;

    case ATT_CHARACTERISTIC_6AA50003_6352_4D57_A7B4_003A416FBB0B_01_VALUE_HANDLE:
        att_value_len = att_read_callback_ack_handle(offset, buffer, buffer_size, (uint8_t *)FMY_ModelName, sizeof(FMY_ModelName));
        break;

    case ATT_CHARACTERISTIC_6AA50005_6352_4D57_A7B4_003A416FBB0B_01_VALUE_HANDLE:
        att_value_len = att_read_callback_ack_handle(offset, buffer, buffer_size, (uint8_t *)fmna_get_accessory_category(), 8);
        break;

    case ATT_CHARACTERISTIC_6AA50006_6352_4D57_A7B4_003A416FBB0B_01_VALUE_HANDLE:
        att_value_len = att_read_callback_ack_handle(offset, buffer, buffer_size, (uint8_t *)FMY_AccessoryCapabilities, sizeof(FMY_AccessoryCapabilities));
        break;

    case ATT_CHARACTERISTIC_6AA50007_6352_4D57_A7B4_003A416FBB0B_01_VALUE_HANDLE: {
        u32 tmp_version = fmna_version_get_fw_version();
        att_value_len = att_read_callback_ack_handle(offset, buffer, buffer_size, (uint8_t *)&tmp_version, 4);
    }
    break;

    case ATT_CHARACTERISTIC_6AA50008_6352_4D57_A7B4_003A416FBB0B_01_VALUE_HANDLE:
        att_value_len = att_read_callback_ack_handle(offset, buffer, buffer_size, (uint8_t *)fmna_version_get_network_version(), 4);
        break;

    case ATT_CHARACTERISTIC_6AA50009_6352_4D57_A7B4_003A416FBB0B_01_VALUE_HANDLE:
        att_value_len = att_read_callback_ack_handle(offset, buffer, buffer_size, (uint8_t *)&fmy_battery_type, sizeof(fmy_battery_type));
        break;

    case ATT_CHARACTERISTIC_6AA5000A_6352_4D57_A7B4_003A416FBB0B_01_VALUE_HANDLE:
        att_value_len = att_read_callback_ack_handle(offset, buffer, buffer_size, (uint8_t *)&fmy_battery_level, sizeof(fmy_battery_type));
        break;

    case ATT_CHARACTERISTIC_4F860001_943B_49EF_BED4_2F730304427A_01_CLIENT_CONFIGURATION_HANDLE:
    case ATT_CHARACTERISTIC_4F860002_943B_49EF_BED4_2F730304427A_01_CLIENT_CONFIGURATION_HANDLE:
    case ATT_CHARACTERISTIC_4F860003_943B_49EF_BED4_2F730304427A_01_CLIENT_CONFIGURATION_HANDLE:
    case ATT_CHARACTERISTIC_4F860004_943B_49EF_BED4_2F730304427A_01_CLIENT_CONFIGURATION_HANDLE:
    case ATT_CHARACTERISTIC_4F860005_943B_49EF_BED4_2F730304427A_01_CLIENT_CONFIGURATION_HANDLE:
    case ATT_CHARACTERISTIC_94110001_6D9B_4225_A4F1_6A4A7F01B0DE_01_CLIENT_CONFIGURATION_HANDLE:
        if (buffer) {
            // buffer[0] = ble_gatt_server_characteristic_ccc_get(connection_handle, handle);
            buffer[0] = multi_att_get_ccc_config(connection_handle, handle);
            buffer[1] = 0;
        }
        att_value_len = 2;
        break;

    default:
        break;
    }

    log_info("att_value_len= %d", att_value_len);
    if (att_value_len && buffer) {
        log_info("read handle= %04x respond data(%d)", handle, att_value_len);
        log_info_hexdump(buffer, att_value_len);
    }
    return att_value_len;
}


/*************************************************************************************************/
/*!
 *  \brief      处理client write操作
 *
 *  \param      [in]
 *
 *  \return
 *
 *  \note      profile的写属性uuid 有配置 DYNAMIC 关键字，就有write_callback 回调
 */
/*************************************************************************************************/
// ATT Client Write Callback for Dynamic Data
// @param con_handle of hci le connection
// @param attribute_handle to be written
// @param fmyaction - ATT_TRANSACTION_MODE_NONE for regular writes, ATT_TRANSACTION_MODE_ACTIVE for prepared writes and ATT_TRANSACTION_MODE_EXECUTE
// @param offset into the value - used for queued writes and long attributes
// @param buffer
// @param buffer_size
// @param signature used for signed write commmands
// @returns 0 if write was ok, ATT_ERROR_PREPARE_QUEUE_FULL if no space in queue, ATT_ERROR_INVALID_OFFSET if offset is larger than max buffer

static int fmy_att_write_callback(void *ble_hdl, hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t fmyaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size)
{
    if (fmy_connection_update_enable) {
        int index = fmy_get_index_by_conn_handle(connection_handle);
        fmy_conn_info[index].update_flag = 0;
    }
    int result = 0;
    u16 tmp16;

    u16 handle = att_handle;

#if 1
    log_info("write_callback,conn_handle =%04x, handle =%04x,size =%d", connection_handle, handle, buffer_size);
#endif

    switch (handle) {

    case ATT_CHARACTERISTIC_2a00_01_VALUE_HANDLE:
        break;

    case ATT_CHARACTERISTIC_4F860001_943B_49EF_BED4_2F730304427A_01_VALUE_HANDLE:
        log_info("---Pairing Control Point write data:%04x,%d", handle, buffer_size);
        log_info_hexdump(buffer, buffer_size);
        fmna_gatt_pairing_char_authorized_write_handler(connection_handle, 0, buffer_size, buffer);
        break;
    case ATT_CHARACTERISTIC_4F860002_943B_49EF_BED4_2F730304427A_01_VALUE_HANDLE:
        log_info("---Configuration Control Point write data:%04x,%d", handle, buffer_size);
        log_info_hexdump(buffer, buffer_size);
        fmna_gatt_config_char_write_handler(connection_handle, 0, buffer_size, buffer);
        break;

    case ATT_CHARACTERISTIC_4F860003_943B_49EF_BED4_2F730304427A_01_VALUE_HANDLE:
        log_info("---Non Owner Control Point:%04x,%d", handle, buffer_size);
        log_info_hexdump(buffer, buffer_size);
        fmna_gatt_nonown_char_write_handler(connection_handle, 0, buffer_size, buffer);
        break;

    case ATT_CHARACTERISTIC_94110001_6D9B_4225_A4F1_6A4A7F01B0DE_01_VALUE_HANDLE:
        /* log_info("---firmware update write data:%04x,%d", handle, buffer_size); */
        /* log_info_hexdump(buffer, buffer_size); */
        if (FMY_CHECK_CAPABILITIES(FMY_CAPABILITY_SUPPORTS_FW_UPDATE_SERVICE)) {
            fmna_gatt_uarp_char_write_handler(connection_handle, 0, buffer_size, buffer);
        }
        break;

    case ATT_CHARACTERISTIC_4F860004_943B_49EF_BED4_2F730304427A_01_VALUE_HANDLE:
        log_info("---Ower Information Control Point write data:%04x,%d", handle, buffer_size);
        log_info_hexdump(buffer, buffer_size);
        fmna_gatt_paired_owner_char_write_handler(connection_handle, 0, buffer_size, buffer);
        break;

    case ATT_CHARACTERISTIC_4F860005_943B_49EF_BED4_2F730304427A_01_VALUE_HANDLE:
        log_info("---Debug Control Point rx data:%04x,%d", handle, buffer_size);
        log_info_hexdump(buffer, buffer_size);
        fmna_debug_control_point_rx_handler(connection_handle, buffer, buffer_size);
        break;


    case ATT_CHARACTERISTIC_4F860001_943B_49EF_BED4_2F730304427A_01_CLIENT_CONFIGURATION_HANDLE:
    case ATT_CHARACTERISTIC_4F860002_943B_49EF_BED4_2F730304427A_01_CLIENT_CONFIGURATION_HANDLE:
    case ATT_CHARACTERISTIC_4F860003_943B_49EF_BED4_2F730304427A_01_CLIENT_CONFIGURATION_HANDLE:
    case ATT_CHARACTERISTIC_4F860004_943B_49EF_BED4_2F730304427A_01_CLIENT_CONFIGURATION_HANDLE:
    case ATT_CHARACTERISTIC_4F860005_943B_49EF_BED4_2F730304427A_01_CLIENT_CONFIGURATION_HANDLE:
    case ATT_CHARACTERISTIC_94110001_6D9B_4225_A4F1_6A4A7F01B0DE_01_CLIENT_CONFIGURATION_HANDLE:
        /* fmy_send_connetion_updata_deal(connection_handle); */
        log_info("\n------write ccc:%04x,%02x", handle, buffer[0]);
        // ble_gatt_server_characteristic_ccc_set(connection_handle, handle, buffer[0]);
        multi_att_set_ccc_config(connection_handle, handle, buffer[0]);
        break;

    default:
        break;
    }

    return 0;
}

/*************************************************************************************************/
/*!
 *  \brief      fmna模块初始化
 *
 *  \param      [in]
 *
 *  \return
 *
 *  \note
 */
/*************************************************************************************************/

static void fmy_fmna_ble_hdl_init(void)
{
    if (fmy_ble_hdl == NULL) {
        fmy_ble_hdl = app_ble_hdl_alloc();
        if (fmy_ble_hdl == NULL) {
            printf("fmy_ble_hdl alloc err !!\n");
            return;
        }
    }

    app_ble_profile_set(fmy_ble_hdl, findmy_profile_data);
    app_ble_att_read_callback_register(fmy_ble_hdl, fmy_att_read_callback);
    app_ble_att_write_callback_register(fmy_ble_hdl, fmy_att_write_callback);
    app_ble_att_server_packet_handler_register(fmy_ble_hdl, fmy_event_cbk_packet_handler);
    app_ble_hci_event_callback_register(fmy_ble_hdl, fmy_event_cbk_packet_handler);
    app_ble_l2cap_packet_handler_register(fmy_ble_hdl, fmy_event_cbk_packet_handler);
    app_ble_sm_event_callback_register(fmy_ble_hdl, fmy_sm_event_cbk_handler);
    app_ble_adv_address_type_set(fmy_ble_hdl, 1);   // adress type ramdon

    if (fmy_second_ble_hdl == NULL) {
        fmy_second_ble_hdl = app_ble_hdl_alloc();
        if (fmy_second_ble_hdl == NULL) {
            printf("fmy_second_ble_hdl alloc err !!\n");
            return;
        }
    }

    app_ble_profile_set(fmy_second_ble_hdl, findmy_profile_data);
    app_ble_att_read_callback_register(fmy_second_ble_hdl, fmy_att_read_callback);
    app_ble_att_write_callback_register(fmy_second_ble_hdl, fmy_att_write_callback);
    app_ble_att_server_packet_handler_register(fmy_second_ble_hdl, fmy_event_cbk_packet_handler);
    app_ble_hci_event_callback_register(fmy_second_ble_hdl, fmy_event_cbk_packet_handler);
    app_ble_l2cap_packet_handler_register(fmy_second_ble_hdl, fmy_event_cbk_packet_handler);
    app_ble_sm_event_callback_register(fmy_second_ble_hdl, fmy_sm_event_cbk_handler);
    app_ble_adv_address_type_set(fmy_second_ble_hdl, 1);   // adress type ramdon
}


extern void fmy_fmna_init(void);
void fmy_bt_ble_init(void)
{
    log_info("%s", __FUNCTION__);
    log_info("ble_file: %s", __FILE__);

    __fydata->make_new_mac_flag = 1;
    __fydata->pairing_mode_enable = 0;//power on control pairing state
    fmy_vm_deal(__fy_vm, 0);

    if (__fy_vm->reset_config) {
        __fydata->pairing_mode_enable = 1;
        __fy_vm->reset_config = 0;
        fmy_vm_deal(__fy_vm, 1);
        log_info("reset config to pair adv ");
    }
    FMY_cur_name = the_suffix_Name;
    fmy_cur_con_handle = 0;
    fmy_server_adv_config.adv_auto_do = 0;
    fmy_fmna_ble_hdl_init();

    extern const int config_btctler_le_master_multilink;
    log_info("config_btctler_le_master_multilink: %d", config_btctler_le_master_multilink);
    log_info("capability= %02x", FMY_AccessoryCapabilities[0]);
    fmna_set_accessory_category(FMY_AccessoryCategory);
    fmy_fmna_init();
    // set findmy connect link
    if (config_le_hci_connection_num < FMY_MAX_CONNECTIONS) {
        log_info("config_le_hci_connection_num has:%d, fmy_max_connections need:%d", config_le_hci_connection_num, FMY_MAX_CONNECTIONS);
        ASSERT(0, "no enough ble conect links for findmy!!");
    }
    fmna_connection_set_sys_max_connections(FMY_MAX_CONNECTIONS);
    // fmy_enable(1);

    fmy_enable(__fy_vm->is_open);
}

/*************************************************************************************************/
/*!
 *  \brief      模块退出
 *
 *  \param      [in]
 *
 *  \return
 *
 *  \note
 */
/*************************************************************************************************/
void fmy_bt_ble_exit(void)
{
    int cnt = 5;
    log_info("%s\n", __FUNCTION__);
    fmna_connection_disconnect_all();

    while (cnt--) {
        if (app_ble_get_hdl_con_handle(fmy_ble_hdl) \
            || app_ble_get_hdl_con_handle(fmy_second_ble_hdl)) {
            printf("wait disconnect cnt = %d\n", cnt);
            os_time_dly(10);
        } else {
            break;
        }
    }

    app_ble_hdl_free(fmy_ble_hdl);
    fmy_ble_hdl = NULL;
    app_ble_hdl_free(fmy_second_ble_hdl);
    fmy_second_ble_hdl = NULL;

    /* ble_module_enable(0); */
    /* ble_comm_exit(); */
}

/*************************************************************************************************/
/*!
 *  \brief      testbox 按键测试
 *
 *  \param      [in]
 *
 *  \return
 *
 *  \note
 */
/*************************************************************************************************/
extern bool bt_get_remote_test_flag();
void ble_server_send_test_key_num(u8 key_num)
{
    if (fmy_cur_con_handle) {
        if (bt_get_remote_test_flag()) {
            ble_op_test_key_num(fmy_cur_con_handle, key_num);
        } else {
            log_info("-not conn testbox\n");
        }
    }
}

u16 ble_fmy_get_con_handle(void)
{
    return fmy_cur_con_handle;
}

// for_test   0:enable/disable  1:factory reset
u8 test_fmy_en = 0;
void fmy_key_do_in_task(int key_event)
{
    test_fmy_en = !test_fmy_en;
    if (key_event == 0) {
        printf("fmy_enable %d\n", test_fmy_en);
        fmy_enable(test_fmy_en);
        /* fmy_open_close_pairing_mode(test_fmy_en); */
    } else if (key_event == 1) {
        printf("fmy_factory_reset\n");
        fmy_factory_reset();
    }
}

void fmy_key_do(int key_event)
{
    int argv[3];
    argv[0] = (int)fmy_key_do_in_task;
    argv[1] = 1;
    argv[2] = (int)key_event;
    int ret = os_taskq_post_type("app_core", Q_CALLBACK, 3, argv);
    if (ret) {
        printf("fmna_app_evnet_post err \n");
    }
}

#endif
