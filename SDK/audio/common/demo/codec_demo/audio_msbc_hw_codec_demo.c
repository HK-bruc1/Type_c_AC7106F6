//////////////////////////////////////////////////////////////
/*
	demo流程： adc->pcm_data->enc->enc_data->dec->pcm_data->dac

	demo : 硬件编解码流程，仅适用于标准msbc 参数参数，如需适配不同的参数，请使用软件编解码
		   单声道  16k采样率   bitpool=26   子带=8  block=15
		   pcm 240byte 编码出来 58个byte

	注意： 这里仅测试msbc 编解码的流程，并未添加msbc 编码的头信息，及解码的头信息检查，
           如需添加，可以流程说明处自行根据需要添加即可


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

#define MSBC_ENCODER_INBUF_LEN	240	//输入长度
#define MSBC_FRAME_LEN          58  //编码帧长

#if 0   //这里打开代码模块使能



/*编码参数，不同的类型，使用的参数也不同，有些参数是一些类型特有的*/
struct audio_demo_codec_params {
    u8  ch_num;        // 声道数
    u16 frame_len;      //帧长时间，dci-ms
    u32 coding_type;
    u32 sample_rate;
    u32 bit_rate;
};

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


struct audio_msbc_encoder {
    void *priv;
    /* u8 *enc_buf;//编码用的work_buf */
    /* u8 *stk_buf;//编码需要的堆栈scratch */
    u8 start;

    //编码器的结构 ,不同的编码器不一样;
    const struct audio_hwaccel_ops *hwaccel_ops;
    u16 remain_len;
    u8 *remain_addr;
    u8 seqN;
    u8 *inbuf;
    u8 *outbuf;
    void *encoder;

};


//读入待编码的pcm数据
static u16 audio_msbc_enc_read_data(void *priv, u8 *buf, u16 len)
{
    struct audio_msbc_encoder *msbc_enc = (struct audio_msbc_encoder *)priv;

    int rlen =  audio_demo_pcm_data_read(msbc_enc->priv, buf, len);

    /* printf("enc input : %d---%d",len,rlen); */

    return rlen;
}
//编码数据输出
static u16 audio_msbc_enc_output_data(void *priv, u8 *buf, u16 len)
{
    struct audio_msbc_encoder *msbc_enc = (struct audio_msbc_encoder *)priv;

    int wlen = audio_demo_enc_data_write(msbc_enc->priv, buf, len);
    /* printf("enc out len: %d-%d",len,wlen); */

    if (msbc_enc->priv) {
        audio_demo_dec_resume(msbc_enc->priv);
    }

    return wlen;
}




/*打开编码器，申请资源*/
static void *audio_msbc_encoder_open(void *priv)
{
    struct audio_msbc_encoder *msbc_enc = NULL;
    struct audio_hwaccel_ops *p;

    /**先检查有没有硬件加速(编解码)器*/
    list_for_each_codec_hwaccel(p) {
        if (strcmp(p->name, "sbc") == 0) {
            msbc_enc = zalloc(sizeof(*msbc_enc));

            msbc_enc->hwaccel_ops = p;
            msbc_enc->priv = priv;
            return msbc_enc;
        }
    }
    printf("no hwaccel_ops \n  msbc encoder open errer \n");
    return NULL;
}


/*初始化一些编码器配置，准备开始编码*/
static int audio_msbc_encoder_start(void *priv)
{
    struct audio_msbc_encoder *msbc_enc = (struct audio_msbc_encoder *)priv;
    struct audio_hw_codec_info hw_info;
    if (msbc_enc->start) {
        return 0;
    }
    hw_info.format = "msbc";
    hw_info.mode = AUDIO_HW_ENC;
    hw_info.priv = msbc_enc;
    hw_info.pcm_format = AUDIO_CH_L; //仅支持单声道编码，声道设置无效,可以不用配置

    msbc_enc->inbuf = zalloc(MSBC_ENCODER_INBUF_LEN);
    msbc_enc->outbuf = zalloc(MSBC_FRAME_LEN);

    //硬件解码
    msbc_enc->encoder = msbc_enc->hwaccel_ops->open(&hw_info);
    if (!msbc_enc->encoder) {
        return -EFAULT;
    }
    msbc_enc->start = 1;
    return 0;
}


