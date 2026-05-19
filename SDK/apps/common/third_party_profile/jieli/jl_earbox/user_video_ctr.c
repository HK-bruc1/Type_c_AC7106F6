/*
 * @Description:
 * @Author: HJY
 * @Date: 2024-01-13 10:04:32
 * @LastEditTime: 2025-04-22 14:24:23
 * @LastEditors:  shenzihao@zh-jieli.com
 */
#include "edr_hid_user.h"
#include "user_video_ctr.h"
#include "system/includes.h"
#include "app_action.h"
#include "btstack/btstack_task.h"
#include "btstack/bluetooth.h"
#include "user_cfg.h"
#include "vm.h"
#include "app_power_manage.h"
#include "btcontroller_modules.h"
#include "bt_common.h"
#include "3th_profile_api.h"
#include "btstack/avctp_user.h"
#include "app_chargestore.h"
#include "btcrypt.h"
#include "custom_cfg.h"
#include "classic/tws_api.h"
#include "btstack/btstack_event.h"
#if 1
//#define log_info            y_printf
#define log_info(x, ...)  printf("[EDR_HID]" x " ", ## __VA_ARGS__)
#define log_info_hexdump  put_buf

#else
#define log_info(...)
#define log_info_hexdump(...)
#endif
//抖音点赞暂不支持一拖二
#if TCFG_USER_EDR_ENABLE && (TCFG_BT_SUPPORT_HID ==1) && (!TCFG_BT_DUAL_CONN_ENABLE)
static remote_type_e connect_remote_type = REMOTE_TYPE_ANDROID;

static int bt_connect_reset_xy_vm(void);

typedef struct {
    u8 data[3];
    u8 event_type;
    u8 *event_arg;
    u8 key_val;
} mouse_packet_data_t;

static volatile mouse_packet_data_t second_packet = {0};
#define SENSOR_XLSB_IDX         			0
#define SENSOR_YLSB_XMSB_IDX    			(SENSOR_XLSB_IDX + 1)
#define SENSOR_YMSB_IDX         			(SENSOR_YLSB_XMSB_IDX +1)

#define X_RIGHT_DISTANCE         (16)
#define X_LEFT_DISTANCE         (-16)
#define Y_DOWN_DISTANCE          (16)
#define Y_UP_DISTANCE           (-16)

#define REMOTE_IS_IOS()     (connect_remote_type == REMOTE_TYPE_IOS)
#define PACKET_DELAY_TIME()        os_time_dly(3)
#define PACKET_SLOW_DELAY_TIME()        os_time_dly(6)
#define DOUBLE_KEY_DELAY_TIME()    os_time_dly(5)

static s16 save_coordinate_x_cnt = 0;
static s16 save_coordinate_y_cnt = 0;

//----------------------------------
static const u8 keypage_report_map[] = {
    0x05, 0x0C,        // Usage Page (Consumer)
    0x09, 0x01,        // Usage (Consumer Control)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x03,        //   Report ID (3)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)                   11
    0x95, 0x0B,        //   Report Count (11)
    0x09, 0xEA,        //   Usage (Volume Decrement)
    0x09, 0xE9,        //   Usage (Volume Increment)
    0x0A, 0xAE, 0x01,  //   Usage (AL Keyboard Layout)
    0x09, 0x30,        //   Usage (Power)
    0x09, 0x40,        //   Usage (menu)
    0x09, 0x46,        //   Usage (menu escape)
    0x0A, 0x23, 0x02,  //   Usage (AC Home)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x01,        //   Report Count (1)                 13
    0x75, 0x0D,        //   Report Size (13)
    0x81, 0x03,        //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              // End Collection
    0x05, 0x0D,        // Usage Page (Digitizer)
    0x09, 0x02,        // Usage (Pen)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x02,        //   Report ID (2)
    0x09, 0x22,        //   Usage (Finger)
    0xA1, 0x02,        //   Collection (Logical)
    0x09, 0x42,        //     Usage (Tip Switch)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x75, 0x01,        //     Report Size (1)                  1
    0x95, 0x01,        //     Report Count (1)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x09, 0x32,        //     Usage (In Range)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x06,        //     Report Count (6)                 6
    0x81, 0x03,        //     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x08,        //     Report Size (8)                  48
    0x09, 0x51,        //     Usage (0x51) //ContactID
    0x95, 0x01,        //     Report Count (1)                  8
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x26, 0xFF, 0x0F,  //     Logical Maximum (4095)
    0x75, 0x10,        //     Report Size (16)                   16
    0x55, 0x0E,        //     Unit Exponent (-2)
    0x65, 0x33,        //     Unit (System: English Linear, Length: Inch)
    0x09, 0x30,        //     Usage (X)
    0x35, 0x00,        //     Physical Minimum (0)
    0x46, 0xB5, 0x04,  //     Physical Maximum (1205)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x46, 0x8A, 0x03,  //     Physical Maximum (906)
    0x09, 0x31,        //     Usage (Y)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              //   End Collection
    0x05, 0x0D,        //   Usage Page (Digitizer)
    0x09, 0x54,        //   Usage (0x54) //ContactCount
    0x95, 0x01,        //   Report Count (1)                     8
    0x75, 0x08,        //   Report Size (8)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x85, 0x08,        //   Report ID (8)
    0x09, 0x55,        //   Usage (0x55) //ContactMax
    0x25, 0x05,        //   Logical Maximum (5)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0,              // End Collection
    // 119 bytes
};


