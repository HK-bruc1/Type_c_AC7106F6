#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".ear_sports_data_opt.data.bss")
#pragma data_seg(".ear_sports_data_opt.data")
#pragma const_seg(".ear_sports_data_opt.text.const")
#pragma code_seg(".ear_sports_data_opt.text")
#endif

#include "app_config.h"
#include "typedef.h"
#include "rcsp_define.h"
#include "ble_rcsp_server.h"
#include "sport_data_func.h"
#include "ear_sports_data_opt.h"
#include "JL_rcsp_protocol.h"
#include "gSensor/gSensor_manage.h"
#include "hr_sensor/hrSensor_manage.h"
#include "clock_manager/clock_manager.h"
#include "timer.h"
#include "btstack/avctp_user.h"
#include "hx3011/hx3011_hrs_agc.h"
#include "hx3011/hx3011_spo2_agc.h"
#include "health_long_detect.h"
#if TCFG_USER_TWS_ENABLE
#include "bt_tws.h"
#endif

#if JL_RCSP_EAR_SENSORS_DATA_OPT

#if 1
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

// 单次检测数据类型
#define EAR_SENSOR_TYPE_SINGLE_DETECT      0x09

// 超时时间（秒）
#define STEPS_DETECT_TIMEOUT 600  // 10分钟超时 (600秒)

// 检测状态，外部文件可以通过这些变量来判断是否正在检测
u8 hr_detect_status = 0;         // 心率检测的状态
u8 spo2_detect_status = 0;       // 血氧检测的状态
u8 steps_detect_status = 0;      // 计步检测的状态

// 定时器相关变量
static u32 hr_detect_timer_id = 0;      // 心率单次定时检测时间
static u32 spo2_detect_timer_id = 0;    // 血氧单次定时检测时间
static u32 steps_detect_timer = 0;      // 5分钟计数，用于判断5分钟内app有没有来拿数，没有就停止计步
static u32 steps_detect_timer_id = 0;   // 计步单次定时检测时间

static void *hr_detect_priv = NULL;
static u8 hr_detect_sn = 0;
static void *spo2_detect_priv = NULL;
static void *steps_detect_priv = NULL;

// 穿戴状态变化检测变量
static u8 last_wear_status = 0;
// 穿戴状态变化检测函数
void app_sensor_report_wear_status(void)
{
    // 获取当前穿戴状态
    u8 current_wear_status = hr_sensor_get_wear_status();

    // 检查穿戴状态是否发生变化
    if (current_wear_status != last_wear_status) {
        // printf("Wear status changed from %d to %d, triggering automatic report", last_wear_status, current_wear_status);

        // 调用外部接口主动上报穿戴状态变化
        ear_sports_push_wear_status_change(current_wear_status);
        // 更新上一次的穿戴状态
        last_wear_status = current_wear_status;
    }
}

#if TCFG_GSENSOR_ENABLE || TCFG_HRSENSOR_ENABLE
// 给"app_sensor"post信号量，定时把gSensor的数据写给hrSensor
u16 ear_sports_sensor_cb_timer_id = 0;
void ear_sports_sensor_del_data_check_cb()
{
    if (ear_sports_sensor_cb_timer_id != 0) {
        sys_timer_del(ear_sports_sensor_cb_timer_id);
        ear_sports_sensor_cb_timer_id = 0;
    }
}

static void ear_sports_sensor_cb(void *parm)
{
    static u8 wear_status_report_time;
    os_taskq_post_type("app_sensor", (Q_MSG + 1), 0, NULL);
    wear_status_report_time++;
#if TCFG_USER_TWS_ENABLE
    if (tws_api_get_role()) {
        return ;    // 从机不推数据
    }
#endif
    if (wear_status_report_time == 3) {
        wear_status_report_time = 0;
        app_sensor_report_wear_status();    // 实时监测960ms上传一次佩戴状态
    }
}
#endif  // TCFG_GSENSOR_ENABLE || TCFG_HRSENSOR_ENABLE

// 响应数据结构
static int ear_sports_data_send_response(void *priv, u8 OpCode_SN, u8 status, u8 result, u8 flag, u8 *data, u16 data_len)
{
    u8 response_buf[128];
    u16 response_len = 0;
    // 结果码
    response_buf[response_len++] = result;

    // 标志
    response_buf[response_len++] = flag;

    // 如果有数据，则拷贝数据
    if (data && data_len > 0) {
        memcpy(&response_buf[response_len], data, data_len);
        response_len += data_len;
    }

    // 发送响应
    return JL_CMD_response_send(JL_OPCODE_SPORTS_DATA_INFO_GET, JL_PRO_STATUS_SUCCESS, OpCode_SN, response_buf, response_len, 0, NULL);
}

// 推送数据到APP
static int ear_sports_data_push_data(void *priv, u8 OpCode_SN, u8 version, u8 *data, u16 data_len)
{
    u8 push_buf[128];
    u16 push_len = 0;

    // 版本号
    push_buf[push_len++] = version;

    // 分包数量 (固定为1)
    push_buf[push_len++] = 0x01;

    // 包ID (固定为0)
    push_buf[push_len++] = 0x00;

    // 如果有数据，则拷贝数据
    if (data && data_len > 0) {
        memcpy(&push_buf[push_len], data, data_len);
        push_len += data_len;
    }

    // 发送推送数据
    return JL_CMD_send(JL_OPCODE_SPORTS_DATA_INFO_AUTO_UPDATE, push_buf, push_len, JL_NOT_NEED_RESPOND, 0, NULL);
}

