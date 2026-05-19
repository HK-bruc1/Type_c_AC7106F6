#include "uart_transport.h"
#include "cpu/periph/uart.h"


#define UART_NUM            2
#define IRQ_PRIORITY        3
#define RX_TIMEOUT_BYTES    3



struct uart_handle {
    int uart_id;
    u8 *rx_dma_buffer;
    u8 *tx_dma_buffer;
    u16 tx_dma_buffer_size;
    jldtp_handle_t jldtp_handle;
};

static struct uart_handle g_handle[UART_NUM];


static void uart_transport_cleanup(struct uart_handle *handle);

static void uart_irq_callback(int uart_num, enum uart_event event)
{
    struct uart_handle *handle = NULL;

    for (int i = 0; i < UART_NUM; i++) {
        if (g_handle[i].uart_id == uart_num) {
            handle = &g_handle[i];
        }
    }
    if (!handle) {
        return;
    }

    if (event & UART_EVENT_TX_DONE) {
        /*puts("uart_event_tx_done\n");*/
        jldtp_notify_tx_complete(handle->jldtp_handle, 1);
    }

    if (event & UART_EVENT_RX_DATA) {
        /*puts("uart_event_rx_data\n");*/
        jldtp_notify_rx_ready(handle->jldtp_handle);
    }
    if (event & UART_EVENT_RX_TIMEOUT) {
        /*puts("uart_rx_timeout\n");*/
        jldtp_notify_rx_ready(handle->jldtp_handle);
    }
}

static void *transport_uart_open(jldtp_handle_t jldtp_handle, const void *arg)
{
    struct uart_handle *handle = NULL;

    for (int i = 0; i < UART_NUM; i++) {
        if (g_handle[i].jldtp_handle == NULL) {
            g_handle[i].jldtp_handle = jldtp_handle;
            handle = &g_handle[i];
            break;
        }
    }
    if (!handle) {
        return NULL;
    }

    const jldtp_uart_config_t *config = (const jldtp_uart_config_t *)arg;

    struct uart_config uart_cfg = {
        .baud_rate      = config->baud_rate,
        .tx_pin         = config->tx_pin,
        .rx_pin         = config->rx_pin,
        .parity         = UART_PARITY_DISABLE,
        .tx_wait_mutex  = 0,  // 支持中断，不互斥
    };

    handle->uart_id = uart_init(-1, &uart_cfg);
    if (handle->uart_id < 0) {
        printf("UART init failed: %d\n", handle->uart_id);
        goto error_cleanup;
    }
    int rx_buffer_size = config->mtu_size * 2;

    /* 初始化DMA */
    handle->rx_dma_buffer = dma_malloc(rx_buffer_size);
    if (!handle->rx_dma_buffer) {
        printf("Failed to allocate RX DMA buffer, size: %d\n", rx_buffer_size);
        goto error_cleanup;
    }

    /* 计算RX超时阈值（单位：us） */
    u32 rx_timeout_thresh = RX_TIMEOUT_BYTES * 10000000 / config->baud_rate;

    /* 配置DMA参数 */
    struct uart_dma_config dma_config = {
        .rx_timeout_thresh  = rx_timeout_thresh,
        .event_mask         = UART_EVENT_TX_DONE | UART_EVENT_RX_DATA |
        UART_EVENT_RX_FIFO_OVF | UART_EVENT_RX_TIMEOUT,
        .irq_priority       = IRQ_PRIORITY,
        .irq_callback       = uart_irq_callback,
        .rx_cbuffer         = handle->rx_dma_buffer,
        .rx_cbuffer_size    = rx_buffer_size,
        .frame_size         = config->mtu_size,
    };

    int ret = uart_dma_init(handle->uart_id, &dma_config);
    if (ret < 0) {
        printf("UART(%d) DMA init error: %d\n", handle->uart_id, ret);
        goto error_cleanup;
    }

    return handle;

error_cleanup:
    uart_transport_cleanup(handle);
    return NULL;
}


static int transport_uart_read(void *_handle, u8 *buffer, int len)
{
    struct uart_handle *handle = (struct uart_handle *)_handle;
    return uart_recv_bytes(handle->uart_id, buffer, len);
}

static int transport_uart_write(void *_handle, u8 *buffer, int len)
{
    struct uart_handle *handle = (struct uart_handle *)_handle;

    /* 检查是否需要重新分配TX缓冲区 */
    if (handle->tx_dma_buffer && handle->tx_dma_buffer_size < len) {
        dma_free(handle->tx_dma_buffer);
        handle->tx_dma_buffer = NULL;
        handle->tx_dma_buffer_size = 0;
    }

    /* 分配TX缓冲区（如果需要） */
    if (!handle->tx_dma_buffer) {
        handle->tx_dma_buffer_size = (len + 3) / 4 * 4;
        handle->tx_dma_buffer = dma_malloc(handle->tx_dma_buffer_size);
        if (!handle->tx_dma_buffer) {
            printf("Failed to allocate TX DMA buffer, size: %d\n",
                   handle->tx_dma_buffer_size);
            return 0;
        }
    }

    /* 拷贝数据并发送 */
    memcpy(handle->tx_dma_buffer, buffer, len);
    return uart_send_bytes(handle->uart_id, handle->tx_dma_buffer, len);
}

static void uart_transport_cleanup(struct uart_handle *handle)
{
    if (handle->uart_id >= 0) {
        uart_deinit(handle->uart_id);
    }

    if (handle->rx_dma_buffer) {
        dma_free(handle->rx_dma_buffer);
    }

    if (handle->tx_dma_buffer) {
        dma_free(handle->tx_dma_buffer);
    }
    for (int i = 0; i < UART_NUM; i++) {
        if (&g_handle[i] == handle) {
            g_handle[i].jldtp_handle = NULL;
            break;
        }
    }
}

static void transport_uart_close(void *handle)
{
    uart_transport_cleanup((struct uart_handle *)handle);
}

static void transport_uart_reset(void *handle)
{
    puts("transport_uart_reset\n");
}

const jldtp_transport_t jldtp_uart_transport = {
    .open = transport_uart_open,
    .read = transport_uart_read,
    .write = transport_uart_write,
    .close = transport_uart_close,
    .reset = transport_uart_reset,
};