static const u8 keypage_report_map_ios[] = {
    0x05, 0x0C,        // Usage Page (Consumer)
    0x09, 0x01,        // Usage (Consumer Control)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x03,        //   Report ID (3)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)                          11
    0x95, 0x0B,        //   Report Count (11)
    0x09, 0xEA,        //   Usage (Volume Decrement)
    0x09, 0xE9,        //   Usage (Volume Increment)
    0x0A, 0xAE, 0x01,  //   Usage (AL Keyboard Layout)
    0x09, 0x30,        //   Usage (Power)
    0x09, 0x40,        //   Usage (menu)
    0x09, 0x46,        //   Usage (menu escape)
    0x0A, 0x23, 0x02,  //   Usage (AC Home)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x01,        //   Report Count (1)                          13
    0x75, 0x0D,        //   Report Size (13)
    0x81, 0x03,        //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              // End Collection
    0x05, 0x0D,        // Usage Page (Digitizer)
    0x09, 0x02,        // Usage (Pen)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x02,        //   Report ID (2)
    0x09, 0x22,        //   Usage (Finger)
    0xA1, 0x02,        //   Collection (Logical)
    0x09, 0x42,        //     Usage (Tip Switch)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x75, 0x01,        //     Report Size (1)                         1
    0x95, 0x01,        //     Report Count (1)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x09, 0x32,        //     Usage (In Range)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x06,        //     Report Count (6)                         6
    0x81, 0x03,        //     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x08,        //     Report Size (8)                          48
    0x09, 0x51,        //     Usage (0x51)
    0x95, 0x01,        //     Report Count (1)                         8
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x26, 0xFF, 0x0F,  //     Logical Maximum (4095)
    0x75, 0x10,        //     Report Size (16)                          16
    0x55, 0x0E,        //     Unit Exponent (-2)
    0x65, 0x33,        //     Unit (System: English Linear, Length: Inch)
    0x09, 0x30,        //     Usage (X)
    0x35, 0x00,        //     Physical Minimum (0)
    0x46, 0xB5, 0x04,  //     Physical Maximum (1205)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x46, 0x8A, 0x03,  //     Physical Maximum (906)
    0x09, 0x31,        //     Usage (Y)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              //   End Collection
    0x05, 0x0D,        //   Usage Page (Digitizer)
    0x09, 0x54,        //   Usage (0x54)
    0x95, 0x01,        //   Report Count (1)                           8
    0x75, 0x08,        //   Report Size (8)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x85, 0x08,        //   Report ID (8)
    0x09, 0x55,        //   Usage (0x55)
    0x25, 0x05,        //   Logical Maximum (5)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0,              // End Collection
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x02,        // Usage (Mouse)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x04,        //   Report ID (4)
    0x09, 0x01,        //   Usage (Pointer)
    0xA1, 0x00,        //   Collection (Physical)
    0x95, 0x05,        //     Report Count (5)                          5
    0x75, 0x01,        //     Report Size (1)
    0x05, 0x09,        //     Usage Page (Button)
    0x19, 0x01,        //     Usage Minimum (0x01)
    0x29, 0x05,        //     Usage Maximum (0x05)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x01,        //     Report Count (1)                           3
    0x75, 0x03,        //     Report Size (3)
    0x81, 0x01,        //     Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x08,        //     Report Size (8)                             8
    0x95, 0x01,        //     Report Count (1)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x38,        //     Usage (Wheel)
    0x15, 0x81,        //     Logical Minimum (-127)
    0x25, 0x7F,        //     Logical Maximum (127)
    0x81, 0x06,        //     Input (Data,Var,Rel,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x0C,        //     Usage Page (Consumer)
    0x0A, 0x38, 0x02,  //     Usage (AC Pan)
    0x95, 0x01,        //     Report Count (1)                           8
    0x81, 0x06,        //     Input (Data,Var,Rel,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              //   End Collection
    0x85, 0x05,        //   Report ID (5)
    0x09, 0x01,        //   Usage (Consumer Control)
    0xA1, 0x00,        //   Collection (Physical)
    0x75, 0x0C,        //     Report Size (12)                           24
    0x95, 0x02,        //     Report Count (2)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x31,        //     Usage (Y)
    0x16, 0x01, 0xF8,  //     Logical Minimum (-2047)
    0x26, 0xFF, 0x07,  //     Logical Maximum (2047)
    0x81, 0x06,        //     Input (Data,Var,Rel,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              //   End Collection
    0xC0,              // End Collection

    // 203 bytes
};



void usr_set_remote_type(u8 flag)
{
    printf("func:%s , flag :%d\n", __func__, flag);
    if (flag ==  2) {
        connect_remote_type = REMOTE_TYPE_IOS;
    } else {
        connect_remote_type = REMOTE_TYPE_ANDROID;
    }
}

u8 usr_get_remote_type(void)
{
    return connect_remote_type;
}

static int handle_send_packect(const u8 *packet_table, u16 table_size, u8 packet_len)
{
    int i;
    u8 *pt = (u8 *)packet_table;
    int (*hid_data_send_pt)(u8 report_id, u8 * data, u16 len) = NULL;

#if TCFG_USER_EDR_ENABLE
    hid_data_send_pt = edr_hid_data_send;
#endif


    if (!hid_data_send_pt) {
        log_info("no bt");
        return -1;
    }

    printf("remote_type:%s", (connect_remote_type == REMOTE_DEV_IOS) ? "IOS" : "Android");
    log_info("send_packet:%08x,size=%d,len=%d", (u32)packet_table, table_size, packet_len);

    // PACKET_DELAY_TIME();
    for (i = 0; i < table_size; i += packet_len) {
        put_buf(pt, packet_len);
        hid_data_send_pt(pt[0], &pt[1], packet_len - 1);
        pt += packet_len;
        if ((table_size - i) / packet_len < 4) {
            PACKET_SLOW_DELAY_TIME();
        } else {
            PACKET_DELAY_TIME();
        }
    }
    return 0;
}


///音量加减
#define  REPROT_CONSUMER_LEN      (1+3)
static const u8 key_vol_add[][REPROT_CONSUMER_LEN] = {
    {3, 0x02, 0x00, 0x00 },
    {3, 0x00, 0x00, 0x00 },
};
static const u8 key_vol_dec[][REPROT_CONSUMER_LEN] = {
    {3, 0x01, 0x00, 0x00 },
    {3, 0x00, 0x00, 0x00 },
};

void key_vol_ctrl(u8 add_dec)
{
    r_printf("%s[%d]", __func__, add_dec);
    if (add_dec) {
        handle_send_packect((u8 *)key_vol_dec, sizeof(key_vol_dec), REPROT_CONSUMER_LEN);
    } else {
        handle_send_packect((u8 *)key_vol_add, sizeof(key_vol_add), REPROT_CONSUMER_LEN);
    }
}

