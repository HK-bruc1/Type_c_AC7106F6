#include "app_config.h"
#include "timer.h"
#include "gSensor/gSensor_manage.h"
#include "hr_sensor/hrSensor_manage.h"
#include "gSensor/p33_algo_motion.h"
#if JL_RCSP_EAR_SENSORS_DATA_OPT
#include "rcsp/server/functions/sensors/ear_sports_data_opt.h"
#endif
#if TCFG_GSENSOR_ENABLE || TCFG_HRSENSOR_ENABLE

extern int gsensor_data_get(short *buf);
#define SENSOR_KICK  (Q_MSG + 1)
u16 cb_timer_id = 0;

static void refresh_sensor_data_check_cb(void *parm)
{
    os_taskq_post_type("app_sensor", SENSOR_KICK, 0, NULL);
}

void sensor_del_data_check_cb()
{
    if (cb_timer_id != 0) {
        sys_s_hi_timer_del(cb_timer_id);
        cb_timer_id = 0;
    }
}

#if TCFG_GSENSOR_ENABLE
//算法调试
const unsigned char jl_motion_debug_enable  = 1; //调试开关，0为关闭，1为打开
const unsigned char jl_motion_debug_acceler = 0; //打印加速度原始数据

//算法开关，关闭部分算法可以节省内存，1为打开，0为关闭
const unsigned char jl_motion_step_counter     = 1; //计步器
const unsigned char jl_motion_distance         = 1; //距离
const unsigned char jl_motion_step_frequency   = 1; //步频
const unsigned char jl_motion_calories         = 1; //卡路里 = 基础代谢率（BMR） + 活动代谢率（AMR）
const unsigned char jl_motion_calories_amr     = 1; //活动代谢率（AMR）：活动代谢率是指在运动和日常活动中所消耗的能量
const unsigned char jl_motion_sedentary        = 1; //久坐提醒
const unsigned char jl_motion_sleep            = 1; //睡眠
const unsigned char jl_motion_shake            = 1; //摇一摇

//调整计步器的参数
const unsigned char jl_motion_brush_teeth      = 0; //过滤刷牙产生的计步
const unsigned char jl_motion_step_filter      = 50;//过滤日常活动产生的计步

//睡眠算法参数
const unsigned char jl_motion_sleep_conditional_end        = 0; //通过某些条件结束睡眠
const unsigned char jl_motion_sleep_wake_refer_to_samsung  = 1;   //清醒状态参照三星

void gsensor_motion_process(short *p)
{
    short point = p[0]; //frame number
    printf("point = %d\n", point);
    if ((point <= 0) || (point > 64)) {
        return;
    }

    u32 timestamp = 0; // timestamp_mytime_2_utc_sec(&time);   // 计步算法不需要时间戳，这个时间戳主要是给睡眠算法的

    algo_out out = algo_motion_run(timestamp, (accel_data *)&p[1], point, 0, 0); // 算法输出实时的步数
    printf("out.update.step_counter = %d\n", out.update.step_counter);
    // printf("out.steps:%d", out.steps);
    if (out.update.step_counter) {
        g_sensor_add_steps(out.steps);    // 使用新的步数管理接口累加步数
    }
    // 更新算法输出数据，使用统一的管理接口
    g_sensor_update_algo_out(out);
}
#endif

void app_sensor_task(void *p)
{
    short accel_data[1 + 64 * sizeof(axis_info_t)]; //lenght + 64 frame acceleromter data
    memset(accel_data, 0, sizeof(accel_data));

#if TCFG_GSENSOR_ENABLE
    // 计步算法初始化
    algo_type open_algo;
    accel_config accel;
    user_info user = {.ages = 28, .gender = 1, .height = 170, .weight = 60, .step_factor = 80};
    unsigned char thres[4] = {25, 7, 5, 2};
    open_algo.all = 0;
    open_algo.step_counter = 1;
    accel.sps = 25;
    accel.lsb_g = 1024;
    algo_motion_init(open_algo, accel, user);
#endif

    // cb_timer_id = sys_s_hi_timer_add(NULL, refresh_sensor_data_check_cb, 320); //320ms 这个timer是读gsensor数据，并输入给心率，hx3918是320ms读取ppg数据
    //os_time_dly(100);
#if TCFG_HRSENSOR_ENABLE
    /* hr_sensor_measure_spo2_start(0, 0); */
    /* hr_sensor_measure_hr_start(0, 0); */
    hr_sensor_measure_wear_start(0, 0);
#endif

    int msg[8];
    int ret;
    while (1) {
        ret = os_taskq_pend("taskq", msg, ARRAY_SIZE(msg));
        if (ret != OS_TASKQ) {
            continue;
        }
        switch (msg[0]) {
        case SENSOR_KICK:
#if TCFG_GSENSOR_ENABLE
            accel_data[0] = gsensor_data_get((short *)accel_data + 1);				// 读加速度数据
#endif
#if TCFG_HRSENSOR_ENABLE
            hr_sensor_io_ctl(INPUT_ACCELEROMETER_DATA, accel_data);  //加速度数据输入给心率
#endif
            //如果有其它算法需要使用加速度数据可在此添加
#if TCFG_GSENSOR_ENABLE
            // 计步
#if TCFG_STEP_COUNT_ENABLE
            extern u8 steps_detect_status;
            if (steps_detect_status) {
                gsensor_motion_process(accel_data);
            }
#endif
#endif
            //如果有其它算法需要使用加速度数据可在此添加
            break;
        default:
            break;
        }

    }
}


void app_sensor_init(void)
{
    task_create(app_sensor_task, NULL, "app_sensor");
}


#endif
