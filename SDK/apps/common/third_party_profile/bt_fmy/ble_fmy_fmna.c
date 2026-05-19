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
#include "ble_fmy.h"
#include "le/ble_api.h"
#include "ble_fmy_fmna.h"
#include "ble_fmy_profile.h"
#include "system/malloc.h"
#include "ble_fmy_cfg.h"
#include "ble_fmy_ota.h"
#include "app_ble_spp_api.h"
#include "ble_fmy_modet.h"

#include "audio_config.h"
#include "volume_node.h"
#include "app_main.h"
#include "jlstream.h"
#include "app_tone.h"


#if (THIRD_PARTY_PROTOCOLS_SEL & FMNA_EN)

#if 1
#define log_info(x, ...)  printf("[BLE_FMY_FMNA]" x "\r\n", ## __VA_ARGS__)
/* #define log_info(x, ...)  r_printf("[BLE_FMY_FMNA]" x "\r\n", ## __VA_ARGS__) */
#define log_info_hexdump  put_buf

#else
#define log_info(...)
#define log_info_hexdump(...)
#endif

//******************************************************************************************
enum FMY_TEXT_EVENT {
    BATTERY_LOW = 69, // 明确设置为70
    BATTERY_FULL,
    SERIALNUMBER_LOOKUP_OPEN,       // 71
    SERIALNUMBER_LOOKUP_CLOSE,       // 71
    PAIRING_OPEN,    // 73
};
//******************************************************************************************
extern void *fmy_ble_hdl;
extern void *fmy_second_ble_hdl;
extern ble_state_e fmy_ble_state;
extern adv_cfg_t fmy_server_adv_config;

#define  FMY_DEBUG_TEST_LOW_ADV_TO_FASE             0 //for debug test

#if FMY_DEBUG_TEST_LOW_ADV_TO_FASE
#define  FMY_ADV_SLOW_INTERVAL                      0xa0  //for test
#else
#define  FMY_ADV_SLOW_INTERVAL                      0xC80  //fixed 2 seconds,bqb
#endif

#define FMY_ADV_PAIR_TIMEOUT_MS                     (10*60*1000L) //10 min

// Separated fast advertising is used for UT finding, e.g. aggressive adv.
#define FMY_FMNA_SEPARATED_ADV_FAST_INTERVAL                         0x30          /**< Fast advertising interval 30ms (in units of 0.625 ms.) */
#define FMY_FMNA_SEPARATED_ADV_SLOW_INTERVAL                         FMY_ADV_SLOW_INTERVAL  /**< Slow advertising interval 2 seconds (in units of 0.625 ms.) */


// Nearby fast advertising is used for leash break.
#define FMY_FMNA_NEARBY_ADV_FAST_INTERVAL                            0x30          /**< Fast advertising interval 30ms (in units of 0.625 ms.) */
#define FMY_FMNA_NEARBY_ADV_SLOW_INTERVAL                            FMY_ADV_SLOW_INTERVAL  /**< Slow advertising interval 2 seconds (in units of 0.625 ms.) */

#define FMY_FMNA_PAIRING_ADV_FAST_INTERVAL                           0x30          /**< Fast advertising interval 30ms (in units of 0.625 ms.) */
#define FMY_FMNA_PAIRING_ADV_SLOW_INTERVAL                           0x30          /**< Slow advertising interval 30ms (in units of 0.625 ms.) */

static const uint16_t fmna_separated_adv_interval[] = {FMY_FMNA_SEPARATED_ADV_FAST_INTERVAL, FMY_FMNA_SEPARATED_ADV_SLOW_INTERVAL};
static const uint16_t fmna_nearby_adv_interval[] =    {FMY_FMNA_NEARBY_ADV_FAST_INTERVAL, FMY_FMNA_NEARBY_ADV_SLOW_INTERVAL};
static const uint16_t fmna_pairing_adv_interval[] =   {FMY_FMNA_PAIRING_ADV_FAST_INTERVAL, FMY_FMNA_PAIRING_ADV_SLOW_INTERVAL};


//******************************************************************************************
#define  PORT_DIR_OUPUT                  0
#define  PORT_DIR_INPUT                  1
#define  PORT_VALUE_HIGH                 1
#define  PORT_VALUE_LOW                  0
#define  SOUND_GPIO_PORT                 IO_PORTA_00