void key_take_photo()
{
    static u8 vol_flag = 0;
    r_printf("%s[%d]", __func__, vol_flag);
    if (vol_flag) {
        handle_send_packect((u8 *)key_vol_dec, sizeof(key_vol_dec), REPROT_CONSUMER_LEN);
        vol_flag = 0;
    } else {
        handle_send_packect((u8 *)key_vol_add, sizeof(key_vol_add), REPROT_CONSUMER_LEN);
        vol_flag = 1;
    }
}

//安卓ios和andoid都可以息屏和亮屏
static const u8 key_power[][REPROT_CONSUMER_LEN] = {
    {3, 0x08, 0x00, 0x00 },
    {3, 0x00, 0x00, 0x00 },
};

//ios返回桌面
static const u8 key_nemu[][REPROT_CONSUMER_LEN] = {
    {3, 0x10, 0x00, 0x00 },
    {3, 0x00, 0x00, 0x00 },
};

//2个平台都无用
static const u8 key_menu_exit[][REPROT_CONSUMER_LEN] = {
    {3, 0x20, 0x00, 0x00 },
    {3, 0x00, 0x00, 0x00 },
};

//安卓返回桌面
static const u8 key_android_home[][REPROT_CONSUMER_LEN] = {
    {3, 0x40, 0x00, 0x00 },
    {3, 0x00, 0x00, 0x00 },
};

void key_power_ctrl(void)
{
    r_printf("%s", __func__);
    handle_send_packect((u8 *)key_power, sizeof(key_power), REPROT_CONSUMER_LEN);//电源键都ok
}

void key_memu_ctrl(void)
{
    r_printf("%s", __func__);
    handle_send_packect((u8 *)key_nemu, sizeof(key_nemu), REPROT_CONSUMER_LEN);//ios菜单ok  安卓挂
    handle_send_packect((u8 *)key_android_home, sizeof(key_android_home), REPROT_CONSUMER_LEN);
}

//report_id + data
#define  REPROT_INFO_LEN0         (1+3)
#define  REPROT_INFO_LEN1         (1+8)
//------------------------------------------------------
static const u8 key_idle_ios_0[][REPROT_INFO_LEN0] = {
    //松手
    {5, 0x00, 0xE0, 0xFF },
    {4, 0x00, 0x00, 0x00 },
};
static const u8 key_idle_ios_1[][REPROT_INFO_LEN0] = {
    //松手
    {5, 0x00, 0x10, 0x00 },
    {4, 0x00, 0x00, 0x00 },
};
extern u8 get_key_active(void);
void bt_connect_timer_loop(void *priv)
{
    static u8 flag = 0;

#if TCFG_USER_EDR_ENABLE
    if (!edr_hid_is_connected()) {
        return;
    }
#endif
    if (get_key_active()) {
        return;
    }
    if (REMOTE_IS_IOS()) {
        if (flag) {
            handle_send_packect((u8 *)key_idle_ios_0, sizeof(key_idle_ios_0), REPROT_INFO_LEN0);
            flag = 0;
        } else {
            handle_send_packect((u8 *)key_idle_ios_1, sizeof(key_idle_ios_1), REPROT_INFO_LEN0);
            flag = 1;
        }
    }
}

//连接复位坐标
static const u8 key_connect_before[][REPROT_INFO_LEN0] = {
    {4, 0x00, 0x00, 0x00 },
    {4, 0x00, 0x00, 0x00 },
    /* {5, 0x01, 0xF8, 0x7F }, //左下*/
    /* {5, 0xFF, 0x17, 0x80 }, //右上 */
    {5, 0x01, 0x18, 0x80 }, //左上
    {5, 0x01, 0x18, 0x80 }, //左上
    {5, 0x01, 0x18, 0x80 }, //左上
    //复位坐标点，发松手坐标

    {5, 0x15, 0x00, 0x00 },
    {5, 0x15, 0x00, 0x00 },
    {5, 0x10, 0x00, 0x00 },
    {5, 0x10, 0x00, 0x00 },
    // {5, 0x10, 0x00, 0x00 },


    {5, 0x00, 0x00, 0x02 },
    {5, 0x00, 0x00, 0x02 },
    {5, 0x00, 0x00, 0x02 },
    {5, 0x00, 0x00, 0x02 },


    {4, 0x00, 0x00, 0x00 },
};

void bt_connect_reset_xy(void)
{
    r_printf("%s", __func__);

#if TCFG_USER_EDR_ENABLE
    if (!edr_hid_is_connected()) {
        return;
    }
#endif
    if (!bt_connect_reset_xy_vm()) {
        return;
    }
    if (REMOTE_IS_IOS()) {
        handle_send_packect((u8 *)key_connect_before, sizeof(key_connect_before), REPROT_INFO_LEN0);
    }
}

//连接复位坐标
static const u8 key_reset_coordinate[][REPROT_INFO_LEN0] = {
    {4, 0x00, 0x00, 0x00 },
    {4, 0x00, 0x00, 0x00 },
    /* {5, 0x01, 0xF8, 0x7F }, //左下*/
    /* {5, 0xFF, 0x17, 0x80 }, //右上 */
    {5, 0x01, 0x18, 0x80 }, //左上
    {5, 0x01, 0x18, 0x80 }, //左上
    {5, 0x01, 0x18, 0x80 }, //左上
    {5, 0x01, 0x18, 0x80 }, //左上
    {5, 0x01, 0x18, 0x80 }, //左上
    //复位坐标点，发松手坐标


    {4, 0x00, 0x00, 0x00 },
};

void bt_connect_reset_coordinate(void)
{
    r_printf("%s", __func__);

#if TCFG_USER_EDR_ENABLE
    if (!edr_hid_is_connected()) {
        return;
    }
#endif
    if (REMOTE_IS_IOS()) {
        handle_send_packect((u8 *)key_reset_coordinate, sizeof(key_reset_coordinate), REPROT_INFO_LEN0);
    }
}

