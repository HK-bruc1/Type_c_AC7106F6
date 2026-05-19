//////////////////////////////////////////////////////////////
/*
	demo流程： adc->pcm_data->enc->enc_data->dec->pcm_data->dac

 	demo :测试speex编解码流程

       本demo 仅做编码一帧解码一帧的流程，如果适配到流程有拼帧拆帧及头信息检查问题，请自行处理
	   并且解码支持几帧数据同时输入解码.多帧数据可以一次给解码解码。


	测试流程仅作为编解码测试所用,
	注意： 整个流程是根据adc 的节奏进行运行，adc 产生一帧数据，编码一次，解码一次，输出到dac,
 		   所以要保证adc 产生的数据正好够一帧数据(ADC_DEMO_IRQ_POINTS 配置adc 起一次中断出来的点数)，
		   才能稳定运行，要注意声道,采样率和中断节奏的适配,
*/
//	6, 6, 6, 20, 20, 28, 38, 46, 62, 62,    //8k采样率，qualtiy从0到9的帧长
//	10, 6, 10, 25, 32, 42, 52, 60, 70, 86    //16k采样率，qualtiy从0到9的帧长
//////////////////////////////////////////////////////////////
#include "audio_splicing.h"
#include "system/includes.h"
#include "cpu/includes.h"
#include "app_main.h"
#include "audio_config.h"
#include "audio_base.h"
#include "audio_adc.h"
#include "audio_dac.h"
#include "codec/speex_codec_api.h"
#include "codec/audio_decode_common_api.h"

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


struct audio_speex_encoder {
    void *priv;
    u8 start;

    //编码器的结构 ,不同的编码器不一样;
    speex_enc_ops *ops;
    SPEEX_EN_FILE_IO IO;
    u8 *run_buf;
    u16 sample_rate;
    u8 quality;
};


//读入待编码的pcm数据
static u16 audio_speex_enc_input_data(void *priv, s16 *buf, u16 len) //此处的len 表示点数
{
    struct audio_speex_encoder *speex_enc = (struct audio_speex_encoder *)priv;

    int rlen =  audio_demo_pcm_data_read(speex_enc->priv, buf, len << 1);

    /* printf("enc input : %d---%d",len,rlen); */

    return rlen >> 1;
}
//编码数据输出
static void audio_speex_enc_output_data(void *priv, u8 *buf, u16 len)
{
    struct audio_speex_encoder *speex_enc = (struct audio_speex_encoder *)priv;

    int wlen = audio_demo_enc_data_write(speex_enc->priv, buf, len);
    /* printf("enc out len: %d-%d",len,wlen); */

    if (speex_enc->priv) {
        audio_demo_dec_resume(speex_enc->priv);
    }

}




/*打开编码器，申请资源*/
static void *audio_speex_encoder_open(void *priv)
{
    struct audio_speex_encoder *speex_enc = NULL;

    printf("---  speex_enc open\n");
    speex_enc = zalloc(sizeof(*speex_enc));
    if (speex_enc) {
        speex_enc->priv = priv;
        return speex_enc;
    }

    printf(" speex encoder open errer \n");
    return NULL;
}


/*初始化一些编码器配置，准备开始编码*/
static int audio_speex_encoder_start(void *priv)
{
    struct audio_speex_encoder *speex_enc = (struct audio_speex_encoder *)priv;
    if (!speex_enc || speex_enc->start) {
        return 0;
    }
    speex_enc->ops = get_speex_enc_obj();
    u32 need_buf_size = speex_enc->ops->need_buf(speex_enc->sample_rate);
    printf("speex enc buf size %d\n", need_buf_size);
    speex_enc->run_buf = zalloc(need_buf_size);
    if (speex_enc->run_buf) {
        speex_enc->IO.input_data  = audio_speex_enc_input_data;
        speex_enc->IO.output_data = audio_speex_enc_output_data;
        speex_enc->IO.priv = speex_enc;
        speex_enc->ops->open(speex_enc->run_buf, &speex_enc->IO, speex_enc->quality, speex_enc->sample_rate, 0);
        speex_enc->start = 1;
        return 0;
    } else {
        printf("speex_encoder_start err \n no buf \n");
        return -ENOMEM;
    }
}