#define PORT_IO_INIT(PORT_IO,DIR)       //{gpio_set_die(PORT_IO,1);\
//gpio_set_pull_down(PORT_IO,0);\
//gpio_set_pull_up(PORT_IO,0);\
//gpio_set_direction(PORT_IO,DIR);}

#define PORT_IO_OUPUT(PORT_IO,VALUE)     //gpio_write(PORT_IO,VALUE);
#define PORT_IO_INPUT(PORT_IO)           //gpio_read(PORT_IO,0);

#define DEV_OPEN_LED_STATE()             //PORT_IO_OUPUT(LED_PAIR_GAPIO_POR,PORT_VALUE_HIGH);
#define DEV_CLOSE_LED_STATE()            //PORT_IO_OUPUT(LED_PAIR_GAPIO_POR,PORT_VALUE_LOW);

#define DEV_SOUND_STATE(ON_OFF)          //PORT_IO_OUPUT(SOUND_GPIO_PORT, ON_OFF);
#define DEV_SOUND_INIT()                 //PORT_IO_INIT(SOUND_GPIO_PORT, PORT_DIR_OUPUT);
#define DEV_SOUND_DEINIT()               //PORT_IO_INIT(SOUND_GPIO_PORT, PORT_DIR_INPUT);

#define SOUND_TICKS_MS             (500)
//***********************************************************************************************
static u8  fmy_adv_data[ADV_RSP_PACKET_MAX];//max is 31
static u8  fmy_scan_rsp_data[ADV_RSP_PACKET_MAX];//max is 31

//***********************************************************************************************
extern int fmy_set_adv_enable(u8 enable);
static int fmy_set_adv_mode(uint8_t mode);
static int fmy_get_static_mac(uint8_t *mac);
static int fmy_set_static_mac(uint8_t *mac);
static int fmy_get_battery_level(void);
static int fmy_disconnect(uint16_t conn_handle, uint8_t reason);
static void fmy_pairing_timeout_start(void);
static void fmy_pairing_timeout_stop(void);
static void fmy_test_get_send_sensor_data(void);
extern void update_list_local_addr(u8 *old_local_addr, u8 *new_local_addr);
//-------------------------------------------------------------------------------------
static const fmna_att_handle_t fmna_att_handle_table = {
    .pairing_control_point_handle = ATT_CHARACTERISTIC_4F860001_943B_49EF_BED4_2F730304427A_01_VALUE_HANDLE,
    .owner_cfg_control_point_handle = ATT_CHARACTERISTIC_4F860002_943B_49EF_BED4_2F730304427A_01_VALUE_HANDLE,
    .owner_info_porint_handle = ATT_CHARACTERISTIC_4F860004_943B_49EF_BED4_2F730304427A_01_VALUE_HANDLE,
    .non_owner_control_point_handle = ATT_CHARACTERISTIC_4F860003_943B_49EF_BED4_2F730304427A_01_VALUE_HANDLE,
    .debug_control_point_handle = ATT_CHARACTERISTIC_4F860005_943B_49EF_BED4_2F730304427A_01_VALUE_HANDLE,
    .firmware_update_handle = ATT_CHARACTERISTIC_94110001_6D9B_4225_A4F1_6A4A7F01B0DE_01_VALUE_HANDLE,
};

//============================================================================================
static void fmy_systerm_reset(u32 delay_ticks)
{
    if (delay_ticks) {
        log_info("delay_reset: %d ticks", delay_ticks);
        os_time_dly(delay_ticks);
    }

    log_info("reset start......");
    cpu_reset();
    while (1);
}

/*************************************************************************************************/
/*!
 *  \brief      推送消息到app_core 任务，app处理
 *
 *  \param      [in]
 *
 *  \return
 *
 *  \note
 */
/*************************************************************************************************/

void fmna_state_machine_event_deal(struct fmna_event *fmna_evt)
{
    log_info("fmna_state_machine_event_deal: event=%d，evt_data= %x,value=%02x,handler= %08x,", (int)fmna_evt->event, (u32)fmna_evt->value, (u32)fmna_evt->event_data, (u32)fmna_evt->handler);
    fmna_state_machine_event_handle(fmna_evt);
    free(fmna_evt);
}