static void key_send_coordinate(s16 axis_x, s16 axis_y)
{
    printf("HJY (x:%d,y:%d)\n", axis_x, axis_y);

    if ((axis_x >= -2047) && (axis_x <= 2047)) {
    } else {
        axis_x = 0;
    }

    if ((axis_y >= -2047) && (axis_y <= 2047)) {
    } else {
        axis_y = 0;
    }

    second_packet.data[SENSOR_XLSB_IDX] = axis_x & 0xFF;
    second_packet.data[SENSOR_YLSB_XMSB_IDX] = ((axis_y << 4) & 0xF0) | ((axis_x >> 8) & 0x0F);
    second_packet.data[SENSOR_YMSB_IDX] = (axis_y >> 4) & 0xFF;
    if (REMOTE_IS_IOS()) {
#if TCFG_USER_EDR_ENABLE
        edr_hid_data_send(5, (u8 *)second_packet.data, sizeof(second_packet.data));
#endif
    }
}
// 抖音坐标右移
void key_send_coordinate_x_right(void)
{
    key_send_coordinate(X_RIGHT_DISTANCE, 0);
}
// 抖音坐标左移
void key_send_coordinate_x_left(void)
{

    key_send_coordinate(X_LEFT_DISTANCE, 0);
}
// 抖音坐标上移
void key_send_coordinate_y_up(void)
{

    key_send_coordinate(0, Y_UP_DISTANCE);
}
// 抖音坐标下移
void key_send_coordinate_y_down(void)
{

    key_send_coordinate(0, Y_DOWN_DISTANCE);
}
// 抖音确认坐标
void key_coordinate_confirm(void)
{

    int ret = syscfg_write(CFG_USER_TIKTOK_X, &save_coordinate_x_cnt, 2);
    if (ret != 2) {
        printf(" save_coordinate_x save err!!!\n");
    }
    ret = syscfg_write(CFG_USER_TIKTOK_Y, &save_coordinate_y_cnt, 2);
    if (ret != 2) {
        printf(" save_coordinate_y save err!!!\n");
    }
    printf("save_coordinate_x_cnt:%d,save_coordinate_y_cnt:%d \n", save_coordinate_x_cnt, save_coordinate_y_cnt);
}
// 抖音坐标复位原点
void key_send_reset_coordinate(void)
{
    if (REMOTE_IS_IOS()) {
        handle_send_packect((u8 *)key_reset_coordinate, sizeof(key_reset_coordinate), REPROT_INFO_LEN0);
    }
}

static int bt_connect_reset_xy_vm(void)
{
    int ret = 0;
    int i;
    if ((0 == save_coordinate_x_cnt) && (0 == save_coordinate_y_cnt)) {
        ret = syscfg_read(CFG_USER_TIKTOK_X, &save_coordinate_x_cnt, 2);
        if (ret != 2) {
            printf("read save_coordinate_x err!!!\n");
            return -1;
        }
        ret = syscfg_read(CFG_USER_TIKTOK_Y, &save_coordinate_y_cnt, 2);
        if (ret != 2) {
            printf("read save_coordinate_y err!!!\n");
            return -1;
        }
    }
    key_send_reset_coordinate();
    PACKET_DELAY_TIME();

    printf("save_coordinate_x_cnt:%d,save_coordinate_y_cnt:%d \n", save_coordinate_x_cnt, save_coordinate_y_cnt);

    if (save_coordinate_x_cnt > 0) {
        for (i = 0; i < save_coordinate_x_cnt; i++) {
            key_send_coordinate_x_right();
            PACKET_DELAY_TIME();
        }
    } else {
        for (i = 0; i > save_coordinate_x_cnt; i--) {
            key_send_coordinate_x_left();
            PACKET_DELAY_TIME();
        }
    }

    if (save_coordinate_y_cnt > 0) {
        for (i = 0; i < save_coordinate_y_cnt; i++) {
            key_send_coordinate_y_down();
            PACKET_DELAY_TIME();
        }
    } else {
        for (i = 0; i > save_coordinate_y_cnt; i--) {
            key_send_coordinate_y_up();
            PACKET_DELAY_TIME();
        }
    }
    return 0;

}



//上键（短按）
static const u8 key_up_before[][REPROT_INFO_LEN0] = {//上一个
    {4, 0x01, 0x00, 0x00 },
    {5, 0x00, 0x00, 0x02 },
    {5, 0x00, 0x00, 0x03 },
    {4, 0x00, 0x00, 0x00 },

    {5, 0x00, 0x00, 0xFE },
    {5, 0x00, 0x00, 0xFD },
    {4, 0x00, 0x00, 0x00 },
};

//Report ID (2)
static const u8 key_up_click[][REPROT_INFO_LEN1] = {
    {2, 0x07, 0x06, 0x70, 0x07, 0xf4, 0x03, 0x01, 0x00 },
    {2, 0x07, 0x06, 0x70, 0x07, 0x4c, 0x04, 0x01, 0x00 },
    {2, 0x07, 0x06, 0x70, 0x07, 0x78, 0x05, 0x01, 0x00 },
    {2, 0x07, 0x06, 0x70, 0x07, 0xa4, 0x06, 0x01, 0x00 },
    {2, 0x07, 0x06, 0x70, 0x07, 0xd0, 0x07, 0x01, 0x00 },
    {2, 0x07, 0x06, 0x70, 0x07, 0xfc, 0x08, 0x01, 0x00 },
    {2, 0x07, 0x06, 0x70, 0x07, 0x28, 0x0a, 0x01, 0x00 },
    {2, 0x07, 0x06, 0x70, 0x07, 0x54, 0x0b, 0x01, 0x00 },
    {2, 0x07, 0x06, 0x70, 0x07, 0x80, 0x0c, 0x01, 0x00 },
    {2, 0x00, 0x06, 0x70, 0x07, 0xac, 0x0d, 0x00, 0x00 },
};

//上键（HOLD） 收到按键HOLD消息发1次，
//Report ID (3)
static const u8 key_up_hold_press[][REPROT_INFO_LEN0] = {
    {3, 0x02, 0x00, 0x00 },
};
static const u8 key_up_hold_release[][REPROT_INFO_LEN0] = {
    {3, 0x00, 0x00, 0x00 },
};

//------------------------------------------------------
//下键（短按）
static const u8 key_down_before[][REPROT_INFO_LEN0] = {//下一个
    {4, 0x01, 0x00, 0x00 },
    {5, 0x00, 0x00, 0xFE },
    {5, 0x00, 0x00, 0xFD },
    {4, 0x00, 0x00, 0x00 },

    {5, 0x00, 0x00, 0x02 },
    {5, 0x00, 0x00, 0x03 },
    {4, 0x00, 0x00, 0x00 },
};

