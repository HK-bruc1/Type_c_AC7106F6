
#include "app_config.h"
#include "clock.h"
#include "eartouch_manage.h"

#if(TCFG_OUTSIDE_EARTCH_ENABLE == 1)

#define LOG_TAG             "[IRSENSOR]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

#define TCFG_IRSENSOR_IIC_HDL            0

#if TCFG_IRSENOR_USE_HW_IIC
#define _IIC_USE_HW//硬件iic
#endif
#include "iic_api.h"

static u8 iic_hdl = TCFG_IRSENSOR_IIC_HDL;

#define IIC_DELAY       50

u8 irSensor_iic_write(u8 w_chip_id, u8 register_address, u8 function_command)
{
    //spin_lock(&iic_lock);
    u8 ret = 1;
    iic_start(iic_hdl);
    if (0 == iic_tx_byte(iic_hdl, w_chip_id)) {
        ret = 0;
        log_e("\n JSA iic wr err 0\n");
        goto __gcend;
    }

    delay_nops(IIC_DELAY);

    if (0 == iic_tx_byte(iic_hdl, register_address)) {
        ret = 0;
        log_e("\n JSA iic wr err 1\n");
        goto __gcend;
    }

    delay_nops(IIC_DELAY);

    if (0 == iic_tx_byte(iic_hdl, function_command)) {
        ret = 0;
        log_e("\n JSA iic wr err 2\n");
        goto __gcend;
    }

    g_printf("JSA iic wr succ\n");
__gcend:
    iic_stop(iic_hdl);

    //spin_unlock(&iic_lock);
    return ret;
}


u8 irSensor_iic_read(u8 r_chip_id, u8 register_address, u8 *buf, u8 data_len)
{
    u8 read_len = 0;

    //spin_lock(&iic_lock);

    iic_start(iic_hdl);
    if (0 == iic_tx_byte(iic_hdl, r_chip_id - 1)) {
        log_e("\n JSA iic rd err 0\n");
        read_len = 0;
        goto __gdend;
    }


    delay_nops(IIC_DELAY);
    if (0 == iic_tx_byte(iic_hdl, register_address)) {
        log_e("\n JSA iic rd err 1\n");
        read_len = 0;
        goto __gdend;
    }

    iic_start(iic_hdl);
    if (0 == iic_tx_byte(iic_hdl, r_chip_id)) {
        log_e("\n JSA iic rd err 2\n");
        read_len = 0;
        goto __gdend;
    }

    delay_nops(IIC_DELAY);

    for (; data_len > 1; data_len--) {
        *buf++ = iic_rx_byte(iic_hdl, 1);
        read_len ++;
    }

    *buf = iic_rx_byte(iic_hdl, 0);
    read_len ++;

__gdend:

    iic_stop(iic_hdl);
    delay_nops(IIC_DELAY);

    //spin_unlock(&iic_lock);
    return read_len;
}

int irSensor_iic_init(eartouch_platform_data *data)
{
    struct iic_master_config ir_iic_cfg = {
        .role = IIC_MASTER,
        .scl_io = data->iic_clk,
        .sda_io = data->iic_dat,
        .io_mode = PORT_INPUT_PULLUP_10K,      //上拉或浮空，如果外部电路没有焊接上拉电阻需要置上拉
        .hdrive = PORT_DRIVE_STRENGT_2p4mA,    //IO口强驱
#if TCFG_IRSENOR_USE_HW_IIC
        .master_frequency = TCFG_IRSENSOR_HW_IIIC_FRE,  //IIC通讯波特率 未使用
#endif
        .io_filter = 0,                        //软件iic无滤波器
    };
    return iic_init(iic_hdl, &ir_iic_cfg);
}

#endif
