#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".charge_device_handle.data.bss")
#pragma data_seg(".charge_device_handle.data")
#pragma const_seg(".charge_device_handle.text.const")
#pragma code_seg(".charge_device_handle.text")
#endif
#include "app_config.h"
#include "asm/charge.h"
#include "asm/power_interface.h"
#include "system/includes.h"
#include "asm/lp_touch_key_api.h"
#include "app_action.h"
#include "app_main.h"
#include "app_charge.h"
#include "bt_tws.h"
#include "bt_common.h"

#if TCFG_CHARGE_ENABLE

/*
 * 充电插入拔出事件,外设处理接口
 * IR_SENSOR G_SENSOR GX8002 ...
 *
 */

static int app_device_msg_charge_full(void)
{
    return 0;
}

static int app_device_msg_ldo5v_online(u8 is_5v)
{
#if TCFG_IRSENSOR_ENABLE
    if (get_bt_tws_connect_status() && is_5v) {
        tws_api_sync_call_by_uuid('T', SYNC_CMD_EARPHONE_CHAREG_START, 300);
    }
#endif

    return 0;
}

static int app_device_msg_ldo5v_offline(int type)
{
    //只需要处理正常拔出开机,和正常拔出关机的case
    switch (type) {
    case LDO5V_OFF_TYPE_NORMAL_ON:
        break;
    case LDO5V_OFF_TYPE_NORMAL_OFF:
        break;
    }

    return 0;
}

static int app_device_charge_msg_handler(int msg, int type)
{
    switch (msg) {
    case CHARGE_EVENT_LDO5V_KEEP:
        return app_device_msg_ldo5v_online(0);
    case CHARGE_EVENT_LDO5V_IN:
        return app_device_msg_ldo5v_online(1);
    case CHARGE_EVENT_LDO5V_OFF:
        return app_device_msg_ldo5v_offline(type);
    case CHARGE_EVENT_CHARGE_FULL:
        return app_device_msg_charge_full();
    }
    return 0;
}

APP_CHARGE_HANDLER(device_charge_msg_entry, 0) = {
    .handler = app_device_charge_msg_handler,
};

#endif