static const u8 key_down_click[][REPROT_INFO_LEN1] = {
    //Report ID (2)
    {2, 0x07, 0x06, 0x70, 0x07, 0x80, 0x0c, 0x01, 0x00 },
    {2, 0x07, 0x06, 0x70, 0x07, 0x28, 0x0a, 0x01, 0x00 },
    {2, 0x07, 0x06, 0x70, 0x07, 0xfc, 0x08, 0x01, 0x00 },
    {2, 0x07, 0x06, 0x70, 0x07, 0xd0, 0x07, 0x01, 0x00 },
    {2, 0x07, 0x06, 0x70, 0x07, 0xa4, 0x06, 0x01, 0x00 },
    {2, 0x07, 0x06, 0x70, 0x07, 0x78, 0x05, 0x01, 0x00 },
    {2, 0x07, 0x06, 0x70, 0x07, 0x4c, 0x04, 0x01, 0x00 },
    {2, 0x07, 0x06, 0x70, 0x07, 0x20, 0x03, 0x01, 0x00 },
    {2, 0x07, 0x06, 0x70, 0x07, 0xf4, 0x01, 0x01, 0x00 },
    {2, 0x00, 0x06, 0x70, 0x07, 0xc8, 0x00, 0x00, 0x00 },
};

//下键（HOLD） 收到按键HOLD消息发1次
//Report ID (3)
static const u8 key_down_hold_press[][REPROT_INFO_LEN0] = {
    {3, 0x01, 0x00, 0x00 },
};

static const u8 key_down_hold_release[][REPROT_INFO_LEN0] = {
    {3, 0x00, 0x00, 0x00 }
};

//---------------------------------------------------------
//左键（短按）
static const u8 key_left_before[][REPROT_INFO_LEN0] = { //左滑
    {4, 0x01, 0x00, 0x00 },
    {5, 0x20, 0x00, 0x00 },
    {5, 0x20, 0x00, 0x00 },
    {4, 0x00, 0x00, 0x00 },

    {5, 0xE0, 0x0F, 0x00 },
    {5, 0xE0, 0x0F, 0x00 },
    {4, 0x00, 0x00, 0x00 },
};

static const u8 key_left_click[][REPROT_INFO_LEN1] = {
    //Report ID (2)
    {2, 0x07, 0x04, 0x00, 0x02, 0x70, 0x04, 0x01, 0x00 },
    {2, 0x07, 0x04, 0x8a, 0x02, 0x70, 0x04, 0x01, 0x00 },
    {2, 0x07, 0x04, 0xb4, 0x03, 0x70, 0x04, 0x01, 0x00 },
    {2, 0x07, 0x04, 0xe2, 0x04, 0x70, 0x04, 0x01, 0x00 },
    {2, 0x07, 0x04, 0x0e, 0x06, 0x70, 0x04, 0x01, 0x00 },
    {2, 0x07, 0x04, 0x3a, 0x07, 0x70, 0x04, 0x01, 0x00 },
    {2, 0x07, 0x04, 0x66, 0x08, 0x70, 0x04, 0x01, 0x00 },
    {2, 0x07, 0x04, 0x92, 0x09, 0x70, 0x04, 0x01, 0x00 },
    {2, 0x07, 0x04, 0xbe, 0x0a, 0x70, 0x04, 0x01, 0x00 },
    {2, 0x07, 0x04, 0xea, 0x0b, 0x70, 0x04, 0x01, 0x00 },
    {2, 0x00, 0x04, 0x16, 0x0d, 0x70, 0x04, 0x00, 0x00 },
};

//---------------------------------------------------------
//右键（短按）
static const u8 key_right_before[][REPROT_INFO_LEN0] = {//右滑
    {4, 0x01, 0x00, 0x00 },
    {5, 0xE0, 0x0F, 0x00 },
    {5, 0xE0, 0x0F, 0x00 },
    {4, 0x00, 0x00, 0x00 },

    {5, 0x20, 0x00, 0x00 },
    {5, 0x20, 0x00, 0x00 },
    {4, 0x00, 0x00, 0x00 },
};

static const u8 key_right_click[][REPROT_INFO_LEN1] = {
    //Report ID (2)
    {2, 0x07, 0x05, 0x00, 0x0d, 0x70, 0x04, 0x01, 0x00 },
    {2, 0x07, 0x05, 0x48, 0x0d, 0x70, 0x04, 0x01, 0x00 },
    {2, 0x07, 0x05, 0x1c, 0x0c, 0x70, 0x04, 0x01, 0x00 },
    {2, 0x07, 0x05, 0xf0, 0x0a, 0x70, 0x04, 0x01, 0x00 },
    {2, 0x07, 0x05, 0xc4, 0x09, 0x70, 0x04, 0x01, 0x00 },
    {2, 0x07, 0x05, 0x98, 0x08, 0x70, 0x04, 0x01, 0x00 },
    {2, 0x07, 0x05, 0x6c, 0x07, 0x70, 0x04, 0x01, 0x00 },
    {2, 0x07, 0x05, 0x40, 0x06, 0x70, 0x04, 0x01, 0x00 },
    {2, 0x07, 0x05, 0x14, 0x05, 0x70, 0x04, 0x01, 0x00 },
    {2, 0x07, 0x05, 0xe8, 0x03, 0x70, 0x04, 0x01, 0x00 },
    {2, 0x00, 0x05, 0xbc, 0x02, 0x70, 0x04, 0x00, 0x00 },
};

//---------------------------------------------------------
//中键（短按，长按）
//-------按下-------
static u8 key_pp_press[][REPROT_INFO_LEN1] = {
    //Report ID (2)
    {2, 0x07, 0x07, 0x70, 0x07, 0x70, 0x07, 0x01, 0x00 },
};

//-------松开-------
static u8 key_pp_release_before[][REPROT_INFO_LEN0] = {//点赞
    {4, 0x01, 0x00, 0x00 },
    {4, 0x00, 0x00, 0x00 },
};

