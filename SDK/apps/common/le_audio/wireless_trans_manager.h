#ifndef _WIRELESS_TRANS_MANAGER_H
#define _WIRELESS_TRANS_MANAGER_H

#include "typedef.h"

/* Wireless dev ioctrl operation. */
enum {
    WIRELESS_DEV_OP_SEND_PACKET,
    WIRELESS_DEV_OP_SET_SYNC,
    WIRELESS_DEV_OP_GET_SYNC,
    WIRELESS_DEV_OP_GET_BLE_CLK,
    WIRELESS_DEV_OP_GET_TX_SYNC,
    WIRELESS_DEV_OP_STATUS_SYNC,
    WIRELESS_DEV_OP_ENTER_PAIR,
    WIRELESS_DEV_OP_EXIT_PAIR,
    WIRELESS_DEV_OP_GET_PAIR_CODE,
    WIRELESS_DEV_OP_SET_PAIR_CODE,
    WIRELESS_DEV_OP_GET_RSSI,
};

typedef struct {

    //dev name
    const char *name;

    //dev init : usage: init->open->(ioctrl)->close->uninit
    int (*init)(void *priv);

    //dev init : usage: init->open->(ioctrl)->close->uninit
    int (*uninit)(void *priv);

    //dev init : usage: init->open->(ioctrl)->close->uninit
    int (*open)(void *priv);

    //dev init : usage: init->open->(ioctrl)->close->uninit
    int (*close)(void *priv);

    //vendor operation collection
    int (*ioctrl)(int op, ...);

} wireless_trans_ops;

#define REGISTER_WIRELESS_DEV(dev) \
        const wireless_trans_ops dev sec(.wireless_trans)

extern const wireless_trans_ops wireless_trans_ops_begin[];
extern const wireless_trans_ops wireless_trans_ops_end[];

#define list_for_each_wireless_trans(p) \
    for (p = wireless_trans_ops_begin; p < wireless_trans_ops_end; p++)


/* --------------------------------------------------------------------------*/
/**
 * @brief data transmit api
 *
 * @param name:dev name for find dev
 * @param buf: data buffer
 * @param len: data length
 * @param priv:type dev param
 *
 * @return 0:succ, other:err
 */
/* ----------------------------------------------------------------------------*/
int wireless_trans_transmit(const char *name, const void *const buf, size_t len, void *priv);

/* --------------------------------------------------------------------------*/
/**
 * @brief 广播同步数据设置接口
 *
 * @param name:dev name for find dev
 * @param buf: data buffer
 * @param len: data length
 * @param priv:dev operating parameters
 *
 * @return 0:succ, other:err
 */
/* ----------------------------------------------------------------------------*/
int wireless_trans_status_sync(const char *name, const void *const buf, size_t len, void *priv);

/* --------------------------------------------------------------------------*/
/**
 * @brief 获取当前设备的即时时钟
 *
 * @param name:dev name for find dev
 * @param clk:pointer for get clock
 *
 * @return 0:succ, other:err
 */
/* ----------------------------------------------------------------------------*/
int wireless_trans_get_cur_clk(const char *name, void *clk);

/* --------------------------------------------------------------------------*/
/**
 * @brief 用于获取发送方最近一次发数时钟
 *
 * @param name:dev name for find dev
 * @param bis_hdl:hdl for find bis
 *
 * @return 0:succ, other:err
 */
/* ----------------------------------------------------------------------------*/
int wireless_trans_get_last_tx_clk(const char *name, void *priv);

/* --------------------------------------------------------------------------*/
/**
 * @brief init dev private params
 *
 * @param name:dev name for find dev
 * @param priv:dev operating parameters
 *
 * @return 0:succ, other:err
 */
/* ----------------------------------------------------------------------------*/
int wireless_trans_init(const char *name, void *priv);

/* --------------------------------------------------------------------------*/
/**
 * @brief init dev private params
 *
 * @param name:dev name for find dev
 * @param priv:dev operating parameters
 *
 * @return 0:succ, other:err
 */
/* ----------------------------------------------------------------------------*/
int wireless_trans_uninit(const char *name, void *priv);

/* --------------------------------------------------------------------------*/
/**
 * @brief startup dev
 *
 * @param name:dev name for find dev
 * @param priv:type dev param
 *
 * @return 0:succ, other:err
 */
/* ----------------------------------------------------------------------------*/
int wireless_trans_open(const char *name, void *priv);

/* --------------------------------------------------------------------------*/
/**
 * @brief stop and close dev
 *
 * @param name:dev name for find dev
 * @param priv:type dev param
 *
 * @return 0:succ, other:err
 */
/* ----------------------------------------------------------------------------*/
int wireless_trans_close(const char *name, void *priv);

/* --------------------------------------------------------------------------*/
/**
 * @brief Trigger the bis baseband to latch synchronization reference time.
 *
 * @param name : dev name for find dev
 * @param priv : type dev param
 *
 * @return 0:succ, other:err
 */
/* ----------------------------------------------------------------------------*/
int wireless_trans_trigger_latch_time(const char *name, void *priv);

/* --------------------------------------------------------------------------*/
/**
 * @brief audio sync enable.
 *
 * @param name : dev name for find dev
 * @param priv : type dev param
 * @param enable :enable param
 *
 * @return 0:succ, other:err
 */
/* ----------------------------------------------------------------------------*/
int wireless_trans_audio_sync_enable(const char *name, void *priv, u8 enable);
/* --------------------------------------------------------------------------*/
/**
 * @brief Get the bis latched synchronization reference time.
 *
 * @param name      : dev name for find dev
 * @param clk_us    : the param to store time
 * @param priv      : type dev param
 *
 * @return 0:succ, other:err
 */
/* ----------------------------------------------------------------------------*/
int wireless_trans_get_latch_time_us(const char *name, u16 *us_1_12th, u32 *clk_us, u32 *event, void *priv);

int wireless_trans_enter_pair(const char *name, u8 mode, void *priv, u32 pair_code);

int wireless_trans_exit_pair(const char *name, void *priv);

/* --------------------------------------------------------------------------*/
/**
 * @brief 广播获取底层随机生成的pair code，只适用于bst传输
 *
 * @param name:dev name for find dev
 * @param pair_code: ptr for get pair code
 * @param privacy: pair code type, 0-common code, 1-pri_code
 *
 * @return 0:succ, other:err
 */
/* ----------------------------------------------------------------------------*/
int wireless_trans_get_pair_code(const char *name, u8 *pair_code, u8 privacy);

/* --------------------------------------------------------------------------*/
/**
 * @brief 广播设置底层随机生成的pair code，只适用于bst传输
 *
 * @param name:dev name for find dev
 * @param pair_code: ptr for get pair code
 *
 * @return 0:succ, other:err
 */
/* ----------------------------------------------------------------------------*/
int wireless_trans_set_pair_code(const char *name, u8 *pair_code);

/* --------------------------------------------------------------------------*/
/**
 * @brief 获取当前连接设备的信号强度
 *
 * @param name:dev name for fine dev
 * @param bis_hdl:链路句柄
 *
 * @return 0:err, other:信号强度
 */
/* ----------------------------------------------------------------------------*/
int wireless_trans_get_rssi(const char *name, u16 bis_hdl);

#endif //_WIRELESS_TRANS_MANAGER_H