static int fmy_evnet_post_msg(u8 priv_event, void *evt_data, u32 value, void *handler)
{
    int argv[3];
    log_info("fmna_app_evnet_post: event=%d，evt_data= %x,value=%02x,handler= %08x,", priv_event, (u32)evt_data, value, (u32)handler);
    struct fmna_event *fmna_evt = zalloc(sizeof(struct fmna_event));;
    if (fmna_evt == NULL) {
        log_info("fmna_evt zalloc err \n");
        return -1;
    }
    fmna_evt->event = priv_event;
    fmna_evt->value = value;
    fmna_evt->event_data = evt_data;
    fmna_evt->handler = handler;

    argv[0] = (int)fmna_state_machine_event_deal;
    argv[1] = 1;
    argv[2] = (int)fmna_evt;

    int ret = os_taskq_post_type("app_core", Q_CALLBACK, 3, argv);
    if (ret) {
        log_info("fmna_app_evnet_post err \n");
        free(fmna_evt);
        return -1;
    }

    return 0;
}

static int fmy_get_static_mac(uint8_t *mac)
{
    int ret;
    u8 cur_ble_address[6];//high->low
    log_info("%s(%d)", __FUNCTION__, __LINE__);

#if FMY_MAC_CHANGE_LOCK
    if (!__fydata->make_new_mac_flag) {
        ret = syscfg_read(CFG_FMNA_BLE_ADDRESS_INFO, cur_ble_address, 6);
        if (ret == 6) {
            log_info("get vm address");
            memcpy(mac, cur_ble_address, 6);
            log_info_hexdump(mac, 6);
            return 0;
        }
    } else {
        __fydata->make_new_mac_flag = 0;
    }
#endif

    log_info("make new address");
    log_info("=============make rand address");
    cur_ble_address[0] = rand32() | STATIC_ADV_ADDR_TYPE_MASK;
    cur_ble_address[1] = rand32();
    cur_ble_address[2] = rand32();
    cur_ble_address[3] = rand32();
    cur_ble_address[4] = rand32();
    cur_ble_address[5] = rand32();
    memcpy(mac, cur_ble_address, 6);
    log_info_hexdump(mac, 6);
#if FMY_MAC_CHANGE_LOCK
    log_info("update vm ble address1");
    syscfg_write(CFG_FMNA_BLE_ADDRESS_INFO, cur_ble_address, 6);
#endif
    return 0;
}


static int fmy_set_static_mac(uint8_t *mac)
{
    log_info("%s(%d)", __FUNCTION__, __LINE__);
    const u8 empty_mac_addr[6] = {0, 0, 0, 0, 0, 0};
    uint8_t new_swap_addr[6];
    uint8_t old_swap_addr[6];
    ble_set_make_random_address(1);
    swapX(mac, new_swap_addr, 6);

    void *app_ble_hdl = fmy_ble_hdl;
    if (app_ble_get_hdl_con_handle(fmy_ble_hdl) != 0) {
        log_info("======second dev adv_enable===");
        app_ble_hdl = fmy_second_ble_hdl;
    }

    //le_controller_set_random_mac(new_swap_addr);
    u8 *old_addr = app_ble_adv_addr_get(app_ble_hdl);
    if (old_addr) {
        swapX(old_addr, old_swap_addr, 6);
    }
    if (memcmp(mac, old_swap_addr, 6) && memcmp(old_swap_addr, empty_mac_addr, 6)) {
        update_list_local_addr(old_swap_addr, mac);
    }
    app_ble_set_mac_addr(app_ble_hdl, new_swap_addr);

    log_info_hexdump(new_swap_addr, 6);
    return 0;
}

static int fmy_get_battery_level(void)
{
    log_info("%s,bat_lev= %d", __FUNCTION__, fmy_battery_level);
    return fmy_battery_level;
}



static int fmy_disconnect(uint16_t conn_handle, uint8_t reason)
{
    log_info("%s(%d): %04x,%02x", __FUNCTION__, __LINE__, conn_handle, reason);

    if (app_ble_get_hdl_con_handle(fmy_ble_hdl) == conn_handle) {
        return app_ble_disconnect(fmy_ble_hdl);
    }

    if (app_ble_get_hdl_con_handle(fmy_second_ble_hdl) == conn_handle) {
        return app_ble_disconnect(fmy_second_ble_hdl);
    }
    return -1;
    // return ble_comm_disconnect_extend(conn_handle, reason);
}

void fmy_state_idle_set_active(u8 active)
{
}