// 穿戴状态变化上报函数 - 外部主动调用
int ear_sports_push_wear_status_change(u8 wear_status)
{
    u8 push_data[7];
    u16 push_data_len = 0;

    // log_info("Pushing wear status change: %s", wear_status ? "wear" : "no wear");

    // 构建传感器状态
    u16 sensorState = 0;

    // bit0: 心率传感器状态 (0:空闲，1：检测中)
    if (hr_detect_status) {
        sensorState |= (1 << 0);
    }

    // bit1: 血氧传感器状态 (0:空闲，1：检测中)
    if (spo2_detect_status) {
        sensorState |= (1 << 1);
    }

    // bit2: 步数传感器状态 (0:空闲，1：检测中)
    if (steps_detect_status) {
        sensorState |= (1 << 2);
    }

    // bit3: 入耳检测传感器 (0:出耳状态，1：入耳状态)
    if (wear_status) {
        sensorState |= (1 << 3);
    } else {
        sensorState &= ~(1 << 3);
    }

    // log_info("Sensor state for wear status change: 0x%04X", sensorState);

    // Build push data according to protocol format
    // Format: [len(2 bytes)][type][sub_mask][data(2 bytes)]
    push_data[push_data_len++] = 0x00;  // len high byte (0005)
    push_data[push_data_len++] = 0x04;  // len low byte (5 bytes: type + sub_mask + sensorState)
    push_data[push_data_len++] = EAR_SENSOR_TYPE_SINGLE_DETECT;  // type: 0x09
    push_data[push_data_len++] = EAR_SENSOR_MASK_DEV_DETECT_RUN; // sub_mask: 0x00
    push_data[push_data_len++] = (sensorState >> 0) & 0xFF;  // sensorState low byte
    push_data[push_data_len++] = (sensorState >> 8) & 0xFF;  // sensorState high byte

    // log_info("Wear status change push data built, data length: %d", push_data_len);

    // Push data using A2 interface
    // SN=0x00, version=0x00, package count=1, package id=0
    return ear_sports_data_push_data(NULL, 0x00, 0x00, push_data, push_data_len);
}

#if TCFG_HR_CHECK_ENABLE
// 心率检测定时器回调函数
static void hr_detect_timer_callback(void *priv)
{
    if (!hr_detect_status) {
        return; // 检测已取消
    }
    if (!hr_sensor_get_wear_status()) {
        return ;    // 非佩戴状态，不推数据
    }
#if TCFG_USER_TWS_ENABLE
    if (tws_api_get_role()) {
        // hr_detect_status = 0;
        // ear_sports_sensor_del_data_check_cb();
        // line_inf
        // hr_sensor_measure_wear_start(0, 0);
        return ;    // 从机不推数据
    }
#endif
    // 尝试读取心率数据
    if (hr_sensor_get_heart_rate()) {
        // log_info("Heart rate detected: %d", hr_sensor_get_heart_rate());
        // 检测成功，发送成功推送
        ear_sports_push_hr_detect_success(hr_detect_priv, hr_detect_sn);
    }
}

// 处理开始检测心率命令
static int ear_sports_handle_hr_detect_start(void *priv, u8 OpCode_SN, u8 *data, u16 len)
{
    clock_refurbish();
    log_info("ear_sports_handle_hr_detect_start");

    // Start heart rate detection
    if (hr_sensor_measure_hr_start(0, 0) == 0) {
        if (!ear_sports_sensor_cb_timer_id) {
            ear_sports_sensor_cb_timer_id = sys_timer_add(NULL, ear_sports_sensor_cb, 320);
        }
        hr_detect_status = 1;
        // 启动定时器开始轮询心率数据
        hr_detect_timer_id = sys_timer_add(NULL, hr_detect_timer_callback, 1000);
#if TCFG_USER_TWS_ENABLE
        tws_sync_health_detect_work_status();
#endif
        // log_info("Heart rate detection start succ");
        return ear_sports_data_send_response(priv, OpCode_SN, 0x00, 0x00, 0x00, NULL, 0);
    } else {
        // log_error("Heart rate detection start failed");
        return ear_sports_data_send_response(priv, OpCode_SN, 0x00, 0x01, 0x00, NULL, 0);
    }
}

// 处理取消检测心率命令
static int ear_sports_handle_hr_detect_cancel(void *priv, u8 OpCode_SN, u8 *data, u16 len)
{
    // log_info("ear_sports_handle_hr_detect_cancel");
    // Stop heart rate detection
    if (hr_detect_timer_id) {
        sys_timer_del(hr_detect_timer_id);
        hr_detect_timer_id = 0;
    }
    if (hr_sensor_measure_hr_stop() == 0) {
        hr_detect_status = 0;
        // log_info("Heart rate detection canceled successfully");
        ear_sports_sensor_del_data_check_cb();
#if TCFG_USER_TWS_ENABLE
        tws_sync_health_detect_work_status();
#endif
// #if TCFG_USER_TWS_ENABLE
//         if (tws_api_get_role()) {
//             return 0;
//         }
// #endif
        hr_sensor_measure_wear_start(0, 0);
        return ear_sports_data_send_response(priv, OpCode_SN, 0x00, 0x00, 0x00, NULL, 0);
    } else {
        // log_error("Heart rate detection cancel failed");
        // Send failure response with result=0x01
        // hr_sensor_measure_wear_start(0, 0);
        return ear_sports_data_send_response(priv, OpCode_SN, 0x00, 0x01, 0x00, NULL, 0);
    }
}

