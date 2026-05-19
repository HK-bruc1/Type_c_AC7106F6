#include "gSensor/gSensor_manage.h"



#if (TCFG_GSENSOR_ENABLE == 1)

#if TCFG_GSENOR_USER_IIC_TYPE
#define _IIC_USE_HW//硬件iic
#endif
#include "iic_api.h"

#define LOG_TAG             "[GSENSOR]"
#define r_printfRROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

static const struct gsensor_platform_data *platform_data;
G_SENSOR_INTERFACE *gSensor_hdl = NULL;
G_SENSOR_INFO  __gSensor_info = {.iic_delay = 0};
#define gSensor_info (&__gSensor_info)


#if  TCFG_HRSENSOR_ENABLE
extern spinlock_t sensor_iic;
extern u8 sensor_iic_init_status;
#else
spinlock_t sensor_iic;
u8 sensor_iic_init_status;
#endif

u8 gravity_sensor_command(u8 w_chip_id, u8 register_address, u8 function_command)
{
    // return i2c_master_write_nbytes_to_device_reg(gSensor_info->iic_hdl, w_chip_id, &register_address, 1, &function_command, 1);
    spin_lock(&sensor_iic);
    u8 ret = 1;
    iic_start(gSensor_info->iic_hdl);
    if (0 == iic_tx_byte(gSensor_info->iic_hdl, w_chip_id)) {
        ret = 0;
        r_printf("\n gsen iic wr err 0\n");
        goto __gcend;
    }
    delay_nops(gSensor_info->iic_delay);
    if (0 == iic_tx_byte(gSensor_info->iic_hdl, register_address)) {
        ret = 0;
        r_printf("\n gsen iic wr err 1\n");
        goto __gcend;
    }
    delay_nops(gSensor_info->iic_delay);
    if (0 == iic_tx_byte(gSensor_info->iic_hdl, function_command)) {
        ret = 0;
        r_printf("\n gsen iic wr err 2\n");
        goto __gcend;
    }
__gcend:
    iic_stop(gSensor_info->iic_hdl);
    spin_unlock(&sensor_iic);
    return ret;
}

u8 _gravity_sensor_get_ndata(u8 r_chip_id, u8 register_address, u8 *buf, u8 data_len)
{
    // return i2c_master_read_nbytes_from_device_reg(gSensor_info->iic_hdl, r_chip_id-1, &register_address,1, buf, data_len);
    spin_lock(&sensor_iic);
    u8 read_len = 0;
    iic_start(gSensor_info->iic_hdl);
    if (0 == iic_tx_byte(gSensor_info->iic_hdl, r_chip_id - 1)) {
        r_printf("\n gsen iic rd err 0\n");
        read_len = 0;
        goto __gdend;
    }
    delay_nops(gSensor_info->iic_delay);
    if (0 == iic_tx_byte(gSensor_info->iic_hdl, register_address)) {
        r_printf("\n gsen iic rd err 1\n");
        read_len = 0;
        goto __gdend;
    }
    iic_start(gSensor_info->iic_hdl);
    if (0 == iic_tx_byte(gSensor_info->iic_hdl, r_chip_id)) {
        r_printf("\n gsen iic rd err 2\n");
        read_len = 0;
        goto __gdend;
    }
    delay_nops(gSensor_info->iic_delay);
    for (; data_len > 1; data_len--) {
        *buf++ = iic_rx_byte(gSensor_info->iic_hdl, 1);
        read_len ++;
    }
    *buf = iic_rx_byte(gSensor_info->iic_hdl, 0);
    read_len ++;
__gdend:
    iic_stop(gSensor_info->iic_hdl);
    delay_nops(gSensor_info->iic_delay);
    spin_unlock(&sensor_iic);
    return read_len;
}

int gsensor_io_ctl(u8 cmd, void *arg)
{
    if ((!gSensor_hdl) || (!gSensor_hdl->gravity_sensor_ctl)) {
        return -1;
    }
    return gSensor_hdl->gravity_sensor_ctl(cmd, arg);
}

int gravity_sensor_init(void *_data)
{
    gSensor_info->init_flag  = 0;
    int retval = 0;

    if (sensor_iic_init_status == 0) {
        platform_data = (const struct gsensor_platform_data *)_data;
        gSensor_info->iic_hdl = platform_data->iic;
        retval = iic_init(gSensor_info->iic_hdl, get_iic_config(gSensor_info->iic_hdl));
        r_printf("\n  gravity_sensor_init\n");
        if (retval < 0) {
            r_printf("\n  open iic for gsensor err\n");
            return retval;
        } else {
            log_info("\n iic open succ\n");
        }
        spin_lock_init(&sensor_iic);
        sensor_iic_init_status = 1;
    }


    retval = -EINVAL;
    list_for_each_gsensor(gSensor_hdl) {
        if (!memcmp(gSensor_hdl->logo, platform_data->gSensor_name, strlen(platform_data->gSensor_name))) {
            retval = 0;
            break;
        }
    }
    if (retval < 0) {
        r_printf(">>>gSensor_hdl logo err\n");
        return retval;
    }
    if (gSensor_hdl->gravity_sensor_init()) {
        r_printf(">>>>gSensor_Int ERROR\n");
    } else {
        log_info(">>>>gSensor_Int SUCC\n");
        gSensor_info->init_flag  = 1;
    }

    return 0;
}

int gsensor_data_get(short *buf)
{
    return gsensor_io_ctl(READ_GSENSOR_DATA, buf);
}
int gsensor_disable(void)
{
    int valid = 0;
    if (gSensor_info->init_flag  == 1) {
        gSensor_hdl->gravity_sensor_ctl(GSENSOR_DISABLE, &valid);
        if (valid == 0) {
            return 0;
        }
    }
    return -1;
}

int gsensor_enable(void)
{
    int valid = 0;
    gSensor_hdl->gravity_sensor_ctl(SEARCH_SENSOR, &valid);
    if (valid == 0) {
        return -1;
    }
    printf("gsensor_reset_succeed\n");
    return 0;
}

void gSensor_wkupup_disable(void)
{
    log_info("gSensor wkup disable\n");
    // power_wakeup_index_enable(4, 0);
}

void gSensor_wkupup_enable(void)
{
    log_info("gSensor wkup enable\n");
    // power_wakeup_index_enable(4, 1);
}

// 步数管理接口
// 步数管理变量
static u32 total_steps = 0;
// 算法输出数据管理
static algo_out g_algo_out = {0};
// 获取累计步数
u32 g_sensor_get_total_steps(void)
{
    return total_steps;
}

// 设置累计步数
void g_sensor_set_total_steps(u32 steps)
{
    total_steps = steps;
}

// 增加步数
void g_sensor_add_steps(u32 steps)
{
    total_steps += steps;
}

// 清零步数
void g_sensor_clear_steps(void)
{
    total_steps = 0;
}

// 算法输出数据管理接口

// 更新算法输出数据
void g_sensor_update_algo_out(algo_out out)
{
    g_algo_out = out;
}

// 获取算法输出数据
algo_out g_sensor_get_algo_out(void)
{
    return g_algo_out;
}

// 清除算法输出数据
void g_sensor_clear_algo_out(void)
{
    memset(&g_algo_out, 0, sizeof(algo_out));
}
#endif
