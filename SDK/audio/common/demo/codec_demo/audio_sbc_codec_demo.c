//////////////////////////////////////////////////////////////
/*
	demo流程： adc->pcm_data->enc->enc_data->dec->pcm_data->dac

 	demo :测试sbc 软件编码  sbc 软件解码及sbc 硬件解码流程
 		 通过宏 USE_HW_SBC_DECODER 配置，选择是否适用硬件解码。
       通过在audio_sbc_enc_dec_test_open() 接口里面配置编解码参数，

       本demo 仅做编码一帧解码一帧的流程，如果适配到流程有拼帧拆帧及头信息检查问题，请自行处理
	   并且解码支持几帧数据同时输入解码.多帧数据可以一次给解码解码。


	测试流程仅作为编解码测试所用,
	注意： 整个流程是根据adc 的节奏进行运行，adc 产生一帧数据，编码一次，解码一次，输出到dac,
 		   所以要保证adc 产生的数据正好够一帧数据(ADC_DEMO_IRQ_POINTS 配置adc 起一次中断出来的点数)，
		   才能稳定运行，要注意声道,采样率和中断节奏的适配,
*/
//////////////////////////////////////////////////////////////
#include "audio_splicing.h"
#include "system/includes.h"
#include "cpu/includes.h"
#include "app_main.h"
#include "audio_config.h"
#include "audio_base.h"
#include "audio_adc.h"
#include "audio_dac.h"
#include "codec/hwaccel.h"
#include "codec/audio_decode_common_api.h"
#include "codec/sbc_enc_source.h"

#if 0   //这里打开代码模块使能


static int audio_demo_enc_resume(void *priv);
static int audio_demo_dec_resume(void *priv);
static int audio_demo_pcm_data_write(void *priv, void *data, u16 len);
static int audio_demo_pcm_data_read(void *priv, void *buf, u16 len);
static int audio_demo_enc_data_write(void *priv, void *data, u16 len);
static int audio_demo_enc_data_read(void *priv, void *buf, u16 len);


//-----------------------------------------------------------------------------
//   dac demo
//-----------------------------------------------------------------------------
extern struct audio_dac_hdl dac_hdl;
static u8 fill_zero_data = 0;
static int audio_demo_dac_write(void *priv, u8 *data, u16 len)
{
    if (fill_zero_data) { //初始填一些0数据，避免dac播空， 实际使用不需要
        u8 zero_data[128 * 2] = {0};
        audio_dac_write(&dac_hdl, zero_data, sizeof(zero_data));
        fill_zero_data = 0;
    }
    int wlen = audio_dac_write(&dac_hdl, data, len);
    if (wlen != len) {
        printf("DAC_W : %d,%d\n", len, wlen);
    }

    return wlen;
}

static int audio_demo_dac_open(u32 sample_rate)
{
    fill_zero_data = 1;
    app_audio_state_switch(APP_AUDIO_STATE_MUSIC, app_audio_volume_max_query(AppVol_BT_MUSIC), NULL);
    audio_dac_set_volume(&dac_hdl, app_audio_get_volume(APP_AUDIO_STATE_MUSIC));
    audio_dac_set_sample_rate(&dac_hdl, sample_rate);
    audio_dac_start(&dac_hdl);
    audio_dac_channel_start(NULL);

    printf("===  audio_demo_dac_open\n");

    return 0;
}


static void audio_demo_dac_close(void)
{
    audio_dac_stop(&dac_hdl);
    audio_dac_close(&dac_hdl);
}


//-----------------------------------------------------------------------------
//   enccoder demo
//-----------------------------------------------------------------------------


struct audio_sbc_encoder {
    void *priv;
    u8 start;

    //编码器的结构 ,不同的编码器不一样;
    sbc_t param;
    u8 *run_buf;
    u8 *in_buf;
    u8 *out_buf;
    u8 *remain_addr;
    u16 remain_len;
    u16 frame_len;
    u16 input_len;

};


//读入待编码的pcm数据
static u16 audio_sbc_enc_read_data(void *priv, u8 *buf, u16 len)
{
    struct audio_sbc_encoder *sbc_enc = (struct audio_sbc_encoder *)priv;

    int rlen =  audio_demo_pcm_data_read(sbc_enc->priv, buf, len);

    /* printf("enc input : %d---%d",len,rlen); */

    return rlen;
}
//编码数据输出
static u16 audio_sbc_enc_output_data(void *priv, u8 *buf, u16 len)
{
    struct audio_sbc_encoder *sbc_enc = (struct audio_sbc_encoder *)priv;

    int wlen = audio_demo_enc_data_write(sbc_enc->priv, buf, len);
    /* printf("enc out len: %d-%d",len,wlen); */

    if (sbc_enc->priv) {
        audio_demo_dec_resume(sbc_enc->priv);
    }

    return wlen;
}




/*打开编码器，申请资源*/
static void *audio_sbc_encoder_open(void *priv)
{
    struct audio_sbc_encoder *sbc_enc = NULL;

    printf("---  sbc_enc open\n");
    sbc_enc = zalloc(sizeof(*sbc_enc));
    if (sbc_enc) {
        sbc_enc->priv = priv;
        return sbc_enc;
    }

    printf(" sbc encoder open errer \n");
    return NULL;
}


