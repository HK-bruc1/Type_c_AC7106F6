#include "jldtp_manager.h"
#include "uart_transport.h"
#include "cpu/periph/gpio.h"
#include "system/task.h"

#define JLDTP_SPEED_TEST 0

#ifndef JLDTP_NUM
#define JLDTP_NUM  1
#endif

#ifndef JLDTP_MC_CHANNEL_NUM
#define JLDTP_MC_CHANNEL_NUM    4
#endif

#define MTU_SIZE        (1024 + 16)
#define UART_BAUDRATE   4000000
#define MAX_TRANS_TIME  ((MTU_SIZE * 10) / (UART_BAUDRATE / 1000) + 10)

static jldtp_handle_t       dt_handle[JLDTP_NUM];
static jldtp_mc_handle_t    mc_handle[JLDTP_NUM];
static jldtp_transport_t    transport[JLDTP_NUM];

static int jldtp_manager_get_role()
{
    int role = JLDTP_MASTER;

#if JLDTP_SPEED_TEST
    gpio_set_mode(IO_PORT_SPILT(IO_PORTB_03), PORT_INPUT_PULLDOWN_10K);
    os_time_dly(1);
    if (gpio_read(IO_PORTB_03)) {
        role = JLDTP_MASTER;
        r_printf("jldtp_role: master\n");
    } else {
        role = JLDTP_SLAVE;
        r_printf("jldtp_role: slave\n");
    }
    gpio_set_mode(IO_PORT_SPILT(IO_PORTB_03), PORT_HIGHZ);
#endif
    return role;
}


int jldtp_manager_init(u8 handle_id)
{
    if (handle_id == JLDTP_WITH_CHIP0) {

        int role = jldtp_manager_get_role();

        jldtp_config_t config = {
            .role           = role,
            .poll_interval  = 1000,
            .max_retries    = 20,
            .max_trans_time = MAX_TRANS_TIME,
            .mtu_size       = MTU_SIZE,
            .task_name      = "app_core",
        };
        dt_handle[0] = jldtp_create(&config);

        const jldtp_uart_config_t uart_config = {
            .rx_pin     = IO_PORTC_01,
            .tx_pin     = IO_PORTC_01,
            .baud_rate  = UART_BAUDRATE,
            .mtu_size   = MTU_SIZE,
        };
        jldtp_register_transport(dt_handle[0], &jldtp_uart_transport, &uart_config);

        mc_handle[0] = jldtp_mc_create(dt_handle[0], JLDTP_MC_CHANNEL_NUM);

        jldtp_start(dt_handle[0]);
        r_printf("jldtp_start\n");
    }

    return 0;
}

void jldtp_manager_deinit(void)
{
    for (int i = 0; i < JLDTP_NUM; i++) {
        if (mc_handle[i] != NULL) {
            jldtp_mc_destroy(mc_handle[i]);
            mc_handle[i] = NULL;
        }
    }
}

jldtp_mc_handle_t jldtp_manager_get_mc_handle(u8 handle_id)
{
    if (handle_id >= JLDTP_NUM) {
        return NULL;
    }
    if (mc_handle[handle_id] == NULL) {
        jldtp_manager_init(handle_id);
    }
    return mc_handle[handle_id];
}



#if JLDTP_SPEED_TEST
#include "system/init.h"
#include "system/timer.h"
#include "generic/jiffies.h"

static jldtp_channel_handle_t test_channel;
static u8 test_buf[2048] __attribute__((aligned(4)));
static u32 tx_seqn = 1;
static u32 rx_seqn = 1;
static u32 start_time = 0;
static u16 tx_timer;

static void test_channel_data_handler(void *user_data, jldtp_channel_handle_t channel, u8 *data, u16 len)
{
    u32 *value = (u32 *)data;
    if (value[0] != rx_seqn) {
        r_printf("jldtp_rx_err: seqn %d, %d\n", value[0], rx_seqn);
    } else {
        if ((value[0] % 100) == 0) {
            int time = jiffies_offset_to_msec(start_time, jiffies);
            int speed = (100 * 1024 * 8) / time;
            start_time = jiffies;
            g_printf("jldtp_rx_suss: %d, %dkbps\n", value[0], speed);
        }
    }
    rx_seqn = value[0] + 1;
}

void jldtp_channel_test_send_data()
{
    u32 *buf = (u32 *)test_buf;

    if (start_time == 0) {
        start_time = jiffies;
    }
    buf[0] = tx_seqn;
    jldtp_mc_result_t err = jldtp_mc_send_data(test_channel, test_buf, 1024);
    if (err == JLDTP_MC_SUCCESS) {
        tx_seqn++;
    } else {
        //    putchar('F');
    }
    if (tx_seqn % 5000 == 0) {
        sys_hi_timer_modify(tx_timer, 5000);
    } else if (tx_seqn % 5000 == 1) {
        sys_hi_timer_modify(tx_timer, 4);
    }
}


int jldtp_channel_test_init()
{
    puts("jldtp_channel_test_init\n");
    jldtp_mc_handle_t mc_handle = jldtp_manager_get_mc_handle(JLDTP_WITH_CHIP0);

    jldtp_channel_config_t config = {
        .channel_id = 5,
        .priority = 0,
        .max_tx_buf_size = 4 * 1024,
    };
    test_channel = jldtp_mc_open_channel(mc_handle, &config);

    jldtp_mc_register_data_callback(test_channel, NULL, test_channel_data_handler);
    if (jldtp_mc_get_role(mc_handle) == JLDTP_MASTER) {
        tx_timer = sys_hi_timer_add(NULL, jldtp_channel_test_send_data, 4);
    }
    return 0;
}
late_initcall(jldtp_channel_test_init);

#endif