static void fmy_sound_timer_control(void *priv)
{
    DEV_SOUND_STATE(__fydata->sound_onoff);
    __fydata->sound_onoff = !__fydata->sound_onoff;
}

static void fmna_tone_set_volume()
{
    printf("func:%s:%d", __FUNCTION__, app_audio_volume_max_query(SysVol_TONE));
    u32 param = VOLUME_NODE_CMD_SET_VOL | app_audio_volume_max_query(SysVol_TONE);
    jlstream_set_node_param(NODE_UUID_VOLUME_CTRLER, "Vol_BtcRing", &param, sizeof(param));
}

static int fmna_tone_play_callback(void *priv, enum stream_event event)
{
    printf("func:%s, event:%d", __FUNCTION__, event);
    if (event == STREAM_EVENT_START) {
        int msg[] = {(int)fmna_tone_set_volume, 0};
        os_taskq_post_type("app_core", Q_CALLBACK, ARRAY_SIZE(msg), msg);
    }
    return 0;
}

static int fmy_sound_control(FMNA_SOUND_OP_t op)
{
    log_info("func:%s,op= %d", __FUNCTION__, op);
    switch (op) {
    case FMNA_SOUND_INIT:
        DEV_SOUND_INIT();
        DEV_SOUND_STATE(PORT_VALUE_LOW);
        break;

    case FMNA_SOUND_START:
#if TCFG_PWMLED_ENABLE
        fmy_state_idle_set_active(true);
#endif
        play_ring_file_alone_with_callback(get_tone_files()->phone_in, NULL, fmna_tone_play_callback);

        __fydata->sound_onoff = 1;
        if (!__fydata->sound_ctrl_timer_id) {
            __fydata->sound_ctrl_timer_id = sys_timer_add(NULL, fmy_sound_timer_control, SOUND_TICKS_MS);
        }
        break;

    case FMNA_SOUND_STOP:
#if TCFG_PWMLED_ENABLE
        fmy_state_idle_set_active(false);
#endif

        tone_player_stop();
        ring_player_stop();

        if (__fydata->sound_ctrl_timer_id) {
            sys_timer_del(__fydata->sound_ctrl_timer_id);
            __fydata->sound_ctrl_timer_id = 0;
        }
        DEV_SOUND_STATE(PORT_VALUE_LOW);
        break;

    default:
        break;
    }
    return 0;
}

static void fmy_pairing_timeout_handle(void *priv)
{
    __fydata->pairing_mode_timer_id = 0;
    __fydata->pairing_mode_enable = 0;
    if (__fydata->adv_fmna_state == FMNA_SM_PAIR) {
        log_info("%s,%d", __FUNCTION__, __LINE__);
        fmy_set_adv_enable(0);
    }
}

void fmy_pairing_timeout_stop(void)
{
    if (__fydata->pairing_mode_timer_id) {
        log_info("%s,%d", __FUNCTION__, __LINE__);
        sys_timeout_del(__fydata->pairing_mode_timer_id);
        __fydata->pairing_mode_timer_id = 0;
    }
}

void fmy_pairing_timeout_start(void)
{
    log_info("%s,%d", __FUNCTION__, __LINE__);
    if (__fydata->pairing_mode_timer_id) {
        fmy_pairing_timeout_stop();
    }
    __fydata->pairing_mode_timer_id = sys_timeout_add(0, fmy_pairing_timeout_handle, FMY_ADV_PAIR_TIMEOUT_MS);
}


void fmy_pairing_restart_adv(void)
{
    if (__fydata->adv_fmna_state == FMNA_SM_PAIR && BLE_ST_ADV != fmy_ble_state) {
        log_info("%s,%d", __FUNCTION__, __LINE__);
        __fydata->pairing_mode_enable = 1;
        fmy_set_adv_enable(1);
        fmy_pairing_timeout_start();
    }
}

void fmy_pairing_reset_address(void)
{
    u8 address[6];
    ble_state_e cur_state;

    if (__fydata->adv_fmna_state == FMNA_SM_PAIR) {
        log_info("%s,%d", __FUNCTION__, __LINE__);
        cur_state = fmy_ble_state;
        fmy_set_adv_enable(0);
        __fydata->make_new_mac_flag = 1;
        fmy_get_static_mac(address);
        fmy_set_static_mac(address);
        if (BLE_ST_ADV == cur_state) {
            fmy_set_adv_enable(1);
        }
    }
}


