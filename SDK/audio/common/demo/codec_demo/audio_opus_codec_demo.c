//////////////////////////////////////////////////////////////
/*
	demo流程： adc->pcm_data->enc->enc_data->dec->pcm_data->dac

 	demo :测试opus编解码流程

       本demo 仅做编码一帧解码一帧的流程，如果适配到流程有拼帧拆帧及头信息检查问题，请自行处理
	   并且解码支持几帧数据同时输入解码.多帧数据可以一次给解码解码。


	测试流程仅作为编解码测试所用,
	注意： 整个流程是根据adc 的节奏进行运行，adc 产生一帧数据，编码一次，解码一次，输出到dac,
 		   所以要保证adc 产生的数据正好够一帧数据(ADC_DEMO_IRQ_POINTS 配置adc 起一次中断出来的点数)，
		   才能稳定运行，要注意声道,采样率和中断节奏的适配,
*/
//const int OPUS_SRINDEX = 0; //选择opus解码文件的帧大小，0代表一帧40字节，1代表一帧80字节，2代表一帧160字节
//////////////////////////////////////////////////////////////
#include "audio_splicing.h"
#include "system/includes.h"
#include "cpu/includes.h"
#include "app_main.h"
#include "audio_config.h"
#include "audio_base.h"
#include "audio_adc.h"
#include "audio_dac.h"
#include "codec/opus_codec_api.h"
#include "codec/audio_decode_common_api.h"

#if 0   //这里打开代码模块使能

extern const int OPUS_SRINDEX; //选择opus解码文件的帧大小，0代表一帧40字节，1代表一帧80字节，2代表一帧160字节

extern const u8 OPUS_ENC_CELT_EN ;
static int audio_demo_enc_resume(void *priv);
static int audio_demo_dec_resume(void *priv);
static int audio_demo_pcm_data_write(void *priv, void *data, u16 len);
static int audio_demo_pcm_data_read(void *priv, void *buf, u16 len);
static int audio_demo_enc_data_write(void *priv, void *data, u16 len);
static int audio_demo_enc_data_read(void *priv, void *buf, u16 len);

extern const int CONFIG_DEC_SUPPORT_CHANNELS;
extern const int CONFIG_DEC_SUPPORT_SAMPLERATE;
extern const int OPUS_SRINDEX ; //选择opus解码文件的帧大小，0代表一帧40字节，1代表一帧80字节，2代表一帧160字节

extern const int CONFIG_OGG_OPUS_DEC_SUPPORT;
extern const int CONFIG_OGG_OPUS_DEC_SET_RAW_MODE;
extern const int CONFIG_OGG_OPUS_DEC_SET_CBR_PACKET_LEN;

extern decoder_ops_t *get_ogg_opus_ops(); //ogg_opus 文件类解码,及特定封装流解码
extern decoder_ops_t *get_opusdec_ops();

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


struct audio_opus_encoder {
    void *priv;
    u8 start;

    //编码器的结构 ,不同的编码器不一样;
    OPUS_ENC_OPS *ops;
    OPUS_EN_FILE_IO IO;
    OPUS_ENC_PARA param;
    u8 *run_buf;
    u16 sample_rate;
    s16 indata_len;
    s16 *indata;
};


//读入待编码的pcm数据
static u16 audio_opus_enc_input_data(void *priv, s16 *buf, u8 channel, u16 len)//此处的len 表示点数
{
    struct audio_opus_encoder *opus_enc = (struct audio_opus_encoder *)priv;

    int rlen = 0;
    if (opus_enc->param.nch == 1) {
        rlen =  audio_demo_pcm_data_read(opus_enc->priv, buf, len << 1);
    } else if (OPUS_ENC_CELT_EN) { //双声道且支持celt 编码使能
        if (opus_enc->indata_len < (len << 2)) {
            if (opus_enc->indata) {
                free(opus_enc->indata);
                opus_enc->indata = NULL;
            }
            opus_enc->indata_len = len << 2;
            if (!opus_enc->indata) {
                opus_enc->indata = malloc(opus_enc->indata_len);
            }
        }
        int i = 0;
        if (channel == 0) {
            rlen = audio_demo_pcm_data_read(opus_enc->priv, (u8 *)opus_enc->indata, len << 2);
            if (rlen < (len << 2)) {
                return 0;
            }
            for (i = 0; i < len; i++) {
                buf[i] = opus_enc->indata[i << 1];
            }
        } else {
            for (i = 0; i < len; i++) {
                buf[i] = opus_enc->indata[(i << 1) + 1];
            }
        }
        return len;
    }


    /* printf("enc input : %d---%d",len,rlen); */

    return rlen >> 1;
}
//编码数据输出
static u32 audio_opus_enc_output_data(void *priv, u8 *buf, u16 len)
{
    struct audio_opus_encoder *opus_enc = (struct audio_opus_encoder *)priv;

    int wlen = audio_demo_enc_data_write(opus_enc->priv, buf, len);
    /* printf("enc out len: %d-%d",len,wlen); */

    if (opus_enc->priv) {
        audio_demo_dec_resume(opus_enc->priv);
    }
    return wlen;

}