static int audio_speex_encoder_set_fmt(void *priv,  u16 sample_rate, u8 quality)
{
    struct audio_speex_encoder *speex_enc = (struct audio_speex_encoder *)priv;
    if (speex_enc) {
        speex_enc->sample_rate = sample_rate;
        speex_enc->quality = quality;
        printf("speex_enc sr：%d, quality: %d\n", speex_enc->sample_rate, speex_enc->quality);
    }
    return 0;
}


/*编码运行函数，包括读pcm 数据，进行编码，编码输出,错误处理等*/
/*有些拜编码器需要外部读数据，有些编码器自己有接口读数据，需要不同处理*/
static int audio_speex_encoder_run(void *priv)
{
    struct audio_speex_encoder *speex_enc = (struct audio_speex_encoder *)priv;

    int wlen = 0;

    if (speex_enc->start == 0) {
        return 0;
    }
    if (speex_enc->ops) {
        speex_enc->ops->run(speex_enc->run_buf);
    }

    return 0;

}

static int audio_speex_encoder_close(void *priv)
{
    struct audio_speex_encoder *speex_enc = (struct audio_speex_encoder *)priv;
    if (speex_enc) {
        speex_enc->start = 0;
        if (speex_enc->run_buf) {
            free(speex_enc->run_buf);
            speex_enc->run_buf = NULL;
        }

        free(speex_enc);
        speex_enc = NULL;
    }
    return 0;

}


//-----------------------------------------------------------------------------
//   decoder demo
//-----------------------------------------------------------------------------


struct audio_speex_decoder {
    void *priv;
    u8 start;
    u8  output_mode;

    //解码器的结构 ,不同的解码器不一样;
    speex_dec_ops *speex_ops;
    deccfg_inf dcf_obj;
    void *dc_buf;
    u16 frame_len;
    u8 *in_buf;
    u16 output_len;
    u8 *out_buf;
    u16 sample_rate;
    u8 quality;
    u16 remain_len;
    u8 *remain_addr;


};

//读数据到解码器，数据写入buf, 读取长度是len
static u16 audio_demo_dec_read_data(void *priv, u8 *buf, u16 len)
{
    struct audio_speex_decoder *speex_dec = (struct audio_speex_decoder *)priv;

    int rlen = audio_demo_enc_data_read(speex_dec->priv, buf, len);

    /* printf("dec input :%d--%d\n",len,rlen); */

    return rlen ;
}




//解码数据输出，数据输出不完，下次run的时候会先输出剩余数据，
static u16 audio_demo_dec_output_data(void *priv, u8 *data, u16 len)
{
    struct audio_speex_decoder *speex_dec = (struct audio_speex_decoder *)priv;
    int wlen = 0;

    wlen = audio_demo_dac_write(NULL, data, len);
    /* printf("dec out len : %d=%d\n",len, wlen); */

    return len;
}


/*打开解码器，申请资源*/
void *audio_speex_decoder_open(void *priv)
{
    struct audio_speex_decoder *speex_dec = NULL;
    speex_dec = zalloc(sizeof(struct audio_speex_decoder));
    if (speex_dec) {
        speex_dec->priv = priv;
        speex_dec->dcf_obj.bitwidth = 16;// 解码输出位宽  24bit 或者 16bit
        return speex_dec;
    }
    printf(" speex decoder open errer \n");
    return NULL;

}


