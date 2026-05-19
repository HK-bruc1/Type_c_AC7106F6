#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".charge_clibration.data.bss")
#pragma data_seg(".charge_clibration.data")
#pragma const_seg(".charge_clibration.text.const")
#pragma code_seg(".charge_clibration.text")
#endif
#include "init.h"
#include "app_config.h"
#include "system/includes.h"
#include "asm/charge.h"
#include "chargestore/chargestore.h"
#include "app_chargestore.h"
#include "app_charge_clibration.h"
#include "user_cfg.h"
#include "device/vm.h"
#include "app_action.h"
#include "app_main.h"

#define LOG_TAG_CONST       APP_CHARGE_CALIBRATION
#define LOG_TAG             "[APP_CHARGE_CALIBRATION]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

//-----------------------------------------------------------------------------------
//--------------------充电电流校准流程实现-------------------------------------------
//-----------------------------------------------------------------------------------

#if TCFG_CHARGE_CALIBRATION_ENABLE

#include "asm/charge_calibration.h"

#define CMD_CALIBRATION_SYNC            0 //询问是否在线
#define CMD_CALIBRATION_SET_CURRENT     1 //设置电流范围
#define CMD_CALIBRATION_REPORT_CURRENT  2 //报告当前电流大小
#define CMD_CALIBRATION_KEEP            3 //保持连接检测

struct charge_calibration_info {
    u32 max_current;
    u32 min_current;
    int reset_timer;
};

static struct charge_calibration_info info;
#define __this  (&info)

static void app_charge_calibration_reset_sys(void *priv)
{
    log_info("charge_calibration_reset_sys!\n");
    charge_system_reset();
}

int app_charge_calibration_event_handler(int *msg)
{
    u8 send_buf[3], report_ret;
    int ret;
    calibration_result result, result_tmp;

    if (__this->reset_timer == 0) {
        __this->reset_timer = sys_timer_add(NULL, app_charge_calibration_reset_sys, 2000);
    } else {
        sys_timer_modify(__this->reset_timer, 2000);
    }

    switch (msg[0]) {
    case CMD_CALIBRATION_SYNC:
        log_info("event_CMD_CALIBRATION_SYNC\n");
        memset((u8 *)&result_tmp, 0xff, sizeof(calibration_result));
        if (IS_CHARGE_EN()) {
            send_buf[0] = CMD_CALIBRATION_MODULE;
            send_buf[1] = CMD_CALIBRATION_SYNC;
            //检测有没有校准过
            ret = syscfg_read(VM_CHARGE_CALIBRATION, (void *)&result, sizeof(calibration_result));
            if (ret == sizeof(calibration_result)) {
                send_buf[2] = 1;
                log_info("already calibration!\n");
            } else {
                send_buf[2] = 0;
                log_info("not calibration!\n");
            }
            chargestore_api_write(send_buf, 3);
            //更新充电电流档位
            set_charge_mA(get_charge_mA_config());
        } else {
            send_buf[0] = CMD_CALIBRATION_MODULE;
            send_buf[1] = 0xff;
            chargestore_api_write(send_buf, 2);
        }
        break;
    case CMD_CALIBRATION_REPORT_CURRENT:
        log_info("event_CMD_CALI_REPORT_CURRENT\n");
        report_ret = charge_calibration_report_current(msg[1]);
        send_buf[0] = CMD_CALIBRATION_MODULE;
        send_buf[1] = CMD_CALIBRATION_REPORT_CURRENT;
        send_buf[2] = report_ret;
        chargestore_api_write(send_buf, 3);
        log_info("event_CMD_CALI_REPORT_CURRENT: %x\n", report_ret);
        if (report_ret == CALIBRATION_SUCC) {
            result = charge_calibration_get_result();
            syscfg_write(VM_CHARGE_CALIBRATION, (void *)&result, sizeof(calibration_result));
        }
        break;
    default:
        break;
    }
    return false;
}

APP_MSG_HANDLER(charge_clibration_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_CHARGE_CLIBRATION,
    .handler    = app_charge_calibration_event_handler,
};

static void charge_cali_event_to_user(u8 event, u32 value)
{
    int msg[2];
    msg[0] = event;
    msg[1] = value;
    os_taskq_post_type("app_core", MSG_FROM_CHARGE_CLIBRATION, 2, msg);
}

static int app_charge_calibration_data_handler(u8 *buf, u8 len)
{
    u32 value;
    u8 send_buf[2];
    if (buf[0] != CMD_CALIBRATION_MODULE) {
        //不是电流校准的指令,返回0
        return 0;
    }
    send_buf[0] = CMD_CALIBRATION_MODULE;
    send_buf[1] = buf[1];
    switch (buf[1]) {
    case CMD_CALIBRATION_SYNC:
        log_info("CMD_CALIBRATION_SYNC");
        charge_cali_event_to_user(CMD_CALIBRATION_SYNC, 0);
        chargestore_api_set_timeout(100);
        break;
    case CMD_CALIBRATION_SET_CURRENT:
        memcpy((u8 *)&__this->max_current, buf + 2, 4);
        memcpy((u8 *)&__this->min_current, buf + 6, 4);
        chargestore_api_write(send_buf, 2);
        charge_calibration_set_current_limit(__this->max_current, __this->min_current);
        log_info("CMD_CALI_SET_CURRENT, max: %d, min: %d", __this->max_current, __this->min_current);
        charge_cali_event_to_user(CMD_CALIBRATION_SET_CURRENT, 0);
        break;
    case CMD_CALIBRATION_REPORT_CURRENT:
        log_info("CMD_CALI_REPORT_CURRENT");
        memcpy((void *)&value, buf + 2, 4);
        charge_cali_event_to_user(CMD_CALIBRATION_REPORT_CURRENT, value);
        chargestore_api_set_timeout(100);
        break;
    case CMD_CALIBRATION_KEEP:
        log_info("CMD_CALIBRATION_KEEP");
        chargestore_api_write(send_buf, 2);
        charge_cali_event_to_user(CMD_CALIBRATION_KEEP, 0);
        break;
    }
    return 1;
}

CHARGESTORE_HANDLE_REG(charge_cali, app_charge_calibration_data_handler);

#endif //TCFG_CHARGE_CALIBRATION_ENABLE