/*************************************************************************************************/
/*!
 *  \brief      组织adv包数据，放入buff
 *
 *  \param      [in]
 *
 *  \return
 *
 *  \note
 */
/*************************************************************************************************/
static int fmy_set_adv_data(uint8_t fmna_state, uint8_t *fmna_payload, uint8_t size)
{
    u8 cnt = 0;
    u8 *buf = fmy_adv_data;

    switch (fmna_state) {
    case FMNA_SM_PAIR:
        buf[cnt++] = 3;
        buf[cnt++] = 3;
        buf[cnt++] = FMY_UUID_SERVICE & 0xff;
        buf[cnt++] = (FMY_UUID_SERVICE >> 8);
        buf[cnt++] = 3 + size;//3 + size -1;
        buf[cnt++] = 0x16;
        buf[cnt++] = FMY_UUID_SERVICE & 0xff;
        buf[cnt++] = (FMY_UUID_SERVICE >> 8);
        break;

    case FMNA_SM_NEARBY:
        buf[cnt++] = 7;
        buf[cnt++] = 0xff;
        buf[cnt++] = APPLE_VENDOR_ID & 0xff;
        buf[cnt++] = (APPLE_VENDOR_ID >> 8);
        break;

    case FMNA_SM_SEPARATED:
        buf[cnt++] = 0x1E;
        buf[cnt++] = 0xff;
        buf[cnt++] = APPLE_VENDOR_ID & 0xff;
        buf[cnt++] = (APPLE_VENDOR_ID >> 8);
        break;

    default:
        log_info("err_state: fmna_state= %d", fmna_state);
        break;

    }

    __fydata->adv_fmna_state = fmna_state;
    memcpy(&buf[cnt], fmna_payload, size);
    cnt += size;
    log_info("fmy state(%d), adv_data(%d)", fmna_state, cnt);
    log_info_hexdump(buf, cnt);

    fmy_server_adv_config.adv_data_len = cnt;
    fmy_server_adv_config.adv_data = fmy_adv_data;
    fmy_server_adv_config.rsp_data_len = 0;
    fmy_server_adv_config.rsp_data = fmy_scan_rsp_data;
    return 0;
}

/*************************************************************************************************/
/*!
 *  \brief      配置广播参数
 *
 *  \param      [in]
 *
 *  \return
 *
 *  \note      开广播前配置都有效
 */
/*************************************************************************************************/
static int fmy_set_adv_mode(uint8_t mode)
{
    if (mode > FMNA_ADV_MODE_SLOW) {
        return -1;
    }

    fmy_server_adv_config.adv_auto_do = 0;
    fmy_server_adv_config.adv_type = ADV_IND;
    fmy_server_adv_config.adv_channel = ADV_CHANNEL_ALL;

    switch (__fydata->adv_fmna_state) {
    case FMNA_SM_PAIR:
        log_info("FMNA_SM_PAIR");
        fmy_server_adv_config.adv_interval = fmna_pairing_adv_interval[mode];
        break;
    case FMNA_SM_SEPARATED:
        log_info("FMNA_SM_SEPARATED");
        fmy_server_adv_config.adv_interval = fmna_separated_adv_interval[mode];
        break;
    case FMNA_SM_NEARBY:
        log_info("FMNA_SM_NEARBY");
        fmy_server_adv_config.adv_interval = fmna_nearby_adv_interval[mode];
        if (app_ble_get_hdl_con_handle(fmy_ble_hdl) != 0) {
            log_info("===set_adv second devices===");
            fmy_server_adv_config.adv_interval = FMY_FMNA_NEARBY_ADV_SLOW_INTERVAL;
        } else {
        }
    default:
        break;

    }

    log_info("set_adv_mode(%d), adv_fmna_state(%d), adv_interval= %04x, adv_type= %d", mode, __fydata->adv_fmna_state, fmy_server_adv_config.adv_interval, fmy_server_adv_config.adv_type);
    return 0;
}

