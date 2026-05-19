/*************************************************************************************************/
/*!
*  \file      le_audio_source_node.c
*
*  \brief
*
*  Copyright (c) 2011-2023 ZhuHai Jieli Technology Co.,Ltd.
*
*/
/*************************************************************************************************/
#include "app_config.h"
#include "jlstream.h"
#include "sync/audio_syncts.h"
#include "le_audio_stream.h"
#include "app_config.h"
#include "effects/effects_adj.h"
#include "tech_lib/jla_ll_codec_api.h"

#if LE_AUDIO_STREAM_ENABLE

#define LE_AUDIO_TX_SOURCE      0
#define LE_AUDIO_LOCAL_SOURCE   1

static int get_le_audio_source_buffer_delay_time(void *iport, void *ch);

struct le_audio_source_syncts {
    u8 start;
    void *syncts;
    u32 timestamp;
    u32 buffered_time;
    struct list_head entry;
};

struct le_audio_source_context {
    void *tx_stream;
    void *rx_stream;
    void *le_audio;
    int sample_rate;
    u32 coding_type;
    struct list_head syncts_list;
    spinlock_t lock;
    u32 le_audio_start_usec;
    u32 local_start_usec;
    u32 local_time;
    u32 le_audio_time;
    char name[16];
};

struct le_audio_source_iport {
    u8 attribute;
    u8 channel_mode;
    u32 coding_type;
    int sample_rate;
    u32 bit_rate;
    u16 frame_dms;
    u16 frame_len;
    u8 bit_width;
};

static int le_audio_source_bind(struct stream_node *node, u16 uuid)
{
    struct le_audio_source_context *ctx = (struct le_audio_source_context *)node->private_data;

    if (!ctx) {
        return -ENOMEM;
    }

    spin_lock_init(&ctx->lock);
    INIT_LIST_HEAD(&ctx->syncts_list);
    /*
     *获取配置文件内的参数,及名字
     * */
    int len = jlstream_read_node_data_new(hdl_node(ctx)->uuid, hdl_node(ctx)->subid, NULL, ctx->name);
    if (!len) {
        printf("%s, read node data err !\n", __FUNCTION__);
        return 0 ;
    }
    printf("%s, read node cfg succ :%s, %d \n", __FUNCTION__, ctx->name, len);
    return 0;
}

extern uint32_t bb_le_clk_get_time_us(void);
static int le_audio_source_frame_timestamp_handler(struct le_audio_source_context *ctx, struct stream_frame *frame)
{
    if (!(frame->flags & FRAME_FLAG_TIMESTAMP_ENABLE)) {
        return 0;
    }

    struct le_audio_source_syncts *node;
    int time_diff = 0;
    spin_lock(&ctx->lock);
    list_for_each_entry(node, &ctx->syncts_list, entry) {
        if (node->start) {
            continue;
        }
        time_diff = frame->timestamp - node->timestamp;
        if (time_diff >= 0) {
            sound_pcm_syncts_latch_trigger(node->syncts);
            node->start = 1;
            int frame_latency = frame->timestamp - (audio_jiffies_usec() * TIMESTAMP_US_DENOMINATOR);
            u32 start_time = (bb_le_clk_get_time_us() + (frame_latency / TIMESTAMP_US_DENOMINATOR)) & 0xfffffff;
            le_audio_stream_set_start_time(ctx->le_audio, start_time);
            printf("--le audio tx syncts start : %u, %u, %d, %u--\n", frame->timestamp, node->timestamp, frame_latency / TIMESTAMP_US_DENOMINATOR, start_time);
        }
    }
    spin_unlock(&ctx->lock);

    return 0;
}
static void le_audio_source_handle_frame(struct stream_iport *iport, struct stream_note *note)
{
    struct le_audio_source_iport *hdl = (struct le_audio_source_iport *)iport->private_data;
    struct le_audio_source_context *ctx = (struct le_audio_source_context *)iport->node->private_data;
    struct stream_frame *frame;
    int wlen;

    while (1) {
        frame = jlstream_pull_frame(iport, note);
        if (!frame) {
            break;
        }

        if (hdl->attribute == LE_AUDIO_TX_SOURCE) {
            le_audio_source_frame_timestamp_handler(ctx, frame);
            wlen = le_audio_stream_tx_write(ctx->tx_stream, frame->data, frame->len);
        } else {
#if LEA_LOCAL_SYNC_PLAY_EN
            le_audio_source_frame_timestamp_handler(ctx, frame);
            wlen = le_audio_stream_rx_write(ctx->rx_stream, frame->data, frame->len);
#else
            wlen = frame->len;
#endif
        }
        if (wlen < frame->len) {
            jlstream_return_frame(iport, frame);
            note->state |= NODE_STA_OUTPUT_BLOCKED;
            break;
        }

        jlstream_free_frame(frame);
    }
}

