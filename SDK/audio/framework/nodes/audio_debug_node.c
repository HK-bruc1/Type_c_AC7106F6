#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".audio_debug_node.data.bss")
#pragma data_seg(".audio_debug_node.data")
#pragma const_seg(".audio_debug_node.text.const")
#pragma code_seg(".audio_debug_node.text")
#endif
#include "jlstream.h"
#include "audio_splicing.h"
#include "media/media_analysis.h"
#include "asm/math_fast_function.h"
#include "effects/effects_adj.h"
#include "sdk_config.h"

#if TCFG_AUDIO_DEBUG_NODE_ENABLE

struct audio_debug_param {
    u8 is_bypass;          // 1-> byass 0 -> no bypass
    u8 time;
    u8 max_min_en;
    u8 rms_en;
    u8 reserve[8];
} __attribute__((packed)) ;

struct audio_debug_node {
    char name[16];
    struct audio_debug_param param;
    float rms_dB;
    float rms_dB_R;
    s32 max;
    s32 min;
    unsigned long pre_us;
    u8 ch_num;
    u8 qval;
};

static void audio_rms_run(struct audio_debug_node *hdl, struct stream_frame *frame)
{
    u8 ch_num = hdl->ch_num;
    u8 nbit = hdl->qval;
    short npoint_per_ch = frame->bit_wide ? (frame->len / ch_num) >> 2 : (frame->len / ch_num) >> 1;
    if (hdl->param.rms_en) {
        short *mono_pcm = NULL;
        short *mono_pcm_r = NULL;
        if (ch_num == 2) {
            mono_pcm = malloc(frame->len / ch_num);
            if (!mono_pcm) {
                return;
            }

            mono_pcm_r = malloc(frame->len / ch_num);
            if (!mono_pcm_r) {
                free(mono_pcm);
                return;
            }
            if (frame->bit_wide) {
                s32 *org = (s32 *)frame->data;
                s32 *m_l  = (s32 *)mono_pcm;
                s32 *m_r  = (s32 *)mono_pcm_r;
                for (int i = 0; i < npoint_per_ch; i++) {
                    m_l[i] = org[2 * i];
                    m_r[i] = org[2 * i + 1];
                }
            } else {
                s16 *org = (s16 *)frame->data;
                s16 *m_l  = (s16 *)mono_pcm;
                s16 *m_r  = (s16 *)mono_pcm_r;
                for (int i = 0; i < npoint_per_ch; i++) {
                    m_l[i] = org[2 * i];
                    m_r[i] = org[2 * i + 1];
                }
            }

        } else {
            mono_pcm = (short *)frame->data;
        }

        if (frame->bit_wide) {
            int rms = rms_calc_32bit((const int *)mono_pcm, npoint_per_ch) + 1;
            hdl->rms_dB = 10 * log10_float(rms) - 20 * log10_float((float)(1 << (nbit)));

            if (ch_num == 2) {
                rms = rms_calc_32bit((const int *)mono_pcm_r, npoint_per_ch) + 1;
                hdl->rms_dB_R = 10 * log10_float(rms) - 20 * log10_float((float)(1 << (nbit)));
            }

        } else {
            int rms = rms_calc((const short *)mono_pcm, npoint_per_ch) + 1;
            hdl->rms_dB = 10 * log10_float(rms) - 20 * log10_float((float)(1 << (nbit)));

            if (ch_num == 2) {
                rms = rms_calc((const short *)mono_pcm_r, npoint_per_ch) + 1;
                hdl->rms_dB_R = 10 * log10_float(rms) - 20 * log10_float((float)(1 << (nbit)));
            }

        }

        if (ch_num != 1) {
            free(mono_pcm);
            mono_pcm = NULL;

            if (ch_num == 2) {
                free(mono_pcm_r);
                mono_pcm_r = NULL;
            }

        }

        printf("name:%s, rms L:%d dB, R:%d dB\n", hdl->name, (int)hdl->rms_dB, (int)hdl->rms_dB_R);
    }

}

static void audio_max_min_run(struct audio_debug_node *hdl, struct stream_frame *frame)
{
    if (hdl->param.max_min_en) {
        u32 points_size = frame->bit_wide ? 4 : 2;
        if (!frame->bit_wide) {
            s16 *dat16 = (s16 *)frame->data;
            u32 points = frame->len / points_size;
            s32 min = 32767;
            s32 max = -32768;
            for (int i = 0; i < points; i++) {
                if (max < dat16[i]) {
                    max = dat16[i];
                }

                if (min > dat16[i]) {
                    min = dat16[i];
                }
            }
            hdl->max = max;
            hdl->min = min;
        } else {
            s32 *dat32 = (s32 *)frame->data;
            u32 points = frame->len / points_size;
            s32 min = 2147483647;
            s32 max = -2147483648;
            for (int i = 0; i < points; i++) {
                if (max < dat32[i]) {
                    max = dat32[i];
                }

                if (min > dat32[i]) {
                    min = dat32[i];
                }
            }
            hdl->max = max;
            hdl->min = min;
        }

        printf("name:%s, max:%d min:%d\n", hdl->name, hdl->max, hdl->min);
    }
}