/*打开编码器，申请资源*/
static void *audio_opus_encoder_open(void *priv)
{
    struct audio_opus_encoder *opus_enc = NULL;

    printf("---  opus_enc open\n");
    opus_enc = zalloc(sizeof(*opus_enc));
    if (opus_enc) {
        opus_enc->priv = priv;
        return opus_enc;
    }

    printf(" opus encoder open errer \n");
    return NULL;
}


/*初始化一些编码器配置，准备开始编码*/
static int audio_opus_encoder_start(void *priv)
{
    struct audio_opus_encoder *opus_enc = (struct audio_opus_encoder *)priv;
    if (!opus_enc || opus_enc->start) {
        return 0;
    }
    if (OPUS_ENC_CELT_EN) {
        opus_enc->ops = get_opus_stenc_ops();
    } else {
        opus_enc->ops = get_opus_enc_ops();
    }
    opus_enc->param.complexity = (opus_enc->param.complexity > 4) ? 4 : opus_enc->param.complexity;

    if (OPUS_ENC_CELT_EN) {
        opus_enc->param.frame_ms = (opus_enc->param.frame_ms == 10 \
                                    || opus_enc->param.frame_ms == 20) ? opus_enc->param.frame_ms : 20 ; //帧长只能是10,20ms
    } else {
        opus_enc->param.frame_ms = (opus_enc->param.frame_ms == 20 \
                                    || opus_enc->param.frame_ms == 40 \
                                    || opus_enc->param.frame_ms == 60 \
                                    || opus_enc->param.frame_ms == 80 \
                                    || opus_enc->param.frame_ms == 100) ? opus_enc->param.frame_ms : 20 ; //帧长只能是20,40,60,80,100
    }
    opus_enc->param.format_mode = (opus_enc->param.format_mode >  3) ? 1 : opus_enc->param.format_mode ;
    printf("opus_open_param:%d,%d,%d,%d,%d,%d\n", opus_enc->param.sr, opus_enc->param.br, opus_enc->param.format_mode, opus_enc->param.complexity, opus_enc->param.frame_ms, opus_enc->param.nch);
    u32 need_buf_size = opus_enc->ops->need_buf(&opus_enc->param);
    printf("opus enc buf size %d\n", need_buf_size);
    opus_enc->run_buf = zalloc(need_buf_size);
    if (opus_enc->run_buf) {
        opus_enc->IO.input_data  = audio_opus_enc_input_data;
        opus_enc->IO.output_data = audio_opus_enc_output_data;
        opus_enc->IO.priv = opus_enc;
        opus_enc->ops->open(opus_enc->run_buf, &opus_enc->IO, &(opus_enc->param));
        opus_enc->start = 1;
        return 0;
    } else {
        printf("opus_encoder_start err \n no buf \n");
        return -ENOMEM;
    }
}

static int audio_opus_encoder_set_fmt(void *priv,  struct audio_fmt *fmt)
{
    struct audio_opus_encoder *opus_enc = (struct audio_opus_encoder *)priv;
    if (opus_enc) {
        opus_enc->param.sr = fmt->sample_rate;
        opus_enc->param.br = fmt->bit_rate;
        opus_enc->param.complexity = fmt->complexity;
        opus_enc->param.frame_ms = fmt->frame_len;
        opus_enc->param.format_mode = *((u16 *)fmt->priv);
        opus_enc->param.nch = fmt->channel;
        printf("\n opus enc complex:%d sr:%d,%d,%d\n", opus_enc->param.complexity, opus_enc->sample_rate, opus_enc->param.br, opus_enc->param.sr);
    }
    return 0;
}


