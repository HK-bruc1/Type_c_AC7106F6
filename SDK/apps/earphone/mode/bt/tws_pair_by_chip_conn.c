#include "jldtp_manager.h"
#include "bt_tws.h"
#include "app_config.h"


#if CONFIG_TWS_PAIR_MODE == CONFIG_TWS_PAIR_BY_CHIP_CONN

enum {
    TWS_PAIR_INTERNAL_ADDR = 1,
};

static u16 pair_timer;
static jldtp_channel_handle_t channel_handle;
static void  send_local_addr_timer(void *p);


void tws_pair_data_handler(void *priv_data, jldtp_channel_handle_t handle, u8 *data, u16 data_len)
{
    puts("tws_pair_data_handler\n");
    put_buf(data, data_len);
    switch (data[0]) {
    case TWS_PAIR_INTERNAL_ADDR:
        bt_tws_set_pair_suss(data + 1);
        if (pair_timer == 0) {
            send_local_addr_timer(NULL);
            pair_timer = sys_timer_add(NULL, send_local_addr_timer, 50);
        }
        break;

    }
}

static void  send_local_addr_timer(void *p)
{
    puts("send_tws_local_addr\n");
    u8 data[12];
    data[0] = TWS_PAIR_INTERNAL_ADDR;
    bt_get_tws_local_addr(data + 1);
    put_buf(data, 7);
    jldtp_mc_send_data(channel_handle, data, 7);
}

int tws_pair_by_chip_conn_init()
{
    jldtp_mc_handle_t mc_handle = jldtp_manager_get_mc_handle(JLDTP_WITH_CHIP0);
    jldtp_channel_config_t config = {
        .channel_id = JLDTP_ID_TWS_PAIR,
        .priority = 0,
        .max_tx_buf_size = 16,
    };
    channel_handle = jldtp_mc_open_channel(mc_handle, &config);
    printf("tws_channel_handle: %x\n", (u32)channel_handle);
    if (!channel_handle) {
        return -EFAULT;
    }
    jldtp_mc_register_data_callback(channel_handle, NULL, tws_pair_data_handler);
    return 0;
}

int tws_start_pair_by_chip_conn()
{
    if (!channel_handle) {
        return -EFAULT;
    }
    send_local_addr_timer(NULL);
    pair_timer = sys_timer_add(NULL, send_local_addr_timer, 50);
    return 0;
}

void tws_stop_pair_by_chip_conn()
{
    if (pair_timer) {
        sys_timer_del(pair_timer);
        pair_timer = 0;
    }
    if (channel_handle) {
        jldtp_mc_close_channel(channel_handle);
        channel_handle = NULL;
    }
}


#endif

