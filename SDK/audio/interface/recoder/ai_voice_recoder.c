#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".ai_voice_recoder.data.bss")
#pragma data_seg(".ai_voice_recoder.data")
#pragma const_seg(".ai_voice_recoder.text.const")
#pragma code_seg(".ai_voice_recoder.text")
#endif
#include "jlstream.h"
#include "ai_voice_recoder.h"
#include "encoder_node.h"
#include "audio_dac.h"
#include "audio_cvp.h"
#include "cvp_node.h"

struct ai_voice_recoder {
    struct jlstream *stream;
};

static struct ai_voice_recoder *g_ai_voice_recoder = NULL;


static void ai_voice_recoder_callback(void *private_data, int event)
{
    struct ai_voice_recoder *recoder = g_ai_voice_recoder;
    struct jlstream *stream = (struct jlstream *)private_data;

    switch (event) {
    case STREAM_EVENT_START:
        break;
    }
}

void ai_voice_recoder_set_ai_tx_node_func(int (*func)(u8 *, u32))
{
    struct ai_voice_recoder *recoder = g_ai_voice_recoder;
    if (recoder && recoder->stream) {
        jlstream_node_ioctl(recoder->stream, NODE_UUID_AI_TX, NODE_IOC_SET_PRIV_FMT, (int)func);
    }
}

int ai_voice_recoder_open(u32 code_type, u8 ai_type)
{
    int err;
    struct stream_fmt fmt;
    struct encoder_fmt enc_fmt;
    struct ai_voice_recoder *recoder;
    struct stream_fmt s_fmt;
    struct cvp_param_fmt cvp_fmt;
    u16 source_uuid = 0;

    if (g_ai_voice_recoder) {
        return -EBUSY;
    }
    u16 uuid = jlstream_event_notify(STREAM_EVENT_GET_PIPELINE_UUID, (int)"ai_voice");
    if (uuid == 0) {
        return -EFAULT;
    }

    recoder = malloc(sizeof(*recoder));
    if (!recoder) {
        return -ENOMEM;
    }

    recoder->stream = jlstream_pipeline_parse(uuid, NODE_UUID_ADC);
    source_uuid = NODE_UUID_ADC;
    if (!recoder->stream) {
        recoder->stream = jlstream_pipeline_parse(uuid, NODE_UUID_PDM_MIC);
        source_uuid = NODE_UUID_PDM_MIC;
    }
    if (!recoder->stream) {
        recoder->stream = jlstream_pipeline_parse(uuid, NODE_UUID_IIS0_RX);
        source_uuid = NODE_UUID_IIS0_RX;
    }
    if (!recoder->stream) {
        err = -ENOMEM;
        goto __exit0;
    }
    switch (code_type) {
    case AUDIO_CODING_OPUS:
        //  bitrate
        //     16000,32000,64000 这三个码率分别对应非ogg解码库
        //     的 OPUS_SRINDEX 值为0,1,2
        //  format
        //     0:百度_无头.
        //     1:酷狗_eng+range.
        //     2:ogg封装,pc软件可播放.
        //     3:size+rangeFinal. 源码可兼容版本.
        //  complexity
        //     0|1|2|3     3质量最好.速度要求最高.
        //  frame_ms (由frame_dms / 10得出)
        //     20|40|60|80|100 ms.
        //  sample_rate
        //     sample_rate=16k         ignore
        //
        //   注意
        //   1. struct encoder_fmt是配置编码器私有参数
        //   有效的参数：
        //   complexity, format, frame_dms
        //   不起效的参数：
        //   bit_rate, sample_rate, ch_num, bit_width
        //   不起效参数只用作阅读，需要修改请到音频流程图的”编码器“节点修改
        enc_fmt.bit_rate = 16000;
        enc_fmt.complexity = 0;
        enc_fmt.sample_rate = 16000;
        enc_fmt.format = (ai_type >> 6);   //传入的ai_type实参是bit7:6，用于选择封装格式，兼容以前调用的接口
        /* enc_fmt.frame_dms = 20 * 10;//与工具保持一致，要乘以10,表示20ms */
        fmt.coding_type = AUDIO_CODING_OPUS;
        break;
    case AUDIO_CODING_SPEEX:
        enc_fmt.quality = 5;
        enc_fmt.complexity = 2;
        enc_fmt.sample_rate = 16000;
        fmt.sample_rate = 16000;
        fmt.coding_type = AUDIO_CODING_SPEEX;
        break;
    default:
        printf("do not support this type !!!\n");
        err = -ENOMEM;
        goto __exit1;
        break;
    }
    err = jlstream_node_ioctl(recoder->stream, NODE_UUID_ENCODER, NODE_IOC_SET_PRIV_FMT, (int)(&enc_fmt));
    err += jlstream_node_ioctl(recoder->stream, NODE_UUID_AI_TX, NODE_IOC_SET_FMT, (int)(&fmt));

    //设置ADC的中断点数
    err += jlstream_node_ioctl(recoder->stream, NODE_UUID_SOURCE, NODE_IOC_SET_PRIV_FMT, 320);
    if (err) {
        goto __exit1;
    }
    //设置源节点是哪个
    u16 node_uuid = get_cvp_node_uuid();
    u32 ref_sr = audio_dac_get_sample_rate(&dac_hdl);
    jlstream_node_ioctl(recoder->stream, NODE_UUID_SOURCE, NODE_IOC_GET_FMT, (int)&s_fmt);
    cvp_fmt.mic_num = s_fmt.channel_mode >> 4;
    if (node_uuid) {
#if !(TCFG_AUDIO_CVP_OUTPUT_WAY_IIS_ENABLE && (defined TCFG_IIS_NODE_ENABLE))
        jlstream_node_ioctl(recoder->stream, node_uuid, NODE_IOC_SET_FMT, (int)ref_sr);
#endif
        cvp_fmt.source_uuid = source_uuid;
        err = jlstream_node_ioctl(recoder->stream, node_uuid, NODE_IOC_SET_PRIV_FMT, (int)&cvp_fmt);
        if (err && (err != -ENOENT)) {	//兼容没有cvp节点的情况
            goto __exit1;
        }
        //使能CVP需要将ADC中断点数设成256
        jlstream_node_ioctl(recoder->stream, NODE_UUID_SOURCE, NODE_IOC_SET_PRIV_FMT, 256);
    }
    jlstream_set_callback(recoder->stream, recoder->stream, ai_voice_recoder_callback);
    jlstream_set_scene(recoder->stream, STREAM_SCENE_AI_VOICE);

    err = jlstream_start(recoder->stream);
    if (err) {
        goto __exit1;
    }

    g_ai_voice_recoder = recoder;

    return 0;

__exit1:
    jlstream_release(recoder->stream);
__exit0:
    free(recoder);
    return err;
}

void ai_voice_recoder_close()
{
    struct ai_voice_recoder *recoder = g_ai_voice_recoder;

    if (!recoder) {
        return;
    }
    jlstream_stop(recoder->stream, 0);
    jlstream_release(recoder->stream);

    free(recoder);
    g_ai_voice_recoder = NULL;

    jlstream_event_notify(STREAM_EVENT_CLOSE_RECODER, (int)"ai_voice");
}