/*初始化一些编码器配置，准备开始编码*/
static int audio_sbc_encoder_start(void *priv)
{
    struct audio_sbc_encoder *sbc_enc = (struct audio_sbc_encoder *)priv;
    if (!sbc_enc || sbc_enc->start) {
        return 0;
    }

    int need_buf_size = sbc_enc_query();
    sbc_enc->run_buf = zalloc(need_buf_size);
    if (sbc_enc->run_buf) {

        sbc_t temp_sbc_param = {0};
        memcpy(&temp_sbc_param, &sbc_enc->param, sizeof(sbc_t));
        sbc_init(&sbc_enc->param, 0, sbc_enc->run_buf); //这里面会把参数清掉，所以，临时保存一下，在重新赋值
        sbc_enc->param.frequency = temp_sbc_param.frequency;
        sbc_enc->param.blocks = temp_sbc_param.blocks;
        sbc_enc->param.subbands = temp_sbc_param.subbands;
        sbc_enc->param.mode = temp_sbc_param.mode;
        sbc_enc->param.allocation = temp_sbc_param.allocation;
        sbc_enc->param.endian = temp_sbc_param.endian;
        sbc_enc->param.bitpool = temp_sbc_param.bitpool;

        u8 subbands = sbc_enc->param.subbands ? 8 : 4;
        u8 blocks = ((sbc_enc->param.blocks) + 1) * 4;
        u8 channels = sbc_enc->param.mode  == SBC_MODE_MONO ? 1 : 2;
        u8 joint = sbc_enc->param.mode  == SBC_MODE_JOINT_STEREO ? 1 : 0;

        sbc_enc->frame_len = 4 + ((4 * subbands * channels) >> 3); //编码的帧长
        if (sbc_enc->param.mode >= SBC_MODE_STEREO) {
            sbc_enc->frame_len += (((joint ? subbands : 0) + blocks * sbc_enc->param.bitpool) + 7) >> 3;
        } else {
            sbc_enc->frame_len += ((blocks * channels * sbc_enc->param.bitpool) + 7) >> 3;
        }

        sbc_enc->input_len = subbands * blocks * channels * 2; //编码一帧需要的pcm 长度
        sbc_enc->in_buf = zalloc(sbc_enc->input_len);
        sbc_enc->out_buf = zalloc(sbc_enc->frame_len);
        printf("sbc_enc->input_len : %d,sbc_enc->frame_len ; %d\n", sbc_enc->input_len, sbc_enc->frame_len);
        sbc_enc->start = 1;
        return 0;
    } else {
        printf("sbc_encoder_start err \n no buf \n");
        return -ENOMEM;
    }
}

static int audio_sbc_encoder_set_fmt(void *priv,  sbc_t *sbc_fmt)
{
    struct audio_sbc_encoder *sbc_enc = (struct audio_sbc_encoder *)priv;
    if (sbc_fmt) {
        sbc_enc->param.frequency = sbc_fmt->frequency;
        sbc_enc->param.blocks = sbc_fmt->blocks;
        sbc_enc->param.subbands = sbc_fmt->subbands;
        sbc_enc->param.mode = sbc_fmt->mode;
        sbc_enc->param.allocation = sbc_fmt->allocation;
        sbc_enc->param.endian = sbc_fmt->endian;
        sbc_enc->param.bitpool = sbc_fmt->bitpool;
        y_printf("sbc_enc->param.bitpool：%d\n", sbc_enc->param.bitpool);
    }
    return 0;
}


/*编码运行函数，包括读pcm 数据，进行编码，编码输出,错误处理等*/
/*有些拜编码器需要外部读数据，有些编码器自己有接口读数据，需要不同处理*/
static int audio_sbc_encoder_run(void *priv)
{
    struct audio_sbc_encoder *sbc_enc = (struct audio_sbc_encoder *)priv;

    int wlen = 0;

    if (sbc_enc->start == 0) {
        return 0;
    }

    if (sbc_enc->remain_len) { //如果上次的数据还未输出完，要先输出上次的数据，
        wlen = audio_sbc_enc_output_data(sbc_enc, sbc_enc->remain_addr, sbc_enc->remain_len);
        sbc_enc->remain_len -= wlen;
        sbc_enc->remain_addr += wlen;
        if (sbc_enc->remain_len) {
            return 0;
        }
    }

    int rlen = audio_sbc_enc_read_data(sbc_enc, sbc_enc->in_buf, sbc_enc->input_len);
    if (!rlen) {
        return 0;
    }

    u32 real_o_len = 0;
    sbc_encode(&sbc_enc->param, sbc_enc->in_buf, sbc_enc->input_len, sbc_enc->out_buf, sbc_enc->frame_len, &real_o_len);
    /* y_printf("enc out : %d\n",output_len); */
    if (real_o_len != sbc_enc->frame_len) {
        printf("SBC ENC ERR\n");
        return -ENOMEM; //编码出错，可以根据返回值做一些错误处理
    }

    wlen = audio_sbc_enc_output_data(sbc_enc, sbc_enc->out_buf, sbc_enc->frame_len);
    if (wlen != sbc_enc->frame_len) { //如果后级写不进去，需要处理数据remain的情况
        sbc_enc->remain_len = 	sbc_enc->frame_len	- wlen;
        sbc_enc->remain_addr = sbc_enc->out_buf + wlen;
    }

    return 0;

}