static u8 key_pp_release[][REPROT_INFO_LEN1] = {
    //Report ID (2)
    {2, 0x00, 0x07, 0x70, 0x07, 0x70, 0x07, 0x00, 0x00  },
};

//双击相当于连续发两次
//HOLD动作 就是先发按下，等松手再发松开包


//---------------------------------------------------------
//单键（短按，长按）
//-------按下-------
static const u8 key_one_press_before[][REPROT_INFO_LEN0] = {//暂停
    {4, 0x01, 0x00, 0x00 },
};

static const u8 key_one_press[][REPROT_INFO_LEN1] = {
    //Report ID (2)
    {2, 0x07, 0x08, 0x00, 0x08, 0x60, 0x0d, 0x01, 0x00  },
};

//-------松开-------
static const u8 key_one_release_before[][REPROT_INFO_LEN0] = {
    //Report ID (4)
    {4, 0x00, 0x00, 0x00 },
};
static const u8 key_one_release[][REPROT_INFO_LEN1] = {
    //Report ID (2)
    {2, 0x00, 0x08, 0x00, 0x08, 0x60, 0x0d, 0x00, 0x00  },
};


//------
//双击相当于连续发两次
//HOLD动作 就是先发按下，等松手再发松开包

//****** 相机界面焦点移动  ****/
typedef enum {
    MOVE_UP = 0,
    MOVE_DOWN,
    MOVE_LEFT,
    MOVE_RIGHT,
} MOVE_DIRECTION;
static char *move_str[4] = {"up", "down", "left", "right"};

static u8 key_focus_ios_reset[][REPROT_INFO_LEN0] = {//相机移动焦点，带复位坐标
    {4, 0x00, 0x00, 0x00 },
    {5, 0x01, 0xF8, 0x7F }, //左下
    {5, 0x01, 0xF8, 0x7F }, //左下
    {5, 0x01, 0xF8, 0x7F }, //左下

    {5, 0x70, 0x00, 0xFB }, //移动坐标点,默认拍照图标点坐标，对应乌龟灵敏度是25%
};

static u8 key_focus_ios[][REPROT_INFO_LEN0] = {//相机移动焦点
    {4, 0x00, 0x00, 0x00 },
    {5, 0x00, 0x00, 0x00 },
};

#define MOUSE_STEP   (16 * 1)
static void keypage_coordinate_equal_ios(MOVE_DIRECTION dir)
{
#if 1  //带复位坐标的移动
    u8 *pos = key_focus_ios_reset[4];
    u16 ios_x = ((pos[2] & 0x0F) << 4) | (pos[1] & 0xFF);
    u16 ios_y = ((pos[3] & 0xFF) << 4) | (pos[2] >> 4 & 0x0F);
    if (dir == MOVE_RIGHT) {
        ios_x += MOUSE_STEP;
    }
    if (dir == MOVE_LEFT) {
        if (ios_x < MOUSE_STEP) {
            ios_x = 0;
        } else {
            ios_x -= MOUSE_STEP;
        }
    }
    if (dir == MOVE_DOWN) {
        ios_y += MOUSE_STEP;
    }
    if (dir == MOVE_UP) {
        if (ios_y < MOUSE_STEP) {
            ios_y = 0;
        } else {
            ios_y -= MOUSE_STEP;
        }
    }
    pos[1] = (ios_x & 0xFF);
    pos[2] = ((ios_y & 0x0F) << 4) | (ios_x >> 8 & 0x0F);
    pos[3] = (ios_y >> 4 & 0xFF);
    log_info("%s[x:%d y:%d]", __func__, ios_x, ios_y);
    if (REMOTE_IS_IOS()) {
        handle_send_packect((u8 *)key_focus_ios_reset, sizeof(key_focus_ios_reset), REPROT_INFO_LEN0);
    }
#else
    u16 ios_x = 0, ios_y = 0;
    u8 *pos = key_focus_ios[1];
    if (dir == MOVE_RIGHT) {
        ios_x = MOUSE_STEP;
    }
    if (dir == MOVE_LEFT) {
        ios_x = 0x0FFF - MOUSE_STEP;
    }
    if (dir == MOVE_DOWN) {
        ios_y = MOUSE_STEP;
    }
    if (dir == MOVE_UP) {
        ios_y = 0x0FFF - MOUSE_STEP;
    }
    pos[1] = (ios_x & 0xFF);
    pos[2] = ((ios_y & 0x0F) << 4) | (ios_x >> 8 & 0x0F);
    pos[3] = (ios_y >> 4 & 0xFF);
    log_info("%s", __func__);
    /* put_buf(pos, REPROT_INFO_LEN0); */
    if (REMOTE_IS_IOS()) {
        handle_send_packect((u8 *)key_focus_ios, sizeof(key_focus_ios), REPROT_INFO_LEN0);
    }
#endif
}

//焦点尽量居中
static u8 key_focus_android[][REPROT_INFO_LEN1] = {
    {2, 0x07, 0x08, 0x70, 0x08, 0x70, 0x07, 0x01, 0x00  },
    /* {2, 0x07, 0x08, 0x70, 0x08, 0x70, 0x07, 0x01, 0x00  }, */
    {2, 0x00, 0x08, 0x70, 0x08, 0x70, 0x07, 0x01, 0x00  },
};

