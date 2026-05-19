#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".audio_anc_hw_src.data.bss")
#pragma data_seg(".audio_anc_hw_src.data")
#pragma const_seg(".audio_anc_hw_src.text.const")
#pragma code_seg(".audio_anc_hw_src.text")
#endif
#include "app_config.h"
#include "system/includes.h"
#include "media/includes.h"
#include "audio_config.h"
#include "classic/hci_lmp.h"

#if AUDIO_ANC_DATA_EXPORT_VIA_UART

#if 0
#define cvp_sync_log	printf
#else
#define cvp_sync_log(...)
#endif

#define AUDIO_CVP_SRCBUF_SIZE	672		//输入输出BUFFER长度
#define AUDIO_CVP_CATCH_LEN		AUDIO_CVP_SRCBUF_SIZE	//SRC 拼包BUF 长度, 注意每帧不能超过此长度

typedef struct {
    u8 ch_num;
    u8 src_break;
    u16 offset;
    u32 target_sample_rate;				//目标采样率
    u32 sample_rate;					//采样率
    s16 src_inbuf[AUDIO_CVP_SRCBUF_SIZE / 2];
    s16 src_outbuf[AUDIO_CVP_SRCBUF_SIZE / 2];
    u8 frame_catch[AUDIO_CVP_CATCH_LEN];
    struct audio_src_handle *hw_src;	//SRC句柄
} anc_dev_hw_src_t;

static int audio_anc_dev_hw_src_output_handler(void *priv, void *data, int len)
{
    anc_dev_hw_src_t *hdl = (anc_dev_hw_src_t *)priv;
    if ((hdl->offset + len) > AUDIO_CVP_CATCH_LEN) {
        printf("cvp sync output buf full\n");
        hdl->src_break = 1;
    }
    //回调仅做拼包
    memcpy(hdl->frame_catch + hdl->offset, (u8 *)data, len);
    hdl->offset += len;
    /* if (hdl->cnt > 5) { */
    /* cvp_sync_log("sync len %d, offset %d\n", len, hdl->offset); */
    /* } */
    return len;
}

void *audio_anc_dev_hw_src_open(u32 in_sr, u32 out_sr)
{
    anc_dev_hw_src_t *hdl = zalloc(sizeof(anc_dev_hw_src_t));
    if (!hdl) {
        return NULL;
    }
    hdl->hw_src = zalloc(sizeof(struct audio_src_handle));
    if (!hdl->hw_src) {
        free(hdl);
        return NULL;
    }
    hdl->ch_num = 1;
    hdl->sample_rate = in_sr;
    hdl->target_sample_rate = out_sr;
    // audio_hw_src_open(hdl->hw_src, hdl->ch_num, SRC_TYPE_RESAMPLE);
    hdl->hw_src->base = audio_src_base_open(hdl->ch_num, in_sr, out_sr, SRC_TYPE_RESAMPLE);
    audio_hw_src_set_output_buffer(hdl->hw_src, hdl->src_outbuf, AUDIO_CVP_SRCBUF_SIZE);
    audio_hw_src_set_input_buffer(hdl->hw_src, hdl->src_inbuf, AUDIO_CVP_SRCBUF_SIZE);
    audio_hw_src_set_rate(hdl->hw_src, hdl->sample_rate, hdl->target_sample_rate);
    audio_src_set_output_handler(hdl->hw_src, hdl, audio_anc_dev_hw_src_output_handler);
    return hdl;
}

void audio_anc_dev_hw_src_output(void *cbuf, s16 *data, int len);
int audio_anc_dev_hw_src_run(void *p, s16 *data, int len, void *cbuf)
{
    int wlen;
    anc_dev_hw_src_t *hdl = (anc_dev_hw_src_t *)p;
    if (hdl->hw_src) {
        while (len) {
            hdl->offset = 0;
            wlen = audio_src_resample_write(hdl->hw_src, data, len);
            audio_src_push_data_out(hdl->hw_src);	//执行完则表示当前帧的数据全部输出完毕
            audio_anc_dev_hw_src_output(cbuf, (s16 *)hdl->frame_catch, hdl->offset);	//输出到CVP
            data += wlen / 2;
            len -= wlen;
            if (hdl->src_break) {
                hdl->src_break = 0;
                break;
            }
        }
    }
    return len;
}

int audio_anc_dev_hw_src_close(void *p)
{
    anc_dev_hw_src_t *hdl = (anc_dev_hw_src_t *)p;
    if (hdl->hw_src) {
        audio_hw_src_stop(hdl->hw_src);
        audio_hw_src_close(hdl->hw_src);
        free(hdl->hw_src);
        free(hdl);
        hdl = NULL;
    }
    return 0;
}
#endif/*TCFG_AUDIO_CVP_SYNC*/