static int audio_sbc_encoder_close(void *priv)
{
    struct audio_sbc_encoder *sbc_enc = (struct audio_sbc_encoder *)priv;
    if (sbc_enc) {
        sbc_enc->start = 0;
        if (sbc_enc->in_buf) {
            free(sbc_enc->in_buf);
            sbc_enc->in_buf = NULL;
        }
        if (sbc_enc->out_buf) {
            free(sbc_enc->out_buf);
            sbc_enc->out_buf = NULL;
        }
        if (sbc_enc->run_buf) {
            free(sbc_enc->run_buf);
            sbc_enc->run_buf = NULL;
        }

        free(sbc_enc);
        sbc_enc = NULL;
    }
    return 0;

}


//-----------------------------------------------------------------------------
//   decoder demo
//-----------------------------------------------------------------------------

#define USE_HW_SBC_DECODER  0

struct audio_sbc_decoder {
    void *priv;
    u8 start;
    u8  output_mode;

    //解码器的结构 ,不同的解码器不一样;
#if USE_HW_SBC_DECODER
    const struct audio_hwaccel_ops *hwaccel_ops;
    u8 pcm_buf[1024];
    void *decoder;
    u16 pcm_len;
    s16 remain_len;		//上一包剩余未消耗的长度
    u8 channels;
#else
    void *dc_buf;
    void *stk_buf;
    struct if_decoder_io dec_io;
    struct decoder_inf *info;
    audio_decoder_ops *dec_ops;
    deccfg_inf dcf_obj;
    u8 nch;
#endif
    u16 frame_len;
    u8 *in_buf;
    sbc_t param;

};

//读数据到解码器，数据写入buf, 读取长度是len
static u16 audio_demo_dec_read_data(void *priv, u8 *buf, u16 len)
{
    struct audio_sbc_decoder *sbc_dec = (struct audio_sbc_decoder *)priv;

    int rlen = audio_demo_enc_data_read(sbc_dec->priv, buf, len);

    /* printf("dec input :%d--%d\n",len,rlen); */

    return rlen ;
}




//解码数据输出，数据输出不完，下次run的时候会先输出剩余数据，
static u16 audio_demo_dec_output_data(void *priv, u8 *data, u16 len)
{
    struct audio_sbc_decoder *sbc_dec = (struct audio_sbc_decoder *)priv;
    int wlen = 0;

    wlen = audio_demo_dac_write(NULL, data, len);
    /* printf("dec out len : %d=%d\n",len, wlen); */

    return len;
}


/*打开解码器，申请资源*/
void *audio_sbc_decoder_open(void *priv)
{
    struct audio_sbc_decoder *sbc_dec = NULL;
    struct audio_hwaccel_ops *p;
    sbc_dec = zalloc(sizeof(struct audio_sbc_decoder));
    if (sbc_dec) {
#if USE_HW_SBC_DECODER
        list_for_each_codec_hwaccel(p) {
            if (strcmp(p->name, "sbc") == 0) { //查找硬件解码器
                sbc_dec->hwaccel_ops = p;
                sbc_dec->priv = priv;
                return sbc_dec;
            }
        }
#else
        sbc_dec->priv = priv;
        extern audio_decoder_ops *get_sbc_dec_ops();
        sbc_dec->dec_ops = get_sbc_dec_ops();//获取软件解码器
        ASSERT(sbc_dec->dec_ops != NULL);

        return sbc_dec;
#endif



    }
    printf("no hwaccel_ops \n  sbc decoder open errer \n");
    return NULL;

}

#if USE_HW_SBC_DECODER
static void *sbc_output_alloc(void *priv, int size)
{
    return NULL;
}

//配置输出buf, 如果buf 不够，会按照buf大小分多次输出
static void *sbc_output_alloc_free_space(void *priv, int *size)
{
    struct audio_sbc_decoder *sbc_dec = (struct audio_sbc_decoder *)priv;

    *size = sizeof(sbc_dec->pcm_buf);
    return sbc_dec->pcm_buf;
}

static int sbc_output_finish(void *priv, void *buf, int size)
{
    struct audio_sbc_decoder *sbc_dec = (struct audio_sbc_decoder *)priv;

    int wlen = audio_demo_dec_output_data(sbc_dec->priv, buf, size);
    sbc_dec->remain_len = size - wlen;

    return wlen;
}

static const struct audio_stream_ops sbc_output_ops = {
    .alloc = sbc_output_alloc,
    .alloc_free_space = sbc_output_alloc_free_space,
    .finish = sbc_output_finish,
};

__attribute__((weak))
int hw_sbc_set_output_channel(void *_hdl, enum audio_channel channel)
{
    return 0;
}
#else

/*
 * 检查数据是否完毕
 */
static int __sbc_check_buf(void *priv, u32 addr, void *buf)
{
    return 0;
}


/*
 * 解码后的数据输出
 */
