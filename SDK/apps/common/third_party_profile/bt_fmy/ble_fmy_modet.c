#include "system/includes.h"
#include "gSensor/gSensor_manage.h"
#include "ble_fmy_modet.h"

#if (TCFG_GSENSOR_ENABLE && TCFG_P11GSENSOR_EN)
#include "user_p11_cmd.h"

#if GSENSOR_PRINTF_ENABLE
#define log_info(x, ...)  printf("[FMY_MO_DET]" x "\r\n", ## __VA_ARGS__)
#define log_info_hexdump  put_buf
#else
#define log_info(...)
#define log_info_hexdump(...)
#endif

#define  SENSOR_DATA_LEN                        25  // 需小于传感器采集HZ
// Motion detection config
static char *workbuf = NULL;
static const short fs = 25;
static const float thread = 2.0;

// GET DATA FROM WATCH_SGENSOR_BUF
extern short gsensorbuf[];

static void fmy_motion_detection_print_accel_data(const axis_info_t *accel_data, int size)
{
    for (int i = 0; i < size; i++) {
        log_info("x: %d, y: %d, z: %d", accel_data[i].x, accel_data[i].y, accel_data[i].z);
    }
}

bool fmy_motion_detection(void)
{
    axis_info_t accel_data[SENSOR_DATA_LEN] = {0};
    memcpy(accel_data, gsensorbuf, SENSOR_DATA_LEN * 2 * 3);
    /* fmy_motion_detection_print_accel_data(accel_data, SENSOR_DATA_LEN); */
    bool is_moved = run_MotionDetection(workbuf, SENSOR_DATA_LEN, accel_data);
    if (is_moved) {
        log_info("is moving!!");
    } else {
        log_info("is static!!");
    }
    return is_moved;

}

int fmy_motion_detection_init(void)
{
    int buff_size = get_DetectionBuf(fs);
    workbuf = (char *)malloc(buff_size);

    if (workbuf == NULL) {
        log_info("workbuf init fail!!");
        return -1;
    }
    init_MotionDet(workbuf, fs, thread);

    return 0;
}


int fmy_motion_detection_deinit(void)
{
    if (!workbuf) {
        log_info("sensor is not init so can not deinit!!");
        return 0;
    }
    free(workbuf);
    workbuf = NULL;

    return 0;
}

#endif
