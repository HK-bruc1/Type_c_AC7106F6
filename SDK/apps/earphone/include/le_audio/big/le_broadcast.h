/*********************************************************************************************
    *   Filename        : broadcast_api.h

    *   Description     :

    *   Author          : Weixin Liang

    *   Email           : liangweixin@zh-jieli.com

    *   Last modifiled  : 2022-07-07 14:37

    *   Copyright:(c)JIELI  2011-2022  @ , All Rights Reserved.
*********************************************************************************************/
#ifndef _BROADCAST_API_H
#define _BROADCAST_API_H

/*  Include header */
#include "system/includes.h"
#include "typedef.h"
#include "big.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Data Types
**************************************************************************************************/
/*! \brief 广播角色枚举 */
enum {
    BROADCAST_ROLE_UNKNOW,
    BROADCAST_ROLE_TRANSMITTER,
    BROADCAST_ROLE_RECEIVER,
};

//time 类型
enum {
    CURRENT_TIME,
    PACKET_RX_TIME,
};

/*! \brief 广播同步的参数 */
struct broadcast_sync_info {
    // 状态同步
    u8 volume;
    u8 nch;
    // 解码参数同步
    u16 frame_size;
    int coding_type;
    int sample_rate;
    int bit_rate;
    u8 sdu_period_ms;
};


struct bt_data {
    u8 length;
    u8 type;
    u8 data[0];
} __packed;

struct broadcast_audio_codec_cfg {
    u8 id;
    u16 cid;
    u16 vid;
    u8 codec_len;
    u8 codec[0];
    u8 meta_len;
    u8 meta[0];
} __packed;

struct broadcast_audio_bis_data {
    u8 index;
    u8 codec_len;
    u8 codec[0];
} __packed;

struct broadcast_bap_base_subgroup {
    u8 bis_count;
    struct broadcast_audio_codec_cfg codec_cfg;
    struct broadcast_audio_bis_data bis_data;
} __packed;

struct  broadcast_bap_base {
    u16 uuid;
    u8 pd[3];
    u8 subgroup_count;
    struct broadcast_bap_base_subgroup subgroups;
} __packed;


enum bt_audio_codec_capability_type {

    /**
     *  @brief LC3 sample frequency capability type
     */
    BT_AUDIO_CODEC_LC3_FREQ = 0x01,

    /**
     *  @brief LC3 frame duration capability type
     */
    BT_AUDIO_CODEC_LC3_DURATION = 0x02,

    /**
     *  @brief LC3 channel count capability type
     */
    BT_AUDIO_CODEC_LC3_CHAN_COUNT = 0x03,

    /**
     *  @brief LC3 frame length capability type
     */
    BT_AUDIO_CODEC_LC3_FRAME_LEN = 0x04,

    /**
     *  @brief Max codec frame count per SDU capability type
     */
    BT_AUDIO_CODEC_LC3_FRAME_COUNT = 0x05,
};

enum bt_audio_codec_config_freq {
    /**
     *  @brief 8 Khz codec Sample Frequency configuration
     */
    BT_AUDIO_CODEC_CONFIG_LC3_FREQ_8KHZ = 0x01,
    /**
     *  @brief 11.025 Khz codec Sample Frequency configuration
     */
    BT_AUDIO_CODEC_CONFIG_LC3_FREQ_11KHZ = 0x02,
    /**
     *  @brief 16 Khz codec Sample Frequency configuration
     */
    BT_AUDIO_CODEC_CONFIG_LC3_FREQ_16KHZ = 0x03,
    /**
     *  @brief 22.05 Khz codec Sample Frequency configuration
     */
    BT_AUDIO_CODEC_CONFIG_LC3_FREQ_22KHZ = 0x04,
    /**
     *  @brief 24 Khz codec Sample Frequency configuration
     */
    BT_AUDIO_CODEC_CONFIG_LC3_FREQ_24KHZ = 0x05,
    /**
     *  @brief 32 Khz codec Sample Frequency configuration
     */
    BT_AUDIO_CODEC_CONFIG_LC3_FREQ_32KHZ = 0x06,
    /**
     *  @brief 44.1 Khz codec Sample Frequency configuration
     */
    BT_AUDIO_CODEC_CONFIG_LC3_FREQ_44KHZ = 0x07,
    /**
     *  @brief 48 Khz codec Sample Frequency configuration
     */
    BT_AUDIO_CODEC_CONFIG_LC3_FREQ_48KHZ = 0x08,
    /**
     *  @brief 88.2 Khz codec Sample Frequency configuration
     */
    BT_AUDIO_CODEC_CONFIG_LC3_FREQ_88KHZ = 0x09,
    /**
     *  @brief 96 Khz codec Sample Frequency configuration
     */
    BT_AUDIO_CODEC_CONFIG_LC3_FREQ_96KHZ = 0x0a,
    /**
     *  @brief 176.4 Khz codec Sample Frequency configuration
     */
    BT_AUDIO_CODEC_CONFIG_LC3_FREQ_176KHZ = 0x0b,
    /**
     *  @brief 192 Khz codec Sample Frequency configuration
     */
    BT_AUDIO_CODEC_CONFIG_LC3_FREQ_192KHZ = 0x0c,
    /**
     *  @brief 384 Khz codec Sample Frequency configuration
     */
    BT_AUDIO_CODEC_CONFIG_LC3_FREQ_384KHZ = 0x0d,
};