static void le_audio_usec_to_local_usec(struct le_audio_source_context *ctx)
{
    local_irq_disable();
    ctx->local_time = audio_jiffies_usec();
    ctx->le_audio_time = bb_le_clk_get_time_us();
    local_irq_enable();
    /*printf("local le audio switch init : %d, %d\n", ctx->local_time, ctx->le_audio_time);*/
}

static int le_audio_tx_tick_handler(void *priv, int period, u32 send_timestamp)
{
    struct le_audio_source_context *ctx = (struct le_audio_source_context *)priv;

    int pcm_frames = period * ctx->sample_rate / 1000000;
    u32 tx_timestamp = ctx->local_time + ((send_timestamp - ctx->le_audio_time) & 0xfffffff);
    ctx->local_time = tx_timestamp;
    ctx->le_audio_time = send_timestamp;

    struct le_audio_source_syncts *node;
    spin_lock(&ctx->lock);
    list_for_each_entry(node, &ctx->syncts_list, entry) {
        if (!node->start) {
            continue;
        }
        sound_pcm_update_frame_num_and_time(node->syncts, pcm_frames, tx_timestamp, pcm_frames);
    }
    spin_unlock(&ctx->lock);

    return 0;
}

static void le_audio_source_open_iport(struct stream_iport *iport)
{
}

static void le_audio_source_close_iport(struct stream_iport *iport)
{
}

static void le_audio_source_set_bt_addr(struct stream_iport *iport, int arg)
{
    struct le_audio_source_context *ctx = (struct le_audio_source_context *)iport->node->private_data;

    ctx->le_audio = (void *)arg;

#if LEA_LOCAL_SYNC_PLAY_EN //本地播放的音频流提前打开，避免在发送的音频流挂起或暂时没有数据的时候，本地播放的流程开启出现获取不到参数的问题
    if (!ctx->rx_stream) {
        ctx->rx_stream = le_audio_stream_rx_open(ctx->le_audio, AUDIO_CODING_PCM);
    }
#endif

}

static int le_audio_source_ioc_start(struct stream_iport *iport)
{
    struct le_audio_source_iport *hdl = (struct le_audio_source_iport *)iport->private_data;
    struct le_audio_source_context *ctx = (struct le_audio_source_context *)iport->node->private_data;

    if (!ctx->le_audio) {
        printf("LE Audio source node have no le audio handle.\n");
        return 0;
    }

    if (hdl) {
        le_audio_stream_set_bit_width(ctx->le_audio, hdl->bit_width);
    }

#if LEA_DUAL_STREAM_MERGE_TRANS_MODE
    if (ctx->rx_stream) {
        return 0;
    }
    if (!hdl) {
        return 0;
    }
    int frame_size = le_audio_get_encoder_len(hdl->coding_type, hdl->frame_dms, hdl->bit_rate);

    int ch = strcmp(ctx->name, "LEA_Source_CH1") ? 0 : 1;
    ctx->tx_stream = le_audio_dual_stream_tx_open(ctx->le_audio, hdl->coding_type, frame_size, ch);
    le_audio_stream_set_tx_tick_handler(ctx->le_audio, ctx, le_audio_tx_tick_handler, ch);
    hdl->attribute = LE_AUDIO_TX_SOURCE;
    le_audio_usec_to_local_usec(ctx);
    return 0;

#endif

    if (!ctx->tx_stream) {
        ctx->tx_stream = le_audio_stream_tx_open(ctx->le_audio, hdl->coding_type, NULL, NULL);
        le_audio_stream_set_tx_tick_handler(ctx->le_audio, ctx, le_audio_tx_tick_handler, 0);
        hdl->attribute = LE_AUDIO_TX_SOURCE;
        le_audio_usec_to_local_usec(ctx);
        return 0;
    }

#if LEA_LOCAL_SYNC_PLAY_EN
    if (!ctx->rx_stream) {
        ctx->rx_stream = le_audio_stream_rx_open(ctx->le_audio, hdl->coding_type);
        le_audio_stream_rx_buf_init(ctx->le_audio, hdl->coding_type);
        hdl->attribute = LE_AUDIO_LOCAL_SOURCE;
        return 0;
    }
#endif
    //在被打的断恢复的时候，tx_stream/rx_stream没有释放,通过判断前节点是否是编码节点确定是tx/rx的音频流
    if (iport->prev->node->uuid == NODE_UUID_ENCODER) {
        hdl->attribute = LE_AUDIO_TX_SOURCE;
    } else {
        le_audio_stream_rx_buf_init(ctx->le_audio, hdl->coding_type);
        hdl->attribute = LE_AUDIO_LOCAL_SOURCE;
    }

    return 0;
}

