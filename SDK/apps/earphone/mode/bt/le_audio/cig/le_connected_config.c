/*********************************************************************************************
    *   Filename        : cig_params.c

    *   Description     :

    *   Author          : Weixin Liang

    *   Email           : liangweixin@zh-jieli.com

    *   Last modifiled  : 2022-12-15 14:31

    *   Copyright:(c)JIELI  2011-2022  @ , All Rights Reserved.
*********************************************************************************************/
#include "app_config.h"
#include "app_main.h"
#include "audio_base.h"
#include "le_connected.h"
#include "wireless_trans.h"
#include "media_config.h"

#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)))

/**************************************************************************************************
  Macros
**************************************************************************************************/

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/**************************************************************************************************
  Global Variables
**************************************************************************************************/


#if ((TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_UNICAST_SINK_EN))
static cig_parameter_t cig_perip_param = {
    .cb        = &cig_perip_cb,
    .pair_en        = 0,

    .vdr = {
        .tx_delay   = 1500,
        .aclMaxPduCToP = 0,
        .aclMaxPduPToC = 0,
    },
};
#else
static cig_parameter_t cig_perip_param = {
    .cb        = &cig_perip_cb,
    .pair_en        = 0,

    .vdr = {
        .tx_delay   = 1500,
        .aclMaxPduCToP = JL_UNICAST_ACL_MAX_PDU_CTOP,
        .aclMaxPduPToC = 13,
    },
};
#endif


static cig_parameter_t *perip_params = NULL;
static u16 cig_transmit_data_len = 0;
static u32 enc_output_frame_len = 0;
static u32 dec_input_buf_len = 0;
static u32 enc_output_buf_len = 0;
static u8 platform_data_index = 0;
static struct connected_platform_data platform_data;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/*! \brief 每包编码数据长度 */
/* (int)((LE_AUDIO_CODEC_FRAME_LEN / 10) * (JLA_CODING_BIT_RATE / 1000 / 8) + 2) */
/* 如果码率超过96K,即帧长超过122,就需要将每次传输数据大小 修改为一帧编码长度 */
static u32 calcul_cig_enc_output_frame_len(u16 frame_len, u32 bit_rate)
{
    int len = 0;
    if (LE_AUDIO_CODEC_TYPE == AUDIO_CODING_LC3) {
        len = (frame_len * bit_rate / 1000 / 8 / 10);
    } else if (LE_AUDIO_CODEC_TYPE == AUDIO_CODING_JLA_V2) {
        len = (frame_len * bit_rate / 1000 / 8 / 10 + (JLA_V2_CODEC_WITH_FRAME_HEADER == 0 ? 0 : 2));
    } else {
        printf("calcul_cig_enc_output_frame_len  error !!!  \n  unsupport this coding_type\n");
    }
    return len;

}

u32 get_cig_enc_output_frame_len(void)
{
    ASSERT(enc_output_frame_len, "enc_output_frame_len is 0");
    return enc_output_frame_len;
}

static u16 calcul_cig_transmit_data_len(u32 encode_output_frame_len, u16 period, u16 codec_frame_len)
{
    return (encode_output_frame_len * (period * 10 / 1000 / codec_frame_len));
}

u16 get_cig_transmit_data_len(void)
{
    ASSERT(cig_transmit_data_len, "cig_transmit_data_len is 0");
    return cig_transmit_data_len;
}

static u32 calcul_cig_enc_output_buf_len(u32 transmit_data_len)
{
    return (transmit_data_len * 2);
}

u32 get_cig_enc_output_buf_len(void)
{
    ASSERT(enc_output_buf_len, "enc_output_buf_len is 0");
    return enc_output_buf_len;
}

static u32 calcul_cig_dec_input_buf_len(u32 transmit_data_len)
{
    return (transmit_data_len * 10);
}

u32 get_cig_dec_input_buf_len(void)
{
    ASSERT(dec_input_buf_len, "dec_input_buf_len is 0");
    return dec_input_buf_len;
}

u32 get_cig_sdu_period_us(void)
{
    return platform_data.args[platform_data_index].sdu_interval;
}

u32 get_cig_mtl_time(void)
{
    int mtl = 1;
    return mtl;
}

void set_cig_hdl(u8 role, u8 cig_hdl)
{
    perip_params->cig_hdl = cig_hdl;
}
void set_le_audio_jl_dongle_device_type(u8 type)
{
    platform_data.device_type = type;
}
u8 get_le_audio_jl_dongle_device_type()
{
    return platform_data.device_type;
}

int get_cig_audio_coding_nch(void)
{
    return platform_data.nch;
}
int get_cig_audio_coding_sample_rate(void)
{
    return platform_data.sample_rate;
}

int get_cig_audio_coding_bit_rate(void)
{
    return platform_data.args[platform_data_index].bitrate;
}

int get_cig_audio_coding_frame_duration(void)
{
    return platform_data.frame_len;
}