// 推送心率检测成功
int ear_sports_push_hr_detect_success(void *priv, u8 OpCode_SN)
{
    u8 push_data[7];
    u16 push_data_len = 0;
    if (hr_detect_status) {
        // 获取实际的心率数据
        log_info("ear_sports_push_hr_detect_success, heart rate value: %d", hr_sensor_get_heart_rate());
        // Build push data according to A2 protocol format
        // Format: [len(2 bytes)][type][sub_mask][data(1 byte)]
        push_data[push_data_len++] = 0x00;  // len high byte (0003)
        push_data[push_data_len++] = 0x03;  // len low byte (3 bytes: type + sub_mask + heart_rate)
        push_data[push_data_len++] = 0x00;  // type: 0x00
        push_data[push_data_len++] = 0x01;  // sub_mask: 0x01
        push_data[push_data_len++] = hr_sensor_get_heart_rate();  // 使用实际获取的心率值实时值
        log_info("Push data built successfully, data length: %d", push_data_len);
        // Push data using A2 interface
        // SN=0x00, version=0x00, package count=1, package id=0
        return ear_sports_data_push_data(priv, OpCode_SN, 0x00, push_data, push_data_len);
    } else {
        log_error("Heart rate detection status abnormal, cannot push success result");
    }
    return -1;
}
#if TCFG_USER_TWS_ENABLE
// 主耳通知开启心率检测
void tws_sibling_hr_start(void)
{
    line_inf
    printf("hr_detect_status : %d\n", hr_detect_status);
    if (!hr_detect_status) {
        clock_refurbish();
        hr_sensor_measure_hr_start(0, 0);
        if (!ear_sports_sensor_cb_timer_id) {
            ear_sports_sensor_cb_timer_id = sys_timer_add(NULL, ear_sports_sensor_cb, 320);
        }
        hr_detect_status = 1;
        // 启动定时器开始轮询心率数据
        hr_detect_timer_id = sys_timer_add(NULL, hr_detect_timer_callback, 1000);
    }
}
// 主耳通知停止心率检测
void tws_sibling_hr_stop(void)
{
    line_inf
    printf("hr_detect_status : %d\n", hr_detect_status);
    if (hr_detect_status) {
        if (hr_detect_timer_id) {
            sys_timer_del(hr_detect_timer_id);
            hr_detect_timer_id = 0;
        }
        hr_sensor_measure_hr_stop();
        hr_detect_status = 0;
        ear_sports_sensor_del_data_check_cb();
        hr_sensor_measure_wear_start(0, 0);
    }
}
#endif
#endif // TCFG_HR_CHECK_ENABLE

#if TCFG_SPO2_CHECK_ENABLE
// 血氧检测定时器回调函数
static void spo2_detect_timer_callback(void *priv)
{
    if (!spo2_detect_status) {
        return; // 检测已取消
    }
    if (!hr_sensor_get_wear_status()) {
        return ;    // 非佩戴状态，不推数据
    }
#if TCFG_USER_TWS_ENABLE
    if (tws_api_get_role()) {
        // spo2_detect_status = 0;
        // ear_sports_sensor_del_data_check_cb();
        // line_inf
        // hr_sensor_measure_wear_start(0, 0);
        return ;    // 从机不推数据
    }
#endif
    // 尝试读取血氧数据
    if (hr_sensor_get_spo2()) {
        log_info("SpO2 detected: %d", hr_sensor_get_spo2());
        // 检测成功，发送成功推送
        ear_sports_push_spo2_detect_success(spo2_detect_priv);
    }

}

// 处理开始检测血氧命令
static int ear_sports_handle_spo2_detect_start(void *priv, u8 OpCode_SN, u8 *data, u16 len)
{
    clock_refurbish();
    log_info("ear_sports_handle_spo2_detect_start");

    // Start SpO2 detection
    if (hr_sensor_measure_spo2_start(0, 0) == 0) {
        if (!ear_sports_sensor_cb_timer_id) {
            ear_sports_sensor_cb_timer_id = sys_timer_add(NULL, ear_sports_sensor_cb, 320);
        }
        spo2_detect_status = 1;
        // 启动定时器开始轮询血氧数据
        spo2_detect_timer_id = sys_timer_add(NULL, spo2_detect_timer_callback, 1000);
#if TCFG_USER_TWS_ENABLE
        tws_sync_health_detect_work_status();
#endif
        // log_info("SpO2 detection timer started, will poll data every second");
        return ear_sports_data_send_response(priv, OpCode_SN, 0x00, 0x00, 0x00, NULL, 0);
    } else {
        // log_error("SpO2 detection start failed");
        return ear_sports_data_send_response(priv, OpCode_SN, 0x00, 0x01, 0x00, NULL, 0);
    }
}

// 处理取消检测血氧命令
static int ear_sports_handle_spo2_detect_cancel(void *priv, u8 OpCode_SN, u8 *data, u16 len)
{
    log_info("ear_sports_handle_spo2_detect_cancel");
    // Stop SpO2 detection

    if (spo2_detect_timer_id) {
        sys_timer_del(spo2_detect_timer_id);
        spo2_detect_timer_id = 0;
    }
    if (hr_sensor_measure_spo2_stop() == 0) {
        spo2_detect_status = 0;
        log_info("SpO2 detection canceled successfully");
        // Send success response
        // Format: [status=0x00][sn][result=0x00][flag=0x00]
        ear_sports_sensor_del_data_check_cb();
#if TCFG_USER_TWS_ENABLE
        tws_sync_health_detect_work_status();
#endif
        // clock_free("app_sensor");
// #if TCFG_USER_TWS_ENABLE
//         if (tws_api_get_role()) {
//             return 0;
//         }
// #endif
        hr_sensor_measure_wear_start(0, 0);
        return ear_sports_data_send_response(priv, OpCode_SN, 0x00, 0x00, 0x00, NULL, 0);
    } else {
        log_error("SpO2 detection cancel failed");
        // Send failure response with result=0x01

        // hr_sensor_measure_wear_start(0, 0);
        return ear_sports_data_send_response(priv, OpCode_SN, 0x00, 0x01, 0x00, NULL, 0);
    }
}


