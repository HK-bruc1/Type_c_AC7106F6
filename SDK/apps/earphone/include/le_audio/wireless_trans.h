/*********************************************************************************************
    *   Filename        : wireless_params.h

    *   Description     :

    *   Author          : Weixin Liang

    *   Email           : liangweixin@zh-jieli.com

    *   Last modifiled  : 2022-12-15 14:26

    *   Copyright:(c)JIELI  2011-2022  @ , All Rights Reserved.
*********************************************************************************************/
#ifndef __WIRELESS_TRANS_H
#define __WIRELESS_TRANS_H

/*  Include header */
#include "big.h"
#include "cig.h"
#include "le_audio_stream.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/
#ifndef CONFIG_LE_AUDIO_PARAMS_MAX_NUM
#define CONFIG_LE_AUDIO_PARAMS_MAX_NUM  10
#endif

/**************************************************************************************************
  Data Types
**************************************************************************************************/
struct broadcast_cfg_args {
    u8 len;
    u32 sdu_interval;
    u8 rtn;
    u8 mtl;
    u32 bitrate;
    u32 tx_latency;
} __attribute__((packed));

struct broadcast_platform_data {
    struct broadcast_cfg_args args[CONFIG_LE_AUDIO_PARAMS_MAX_NUM];

    //非可视化工具的参数反正下面
    u8 nch;
    u8 frame_len;
    u32 coding_type;
    u32 sample_rate;
};

struct connected_cfg_args {
    u8 len;
    u32 sdu_interval;
    u8 rtnCToP;
    u8 rtnPToC;
    u8 mtlCToP;
    u8 mtlPToC;
    u32 bitrate;
    u32 tx_latency;
} __attribute__((packed));

struct connected_platform_data {
    struct connected_cfg_args args[CONFIG_LE_AUDIO_PARAMS_MAX_NUM];

    //非可视化工具的参数反正下面
    u8 nch;
    u8 frame_len;
    u32 coding_type;
    u32 sample_rate;
    u8 device_type;
};

struct le_audio_mode_ops {
    int (*local_audio_open)(void);
    int (*local_audio_close)(void);
    void *(*tx_le_audio_open)(void *fmt);
    int (*tx_le_audio_close)(void *le_audio);
    int (*rx_le_audio_open)(void *rx_audio, void *args);
    int (*rx_le_audio_close)(void *rx_audio);
    int (*play_status)(void);                 //获取当前音频播放状态
};

/*! \brief 本地音源播放器状态 */
enum {
    LOCAL_AUDIO_PLAYER_STATUS_ERR,
    LOCAL_AUDIO_PLAYER_STATUS_PLAY,
    LOCAL_AUDIO_PLAYER_STATUS_STOP,
};

enum le_audio_mode_t {
    LE_AUDIO_1T3_MODE = 100,
};

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/
void read_le_audio_product_name(void);
void read_le_audio_pair_name(void);
const char *get_le_audio_product_name(void);
const char *get_le_audio_pair_name(void);
struct le_audio_mode_ops *get_broadcast_audio_sw_ops();
struct le_audio_mode_ops *get_connected_audio_sw_ops();
int le_audio_ops_register(u8 mode);
int le_audio_ops_unregister(void);
void wireless_trans_auto_test1_init(void);
void wireless_trans_auto_test2_init(void);
void wireless_trans_auto_test3_init(void);
void wireless_trans_auto_test4_init(void);

u32 get_big_enc_output_frame_len(void);
u16 get_big_transmit_data_len(void);
u32 get_big_enc_output_buf_len(void);
u32 get_big_dec_input_buf_len(void);
u32 get_big_sdu_period_us(void);
u32 get_big_sample_rate(void);
u32 get_big_coding_type(void);
u32 get_big_tx_latency(void);
u32 get_big_mtl_time(void);
u8 get_bis_num(u8 role);
void set_big_hdl(u8 role, u8 big_hdl);
void update_receiver_big_codec_params(void *sync_data);
big_parameter_t *set_big_params(u8 app_task, u8 role, u8 big_hdl);
int get_big_audio_coding_nch(void);
int get_big_audio_coding_bit_rate(void);
int get_big_audio_coding_frame_duration(void);
int get_big_tx_rtn(void);
int get_big_tx_delay(void);

u32 get_cig_enc_output_frame_len(void);
u16 get_cig_transmit_data_len(void);
u32 get_cig_enc_output_buf_len(void);
u32 get_cig_dec_input_buf_len(void);
u32 get_cig_sdu_period_us(void);
u32 get_cig_mtl_time(void);
void set_cig_hdl(u8 role, u8 cig_hdl);
cig_parameter_t *set_cig_params(void);
int get_cig_audio_coding_nch(void);
int get_cig_audio_coding_bit_rate(void);
int get_cig_audio_coding_frame_duration(void);
int get_cig_audio_coding_sample_rate(void);
int get_cig_tx_rtn(void);
int get_cig_tx_delay(void);
u8 get_cis_num(u8 role);
void reset_cig_params(void);
void set_unicast_lc3_info(u8 *date, u8 len);
u8 get_le_audio_jl_dongle_device_type();
void set_le_audio_jl_dongle_device_type(u8 type);
void get_decoder_params_fmt(struct le_audio_stream_format *fmt);
void get_encoder_params_fmt(struct le_audio_stream_format *fmt);
u16 get_lc3_decoder_info_octets_per_frame();

#ifdef __cplusplus
};
#endif

#endif