int fmy_set_adv_enable(u8 enable)
{
    log_info("%s(%d): %d", __FUNCTION__, __LINE__, enable);

    void *app_ble_hdl = fmy_ble_hdl;
    if (app_ble_adv_state_get(fmy_second_ble_hdl)) {
        log_info("======second dev adv_disable===");
        app_ble_hdl = fmy_second_ble_hdl;
    } else if (app_ble_get_hdl_con_handle(fmy_ble_hdl) != 0) {
        log_info("======second dev adv_enable===");
        app_ble_hdl = fmy_second_ble_hdl;
    }

    if (enable) {
        if (!__fy_vm->is_open) {
            log_info("fmy is not open, adv is locked!");
            return 0;
        }
        if (__fydata->adv_fmna_state == FMNA_SM_PAIR) {
            /* if (0 == __fydata->pairing_mode_enable) { */
            /* log_info("hold Find My network pairing mode!!!"); */
            /* return 0; */
            /* } else { */
            if (__fydata->pairing_mode_timer_id) {
                log_info("reset pairing timer");
                fmy_pairing_timeout_stop();
                fmy_pairing_timeout_start();
            } else {
                log_info("start pairing timer");
                fmy_pairing_timeout_start();
            }
            /* } */
        } else {
            //other state,bonded, default pairing unlock
            __fydata->pairing_mode_enable = 1;
        }

        /* if (ble_comm_dev_get_connected_nums(GATT_ROLE_SERVER)) { */
        /* log_info("======second dev adv_enable==="); */
        /* } */

        /* ble_gatt_server_set_adv_config(&fmy_server_adv_config); */
        // if (app_ble_get_hdl_con_handle(app_ble_hdl) == 0) {
        app_ble_set_adv_param(app_ble_hdl, \
                              fmy_server_adv_config.adv_interval, \
                              fmy_server_adv_config.adv_type, \
                              fmy_server_adv_config.adv_channel);
        app_ble_adv_data_set(app_ble_hdl, \
                             (u8 *)fmy_server_adv_config.adv_data,
                             fmy_server_adv_config.adv_data_len);
        app_ble_rsp_data_set(app_ble_hdl, \
                             (u8 *)fmy_server_adv_config.rsp_data,
                             fmy_server_adv_config.rsp_data_len);
        // }
    }

    if (enable && BLE_ST_ADV == fmy_ble_state) {
        log_info("already adv open!!!");
        return 0;
    }

    /* ble_gatt_server_adv_enable(enable); */
    if (enable) {
        fmy_ble_state = BLE_ST_ADV;
    } else {
        fmy_ble_state = BLE_ST_IDLE;
    }

    log_info(">>>> fmy fmy adv:%x %x", (u32)app_ble_hdl, app_ble_get_hdl_con_handle(app_ble_hdl));
    //if (enable) {
    //   if (app_ble_get_hdl_con_handle(app_ble_hdl) == 0) {
    //        app_ble_adv_enable(app_ble_hdl, enable);
    //    }
    //} else {
    app_ble_adv_enable(app_ble_hdl, enable);
    //}
    log_info("%s(%d): %d", __FUNCTION__, __LINE__, enable);
    return 0;
}

static int ble_fmy_att_send_data(u16 conn_handle, u16 att_handle, u8 *data, u16 len, att_op_type_e op_type)
{
    log_info("att_send:con_hdl= %04x,att_hdl= %04x,le= %d,op_type= %d", conn_handle, att_handle, len, op_type);
    log_info_hexdump(data, len);

    if (app_ble_get_hdl_con_handle(fmy_ble_hdl) == conn_handle) {
        return app_ble_att_send_data(fmy_ble_hdl, att_handle, data, len, op_type);
    }

    if (app_ble_get_hdl_con_handle(fmy_second_ble_hdl) == conn_handle) {
        return app_ble_att_send_data(fmy_second_ble_hdl, att_handle, data, len, op_type);
    }

    return -1;
}

/*************************************************************************************************/
/*!
 *  \brief      断开连接
 *
 *  \param      [in]
 *
 *  \return
 *
 *  \note
 */
/*************************************************************************************************/
void fmy_test_disconnect(void)
{
    log_info("%s", __FUNCTION__);
    fmna_connection_disconnect_all();
}

/*************************************************************************************************/
void fmy_test_switch_battery_level(u8 bat_val)
{
    fmy_battery_level = bat_val;
    log_info("%s, bat= %d", __FUNCTION__, fmy_battery_level);
}

/*
 * @brief findmy state callback，状态类别参考FMNA_SM_State_t枚举体
 */