/*初始化一些解码器配置，准备开始编码*/
static int audio_speex_decoder_start(void *priv)
{
    struct audio_speex_decoder *speex_dec = (struct audio_speex_decoder *)priv;

    u8 speex_frame_len_sr8k[] = { 6, 6, 6, 20, 20, 28, 38, 46, 62, 62, 62, };
    u8 speex_frame_len_sr16k[] = { 10, 6, 10, 25, 32, 42, 52, 60, 70, 86, 106 };

    if (!speex_dec || speex_dec->start) {
        return 0;
    }

    speex_dec->speex_ops = get_speex_dec_obj();

    int dcbuf_size = speex_dec->speex_ops->need_buf(speex_dec->dcf_obj.bitwidth);
    speex_dec->dc_buf = zalloc(dcbuf_size);
    ASSERT(speex_dec->dc_buf != NULL);

    if (speex_dec->sample_rate == 8000) {
        speex_dec->output_len = 160 * 2 ;
        speex_dec->frame_len = speex_frame_len_sr8k[speex_dec->quality];
    } else {
        speex_dec->output_len = 320 * 2;
        speex_dec->frame_len = speex_frame_len_sr16k[speex_dec->quality];
    }
    speex_dec->in_buf = zalloc(speex_dec->frame_len);
    speex_dec->out_buf = zalloc(speex_dec->output_len);
    speex_dec->speex_ops->set_maxframelen(speex_dec->dc_buf, speex_dec->frame_len);



    /*
     * 打开解码器
     */
    if (speex_dec->speex_ops->open) {
        speex_dec->speex_ops->open((u32 *)speex_dec->dc_buf, speex_dec->sample_rate, speex_dec->dcf_obj.bitwidth);
    }


    speex_dec->start = 1;
    return 0;
}


static int audio_speex_decoder_set_fmt(void *priv,  u16 sample_rate, u8 quality)
{
    struct audio_speex_decoder *speex_dec = (struct audio_speex_decoder *)priv;
    if (speex_dec) {
        speex_dec->sample_rate = sample_rate;
        speex_dec->quality = quality;
        printf("speex_dec sample_rate : %d, quality: %d\n", speex_dec->sample_rate, speex_dec->quality);
    }
    return 0;
}

static int audio_speex_decoder_run(void *priv)
{
    struct audio_speex_decoder *speex_dec = (struct audio_speex_decoder *)priv;
    int ret  = 0;
    int wlen = 0;
    if (speex_dec->start == 0) {
        return 0;
    }

    if (speex_dec->remain_len) {
        wlen = audio_demo_dec_output_data(speex_dec->priv, speex_dec->remain_addr, speex_dec->remain_len);
        speex_dec->remain_len = speex_dec->output_len - wlen;
        speex_dec->remain_addr = (u8 *)speex_dec->out_buf - wlen;
        if (speex_dec->remain_len) {
            return 0;
        }
    }

    int rlen = audio_demo_dec_read_data(speex_dec, speex_dec->in_buf, speex_dec->frame_len);
    if (!rlen) {
        return 0;
    }

    ret = speex_dec->speex_ops->run(speex_dec->dc_buf, (char *)speex_dec->in_buf, (short *)speex_dec->out_buf);
    if (ret != speex_dec->output_len / 2) { //错误处理
        printf("speex_decoder err !!, %d", ret);
    }


    wlen = audio_demo_dec_output_data(speex_dec->priv, speex_dec->out_buf, speex_dec->output_len);
    speex_dec->remain_len = speex_dec->output_len - wlen;
    speex_dec->remain_addr = (u8 *)speex_dec->out_buf - wlen;


    return ret;
}

