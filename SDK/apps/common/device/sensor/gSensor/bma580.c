#include "app_config.h"
#include "gSensor_manage.h"
#include "includes.h"
#include "iic_api.h"
#include "bank_switch.h"
#include "gpio.h"
#include <math.h>
#include "system/includes.h"

#define LOG_TAG_CONST      	GSENSOR
#define LOG_TAG     		"[GSENSOR]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"

#if (defined TCFG_BMA580_EN) && TCFG_BMA580_EN
#define BMAI2C_ADDRESS	0x18  //Pin1 connected to VDD

#define BMA_REG_CHIPID          				   0x00
#define BMA5_REG_HEALTH_STATUS                     0x02
#define BMA5_REG_CMD_SUSPEND                       0x04
#define BMA5_REG_FIFO_LEVEL_0                      0x22
#define BMA5_REG_FIFO_LEVEL_1                      0x23
#define BMA5_REG_FIFO_DATA_OUT                     0x24
#define BMA5_REG_ACC_CONF_0                        0x30
#define BMA5_REG_ACC_CONF_1                        0x31
#define BMA5_REG_ACC_CONF_2                        0x32
#define BMA5_REG_FIFO_CONF_0                       0x41
#define BMA5_REG_FIFO_CONF_1                       0x42
#define BMA5_REG_FEAT_ENG_CONF                     0x50

#define BMA5_CHIPID_VAL      0xC4	/* BMA580 CHIP ID = 0x26 */
#define BMA5_SENSOR_HEALTH_STATUS_MSK              0x0F

static u8 chip_id = 0;

static s8 bma580_register_read(u8 addr, u8 *data, u8 len)
{
    return _gravity_sensor_get_ndata((BMAI2C_ADDRESS << 1) | 0x01, addr, data, len);
}

static s8 bma580_register_write(u8 addr, u8 data)
{
    return gravity_sensor_command((BMAI2C_ADDRESS << 1) | 0x00, addr, data);
}

void bma580_power_ctrl(u8 ctrl)
{
    if (ctrl == 0) {
        gpio_set_mode(IO_PORT_SPILT(IO_PORTA_03), PORT_HIGHZ);
    } else {
        gpio_set_mode(IO_PORT_SPILT(IO_PORTA_03), PORT_OUTPUT_HIGH);
    }
}

bool bma580_check_chip_id(void)
{
    bma580_power_ctrl(1);
    u8 check_cnt = 0;
    while (1) {
        bma580_register_read(BMA_REG_CHIPID, &chip_id, 1);
        if (chip_id == BMA5_CHIPID_VAL) {

            return true;
        }
        if (check_cnt++ >= 20) {
            log_error("can not find sensor id id=%x", chip_id);
            break;
        }

        /* os_time_dly(1); */
    }
    return false;
}
u8 bma580_set_cmd_supend(u8 supend_ctrl)
{
    return (u8)bma580_register_write(BMA5_REG_CMD_SUSPEND, supend_ctrl);
}
u8 bma580_health_check()
{
    u8 health_check = 0;
    u8 check_cnt = 0;
    while (1) {

        bma580_register_read(BMA5_REG_HEALTH_STATUS, &health_check, 1);
        if ((health_check & 0xf) == BMA5_SENSOR_HEALTH_STATUS_MSK) {
            return true;
        }
        if (check_cnt++ >= 20) {
            log_error("sensor health check fail 0x%x", health_check);
            break;
        }
        /* os_time_dly(1); */
    }
    return false;
}
u8 bma580_config()
{
    u8 ret = 0;
    bma580_register_write(BMA5_REG_ACC_CONF_0, 0x0f);//sensor开启
    bma580_register_write(BMA5_REG_ACC_CONF_1, 0xa4);//odr:25hz 高性能模式
    bma580_register_write(BMA5_REG_ACC_CONF_2, 0x85);//4g量程
    ret = 0;
    bma580_register_read(BMA5_REG_ACC_CONF_0, &ret, 1);
    if (ret != 0xf) {
        log_error(" BMA5_REG_ACC_CONF_0 write err ret=%x", ret);
    }
    bma580_register_read(BMA5_REG_ACC_CONF_1, &ret, 1);
    if (ret != 0xa4) {
        log_error(" BMA5_REG_ACC_CONF_1 write err ret=%x", ret);
    }
    bma580_register_read(BMA5_REG_ACC_CONF_2, &ret, 1);
    if (ret != 0x85) {
        log_error(" BMA5_REG_ACC_CONF_2 write err ret=%x", ret);
    }
    return 0;
}
u8 bma580_set_fifo_config()
{
    u8 ret = 0;
    bma580_register_write(0x40, 0x01);
    bma580_register_write(BMA5_REG_FIFO_CONF_0, 0x0f);//fifo xyz加速度开启,不开启压缩模式
    bma580_register_write(BMA5_REG_FIFO_CONF_1, 0x03);//fifo缓存大小为1024
    bma580_register_read(BMA5_REG_FIFO_CONF_0, &ret, 1);
    if (ret != 0xf) {
        log_error("BMA5_REG_FIFO_CONF_0 write err ret=%x", ret);
    }
    bma580_register_read(BMA5_REG_FIFO_CONF_1, &ret, 1);
    if (ret != 0x3) {
        log_error("BMA5_REG_FIFO_CONF_1 write err ret=%x", ret);
    }

    return 0;


}