/*编码运行函数，包括读pcm 数据，进行编码，编码输出,错误处理等*/
/*有些拜编码器需要外部读数据，有些编码器自己有接口读数据，需要不同处理*/
static int audio_opus_encoder_run(void *priv)
{
    struct audio_opus_encoder *opus_enc = (struct audio_opus_encoder *)priv;

    int wlen = 0;

    if (opus_enc->start == 0) {
        return 0;
    }
    if (opus_enc->ops) {
        opus_enc->ops->run(opus_enc->run_buf);
    }

    return 0;

}

static int audio_opus_encoder_close(void *priv)
{
    struct audio_opus_encoder *opus_enc = (struct audio_opus_encoder *)priv;
    if (opus_enc) {
        opus_enc->start = 0;
        if (opus_enc->run_buf) {
            free(opus_enc->run_buf);
            opus_enc->run_buf = NULL;
        }

        free(opus_enc);
        opus_enc = NULL;
    }
    return 0;

}


//-----------------------------------------------------------------------------
//   decoder demo
//-----------------------------------------------------------------------------


struct audio_opus_decoder {
    void *priv;
    u8 start;
    u8  output_mode;

    //解码器的结构 ,不同的解码器不一样;
    opuslib_dec_ops *opus_ops;
    deccfg_inf dcf_obj;
    void *dc_buf;
    u16 frame_len;
    u8 *in_buf;
    u16 output_len;
    u8 *out_buf;
    u16 sample_rate;
    u8 quality;
    u16 remain_len;
    u16 sr;
    u8 *remain_addr;

    void *stk_buf;
    union {
        struct { // data_type == AUDIO_INPUT_FILE
            struct if_decoder_io dec_io;
            struct decoder_inf *info;
            decoder_ops_t *opus_ops;
        } file;
        struct { // data_type == AUDIO_INPUT_FRAME
            opuslib_dec_ops *opus_ops;
            s16 *obuf;
            u16 total_len;
        } frame;
    } ops;

};

//读数据到解码器，数据写入buf, 读取长度是len
static u16 audio_demo_dec_read_data(void *priv, u8 *buf, u16 len)
{
    struct audio_opus_decoder *opus_dec = (struct audio_opus_decoder *)priv;

    int rlen = audio_demo_enc_data_read(opus_dec->priv, buf, len);

    /* printf("dec input :%d--%d\n",len,rlen); */

    return rlen ;
}




//解码数据输出，数据输出不完，下次run的时候会先输出剩余数据，
static u16 audio_demo_dec_output_data(void *priv, u8 *data, u16 len)
{
    struct audio_opus_decoder *opus_dec = (struct audio_opus_decoder *)priv;
    int wlen = 0;

    wlen = audio_demo_dac_write(NULL, data, len);
    /* printf("dec out len : %d=%d\n",len, wlen); */

    return len;
}


/*打开解码器，申请资源*/
void *audio_opus_decoder_open(void *priv)
{
    struct audio_opus_decoder *opus_dec = NULL;
    opus_dec = zalloc(sizeof(struct audio_opus_decoder));
    if (opus_dec) {
        opus_dec->priv = priv;
        opus_dec->dcf_obj.bitwidth = 16;// 解码输出位宽  24bit 或者 16bit
        return opus_dec;
    }
    printf(" opus decoder open errer \n");
    return NULL;

}
static int __opus_input(void *priv, u32 addr, void *buf, int len, u8 type)
{
    struct audio_opus_decoder *opus_dec = (struct audio_opus_decoder *)priv;

    int rlen = audio_demo_dec_read_data(opus_dec, buf, len);
    if (!rlen) {
        return 0;
    }
    return rlen;
}

/*
 * 检查数据是否完毕
 */
