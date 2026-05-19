/*********************************************************************************************
    *   Filename        : le_broadcast_config.c

    *   Description     :

    *   Author          : Weixin Liang

    *   Email           : liangweixin@zh-jieli.com

    *   Last modifiled  : 2022-12-15 14:17

    *   Copyright:(c)JIELI  2011-2022  @ , All Rights Reserved.
*********************************************************************************************/
#include "app_config.h"
#include "app_main.h"
#include "audio_base.h"
#include "le_broadcast.h"
#include "wireless_trans.h"
#include "debug.h"

#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_AURACAST_SINK_EN | LE_AUDIO_JL_AURACAST_SINK_EN)))||((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_AURACAST_SOURCE_EN | LE_AUDIO_JL_AURACAST_SOURCE_EN)))

/**************************************************************************************************
  Macros
**************************************************************************************************/
#define LOG_TAG             "[BROADCAST_CONFIG]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE

/*! \brief 配对名 */
#define BIG_PAIR_NAME           "jl_soundbox_led"
/*! \brief Broadcast Code */
#define BIG_BROADCAST_CODE      "JL_BROADCAST"

/*! \配置广播通道数，不同通道可发送不同数据，例如多声道音频  */
#define TX_USED_BIS_NUM                 1
#define RX_USED_BIS_NUM                 1
#define SCAN_WINDOW_SLOT                16
#define SCAN_INTERVAL_SLOT              28
#define PRIMARY_ADV_INTERVAL_SLOT       192

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/**************************************************************************************************
  Local Global Variables
**************************************************************************************************/

static big_parameter_t big_rx_param = {
    .pair_name = BIG_PAIR_NAME,

    .num_bis   = RX_USED_BIS_NUM,
    .cb        = &big_rx_cb,
    .ext_phy   = 1,

    .rx = {
        .ext_scan_int = SCAN_INTERVAL_SLOT,
        .ext_scan_win = SCAN_WINDOW_SLOT,
        .psync_to_ms    = 2500,
        .bis            = {1},
        .bsync_to_ms    = 2000,
        .num_br = 0,
        .broadcast_name = {
            [0] = "LE-H_54B7E5C85311",
            [1] = "MoerDuo_BLE",
            [2] = "aaaabbbb",
        },
        .enc       = {
            [0] = 0,
            [1] = 0,
            [2] = 1,
        },
        .bc        = {
            [0] = "",
            [1] = "",
            [2] = {0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38},
        },
    },
};




static big_parameter_t big_tx_param = {
    .cb        = &big_tx_cb,
    .num_bis   = TX_USED_BIS_NUM,
    .ext_phy   = 1,
    .form      = 0,

    .tx = {
        .phy            = BIT(1),
        .aux_phy        = 2,
        .eadv_int_slot  = PRIMARY_ADV_INTERVAL_SLOT,
        .padv_int_slot  = PRIMARY_ADV_INTERVAL_SLOT,
        .num_br    = 0,
        .enc       = {
            [0] = 0,
        },
        .bc        = {
            [0] = BIG_BROADCAST_CODE,
        },
        .vdr = {
            .tx_delay   = 3500,
        },
    },
};


static big_parameter_t *tx_params = NULL;
static big_parameter_t *rx_params = NULL;
static u16 big_transmit_data_len = 0;
static u32 enc_output_frame_len = 0;
static u32 dec_input_buf_len = 0;
static u32 enc_output_buf_len = 0;
static u8 platform_data_index = 0;
static u8 broadcast_num_br = 0;
static struct broadcast_platform_data platform_data;
const static u8 platform_data_mapping[] = {
    APP_MODE_BT,
    APP_MODE_MUSIC,
    APP_MODE_LINEIN,
    APP_MODE_PC,
    //APP_MODE_IIS,
    //APP_MODE_MIC,
    //APP_MODE_SPDIF,
    //APP_MODE_FM,
};

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/*! \brief 每包编码数据长度 */
/* (int)((LE_AUDIO_CODEC_FRAME_LEN / 10) * (JLA_CODING_BIT_RATE / 1000 / 8) + 2) */
/* 如果码率超过96K,即帧长超过122,就需要将每次传输数据大小 修改为一帧编码长度 */
static u32 calcul_big_enc_output_frame_len(u16 frame_len, u32 bit_rate)
{
    return (frame_len * bit_rate / 1000 / 8 / 10 + 2);
}

u32 get_big_enc_output_frame_len(void)
{
    ASSERT(enc_output_frame_len, "enc_output_frame_len is 0");
    return enc_output_frame_len;
}

static u16 calcul_big_transmit_data_len(u32 encode_output_frame_len, u16 period, u16 codec_frame_len)
{
    return (encode_output_frame_len * (period * 10 / 1000 / codec_frame_len));
}

