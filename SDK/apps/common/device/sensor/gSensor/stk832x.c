#include "app_config.h"
#include "gSensor_manage.h"
#include "includes.h"
#include "iic_api.h"
#include "bank_switch.h"
#include "gpio.h"

/* #pragma bss_seg(".gsport_dev_bss") */
/* #pragma const_seg(".gsport_dev_const") */
/* #pragma code_seg(".gsport_dev_code") */
/*  */


#define BANK_NAME  BANK_SENSOR_TAG

#if (defined TCFG_STK832x_EN) && TCFG_STK832x_EN

#define LOG(fmt,...)    printf("[stk832x] " fmt "\n",##__VA_ARGS__)

// #define STKI2C_ADDRESS	0x0F  //Pin1 connected to GND
#define STKI2C_ADDRESS	0x1F  //Pin1 connected to VDD

#define STK832x_REG_RANGESEL    0x0F
#define STK832x_RANGE_2G        0x03
#define STK832x_RANGE_4G        0x05
#define STK832x_RANGE_8G        0x08

#define STK_REG_POWMODE         0x11
#define STK_SUSPEND_MODE        0x80

#define STK832x_SET_ODR_25HZ
#define STK832x_SET_ODR_50HZ
#define STK832x_SET_ODR_100HZ

#define STK_REG_CHIPID          0x00
#define STK8327_CHIPID_VAL      0x26	/* STK8327 CHIP ID = 0x26 */
#define STK8329_CHIPID_VAL      0x25	/* STK8329&STK8325 CHIP ID = 0x25 */
#define STK832x_CHIPID_VAL      0x23    /* STK8321 or STK8323 CHIP ID = 0x23 */
#define FIFO_FRAME_MAX_CNT      32

static u8 chip_id = 0;

static s8 stk832x_register_read(u8 addr, u8 *data, u8 len)
{
    return _gravity_sensor_get_ndata((STKI2C_ADDRESS << 1) | 0x01, addr, data, len);
}

static s8 stk832x_register_write(u8 addr, u8 data)
{
    return gravity_sensor_command((STKI2C_ADDRESS << 1) | 0x00, addr, data);
}

static void stk832x_software_reset(void)
{
    stk832x_register_write(0x14, 0xB6);
    mdelay(50);
}

static bool stk832x_check_chip_id(void)
{
    stk832x_software_reset();
    stk832x_register_read(STK_REG_CHIPID, &chip_id, 1);

    if (chip_id == STK8327_CHIPID_VAL || chip_id == STK8329_CHIPID_VAL || chip_id == STK832x_CHIPID_VAL) {
        LOG("read stkchip id ok, chip_id = 0x%x\n", chip_id);
        return true;
    }
    LOG("read stkchip id fail, chip_id = 0x%x\n", chip_id);
    return false;
}

static u8 stk832x_suspend_mode(void)
{
    return stk832x_register_write(STK_REG_POWMODE, STK_SUSPEND_MODE);
}

static void stk832x_correction_hz(void)
{
    u8 temp = 0;
    stk832x_register_read(0x45, &temp, 1);
    LOG("before 0x45 = %d", temp);
    temp += 0x20;
    stk832x_register_write(0x45, temp);
    mdelay(5);
    LOG("after 0x45 = %d", temp);
}

static void stk832x_set_odr(void)
{
    if (STK832x_CHIPID_VAL == chip_id) {
        stk832x_correction_hz();
        stk832x_register_write(0x11, 0x74); //set low-power mode Duration (10ms)
        stk832x_register_write(0x3E, 0xC8); //stream mode and store interval 4 (ODR=100 and FIFO ODR=100/4=25Hz)
    } else {
        stk832x_register_write(0x12, 0x08); // set es mode
        stk832x_register_write(0x3E, 0xC0); // stream mode and store interval 1
        stk832x_register_write(0x11, 0x76); //25hz
    }
}

static u8 stk832x_init(void)
{
    if (!stk832x_check_chip_id()) {
        LOG("init fail!");
        return -1;
    }

    stk832x_software_reset();
    stk832x_register_write(STK832x_REG_RANGESEL, STK832x_RANGE_4G);
    stk832x_register_write(0x5E, 0xC0); // eng mode for power saving
    stk832x_register_write(0x34, 0x04); // enable watch dog
    stk832x_register_write(0x10, 0x0D); // set bandwidth
    stk832x_set_odr();
    return 0;
}


static int stk832x_read_fifo(short *frame, axis_info_t *accel)
{
    u8 value[6];

    stk832x_register_read(0x0C, value, 1);
    *frame = value[0] & 0x7f;
    printf("[msg]%s-%d>>>>>>>>>>>*frame=%d", __FUNCTION__, __LINE__, *frame);
    for (u8 i = 0; i < *frame; i++) {
        stk832x_register_read(0x3F, value, 6);
        accel[i].x = (short int)((((int)((char)value[1])) << 8) | (value[0] & 0xF0)) >> 3;  //1024 lbs/g
        accel[i].y = (short int)((((int)((char)value[3])) << 8) | (value[2] & 0xF0)) >> 3;  //1024 lbs/g
        accel[i].z = (short int)((((int)((char)value[5])) << 8) | (value[4] & 0xF0)) >> 3;  //1024 lbs/g
        LOG("xyz:%d,%d,%d", accel[i].x, accel[i].y, accel[i].z);
    }
    return *frame;
}



static s32 stk832x_ctl(u8 cmd, void *arg)
{
    s8  res;
    s32 ret = 0;
//   LOG("cmd=%d",cmd);
    switch (cmd) {
    case GSENSOR_DISABLE:
        res = stk832x_suspend_mode();
        memcpy(arg, &res, 1);
        break;
    case GSENSOR_RESET_INT:
        res = stk832x_init();
        memcpy(arg, &res, 1);
        break;
    case GSENSOR_RESUME_INT:
        break;
    case GSENSOR_INT_DET:
        break;
    case READ_GSENSOR_DATA: {
        short *data = arg;
        ret = stk832x_read_fifo(&data[0], (axis_info_t *)&data[1]);
    }
    break;
    case SEARCH_SENSOR:
        res = stk832x_check_chip_id();
        memcpy(arg, &res, 1);
        break;
    default:
        break;
    }
    return ret;
}


REGISTER_GRAVITY_SENSOR(gSensor) = {
    .logo = "stk832x",
    .gravity_sensor_init  = stk832x_init,
    .gravity_sensor_check = NULL,
    .gravity_sensor_ctl   = stk832x_ctl,
};

#endif