static int __opus_check_buf(void *priv, u32 addr, void *buf)
{

    int rlen = __opus_input(priv, addr, buf, 512, 0);
    if (rlen != 512) {
        printf("__opus_check_buf read rlen= %d, len= %d\n", rlen, 512);
    }
    return rlen;
}


/*
 * 解码后的数据输出
 */
static int __opus_output(void *priv, void *data, int len)
{

    int olen = audio_demo_dec_output_data(priv, data, len);
    return olen;
}


/*
 * 获取文件长度
 */
static u32 __opus_get_lslen(void *priv)
{
    return 0x7fffffff;
}


static u32 __opus_store_rev_data(void *priv, u32 addr, int len)
{
    printf("__opus_store_rev_data..:len=%d\n", len);
    return len;
}



/*初始化一些解码器配置，准备开始编码*/
static int audio_opus_decoder_start(void *priv)
{
    struct audio_opus_decoder *opus_dec = (struct audio_opus_decoder *)priv;


    AUDIO_DECODE_PARA dec_para;
    if (!opus_dec || opus_dec->start) {
        return 0;
    }

    if (CONFIG_OGG_OPUS_DEC_SUPPORT) {
        if (1) {
            opus_dec->ops.file.opus_ops = get_ogg_opus_ops();
        } else {//这个是解码非ogg文件的
            opus_dec->ops.file.opus_ops = get_opusdec_ops();
        }
        ASSERT(opus_dec->ops.file.opus_ops != NULL);
        opus_dec->dcf_obj.bitwidth = 16;//   24bit 或者 16bit
        opus_dec->dcf_obj.channels = CONFIG_DEC_SUPPORT_CHANNELS;  //  支持的最大声道数
        opus_dec->dcf_obj.sr = CONFIG_DEC_SUPPORT_SAMPLERATE;    //  支持的最大采样率

        /*配置输入输出接口*/
        opus_dec->ops.file.dec_io.priv = opus_dec;
        opus_dec->ops.file.dec_io.input = __opus_input;
        opus_dec->ops.file.dec_io.output = __opus_output;
        opus_dec->ops.file.dec_io.check_buf = __opus_check_buf;
        opus_dec->ops.file.dec_io.get_lslen = __opus_get_lslen;
        opus_dec->ops.file.dec_io.store_rev_data = __opus_store_rev_data;


        /*
         * 创建解码工作区BUF
         */
        int dcbuf_size = opus_dec->ops.file.opus_ops->need_dcbuf_size(&opus_dec->dcf_obj);
        opus_dec->dc_buf = malloc(dcbuf_size);

        printf("dcbuf_size = %d, buf:0x%p \n", dcbuf_size, opus_dec->dc_buf);
        ASSERT(opus_dec->dc_buf != NULL);

        //堆栈设置
        int stk_size = opus_dec->ops.file.opus_ops->need_skbuf_size(&opus_dec->dcf_obj);
        if (stk_size) {
            opus_dec->stk_buf = zalloc(stk_size);
            printf("opus stk_buf_size = %d, buf:0x%p \n", stk_size, opus_dec->stk_buf);
            ASSERT(opus_dec->stk_buf != NULL);
        }
        /*
         * 打开解码器
         */
        if (opus_dec->ops.file.opus_ops->open) {
            opus_dec->ops.file.opus_ops->open(opus_dec->dc_buf, opus_dec->stk_buf, &opus_dec->ops.file.dec_io, NULL, &opus_dec->dcf_obj);
        }

        dec_para.mode = 1;
        opus_dec->ops.file.opus_ops->dec_config(opus_dec->dc_buf, SET_DECODE_MODE, &dec_para);

        if (CONFIG_OGG_OPUS_DEC_SUPPORT) {
            if (CONFIG_OGG_OPUS_DEC_SET_RAW_MODE) {
                opus_dec->ops.file.opus_ops->dec_config(opus_dec->dc_buf, SET_OPUS_RAWDTF, NULL);
            } else if (CONFIG_OGG_OPUS_DEC_SET_CBR_PACKET_LEN) {
                AUDIO_OPUS_PKTLEN opus_pktlen = {0};
                opus_pktlen.opus_pkt_len =  CONFIG_OGG_OPUS_DEC_SET_CBR_PACKET_LEN;
                int err = opus_dec->ops.file.opus_ops->dec_config(opus_dec->dc_buf, SET_OPUS_CBR_PKTLEN, &opus_pktlen);
                if (err) {
                    printf("opus dec set cbr_packet_len error \n");
                }
            }
        } else {
            BR_CONTEXT br_obj;
            br_obj.br_index = OPUS_SRINDEX;
            opus_dec->ops.file.opus_ops->dec_config(opus_dec->dc_buf, SET_DEC_SR, &br_obj);
        }

        /*
         * 格式检查
         */
        if (opus_dec->ops.file.opus_ops->format_check) {
            int err = opus_dec->ops.file.opus_ops->format_check(opus_dec->dc_buf, opus_dec->stk_buf);
            if (err) {
                printf("opus format_check err:0x%x\n", err);
                /* if (config_opus_dec_use_malloc) { */
                free(opus_dec->dc_buf);
                opus_dec->dc_buf = NULL;
                return err;
            }
            /* ASSERT(err == 0, "opus format_check err:%d!\n", err); */
        }

        /*
         * 获取解码信息
         */

        if (0) { //文件解码
            if (opus_dec->ops.file.opus_ops->get_dec_inf) {
                opus_dec->ops.file.info = opus_dec->ops.file.opus_ops->get_dec_inf(opus_dec->dc_buf);
                printf("opus info.sr = %d\n", opus_dec->ops.file.info->sr);
                printf("opus info.br = %d\n", opus_dec->ops.file.info->br);
                printf("opus info.nch = %d\n", opus_dec->ops.file.info->nch);
                printf("opus info.total_time = %d\n", opus_dec->ops.file.info->total_time);
                opus_dec->sr = opus_dec->ops.file.info->br;
            }
        } else {
            opus_dec->sr = 48000;
        }
    } else {
        opus_dec->opus_ops = getopuslibdec_ops();
        int dcbuf_size = opus_dec->opus_ops->need_buf(opus_dec->dcf_obj.bitwidth);
        opus_dec->dc_buf = zalloc(dcbuf_size);
        ASSERT(opus_dec->dc_buf != NULL);

        if (opus_dec->sample_rate == 8000) {
            opus_dec->output_len = 160 * 2 ;
        } else {
            opus_dec->output_len = 320 * 2;
        }
        if (OPUS_SRINDEX == 1) { //OPUS_SRINDEX 对应编码的quality
            opus_dec->frame_len = 80; //编码帧的长度。
        } else if (OPUS_SRINDEX == 2) {
            opus_dec->frame_len = 160;
        } else {
            opus_dec->frame_len = 40;
        }

        opus_dec->in_buf = zalloc(opus_dec->frame_len);
        opus_dec->out_buf = zalloc(opus_dec->output_len);

        /*
         * 打开解码器
         */
        if (opus_dec->opus_ops->open) {
            opus_dec->opus_ops->open((u32 *)opus_dec->dc_buf, OPUS_SRINDEX, opus_dec->dcf_obj.bitwidth);
        }

    }

    opus_dec->start = 1;
    return 0;
}


