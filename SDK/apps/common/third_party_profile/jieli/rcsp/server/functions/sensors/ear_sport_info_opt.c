#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".ear_sport_info_opt.data.bss")
#pragma data_seg(".ear_sport_info_opt.data")
#pragma const_seg(".ear_sport_info_opt.text.const")
#pragma code_seg(".ear_sport_info_opt.text")
#endif
#include "app_config.h"
#include "typedef.h"
#include "rcsp_define.h"
#include "ble_rcsp_server.h"
#include "sport_data_func.h"
#include "health_long_detect.h"
#include "ear_sport_info_opt.h"
#include "JL_rcsp_protocol.h"
#include "hrSensor_manage.h"

#include "timer.h"
#include "btstack/avctp_user.h"
#include "hx3011/hx3011_hrs_agc.h"
#include "hx3011/hx3011_spo2_agc.h"
#include "health_long_detect.h"
#if TCFG_USER_TWS_ENABLE
#include "bt_tws.h"
#endif

#if JL_RCSP_EAR_SENSORS_DATA_OPT && HEALTH_ALL_DAY_CHECK_ENABLE

#if 0
#define LOG_TAG             "[EAR_SENSOR]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#include "debug.h"
#else
#define log_info(fmt, ...)
#define log_error(fmt, ...)
#define log_debug(fmt, ...)
#endif

extern u8 health_all_day_detect_mode;
// A5 支持耳机推数/也支持app拿数，这里是因为耳机只用了心率全天检测的功能
enum {
    SPORTS_INFO_OPT_GET,
    SPORTS_INFO_OPT_SET,
    SPORTS_INFO_OPT_NOTIFY,
};

u8 resp[] = {0x03, 0x01, 0X00, 0x00};
bool poweron_first_read = 0;
static void ear_get_sport_info(void *priv, u8 OpCode, u8 OpCode_SN, u8 *data, u16 len)
{
    printf("ear_get_sport_info\n");
    log_info("Data: ");
    put_buf(data, len);

    resp[3] |= 0x04;        // bit2是心率传感器开关，耳机这里要常开。

    u8 health_all_day_detect_last_mode;  //
    int ret = syscfg_read(CFG_USER_APP_ALL_DAY_DETECT_MODE, &health_all_day_detect_last_mode, sizeof(health_all_day_detect_last_mode));
    printf("ret:%d, health_all_day_detect_last_mode:%d\n", ret, health_all_day_detect_last_mode);
    if (health_all_day_detect_last_mode) {
        resp[3] |= 0x08;        // bit3是心率传感器记录功能开关
    } else {
        resp[3] &= (~0x08);
    }
    JL_CMD_response_send(OpCode, JL_PRO_STATUS_SUCCESS, OpCode_SN, resp, sizeof(resp), 0, NULL);
}

static void ear_set_sport_info(void *priv, u8 OpCode, u8 OpCode_SN, u8 *data, u16 len)
{
    printf("ear_set_sport_info\n");
    log_info("Data: ");
    put_buf(data, len);
    resp[3] |= 0x04;        // bit2是心率传感器开关，要常开。
    if (data[3] & 0x08) {
        line_inf
        health_all_day_detect_switch(1);    // 开全天后台检测
        resp[3] |= 0x08;
    } else if (!(data[3] & 0x08)) {
        line_inf
        health_all_day_detect_switch(0);    // 关全天后台检测
        resp[3] &= (~0x08);
    }
#if TCFG_USER_TWS_ENABLE
    tws_sync_health_detect_work_status();
#endif
    JL_CMD_response_send(OpCode, JL_PRO_STATUS_SUCCESS, OpCode_SN, resp, sizeof(resp), 0, NULL);
}

int JL_rcsp_ear_sports_info_funciton(void *priv, u8 OpCode, u8 OpCode_SN, u8 *data, u16 len)
{
    line_inf
    int ret = -1;
    if (JL_OPCODE_SPORTS_DATA_INFO_OPT == OpCode) {
        u8 op = data[0];
        switch (op) {
        case SPORTS_INFO_OPT_GET:
            ear_get_sport_info(priv, OpCode, OpCode_SN, data + 1, len - 1);
            break;
        case SPORTS_INFO_OPT_SET:
            ear_set_sport_info(priv, OpCode, OpCode_SN, data + 1, len - 1);
            break;
        case SPORTS_INFO_OPT_NOTIFY:
            break;
        }
        ret = 0;
    }
    return ret;
}

#endif