/*编码运行函数，包括读pcm 数据，进行编码，编码输出,错误处理等*/
/*有些拜编码器需要外部读数据，有些编码器自己有接口读数据，需要不同处理*/
static int audio_msbc_encoder_run(void *priv)
{
    struct audio_msbc_encoder *msbc_enc = (struct audio_msbc_encoder *)priv;

    struct audio_codec_frame iframe; //编码输入数据
    struct audio_codec_frame oframe; //编码输出数据
    int wlen = 0;

    if (msbc_enc->start == 0) {
        return 0;
    }

    if (msbc_enc->remain_len) { //如果上次的数据还未输出完，要先输出上次的数据，
        wlen = audio_msbc_enc_output_data(msbc_enc, msbc_enc->remain_addr, msbc_enc->remain_len);
        msbc_enc->remain_len -= wlen;
        msbc_enc->remain_addr += wlen;
        if (msbc_enc->remain_len) {
            return 0;
        }
    }

    int rlen = audio_msbc_enc_read_data(msbc_enc, msbc_enc->inbuf, MSBC_ENCODER_INBUF_LEN);
    if (!rlen) {
        return 0;
    }

    /*配置硬件解码输入输出数据*/
    iframe.data = msbc_enc->inbuf;
    iframe.size = MSBC_ENCODER_INBUF_LEN;

    oframe.data = msbc_enc->outbuf; //偏移2个byte 放同步头信息
    oframe.size = MSBC_FRAME_LEN;

    int output_len = msbc_enc->hwaccel_ops->encode_frame(msbc_enc->encoder, &iframe, &oframe); //编码
    /* y_printf("enc out : %d\n",output_len); */
    if (output_len < 0) {
        return output_len; //编码出错，可以根据返回值做一些错误处理
    }

    /*如需加同步头信息，在编码完成后加,加完在向后级输出*/


    wlen = audio_msbc_enc_output_data(msbc_enc, msbc_enc->outbuf, output_len);
    if (wlen != output_len) { //如果后级写不进去，需要处理数据remain的情况
        msbc_enc->remain_len = 	output_len	- wlen;
        msbc_enc->remain_addr = msbc_enc->outbuf + wlen;
    }

    return 0;

}

static int audio_msbc_encoder_close(void *priv)
{
    struct audio_msbc_encoder *msbc_enc = (struct audio_msbc_encoder *)priv;
    if (msbc_enc) {
        msbc_enc->start = 0;
        if (msbc_enc->inbuf) {
            free(msbc_enc->inbuf);
            msbc_enc->inbuf = NULL;
        }
        if (msbc_enc->outbuf) {
            free(msbc_enc->outbuf);
            msbc_enc->outbuf = NULL;
        }
        free(msbc_enc);
        msbc_enc = NULL;
    }
    return 0;

}


//-----------------------------------------------------------------------------
//   decoder demo
//-----------------------------------------------------------------------------

struct audio_msbc_decoder {
    void *priv;
    /* u8 *dec_buf;//解码用的work_buf */
    /* u8 *stk_buf;//解码需要的堆栈scratch */
    u8 start;
    u8  output_mode;

    //解码器的结构 ,不同的解码器不一样;
    void *decoder;
    const struct audio_hwaccel_ops *hwaccel_ops;
    u8 inbuf[(MSBC_FRAME_LEN + 3) / 4 * 4];     // 4byte align 输入buffer
    s16 pcm_buf[128];	//输出buffer
    s16 remain_len;		//上一包剩余未消耗的长度

};

//读数据到解码器，数据写入buf, 读取长度是len
static u16 audio_demo_dec_read_data(void *priv, u8 *buf, u16 len)
{
    struct audio_msbc_decoder *msbc_dec = (struct audio_msbc_decoder *)priv;

    int rlen = audio_demo_enc_data_read(msbc_dec->priv, buf, len);

    /* printf("dec input :%d--%d\n",len,rlen); */

    return rlen ;
}