static uint16_t fmy_state_callback(FMNA_SM_State_t fmy_state, const char *state_string)
{
    log_info("fmy state change->fmy_state:%s", state_string);
    __fydata->fmna_state = fmy_state;
    switch (fmy_state) {
    case FMNA_SM_CONNECTING:
        //UI_MSG_POST("SET_CONNECTING_FLAG:a=%4", 1);
        break;
    case FMNA_SM_FMNA_PAIR_COMPLETE:
        log_info("findmy pair success!!");
        //UI_MSG_POST("PAIR_SUCCESS");
        break;
    case FMNA_SM_UNPAIR:
        // 锁住pairing mode 广播,待重新开启
        //UI_MSG_POST("UNPAIR_SUCCESS");
        __fydata->pairing_mode_enable = 0;
        log_info("findmy unpair success!!");
        break;
    case FMNA_SM_FMNA_PAIR:
        log_info("SM_FMNA_PAIR!!");
        break;
    default:
        break;
    }
    return 0;
}

//=====================================================================
static const fmna_app_api_t fmna_app_api_table = {
    .evnet_post_msg = fmy_evnet_post_msg,
    .set_adv_data = fmy_set_adv_data,
    .set_adv_mode = fmy_set_adv_mode,
    .set_adv_enable = fmy_set_adv_enable,
    .get_mac = fmy_get_static_mac,
    .set_mac = fmy_set_static_mac,
    .att_send_data = ble_fmy_att_send_data,
    .get_battery_level = fmy_get_battery_level,
    .call_disconnect = fmy_disconnect,
    .sound_control = fmy_sound_control,
    .uarp_ota_process = fmy_ota_process,

#if TCFG_GSENSOR_ENABLE && TCFG_P11GSENSOR_EN
    .sensor_init = fmy_motion_detection_init,
    .sensor_deinit = fmy_motion_detection_deinit,
    .motion_deteted = fmy_motion_detection,
#endif

    .state_callback = fmy_state_callback,
    .check_capability = fmy_check_capabilities_is_enalbe,
};

static const char fmy_user_config_path[] = "flash/EXT_RESERVED/FINDMY"; //配置区路径(isd_config_rule.c)
static int fmy_set_user_infomation(void)
{
    ret_code_t ret = FMY_SUCCESS;
    fmna_input_cfg_t input_cfg;

    fmna_user_cfg_set_patch(fmy_user_config_path);
    fmna_set_product_data(fmy_product_data);
    fmna_set_crypto_enc_key_config(fmy_Server_Encryption_Key, fmy_Signature_Verification_Key);

    uint8_t *data_ram = zalloc(16 + 1024);
    int len;
    if (data_ram) {
        fmna_user_cfg_set_patch(fmy_user_config_path);
        ret = fmna_user_cfg_open();
        if (ret == FMY_SUCCESS) {
            if (fmna_user_cfg_is_exist()) {
                log_info("exist user_cfg info");
                goto w_end;
            }
            //配置区有效，并且为空才写入
            input_cfg.serial_number = (uint8_t *)fmy_serial_number;
            input_cfg.uuid = data_ram;
            input_cfg.token = &data_ram[16];

#if UUID_TOKEN_IS_BASE64_MODE
            len = uuid_str_to_hex(fmy_token_uuid_char, input_cfg.uuid);
            if (len != 16) {
                log_info("err uuid char");
                ret = FMY_ERROR_INVALID_DATA;
                goto w_end;
            }
            len = fmna_Base64Decode((char *)fmy_token_auth_char, (char *)input_cfg.token, 1024);
            if (len == 0) {
                log_info("err token char");
                ret = FMY_ERROR_INVALID_DATA;
                goto w_end;
            }
#else
            memcpy(input_cfg.uuid, fmy_token_uuid_hex, 16);
            int cpy_len = sizeof(fmy_token_auth_hex);
            if (cpy_len > 1024) {
                cpy_len =  1024;
            }
            memcpy(input_cfg.token, fmy_token_auth_hex, sizeof(fmy_token_auth_hex));

#endif
            ret = fmna_user_cfg_write(&input_cfg);
        }
w_end:
        fmna_user_cfg_close();
        if (ret == FMY_SUCCESS) {
            log_info("set user_cfg succ");
        } else {
            log_info("set user_cfg fail,%d", ret);
        }

        free(data_ram);
    }
    return 0;
}

static void fmy_devices_init(void)
{
    // enable fmy sound
    DEV_SOUND_INIT();
    DEV_SOUND_STATE(PORT_VALUE_LOW);
}