static int __sbc_output(void *priv, void *data, int len)
{
    struct audio_sbc_decoder *sbc_dec = (struct audio_sbc_decoder *)priv;

    return audio_demo_dec_output_data(sbc_dec->priv, data, len);
}

/*
 * 获取文件长度
 */
static u32 __sbc_get_lslen(void *priv)
{
    return 0x7fffffff;
}


static u32 __sbc_store_rev_data(void *priv, u32 addr, int len)
{
    return len;
}

/*
 * 数据输入到解码器buf
 */
static int __sbc_input_fr(void *priv, void **buf)
{
    struct audio_sbc_decoder *sbc_dec = (struct audio_sbc_decoder *)priv;

    int rlen = audio_demo_dec_read_data(sbc_dec, sbc_dec->in_buf, sbc_dec->frame_len);
    if (rlen) {
        *buf = sbc_dec->in_buf;
    } else {
        *buf = NULL;
        return 0;
    }

    /* printf("rlen = %d\n",rlen); */
    return rlen;
}
static int audio_sbc_decoder_set_output_channel(void *_priv, enum audio_channel output_channel)
{
    struct audio_sbc_decoder *sbc_dec = (struct audio_sbc_decoder *)_priv;
    AUDIO_DEC_CH_OPUT_PARA  out_param;
    memset(&out_param, 0x0, sizeof(out_param));

    u8 switch_channel = output_channel;
    if (sbc_dec->output_mode == output_channel) {
        if (sbc_dec->start) {
            return 0;
        }
    } else {
        //不同声道切换时做淡入淡出处理 待完善

    }
    switch (switch_channel) {
    case AUDIO_CH_LR:
    case AUDIO_CH_DUAL_L:
    case AUDIO_CH_DUAL_R:
    case AUDIO_CH_DUAL_LR:
        if (sbc_dec->nch == 2) {
            out_param.mode = SBC_DEC_OPUT_LR;
        } else if (sbc_dec->nch == 1) {
            out_param.mode = SBC_DEC_OPUT_L;
        }
        break;
    case AUDIO_CH_L:
        out_param.mode = SBC_DEC_OPUT_L;
        break;
    case AUDIO_CH_R:
        out_param.mode = SBC_DEC_OPUT_R;
        break;
    case AUDIO_CH_DIFF:
    case AUDIO_CH_MIX:
        if (sbc_dec->nch == 1) {
            out_param.mode = 1;
        } else if (sbc_dec->nch == 2) {
            out_param.mode = SBC_DEC_OPUT_MIX;
            out_param.pL = 8192;
            out_param.pR = 8192;
        }
        break;
    }

    sbc_dec->dec_ops->dec_config(sbc_dec->dc_buf, SET_DEC_CH_MODE, (void *)&out_param);
    printf("sbc_dec->output_mode = 0x%x\n", output_channel);
    sbc_dec->output_mode = output_channel;

    return 0;
}

#endif

/*初始化一些解码器配置，准备开始编码*/
static int audio_sbc_decoder_start(void *priv)
{
    struct audio_sbc_decoder *sbc_dec = (struct audio_sbc_decoder *)priv;

    if (!sbc_dec || sbc_dec->start) {
        return 0;
    }

#if USE_HW_SBC_DECODER
    struct audio_hw_codec_info hw_info;
    int channel_mode = AUDIO_CH_DIFF; //根据需要配置解码输出声道
    if (channel_mode == AUDIO_CH_MIX) {
        channel_mode = AUDIO_CH_DIFF;
    }

    hw_info.format = "sbc";
    hw_info.mode = AUDIO_HW_DEC;
    hw_info.ops = &sbc_output_ops; //配置输出相关接口
    hw_info.priv = sbc_dec;
    hw_info.pcm_format = channel_mode;

    sbc_dec->decoder = sbc_dec->hwaccel_ops->open(&hw_info);
    hw_sbc_set_output_channel(sbc_dec->decoder, channel_mode);

#else

    sbc_dec->dcf_obj.bitwidth = 16;// 解码输出位宽  24bit 或者 16bit
    sbc_dec->dcf_obj.channels = 2;  //  支持的最大声道数
    sbc_dec->dcf_obj.sr = 48000;    //  支持的最大采样率

    sbc_dec->dec_io.priv            = sbc_dec;
    sbc_dec->dec_io.check_buf       = __sbc_check_buf;
    sbc_dec->dec_io.output          = __sbc_output;
    sbc_dec->dec_io.get_lslen       = __sbc_get_lslen;
    sbc_dec->dec_io.store_rev_data  = __sbc_store_rev_data;
    sbc_dec->dec_io.input_fr        = __sbc_input_fr;

    /*
     * 创建解码工作区BUF
     */
    u32 dcbuf_size = sbc_dec->dec_ops->need_dcbuf_size(&sbc_dec->dcf_obj);

    sbc_dec->dc_buf = zalloc(dcbuf_size);

    ASSERT(sbc_dec->dc_buf != NULL);

    //堆栈设置
    u32 stk_size = sbc_dec->dec_ops->need_skbuf_size(&sbc_dec->dcf_obj);
    if (stk_size) {
        sbc_dec->stk_buf = zalloc(stk_size);
        printf("sbc stk_buf_size = %d, buf:0x%p \n", stk_size, sbc_dec->stk_buf);
        ASSERT(sbc_dec->stk_buf != NULL);
    }

    /*
     * 打开解码器
     */
    if (sbc_dec->dec_ops->open) {
        sbc_dec->dec_ops->open(sbc_dec->dc_buf, sbc_dec->stk_buf, &sbc_dec->dec_io, 0, &sbc_dec->dcf_obj);
    }

    AUDIO_DECODE_PARA dec_para;
    dec_para.mode = 0;    //VVVV__0__1
    sbc_dec->dec_ops->dec_config(sbc_dec->dc_buf, SET_SPECIAL_OCH, (void *)&dec_para);


    audio_sbc_decoder_set_output_channel(sbc_dec, AUDIO_CH_DIFF); //设置解码输出声道类型

#endif


    sbc_dec->in_buf = zalloc(sbc_dec->frame_len);
    sbc_dec->start = 1;
    return 0;
}