static int audio_opus_decoder_set_fmt(void *priv,  u16 sample_rate, u8 quality)
{
    struct audio_opus_decoder *opus_dec = (struct audio_opus_decoder *)priv;
    if (opus_dec) {
        opus_dec->sample_rate = sample_rate;
        opus_dec->quality = quality;
        printf("opus_dec sample_rate : %d, quality: %d\n", opus_dec->sample_rate, opus_dec->quality);
    }
    return 0;
}

static int audio_opus_decoder_run(void *priv)
{
    struct audio_opus_decoder *opus_dec = (struct audio_opus_decoder *)priv;
    int ret  = 0;
    int wlen = 0;
    if (opus_dec->start == 0) {
        return 0;
    }

    if (CONFIG_OGG_OPUS_DEC_SUPPORT) {
        if (opus_dec->ops.file.opus_ops->run) {
            ret = opus_dec->ops.file.opus_ops->run(opus_dec->dc_buf, 0, opus_dec->stk_buf);
            //数据流类型解码错误返回继续解码
            if (ret && ret != 0x40) {	 //非读不到数据的错误，打印出来解码错误类型
                printf("opus_dec error: %d\n", ret);
            }
            ret = 0;
        }
    } else {
        if (opus_dec->remain_len) {
            wlen = audio_demo_dec_output_data(opus_dec->priv, opus_dec->remain_addr, opus_dec->remain_len);
            opus_dec->remain_len = opus_dec->output_len - wlen;
            opus_dec->remain_addr = (u8 *)opus_dec->out_buf - wlen;
            if (opus_dec->remain_len) {
                return 0;
            }
        }

        int rlen = audio_demo_dec_read_data(opus_dec, opus_dec->in_buf, opus_dec->frame_len);
        if (!rlen) {
            return 0;
        }

        ret = opus_dec->opus_ops->run(opus_dec->dc_buf, (char *)opus_dec->in_buf, (short *)opus_dec->out_buf);
        if (ret != opus_dec->output_len / 2) { //错误处理
            printf("opus_decoder err !!, %d", ret);
        }


        wlen = audio_demo_dec_output_data(opus_dec->priv, opus_dec->out_buf, opus_dec->output_len);
        opus_dec->remain_len = opus_dec->output_len - wlen;
        opus_dec->remain_addr = (u8 *)opus_dec->out_buf - wlen;

    }

    return ret;
}

