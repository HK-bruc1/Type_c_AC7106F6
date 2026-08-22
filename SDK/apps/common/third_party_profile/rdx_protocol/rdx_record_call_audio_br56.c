#include "app_config.h"

#if TCFG_RDX_ENABLE && RDX_CFG_CALL_RECORDING_ENABLE

#include "encoder_node.h"
#include "jlstream.h"
#include "rdx_record_call_audio_br56.h"
#include "source_dev1_file.h"
#include "spinlock.h"
#include "usb_call_audio_bridge.h"

#define RDX_CALL_AUDIO_PCM_QUEUE_DEPTH              4

struct rdx_call_audio_pcm_slot {
    u32 bridge_generation;
    s16 pcm[USB_CALL_AUDIO_BRIDGE_FRAME_SAMPLES];
};

struct rdx_call_audio_stats {
    u32 pcm_frames;
    u32 queued_frames;
    u32 dequeued_frames;
    u32 overflow_drops;
    u32 generation_drops;
    u32 generation_resets;
    u32 opus_frames;
    u32 opus_bytes;
};

struct rdx_call_audio_context {
    struct jlstream *stream;
    struct rdx_call_audio_pcm_slot queue[RDX_CALL_AUDIO_PCM_QUEUE_DEPTH];
    struct rdx_call_audio_stats stats;
    rdx_record_audio_frame_callback_t frame_callback;
    rdx_record_audio_fault_callback_t fault_callback;
    void *frame_priv;
    u32 engine_generation;
    u32 session_id;
    u32 capture_generation;
    u32 bridge_generation;
    u8 read_index;
    u8 count;
    u8 opened;
    u8 accepting;
};

static struct rdx_call_audio_context rdx_call_audio;
static DEFINE_SPINLOCK(rdx_call_audio_lock);

static int rdx_call_audio_source_read(void *priv, u8 *data, u16 len)
{
    struct rdx_call_audio_context *context = priv;
    struct rdx_call_audio_pcm_slot *slot;

    if (len != sizeof(slot->pcm)) {
        return 0;
    }

    spin_lock(&rdx_call_audio_lock);
    if (!context->accepting || !context->count) {
        spin_unlock(&rdx_call_audio_lock);
        return 0;
    }
    slot = &context->queue[context->read_index];
    if (slot->bridge_generation != context->bridge_generation) {
        context->read_index =
            (context->read_index + 1) % RDX_CALL_AUDIO_PCM_QUEUE_DEPTH;
        context->count--;
        context->stats.generation_drops++;
        spin_unlock(&rdx_call_audio_lock);
        return 0;
    }
    memcpy(data, slot->pcm, sizeof(slot->pcm));
    context->read_index =
        (context->read_index + 1) % RDX_CALL_AUDIO_PCM_QUEUE_DEPTH;
    context->count--;
    context->stats.dequeued_frames++;
    spin_unlock(&rdx_call_audio_lock);
    return sizeof(slot->pcm);
}

static const struct source_dev1_provider rdx_call_audio_provider = {
    .priv = &rdx_call_audio,
    .format = {
        .sample_rate = RDX_RECORD_CALL_V1_SAMPLE_RATE,
        .coding_type = AUDIO_CODING_PCM,
        .channel_mode = AUDIO_CH_MIX,
        .bit_wide = DATA_BIT_WIDE_16BIT,
        .frame_dms = RDX_RECORD_CALL_V1_FRAME_MS * 10,
    },
    .frame_bytes = USB_CALL_AUDIO_BRIDGE_FRAME_SAMPLES * sizeof(s16),
    .read = rdx_call_audio_source_read,
};

static void rdx_call_audio_bridge_consumer(
    void *priv, u32 bridge_generation, const s16 *pcm, u16 samples)
{
    struct rdx_call_audio_context *context = priv;
    struct rdx_call_audio_pcm_slot *slot;
    u8 write_index;

    if (!bridge_generation || !pcm
        || samples != USB_CALL_AUDIO_BRIDGE_FRAME_SAMPLES) {
        return;
    }

    spin_lock(&rdx_call_audio_lock);
    context->stats.pcm_frames++;
    if (!context->accepting) {
        spin_unlock(&rdx_call_audio_lock);
        return;
    }
    if (context->bridge_generation != bridge_generation) {
        context->stats.generation_drops += context->count;
        context->stats.generation_resets++;
        context->bridge_generation = bridge_generation;
        context->read_index = 0;
        context->count = 0;
    }
    if (context->count == RDX_CALL_AUDIO_PCM_QUEUE_DEPTH) {
        context->read_index =
            (context->read_index + 1) % RDX_CALL_AUDIO_PCM_QUEUE_DEPTH;
        context->count--;
        context->stats.overflow_drops++;
    }
    write_index = (context->read_index + context->count)
                  % RDX_CALL_AUDIO_PCM_QUEUE_DEPTH;
    slot = &context->queue[write_index];
    slot->bridge_generation = bridge_generation;
    memcpy(slot->pcm, pcm, sizeof(slot->pcm));
    context->count++;
    context->stats.queued_frames++;
    spin_unlock(&rdx_call_audio_lock);

    source_dev1_data_notify();
}

