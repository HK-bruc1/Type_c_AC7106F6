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

// u8 xmly_lib_debug_enable = 0xF;

int ximalaya_app_speech_start(void)
{
    if (!xmly_is_tws_master_role()) {
        return -1;
    }
    if (BT_STATUS_TAKEING_PHONE == bt_get_connect_status()) {
        printf("phone ing...\n");
        return -1;
    }
    if (ai_mic_is_busy()) {
        printf("mic activing...\n");
        return -1;
    }

    ai_mic_rec_start();

    return 0;
}

void ximalaya_app_speech_stop(void)
{
    if (ai_mic_is_busy()) {
        printf("ximalaya_speech_stop\n");
        ai_mic_rec_close();
    }
}

u16 ximalaya_app_speech_send(u8 *data, u16 len)
{
    /* printf("ximalaya_app_speech_send %d\n", len); */
    /* put_buf(data, len); */
    xmly_speech_data_send(data, len);
    return 0;
}

int ximalaya_event_callback_handler(u8 *remote_addr, u32 event, u8 *data, u32 len)
{
    printf("ximalaya_event_callback_handler 0x%x\n", event);
    switch (event) {
    case XMLY_EVENT_TWS_SYNC:
        printf("XMLY_EVENT_TWS_SYNC %d\n", len);
        xmly_tws_sync_send_cmd(data, len);
        break;
    case XMLY_EVENT_BLE_CONNECTION:
        break;
    case XMLY_EVENT_BLE_DISCONNECTION:
        xmly_adv_enable(1);
        break;
    case XMLY_EVENT_SPEECH_START:
        ximalaya_app_speech_start();
        break;
    case XMLY_EVENT_SPEECH_STOP:
        ximalaya_app_speech_stop();
        break;
    case XMLY_EVENT_AUTH_RESULT:
        if (data[0] == 0x00) {
            printf("auth suss\n");
        } else {
            printf("auth fail\n");
        }
        break;
    case XMLY_EVENT_XIAOYA_TOUCH:
        if (data[0] == 0x01) {
            printf("xiaoya touch suss\n");
        } else {
            printf("xiaoya touch fail\n");
        }
        break;
    };
    return 0;
}

void ximalaya_protocol_init(void)
{
    xmly_all_init();
    xmly_event_callback_register(ximalaya_event_callback_handler);
    mic_rec_pram_init(AUDIO_CODING_OPUS, 0, ximalaya_app_speech_send, 4, 1024);
    xmly_adv_interval_set(150);
    xmly_adv_enable(1);
}

void ximalaya_protocol_exit(void)
{
    xmly_all_exit();
}


/*******************************************************
               TWS 同步
*******************************************************/

#if TCFG_USER_TWS_ENABLE

#define TWS_FUNC_ID_XMLY_TWS_SYNC \
	(((u8)('X' + 'M') << (3 * 8)) | \
	 ((u8)('L' + 'Y') << (2 * 8)) | \
	 ((u8)('T' + 'W' + 'S') << (1 * 8)) | \
	 ((u8)('S' + 'N' + 'Y' + 'C') << (0 * 8)))

#define XMLY_PROTOCOL_TWS_SYNC_HDL     (0x01)
#define XMLY_PROTOCOL_TWS_SYNC_CMD     (0x02)

extern const bool config_tws_le_role_sw;    //是否支持ble跟随tws主从切换
void xmly_tws_sync_send_hdl(void)
{
    u8 *temp_buf = 0;
    int size = 0;

    printf("xmly_tws_sync_send_hdl : %d %d\n", get_bt_tws_connect_status(), tws_api_get_role());

    if (config_tws_le_role_sw == 0) {
        return;
    }

    if (!get_bt_tws_connect_status() || !(tws_api_get_role() == TWS_ROLE_MASTER)) {
        return;
    }

    size = xmly_tws_sync_size_get();
    if (size) {
        temp_buf = zalloc(size + 1);
        temp_buf[0] = XMLY_PROTOCOL_TWS_SYNC_HDL;
        xmly_tws_sync_data_get(&temp_buf[1]);
        tws_api_send_data_to_sibling(temp_buf, size + 1, TWS_FUNC_ID_XMLY_TWS_SYNC);
        free(temp_buf);
    }
}

