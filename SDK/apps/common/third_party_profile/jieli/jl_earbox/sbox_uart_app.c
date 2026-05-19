/*
 * @Author: shenzihao@zh-jieli.com
 * @Date: 2024-05-17 14:49:01
 * @LastEditors:  shenzihao@zh-jieli.com
 * @LastEditTime: 2025-04-01 16:31:03
 * @FilePath: \SDK\apps\common\third_party_profile\jl_earbox\sbox_uart_app.c
 * @Description:
 *
 * Copyright (c) 2024 by ${git_name_email}, All Rights Reserved.
 */
#include "typedef.h"
#include "app_config.h"
#include "sbox_core_config.h"
#include "chargestore/chargestore.h"
#include "user_cfg.h"

#define CMD_CUSTOM_DEAL             0xAA            //彩屏仓自定义命令
#define UART_PROFILE_HEAD           0xAA
#define UART_PROFILE_ENDING         0xBB
#define UART_PROFILE_HEAD_INDEX     0
#define UART_PROFILE_CMD_INDEX      1
#define UART_PROFILE_DATA_INDEX     2



#if SBOX_UART_FUNCTION

enum {
    EARPHONE_MAC_MSG = 0x31,    //earphone mac event
};

void Send_Mac(u8 event, u8 *addr, u8 len)
{
    printf("Send_Mac\n");
    put_buf(addr, 6);
    unsigned char temp[9];
    unsigned short j = 0;
    temp[0] = CMD_CUSTOM_DEAL; //数据包头
    temp[1] = event;
    memcpy(&temp[2], addr, 6);
    u16 sum = 0;
    for (u8 i = 0; i < (sizeof(temp) - 1); i++) {
        sum += temp[i];
    }
    temp[sizeof(temp) - 1] = (sum) & 0xff; //表示一次传输完成
    put_buf(temp, sizeof(temp));
    chargestore_api_write(temp, sizeof(temp));
}


/*放到智能充电仓的单线串口数据接收接口里面*/
static int user_app_chargestore_data_deal(u8 *buf, u8 len)
{
    // printf_hexdump(buf, len);
    switch (buf[0]) {
    case CMD_CUSTOM_DEAL:
        put_buf(buf, len);
        u16 sum = 0;
        for (u8 i = 0; i < len - 1; i++) {
            sum += buf[i];
        }
        u8 check = sum & 0xFF;
        printf("sscheck:%d buf[len-1]:%d  %d\n", check, buf[len - 1], buf[UART_PROFILE_CMD_INDEX]);
        if (buf[len - 1] != check) {
            printf("err check\n");
            return 0;
        }
        switch (buf[UART_PROFILE_CMD_INDEX]) {
        case 0x10:
            Send_Mac(EARPHONE_MAC_MSG, (u8 *)bt_get_mac_addr(), 6);
            printf("充电仓发送mac地址过来\n");
            return 1;
        default:
            break;
        }
        break;
    default:
        return 0;
    }
    return 0;
}
CHARGESTORE_HANDLE_REG(sbox_uart, user_app_chargestore_data_deal);


#endif