u16 get_big_transmit_data_len(void)
{
    /* ASSERT(big_transmit_data_len, "big_transmit_data_len is 0"); */
    return big_transmit_data_len;
}

static u32 calcul_big_enc_output_buf_len(u32 transmit_data_len)
{
    return (transmit_data_len * 2);
}

u32 get_big_enc_output_buf_len(void)
{
    ASSERT(enc_output_buf_len, "enc_output_buf_len is 0");
    return enc_output_buf_len;
}

static u32 calcul_big_dec_input_buf_len(u32 transmit_data_len)
{
    return (transmit_data_len * 10);
}

u32 get_big_dec_input_buf_len(void)
{
    ASSERT(dec_input_buf_len, "dec_input_buf_len is 0");
    return dec_input_buf_len;
}

u32 get_big_sdu_period_us(void)
{
    return platform_data.args[platform_data_index].sdu_interval;
}

u32 get_big_sample_rate(void)
{
    return platform_data.sample_rate;
}

u32 get_big_coding_type(void)
{
    return platform_data.coding_type;
}


u32 get_big_tx_latency(void)
{
    return platform_data.args[platform_data_index].tx_latency;
}

u32 get_big_mtl_time(void)
{
    return tx_params->tx.mtl;
}

u8 get_bis_num(u8 role)
{
    u8 num = 0;
    if ((role == BROADCAST_ROLE_TRANSMITTER) && tx_params) {
        num = tx_params->num_bis;
    } else if ((role == BROADCAST_ROLE_RECEIVER) && rx_params) {
        num = rx_params->num_bis;
    }
    return num;
}

void set_big_hdl(u8 role, u8 big_hdl)
{
    if ((role == BROADCAST_ROLE_TRANSMITTER) && tx_params) {
        tx_params->big_hdl = big_hdl;
    } else if ((role == BROADCAST_ROLE_RECEIVER) && rx_params) {
        rx_params->big_hdl = big_hdl;
    }
}

int get_big_audio_coding_nch(void)
{
    return platform_data.nch;
}

int get_big_audio_coding_bit_rate(void)
{
    return platform_data.args[platform_data_index].bitrate;
}

int get_big_audio_coding_frame_duration(void)
{
    return platform_data.frame_len;
}

int get_big_tx_rtn(void)
{
    if (tx_params) {
        return tx_params->tx.rtn;
    }

    return 0;
}

int get_big_tx_delay(void)
{
    if (tx_params) {
        return tx_params->tx.vdr.tx_delay;
    }

    return 0;
}

void update_receiver_big_codec_params(void *sync_data)
{
    struct broadcast_sync_info *data_sync = (struct broadcast_sync_info *)sync_data;

    if (data_sync->nch) {
        platform_data.nch = data_sync->nch;
    }
    if (data_sync->sdu_period_ms) {
        platform_data.args[platform_data_index].sdu_interval = data_sync->sdu_period_ms * 1000;
    }
    if (data_sync->frame_size) {
        platform_data.frame_len = data_sync->frame_size;
        platform_data.args[platform_data_index].bitrate = data_sync->frame_size * 10000 * 8 / 100;
    }
    if (data_sync->sample_rate) {
        platform_data.sample_rate = data_sync->sample_rate;
    }
    if (data_sync->coding_type) {
        platform_data.coding_type = data_sync->coding_type;
    }
    if (data_sync->frame_size || data_sync->bit_rate || data_sync->sdu_period_ms) {
        enc_output_frame_len = calcul_big_enc_output_frame_len(platform_data.frame_len, platform_data.args[platform_data_index].bitrate);
        big_transmit_data_len = calcul_big_transmit_data_len(enc_output_frame_len, platform_data.args[platform_data_index].sdu_interval, platform_data.frame_len);
        dec_input_buf_len = calcul_big_dec_input_buf_len(big_transmit_data_len);
    }
    log_info("per adv update audio param frame_size[%d]coding_type[%x]sample_rate[%d]bit_rate[%d]sdu_period_us[%d]\n", platform_data.frame_len, platform_data.coding_type, platform_data.sample_rate, platform_data.args[platform_data_index].bitrate, platform_data.args[platform_data_index].sdu_interval);
}