// 推送血氧检测成功
int ear_sports_push_spo2_detect_success(void *priv)
{
    u8 push_data[7];
    u16 push_data_len = 0;

    if (spo2_detect_status) {
        // 获取实际的血氧数据
        log_info("ear_sports_push_spo2_detect_success, SpO2 value: %d", hr_sensor_get_spo2());
        // Build push data according to A2 protocol format
        // Format: [len(2 bytes)][type][sub_mask][data(1 byte)]
        push_data[push_data_len++] = 0x00;  // len high byte (0003)
        push_data[push_data_len++] = 0x03;  // len low byte (3 bytes: type + sub_mask + spo2_level)
        push_data[push_data_len++] = 0x05;  // type: 0x09
        push_data[push_data_len++] = 0x01; // sub_mask: 0x07
        push_data[push_data_len++] = hr_sensor_get_spo2();    // 使用实际获取的血氧值实时值
        log_info("Push data built successfully, data length: %d", push_data_len);
        // Push data using A2 interface
        // SN=0x00, version=0x00, package count=1, package id=0
        return ear_sports_data_push_data(priv, 0x00, 0x00, push_data, push_data_len);
    } else {
        log_error("SpO2 detection status abnormal, cannot push success result");
    }

    return -1;
}
#if TCFG_USER_TWS_ENABLE
// 主耳通知开启血氧检测
void tws_sibling_spo2_start(void)
{
    line_inf
    printf("spo2_detect_status : %d\n", spo2_detect_status);
    if (!spo2_detect_status) {
        clock_refurbish();
        hr_sensor_measure_spo2_start(0, 0);
        if (!ear_sports_sensor_cb_timer_id) {
            ear_sports_sensor_cb_timer_id = sys_timer_add(NULL, ear_sports_sensor_cb, 320);
        }
        spo2_detect_status = 1;
        // 启动定时器开始轮询血氧数据
        spo2_detect_timer_id = sys_timer_add(NULL, spo2_detect_timer_callback, 1000);
    }
}
// 主耳通知停止血氧检测
void tws_sibling_spo2_stop(void)
{
    line_inf
    printf("spo2_detect_status : %d\n", spo2_detect_status);
    if (spo2_detect_status) {
        if (spo2_detect_timer_id) {
            sys_timer_del(spo2_detect_timer_id);
            spo2_detect_timer_id = 0;
        }
        hr_sensor_measure_spo2_stop();
        spo2_detect_status = 0;
        ear_sports_sensor_del_data_check_cb();
        hr_sensor_measure_wear_start(0, 0);
    }
}
#endif
#endif // TCFG_SPO2_CHECK_ENABLE

#if TCFG_STEP_COUNT_ENABLE
// 步数检测定时器回调函数，作用：app5分钟内不拿数据，认为app被杀死，停止计步
static void steps_detect_timer_callback(void *priv)
{
    if (!steps_detect_status) {
        return; // 检测已取消
    }

    // 检查定时器是否超时
    if (steps_detect_timer > 0) {
        steps_detect_timer--;

        log_info("Steps detection running, remaining time: %d seconds", steps_detect_timer);

        // 设置下一次定时器检查（1秒后）
        steps_detect_timer_id = sys_timeout_add(NULL, steps_detect_timer_callback, 1000);
    } else {
        // 5分钟超时，APP未获取数据，取消步数监测
        log_error("Steps detection timeout - APP did not retrieve data within 5 minutes");

        // 停止步数监测
#if TCFG_USER_TWS_ENABLE
        tws_sibling_steps_detect_stop();
#endif
        steps_detect_status = 0;
        steps_detect_timer = 0;
        if ((!hr_detect_status) && (!spo2_detect_status)) {     // 实时健康检测中，不用停post（给app_sensor）
            if (ear_sports_sensor_cb_timer_id != 0) {
                sys_timer_del(ear_sports_sensor_cb_timer_id);
                ear_sports_sensor_cb_timer_id = 0;
            }
        }
        // 删除步数检测定时器
        if (steps_detect_timer_id != 0) {
            sys_timer_del(steps_detect_timer_id);
            log_info("Steps detection timer deleted due to timeout, timer_id: %lu", steps_detect_timer_id);
            steps_detect_timer_id = 0;
        }

        // 步数数据和算法输出数据归零
        g_sensor_clear_steps();
        g_sensor_clear_algo_out();

        log_info("Steps detection stopped due to timeout, step count and algo data reset to zero");
    }
}

// 处理开始检测步数命令
static int ear_sports_handle_steps_detect_start(void *priv, u8 OpCode_SN, u8 *data, u16 len)
{
    log_info("ear_sports_handle_steps_detect_start");

    clock_refurbish();
    // 先清除步数和算法输出数据
    g_sensor_clear_steps();
    g_sensor_clear_algo_out();
    log_info("Step count and algo data cleared before starting detection");

    // 设置步数检测状态
    steps_detect_status = 1;
    steps_detect_timer = STEPS_DETECT_TIMEOUT;
    steps_detect_priv = priv;

    log_info("Steps detection started successfully, timeout: %d seconds", STEPS_DETECT_TIMEOUT);

    if (!ear_sports_sensor_cb_timer_id) {
        ear_sports_sensor_cb_timer_id = sys_timer_add(NULL, ear_sports_sensor_cb, 320);
    }
    // 启动步数检测定时器
    if (!steps_detect_timer_id) {
        steps_detect_timer_id = sys_timeout_add(NULL, steps_detect_timer_callback, 1000);
    }
    log_info("Steps detection timer started, timer_id: %lu, will check timeout every second", steps_detect_timer_id);

    // 发送响应: cmd:0xA0, result:0x00, flag:0x00
    return ear_sports_data_send_response(priv, OpCode_SN, 0x00, 0x00, 0x00, NULL, 0);
}