#define TOUCH_STEP        (128 * 2) //安卓焦点移动步进
static u16  x_lab, y_lab;
static u16  x_lab_low = 0, x_lab_hig = 0, y_lab_low = 0, y_lab_hig = 0;
static void keypage_coordinate_equal(MOVE_DIRECTION dir)
{
    x_lab = (key_focus_android[0][4] << 8) | (key_focus_android[0][3]);
    y_lab = (key_focus_android[0][6] << 8) | (key_focus_android[0][5]);
    if (dir == MOVE_RIGHT) {
        x_lab += TOUCH_STEP;
        if (x_lab > 4095) {
            x_lab = 0;
        }
    }
    if (dir == MOVE_LEFT) {
        x_lab -= TOUCH_STEP;
        if (x_lab <= 0) {
            x_lab = 4095;
        }
    }
    if (dir == MOVE_DOWN) {
        y_lab += TOUCH_STEP;
        if (y_lab >= 4095) {
            y_lab = 0;
        }
    }
    if (dir == MOVE_UP) {
        y_lab -= TOUCH_STEP;
        if (y_lab <= 0) {
            y_lab =  4095;
        }
    }
    x_lab_low = x_lab & 0xff;
    x_lab_hig = x_lab >> 8;
    y_lab_low = y_lab & 0xff;
    y_lab_hig = y_lab >> 8;
    key_focus_android[0][3] = key_focus_android[1][3] = x_lab_low;
    key_focus_android[0][4] = key_focus_android[1][4] = x_lab_hig;
    key_focus_android[0][5] = key_focus_android[1][5] = y_lab_low;
    key_focus_android[0][6] = key_focus_android[1][6] = y_lab_hig;
    log_info("%s[%d  %d]", __func__, x_lab, y_lab);
    /* put_buf(key_focus_android, sizeof(key_focus_android)); */
    handle_send_packect((u8 *)key_focus_android, sizeof(key_focus_android), REPROT_INFO_LEN1);
}

static void keypage_focus_move(MOVE_DIRECTION dir)
{
    log_info("%s[dir:%s]", __func__, move_str[dir]);
    if (REMOTE_IS_IOS()) {
        keypage_coordinate_equal_ios(dir);
    } else {
        keypage_coordinate_equal(dir);
    }
    if (0) { /// 复位原点
        bt_connect_reset_xy();
        return;
    }
}

//点击一次已定位的焦点
static void keypage_click_focus(void)
{
    log_info("%s", __func__);
    if (REMOTE_IS_IOS()) {
        handle_send_packect((u8 *)key_focus_ios_reset, sizeof(key_focus_ios_reset), REPROT_INFO_LEN0);
        handle_send_packect((u8 *)key_pp_release_before, sizeof(key_pp_release_before), REPROT_INFO_LEN0);
    } else {
        handle_send_packect((u8 *)key_focus_android, sizeof(key_focus_android), REPROT_INFO_LEN1);
    }
}

//系统相机拍照功能
static u8 key_photo_android[][REPROT_INFO_LEN1] = {
    {2, 0x07, 0x08, 0x00, 0x08, 0x50, 0x0e, 0x01, 0x00  },
    {2, 0x00, 0x08, 0x00, 0x08, 0x50, 0x0e, 0x01, 0x00  },
};

void keypage_photo(void)
{
    log_info("%s", __func__);
    if (REMOTE_IS_IOS()) {
        static u8 flag = 0;
        flag = !flag;
        key_vol_ctrl(!flag);
    } else {
        handle_send_packect((u8 *)key_photo_android, sizeof(key_photo_android), REPROT_INFO_LEN1);
    }
}

//自动定时循环, 点击已定位的坐标点
static void keypage_auto_click_focus_loop(void *arg)
{
    log_info("%s", __func__);
    if (REMOTE_IS_IOS()) {
        handle_send_packect((u8 *)key_pp_release_before, sizeof(key_pp_release_before), REPROT_INFO_LEN0);
    } else {
        handle_send_packect((u8 *)key_focus_android, sizeof(key_focus_android), REPROT_INFO_LEN1);
    }
}

static u16 auto_click_focus_timer_loop = 0;
static void keypage_auto_click_focus_ctrl(void)
{
    if (auto_click_focus_timer_loop) {
        sys_timer_del(auto_click_focus_timer_loop);
        auto_click_focus_timer_loop = 0;
    } else {
        auto_click_focus_timer_loop = sys_timer_add(NULL, keypage_auto_click_focus_loop, 100);
    }
}

//自动划屏
#define AUTO_NEXT_TIME     (5 * 1000) //5s // 自动下一个视频时间
static void keypage_auto_next_loop(void *arg)
{
    log_info("%s", __func__);
    if (REMOTE_IS_IOS()) {
        handle_send_packect((u8 *)key_down_before, sizeof(key_down_before), REPROT_INFO_LEN0);
    } else {
        handle_send_packect((u8 *)key_down_click, sizeof(key_down_click), REPROT_INFO_LEN1);
    }
}

static u16 auto_next_timer_loop = 0;
static void keypage_auto_next_ctrl(void)
{
    if (auto_next_timer_loop) {
        sys_timer_del(auto_next_timer_loop);
        auto_next_timer_loop = 0;
    } else {
        auto_next_timer_loop = sys_timer_add(NULL, keypage_auto_next_loop, AUTO_NEXT_TIME);
    }
}

static int bt_connect_phone_back_start(void)
{
    log_info("bt_connect_phone_back_start");
#if 0
    //该函数已经改成static
    extern u8 connect_last_device_from_vm();
    if (connect_last_device_from_vm()) {
        log_info("---connect_last_device_from_vm");
        return 1 ;
    }
    return 0;
#else
    bt_cmd_prepare(USER_CTRL_CONNECT_LAST_REMOTE_INFO, 0, NULL);
    return 1;
#endif
}

#define COORDINATE_VM_HEAD_TAG   (0x4000)
typedef struct {
    u8   head_tag;
    u16  x_vm_lab;
    u16  y_vm_lab;
} hid_vm_lab;

static void keypage_coordinate_vm_deal(u8 flag)
{
    hid_vm_lab lab;
    int ret = 0;
    int len = sizeof(hid_vm_lab);
    memset(&lab, 0, sizeof(hid_vm_lab));
    if (flag == 0) {
        //ret = syscfg_read(CFG_COORDINATE_ADDR, (u8 *)&lab, sizeof(lab));
        if (ret <= 0) {
            log_info("init null \n");
            x_lab =  0x0770;
            y_lab =  0x0770;
        } else {

            x_lab = lab.x_vm_lab;
            y_lab = lab.y_vm_lab;
        }
    } else {
        lab.x_vm_lab = x_lab;
        lab.y_vm_lab = y_lab;
        // ret = syscfg_write(CFG_COORDINATE_ADDR, (u8 *)&lab, sizeof(lab));
    }
}

