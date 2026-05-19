#include "asm/power_interface.h"
#include "gpadc.h"
#include "app_config.h"
#include "gSensor/gSensor_manage.h"
#include "imuSensor_manage.h"
#include "iic_api.h"
#include "asm/sdmmc.h"
#include "asm/spi_hw.h"
#include "rtc/rtc_dev.h"
#include "linein_dev.h"
#include "usb/host/usb_storage.h"
#include "spi.h"

#if TCFG_MPU6887P_ENABLE
#include "imu_sensor/mpu6887/mpu6887p.h"
#endif /*TCFG_MPU6887P_ENABLE*/
#if TCFG_QMI8658_ENABLE
#include "imu_sensor/qmi8658/qmi8658c.h"
#endif /*TCFG_QMI8658_ENABLE*/
#if TCFG_LSM6DSL_ENABLE
#include "imu_sensor/lsm6dsl/lsm6dsl.h"
#endif /*TCFG_LSM6DSL_ENABLE*/
#if TCFG_ICM42670P_ENABLE
#include "imu_sensor/icm_42670p/icm_42670p.h"
#endif /*TCFG_ICM42670P_ENABLE*/

const struct iic_master_config soft_iic_cfg_const[MAX_SOFT_IIC_NUM] = {
    //soft iic0
    {
        .role = IIC_MASTER,
        .scl_io = TCFG_SW_I2C0_CLK_PORT,
        .sda_io = TCFG_SW_I2C0_DAT_PORT,
        .io_mode = PORT_INPUT_PULLUP_10K,      //上拉或浮空，如果外部电路没有焊接上拉电阻需要置上拉
        .hdrive = PORT_DRIVE_STRENGT_2p4mA,    //IO口强驱
        .master_frequency = TCFG_SW_I2C0_DELAY_CNT,  //IIC通讯波特率 未使用
        .io_filter = 0,                        //软件iic无滤波器
    },
#if 0
    //soft iic1
    {
        .role = IIC_MASTER,
        .scl_io = TCFG_SW_I2C1_CLK_PORT,
        .sda_io = TCFG_SW_I2C1_DAT_PORT,
        .io_mode = PORT_INPUT_PULLUP_10K,      //上拉或浮空，如果外部电路没有焊接上拉电阻需要置上拉
        .hdrive = PORT_DRIVE_STRENGT_2p4mA,    //IO口强驱
        .master_frequency = TCFG_SW_I2C1_DELAY_CNT,  //IIC通讯波特率 未使用
        .io_filter = 0,                        //软件iic无滤波器
    },

#endif
};

const struct iic_master_config hw_iic_cfg_const[MAX_HW_IIC_NUM] = {
    {
        .role = IIC_MASTER,
#if (defined CONFIG_CPU_BR36 || defined CONFIG_CPU_BR27)
        .scl_io = TCFG_HW_I2C0_CLK_PORT,
        .sda_io = TCFG_HW_I2C0_DAT_PORT,
#else
#endif
        .io_mode = PORT_INPUT_PULLUP_10K,      //上拉或浮空，如果外部电路没有焊接上拉电阻需要置上拉
        .hdrive = PORT_DRIVE_STRENGT_2p4mA,    //IO口强驱
        .master_frequency = TCFG_HW_I2C0_CLK,  //IIC通讯波特率
        .io_filter = 1,                        //是否打开滤波器（去纹波）
    },
};