// 处理停止检测步数命令
static int ear_sports_handle_steps_detect_stop(void *priv, u8 OpCode_SN, u8 *data, u16 len)
{
    log_info("ear_sports_handle_steps_detect_stop");
#if TCFG_USER_TWS_ENABLE
    tws_sibling_steps_detect_stop();
#endif
    // 停止步数检测
    steps_detect_status = 0;
    steps_detect_timer = 0;

    // 删除步数检测定时器
    if (steps_detect_timer_id != 0) {
        sys_timer_del(steps_detect_timer_id);
        log_info("Steps detection timer deleted, timer_id: %lu", steps_detect_timer_id);
        steps_detect_timer_id = 0;
    }
    // ear_sports_sensor_del_data_check_cb();
    log_info("Steps detection stopped successfully");
    ear_sports_sensor_del_data_check_cb();

    // 发送响应: cmd:0xA0, result:0x00, flag:0x00
    return ear_sports_data_send_response(priv, OpCode_SN, 0x00, 0x00, 0x00, NULL, 0);
}

// 处理步数数据查询
static int ear_sports_handle_steps_data_query(void *priv, u8 OpCode_SN, u8 *data, u16 len)
{
#if TCFG_USER_TWS_ENABLE
    if (tws_api_get_role()) {
        return -1;    // 从机不推数据
    }
#endif
    // if (!ear_sports_sensor_cb_timer_id) {    // 优化：主机进仓，从机继续计步
    //     g_sensor_clear_steps();
    //     g_sensor_clear_algo_out();
    //     // 设置步数检测状态
    //     steps_detect_status = 1;
    //     steps_detect_timer = STEPS_DETECT_TIMEOUT;
    //     ear_sports_sensor_cb_timer_id = sys_timer_add(NULL, ear_sports_sensor_cb, 320);
    //     steps_detect_timer_id = sys_timeout_add(NULL, steps_detect_timer_callback, 1000);
    // }
    if (!steps_detect_timer_id) {       // 主从切换后，新的从机没有app被杀死的检测
        steps_detect_timer = STEPS_DETECT_TIMEOUT;
        steps_detect_timer_id = sys_timeout_add(NULL, steps_detect_timer_callback, 1000);
    }

    u8 response_data[12];
    u16 response_data_len = 0;

    log_info("ear_sports_handle_steps_data_query");
    // 获取步数数据
    u32 current_steps = g_sensor_get_total_steps();

    // 使用算法输出结构体中的实际数据
    algo_out current_algo_out = g_sensor_get_algo_out();

    // 注意: 距离单位是cm，需要转换为米
    u16 distance = (u16)(current_algo_out.distance / 100);  // 距离 (米)
    u16 calories = (u16)current_algo_out.calories;          // 热量 (卡路里)

    log_info("Current steps: %lu, distance: %u meters, calories: %u", current_steps, distance, calories);
    log_info("Algo out - steps: %d, distance: %d cm, calories: %d", current_algo_out.steps, current_algo_out.distance, current_algo_out.calories);

    // 构建响应数据
    // cmd:0xA0, result:0x00, flag:0x01, Len:0x0a, type:0x03, sub_mask:0x07
    // data: 0xnn 0xnn 0xnn 0xnn(实时步数) 0xnn 0xnn(距离) 0xnn 0xnn(热量)

    //模拟数据
    /* current_steps = 66; */
    /* distance = 23; */
    /* calories = 31; */

    response_data[response_data_len++] = 0x00;  // len high byte (000a)
    response_data[response_data_len++] = 0x0a;  // len low byte  (10 bytes: type + sub_mask + 6byte data)
    response_data[response_data_len++] = 0x03;  // type: 0x03
    response_data[response_data_len++] = 0x07; // sub_mask: 0x07

    // 实时步数 (4 bytes, little endian)
    response_data[response_data_len++] = (current_steps >> 24) & 0xFF;
    response_data[response_data_len++] = (current_steps >> 16) & 0xFF;
    response_data[response_data_len++] = (current_steps >> 8) & 0xFF;
    response_data[response_data_len++] = (current_steps >> 0) & 0xFF;

    // 距离 (2 bytes, little endian)
    response_data[response_data_len++] = (distance >> 8) & 0xFF;
    response_data[response_data_len++] = (distance >> 0) & 0xFF;

    // 热量 (2 bytes, little endian)
    response_data[response_data_len++] = (calories >> 8) & 0xFF;
    response_data[response_data_len++] = (calories >> 0) & 0xFF;

    log_info("Steps data response built, data length: %d", response_data_len);

    // 重置定时器，因为APP获取了数据，重新开始5分钟计时
    // if (steps_detect_status) {
    steps_detect_timer = STEPS_DETECT_TIMEOUT;
    log_info("Steps detection timer reset to %d seconds", STEPS_DETECT_TIMEOUT);

    // 清除步数和算法输出数据重新开始计数
    g_sensor_clear_steps();
    g_sensor_clear_algo_out();
    log_info("Step count and algo data cleared for next 5-minute period");
    // }

    // 发送响应: cmd:0xA0, result:0x00, flag:0x01
    return ear_sports_data_send_response(priv, OpCode_SN, 0x00, 0x00, 0x01, response_data, response_data_len);
}