int get_cig_tx_rtn(void)
{
    int rtn = 0;
    return rtn;
}
typedef struct {
    // lc3 codec config
    uint16_t sampling_frequency_hz;
    uint8_t frame_duration;
    uint16_t number_samples_per_frame;
    uint16_t octets_per_frame;
    uint8_t  num_bis;
    uint8_t  num_channels;
} lc3_codec_config_t;

lc3_codec_config_t lc3_decoder_info;
lc3_codec_config_t lc3_encoder_info;

void set_unicast_lc3_info(u8 *date, u8 len)
{
    int info_len = sizeof(lc3_codec_config_t);
    memcpy(&lc3_decoder_info, date, info_len);
    memcpy(&lc3_encoder_info, date + info_len, info_len);
    if (len <= (info_len * 2)) {
        platform_data.args[platform_data_index].sdu_interval = 100 * lc3_decoder_info.frame_duration;
        /* ASSERT(0,"update dongle,add new sdu_interval");	 */
    } else {
        memcpy(&platform_data.args[platform_data_index].sdu_interval, date + (2 * info_len), 2);
        ASSERT(platform_data.args[platform_data_index].sdu_interval, "sdu_interval is 0");


    }
    /* printf("dec sampling=%d,frame_duration%d,octets_per_frame=%d,num_channels=%d\n", */
    /* lc3_decoder_info.sampling_frequency_hz, */
    /* lc3_decoder_info.frame_duration, */
    /* lc3_decoder_info.octets_per_frame, */
    /* lc3_decoder_info.num_channels */
    /* ); */
    /* printf("enc sampling=%d,frame_duration%d,octets_per_frame=%d,num_channels=%d\n", */
    /* lc3_encoder_info.sampling_frequency_hz, */
    /* lc3_encoder_info.frame_duration, */
    /* lc3_encoder_info.octets_per_frame, */
    /* lc3_encoder_info.num_channels */
    /* ); */

}
void get_decoder_params_fmt(struct le_audio_stream_format *fmt)
{
    /* lc3_decoder_info */
    fmt->nch = lc3_decoder_info.num_channels;
    platform_data.nch = fmt->nch;
    fmt->sample_rate = lc3_decoder_info.sampling_frequency_hz;
    platform_data.sample_rate = fmt->sample_rate;
    fmt->frame_dms = lc3_decoder_info.frame_duration;
    /* platform_data.args[platform_data_index].sdu_interval = 100*lc3_decoder_info.frame_duration; */
    platform_data.frame_len = lc3_decoder_info.frame_duration;
    fmt->sdu_period = platform_data.args[platform_data_index].sdu_interval;
    fmt->bit_rate = lc3_decoder_info.octets_per_frame * 8 * 1000 * 10 / fmt->frame_dms;
    platform_data.args[platform_data_index].bitrate = fmt->bit_rate;
    printf("get_decoder_params_fmt=%d,%d,%d,%d,%d,%d,%d\n", fmt->nch, fmt->sample_rate, fmt->frame_dms, fmt->sdu_period, fmt->bit_rate, lc3_decoder_info.octets_per_frame, platform_data.args[platform_data_index].sdu_interval);

}
u16 get_lc3_decoder_info_octets_per_frame()
{
    return lc3_decoder_info.octets_per_frame;
}
void get_encoder_params_fmt(struct le_audio_stream_format *fmt)
{
    /* lc3_encoder_info */
    fmt->nch = lc3_encoder_info.num_channels;
    fmt->sample_rate = lc3_encoder_info.sampling_frequency_hz;
    fmt->frame_dms = lc3_encoder_info.frame_duration;
    fmt->sdu_period = platform_data.args[platform_data_index].sdu_interval;
    fmt->bit_rate =  lc3_encoder_info.octets_per_frame * 8 * 1000 * 10 / fmt->frame_dms;
    enc_output_frame_len = calcul_cig_enc_output_frame_len(fmt->frame_dms, fmt->bit_rate);
    cig_transmit_data_len = calcul_cig_transmit_data_len(enc_output_frame_len, fmt->sdu_period, fmt->frame_dms);
    printf("get_encoder_params_fmt=%d,%d,%d,%d,%d %d\n", fmt->nch, fmt->sample_rate, fmt->frame_dms, fmt->sdu_period, fmt->bit_rate, lc3_encoder_info.octets_per_frame);

}
void lea_profile_set_unicast_lc3_decoder_param(void *lc3_info)
{
    lc3_codec_config_t *coder_info = (lc3_codec_config_t *)lc3_info;
    //这个位置可以建立解码线程了,初始化的时候要注意我们的有没有区分channel
    r_printf("unicast lc3_decode_info--num_bis:%d, Sampling_fre:%d, frame_duration:%d,octets_per_frame:%d,ch_num : %d,number_samples_per_frame : %d\n",
             coder_info->num_bis, coder_info->sampling_frequency_hz,
             coder_info->frame_duration, coder_info->octets_per_frame, coder_info->num_channels, coder_info->number_samples_per_frame);

    platform_data.nch = coder_info->num_channels;
    platform_data.sample_rate = coder_info->sampling_frequency_hz;
    platform_data.frame_len = coder_info->frame_duration;
    if (platform_data.frame_len == 75) {
        platform_data.args[platform_data_index].sdu_interval = 7500;
    } else {
        platform_data.args[platform_data_index].sdu_interval = 10000;
    }
    //手机传来的帧长是单声道的，双声道每包的帧长要*2
    platform_data.args[platform_data_index].bitrate = coder_info->num_channels * coder_info->octets_per_frame * 8 * 1000 * 10 /  platform_data.frame_len;

    //计算并配置编码输出长度，配置相应的buf_len;
    //这里默认编解码参数相同，如果遇到不同的情况需要分开处理;
    enc_output_frame_len = calcul_cig_enc_output_frame_len(platform_data.frame_len, platform_data.args[platform_data_index].bitrate);
    cig_transmit_data_len = calcul_cig_transmit_data_len(enc_output_frame_len, get_cig_sdu_period_us(), platform_data.frame_len);
    dec_input_buf_len = calcul_cig_dec_input_buf_len(cig_transmit_data_len);

    printf("cis bit_rate:%d,enc_output_len:%d,cig_transmit_len:%d\n",
           platform_data.args[platform_data_index].bitrate, enc_output_frame_len, cig_transmit_data_len);
}
void lea_profile_set_unicast_lc3_encoder(void *lc3_info)
{
    lc3_codec_config_t *coder_info = (lc3_codec_config_t *)lc3_info;
    //这个位置可以建立编码线程了,初始化的时候要注意我们的有没有区分channel
    y_printf("unicast lc3_encode_info--num_bis:%d, Sampling_fre:%d, frame_duration:%d,octets_per_frame:%d\n",
             coder_info->num_bis, coder_info->sampling_frequency_hz,
             coder_info->frame_duration, coder_info->octets_per_frame);
    printf("lc3 cis enc channel:%d\n", coder_info->num_channels);
}