/**
 *  @brief LC3 7.5 msec Frame Duration configuration
 */
#define BT_AUDIO_CODEC_CONFIG_LC3_DURATION_7_5 0x00
/**
 *  @brief LC3 10 msec Frame Duration configuration
 */
#define BT_AUDIO_CODEC_CONFIG_LC3_DURATION_10  0x01




/*! \brief 广播接收端音频结构体 */
struct broadcast_rx_audio_hdl {
    void *le_audio;
    void *rx_stream;
};

/**************************************************************************************************
  Global Variables
**************************************************************************************************/
extern const big_callback_t big_tx_cb;
extern const big_callback_t big_rx_cb;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/* ***************************************************************************/
/**
 * @brief open broadcast as transmitter
 *
 * @return available_big_hdl:success
 */
/* *****************************************************************************/
int broadcast_transmitter(big_parameter_t *params);

/* ***************************************************************************/
/**
 * @brief open broadcast as receiver
 *
 * @return available_big_hdl:success
 */
/* *****************************************************************************/
int broadcast_receiver(big_parameter_t *params);

/* ***************************************************************************/
/**
 * @brief close broadcast function
 *
 * @param big_hdl:need closed of big_hdl
 *
 * @return 0:success
 */
/* *****************************************************************************/
int broadcast_close(u8 big_hdl);

/* --------------------------------------------------------------------------*/
/**
 * @brief get current broadcast role
 *
 * @return broadcast role
 */
/* ----------------------------------------------------------------------------*/
u8 get_broadcast_role(void);

/* --------------------------------------------------------------------------*/
/**
 * @brief init broadcast params
 */
/* ----------------------------------------------------------------------------*/
void broadcast_init(void);

/* --------------------------------------------------------------------------*/
/**
 * @brief uninit broadcast params
 */
/* ----------------------------------------------------------------------------*/
void broadcast_uninit(void);

/* --------------------------------------------------------------------------*/
/**
 * @brief 设置需要同步的状态数据
 *
 * @param big_hdl:big句柄
 * @param data:数据buffer
 * @param length:数据长度
 *
 * @return -1:fail，0:success
 */
/* ----------------------------------------------------------------------------*/
int broadcast_set_sync_data(u8 big_hdl, void *data, size_t length);

/* --------------------------------------------------------------------------*/
/**
 * @brief 初始化同步的状态数据的内容
 *
 * @param data:用来同步的数据
 */
/* ----------------------------------------------------------------------------*/
void broadcast_sync_data_init(struct broadcast_sync_info *data);

/* --------------------------------------------------------------------------*/
/**
 * @brief 进入配对模式接口
 *
 * @param role:进入配对模式的角色
 * @param mode:配对模式，0-广播配对，1-连接配对
 * @param pair_event_cb:配对事件回调函数
 * @param user_pair_code:非0时，底层使用该配对码配对
 *
 * @return 0:成功
 */
/* ----------------------------------------------------------------------------*/
int broadcast_enter_pair(u8 role, u8 mode, void *pair_event_cb, u32 user_pair_code);

/* --------------------------------------------------------------------------*/
/**
 * @brief 退出配对模式接口
 *
 * @param role:退出配对模式的角色
 *
 * @return 0:成功
 */
/* ----------------------------------------------------------------------------*/
int broadcast_exit_pair(u8 role);

/* --------------------------------------------------------------------------*/
/**
 * @brief 发送端开启广播成功后的流程开启接口
 *
 * @param priv:big_hdl_t
 * @param mode:保留
 *
 * @return 0:成功
 */
/* ----------------------------------------------------------------------------*/
int broadcast_transmitter_connect_deal(void *priv, u8 mode);

/* --------------------------------------------------------------------------*/
/**
 * @brief 接收端开启广播成功后的流程开启接口
 *
 * @param priv:big_hdl_t
 *
 * @return 0:成功
 */
/* ----------------------------------------------------------------------------*/
int broadcast_receiver_connect_deal(void *priv);

/* --------------------------------------------------------------------------*/
/**
 * @brief 接收端开启广播监听失败后的流程关闭接口
 *
 * @param priv:big句柄
 *
 * @return 0:成功
 */
/* ----------------------------------------------------------------------------*/
int broadcast_receiver_disconnect_deal(void *priv);

/* --------------------------------------------------------------------------*/
/**
 * @brief 供外部手动重置recorder模块
 *
 * @param big_hdl:recorder模块所对应的big_hdl
 *
 * @return 0:success
 */
/* ----------------------------------------------------------------------------*/
int broadcast_audio_recorder_reset(u16 big_hdl);

/* --------------------------------------------------------------------------*/
/**
 * @brief 供外部手动关闭recorder模块
 *
 * @param big_hdl:recorder模块所对应的big_hdl
 *
 * @return 0:success
 */
/* ----------------------------------------------------------------------------*/
int broadcast_audio_recorder_close(u16 big_hdl);

#ifdef __cplusplus
};
#endif

#endif //_BROADCASE_API_H

