#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".rcsp_interface_simplified.data.bss")
#pragma data_seg(".rcsp_interface_simplified.data")
#pragma const_seg(".rcsp_interface_simplified.text.const")
#pragma code_seg(".rcsp_interface_simplified.text")
#endif
#include "system/includes.h"
#include "string.h"
#include "asm/cpu.h"
#include "bt_common.h"
#include "spp_user.h"
#include "btstack_rcsp_user.h"
#include "rcsp_manage.h"
#include "rcsp_bt_manage.h"
#include "ble_rcsp_server.h"
#include "btstack/le/sm.h"
#include "btstack/le/le_user.h"
#include "btstack/le/le_common_define.h"
#include "rcsp_config.h"
#include "app_main.h"

#if (THIRD_PARTY_PROTOCOLS_SEL & RCSP_MODE_EN) && TCFG_THIRD_PARTY_PROTOCOLS_SIMPLIFIED

#if 0
#define rcsp_lib_puts(x)				puts(x)
#define rcsp_lib_putchar(x)				putchar(x)
#define rcsp_lib_printf					printf
#define rcsp_lib_printf_buf(x, y)		put_buf(x, y)
#else
#define rcsp_lib_puts(...)
#define rcsp_lib_putchar(...)
#define rcsp_lib_printf(...)
#define rcsp_lib_printf_buf(...)
#endif

static struct spp_operation_t *rcsp_spp_op = NULL;
static u8 rcsp_spp_state;
static u8 rcsp_spp_remote_addr[6] = {0};
extern const int config_le_sm_support_enable; //是否支持加密配对
#include "bt_tws.h"

int bt_rcsp_data_send(u16 ble_con_hdl, u8 *remote_addr, u8 *buf, u16 len)
{
    int ret = 0;
    rcsp_lib_printf("rcsp_data_send ble_con_hdl:%d, remote_addr:", ble_con_hdl);
    if (remote_addr) {
        rcsp_lib_printf_buf(remote_addr, 6);
    }
    rcsp_lib_printf("g_ble_con_hdl:%d, g_remote_addr:", rcsp_ble_con_handle_get());
    rcsp_lib_printf_buf(rcsp_simplified_spp_addr(), 6);

    if (ble_con_hdl) {
        if (ble_con_hdl != rcsp_ble_con_handle_get()) {
            rcsp_lib_printf("app_send_fail:%d %d %d !!!!!!\n", APP_BLE_OPERATION_ERROR, ble_con_hdl, rcsp_ble_con_handle_get());
            return APP_BLE_OPERATION_ERROR;
        }

        rcsp_lib_printf("ble rcsp_tx(%d):", len);
        rcsp_lib_printf_buf(buf, len);
        ret = ble_user_cmd_prepare(BLE_CMD_ATT_SEND_DATA, 4, ATT_CHARACTERISTIC_ae02_01_VALUE_HANDLE, buf, len, ATT_OP_AUTO_READ_CCC);
        if (ret == BLE_BUFFER_FULL) {
            rcsp_lib_printf("app_send_fail1:%d !!!!!!\n", ret);
            ret = APP_BLE_BUFF_FULL;
        }


    }
    if (remote_addr) {
        rcsp_lib_printf("spp rcsp_tx(%d):", len);
        rcsp_lib_printf_buf(buf, len);
        rcsp_spp_send_data(buf, len);
    }
    if (ret) {
        rcsp_lib_printf("app_send_fail2:%d !!!!!!\n", ret);
    }
    return ret;
}

void ble_sm_setup_init(io_capability_t io_type, u8 auth_req, uint8_t min_key_size, u8 security_en)
{
    //setup SM: Display only
    sm_init();
    sm_set_io_capabilities(io_type);
    sm_set_authentication_requirements(auth_req);
    sm_set_encryption_key_size_range(min_key_size, 16);
    sm_set_request_security(0);
    sm_event_callback_set(&rcsp_user_cbk_sm_packet_handler);
}

void rcsp_simplified_ble_profile_init(const uint8_t *rcsp_profile_data, att_read_callback_t read_callback, att_write_callback_t write_callback)
{
    printf("ble profile init\n");
    le_device_db_init();
    ble_sm_setup_init(IO_CAPABILITY_NO_INPUT_NO_OUTPUT, SM_AUTHREQ_BONDING, 7, config_le_sm_support_enable);

    /* setup ATT server */
    att_server_init(rcsp_profile_data, read_callback, write_callback);
    att_server_register_packet_handler(rcsp_cbk_packet_handler);

    // register for HCI events
    hci_event_callback_set(&rcsp_cbk_packet_handler);
    le_l2cap_register_packet_handler(&rcsp_cbk_packet_handler);
}

bool rcsp_spp_conn_state_get()
{
    /* rcsp_lib_printf("rcsp_spp_conn_state_get:%d\n", rcsp_spp_state); */
    return (rcsp_spp_state == SPP_USER_ST_CONNECT) ? true : false;
}

