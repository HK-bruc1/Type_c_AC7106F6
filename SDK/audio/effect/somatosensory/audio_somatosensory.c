#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".audio_somatosensory.data.bss")
#pragma data_seg(".audio_somatosensory.data")
#pragma const_seg(".audio_somatosensory.text.const")
#pragma code_seg(".audio_somatosensory.text")
#endif

#include "typedef.h"
#include "system/includes.h"
#include "audio_base.h"
#include "app_config.h"
#include "audio_somatosensory.h"
#include "circular_buf.h"
#include "btstack/avctp_user.h"
#include "clock_manager/clock_manager.h"

#if (defined TCFG_AUDIO_SOMATOSENSORY_ENABLE && TCFG_AUDIO_SOMATOSENSORY_ENABLE)

#define SOMATOSENSORY_DEBUG_INFO
#ifdef SOMATOSENSORY_DEBUG_INFO
#define somato_printf    r_printf
#else
#define somato_printf(...)
#endif

struct somatosensory_hdl *somatosensory_hdl = NULL;

void somatosensory_open()
{
    if (somatosensory_hdl) {
        somato_printf("somatosensory is running, open fail");
        return;
    }
    somatosensory_hdl = audio_somatosensory_open();
}

void somatosensory_close()
{
    audio_somatosensory_close(somatosensory_hdl);
    somatosensory_hdl = NULL;
}

static int audio_somatosensory_run(struct somatosensory_hdl *hdl)
{
    if (!hdl || !hdl->work_buf) {
        return 0;
    }
    int cbuf_len = cbuf_get_data_len(&hdl->cbuf);
    int data_len = cbuf_len / 12 * 12; //6个点(s16)/12个byte 为一组
    int rlen = cbuf_read(&hdl->cbuf, hdl->run_data, data_len);
    if (rlen != data_len) {
        somato_printf("somatosensory cbuf read err %d %d", rlen, data_len);
    }
    return run_somatosensory(hdl->work_buf, (s16 *)hdl->run_data, rlen >> 1);
}

static void audio_somatosensory_task(void *priv)
{
    struct somatosensory_hdl *hdl = (struct somatosensory_hdl *)priv;
    while (1) {
        os_time_dly(2);
        if (!hdl->task_state) {
            continue;
        }
        hdl->task_busy = 1;
        int sensor_data_len = 0;
        if (hdl->sensor) {
            sensor_data_len = space_motion_data_read(hdl->sensor, hdl->sensor_data, SENSOR_BUF_LEN);
        }
        int wlen = cbuf_write(&hdl->cbuf, hdl->sensor_data, sensor_data_len);
        if (wlen != sensor_data_len) {
            somato_printf("somatosensory cbuf write err %d %d", wlen, sensor_data_len);
        }
        int event = audio_somatosensory_run(hdl);
        switch (event) {
        case UNIDENTIFIED:
            break;
        case TURNING_HEAD:
            somato_printf("TURN_HEAD\n");
            break;
        case SHAKING_HEAD:
            somato_printf("SHAKE_HEAD\n");
#if SOMATOSENSORY_CALL_EVENT
            bt_cmd_prepare(USER_CTRL_HFP_CALL_HANGUP, 0, NULL);
#endif
            break;
        case NODDING_HEAD:
            somato_printf("NODDING_HEAD\n");
#if SOMATOSENSORY_CALL_EVENT
            bt_cmd_prepare(USER_CTRL_HFP_CALL_ANSWER, 0, NULL);
#endif
            break;
        }
        hdl->task_busy = 0;
    }
}

struct somatosensory_hdl *audio_somatosensory_open()
{
    struct somatosensory_hdl *hdl = zalloc(sizeof(struct somatosensory_hdl));
    if (!hdl) {
        somato_printf("somatosensory_hdl alloc err!!!");
        return NULL;
    }
    acc_cel_t acc_cel;
    int ret = syscfg_read(CFG_IMU_ACC_OFFEST_ID, &acc_cel, sizeof(acc_cel_t));
    if (ret != sizeof(acc_cel_t)) {
        somato_printf("vm acc_cel read fail !!!");
        /* vm中没有记录传感器数据，使用默认数据。*/
        acc_cel.acc_offx = -144.453f;
        acc_cel.acc_offy = -70.315f;
        acc_cel.acc_offz = -83.113f;
    } else {
        somato_printf("vm acc_cel read succ !!!");
        /* put_float(acc_cel.acc_offx); */
        /* put_float(acc_cel.acc_offy); */
        /* put_float(acc_cel.acc_offz); */
    }
    gyro_cel_t gyro_cel;
    ret = syscfg_read(CFG_IMU_GYRO_OFFEST_ID, &gyro_cel, sizeof(gyro_cel_t));
    if (ret != sizeof(gyro_cel_t)) {
        somato_printf("vm gyro_cel read fail !!!");
        /* vm中没有记录传感器数据，使用默认数据。*/
        gyro_cel.gyro_x = 0.796f;
        gyro_cel.gyro_y = -0.865f;
        gyro_cel.gyro_z = 0.420f;
    } else {
        somato_printf("vm gyro_cel read succ !!!");
        /* put_float(gyro_cel.gyro_x); */
        /* put_float(gyro_cel.gyro_y); */
        /* put_float(gyro_cel.gyro_z); */
    }

