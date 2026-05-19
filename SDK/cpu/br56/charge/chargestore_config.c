#include "app_config.h"
#include "chargestore/chargestore.h"
#include "asm/charge.h"
#include "gpio_config.h"
#include "system/init.h"
#include "asm/power_interface.h"
#include "gpio.h"

//br56只有两路串口,以下情况的时候不初始化电源仓的串口，防止占用：1、串口在线调试；2、FCC和BQB测试；3、串口读卡功能
#if (CONFIG_CPU_BR56) && \
    ((TCFG_COMM_TYPE == TCFG_UART_COMM) && (TCFG_CFG_TOOL_ENABLE == 1) || \
    (CONFIG_BT_MODE != BT_NORMAL) || \
    (TCFG_AUDIO_DATA_EXPORT_DEFINE == AUDIO_DATA_EXPORT_VIA_UART))

#elif (TCFG_CHARGESTORE_ENABLE || TCFG_TEST_BOX_ENABLE || TCFG_ANC_BOX_ENABLE)

#if ((TCFG_NORMAL_SET_DUT_MODE && TCFG_USER_BLE_ENABLE) && (TCFG_DEBUG_UART_ENABLE))  //br56串口有限，需要测试ble的rf指标时，请选择关闭充电脚串口(无法串口升级)或者关闭串口调试
#error BR56 serial quantity limit !!!
#endif

static void chargestore_wakeup_callback(P33_IO_WKUP_EDGE edge)
{
    chargestore_ldo5v_fall_deal();
}

static const struct _p33_io_wakeup_config port1 = {
    .pullup_down_mode   = PORT_INPUT_FLOATING,      //配置I/O 内部上下拉是否使能
    .edge               = FALLING_EDGE,             //唤醒方式选择,可选：上升沿\下降沿
    .filter      		= PORT_FLT_1ms,
    .gpio               = TCFG_CHARGESTORE_PORT,    //唤醒口选择
    .callback			= chargestore_wakeup_callback,
};

static const struct chargestore_platform_data chargestore_data = {
    .io_port                = TCFG_CHARGESTORE_PORT,
    .baudrate               = 9600,
    .init                   = chargestore_init,
    .open                   = chargestore_open,
    .close                  = chargestore_close,
    .write                  = chargestore_write,
};

__INITCALL_BANK_CODE
int board_chargestore_config()
{
    p33_io_wakeup_port_init(&port1);
    p33_io_wakeup_enable(TCFG_CHARGESTORE_PORT, 1);

    chargestore_api_init(&chargestore_data);
    return 0;
}
static void board_chargestore_not_config()
{
    p33_io_wakeup_enable(TCFG_CHARGESTORE_PORT, 0);
}

platform_initcall(board_chargestore_config);

platform_uninitcall(board_chargestore_not_config);

#endif