static void fmy_devices_deinit()
{
    // disable fmy sound
    DEV_SOUND_DEINIT();
    fmy_sound_control(FMNA_SOUND_STOP);
}

void fmy_fmna_init(void)
{
    log_info("fmna_init: %s,%s", __DATE__, __TIME__);
    log_info("production mode= %d, fmy_version= %d", TOKEN_ID_PRODUCTION_MODE,
             FMY_FW_VERSION_MAJOR_NUMBER * 100 + FMY_FW_VERSION_MINOR_NUMBER * 10 + FMY_FW_VERSION_REVISION_NUMBER);

    fmy_set_user_infomation();
    fmna_config_user_vm_rang(CFG_FMNA_SOFTWARE_AUTH_START, CFG_FMNA_SOFTWARE_AUTH_END);
    fmna_set_app_api((fmna_app_api_t *)&fmna_app_api_table);
    fmna_gatt_set_att_handle_table(&fmna_att_handle_table);
    fmna_version_init(FMY_FW_VERSION_MAJOR_NUMBER, FMY_FW_VERSION_MINOR_NUMBER, FMY_FW_VERSION_REVISION_NUMBER);
    fmna_main_start();
    log_info("reboot fmna state: %d", fmna_get_current_state());
}

static void fmy_set_adv_enable_timeout(void *priv)
{
    fmy_set_adv_enable((u8)priv);
}

int fmy_enable(bool en)
{
    // 写开关标志位
    __fy_vm->is_open = en;
    fmy_vm_deal(__fy_vm, 1);

    if (en) {
        log_info(">>>>>fmy open");
        // 使能fmy外设
        fmy_devices_init();
        // 处于配对状态就不需要做开关配对广播操作
        /* if (__fydata->adv_fmna_state == FMNA_SM_PAIR) { */
        /* printf("err:fmna_state == FMNA_SM_PAIR\n"); */
        /* return 0; */
        /* } */
    } else {
        log_info(">>>>>fmy close");
        // 关闭findmy外设
        fmy_devices_deinit();
        // 如果处于连接状态则需要先断开连接再关广播
        if (__fydata->fmna_state == FMNA_SM_CONNECTED) {
            fmy_test_disconnect();
            sys_timeout_add((void *)0, fmy_set_adv_enable_timeout, 500);
            return 0;
        }
    }
    // 开关广播操作
    return fmy_set_adv_enable(en);
}

// 开关配对广播
void fmy_open_close_pairing_mode(bool en)
{
    if (en) {
        log_info(">>>fmy start pairing adv");
        fmy_pairing_reset_address();
        fmy_pairing_restart_adv();
    } else {
        if (__fydata->adv_fmna_state == FMNA_SM_PAIR) {
            log_info(">>>fmy close pairing adv");
            __fydata->pairing_mode_enable = 0;
            fmy_set_adv_enable(0);
            fmy_pairing_timeout_stop();
        }
    }
}

// 是否配对, 1:已配对 0:未配对
bool fmy_get_pair_state(void)
{
    return fmna_connection_is_fmna_paired();
}

// 手动强制解除配对
void fmy_factory_reset(void)
{
    log_info(">>>findmy factory reset!");
    fmy_sound_control(FMNA_SOUND_STOP);
    fmna_factory_reset(NULL);
}

void fmy_key_process(u8 key_event)
{
#if 0//过认证的测试按键,用到的时候在打开需要再iokey.c去配置
    switch (key_event) {
    case BATTERY_LOW:
        fmy_test_switch_battery_level(BAT_STATE_CRITICALLY_LOW);
        break;
    case SERIALNUMBER_LOOKUP_OPEN:
        fmna_paired_serialnumber_lookup_enable(1);
        break;
    case BATTERY_FULL:
        fmy_test_switch_battery_level(BAT_STATE_FULL);
        break;
    case SERIALNUMBER_LOOKUP_CLOSE:
        fmna_paired_serialnumber_lookup_enable(0);
        break;
    case PAIRING_OPEN:
        fmy_enable(1);
        fmy_open_close_pairing_mode(1);
        break;
    }
#endif
}

/* void fmy_key_process(u8 key_event) */
/* { */
/* if (key_event == 64) {  // short */
/* printf("fmy key short"); */
/* fmy_open_close_pairing_mode(1); */
/* } else if (key_event == 66) {   // long */
/* fmy_factory_reset(); */
/* } */
/* } */

#endif