//解码数据输出，数据输出不完，下次run的时候会先输出剩余数据，
static u16 audio_demo_dec_output_data(void *priv, u8 *data, u16 len)
{
    struct audio_msbc_decoder *msbc_dec = (struct audio_msbc_decoder *)priv;
    int wlen = 0;

    wlen = audio_demo_dac_write(NULL, data, len);
    /* printf("dec out len : %d=%d\n",len, wlen); */

    return len;
}


/*打开解码器，申请资源*/
static void *audio_msbc_decoder_open(void *priv)
{
    struct audio_msbc_decoder *msbc_dec = NULL;
    struct audio_hwaccel_ops *p;
    msbc_dec = zalloc(sizeof(struct audio_msbc_decoder));
    if (msbc_dec) {
        list_for_each_codec_hwaccel(p) {
            if (strcmp(p->name, "sbc") == 0) {
                msbc_dec->hwaccel_ops = p;
                msbc_dec->priv = priv;
                return msbc_dec;
            }
        }
    }
    printf("no hwaccel_ops \n  msbc decoder open errer \n");
    return NULL;

}

static void *msbc_output_alloc(void *priv, int size)
{
    return NULL;
}

static void *msbc_output_alloc_free_space(void *priv, int *size)
{
    struct audio_msbc_decoder *msbc_dec = (struct audio_msbc_decoder *)priv;

    *size = sizeof(msbc_dec->pcm_buf);
    return msbc_dec->pcm_buf;
}

static int msbc_output_finish(void *priv, void *buf, int size)
{
    struct audio_msbc_decoder *msbc_dec = (struct audio_msbc_decoder *)priv;

    int wlen = audio_demo_dec_output_data(msbc_dec->priv, buf, size);
    msbc_dec->remain_len = size - wlen;

    return wlen;
}
static const struct audio_stream_ops msbc_output_ops = {
    .alloc = msbc_output_alloc,
    .alloc_free_space = msbc_output_alloc_free_space,
    .finish = msbc_output_finish,
};

/*初始化一些解码器配置，准备开始编码*/
static int audio_msbc_decoder_start(void *priv)
{
    struct audio_msbc_decoder *msbc_dec = (struct audio_msbc_decoder *)priv;
    struct audio_hw_codec_info hw_info;

    if (msbc_dec->start) {
        return 0;
    }

    hw_info.format = "msbc";
    hw_info.mode = AUDIO_HW_DEC;
    hw_info.ops = &msbc_output_ops; //配置输出相关接口
    hw_info.priv = msbc_dec;
    hw_info.pcm_format = AUDIO_CH_MIX;
    msbc_dec->decoder = msbc_dec->hwaccel_ops->open(&hw_info);


    msbc_dec->start = 1;
    return 0;
}


static int audio_msbc_decoder_run(void *priv)
{
    struct audio_msbc_decoder *msbc_dec = (struct audio_msbc_decoder *)priv;

    if (msbc_dec->start == 0) {
        return 0;
    }

    if (msbc_dec->remain_len) {
        msbc_output_finish(msbc_dec, (u8 *)msbc_dec->pcm_buf + sizeof(msbc_dec->pcm_buf) - msbc_dec->remain_len, msbc_dec->remain_len);
        if (msbc_dec->remain_len) {
            return 0;
        }
    }

    int rlen = audio_demo_dec_read_data(msbc_dec, msbc_dec->inbuf, MSBC_FRAME_LEN);
    if (!rlen) {
        return 0;
    }

    /*如需做头部检查，做拼帧的操作，在读数据这里做，拼成完成一帧给解码器*/

    int ret  = msbc_dec->hwaccel_ops->decode(msbc_dec->decoder, msbc_dec->inbuf, MSBC_FRAME_LEN);
    if (ret) { //错误处理
        printf("msbc_decoder err !!, %d", ret);
        /* } */
    }

    return ret;
}

static int audio_msbc_decoder_close(void *priv)
{
    struct audio_msbc_decoder *msbc_dec = (struct audio_msbc_decoder *)priv;
    if (msbc_dec) {
        msbc_dec->start = 0;
        free(msbc_dec);
        msbc_dec = NULL;
    }
    return 0;
}