static int audio_speex_decoder_close(void *priv)
{
    struct audio_speex_decoder *speex_dec = (struct audio_speex_decoder *)priv;
    if (speex_dec) {
        speex_dec->start = 0;
        if (speex_dec->in_buf) {
            free(speex_dec->in_buf);
            speex_dec->in_buf = NULL;
        }
        if (speex_dec->out_buf) {
            free(speex_dec->out_buf);
            speex_dec->out_buf = NULL;
        }
        if (speex_dec->dc_buf) {
            free(speex_dec->dc_buf);
            speex_dec->dc_buf = NULL;
        }
        free(speex_dec);
        speex_dec = NULL;
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

// #define TCFG_SOUND_SINE_DATA_TEST   1 // 将adc 中断的数据替换为正弦波数据

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

#define AUDIO_speex_ENCODER_RUN             0
#define AUDIO_speex_DECODER_RUN             1
#define AUDIO_DEMO_CODEC_TASK              "audio_codec_task"

struct audio_demo_enc_dec_hdl {
    struct audio_speex_encoder *enc;
    struct audio_speex_decoder *dec;
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

    return  os_taskq_post_msg(AUDIO_DEMO_CODEC_TASK, 2, AUDIO_speex_ENCODER_RUN, (int)hdl);

}

//驱动解码
static int audio_demo_dec_resume(void *priv)
{
    struct audio_demo_enc_dec_hdl *hdl = (struct audio_demo_enc_dec_hdl *)priv;
    return  os_taskq_post_msg(AUDIO_DEMO_CODEC_TASK, 2, AUDIO_speex_DECODER_RUN, (int)hdl);

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
            case AUDIO_speex_ENCODER_RUN:
                hdl = (struct audio_demo_enc_dec_hdl *)msg[2];
                putchar('E');
                audio_speex_encoder_run(hdl->enc);  //编码主函数
                break;
            case AUDIO_speex_DECODER_RUN:
                hdl = (struct audio_demo_enc_dec_hdl *)msg[2];
                putchar('D');
                audio_speex_decoder_run(hdl->dec);  //解码主函数
                break;
            }

        }
    }
}

////////////*  test   *//////////////

int audio_speex_enc_dec_test_open(void)
{
    struct audio_demo_enc_dec_hdl *hdl = zalloc(sizeof(struct audio_demo_enc_dec_hdl));
    ASSERT(hdl != NULL);

    cbuf_init(&hdl->pcm_cbuf, hdl->pcm_buf, AUDIO_DEMO_PCM_BUF_LEN);  //pcm 数据交互buf
    cbuf_init(&hdl->enc_cbuf, hdl->enc_buf, AUDIO_DEMO_ENC_BUF_LEN);  //编码数据交互buf

    /*配置编解码格式信息: 所需格式参数 不同格式需要的参数可能不一致，不限于下面几种*/


    struct audio_fmt fmt = {0};
    fmt.coding_type =  AUDIO_CODING_SPEEX;
    fmt.sample_rate = 16000; //仅支持8K 16K采样率
    fmt.quality = 5; // speex的码率,范围0到9,值越大,质量越好,编解码越慢
    fmt.channel = 1;

    printf("fmt: 0x%x,%d,%d,%d\n", fmt.coding_type, fmt.sample_rate, fmt.frame_len, fmt.channel);

    // 开编码流程，配置相关参数
    hdl->enc =  audio_speex_encoder_open(hdl);
    if (!hdl->enc) {
        printf("audio_demo_enc_open failed\n");
    }
    audio_speex_encoder_set_fmt(hdl->enc, fmt.sample_rate, fmt.quality);
    audio_speex_encoder_start(hdl->enc);


    // 开解码流程，配置相关参数
    hdl->dec =  audio_speex_decoder_open(hdl);
    if (!hdl->enc) {
        printf("audio_demo_dec_open failed\n");

    }
    audio_speex_decoder_set_fmt(hdl->dec, fmt.sample_rate, fmt.quality);
    audio_speex_decoder_start(hdl->dec);

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

int audio_speex_enc_dec_test_close(void *priv)
{
    struct audio_demo_enc_dec_hdl *hdl = (struct audio_demo_enc_dec_hdl *)priv;

    hdl->start = 0;
    audio_demo_mic_close(hdl->mic);
    audio_speex_encoder_close(hdl->enc);
    audio_speex_decoder_close(hdl->dec);
    os_task_del(AUDIO_DEMO_CODEC_TASK);
    audio_demo_dac_close();
    free(hdl);
    return 0;
}


#endif