    hdl->sensor = space_motion_detect_open();
    if (!hdl->sensor) {
        somato_printf("sensor open err");
        free(hdl);
        return NULL;
    }

    hdl->axis_parm.acc_off_x = acc_cel.acc_offx;
    hdl->axis_parm.acc_off_y = acc_cel.acc_offx;
    hdl->axis_parm.acc_off_z = acc_cel.acc_offx;
    hdl->axis_parm.gro_off_x = gyro_cel.gyro_x;
    hdl->axis_parm.gro_off_y = gyro_cel.gyro_y;
    hdl->axis_parm.gro_off_z = gyro_cel.gyro_z;

    /* 不同传感器采样率不同 */
#if TCFG_ICM42670P_ENABLE
    hdl->somato_parm.fs          = 99;
#elif TCFG_LSM6DSL_ENABLE
    hdl->somato_parm.fs          = 104;
#else
    hdl->somato_parm.fs          = 100;
#endif

    /* 以下参数根据效果细调 */
    hdl->somato_parm.val_dif     = 28.0f;
    hdl->somato_parm.val_station = 400;
    hdl->somato_parm.val_nod     = 500;
    hdl->somato_parm.val_turn    = 1000;

    /* 以下参数根据硬件上传感器摆放位置调整 */
    hdl->somato_parm.main_axis   = 1;
    hdl->somato_parm.plane_axis  = 0;
    hdl->somato_parm.sign_gyro1  = -1;
    hdl->somato_parm.sign_gyro2  = 1;
    hdl->somato_parm.sign_gyro3  = 1;
    hdl->somato_parm.G_val       = 1400;

    int size = get_somatosensory_buf();
    somato_printf("somatosensory size %d byte", size);
    hdl->work_buf = zalloc(size);
    if (!hdl->work_buf) {
        somato_printf("somatosensory workbuf alloc err!!!");
        free(hdl);
        return NULL;
    }
    init_somatosensory(hdl->work_buf, &hdl->axis_parm, &hdl->somato_parm);
    hdl->cbuf_data = zalloc(SENSOR_BUF_LEN * 2);
    ASSERT(hdl->cbuf_data);
    hdl->sensor_data = zalloc(SENSOR_BUF_LEN);
    ASSERT(hdl->sensor_data);
    hdl->run_data = zalloc((SENSOR_BUF_LEN * 2) / 12 * 12);
    ASSERT(hdl->run_data);
    cbuf_init(&hdl->cbuf, hdl->cbuf_data, SENSOR_BUF_LEN * 2);
    hdl->task_state = 1;
    hdl->task_busy = 0;
#if TCFG_SENSOR_DATA_EXPORT_ENABLE
    //使能数据导出，抬高时钟
    clock_alloc("SOMATOSENSORY", 192 * 1000000);
#else
    clock_alloc("SOMATOSENSORY", 30 * 1000000);
#endif
    int err = task_create(audio_somatosensory_task, hdl, SOMATOSENSORY_TASK_NAME);
    if (err != OS_NO_ERR) {
        ASSERT(0, "audio_somatosensory_task create err!!! %x\n", err);
    }
    somato_printf("somatosensory open success");
    return hdl;
}

void audio_somatosensory_close(struct somatosensory_hdl *hdl)
{
    if (!hdl) {
        return;
    }
    hdl->task_state = 0;
    while (hdl->task_busy) {
        os_time_dly(1);
    }
    task_kill(SOMATOSENSORY_TASK_NAME);
    clock_free("SOMATOSENSORY");
    if (hdl->sensor) {
        space_motion_detect_close(hdl->sensor);
        hdl->sensor = NULL;
    }
    if (hdl->sensor_data) {
        free(hdl->sensor_data);
        hdl->sensor_data = NULL;
    }
    if (hdl->cbuf_data) {
        free(hdl->cbuf_data);
        hdl->cbuf_data = NULL;
    }
    if (hdl->run_data) {
        free(hdl->run_data);
        hdl->run_data = NULL;
    }
    if (hdl->work_buf) {
        free(hdl->work_buf);
        hdl->work_buf = NULL;
    }
    free(hdl);
    somato_printf("somatosensory close");
}

#endif