#if TCFG_USER_TWS_ENABLE
// 主耳通知开启计步
void tws_sibling_steps_detect_start(void)
{
    line_inf
    printf("tws_sibling_steps_counting_start : %d\n", steps_detect_status);
    if (!steps_detect_status) {
        clock_refurbish();
        // 先清除步数和算法输出数据
        g_sensor_clear_steps();
        g_sensor_clear_algo_out();
        steps_detect_status = 1;
        if (!ear_sports_sensor_cb_timer_id) {
            ear_sports_sensor_cb_timer_id = sys_timer_add(NULL, ear_sports_sensor_cb, 320);
        }
        steps_detect_timer_id = sys_timeout_add(NULL, steps_detect_timer_callback, 1000);
    }
}
// 主耳通知停止计步
void tws_sibling_steps_detect_stop(void)
{
    line_inf
    if (steps_detect_status) {
        steps_detect_status = 0;
        steps_detect_timer = 0;
    }
    // 删除步数检测定时器
    if (steps_detect_timer_id != 0) {
        sys_timer_del(steps_detect_timer_id);
        steps_detect_timer_id = 0;
    }
    ear_sports_sensor_del_data_check_cb();
}
#endif

#endif // TCFG_STEP_COUNT_ENABLE

// 处理设备检测状态查询
static int ear_sports_handle_dev_detect(void *priv, u8 OpCode_SN, u8 *data, u16 len)
{
    u8 response_data[6];
    u16 response_data_len = 0;

    log_info("ear_sports_handle_dev_detect");

    // 构建传感器状态
    u16 sensorState = 0;

#if TCFG_HR_CHECK_ENABLE
    // bit0: 心率传感器状态 (0:空闲，1：检测中)
    if (hr_detect_status) {
        sensorState |= (1 << 0);
    }
#endif	// TCFG_HR_CHECK_ENABLE

#if TCFG_SPO2_CHECK_ENABLE
    // bit1: 血氧传感器状态 (0:空闲，1：检测中)
    if (spo2_detect_status) {
        sensorState |= (1 << 1);
    }

#endif  // TCFG_SPO2_CHECK_ENABLE

#if TCFG_STEP_COUNT_ENABLE
    // bit2: 步数传感器状态 (0:空闲，1：检测中)
    if (steps_detect_status) {
        sensorState |= (1 << 2);
    }
#endif  // TCFG_STEP_COUNT_ENABLE

#if TCFG_HRSENSOR_ENABLE
    // bit3: 入耳检测传感器 (0:出耳状态，1：入耳状态)
    u8 wear_status = hr_sensor_get_wear_status();
    printf(">wear_status:%d\n", wear_status);  //
    if (wear_status) {
        sensorState |= (1 << 3);
    } else {
        sensorState &= (0 << 3);
    }
#endif  // TCFG_HRSENSOR_ENABLE

    log_info("Sensor state: HR=%d, SpO2=%d, Steps=%d, sensorState=0x%04X",
             hr_detect_status, spo2_detect_status, steps_detect_status, sensorState);

    // 构建响应数据
    // 格式: Len(2Bytes)+Type(1Byte)+sub_mask(1Byte)+sensorState(2Bytes)

    // Len: 4字节 (Type + sub_mask + sensorState)
    response_data[response_data_len++] = 0x00;  // Len高字节
    response_data[response_data_len++] = 0x04;  // Len低字节 (4 bytes)

    // Type: 单次检测类型
    response_data[response_data_len++] = EAR_SENSOR_TYPE_SINGLE_DETECT;  // 0x09

    // sub_mask: 设备检测
    response_data[response_data_len++] = EAR_SENSOR_MASK_DEV_DETECT_RUN;  // 0x00

    // sensorState: 2字节 (little endian)
    response_data[response_data_len++] = (sensorState >> 0) & 0xFF;  // 低字节
    response_data[response_data_len++] = (sensorState >> 8) & 0xFF;  // 高字节

    log_info("Device detect response built, data length: %d", response_data_len);

    // 发送响应
    return ear_sports_data_send_response(priv, OpCode_SN, 0x00, 0x00, 0x01, response_data, response_data_len);
}



#if HEALTH_ALL_DAY_CHECK_ENABLE

#define PACKAGE_TO_APP_NUMBERS 256  // 给app发的包大小

// 全天检测，rcsp发包处理
static int ear_sports_handle_hr_data_report(u8 *all_hr_data, u16 all_hr_data_len)
{
    int ret = 0;
    u8 push_buf[PACKAGE_TO_APP_NUMBERS + 3] = {0};
    u16 push_len = 0;
    u16 curr_pack_len;

    u16 push_buf_cnt = all_hr_data_len / PACKAGE_TO_APP_NUMBERS + 1;
    printf("push_buf_cnt %d", push_buf_cnt);
    for (u8 i = 0; push_buf_cnt > i; i++) {
        push_buf[push_len++] = 0x00;            // 版本号
        push_buf[push_len++] = push_buf_cnt;    // 分包数量
        push_buf[push_len++] = i;               // 包ID

        curr_pack_len = (push_buf_cnt == i + 1) ? (all_hr_data_len % PACKAGE_TO_APP_NUMBERS) : PACKAGE_TO_APP_NUMBERS;
        log_info("curr_pack_len=%d\n", curr_pack_len);
        // put_buf(all_hr_data + (i * PACKAGE_TO_APP_NUMBERS), curr_pack_len);

        memcpy(&push_buf[push_len], all_hr_data + (i * PACKAGE_TO_APP_NUMBERS), curr_pack_len);
        push_len += curr_pack_len;
        // put_buf(all_hr_data + (i * PACKAGE_TO_APP_NUMBERS), curr_pack_len);
        // put_buf(push_buf, PACKAGE_TO_APP_NUMBERS + 3);

        ret = JL_CMD_send(JL_OPCODE_SPORTS_DATA_INFO_AUTO_UPDATE, push_buf, push_len, JL_NOT_NEED_RESPOND, 0, NULL);
        push_len = 0;
    }
    free(all_hr_data);
    return ret;
}

int ear_sports_handle_hr_data_send()
{
    int ret = 0;
    u8 *all_hr_data = NULL;
    u16 all_hr_data_len;
    all_hr_data_len = ear_sports_handle_hr_data_pack(&all_hr_data);
    log_info("ear_sports_handle_hr_data_pack,len:%d\n", all_hr_data_len);
    ret = ear_sports_handle_hr_data_report(all_hr_data, all_hr_data_len);
    return ret;
}