void audio_debug_run(struct audio_debug_node *hdl, struct stream_frame *frame)
{
    if (hdl->param.is_bypass) {
        return;
    }
    if (!hdl->param.time) {
        audio_rms_run(hdl, frame);
        audio_max_min_run(hdl, frame);
    } else {
        if (!hdl->pre_us) {
            hdl->pre_us = audio_jiffies_usec();
        }
        if (((audio_jiffies_usec() - hdl->pre_us) / 1000000) >= hdl->param.time) {
            audio_rms_run(hdl, frame);
            audio_max_min_run(hdl, frame);
            hdl->pre_us = audio_jiffies_usec();
        }
    }
}

static void audio_debug_node_handle_frame(struct stream_iport *iport, struct stream_note *note)
{
    struct stream_frame *frame;
    struct stream_node *node = iport->node;
    struct audio_debug_node *hdl = (struct audio_debug_node *)iport->node->private_data;


    frame = jlstream_pull_frame(iport, note);
    if (!frame) {
        return;
    }
    audio_debug_run(hdl, frame);
    jlstream_push_frame(node->oport, frame);
}

static int audio_debug_node_bind(struct stream_node *node, u16 uuid)
{
    return 0;
}

static void audio_debug_node_open_iport(struct stream_iport *iport)
{
}


static void audio_debug_ioc_start(struct audio_debug_node *hdl)
{
    struct stream_fmt *fmt = &hdl_node(hdl)->oport->fmt;


    hdl->ch_num = AUDIO_CH_NUM(fmt->channel_mode);
    hdl->qval  = hdl_node(hdl)->oport->fmt.Qval;
    /*
     *获取配置文件内的参数,及名字
     * */
    int len = jlstream_read_node_data_new(hdl_node(hdl)->uuid, hdl_node(hdl)->subid, (void *)&hdl->param, hdl->name);
    if (!len) {
        printf("%s, read node data err\n", __FUNCTION__);
        return;
    }

    /*
     *获取在线调试的临时参数
     * */
    if (config_audio_cfg_online_enable) {
        if (jlstream_read_effects_online_param(hdl_node(hdl)->uuid, hdl->name, &hdl->param, sizeof(hdl->param))) {
            printf("get audio debug online param error \n");
        }
    }
    printf(" %s, %d %d %d %d\n", hdl->name, hdl->param.is_bypass, hdl->param.time, hdl->param.max_min_en, hdl->param.rms_en);
    printf("%s, bit_wide %d, qval %d\n", hdl->name, hdl_node(hdl)->oport->fmt.bit_wide, hdl->qval);

    hdl->pre_us = audio_jiffies_usec();

}
static int audio_debug_ioc_update_parm(struct audio_debug_node *hdl, int parm)
{
    int ret = false;
    struct audio_debug_param *cfg = (struct audio_debug_param *)parm;
    if (hdl) {
        memcpy(&hdl->param, cfg, sizeof(struct audio_debug_param));
        ret = true;
    }
    return ret;
}

static int audio_debug_node_ioctl(struct stream_iport *iport, int cmd, int arg)
{
    int ret = 0;
    struct audio_debug_node *hdl = (struct audio_debug_node *)iport->node->private_data;
    switch (cmd) {
    case NODE_IOC_OPEN_IPORT:
        audio_debug_node_open_iport(iport);
        break;
    case NODE_IOC_START:
        audio_debug_ioc_start(hdl);
        break;
    case NODE_IOC_SUSPEND:
    case NODE_IOC_STOP:
        break;
    case NODE_IOC_NAME_MATCH:
        if (!strcmp((const char *)arg, hdl->name)) {
            ret = 1;
        }
        break;

    case NODE_IOC_SET_PARAM:
        ret = audio_debug_ioc_update_parm(hdl, arg);
        break;

    }

    return ret;
}

static void audio_debug_node_release(struct stream_node *node)
{
}


REGISTER_STREAM_NODE_ADAPTER(audio_debug_node_adapter) = {
    .uuid       = NODE_UUID_AUDIO_DEBUG,
    .bind       = audio_debug_node_bind,
    .ioctl      = audio_debug_node_ioctl,
    .release    = audio_debug_node_release,
    .handle_frame = audio_debug_node_handle_frame,				//注册输出回调
    .hdl_size   = sizeof(struct audio_debug_node),
};
REGISTER_ONLINE_ADJUST_TARGET(audio_debug) = {
    .uuid = NODE_UUID_AUDIO_DEBUG,
};
#endif