/************************** imu sensor ****************************/
#if TCFG_IMUSENSOR_ENABLE
static const struct imusensor_platform_data imu_sensor_data[] = {
#if TCFG_SH3001_ENABLE
    {
        .peripheral_hdl = TCFG_SH3001_USER_IIC_INDEX,     //iic_hdl     or  spi_hdl
        .peripheral_param0 = 0,  //iic_delay(iic byte间间隔)   or spi_cs_pin
        .imu_sensor_name = "sh3001",
        .imu_sensor_int_io = TCFG_SH3001_DETECT_IO,
    },
#endif
#if TCFG_TP_MPU9250_ENABLE
    {
        .peripheral_hdl = TCFG_MPU9250_USER_IIC_INDEX,     //iic_hdl     or  spi_hdl
        .peripheral_param0 = 1, //iic_delay(iic byte间间隔)   or spi_cs_pin//iic byte间间隔
        .imu_sensor_name = "mpu9250",
        .imu_sensor_int_io = TCFG_MPU9250_DETECT_IO,
    },
#endif
#if TCFG_MPU6887P_ENABLE
    {
        .peripheral_hdl = TCFG_MPU6887P_USER_IIC_INDEX,     //iic_hdl     or  spi_hdl
        .peripheral_param0 = 0, //iic_delay(iic byte间间隔)   or spi_cs_pin//iic byte间间隔
        .imu_sensor_name = "mpu6887p",
        .imu_sensor_int_io = TCFG_MPU6887P_DETECT_IO,
    },
#endif
#if TCFG_ICM42670P_ENABLE
    {
        .peripheral_hdl = TCFG_ICM42670P_USER_IIC_INDEX,     //iic_hdl     or  spi_hdl
        .peripheral_param0 = 0, //iic_delay(iic byte间间隔)   or spi_cs_pin//iic byte间间隔
        .imu_sensor_name = "icm42670p",
        .imu_sensor_int_io = TCFG_ICM42670P_DETECT_IO,
    },
#endif
#if TCFG_MPU6050_EN //未整合参数如GSENSOR_PLATFORM_DATA_BEGIN配置
    {
        .peripheral_hdl = 0,     //iic_hdl     or  spi_hdl
        .peripheral_param0 = 0, //iic_delay(iic byte间间隔)   or spi_cs_pin//iic byte间间隔
        .imu_sensor_name = "mpu6050",
        .imu_sensor_int_io = -1,
    },
#endif
#if TCFG_QMI8658_ENABLE
    {
        .peripheral_hdl = TCFG_QMI8658_USER_IIC_INDEX,     //iic_hdl     or  spi_hdl
        .peripheral_param0 = 0, //iic_delay(iic byte间间隔)   or spi_cs_pin//iic byte间间隔
        .imu_sensor_name = "qmi8658",
        .imu_sensor_int_io = TCFG_QMI8658_DETECT_IO,
    },
#endif
#if TCFG_LSM6DSL_ENABLE
    {
        .peripheral_hdl = TCFG_LSM6DSL_USER_IIC_INDEX,     //iic_hdl     or  spi_hdl
        .peripheral_param0 = 0, //iic_delay(iic byte间间隔)   or spi_cs_pin//iic byte间间隔
        .imu_sensor_name = "lsm6dsl",
        .imu_sensor_int_io = TCFG_LSM6DSL_DETECT_IO,
    },
#endif
};
#endif


#if TCFG_SD0_ENABLE
SD0_PLATFORM_DATA_BEGIN(sd0_data) = {
    .port = {
        TCFG_SD0_PORT_CMD,
        TCFG_SD0_PORT_CLK,
        TCFG_SD0_PORT_DA0,
        TCFG_SD0_PORT_DA1,
        TCFG_SD0_PORT_DA2,
        TCFG_SD0_PORT_DA3,
    },
    .data_width             = TCFG_SD0_DAT_MODE,
    .speed                  = TCFG_SD0_CLK,
    .detect_mode            = TCFG_SD0_DET_MODE,
    .priority				= 3,

#if (TCFG_SD0_DET_MODE == SD_IO_DECT)
    .detect_io              = TCFG_SD0_DET_IO,
    .detect_io_level        = TCFG_SD0_DET_IO_LEVEL,
    .detect_func            = sdmmc_0_io_detect,
#elif (TCFG_SD0_DET_MODE == SD_CLK_DECT)
    .detect_io_level        = TCFG_SD0_DET_IO_LEVEL,
    .detect_func            = sdmmc_0_clk_detect,
#else
    .detect_func            = sdmmc_cmd_detect,
#endif

#if (TCFG_SD0_POWER_SEL == SD_PWR_SDPG)
    .power                  = sd_set_power,
#else
    .power                  = NULL,
#endif

    SD0_PLATFORM_DATA_END()
};
#endif

/************************** linein KEY ****************************/
#if TCFG_APP_LINEIN_EN
struct linein_dev_data linein_data = {
    .enable = TCFG_APP_LINEIN_EN,
    .port   = NO_CONFIG_PORT,
    .up     = 1,
    .down   = 0,
    .ad_channel = NO_CONFIG_PORT,
    .ad_vol = 0,
};
#endif

/************************** rtc ****************************/
#if TCFG_APP_RTC_EN
//初始一下当前时间
const struct sys_time def_sys_time = {
    .year = 2020,
    .month = 1,
    .day = 1,
    .hour = 0,
    .min = 0,
    .sec = 0,
};

//初始一下目标时间，即闹钟时间
const struct sys_time def_alarm = {
    .year = 2050,
    .month = 1,
    .day = 1,
    .hour = 0,
    .min = 0,
    .sec = 0,
};

const struct rtc_dev_platform_data data = {
    .default_sys_time = &def_sys_time,
    .default_alarm = &def_alarm,

    .rtc_clk = RTC_CLK_RES_SEL,
    .rtc_sel = HW_RTC,
    //闹钟中断的回调函数,用户自行定义
    .cbfun = NULL,
    /* .cbfun = alarm_isr_user_cbfun, */
    .x32xs = 0,
};