static int audio_sbc_decoder_set_fmt(void *priv,  sbc_t *sbc_fmt)
{
    struct audio_sbc_decoder *sbc_dec = (struct audio_sbc_decoder *)priv;
    if (sbc_fmt) {
        sbc_dec->param.frequency = sbc_fmt->frequency;
        sbc_dec->param.blocks = sbc_fmt->blocks;
        sbc_dec->param.subbands = sbc_fmt->subbands;
        sbc_dec->param.mode = sbc_fmt->mode;
        sbc_dec->param.allocation = sbc_fmt->allocation;
        sbc_dec->param.endian = sbc_fmt->endian;
        sbc_dec->param.bitpool = sbc_fmt->bitpool;
        /* sbc_dec->channels = (sbc_dec->param.mode  == SBC_MODE_MONO ? 1 : 2); */

        u8 subbands = sbc_dec->param.subbands ? 8 : 4;
        u8 blocks = ((sbc_dec->param.blocks) + 1) * 4;
        u8 channels = sbc_dec->param.mode  == SBC_MODE_MONO ? 1 : 2;
        u8 joint = sbc_dec->param.mode  == SBC_MODE_JOINT_STEREO ? 1 : 0;

        sbc_dec->frame_len = 4 + ((4 * subbands * channels) >> 3);
        if (sbc_dec->param.mode >= SBC_MODE_STEREO) {
            sbc_dec->frame_len += (((joint ? subbands : 0) + blocks * sbc_dec->param.bitpool) + 7) >> 3;
        } else {
            sbc_dec->frame_len += ((blocks * channels * sbc_dec->param.bitpool) + 7) >> 3;
        }
#if USE_HW_SBC_DECODER
        sbc_dec->pcm_len = blocks * subbands * channels * 2; //一帧数据解码出来的pcm 数据长度;
        printf("	sbc_dec->frame_len : %d, sbc_dec->pcm_len: %d\n", sbc_dec->frame_len, sbc_dec->pcm_len);
#else
        sbc_dec->nch =  channels;
        printf("	sbc_dec->frame_len : %d, channels: %d\n", sbc_dec->frame_len, channels);
#endif
    }
    return 0;
}

static int audio_sbc_decoder_run(void *priv)
{
    struct audio_sbc_decoder *sbc_dec = (struct audio_sbc_decoder *)priv;
    int ret  = 0;
    if (sbc_dec->start == 0) {
        return 0;
    }
#if USE_HW_SBC_DECODER
    if (sbc_dec->remain_len) {
        sbc_output_finish(sbc_dec, (u8 *)sbc_dec->pcm_buf + sizeof(sbc_dec->pcm_buf) - sbc_dec->remain_len, sbc_dec->remain_len);
        if (sbc_dec->remain_len) {
            return 0;
        }
    }

    int rlen = audio_demo_dec_read_data(sbc_dec, sbc_dec->in_buf, sbc_dec->frame_len);
    if (!rlen) {
        return 0;
    }


    ret  = sbc_dec->hwaccel_ops->decode(sbc_dec->decoder, sbc_dec->in_buf, sbc_dec->frame_len);
    if (ret) { //错误处理
        printf("sbc_decoder err !!, %d", ret);
        /* } */
    }
#else

    ret = sbc_dec->dec_ops->run(sbc_dec->dc_buf, 0, sbc_dec->stk_buf);
    if (ret) { //错误处理
        if (ret == ERR_NEED_NEW_PACKET || ret == ERR_READ_NO_DATA) { //解码完输出后，会继续读数据解码，直到解码读不到数据或者输出不了
            //未读到数据，不处理
            ret = 0;
        }
        if (ret) {
            printf("sbc_decoder err !!, %d", ret);
        }
    }

#endif
    return ret;
}

static int audio_sbc_decoder_close(void *priv)
{
    struct audio_sbc_decoder *sbc_dec = (struct audio_sbc_decoder *)priv;
    if (sbc_dec) {
        sbc_dec->start = 0;
        if (sbc_dec->in_buf) {
            free(sbc_dec->in_buf);
            sbc_dec->in_buf = NULL;
        }
#if USE_HW_SBC_DECODER
#else
        if (sbc_dec->dc_buf) {
            free(sbc_dec->dc_buf);
            sbc_dec->dc_buf = NULL;
        }
        if (sbc_dec->stk_buf) {
            free(sbc_dec->stk_buf);
            sbc_dec->stk_buf = NULL;
        }
#endif
        free(sbc_dec);
        sbc_dec = NULL;
    }
    return 0;
}