void keypage_app_key_deal_test(u8 key_value)
{
    u16 key_msg = 0;
    g_printf("%s[key:%d, task:%s]", __func__, key_value, os_current_task());

#if TCFG_USER_EDR_ENABLE
    // if (bt_hid_mode == HID_MODE_EDR && !edr_hid_is_connected()) {
    if (!edr_hid_is_connected()) {
        if (bt_connect_phone_back_start()) { //回连
            log_info("edr reconnect");
            return;
        }
    }
#endif
    switch (key_value) {
    case 0:
        if (REMOTE_IS_IOS()) {
            handle_send_packect((u8 *)key_up_before, sizeof(key_up_before), REPROT_INFO_LEN0);    //上滑
        } else {
            handle_send_packect((u8 *)key_up_click, sizeof(key_up_click), REPROT_INFO_LEN1);
        }
        break;

    case 1:
        if (REMOTE_IS_IOS()) {
            handle_send_packect((u8 *)key_down_before, sizeof(key_down_before), REPROT_INFO_LEN0); //下滑
        } else {
            handle_send_packect((u8 *)key_down_click, sizeof(key_down_click), REPROT_INFO_LEN1);
        }
        break;

    case 2:
        if (REMOTE_IS_IOS()) {
            handle_send_packect((u8 *)key_left_before, sizeof(key_left_before), REPROT_INFO_LEN0);  //左滑
        } else {
            handle_send_packect((u8 *)key_left_click, sizeof(key_left_click), REPROT_INFO_LEN1);
        }
        break;

    case 3:
        if (REMOTE_IS_IOS()) {
            handle_send_packect((u8 *)key_right_before, sizeof(key_right_before), REPROT_INFO_LEN0);   //右滑
        } else {
            handle_send_packect((u8 *)key_right_click, sizeof(key_right_click), REPROT_INFO_LEN1);
        }
        break;
    case 4: {
        u8 count = 2;
        while (count--) {
            if (REMOTE_IS_IOS()) {
                handle_send_packect((u8 *)key_pp_release_before, sizeof(key_pp_release_before), REPROT_INFO_LEN0);   //点赞
            } else {
                handle_send_packect((u8 *)key_pp_press, sizeof(key_pp_press), REPROT_INFO_LEN1);
                handle_send_packect((u8 *)key_pp_release, sizeof(key_pp_release), REPROT_INFO_LEN1);
            }
            DOUBLE_KEY_DELAY_TIME();
        }
        // if(REMOTE_IS_IOS()){
        //     bt_connect_reset_xy();
        // }
    }
    default:
        break;
    }

    keypage_coordinate_vm_deal(1);
}



u8 *usr_get_keypage_report_map(u8 flag, u16 *len)
{
    printf("func:%s , remote dev:%s ,", __func__, flag ? "Android" : "IOS");
    if (flag) {
        //安卓
        *len = sizeof(keypage_report_map);
        return (u8 *)keypage_report_map;
    } else {
        //IOS
        *len = sizeof(keypage_report_map_ios);
        return (u8 *)keypage_report_map_ios;
    }
}


void sbox_ctrl_tiktok(u8 cmd)
{
    g_printf("%s[cmd:%d, task:%s]", __func__, cmd, os_current_task());
    switch (cmd) {
    case 0:
        if (REMOTE_IS_IOS()) {
            handle_send_packect((u8 *)key_up_before, sizeof(key_up_before), REPROT_INFO_LEN0);    //上滑
        } else {
            handle_send_packect((u8 *)key_up_click, sizeof(key_up_click), REPROT_INFO_LEN1);
        }
        break;

    case 1:
        if (REMOTE_IS_IOS()) {
            handle_send_packect((u8 *)key_down_before, sizeof(key_down_before), REPROT_INFO_LEN0); //下滑
        } else {
            handle_send_packect((u8 *)key_down_click, sizeof(key_down_click), REPROT_INFO_LEN1);
        }
        break;

    case 2:
        if (REMOTE_IS_IOS()) {
            handle_send_packect((u8 *)key_left_before, sizeof(key_left_before), REPROT_INFO_LEN0);  //左滑
        } else {
            handle_send_packect((u8 *)key_left_click, sizeof(key_left_click), REPROT_INFO_LEN1);
        }
        break;

    case 3:
        if (REMOTE_IS_IOS()) {
            handle_send_packect((u8 *)key_right_before, sizeof(key_right_before), REPROT_INFO_LEN0);   //右滑
        } else {
            handle_send_packect((u8 *)key_right_click, sizeof(key_right_click), REPROT_INFO_LEN1);
        }
        break;
    case 4: {
        u8 count = 2;
        while (count--) {
            if (REMOTE_IS_IOS()) {
                handle_send_packect((u8 *)key_pp_release_before, sizeof(key_pp_release_before), REPROT_INFO_LEN0);   //点赞
            } else {
                handle_send_packect((u8 *)key_pp_press, sizeof(key_pp_press), REPROT_INFO_LEN1);
                handle_send_packect((u8 *)key_pp_release, sizeof(key_pp_release), REPROT_INFO_LEN1);
            }
            DOUBLE_KEY_DELAY_TIME();
        }
        // if(REMOTE_IS_IOS()){
        //     bt_connect_reset_xy();
        // }
        break;
    }
    //调整复位坐标
    case 5:
        save_coordinate_x_cnt = 0;
        save_coordinate_y_cnt = 0;
        key_send_reset_coordinate();
        break;
    case 6:
        key_coordinate_confirm();
        break;
    case 7:
        save_coordinate_x_cnt++;
        key_send_coordinate_x_right();
        break;
    case 8:
        save_coordinate_x_cnt--;
        key_send_coordinate_x_left();
        break;
    case 9:
        save_coordinate_y_cnt--;
        key_send_coordinate_y_up();
        break;
    case 10:
        save_coordinate_y_cnt++;
        key_send_coordinate_y_down();
        break;

    default:
        break;
    }

}

void sbox_ctrl_siri(u8 cmd)
{
    g_printf("%s[cmd:%d, task:%s]", __func__, cmd, os_current_task());

    switch (cmd) {
    case 0:
        bt_cmd_prepare(USER_CTRL_HFP_GET_SIRI_OPEN, 0, NULL);
        break;
    case 1:
        bt_cmd_prepare(USER_CTRL_HFP_GET_SIRI_CLOSE, 0, NULL);
        break;

    default:
        break;
    }
}

#endif