void xmly_tws_sync_send_cmd(u8 *data, u16 size)
{
    u8 *temp_buf = 0;
    printf("xmly_tws_sync_send_cmd : %d %d\n", get_bt_tws_connect_status(), tws_api_get_role());
    if (!get_bt_tws_connect_status() || !(tws_api_get_role() == TWS_ROLE_MASTER)) {
        return;
    }
    temp_buf = zalloc(size + 1);
    temp_buf[0] = XMLY_PROTOCOL_TWS_SYNC_CMD;
    memcpy(&temp_buf[1], data, size);
    tws_api_send_data_to_sibling(temp_buf, size + 1, TWS_FUNC_ID_XMLY_TWS_SYNC);
    free(temp_buf);
}

static void xmly_protocol_tws_sync_in_task(u8 *data, int len)
{
    int i;
    printf("xmly_protocol_tws_sync_in_task %d\n", len);
    put_buf(data, len);
    switch (data[0]) {
    case XMLY_PROTOCOL_TWS_SYNC_HDL:
        xmly_tws_sync_data_set(&data[1]);
        break;
    case XMLY_PROTOCOL_TWS_SYNC_CMD:
        xmly_cmd_recieve_callback(&data[1], len - 1);
        break;
    }
    free(data);
}

static void xmly_protocol_tws_sync_in_irq(void *_data, u16 len, bool rx)
{
    int i;
    int argv[4];
    u8 *rx_data = NULL;
    printf("xmly_protocol_tws_sync_in_irq %d\n", len);
    put_buf(_data, len);
    if (get_bt_tws_connect_status()) {
        if (rx && (tws_api_get_role() == TWS_ROLE_SLAVE)) {
            rx_data = malloc(len);
            if (rx_data == NULL) {
                return;
            }
            memcpy(rx_data, _data, len);
            argv[0] = (int)xmly_protocol_tws_sync_in_task;
            argv[1] = 2;
            argv[2] = (int)rx_data;
            argv[3] = (int)len;
            int ret = os_taskq_post_type("app_core", Q_CALLBACK, 4, argv);
            if (ret) {
                printf("%s taskq post err \n", __func__);
            }
        }
    }
}

REGISTER_TWS_FUNC_STUB(xmly_tws_sync) = {
    .func_id = TWS_FUNC_ID_XMLY_TWS_SYNC,
    .func = xmly_protocol_tws_sync_in_irq,
};

static int xmly_tws_msg_handler(int *_msg)
{
    struct tws_event *evt = (struct tws_event *)_msg;
    int role = evt->args[0];
    int reason = evt->args[2];

    switch (evt->event) {
    case TWS_EVENT_CONNECTED:
        printf("xmly TWS_EVENT_CONNECTED role:%d\n", role);
        if (role == TWS_ROLE_MASTER) {
            xmly_tws_sync_send_hdl();
        } else {
            xmly_ble_disconnect();
            xmly_adv_enable(0);
        }
        break;
    case TWS_EVENT_ROLE_SWITCH:
        printf("xmly TWS_EVENT_ROLE_SWITCH role:%d\n", role);
        //对耳主从切换时，将旧主机的蓝牙地址和pair_state同步给新主机
        if (role == TWS_ROLE_SLAVE) {
            xmly_adv_enable(0);
        } else {
            xmly_adv_enable(1);
        }
        break;
    case TWS_EVENT_CONNECTION_DETACH:
        break;
    }
    return 0;
}
APP_MSG_HANDLER(xmly_tws_msg_entry) = {
    .owner = 0xff,
    .from = MSG_FROM_TWS,
    .handler = xmly_tws_msg_handler,
};

#else

void xmly_tws_sync_send_hdl(void)
{
}

void xmly_tws_sync_send_cmd(u8 *data, u16 size)
{
}

#endif
/*******************************************************
               TWS 同步 end
*******************************************************/

#endif