//-----------------------------------------------------------------------------
// mic
//-----------------------------------------------------------------------------
#define ADC_DEMO_CH_NUM        	2	/*支持的最大采样通道(max = 2)*/
#define ADC_DEMO_BUF_NUM        2	/*采样buf数*/
#define ADC_DEMO_IRQ_POINTS     128	/*采样中断点数*/
#define ADC_DEMO_BUFS_SIZE      (ADC_DEMO_CH_NUM * ADC_DEMO_BUF_NUM * ADC_DEMO_IRQ_POINTS)

struct audio_demo_mic_hdl {
    struct audio_adc_output_hdl adc_output;
    struct adc_mic_ch    mic_ch;
    s16 adc_buf[ADC_DEMO_BUFS_SIZE];
    s16 *sample_buffer;                     //单双声道转换缓存
    volatile u8 start;
    u8 out_ch_num;
    u8 mic_nch;
    void *priv;
};

/* #define TCFG_SOUND_SINE_DATA_TEST   1 */

extern struct audio_adc_hdl adc_hdl;


#if (defined TCFG_SOUND_SINE_DATA_TEST && TCFG_SOUND_SINE_DATA_TEST)
static u16 sin_data_offset = 0;
static const s16 sin_48k[48] = {
    0, 2139, 4240, 6270, 8192, 9974, 11585, 12998,
    14189, 15137, 15826, 16244, 16384, 16244, 15826, 15137,
    14189, 12998, 11585, 9974, 8192, 6270, 4240, 2139,
    0, -2139, -4240, -6270, -8192, -9974, -11585, -12998,
    -14189, -15137, -15826, -16244, -16384, -16244, -15826, -15137,
    -14189, -12998, -11585, -9974, -8192, -6270, -4240, -2139
};
static int get_sine_data(s16 *data, u16 points, u8 ch)
{
    while (points--) {
        if (sin_data_offset >= ARRAY_SIZE(sin_48k)) {
            sin_data_offset = 0;
        }
        *data++ = sin_48k[sin_data_offset];
        if (ch == 2) {
            *data++ = sin_48k[sin_data_offset];
        }
        sin_data_offset++;
    }
    return 0;
}
#endif


static void mic_demo_output_data(void *priv, s16 *data, int len)
{
    struct audio_demo_mic_hdl *mic_hdl = (struct audio_demo_mic_hdl *)priv;
    int wlen = 0;
    /* y_printf("mic:0x%x, len=%d\n",data,len); */
    if (!mic_hdl || !mic_hdl->start) {
        return;
    }

#if (defined TCFG_SOUND_SINE_DATA_TEST && TCFG_SOUND_SINE_DATA_TEST)
    get_sine_data(data, len / 2, 1);
#endif

    if (mic_hdl->mic_nch == 1 && mic_hdl->out_ch_num >= 2) { //硬件配置单声道，需要输出双声道
        if (!mic_hdl->sample_buffer) {
            mic_hdl->sample_buffer = zalloc(len * 2);
        }
        pcm_single_to_dual(mic_hdl->sample_buffer, data, len);
        wlen = audio_demo_pcm_data_write(mic_hdl->priv, mic_hdl->sample_buffer, len * 2);
        if (wlen < len * 2) {
            putchar('N');
        }
    } else {
        wlen = audio_demo_pcm_data_write(mic_hdl->priv, data, len);
        if (wlen < len) {
            putchar('L');
        }
    }

    if (mic_hdl->priv) {
        audio_demo_enc_resume(mic_hdl->priv);
    }
}


static void audio_demo_mic_close(void *priv)
{
    struct audio_demo_mic_hdl *mic_hdl = (struct audio_demo_mic_hdl *)priv;
    if (!mic_hdl) {
        return ;
    }
    mic_hdl->start = 0;
    if (mic_hdl->sample_buffer) {
        free(mic_hdl->sample_buffer);
        mic_hdl->sample_buffer = NULL;
    }
    audio_adc_del_output_handler(&adc_hdl, &mic_hdl->adc_output);
    audio_adc_mic_close(&mic_hdl->mic_ch);
    local_irq_disable();
    free(mic_hdl);
    mic_hdl = NULL;
    local_irq_enable();
}

