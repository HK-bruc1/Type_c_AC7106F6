/*********************************************************************************************
    *   Filename        : connected_api.h

    *   Description     :

    *   Author          : Weixin Liang

    *   Email           : liangweixin@zh-jieli.com

    *   Last modifiled  : 2022-12-15 14:33

    *   Copyright:(c)JIELI  2011-2022  @ , All Rights Reserved.
*********************************************************************************************/
#ifndef _CONNECTED_API_H
#define _CONNECTED_API_H

/*  Include header */
#include "app_config.h"
#include "typedef.h"
#include "system/includes.h"
#include "cig.h"
#include "wireless_trans.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief CIG角色枚举 */
enum {
    CONNECTED_ROLE_UNKNOW,
    CONNECTED_ROLE_CENTRAL,
    CONNECTED_ROLE_PERIP,
};

/*! \brief CIG同步模块结构体 */
struct connected_sync_hdl {
    struct list_head entry; /*!< 同步模块链表项，用于多同步模块管理 */
    u8 cig_hdl;         /*!< cig句柄，用于cig控制接口 */
    u8 channel_num;
    u16 seqn;
    int sample_rate;
    int connected_sample_rate;
    u32 tx_sync_time;
    OS_SEM sem;
    void *bcsync;
    void *connected_hdl;
};

/*! \brief CIG数据转发和事件同步回调函数接头体 */
struct connected_sync_channel {
    void(*event_handler)(const CIG_EVENT event, void *arg);
    int(*tx_events_suss)(void *data, int len);
    int(*rx_events_suss)(u16 acl_hdl, const void *const data, int len);
};

/*! \brief CIG接收端音频结构体 */
struct connected_rx_audio_hdl {
    void *le_audio;
    void *rx_stream;
};

/**************************************************************************************************
  Global Variables
**************************************************************************************************/
extern const cig_callback_t cig_perip_cb;
extern struct connected_sync_channel  connected_sync_channel_begin[];
extern struct connected_sync_channel  connected_sync_channel_end[];

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/* --------------------------------------------------------------------------*/
/**
 * @brief CIG主机启动接口
 *
 * @param params:CIG主机启动所需参数
 *
 * @return 分配的cig_hdl
 */
/* ----------------------------------------------------------------------------*/
int connected_central_open(cig_parameter_t *params);

/* --------------------------------------------------------------------------*/
/**
 * @brief CIG从机启动接口
 *
 * @param params:CIG从机启动所需参数
 *
 * @return 分配的cig_hdl
 */
/* ----------------------------------------------------------------------------*/
int connected_perip_open(cig_parameter_t *params);

/* --------------------------------------------------------------------------*/
/**
 * @brief 关闭对应cig_hdl的CIG连接
 *
 * @param cig_hdl:需要关闭的CIG连接的cig_hdl
 */
/* ----------------------------------------------------------------------------*/
void connected_close(u8 cig_hdl);

/* --------------------------------------------------------------------------*/
/**
 * @brief 获取当前CIG是主机还是从机
 *
 * @return CIG角色
 */
/* ----------------------------------------------------------------------------*/
u8 get_connected_role(void);

/* --------------------------------------------------------------------------*/
/**
 * @brief 初始化CIG所需的参数及流程
 *
 */
/* ----------------------------------------------------------------------------*/
void connected_init(void);

/* --------------------------------------------------------------------------*/
/**
 * @brief 复位CIG所需的参数及流程
 *
 */
/* ----------------------------------------------------------------------------*/
void connected_uninit(void);

/* --------------------------------------------------------------------------*/
/**
 * @brief acl数据发送接口
 *
 * @param acl_hdl:acl数据通道句柄
 * @param data:需要发送的数据
 * @param length:数据长度
 *
 * @return 实际发送出去的数据长度
 */
/* ----------------------------------------------------------------------------*/
int connected_send_acl_data(u16 acl_hdl, void *data, size_t length);

