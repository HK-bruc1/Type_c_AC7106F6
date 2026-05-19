#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".ble_iot_msg_manager.data.bss")
#pragma data_seg(".ble_iot_msg_manager.data")
#pragma const_seg(".ble_iot_msg_manager.text.const")
#pragma code_seg(".ble_iot_msg_manager.text")
#endif
#include "ble_iot_msg_manager.h"
#include "app_config.h"
#include "bt_common.h"
#include "app_msg.h"
#include "ble_iot_anc_manager.h"
#include "ble_iot_tws_manager.h"

#if (THIRD_PARTY_PROTOCOLS_SEL & LL_SYNC_EN)

#if 1
#define log_info(x, ...)       printf("[LL_LOG_ANC]" x " ", ## __VA_ARGS__)
#define log_info_hexdump       put_buf
#else
#define log_info(...)
#define log_info_hexdump(...)
#endif

struct iot_msg {
    u8 msg_type;
};

/**
 * @brief 发送iot消息到app_core处理
 *
 * @pragma IOT_MSG_TYPE
 */
void iot_msg_send_to_app_core(IOT_MSG_TYPE msg_type)
{
    struct iot_msg iot_m;
    iot_m.msg_type = msg_type;
    struct iot_msg *iot_e_m = &iot_m;
    // 发送消息到app_core中处理
    app_send_message_from(MSG_FROM_IOT, sizeof(*iot_e_m), (int *)iot_e_m);
}

static int iot_msg_handler(int *msg)
{
    ASSERT(msg, "iot_msg_handler is illegal!");
    struct iot_msg *iot_m = (struct iot_msg *)msg;
    switch (iot_m->msg_type) {
    case IOT_MSG_TYPE_UPDATE_ANC_VOICE_MAX_SYNC:
        log_info("MSG_IOT_UPDATE_ANC_VOICE_MAX_SYNC\n");
#if TCFG_USER_TWS_ENABLE && CONFIG_ANC_ENABLE
        iot_anc_effect_update_vm_value();
#endif
        break;
    case IOT_MSG_TYPE_SETTING_UPDATE:
        iot_update_function_from_tws_info();
        break;
    default:
        break;
    }
    return 0;
}
APP_MSG_HANDLER(iot_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_IOT,
    .handler    = iot_msg_handler,
};

#endif // (THIRD_PARTY_PROTOCOLS_SEL == LL_SYNC_EN)