u16 bma580_get_fifo_data_len()
{
    u8 data_len[2];
    bma580_register_read(BMA5_REG_FIFO_LEVEL_0, &data_len[0], 1);
    bma580_register_read(BMA5_REG_FIFO_LEVEL_1, &data_len[1], 1);
    log_info("bma580 get fifo_len=%d", ((data_len[1] << 8) | data_len[0]));
    return (((u16)data_len[1] << 8) | data_len[0]);
}

u8 bma580_get_fifo_data(short *frame, axis_info_t *accel)
{
    /* bma580_register_write(0x12,0x04); */
    u8 status = 0;
    short int x = 0, y = 0, z = 0, msb = 0, lsb = 0;
    u16 fifo_len = bma580_get_fifo_data_len();
    u16 i = 0;
    if (fifo_len > 384) { //gsensor的buf390字节
        fifo_len = 384;
    } else if (fifo_len == 0) {
        log_warn("sensor no fifo_len");
        return 0;
    }
    u8 *fifo_data = zalloc(fifo_len + (fifo_len / 6));
    *frame = fifo_len / 6;
    u16 read_data_len = fifo_len + (fifo_len / 6); //每一帧有一个帧头不计算在其中所以要补上;
    u16 ret = bma580_register_read(BMA5_REG_FIFO_DATA_OUT, (u8 *)fifo_data, read_data_len);
    /* put_buf(fifo_data,fifo_len+(fifo_len/6)); */
    for (u16 idx = 0; idx < (read_data_len); idx += 7) {
        msb = fifo_data[idx + 2];
        lsb = fifo_data[idx + 1];
        accel[i].x = ((s16)((msb << 8) | (lsb))) >> 3;
        msb = fifo_data[idx + 4];
        lsb = fifo_data[idx + 3];
        accel[i].y = ((s16)((msb << 8) | (lsb))) >> 3;
        msb = fifo_data[idx + 6];
        lsb = fifo_data[idx + 5];
        accel[i].z = ((s16)((msb << 8) | (lsb))) >> 3;
        i++;
        log_info("xyz:%d,%d,%d", accel[i].x, accel[i].y, accel[i].z);
    }
    free(fifo_data);
    return 0;
}
u8 bma580_set_int_map()
{

    u8 ret = bma580_register_write(0x36, 0x10);
    bma580_register_write(BMA5_REG_FEAT_ENG_CONF, 0x0);

    bma580_register_read(0x36, &ret, 1);
    return 0;
}

u8  bma580_init(void)
{
    u8 ret = 0;
    if (bma580_check_chip_id() != true) {
        return false;
    }
    if (bma580_set_cmd_supend(0) != true) {
        log_error("exit supend mode fail");
    }
    if (bma580_health_check() != true) {
        return false;
    }
    bma580_register_write(BMA5_REG_FEAT_ENG_CONF, 0x0); //关闭特征模式
    /* bma580_set_int_map(); */ //设置sensor中断引脚 默认关闭
    bma580_config();
    bma580_set_fifo_config();
    return 1;
}
u8 bma580_suspend_mode()
{
    u8 ret = bma580_set_cmd_supend(1);
    bma580_power_ctrl(0);
    return ret;
}
static s32 bma580_ctl(u8 cmd, void *arg)
{
    s8  res;
    s32 ret = 0;
//   LOG("cmd=%d",cmd);
    switch (cmd) {
    case GSENSOR_DISABLE:
        res = bma580_suspend_mode();
        memcpy(arg, &res, 1);
        break;
    case GSENSOR_RESET_INT:
        res = bma580_init();
        memcpy(arg, &res, 1);
        break;
    case GSENSOR_RESUME_INT:
        break;
    case GSENSOR_INT_DET:
        break;
    case READ_GSENSOR_DATA: {
        short *data = arg;
        ret = bma580_get_fifo_data(&data[0], (axis_info_t *)&data[1]);
    }
    break;
    case SEARCH_SENSOR:
        res = bma580_check_chip_id();
        memcpy(arg, &res, 1);
        break;
    default:
        break;
    }
    return ret;
}


REGISTER_GRAVITY_SENSOR(gSensor) = {
    .logo = "bma580",
    .gravity_sensor_init  = bma580_init,
    .gravity_sensor_check = NULL,
    .gravity_sensor_ctl   = bma580_ctl,
};
#endif