int connected_send_cis_data(u8 cig_hdl, void *data, size_t length);
/* --------------------------------------------------------------------------*/
/**
 * @brief CIG主机连接成功处理事件
 *
 * @param priv:连接成功附带的句柄参数
 * @param mode:当前音源模式
 *
 * @return 是否执行成功 -- 0为成功，其他为失败
 */
/* ----------------------------------------------------------------------------*/
int connected_central_connect_deal(void *priv, u8 mode);

/* --------------------------------------------------------------------------*/
/**
 * @brief CIG主机连接断开成功处理接口口
 *
 * @param priv:断链成功附带的句柄参数
 *
 * @return 是否执行成功 -- 0为成功，其他为失败
 */
/* ----------------------------------------------------------------------------*/
int connected_central_disconnect_deal(void *priv);

/* --------------------------------------------------------------------------*/
/**
 * @brief CIG从机连接成功处理事件
 *
 * @param priv:连接成功附带的句柄参数
 *
 * @return 是否执行成功 -- 0为成功，其他为失败
 */
/* ----------------------------------------------------------------------------*/
int connected_perip_connect_deal(void *priv);

/* --------------------------------------------------------------------------*/
/**
 * @brief CIG从机连接断开成功处理接口口
 *
 * @param priv:断链成功附带的句柄参数
 *
 * @return 是否执行成功 -- 0为成功，其他为失败
 */
/* ----------------------------------------------------------------------------*/
int connected_perip_disconnect_deal(void *priv);

/* --------------------------------------------------------------------------*/
/**
 * @brief 供外部手动初始化recorder模块
 *
 * @param cis_hdl:recorder模块所对应的cis_hdl
 */
/* ----------------------------------------------------------------------------*/
void cis_audio_recorder_reset(u16 cis_hdl);

/* --------------------------------------------------------------------------*/
/**
 * @brief 供外部手动关闭recorder模块
 *
 * @param cis_hdl:recorder模块所对应的cis_hdl
 */
/* ----------------------------------------------------------------------------*/
void cis_audio_recorder_close(u16 cis_hdl);

/* ----------------------------------------------------------------------------*/
/**
 * @brief 私有cis命令开启或者关闭编码器
 *
 * @param en 开启/关闭解码器
 * @param acl_hdl acl链路句柄
 */
/* ----------------------------------------------------------------------------*/
void connected_perip_connect_recoder(u8 en, u16 acl_hdl);

/* ----------------------------------------------------------------------------*/
/**
 * @brief 判断cis连接是否初始化
 *
 * @return 1:初始化 0:已卸载，未初始化
 */
/* ----------------------------------------------------------------------------*/
u8 is_cis_connected_init();

/* ----------------------------------------------------------------------------*/
/**
 * @brief cig事件发送到用户线程
 *
 * @param event 事件类型
 * @param value 事件消息
 * @param len 	事件消息长度
 */
/* ----------------------------------------------------------------------------*/
int cig_event_to_user(int event, void *value, u32 len);

/* ----------------------------------------------------------------------------*/
/**
 * @brief 根据cig句柄关闭解码器
 *
 * @param cig_hdl cig句柄
 */
/* ----------------------------------------------------------------------------*/
int cis_audio_player_close(u8 cig_hdl);

/* ----------------------------------------------------------------------------*/
/**
 * @brief 根据cig句柄回复上下行
 *
 * @param cig_hdl cig句柄
 * @param cig_phone_call_play 是否需要恢复上行
 */
/* ----------------------------------------------------------------------------*/
int cis_audio_player_resume(u8 cig_hdl, u8 cig_phone_call_play);

/* ----------------------------------------------------------------------------*/
/**
 * @brief acl私有协议接收处理函数注册
 *
 * @param priv
 * @param recv_handle 消息处理callback
 *
 * @return 0
 */
/* ----------------------------------------------------------------------------*/
int connected_iso_recv_handle_register(void *priv, void (*recv_handle)(u16 conn_handle, const void *const buf, size_t length, void *priv));

#ifdef __cplusplus
};
#endif

#endif //_connected_API_H

