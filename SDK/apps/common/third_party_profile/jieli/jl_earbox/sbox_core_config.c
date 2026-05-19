/*
 * @Author: shenzihao@zh-jieli.com shenzihao@zh-jieli.com
 * @Date: 2024-09-27 11:01:23
 * @LastEditors:  shenzihao@zh-jieli.com
 * @LastEditTime: 2025-04-21 19:31:45
 * @FilePath: \SDK\apps\common\third_party_profile\jl_earbox\sbox_core_config.c
 * @Description:
 * 此文件是配置彩屏仓通信协议相关的配置：
 *      1.是否打开数据保护，防止数据踩踏
 *      2.BLE通信BUF的大小定义，需要和仓同步这个大小，需要 耳机buf >= 仓buf
 *      3.仓通信的profile handle 的定义,ble发数类型的定义，具体发数接口的载入
 *      4.协议库底层通知事件的定义，不同SDK接口不一致，需要根据SDK具体情况去改写
 * Copyright (c) 2024 by ${git_name_email}, All Rights Reserved.
 */
#include "sbox_core_config.h"
#include "sbox_user_app.h"
#include "event.h"
#include "app_main.h"
#include "system/includes.h"
#include "app_msg.h"
// #include "circular_buf.h"
#include "le/ble_api.h"
#include "app_ble_spp_api.h"

extern void *sbox_demo_ble_hdl;

const u8 sbox_encrypt_mode_aes = 1;  //彩屏仓加密使用aes？1为使用aes，0为使用xor

int custom_send_data_to_box(u8 *data, u32 len);
s_box_func_hdl sbox_func_hdl = {
    // .local_ble_addr = {1,2,3,4,5,6},
    // .set_box_ble_addr = sbox_set_local_ble_addr,
    // .get_box_ble_addr = sbox_get_local_ble_addr,
    .sbox_ble_send_handle = ATT_CHARACTERISTIC_ae99_01_VALUE_HANDLE,
    .sbox_handle_type = ATT_OP_NOTIFY,
    .sbox_ble_send_data = custom_send_data_to_box,
};

int custom_send_data_to_box(u8 *data, u32 len)
{
    put_buf(data, len);
    int ret = app_ble_att_send_data(sbox_demo_ble_hdl, sbox_func_hdl.sbox_ble_send_handle, data, len, sbox_func_hdl.sbox_handle_type);
    if (ret) {
        printf("send fail\n");
    }
    return ret;
}

//1.数据保护
#if SBOX_OPEN_DATA_PROTECT
/*--------------------------------------------------sbox cbuf------------------------------------------------------------------------*/
//cbuf交替处理协议数据，避免数据踩踏

u8 sbox_tr_rbuf[SEND_BUF_MAX_LEN] __attribute__((aligned(4)));//线程读取
static u8 sbox_tr_cbuf[SBOX_TR_BUF_LEN] __attribute__((aligned(4)));//缓存使用,可以扩大几倍

///cbuf缓存多包串口中断接收数据
static cbuffer_t sbox_tr_cbuft;
#define cbuf_get_space(a) (a)->total_len
typedef struct {
    u8   sbox_id;
    u32  rx_len;
    u8   rx_cmd;
} __attribute__((packed)) sbox_tr_head_t;

static int sbox_tr_cbuf_is_write_able(u32 len)
{
    u32 wlen, buf_space;
    /* OS_ENTER_CRITICAL(); */
    /* cbuf_is_write_able() */
    buf_space = cbuf_get_space(&sbox_tr_cbuft) - cbuf_get_data_size(&sbox_tr_cbuft);
    if (buf_space < len) {
        wlen = 0;
    } else {
        wlen = len;
    }
    /* OS_EXIT_CRITICAL(); */
    printf("%s[%d %d %d]", __func__, len, buf_space, wlen);
    return wlen;
}

int sbox_tr_ibuf_to_cbuf(u8 *buf, u32 len, u8 cmd)
{
    int ret = 0;
    u16 wlen = 0;
    u16 head_size = sizeof(sbox_tr_head_t);
    sbox_tr_head_t rx_head;
    rx_head.sbox_id = 1; //use uart1
    OS_ENTER_CRITICAL();
    if (sbox_tr_cbuf_is_write_able(len + head_size)) {
        rx_head.rx_len = len;
        rx_head.rx_cmd = cmd;
        wlen = cbuf_write(&sbox_tr_cbuft, &rx_head, head_size);
        wlen += cbuf_write(&sbox_tr_cbuft, buf, rx_head.rx_len);
        if (wlen != (rx_head.rx_len + head_size)) {
            ret = -1;
        } else {
            ret = wlen;
        }
    } else {
        ret = -2;
    }
    OS_EXIT_CRITICAL();
    printf("%s[%d %d %d]", __func__, len, head_size, ret);
    return ret;
}

int sbox_tr_cbuf_read(u8 *buf, u32 *len, u8 *cmd)
{
    int ret = 0;
    u16 rlen = 0;
    u16 head_size = sizeof(sbox_tr_head_t);
    sbox_tr_head_t rx_head = {0};
    OS_ENTER_CRITICAL();
    if (0 == cbuf_get_data_size(&sbox_tr_cbuft)) {
        /* OS_EXIT_CRITICAL(); */
        ret =  -1;
    } else {
        rlen = cbuf_read(&sbox_tr_cbuft, &rx_head, sizeof(sbox_tr_head_t));
        if (rlen != sizeof(sbox_tr_head_t)) {
            ret =  -2;
        } else if (rx_head.rx_len) {
            rlen = cbuf_read(&sbox_tr_cbuft, buf, rx_head.rx_len);
            if (rlen != rx_head.rx_len) {
                ret =  -3;
            } else {
                *len = rx_head.rx_len;
                *cmd = rx_head.rx_cmd;
            }
        }
    }
    OS_EXIT_CRITICAL();
    printf("%s[%d 0x%x %d %x]", __func__, ret, *len, rx_head.rx_len, *cmd);
    return ret;
}

void sbox_tr_cbuf_init(void)
{
    cbuf_init(&sbox_tr_cbuft, sbox_tr_cbuf, sizeof(sbox_tr_cbuf));
}
#endif


//仓协议推送事件到app_core执行
void sys_smartstore_notify_event(struct box_info *boxinfo)
{
    int ret = sbox_tr_ibuf_to_cbuf(boxinfo->data, boxinfo->lens, boxinfo->cmd);
    if (ret < 0) {
        printf("%s[%s]", __func__, "sbox_tr_ibuf_to_cbuf faild");
    }
    int err;
    int msg[3];
    msg[0] = (int)sys_smartstore_event_handle;
    msg[1] = 1;
    msg[2] = (int)(boxinfo);
    err = os_taskq_post_type("app_core", Q_CALLBACK, 3, msg);
    if (err) {
        y_printf("send error\n");
    }
}