static const struct broadcast_platform_data *get_broadcast_platform_data(u8 mode)
{
    u8 find = 0;
    int len = syscfg_read(CFG_BIG_PARAMS, platform_data.args, sizeof(platform_data.args));

    if (len <= 0) {
        r_printf("ERR:Can not read the broadcast config\n");
        return NULL;
    }

    for (platform_data_index = 0; platform_data_index < ARRAY_SIZE(platform_data_mapping); platform_data_index++) {
        if (mode == platform_data_mapping[platform_data_index]) {
            find = 1;
            break;
        }
    }

    if (!find) {
        r_printf("ERR:Can not find the broadcast config\n");
        return NULL;
    }

    platform_data.nch = LE_AUDIO_CODEC_CHANNEL;
    platform_data.frame_len = LE_AUDIO_CODEC_FRAME_LEN;
    platform_data.sample_rate = LE_AUDIO_CODEC_SAMPLERATE;
    platform_data.coding_type = LE_AUDIO_CODEC_TYPE;

    put_buf((const u8 *) & (platform_data.args[platform_data_index]), sizeof(struct broadcast_platform_data));
    g_printf("sdu_interval:%d", platform_data.args[platform_data_index].sdu_interval);
    g_printf("tx_latency:%d\n", platform_data.args[platform_data_index].tx_latency);
    g_printf("rtnCToP:%d", platform_data.args[platform_data_index].rtn);
    g_printf("mtlCToP:%d", platform_data.args[platform_data_index].mtl);
    g_printf("bitrate:%d", platform_data.args[platform_data_index].bitrate);
    g_printf("nch:%d", platform_data.nch);
    g_printf("frame_len:%d", platform_data.frame_len);
    g_printf("sample_rate:%d", platform_data.sample_rate);
    g_printf("coding_type:0x%x", platform_data.coding_type);

    return &platform_data;
}

big_parameter_t *set_big_params(u8 app_task, u8 role, u8 big_hdl)
{
    u32 pair_code;
    int ret;

    const struct broadcast_platform_data *data = get_broadcast_platform_data(app_task);

    if (role == BROADCAST_ROLE_TRANSMITTER) {
        tx_params = &big_tx_param;
        memcpy(tx_params->pair_name, get_le_audio_pair_name(), sizeof(tx_params->pair_name));
        enc_output_frame_len = calcul_big_enc_output_frame_len(data->frame_len, data->args[platform_data_index].bitrate);
        big_transmit_data_len = calcul_big_transmit_data_len(enc_output_frame_len, data->args[platform_data_index].sdu_interval, data->frame_len);
        dec_input_buf_len = calcul_big_dec_input_buf_len(big_transmit_data_len);
        enc_output_buf_len = calcul_big_enc_output_buf_len(big_transmit_data_len);
        if (tx_params) {
            tx_params->big_hdl = big_hdl;
            tx_params->tx.mtl = data->args[platform_data_index].sdu_interval * data->args[platform_data_index].mtl;
            tx_params->tx.rtn = data->args[platform_data_index].rtn - 1;
            tx_params->tx.sdu_int_us = data->args[platform_data_index].sdu_interval;
            if (data->nch == 2) {
#if (LEA_TX_CHANNEL_SEPARATION && (LE_AUDIO_CODEC_CHANNEL == 2))
                u8 packet_num;
                u16 single_ch_trans_data_len;
                packet_num = big_transmit_data_len / enc_output_frame_len;
                single_ch_trans_data_len = big_transmit_data_len / 2 + packet_num;
                tx_params->tx.max_sdu = single_ch_trans_data_len;
#else
                tx_params->tx.max_sdu = big_transmit_data_len;
#endif
            } else {
                tx_params->tx.max_sdu = big_transmit_data_len;
            }

            if (big_transmit_data_len > 251) {
                tx_params->tx.vdr.max_pdu = enc_output_frame_len;
            }
        }
#if 0
        ret = syscfg_read(VM_WIRELESS_PAIR_CODE0, &pair_code, sizeof(u32));
        if (ret <= 0) {
            pair_code = 0;
        }
#else
        pair_code = 0;
#endif
        tx_params->pri_ch = pair_code;
        g_printf("wireless_pair_code:0x%x", pair_code);
        return tx_params;
    }

    if (role == BROADCAST_ROLE_RECEIVER) {
        rx_params = &big_rx_param;
        if (broadcast_num_br >= BIG_MAX_RX_BROADCAST_NUMS) {
            broadcast_num_br = 0;
        }
        rx_params->rx.num_br = broadcast_num_br;
        broadcast_num_br++;
        //memcpy(rx_params->pair_name, get_le_audio_pair_name(), sizeof(rx_params->pair_name));
        enc_output_frame_len = calcul_big_enc_output_frame_len(data->frame_len, data->args[platform_data_index].bitrate);
        big_transmit_data_len = calcul_big_transmit_data_len(enc_output_frame_len, data->args[platform_data_index].sdu_interval, data->frame_len);
        dec_input_buf_len = calcul_big_dec_input_buf_len(big_transmit_data_len);
        rx_params->big_hdl = big_hdl;
#if 0
        ret = syscfg_read(VM_WIRELESS_PAIR_CODE0, &pair_code, sizeof(u32));
        if (ret <= 0) {
            pair_code = 0;
        }
#else
        pair_code = 0;
#endif
        rx_params->pri_ch = pair_code;
        g_printf("wireless_pair_code:0x%x", pair_code);
        return rx_params;
    }

    return NULL;
}

#endif