#endif

#if (TCFG_HW_SPI1_ENABLE || TCFG_HW_SPI2_ENABLE)
const struct spi_platform_data spix_p_data[HW_SPI_MAX_NUM] = {
    [0] = {
        //spi0
    },
#if TCFG_HW_SPI1_ENABLE
    [1] = {
        //spi1
        .port = {
            TCFG_HW_SPI1_PORT_CLK, //clk any io
            TCFG_HW_SPI1_PORT_DO, //do any io
            TCFG_HW_SPI1_PORT_DI, //di any io
#ifdef  TCFG_HW_SPI1_PORT_D2
            TCFG_HW_SPI1_PORT_D2, //d2 any io
#endif
#ifdef  TCFG_HW_SPI1_PORT_D3
            TCFG_HW_SPI1_PORT_D3, //d3 any io
#endif
            0xff, //cs any io(主机不操作cs)
        },
        .role = TCFG_HW_SPI1_ROLE,//SPI_ROLE_MASTER,
        .clk  = TCFG_HW_SPI1_BAUD,
        .mode = TCFG_HW_SPI1_MODE,//SPI_MODE_BIDIR_1BIT,//SPI_MODE_UNIDIR_2BIT,
        .bit_mode = SPI_FIRST_BIT_MSB,
        .cpol = 0,//clk level in idle state:0:low,  1:high
        .cpha = 0,//sampling edge:0:first,  1:second
        .ie_en = 0, //ie enbale:0:disable,  1:enable
        .irq_priority = 3,
        .spi_isr_callback = NULL,  //spi isr callback
    },
#endif
#if SUPPORT_SPI2
#if TCFG_HW_SPI2_ENABLE
    [2] = {
        //spi2
        .port = {
            TCFG_HW_SPI2_PORT_CLK, //clk any io
            TCFG_HW_SPI2_PORT_DO, //do any io
            TCFG_HW_SPI2_PORT_DI, //di any io
#ifdef  TCFG_HW_SPI2_PORT_D2
            TCFG_HW_SPI2_PORT_D2, //d2 any io
#endif
#ifdef  TCFG_HW_SPI2_PORT_D3
            TCFG_HW_SPI2_PORT_D3, //d3 any io
#endif
            0xff, //cs any io(主机不操作cs)
        },
        .role = TCFG_HW_SPI2_ROLE,//SPI_ROLE_MASTER,
        .clk  = TCFG_HW_SPI2_BAUD,
        .mode = TCFG_HW_SPI2_MODE,//SPI_MODE_BIDIR_1BIT,//SPI_MODE_UNIDIR_2BIT,
        .bit_mode = SPI_FIRST_BIT_MSB,
        .cpol = 0,//clk level in idle state:0:low,  1:high
        .cpha = 0,//sampling edge:0:first,  1:second
        .ie_en = 0, //ie enbale:0:disable,  1:enable
        .irq_priority = 3,
        .spi_isr_callback = NULL,  //spi isr callback
    },
#endif
#endif
};
#endif


#if TCFG_NANDFLASH_DEV_ENABLE
#include "nandflash.h"
const struct nandflash_dev_platform_data nandflash_dev_data = {
    .spi_hw_num     = TCFG_FLASH_DEV_SPI_HW_NUM,
    .spi_cs_port    = TCFG_FLASH_DEV_SPI_CS_PORT,
    .spi_read_width = TCFG_FLASH_DEV_FLASH_READ_WIDTH,//flash读数据的线宽
    .start_addr     = 0,
    .size           = 128 * 1024 * 1024,
#if (TCFG_FLASH_DEV_SPI_HW_NUM == 1)
    .spi_pdata      = &spix_p_data[1],
#elif (TCFG_FLASH_DEV_SPI_HW_NUM == 2)
    .spi_pdata      = &spix_p_data[2],
#endif
};


#endif


REGISTER_DEVICES(device_table) = {
#if TCFG_SD0_ENABLE
    { "sd0", 	&sd_dev_ops,	(void *) &sd0_data},
#endif
#if TCFG_APP_LINEIN_EN
    { "linein",  &linein_dev_ops, (void *) &linein_data},
#endif
#if TCFG_UDISK_ENABLE
    { "udisk0",   &mass_storage_ops, NULL},
#endif

#if TCFG_APP_RTC_EN
    { "rtc",   &rtc_dev_ops, (void *) &rtc_data},
#endif

#if TCFG_NANDFLASH_DEV_ENABLE
    {"nand_flash",   &nandflash_dev_ops, (void *) &nandflash_dev_data},
    {"nandflash_ftl",   &ftl_dev_ops, NULL },
#endif

};