u8 bt_rcsp_device_conn_num(void)
{
    u8 cnt = 0;
    if (rcsp_ble_con_handle_get()) {
        cnt++;
    }
    if (rcsp_spp_conn_state_get()) {
        cnt++;
    }
    /* printf("%s, %s, %d, cnt:%d\n", __FILE__, __FUNCTION__, __LINE__, cnt); */
    return cnt;
}

u8 bt_rcsp_spp_conn_num(void)
{
    return rcsp_spp_conn_state_get() ? 1 : 0;
}

int rcsp_spp_send_data(u8 *data, u16 len)
{
    if (rcsp_spp_op && rcsp_spp_conn_state_get()) {
        rcsp_lib_printf("===spp rcsp_tx(%d):", len);
        rcsp_lib_printf_buf(data, len);
        return rcsp_spp_op->send_data(NULL, data, len);
    }
    rcsp_lib_printf("%s, %s, %d, %d", __FILE__, __FUNCTION__, __LINE__, rcsp_spp_state);
    return SPP_USER_ERR_SEND_FAIL;
}

static void rcsp_spp_recieve_cbk(void *priv, u8 *buf, u16 len)
{
    rcsp_lib_printf("%s, %s, %d, %d", __FILE__, __FUNCTION__, __LINE__, rcsp_spp_state);
    rcsp_lib_printf_buf(rcsp_spp_remote_addr, 6);
    rcsp_lib_printf_buf(buf, len);
    u8 _addr_temp[6] = {0};
    if (memcmp(_addr_temp, rcsp_spp_remote_addr, 6)) {

        if (!JL_rcsp_get_auth_flag_with_bthdl(0, rcsp_spp_remote_addr)) {
            if (!rcsp_protocol_head_check(buf, len)) {
                // 如果还没有验证，则只接收验证信息
                rcsp_lib_printf("%d===rcsp_rx(%d):", __LINE__, len);
                rcsp_lib_printf_buf(buf, len);
                JL_rcsp_auth_recieve(0, rcsp_spp_remote_addr, buf, len);
            }
            rcsp_lib_printf("%s, %s, %d\n", __FILE__, __FUNCTION__, __LINE__);
            return;
        }
        rcsp_lib_printf("%d===rcsp_rx(%d):", __LINE__, len);
        rcsp_lib_printf_buf(buf, len);
        JL_protocol_data_recieve(NULL, buf, len, 0, rcsp_spp_remote_addr);

    }
}

static void rcsp_rcsp_spp_state_cbk(u8 state)
{
    rcsp_spp_state = state;
    switch (state) {
    case SPP_USER_ST_CONNECT:
        printf("RCSP SPP_USER_ST_CONNECT ~~~\n");
        memset(rcsp_spp_remote_addr, 1, 6);
        rcsp_lib_printf_buf(rcsp_spp_remote_addr, 6);
        /* rcsp_protocol_bound(0, rcsp_spp_remote_addr); */
        /* if (rcsp_get_auth_support()) { */
        /*     JL_rcsp_reset_bthdl_auth(0, rcsp_spp_remote_addr); */
        /* } */
        break;

    case SPP_USER_ST_DISCONN:
        printf("RCSP SPP_USER_ST_DISCONN ~~~\n");
        /* rcsp_protocol_reset_bound(0, rcsp_spp_remote_addr); */
        /* if (rcsp_get_auth_support()) { */
        /*     JL_rcsp_reset_bthdl_auth(0, rcsp_spp_remote_addr); */
        /* } */
        break;

    default:
        break;
    }

    // rcsp_user_spp_state_specific需要上面绑定设备后才能被调用
    if (!app_var.goto_poweroff_flag) {
        rcsp_user_spp_state_specific(state, rcsp_spp_remote_addr);
    }
    // 需要app_core线程处理的操作放到以下函数
    rcsp_user_event_spp_handler(state, 1);

}

void rcsp_simplified_reset_spp_info()
{
    printf("rcsp_simplified_reset_spp_info\n");
    memset(rcsp_spp_remote_addr, 0, 6);
    rcsp_spp_state = SPP_USER_ST_DISCONN;
}

u8 rcsp_simplified_set_spp_conn_addr(u8 *spp_addr)
{
    printf("rcsp_simplified_set_spp_conn_addr\n");
    rcsp_spp_state = SPP_USER_ST_CONNECT;
    memcpy(rcsp_spp_remote_addr, spp_addr, 6);
    rcsp_lib_printf_buf(rcsp_spp_remote_addr, 6);
    return 0;
}

u8 *rcsp_simplified_spp_addr()
{
    return (u8 *)&rcsp_spp_remote_addr;
}

static void rcsp_spp_send_wakeup(void)
{
    /* putchar('W'); */
}


void rcsp_simplified_spp_init()
{
    rcsp_spp_state = 0;
    spp_get_operation_table(&rcsp_spp_op);
    rcsp_spp_op->regist_recieve_cbk(0, rcsp_spp_recieve_cbk);
    rcsp_spp_op->regist_state_cbk(0, rcsp_rcsp_spp_state_cbk);
    rcsp_spp_op->regist_wakeup_send(NULL, rcsp_spp_send_wakeup);
}


#if TCFG_USER_TWS_ENABLE

#endif

#endif


