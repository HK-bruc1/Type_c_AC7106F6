#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".debug_uart_config.data.bss")
#pragma data_seg(".debug_uart_config.data")
#pragma const_seg(".debug_uart_config.text.const")
#pragma code_seg(".debug_uart_config.text")
#endif
#include "app_config.h"
#include "cpu/includes.h"
#include "gpio_config.h"
#include "system/init.h"

#if CONFIG_DEBUG_ENABLE || CONFIG_DEBUG_LITE_ENABLE

#define     DEBUG_UART_NUM  0

static u8 uart_mode = 0;        //0:typical putbyte, 1:exception putbyte

__INITCALL_BANK_CODE
void debug_uart_init()
{
    const struct uart_config debug_uart_config = {
        .baud_rate = TCFG_DEBUG_UART_BAUDRATE,
        .tx_pin = TCFG_DEBUG_UART_TX_PIN,
        .rx_pin = -1,
        .tx_wait_mutex = 0,//1:不支持中断调用,互斥,0:支持中断,不互斥
    };

//br29 uart0无dma
#if 0//def CONFIG_CPU_BR29

    JL_PORTA->DIE |=  BIT(5);
    JL_PORTA->DIR &= ~BIT(5);
    JL_OMAP->PA5_OUT = FO_GP_OCH5;

    JL_IOMC->OCH_CON0 &= ~(0b11111 << 25);//uart0_tx
    JL_IOMC->IOMC0 |= BIT(3);//采用outputchannel

    JL_UART0->CON0 = BIT(13) | BIT(12);
    JL_UART0->CON0 &= ~BIT(0);
    JL_UART0->CON0 |= BIT(13) | BIT(12);
    JL_UART0->BAUD = (48000000 / TCFG_DEBUG_UART_BAUDRATE) / 4 - 1;
    JL_UART0->CON0 |= BIT(13) | BIT(12) | BIT(0);
    JL_UART0->BUF = ' ';
#else

    uart_init(DEBUG_UART_NUM, &debug_uart_config);

#endif

}


static void __putbyte(char a)
{
    /* if(a == '\n'){                          */
    /*     uart_log_putbyte(DEBUG_UART_NUM, '\r'); */
    /* }                                       */

    uart_log_putbyte(DEBUG_UART_NUM, a);
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 通用打印putbyte函数，用于SDK调试
 *
 * @param a char
 */
/* ----------------------------------------------------------------------------*/
void putbyte(char a)
{

#if 0//def CONFIG_CPU_BR29

    if (!(JL_UART0->CON0 & BIT(0))) {
        return;
    }
    u32 i = 0x10000;
    while (((JL_UART0->CON0 & BIT(15)) == 0) && (0 != i)) {  //TX IDLE
        i--;
    }
    JL_UART0->CON0 |= BIT(13);  //清Tx pending
    JL_UART0->BUF = a;
    __asm__ volatile("csync");

#else

    if (uart_mode == 0) {
        __putbyte(a);
    }

#endif
}


/* --------------------------------------------------------------------------*/
/**
 * @brief 特俗场景putbyte函数，暂用于异常服务函数
 *          调用此函数后，通用putbyte函数会停用，防止打印冲突。
 * @param a char
 */
/* ----------------------------------------------------------------------------*/
//excpt默认使用内部打印函数，使用uart0，放RAM。
//若出现异常无打印情况，考虑打开此函数，放Flash。
#if 0
void excpt_putbyte(char a)
{
    uart_mode = 1;
    __putbyte(a);
}
#endif

#else

void putbyte(char a)
{
}

#endif