static const struct connected_platform_data *get_connected_platform_data(u8 index)
{
    u8 find = 0;
    int len = syscfg_read(CFG_CIG_PARAMS, platform_data.args, sizeof(platform_data.args));
    if (len <= 0) {
        r_printf("ERR:Can not read the connected config\n");
        /* return NULL; */
    }

    platform_data_index = index;

    if (!find) {
        r_printf("ERR:Can not find the connected config\n");
        /* return NULL; */
    }

    platform_data.nch = LE_AUDIO_CODEC_CHANNEL;
    platform_data.frame_len = LE_AUDIO_CODEC_FRAME_LEN;
    platform_data.sample_rate = LE_AUDIO_CODEC_SAMPLERATE;
    platform_data.coding_type = LE_AUDIO_CODEC_TYPE;
    platform_data.args[platform_data_index].sdu_interval = 10000;
    platform_data.args[platform_data_index].rtnCToP = 1;
    platform_data.args[platform_data_index].rtnPToC = 1;
    platform_data.args[platform_data_index].mtlCToP = 1;
    platform_data.args[platform_data_index].mtlPToC = 1;
    platform_data.args[platform_data_index].bitrate = 192 * 1000;

    /* put_buf((const u8 *)&(platform_data.args[platform_data_index]), sizeof(struct connected_cfg_args)); */
    g_printf("sdu_interval:%d", platform_data.args[platform_data_index].sdu_interval);
    g_printf("tx_latency:%d\n", platform_data.args[platform_data_index].tx_latency);
    g_printf("rtnCToP:%d", platform_data.args[platform_data_index].rtnCToP);
    g_printf("rtnPToC:%d", platform_data.args[platform_data_index].rtnPToC);
    g_printf("mtlCToP:%d", platform_data.args[platform_data_index].mtlCToP);
    g_printf("mtlPToC:%d", platform_data.args[platform_data_index].mtlPToC);
    g_printf("bitrate:%d", platform_data.args[platform_data_index].bitrate);
    g_printf("nch:%d", platform_data.nch);
    g_printf("frame_len:%d", platform_data.frame_len);
    g_printf("sample_rate:%d", platform_data.sample_rate);
    g_printf("coding_type:0x%x", platform_data.coding_type);

    return &platform_data;
}

cig_parameter_t *set_cig_params(void)
{
    int ret;

    const struct connected_platform_data *data = get_connected_platform_data(0);

    perip_params = &cig_perip_param;
    memcpy(perip_params->pair_name, get_le_audio_pair_name(), sizeof(perip_params->pair_name));
    enc_output_frame_len = calcul_cig_enc_output_frame_len(data->frame_len, data->args[platform_data_index].bitrate);
    cig_transmit_data_len = calcul_cig_transmit_data_len(enc_output_frame_len, data->args[platform_data_index].sdu_interval, data->frame_len);
    dec_input_buf_len = calcul_cig_dec_input_buf_len(cig_transmit_data_len);
    return perip_params;
}

void reset_cig_params(void)
{
    perip_params = 0;
}

#endif