//-----------------------------------------------------------------------------
// mic
//-----------------------------------------------------------------------------
#define ADC_DEMO_CH_NUM        	2	/*支持的最大采样通道(max = 2)*/
#define ADC_DEMO_BUF_NUM        2	/*采样buf数*/
#define ADC_DEMO_IRQ_POINTS     120	/*采样中断点数*/
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

#define TCFG_SOUND_PCM_DELAY_TEST   1
extern struct audio_adc_hdl adc_hdl;


#if (defined TCFG_SOUND_PCM_DELAY_TEST && TCFG_SOUND_PCM_DELAY_TEST)
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

#if (defined TCFG_SOUND_PCM_DELAY_TEST && TCFG_SOUND_PCM_DELAY_TEST)
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

#define AUDIO_MSBC_ENCODER_RUN             0
#define AUDIO_MSBC_DECODER_RUN             1
#define AUDIO_DEMO_CODEC_TASK              "audio_codec_task"

struct audio_demo_enc_dec_hdl {
    struct audio_msbc_encoder *enc;
    struct audio_msbc_decoder *dec;
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

    return  os_taskq_post_msg(AUDIO_DEMO_CODEC_TASK, 2, AUDIO_MSBC_ENCODER_RUN, (int)hdl);

}

//驱动解码
static int audio_demo_dec_resume(void *priv)
{
    struct audio_demo_enc_dec_hdl *hdl = (struct audio_demo_enc_dec_hdl *)priv;
    return  os_taskq_post_msg(AUDIO_DEMO_CODEC_TASK, 2, AUDIO_MSBC_DECODER_RUN, (int)hdl);

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
            case AUDIO_MSBC_ENCODER_RUN:
                hdl = (struct audio_demo_enc_dec_hdl *)msg[2];
                putchar('E');
                audio_msbc_encoder_run(hdl->enc);  //编码主函数
                break;
            case AUDIO_MSBC_DECODER_RUN:
                hdl = (struct audio_demo_enc_dec_hdl *)msg[2];
                putchar('D');
                audio_msbc_decoder_run(hdl->dec);  //解码主函数
                break;
            }

        }
    }
}

////////////*  test   *//////////////
int audio_msbc_hw_enc_dec_test_open(void)
{
    struct audio_demo_enc_dec_hdl *hdl = zalloc(sizeof(struct audio_demo_enc_dec_hdl));
    ASSERT(hdl != NULL);
    cbuf_init(&hdl->pcm_cbuf, hdl->pcm_buf, AUDIO_DEMO_PCM_BUF_LEN);  //pcm 数据交互buf
    cbuf_init(&hdl->enc_cbuf, hdl->enc_buf, AUDIO_DEMO_ENC_BUF_LEN);  //编码数据交互buf

    /*配置编解码格式信息: hw_msbc 参数是固定的不用配置编解码参数*/

    struct audio_fmt fmt = {0};
    fmt.sample_rate = 16000;
    fmt.channel = 1;

    printf("fmt: 0x%x,%d,%d,%d\n", fmt.coding_type, fmt.sample_rate, fmt.frame_len, fmt.channel);

    // 开编码流程，配置相关参数
    hdl->enc =  audio_msbc_encoder_open(hdl);
    if (!hdl->enc) {
        printf("audio_demo_enc_open failed\n");
    }
    audio_msbc_encoder_start(hdl->enc);


    // 开解码流程，配置相关参数
    hdl->dec =  audio_msbc_decoder_open(hdl);
    if (!hdl->enc) {
        printf("audio_demo_dec_open failed\n");

    }
    audio_msbc_decoder_start(hdl->dec);

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

int audio_msbc_hw_enc_dec_test_close(void *priv)
{
    struct audio_demo_enc_dec_hdl *hdl = (struct audio_demo_enc_dec_hdl *)priv;

    hdl->start = 0;
    audio_demo_mic_close(hdl->mic);
    audio_msbc_encoder_close(hdl->enc);
    audio_msbc_decoder_close(hdl->dec);
    os_task_del(AUDIO_DEMO_CODEC_TASK);
    audio_demo_dac_close();
    free(hdl);
    return 0;
}

#endif