static int audio_opus_decoder_close(void *priv)
{
    struct audio_opus_decoder *opus_dec = (struct audio_opus_decoder *)priv;
    if (opus_dec) {
        opus_dec->start = 0;
        if (opus_dec->in_buf) {
            free(opus_dec->in_buf);
            opus_dec->in_buf = NULL;
        }
        if (opus_dec->out_buf) {
            free(opus_dec->out_buf);
            opus_dec->out_buf = NULL;
        }
        if (opus_dec->dc_buf) {
            free(opus_dec->dc_buf);
            opus_dec->dc_buf = NULL;
        }

        if (opus_dec->stk_buf) {
            free(opus_dec->stk_buf);
        }

        free(opus_dec);
        opus_dec = NULL;
    }
    return 0;
}


//-----------------------------------------------------------------------------
// mic
//-----------------------------------------------------------------------------
#define ADC_DEMO_CH_NUM        	2	/*支持的最大采样通道(max = 2)*/
#define ADC_DEMO_BUF_NUM        2	/*采样buf数*/
#define ADC_DEMO_IRQ_POINTS     320	/*采样中断点数*/
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

#define TCFG_SOUND_SINE_DATA_TEST   1 // 将adc 中断的数据替换为正弦波数据

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

#define AUDIO_opus_ENCODER_RUN             0
#define AUDIO_opus_DECODER_RUN             1
#define AUDIO_DEMO_CODEC_TASK              "audio_codec_task"

struct audio_demo_enc_dec_hdl {
    struct audio_opus_encoder *enc;
    struct audio_opus_decoder *dec;
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

    return  os_taskq_post_msg(AUDIO_DEMO_CODEC_TASK, 2, AUDIO_opus_ENCODER_RUN, (int)hdl);

}

//驱动解码
static int audio_demo_dec_resume(void *priv)
{
    struct audio_demo_enc_dec_hdl *hdl = (struct audio_demo_enc_dec_hdl *)priv;
    return  os_taskq_post_msg(AUDIO_DEMO_CODEC_TASK, 2, AUDIO_opus_DECODER_RUN, (int)hdl);

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
#if 1
    /* y_printf("adc pcm read: len=%d, %d\n",len, cbuf_get_data_size(&hdl->pcm_cbuf)); */
    int rlen = cbuf_read(&hdl->pcm_cbuf, buf, len);
    return rlen;
#else
    get_sine_data((s16 *)buf, len / 2, 1);
    return len;
#endif

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
            case AUDIO_opus_ENCODER_RUN:
                hdl = (struct audio_demo_enc_dec_hdl *)msg[2];
                putchar('E');
                audio_opus_encoder_run(hdl->enc);  //编码主函数
                break;
            case AUDIO_opus_DECODER_RUN:
                hdl = (struct audio_demo_enc_dec_hdl *)msg[2];
                putchar('D');
                audio_opus_decoder_run(hdl->dec);  //解码主函数
                break;
            }

        }
    }
}

