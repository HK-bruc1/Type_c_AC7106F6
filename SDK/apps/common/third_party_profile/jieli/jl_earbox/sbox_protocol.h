/*
 * @Author:  shenzihao@zh-jieli.com
 * @Date: 2025-03-29 11:32:35
 * @LastEditors:  shenzihao@zh-jieli.com
 * @LastEditTime: 2025-03-31 09:25:01
 * @FilePath: \SDK\apps\common\third_party_profile\jl_earbox\sbox_protocol.h
 * @Description:
 *
 * Copyright (c) 2025 by zh_jieli, All Rights Reserved.
 */
#ifndef _CUSTOM_PROTOCOL_H_
#define _CUSTOM_PROTOCOL_H_

#include "system/includes.h"


enum {
    SBOX_EVENT_PHONE_CONNECT = 0,
    SBOX_EVENT_PHONE_DISCONNECT,
    SBOX_EVENT_BLE_CONNECT,
    SBOX_EVENT_BLE_DISCONNECT,
    SBOX_EVENT_TWS_CONNECT,
    SBOX_EVENT_TWS_DISCONNECT,
    SBOX_EVENT_TWS_ROLE_SWITCH,
    SBOX_EVENT_BAT_MODIFY,
    SBOX_EVENT_MUSIC_STATUS,
    SBOX_EVENT_EQ_SWITCH,
    SBOX_EVENT_ANC_SWITCH,
};

void custom_demo_all_init(void);
void custom_demo_all_exit(void);
int custom_demo_adv_enable(u8 enable);
int custom_demo_ble_send(u8 *data, u32 len);
int custom_demo_spp_send(u8 *data, u32 len);

void sbox_demo_all_init(void);
void sbox_demo_all_exit(void);

#endif