// 全天检测，耳机固件给app推数据
static int ear_sports_handle_hr_data_report_response(void *priv, u8 OpCode_SN, u8 *data, u16 len)
{
    int ret = 0;
    ear_sports_data_send_response(priv, OpCode_SN, 0x00, 0x00, 0x00, NULL, 0);
#if TCFG_USER_TWS_ENABLE
    master_get_all_day_data();
    // TWS耳机这里先不发,等10s，10s后再根据是否用从机数据一起发
    return ret;
#endif
    ret = ear_sports_handle_hr_data_send();    // 头戴式没有从机，直接发
    return ret;
}

#endif

// 主处理函数
int JL_rcsp_ear_sports_data_funciton(void *priv, u8 OpCode, u8 OpCode_SN, u8 *data, u16 len)
{
    u8 version;
    u8 sub_mask = 0;
    u32 type_field;
#if TCFG_USER_TWS_ENABLE
    static u8 sensor_tws_role;
    static u8 sibling_wear_status;
    sensor_tws_role = tws_api_get_role();
    sibling_wear_status = bt_tws_get_sibling_wear_status();
    printf(">sensor_tws_role:%d, wear_status:%d, sibling_wear_status:%d\n", sensor_tws_role, hr_sensor_get_wear_status(), bt_tws_get_sibling_wear_status());
    if (tws_api_get_role()) {
        return -1;    // 从机不推数据
    }
    if (sub_mask == EAR_SENSOR_MASK_HR_DETECT_START ||
        sub_mask == EAR_SENSOR_MASK_SPO2_DETECT_START ||
        sub_mask == EAR_SENSOR_MASK_HR_DATA_REPORT) {
        tws_api_auto_role_switch_disable();
        log_info("TWS role switching disabled for detection start command");
    } else if (sub_mask == EAR_SENSOR_MASK_HR_DETECT_CANCEL ||
               sub_mask == EAR_SENSOR_MASK_SPO2_DETECT_CANCEL ||
               sub_mask == EAR_SENSOR_MASK_HR_DATA_CLEAR) {
        tws_api_auto_role_switch_enable();
        log_info("TWS role switching enabled for detection cancel command");
    }
#endif
    log_info("=== EAR SENSOR DATA FUNCTION CALLED ===");
    log_info("OpCode: 0x%02X, OpCode_SN: 0x%02X, len: %d", OpCode, OpCode_SN, len);

    if (len > 0) {
        log_info("Data: ");
        put_buf(data, len);
    }

    // Check parameter length - minimum 6 bytes for version + sub_mask + type field
    if (len < 6) {
        log_error("Insufficient parameter length: %d < 6", len);
        return ear_sports_data_send_response(priv, OpCode_SN, 0, 1, 0x00, NULL, 0);
    }

    // Parse parameters according to protocol structure
    // Data format: [type_field(4 bytes)][version][sub_mask][...]
    type_field = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    version = data[4];
    sub_mask = data[5];

    log_info("Type field: 0x%08X, Version: 0x%02X, sub_mask: 0x%02X", type_field, version, sub_mask);

    // 检查是否为步数数据查询 (mask type_field 第3 bit: 0x00 0x00 0x00 0x08)
    // 步数查询是特殊命令，不需要bit 8设置，应该优先处理
    if (type_field & (1 << 3)) {
        log_info("Processing steps data query (type_field bit 3 set)");
        return ear_sports_handle_steps_data_query(priv, OpCode_SN, data, len);
    }

    // Check if the 9th bit (bit 8) of type_field is set for single detect type
    if (!(type_field & (1 << 8))) {
        log_error("Invalid TYPE field: 0x%08X, expected bit 8 to be set for single detect", type_field);
        return ear_sports_data_send_response(priv, OpCode_SN, 0, 1, 0x00, NULL, 0);
    }

    if (OpCode != 0xA0) {
        return ear_sports_data_send_response(priv, OpCode_SN, 0, 1, 0x00, NULL, 0);
    }
    // Handle different commands based on sub_mask
    switch (sub_mask) {

    case EAR_SENSOR_MASK_DEV_DETECT_RUN:
        log_info("EAR_SENSOR_MASK_DEV_DETECT_RUN");
        return ear_sports_handle_dev_detect(priv, OpCode_SN, data, len);            // 处理设备检测状态查询
#if TCFG_HR_CHECK_ENABLE
    case EAR_SENSOR_MASK_HR_DETECT_START:
        log_info("EAR_SENSOR_MASK_HR_DETECT_START");
        return ear_sports_handle_hr_detect_start(priv, OpCode_SN, data, len);        // 处理开始检测心率命令
    case EAR_SENSOR_MASK_HR_DETECT_CANCEL:
        log_info("EAR_SENSOR_MASK_HR_DETECT_CANCEL");
        return ear_sports_handle_hr_detect_cancel(priv, OpCode_SN, data, len);      // 处理取消检测心率命令
#endif  // TCFG_HR_CHECK_ENABLE

#if TCFG_SPO2_CHECK_ENABLE
    case EAR_SENSOR_MASK_SPO2_DETECT_START:
        log_info("EAR_SENSOR_MASK_SPO2_DETECT_START");
        return ear_sports_handle_spo2_detect_start(priv, OpCode_SN, data, len);     // 处理开始检测血氧命令
    case EAR_SENSOR_MASK_SPO2_DETECT_CANCEL:
        log_info("EAR_SENSOR_MASK_SPO2_DETECT_CANCEL");
        return ear_sports_handle_spo2_detect_cancel(priv, OpCode_SN, data, len);    // 处理取消检测血氧命令
#endif  // TCFG_SPO2_CHECK_ENABLE

#if TCFG_STEP_COUNT_ENABLE
    case EAR_SENSOR_MASK_STEPS_DETECT_START:
        log_info("EAR_SENSOR_MASK_STEPS_DETECT_START");
        return ear_sports_handle_steps_detect_start(priv, OpCode_SN, data, len);    // 处理开始检测步数命令
    case EAR_SENSOR_MASK_STEPS_DETECT_STOP:
        log_info("EAR_SENSOR_MASK_STEPS_DETECT_STOP");
        return ear_sports_handle_steps_detect_stop(priv, OpCode_SN, data, len);      // 处理停止检测步数命令
#endif  // TCFG_STEP_COUNT_ENABLE

#if HEALTH_ALL_DAY_CHECK_ENABLE
    case EAR_SENSOR_MASK_HR_DATA_REPORT:
        log_info("EAR_SENSOR_MASK_HR_DATA_REPORT");
        return ear_sports_handle_hr_data_report_response(priv, OpCode_SN, data, len);        // 获取全天检测数据
    case EAR_SENSOR_MASK_HR_DATA_CLEAR:
        log_info("EAR_SENSOR_MASK_HR_DATA_CLEAR");
        background_check_clear_data();                                              // 清除后台检测数据
        return ear_sports_data_send_response(priv, OpCode_SN, 0, 0, 0x00, NULL, 0);
#endif  // HEALTH_ALL_DAY_CHECK_ENABLE
    default:
        log_error("Unknown sub_mask command: 0x%02X", sub_mask);
        // Unknown command
        return ear_sports_data_send_response(priv, OpCode_SN, 0, 1, 0x00, NULL, 0);
    }
}