static void enc_resume_timer(void *arg)
{
    audio_demo_enc_resume(arg);
}


////////////*  test   *//////////////

int audio_opus_enc_dec_test_open(void)
{
    struct audio_demo_enc_dec_hdl *hdl = zalloc(sizeof(struct audio_demo_enc_dec_hdl));
    ASSERT(hdl != NULL);

    cbuf_init(&hdl->pcm_cbuf, hdl->pcm_buf, AUDIO_DEMO_PCM_BUF_LEN);  //pcm 数据交互buf
    cbuf_init(&hdl->enc_cbuf, hdl->enc_buf, AUDIO_DEMO_ENC_BUF_LEN);  //编码数据交互buf

    /*配置编解码格式信息: 所需格式参数 不同格式需要的参数可能不一致，不限于下面几种*/


    struct audio_fmt fmt = {0};
    fmt.coding_type =  AUDIO_CODING_OPUS;
    fmt.sample_rate = 16000; //没有使能OPUS_ENC_CELT_EN仅支持8K 16K采样率,使能以后支持8k,16k,24k,48k
    fmt.complexity = 0 ;//范围0到3,3质量最好
    fmt.frame_len =  20; //没有使能OPUS_ENC_CELT_EN :20,40,60,80,100,如果选60以上,需要把编码输入buffer 和 编码输出buffer都改大一点，避免输出不了,注意此处单位为ms，例如20表示帧长为20ms
    //使能OPUS_ENC_CELT_EN : 10,20
    u8 enc_priv = 0; //opus 编码format: //0:百度_无头.                   1:酷狗_eng+range. 2:ogg封装,pc软件可播放.  3:size+rangeFinal. 源码可兼容版本.
    fmt.priv = &enc_priv;
    fmt.channel = 1;
    fmt.bit_rate = 16000; //16000,320000,640000 这三个码率分别对应非ogg解码库的 OPUS_SRINDEX 值为0,1,2

    printf("fmt: 0x%x,%d,%d,%d\n", fmt.coding_type, fmt.sample_rate, fmt.frame_len, fmt.channel);

    // 开编码流程，配置相关参数
    hdl->enc =  audio_opus_encoder_open(hdl);
    if (!hdl->enc) {
        printf("audio_demo_enc_open failed\n");
    }
    audio_opus_encoder_set_fmt(hdl->enc, &fmt);
    audio_opus_encoder_start(hdl->enc);


    // 开解码流程，配置相关参数
    hdl->dec =  audio_opus_decoder_open(hdl);
    if (!hdl->enc) {
        printf("audio_demo_dec_open failed\n");

    }
    audio_opus_decoder_set_fmt(hdl->dec, fmt.sample_rate, fmt.quality);
    audio_opus_decoder_start(hdl->dec);

#if 1
    //开mic,产生pcm 数据
    hdl->mic = audio_demo_mic_open(hdl, fmt.sample_rate, 10, fmt.channel);
    if (!hdl->mic) {
        printf("audio_demo_mic_open failed\n");
    }
#else
    sys_timer_add(hdl->enc, enc_resume_timer, fmt.frame_len);
#endif

    //开dac, 播放数据
    audio_demo_dac_open(fmt.sample_rate);

    extern int clk_set_api(const char *name, u32 frequency);
    clk_set_api("sys", 128 * 1000000L); //设置时钟。

    hdl->start = 1;
    int ret = os_task_create(audio_codec_test_task, hdl, 5, 1024, 32, AUDIO_DEMO_CODEC_TASK);

    printf("== audio_demo_enc_dec_test_open\n");

    return 0;

}

int audio_opus_enc_dec_test_close(void *priv)
{
    struct audio_demo_enc_dec_hdl *hdl = (struct audio_demo_enc_dec_hdl *)priv;

    hdl->start = 0;
    audio_demo_mic_close(hdl->mic);
    audio_opus_encoder_close(hdl->enc);
    audio_opus_decoder_close(hdl->dec);
    os_task_del(AUDIO_DEMO_CODEC_TASK);
    audio_demo_dac_close();
    free(hdl);
    return 0;
}


#endif