static void le_audio_source_ioc_stop(struct stream_iport *iport)
{
    struct le_audio_source_iport *hdl = (struct le_audio_source_iport *)iport->private_data;
    struct le_audio_source_context *ctx = (struct le_audio_source_context *)iport->node->private_data;

    if (hdl->attribute == LE_AUDIO_LOCAL_SOURCE) {
#if LEA_LOCAL_SYNC_PLAY_EN
        if (ctx->rx_stream) {
            le_audio_stream_rx_close(ctx->rx_stream);
            ctx->rx_stream = NULL;
        }
#endif
    } else {
#if LEA_DUAL_STREAM_MERGE_TRANS_MODE
        int ch = strcmp(ctx->name, "LEA_Source_CH1") ? 0 : 1;
        le_audio_stream_set_tx_tick_handler(ctx->le_audio, NULL, NULL, ch);
#else
        le_audio_stream_set_tx_tick_handler(ctx->le_audio, NULL, NULL, 0);
#endif
        if (ctx->tx_stream) {
            le_audio_stream_tx_close(ctx->tx_stream);
            ctx->tx_stream = NULL;
        }
    }

}

static void le_audio_source_suspend(struct stream_iport *iport)
{
    struct le_audio_source_iport *hdl = (struct le_audio_source_iport *)iport->private_data;
    struct le_audio_source_context *ctx = (struct le_audio_source_context *)iport->node->private_data;

    if (hdl->attribute == LE_AUDIO_LOCAL_SOURCE) {
#if LEA_LOCAL_SYNC_PLAY_EN
        if (ctx->rx_stream) {
            le_audio_stream_rx_drain(ctx->rx_stream);
        }
#endif
    } else {
#if LEA_DUAL_STREAM_MERGE_TRANS_MODE
        int ch = strcmp(ctx->name, "LEA_Source_CH1") ? 0 : 1;
        le_audio_stream_set_tx_tick_handler(ctx->le_audio, NULL, NULL, ch);
#else
        le_audio_stream_set_tx_tick_handler(ctx->le_audio, NULL, NULL, 0);
#endif
        if (ctx->tx_stream) {
            le_audio_stream_tx_drain(ctx->tx_stream);
        }
    }

}

static int le_audio_source_ioc_fmt_nego(struct stream_iport *iport)
{
    struct le_audio_source_iport *hdl = (struct le_audio_source_iport *)iport->private_data;
    struct le_audio_source_context *ctx = (struct le_audio_source_context *)iport->node->private_data;
    struct stream_fmt *in_fmt = &iport->prev->fmt;

    if (!ctx->coding_type) {
        ctx->coding_type = in_fmt->coding_type;
    }

    if (in_fmt->coding_type == 0) {
        in_fmt->coding_type = hdl->coding_type;
    }
    hdl->coding_type = in_fmt->coding_type;
    hdl->bit_width = in_fmt->bit_wide;
    in_fmt->channel_mode = hdl->channel_mode;
    in_fmt->sample_rate = hdl->sample_rate;

    return NEGO_STA_ACCPTED;
}

static int le_audio_source_buffer_delay_time(struct stream_iport *iport)
{
    struct le_audio_source_context *ctx = (struct le_audio_source_context *)iport->node->private_data;

    if (ctx->tx_stream) {
        return le_audio_stream_tx_buffered_time(ctx->tx_stream)/*us*/ / 100;
    }



    return 0;
}

static int get_le_audio_source_buffer_delay_time(void *iport, void *ch)
{
    return le_audio_source_buffer_delay_time(iport);
}