#if TCFG_USER_TWS_ENABLE
// 这个佩戴检测功能是基于HX3011这个传感器芯片实现的。
u8 sibling_health_check_wear_status;
// 获取对耳的佩戴状态
int bt_tws_get_sibling_wear_status(void)
{
    return sibling_health_check_wear_status;
}

static void set_tws_sibling_health_check_wear_status(void *_data, u16 len, bool rx)
{
    u8 *data = (u8 *)_data;
    if (rx) {
        sibling_health_check_wear_status = data[0];
    }
}

REGISTER_TWS_FUNC_STUB(wear_status_sync_stub) = {
    .func_id = TWS_FUNC_ID_WEAR_STATE,
    .func    = set_tws_sibling_health_check_wear_status,
};

void tws_sync_health_check_wear_status(void)
{
    u8 data[1];
    data[0] = hr_sensor_get_wear_status();
    tws_api_send_data_to_sibling(data, 1, TWS_FUNC_ID_WEAR_STATE);
    printf("tws_sync_health_check_wear_status: %d\n", data[0]);
}

extern u8 hr_detect_status;     // 心率检测的状态
extern u8 spo2_detect_status;   // 血氧检测的状态
extern u8 steps_detect_status;  // 计步检测的状态
extern u8 health_all_day_detect_mode; // 全天心率检测的状态
/**
 * @brief 主机通知同步健康检测状态
 */
static void set_tws_sibling_health_detect_work_status(void *_data, u16 len, bool rx)
{
    line_inf
    u8 *data = (u8 *)_data;
    printf("rx:%d, data[0]:0x%x\n", rx, data[0]);
    if (rx) {
        // 心率检测
        if ((data[0] & (1 << 0))) {
            tws_sibling_hr_start();
        } else if ((data[0] & (1 << 0)) == 0) {
            tws_sibling_hr_stop();
        }
        // 血氧检测
        if ((data[0] & (1 << 1))) {
            tws_sibling_spo2_start();
        } else if ((data[0] & (1 << 1)) == 0) {
            tws_sibling_spo2_stop();
        }
        // 计步检测
        if ((data[0] & (1 << 2))) {
            tws_sibling_steps_detect_start();
        } else if ((data[0] & (1 << 2)) == 0) {
            tws_sibling_steps_detect_stop();
        }
#if HEALTH_ALL_DAY_CHECK_ENABLE
        // 全天心率检测
        if ((data[0] & (1 << 3))) {
            health_all_day_detect_switch(1);
        } else if ((data[0] & (1 << 3)) == 0) {
            health_all_day_detect_switch(0);
        }
#endif

    }
}

REGISTER_TWS_FUNC_STUB(work_sync_stub) = {
    .func_id = TWS_FUNC_ID_HEAL_DETCT_WORK_STATUS,
    .func    = set_tws_sibling_health_detect_work_status,
};

/**
 * @brief 通知对耳同步健康检测状态
 */
void tws_sync_health_detect_work_status(void)
{
    u8 data[1];
    data[0] = 0;
    // 3011的心率和血氧是互斥的，不支持同时开
    // 心率检测
    if (hr_detect_status) {
        data[0] |= (1 << 0);
    } else {
        data[0] &= ~(1 << 0);
    }
    // 血氧检测
    if (spo2_detect_status) {
        data[0] |= (1 << 1);
    } else {
        data[0] &= ~(1 << 1);
    }
    // 计步检测
    if (steps_detect_status) {
        data[0] |= (1 << 2);
    } else {
        data[0] &= ~(1 << 2);
    }
#if HEALTH_ALL_DAY_CHECK_ENABLE
    // 全天心率检测
    if (health_all_day_detect_mode) {
        data[0] |= (1 << 3);
    } else {
        data[0] &= ~(1 << 3);
    }
#endif
    tws_api_send_data_to_sibling(data, 1, TWS_FUNC_ID_HEAL_DETCT_WORK_STATUS);
    printf("tws_sync_health_detect_work_status: 0x%x\n", data[0]);
}
#endif  // TCFG_USER_TWS_ENABLE

#endif