static void *audio_demo_mic_open(void *priv, int sample_rate, u8 gain, u8 out_ch_num)
{

    struct audio_demo_mic_hdl *mic_demo = zalloc(sizeof(struct audio_demo_mic_hdl));
    printf("mic_demo = 0x%x\n", (int)mic_demo);
    ASSERT(mic_demo);
    mic_demo->out_ch_num = out_ch_num;
    mic_demo->priv = priv;
    //printf("JL_AUDIO->ADC_CON = 0x%x\n", JL_AUDIO->ADC_CON);

    struct mic_open_param mic_param = {0};
    mic_param.mic_mode      = AUDIO_MIC_CAP_MODE;
    mic_param.mic_ain_sel   = AUDIO_MIC0_CH0;
    mic_param.mic_bias_sel  = AUDIO_MIC_BIAS_CH0;
    mic_param.mic_bias_rsel = 4;
    mic_param.mic_dcc       = 8;
#if 1 //可视化分支重置读到的mic配置，改成打开一个adc 通道
    memset(adc_hdl.adc_sel, 0xff, sizeof(adc_hdl.adc_sel));
    adc_hdl.max_adc_num = 0;
    audio_adc_add_ch(&adc_hdl, 0);
#endif
    audio_adc_mic_open(&mic_demo->mic_ch, AUDIO_ADC_MIC_0, &adc_hdl, &mic_param);
    audio_adc_mic_set_gain(&mic_demo->mic_ch, AUDIO_ADC_MIC_0, gain);
    audio_adc_mic_gain_boost(AUDIO_ADC_MIC_0, 1);
    mic_demo->mic_nch = 1;


    //step1:设置mic通道采样率
    audio_adc_mic_set_sample_rate(&mic_demo->mic_ch, sample_rate);
    //step2:设置mic采样buf
    audio_adc_mic_set_buffs(&mic_demo->mic_ch, mic_demo->adc_buf, ADC_DEMO_IRQ_POINTS * 2, ADC_DEMO_BUF_NUM);
    printf("mic_demo->adc_buf = 0x%x\n", (int)mic_demo->adc_buf);
    //step3:设置mic采样输出回调函数
    mic_demo->adc_output.priv    = mic_demo;
    mic_demo->adc_output.handler = mic_demo_output_data;
    audio_adc_add_output_handler(&adc_hdl, &mic_demo->adc_output);
    //step4:启动mic通道采样
    audio_adc_mic_start(&mic_demo->mic_ch);

    mic_demo->start = 1;

    return mic_demo;
}


//////////////////////////////////////////////////////////////
//
// 测试流程： adc->pcm_data->enc->enc_data->dec->pcm_data->dac
//
//////////////////////////////////////////////////////////////


#define AUDIO_DEMO_PCM_BUF_LEN		(4*1024)
#define AUDIO_DEMO_ENC_BUF_LEN		(1024)

#define AUDIO_sbc_ENCODER_RUN             0
#define AUDIO_sbc_DECODER_RUN             1
#define AUDIO_DEMO_CODEC_TASK              "audio_codec_task"

struct audio_demo_enc_dec_hdl {
    struct audio_sbc_encoder *enc;
    struct audio_sbc_decoder *dec;
    struct audio_demo_mic_hdl *mic;

    cbuffer_t pcm_cbuf; //数据交互buf;
    int pcm_buf[AUDIO_DEMO_PCM_BUF_LEN / 4];
    cbuffer_t enc_cbuf;
    int enc_buf[AUDIO_DEMO_ENC_BUF_LEN / 4];
    u8 start;
};

//驱动编码
static int audio_demo_enc_resume(void *priv)
{
    struct audio_demo_enc_dec_hdl *hdl = (struct audio_demo_enc_dec_hdl *)priv;

    return  os_taskq_post_msg(AUDIO_DEMO_CODEC_TASK, 2, AUDIO_sbc_ENCODER_RUN, (int)hdl);

}

//驱动解码
static int audio_demo_dec_resume(void *priv)
{
    struct audio_demo_enc_dec_hdl *hdl = (struct audio_demo_enc_dec_hdl *)priv;
    return  os_taskq_post_msg(AUDIO_DEMO_CODEC_TASK, 2, AUDIO_sbc_DECODER_RUN, (int)hdl);

}

//保存pcm 数据
static int audio_demo_pcm_data_write(void *priv, void *data, u16 len)
{
    struct audio_demo_enc_dec_hdl *hdl = (struct audio_demo_enc_dec_hdl *)priv;

    if (!hdl || (!hdl->start)) {
        return 0;
    }
    /* y_printf("adc pcm write: len=%d\n",len); */

    int wlen = cbuf_write(&hdl->pcm_cbuf, data, len);
    if (wlen != len) {
        putchar('W');
    }

    return wlen;
}

//读取pcm 数据
static int audio_demo_pcm_data_read(void *priv, void *buf, u16 len)
{
    struct audio_demo_enc_dec_hdl *hdl = (struct audio_demo_enc_dec_hdl *)priv;

    if (!hdl || (!hdl->start)) {
        return 0;
    }

    /* y_printf("adc pcm read: len=%d, %d\n",len, cbuf_get_data_size(&hdl->pcm_cbuf)); */
    int rlen = cbuf_read(&hdl->pcm_cbuf, buf, len);

    return rlen;
}

//保存编码数据
static int audio_demo_enc_data_write(void *priv, void *data, u16 len)
{
    struct audio_demo_enc_dec_hdl *hdl = (struct audio_demo_enc_dec_hdl *)priv;
    if (!hdl || (!hdl->start)) {
        return 0;
    }
    int wlen = cbuf_write(&hdl->enc_cbuf, data, len);
    if (wlen != len) {
        printf("enc_w : %d--%d\n", len, wlen);

    }

    return wlen;

}