static void rdx_call_audio_bridge_event(
    void *priv, enum usb_call_audio_bridge_event event)
{
    struct rdx_call_audio_context *context = priv;
    rdx_record_audio_fault_callback_t callback = NULL;
    void *callback_priv = NULL;

    if (event != USB_CALL_AUDIO_BRIDGE_EVENT_SOURCE_LOST) {
        return;
    }
    spin_lock(&rdx_call_audio_lock);
    if (context->accepting) {
        callback = context->fault_callback;
        callback_priv = context->frame_priv;
    }
    spin_unlock(&rdx_call_audio_lock);
    if (callback) {
        callback(callback_priv, RDX_RECORD_CAUSE_SOURCE_LOST);
    }
}

static int rdx_call_audio_ai_tx(u8 *data, u32 len)
{
    rdx_record_audio_frame_callback_t callback;
    void *priv;
    u32 engine_generation;
    u32 session_id;
    u32 capture_generation;

    spin_lock(&rdx_call_audio_lock);
    if (!rdx_call_audio.accepting || !rdx_call_audio.frame_callback) {
        spin_unlock(&rdx_call_audio_lock);
        return 0;
    }
    rdx_call_audio.stats.opus_frames++;
    rdx_call_audio.stats.opus_bytes += len;
    callback = rdx_call_audio.frame_callback;
    priv = rdx_call_audio.frame_priv;
    engine_generation = rdx_call_audio.engine_generation;
    session_id = rdx_call_audio.session_id;
    capture_generation = rdx_call_audio.capture_generation;
    spin_unlock(&rdx_call_audio_lock);

    return callback(priv, engine_generation, session_id,
                    capture_generation, data, len);
}

int rdx_record_call_audio_br56_probe(
    enum rdx_record_format_id format_id)
{
    if (format_id != RDX_RECORD_FORMAT_CALL_V1) {
        return -EINVAL;
    }
    return usb_call_audio_bridge_probe();
}

int rdx_record_call_audio_br56_get_format(
    enum rdx_record_format_id *format_id)
{
    if (!format_id) {
        return -EINVAL;
    }
    *format_id = RDX_RECORD_FORMAT_CALL_V1;
    return 0;
}

