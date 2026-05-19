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

static u8 xmly_eq = 0x00;
static u8 xmly_anc = 0x00;
static u8 xmly_latency = 0x00;
static u8 xmly_wear_det = 0x00;

/***********
事件类型：
0x00:⽆事件
0x01:⾳量增加
0x02: ⾳量减少
0x03: 上⼀⾸
0x04: 下⼀⾸
0x05: 播放/暂停
0x06: ⼿机助⼿
0x07: anc模式切换
0x08: 低延时模式切换
0x09: ⼩雅键
***********/
static u8 xmly_left_key_double = 0x01;
static u8 xmly_left_key_trible = 0x02;
static u8 xmly_left_key_long   = 0x03;
static u8 xmly_right_key_double = 0x01;
static u8 xmly_right_key_trible = 0x02;
static u8 xmly_right_key_long   = 0x03;

int xmly_is_tws_master_role(void)
{
#if TCFG_USER_TWS_ENABLE
    return (tws_api_get_role() == TWS_ROLE_MASTER);
#endif
    return 1;
}

// int xmly_notify_battery(void)
int xmly_platform_get_battery(u8 *data, u16 size)
{
    data[0] = 0x63; // L bat (0x00 - 0x64)
    data[1] = 0x00; // L 0:电池供电 1:使⽤电池盒充电中
    data[2] = 0x01; // L 0：未连接  1：已连接
    data[3] = 0x63; // R bat (0x00 - 0x64)
    data[4] = 0x00; // R 0:电池供电 1:使⽤电池盒充电中
    data[5] = 0x01; // R 0：未连接  1：已连接
    data[6] = 0x63; // ⽿机仓 bat (0x00 - 0x64)
    data[7] = 0x00; // ⽿机仓 0:电池供电 1:使⽤电池盒充电中
    data[8] = 0x00; // ⽿机仓 0：未连接  1：已连接
    data[9] = 0x00; // reserve
    return 0;
}

int xmly_platform_get_setting(u8 *data, u16 size)
{
    data[0] = xmly_eq;
    data[1] = xmly_anc;
    data[2] = xmly_latency;
    data[3] = xmly_wear_det;
    return 0;
}

int xmly_platform_set_setting(u8 *data, u16 size)
{
    xmly_eq = data[0];
    xmly_anc = data[1];
    xmly_latency = data[2];
    xmly_wear_det = data[3];
    return 0;
}

int xmly_platform_get_key_setting(u8 *data, u16 size)
{
    data[0] = xmly_left_key_double;
    data[1] = xmly_left_key_trible;
    data[2] = xmly_left_key_long;
    data[3] = xmly_right_key_double;
    data[4] = xmly_right_key_trible;
    data[5] = xmly_right_key_long;
    return 0;
}

int xmly_platform_set_key_setting(u8 *data, u16 size)
{
    xmly_left_key_double = data[0];
    xmly_left_key_trible = data[1];
    xmly_left_key_long = data[2];
    xmly_right_key_double = data[3];
    xmly_right_key_trible = data[4];
    xmly_right_key_long = data[5];
    return 0;
}

#endif