static void le_audio_source_set_encode_fmt(struct stream_iport *iport, int arg)
{
    struct le_audio_source_iport *hdl = (struct le_audio_source_iport *)iport->private_data;
    struct le_audio_source_context *ctx = (struct le_audio_source_context *)iport->node->private_data;
    struct stream_enc_fmt *fmt = (struct stream_enc_fmt *)arg;
    //printf(" =========%s %d,%d,%d,%d, %d\n",ctx->name,fmt->coding_type,fmt->bit_rate,fmt->frame_dms,fmt->sample_rate,fmt->channel);

    hdl->bit_rate = fmt->bit_rate;
    hdl->frame_dms = fmt->frame_dms;
    hdl->sample_rate = fmt->sample_rate;
    hdl->frame_len = hdl->bit_rate * hdl->frame_dms / 8 / 10000 + 2;
    hdl->channel_mode = fmt->channel == 2 ? AUDIO_CH_LR : AUDIO_CH_MIX;

    ctx->sample_rate = fmt->sample_rate;
}

static void le_audio_source_add_syncts(struct le_audio_source_context *ctx, void *syncts, u32 timestamp)
{
    struct le_audio_source_syncts *node = NULL;

    spin_lock(&ctx->lock);
    list_for_each_entry(node, &ctx->syncts_list, entry) {
        if ((u32)node->syncts == (u32)syncts) {
            spin_unlock(&ctx->lock);
            return;
        }
    }
    spin_unlock(&ctx->lock);

    node = (struct le_audio_source_syncts *)zalloc(sizeof(struct le_audio_source_syncts));
    node->syncts = syncts;
    node->timestamp = timestamp;
    spin_lock(&ctx->lock);
    list_add(&node->entry, &ctx->syncts_list);
    spin_unlock(&ctx->lock);
}

static void le_audio_source_remove_syncts(struct le_audio_source_context *ctx, void *syncts)
{
    struct le_audio_source_syncts *node = NULL;

    spin_lock(&ctx->lock);
    list_for_each_entry(node, &ctx->syncts_list, entry) {
        if (node->syncts == syncts) {
            goto remove_node;
        }
    }

    spin_unlock(&ctx->lock);
    return;
remove_node:
    list_del(&node->entry);
    free(node);
    spin_unlock(&ctx->lock);
}

static int le_audio_source_syncts_handler(struct stream_iport *iport, int arg)
{
    struct le_audio_source_context *ctx = (struct le_audio_source_context *)iport->node->private_data;
    struct audio_syncts_ioc_params *params = (struct audio_syncts_ioc_params *)arg;

    if (!params) {
        return 0;
    }

    switch (params->cmd) {
    case AUDIO_SYNCTS_MOUNT_ON_SNDPCM:
        le_audio_source_add_syncts(ctx, (void *)params->data[0], params->data[1]);
        break;
    case AUDIO_SYNCTS_UMOUNT_ON_SNDPCM:
        le_audio_source_remove_syncts(ctx, (void *)params->data[0]);
        break;
    }
    return 0;
}

static int le_audio_source_ioctl(struct stream_iport *iport, int cmd, int arg)
{
    struct le_audio_source_context *ctx = (struct le_audio_source_context *)iport->node->private_data;
    int ret = 0;
    switch (cmd) {
    case NODE_IOC_OPEN_IPORT:
        le_audio_source_open_iport(iport);
        break;
    case NODE_IOC_CLOSE_IPORT:
        break;
    case NODE_IOC_SET_BTADDR:
        le_audio_source_set_bt_addr(iport, arg);
        break;
    case NODE_IOC_START:
        le_audio_source_ioc_start(iport);
        break;
    case NODE_IOC_SUSPEND:
        le_audio_source_suspend(iport);
        break;
    case NODE_IOC_STOP:
        le_audio_source_ioc_stop(iport);
        break;
    case NODE_IOC_NEGOTIATE:
        *(int *)arg |= le_audio_source_ioc_fmt_nego(iport);
        break;
    case NODE_IOC_GET_DELAY:
        return le_audio_source_buffer_delay_time(iport);
    case NODE_IOC_SET_ENC_FMT:
        le_audio_source_set_encode_fmt(iport, arg);
        break;
    case NODE_IOC_SYNCTS:
        le_audio_source_syncts_handler(iport, arg);
        break;
    default:
        break;
    }
    return ret;
}

static void le_audio_source_release(struct stream_node *node)
{
}

REGISTER_STREAM_NODE_ADAPTER(le_audio_source_adapter) = {
    .uuid       = NODE_UUID_LE_AUDIO_SOURCE,
    .bind       = le_audio_source_bind,
    .ioctl      = le_audio_source_ioctl,
    .release    = le_audio_source_release,
    .handle_frame = le_audio_source_handle_frame,
    .hdl_size = sizeof(struct le_audio_source_context),
    .iport_size = sizeof(struct le_audio_source_iport),
};


#endif/*LE_AUDIO_STREAM_ENABLE*/