int rdx_record_call_audio_br56_open(
    const struct rdx_record_audio_open_params *params)
{
    struct stream_enc_fmt stream_fmt = {
        .channel = RDX_RECORD_CALL_V1_CHANNELS,
        .bit_width = DATA_BIT_WIDE_16BIT,
        .frame_dms = RDX_RECORD_CALL_V1_FRAME_MS * 10,
        .sample_rate = RDX_RECORD_CALL_V1_SAMPLE_RATE,
        .bit_rate = RDX_RECORD_CALL_V1_BIT_RATE,
        .coding_type = AUDIO_CODING_OPUS,
    };
    struct encoder_fmt encoder_fmt = {
        .complexity = 0,
        .ch_num = RDX_RECORD_CALL_V1_CHANNELS,
        .format = 0,
        .bit_width = DATA_BIT_WIDE_16BIT,
        .frame_dms = RDX_RECORD_CALL_V1_FRAME_MS * 10,
        .bit_rate = RDX_RECORD_CALL_V1_BIT_RATE,
        .sample_rate = RDX_RECORD_CALL_V1_SAMPLE_RATE,
    };
    struct stream_fmt ai_tx_fmt = {
        .bit_wide = DATA_BIT_WIDE_16BIT,
        .channel_mode = AUDIO_CH_MIX,
        .frame_dms = RDX_RECORD_CALL_V1_FRAME_MS * 10,
        .bit_rate = RDX_RECORD_CALL_V1_BIT_RATE,
        .sample_rate = RDX_RECORD_CALL_V1_SAMPLE_RATE,
        .coding_type = AUDIO_CODING_OPUS,
    };
    struct jlstream *stream;
    u16 pipeline_uuid;
    int ret;

    if (!params || !params->frame_callback
        || params->format_id != RDX_RECORD_FORMAT_CALL_V1
        || !params->engine_generation || !params->session_id
        || !params->capture_generation) {
        return -EINVAL;
    }
    ret = rdx_record_call_audio_br56_probe(params->format_id);
    if (ret) {
        return ret;
    }

    spin_lock(&rdx_call_audio_lock);
    if (rdx_call_audio.opened) {
        spin_unlock(&rdx_call_audio_lock);
        return -EBUSY;
    }
    memset(&rdx_call_audio, 0, sizeof(rdx_call_audio));
    rdx_call_audio.opened = 1;
    rdx_call_audio.engine_generation = params->engine_generation;
    rdx_call_audio.session_id = params->session_id;
    rdx_call_audio.capture_generation = params->capture_generation;
    rdx_call_audio.frame_callback = params->frame_callback;
    rdx_call_audio.fault_callback = params->fault_callback;
    rdx_call_audio.frame_priv = params->frame_priv;
    spin_unlock(&rdx_call_audio_lock);

    ret = source_dev1_provider_register(&rdx_call_audio_provider);
    if (ret) {
        goto failed;
    }
    pipeline_uuid = jlstream_event_notify(
        STREAM_EVENT_GET_PIPELINE_UUID, (int)"user_defined");
    if (!pipeline_uuid) {
        ret = -EFAULT;
        goto failed;
    }
    stream = jlstream_pipeline_parse(pipeline_uuid, NODE_UUID_SOURCE_DEV1);
    if (!stream) {
        ret = -EFAULT;
        goto failed;
    }
    spin_lock(&rdx_call_audio_lock);
    rdx_call_audio.stream = stream;
    spin_unlock(&rdx_call_audio_lock);

    ret = jlstream_ioctl(stream, NODE_IOC_SET_ENC_FMT, (int)&stream_fmt);
    if (ret) {
        goto failed;
    }
    ret = jlstream_node_ioctl(stream, NODE_UUID_ENCODER,
                              NODE_IOC_SET_PRIV_FMT, (int)&encoder_fmt);
    if (ret) {
        goto failed;
    }
    ret = jlstream_node_ioctl(stream, NODE_UUID_AI_TX,
                              NODE_IOC_SET_FMT, (int)&ai_tx_fmt);
    if (ret) {
        goto failed;
    }
    ret = jlstream_node_ioctl(stream, NODE_UUID_AI_TX,
                              NODE_IOC_SET_PRIV_FMT,
                              (int)rdx_call_audio_ai_tx);
    if (ret) {
        goto failed;
    }
    jlstream_set_scene(stream, STREAM_SCENE_AI_VOICE);
    ret = jlstream_start(stream);
    if (ret) {
        goto failed;
    }

    spin_lock(&rdx_call_audio_lock);
    rdx_call_audio.accepting = 1;
    spin_unlock(&rdx_call_audio_lock);
    ret = usb_call_audio_bridge_open(
        rdx_call_audio_bridge_consumer,
        rdx_call_audio_bridge_event,
        &rdx_call_audio);
    if (ret) {
        goto failed;
    }
    ret = usb_call_audio_bridge_probe();
    if (ret) {
        goto failed;
    }

    printf("[RDX][CALL_AUDIO] opened session=%u capture=%u pipeline=0x%x format=opus_16k_mono queue=%u\n",
           (unsigned int)params->session_id,
           (unsigned int)params->capture_generation,
           pipeline_uuid, RDX_CALL_AUDIO_PCM_QUEUE_DEPTH);
    return 0;

failed:
    rdx_record_call_audio_br56_close();
    printf("[RDX][CALL_AUDIO] open_failed session=%u capture=%u ret=%d\n",
           (unsigned int)params->session_id,
           (unsigned int)params->capture_generation, ret);
    return ret;
}

int rdx_record_call_audio_br56_pause(void)
{
    rdx_record_call_audio_br56_close();
    return 0;
}

int rdx_record_call_audio_br56_resume(
    const struct rdx_record_audio_open_params *params)
{
    return rdx_record_call_audio_br56_open(params);
}

void rdx_record_call_audio_br56_close(void)
{
    struct rdx_call_audio_stats stats;
    struct jlstream *stream;
    u32 session_id;
    u32 capture_generation;
    u8 was_open;

    spin_lock(&rdx_call_audio_lock);
    was_open = rdx_call_audio.opened;
    rdx_call_audio.accepting = 0;
    stream = rdx_call_audio.stream;
    rdx_call_audio.stream = NULL;
    rdx_call_audio.count = 0;
    stats = rdx_call_audio.stats;
    session_id = rdx_call_audio.session_id;
    capture_generation = rdx_call_audio.capture_generation;
    spin_unlock(&rdx_call_audio_lock);

    if (!was_open) {
        return;
    }
    usb_call_audio_bridge_close();
    if (stream) {
        jlstream_stop(stream, 0);
        jlstream_release(stream);
    }
    source_dev1_provider_unregister(&rdx_call_audio_provider);

    spin_lock(&rdx_call_audio_lock);
    memset(&rdx_call_audio, 0, sizeof(rdx_call_audio));
    spin_unlock(&rdx_call_audio_lock);

    printf("[RDX][CALL_AUDIO] closed session=%u capture=%u pcm=%u/%u/%u overflow=%u generation_drop=%u resets=%u opus=%u/%u\n",
           (unsigned int)session_id,
           (unsigned int)capture_generation,
           (unsigned int)stats.pcm_frames,
           (unsigned int)stats.queued_frames,
           (unsigned int)stats.dequeued_frames,
           (unsigned int)stats.overflow_drops,
           (unsigned int)stats.generation_drops,
           (unsigned int)stats.generation_resets,
           (unsigned int)stats.opus_frames,
           (unsigned int)stats.opus_bytes);
}

#endif /* TCFG_RDX_ENABLE && RDX_CFG_CALL_RECORDING_ENABLE */