//读取编码数据
static int audio_demo_enc_data_read(void *priv, void *buf, u16 len)
{
    struct audio_demo_enc_dec_hdl *hdl = (struct audio_demo_enc_dec_hdl *)priv;
    if (!hdl || (!hdl->start)) {
        return 0;
    }
    int rlen = cbuf_read(&hdl->enc_cbuf, buf, len);


    return rlen;
}

static void audio_codec_test_task(void *priv)
{
    struct audio_demo_enc_dec_hdl *hdl;
    int res = 0;
    int msg[16];
    u8 pend_taskq = 1;
    while (1) {
        res = os_taskq_pend("taskq", msg, ARRAY_SIZE(msg));
        if (res == OS_TASKQ) {
            switch (msg[1]) {
            case AUDIO_sbc_ENCODER_RUN:
                hdl = (struct audio_demo_enc_dec_hdl *)msg[2];
                putchar('E');
                audio_sbc_encoder_run(hdl->enc);  //编码主函数
                break;
            case AUDIO_sbc_DECODER_RUN:
                hdl = (struct audio_demo_enc_dec_hdl *)msg[2];
                putchar('D');
                audio_sbc_decoder_run(hdl->dec);  //解码主函数
                break;
            }

        }
    }
}

static int audio_sbc_enc_get_rate(u8 frequency)
{
    int sr = 44100;
    switch (frequency) {
    case SBC_FREQ_16000:
        sr = 16000;
        break;
    case SBC_FREQ_32000:
        sr = 32000;
        break;
    case SBC_FREQ_44100:
        sr = 44100;
        break;
    case SBC_FREQ_48000:
        sr = 48000;
        break;
    }
    return sr;
}
////////////*  test   *//////////////

int audio_sbc_enc_dec_test_open(void)
{
    struct audio_demo_enc_dec_hdl *hdl = zalloc(sizeof(struct audio_demo_enc_dec_hdl));
    ASSERT(hdl != NULL);
    cbuf_init(&hdl->pcm_cbuf, hdl->pcm_buf, AUDIO_DEMO_PCM_BUF_LEN);  //pcm 数据交互buf
    cbuf_init(&hdl->enc_cbuf, hdl->enc_buf, AUDIO_DEMO_ENC_BUF_LEN);  //编码数据交互buf

    /*配置编解码格式信息: 所需格式参数 不同格式需要的参数可能不一致，不限于下面几种*/
    sbc_t sbc_param = {0};
    sbc_param.frequency = SBC_FREQ_44100;
    sbc_param.blocks = SBC_BLK_16;
    sbc_param.subbands = SBC_SB_8;
    sbc_param.mode = SBC_MODE_MONO;
    sbc_param.allocation = 0;
    sbc_param.endian = SBC_LE;
    sbc_param.bitpool = 32;

    struct audio_fmt fmt = {0};
    fmt.coding_type =  AUDIO_CODING_SBC;
    fmt.sample_rate = audio_sbc_enc_get_rate(sbc_param.frequency);
    ;
    fmt.channel = (sbc_param.mode == SBC_MODE_MONO ? 1 : 2);

    printf("fmt: 0x%x,%d,%d,%d\n", fmt.coding_type, fmt.sample_rate, fmt.frame_len, fmt.channel);

    // 开编码流程，配置相关参数
    hdl->enc =  audio_sbc_encoder_open(hdl);
    if (!hdl->enc) {
        printf("audio_demo_enc_open failed\n");
    }
    audio_sbc_encoder_set_fmt(hdl->enc, &sbc_param);
    audio_sbc_encoder_start(hdl->enc);


    // 开解码流程，配置相关参数
    hdl->dec =  audio_sbc_decoder_open(hdl);
    if (!hdl->enc) {
        printf("audio_demo_dec_open failed\n");

    }
    audio_sbc_decoder_set_fmt(hdl->dec, &sbc_param);
    audio_sbc_decoder_start(hdl->dec);

    //开mic,产生pcm 数据
    hdl->mic = audio_demo_mic_open(hdl, fmt.sample_rate, 10, fmt.channel);
    if (!hdl->mic) {
        printf("audio_demo_mic_open failed\n");
    }

    //开dac, 播放数据
    audio_demo_dac_open(fmt.sample_rate);

    extern int clk_set_api(const char *name, u32 frequency);
    clk_set_api("sys", 128 * 1000000L); //设置时钟。

    hdl->start = 1;
    int ret = os_task_create(audio_codec_test_task, hdl, 5, 512, 32, AUDIO_DEMO_CODEC_TASK);

    printf("== audio_demo_enc_dec_test_open\n");

    return 0;

}

int audio_sbc_enc_dec_test_close(void *priv)
{
    struct audio_demo_enc_dec_hdl *hdl = (struct audio_demo_enc_dec_hdl *)priv;

    hdl->start = 0;
    audio_demo_mic_close(hdl->mic);
    audio_sbc_encoder_close(hdl->enc);
    audio_sbc_decoder_close(hdl->dec);
    os_task_del(AUDIO_DEMO_CODEC_TASK);
    audio_demo_dac_close();
    free(hdl);
    return 0;
}


#endif
