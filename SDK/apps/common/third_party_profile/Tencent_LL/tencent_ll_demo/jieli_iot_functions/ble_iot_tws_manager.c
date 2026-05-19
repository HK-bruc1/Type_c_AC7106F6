#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".ble_iot_tws_manager.data.bss")
#pragma data_seg(".ble_iot_tws_manager.data")
#pragma const_seg(".ble_iot_tws_manager.text.const")
#pragma code_seg(".ble_iot_tws_manager.text")
#endif
#include "ble_iot_tws_manager.h"
#include "app_config.h"
#include "system/includes.h"
#include "bt_tws.h"
#include "ble_iot_anc_manager.h"
#include "ble_iot_msg_manager.h"

#if (THIRD_PARTY_PROTOCOLS_SEL & LL_SYNC_EN)

#if 1
#define log_info(x, ...)       printf("[LL_LOG_POWER]" x " ", ## __VA_ARGS__)
#define log_info_hexdump       put_buf
#else
#define log_info(...)
#define log_info_hexdump(...)
#endif

#if TCFG_USER_TWS_ENABLE

#define IOT_READ_BIG_U16(a)   ((*((u8*)(a)) <<8)  + *((u8*)(a)+1))
#define IOT_WRITE_BIG_U16(a,src)   {*((u8*)(a)+0) = (u8)(src>>8); *((u8*)(a)+1) = (u8)(src&0xff); }

static u16 iot_setting_event_flag = 0;
static void iot_set_setting_event_flag(u16 flag)
{
    iot_setting_event_flag = flag;
}

u16 iot_get_setting_event_flag(void)
{
    return iot_setting_event_flag;
}

/**
 *	@brief 更新tws消息缓存到vm及更新功能状态
 */
void iot_update_function_from_tws_info(void)
{
#if CONFIG_ANC_ENABLE
    if (iot_get_setting_event_flag() & BIT(IotFuncTwsSyncTypeAncEffect)) {
        iot_deal_anc_effect(NULL, 1, 0);
    }
#endif

    iot_set_setting_event_flag(0);
}

static void iot_deal_sibling_setting(u8 *buf, u16 buf_size)
{
    u8 type;
    u16 len = IOT_READ_BIG_U16(buf);
    u8 offset = sizeof(len);
    u8 *data;
    iot_set_setting_event_flag(0);
    while (offset < len) {
        type = buf[offset++];
        data = buf + offset;
        switch (type) {
#if CONFIG_ANC_ENABLE
        case IotFuncTwsSyncTypeAncEffect:
            iot_set_anc_effect_info(data);
            offset += 25;
            break;
#endif
        default:
            return;
        }
        iot_set_setting_event_flag(iot_get_setting_event_flag() | BIT(type));
    }
    // 发送消息通知app_core要保存消息内容到vm
    iot_msg_send_to_app_core(IOT_MSG_TYPE_SETTING_UPDATE);
}

// 收到对耳消息回调
static void iot_sync_tws_func_t(void *data, u16 len, bool rx)
{
    if (rx) {
        iot_deal_sibling_setting((u8 *)data, len);
    }
}

REGISTER_TWS_FUNC_STUB(iot_tws_sync) = {
    .func_id = IOT_TWS_FUNC_ID_TWS_SYNC,
    .func    = iot_sync_tws_func_t,
};

static u16 iot_add_tws_setting_string_item(u8 *des, u8 *src, u8 src_len, u8 type)
{
    des[0] = type;
    memcpy(des + 1, src, src_len);
    return src_len + sizeof(type);
}

/**
 * @brief 同步功能信息到对耳
 *
 * @param u16 types (IotFuncTwsSyncType | IotFuncTwsSyncType | ...)
 */
void iot_func_tws_sync_to_sibling(u16 types)
{
    // 采用 L     +   [TYPE VALUE] + [TYPE VALUE] + ...结构
    //     2byte
    u16 offset = 2;
    //              L    anc effect
    u16 total_len = 2 + (1 + 25);

    u8 *setting_to_sync = zalloc(total_len);
    if (NULL == setting_to_sync) {
        printf("%s zalloc fail\n", __FUNCTION__);
        return;
    }

#if CONFIG_ANC_ENABLE
    if (types & BIT(IotFuncTwsSyncTypeAncEffect)) {
        u8 anc_voice_info[25];
        iot_get_anc_effect_info(anc_voice_info);
        offset += iot_add_tws_setting_string_item(setting_to_sync + offset, anc_voice_info, sizeof(anc_voice_info), IotFuncTwsSyncTypeAncEffect);
    }
#endif

    if (offset > 2) {
        IOT_WRITE_BIG_U16(setting_to_sync, offset);
        if (types == ((u16) - 1)) {
            tws_api_send_data_to_sibling(setting_to_sync, offset, IOT_TWS_FUNC_ID_TWS_SYNC);
        } else {
            if (tws_api_get_role() == TWS_ROLE_MASTER) {
                tws_api_send_data_to_sibling(setting_to_sync, offset, IOT_TWS_FUNC_ID_TWS_SYNC);
            }
        }
    }

    if (setting_to_sync) {
        free(setting_to_sync);
    }
}

#endif // TCFG_USER_TWS_ENABLE

#endif // (THIRD_PARTY_PROTOCOLS_SEL == LL_SYNC_EN)
