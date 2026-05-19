#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".cfg_tool_cdc.data.bss")
#pragma data_seg(".cfg_tool_cdc.data")
#pragma const_seg(".cfg_tool_cdc.text.const")
#pragma code_seg(".cfg_tool_cdc.text")
#endif
#include "cfg_tool_cdc.h"
#include "app_config.h"
#include "cfg_tool.h"
#if TCFG_USB_SLAVE_CDC_ENABLE
#include "usb/device/cdc.h"
#endif

#if TCFG_CFG_TOOL_ENABLE && (TCFG_COMM_TYPE == TCFG_USB_COMM)

#if TCFG_TUNING_WITH_DUAL_DEVICE_ENBALE

#include "uart.h"
#include "system/init.h"

#define UART_DMA_LEN        0x800
#define UART_DEFAULT_BAUD   1000000


static int uart_num;
static u32 uart_dma_rx_buf[UART_DMA_LEN / 4];
static u32 uart_dma_tx_buf[256 / 4];
static u32 g_cfg_tool_cdc_id;

static void read_uart_data(int arg)
{
    u8 tx_buf[128];
    if (uart_num == -1) {
        return;
    }
    u8 *send_buf = tx_buf;
    int len = uart_get_recv_len(uart_num);
    if (len > sizeof(tx_buf)) {
        send_buf = malloc(len);
    }
    //printf("rx: %d\n", len);
    u32 ret = uart_recv_blocking(uart_num, send_buf, len, 10);
    cdc_write_data(0, send_buf, len, g_cfg_tool_cdc_id);
    if (len > sizeof(tx_buf)) {
        free(send_buf);
    }
}


static void uart_irq_callback(int uart_num, enum uart_event event)
{
    if (event & (UART_EVENT_RX_DATA | UART_EVENT_RX_TIMEOUT)) {
        int msg[3] = { (int)read_uart_data, 1, 0 };
        os_taskq_post_type("app_core", Q_CALLBACK, 3, msg);
    }

    if (event & UART_EVENT_RX_FIFO_OVF) {
        printf("uart[%d] rx fifo ovf", uart_num);
    }
}



static void uart_hw_try_init(void *p)
{
#if TCFG_UPDATE_UART_IO_EN
    extern bool get_uart_update_sta(void);
    if (get_uart_update_sta()) {
        return;
    }
#endif

    struct uart_config uart_up_config = {-1};
    uart_up_config.baud_rate = UART_DEFAULT_BAUD;
    uart_up_config.tx_pin = TCFG_ONLINE_TX_PORT;
    uart_up_config.rx_pin = TCFG_ONLINE_RX_PORT;

    memset(uart_dma_rx_buf, 0, UART_DMA_LEN);
    struct uart_dma_config uart_up_dma = {
        .rx_timeout_thresh = 1000,
        .frame_size = UART_DMA_LEN,
        .event_mask = UART_EVENT_RX_DATA | UART_EVENT_RX_FIFO_OVF | UART_EVENT_RX_TIMEOUT,
        .irq_callback = uart_irq_callback,
        .rx_cbuffer = uart_dma_rx_buf,
        .rx_cbuffer_size = UART_DMA_LEN,
    };

    uart_num = uart_init(-1, &uart_up_config);
    printf(">>>[test]:init uart = %d\n", uart_num);
    if (uart_num < 0) {
        printf("uart_init error %d", uart_num);
        return;
    }
    int r = uart_dma_init(uart_num, &uart_up_dma);
    if (r < 0) {
        printf("uart_dma_init error %d", r);
    }
}

static int dual_device_tuning_init()
{
#if TCFG_UPDATE_UART_IO_EN
#if (TCFG_ONLINE_TX_PORT == TCFG_UART_UPDTAE_TX_PIN) || \
    (TCFG_ONLINE_TX_PORT == TCFG_UART_UPDTAE_RX_PIN)
    sys_timeout_add(NULL, uart_hw_try_init, 8000);
    return 0;
#endif
#endif
    uart_hw_try_init(NULL);
    return 0;
}
late_initcall(dual_device_tuning_init);


#endif

/**
 *	@brief 获取工具cdc最大支持的协议包大小
 */
u16 cfg_tool_cdc_rx_max_mtu()
{
    return 1024 * 4 + 22;
}


/**
 *	@brief 获取cdc的配置/调音工具相关数据
 */
void cfg_tool_data_from_cdc(u8 *buf, u32 rlen, u8 id) // in irq
{
    /* printf("cfg_tool cdc rx:\n"); */
    /* put_buf(buf, rlen); */
    if (id == 0) {
        cfg_tool_combine_rx_data(buf, rlen);
    } else {
#if TCFG_TUNING_WITH_DUAL_DEVICE_ENBALE
        if (uart_num == -1) {
            return;
        }
        /*printf("cdc: %d, %d\n", id, rlen);*/
        g_cfg_tool_cdc_id = id;
        u8 *tx_buf = (u8 *)uart_dma_tx_buf;
        if (rlen > sizeof(uart_dma_tx_buf)) {
            tx_buf = dma_malloc(rlen);
        }
        memcpy(tx_buf, buf, rlen);
        uart_send_blocking(uart_num, tx_buf, rlen, 20);
        if (rlen > sizeof(uart_dma_tx_buf)) {
            dma_free(tx_buf);
        }
#endif
    }
}



#endif

