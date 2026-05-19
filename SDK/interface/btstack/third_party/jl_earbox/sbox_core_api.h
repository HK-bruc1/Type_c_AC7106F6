#ifndef __SBOX_CORE_API_H__
#define __SBOX_CORE_API_H__

#include "typedef.h"

#define SEND_BUF_MAX_LEN            256                 //彩屏仓协议定义的数据传输buf大小，必须为16的倍数
#define SBOX_CORE_DEBUG             (BIT(0))           //协议库打印

struct box_info {
    u8 cmd;
    u8 data[SEND_BUF_MAX_LEN];
    u16 lens;
};

typedef struct {
    // u8 local_ble_addr[6];
    // u8 (*get_box_ble_addr)(void);
    // void (*set_box_ble_addr)(void *data);
    unsigned short sbox_ble_send_handle;
    char  sbox_handle_type;
    int (*sbox_ble_send_data)(u8 *data, u32 len);
} s_box_func_hdl;

extern s_box_func_hdl sbox_func_hdl;
extern const u8 sbox_encrypt_mode_aes;
/**
 * @brief               往对应BLE服务的handle，按照指定方式去发送数据
 * @param[in] cmd       命令
 * @param[in] data      命令的参数
 * @param[in] len       命令参数的长度
 * @brief:              无
 */
void sbox_ble_att_send_data(u8 cmd, u8 *data, u16 len);
/**
 * @brief                彩屏仓私有协议数据处理handle
 * @param[in] buffer     命令的参数
 * @param[in] buffer_size命令参数的长度
 * @return               无
 */
int sbox_att_write_without_rsp_handler(uint8_t *buffer, uint16_t buffer_size);

extern void sys_smartstore_notify_event(struct box_info *boxinfo);

#endif
