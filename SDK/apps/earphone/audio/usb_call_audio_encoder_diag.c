#include "app_config.h"

#if TCFG_USB_CALL_AUDIO_ENCODER_DIAG_ENABLE

#include "encoder_node.h"
#include "jlstream.h"
#include "source_dev1_file.h"
#include "spinlock.h"
#include "system/init.h"
#include "system/timer.h"
#include "usb_call_audio_bridge.h"

#define USB_CALL_AUDIO_ENCODER_QUEUE_DEPTH          4
#define USB_CALL_AUDIO_ENCODER_BIT_RATE             32000
#define USB_CALL_AUDIO_ENCODER_PAYLOAD_BYTES        80
#define USB_CALL_AUDIO_ENCODER_REPORT_MS            5000

struct usb_call_audio_encoder_pcm_slot {
    u32 capture_generation;
    s16 pcm[USB_CALL_AUDIO_BRIDGE_FRAME_SAMPLES];
};

struct usb_call_audio_encoder_stats {
    u32 pcm_frames;
    u32 queued_frames;
    u32 dequeued_frames;
    u32 overflow_drops;
    u32 generation_drops;
    u32 generation_resets;
    u32 invalid_pcm_frames;
    u32 opus_frames;
    u32 opus_bytes;
    u32 invalid_opus_frames;
};

struct usb_call_audio_encoder_context {
    struct jlstream *stream;
    struct usb_call_audio_encoder_pcm_slot
        queue[USB_CALL_AUDIO_ENCODER_QUEUE_DEPTH];
    struct usb_call_audio_encoder_stats stats;
    u32 capture_generation;
    u16 report_timer;
    u8 read_index;
    u8 count;
    u8 accepting;
};

static struct usb_call_audio_encoder_context usb_call_audio_encoder;
static DEFINE_SPINLOCK(usb_call_audio_encoder_lock);

static int usb_call_audio_encoder_source_read(
    void *priv, u8 *data, u16 len)
{
    struct usb_call_audio_encoder_context *context = priv;
    struct usb_call_audio_encoder_pcm_slot *slot;

    if (len != sizeof(slot->pcm)) {
        return 0;
    }

    spin_lock(&usb_call_audio_encoder_lock);
    if (!context->accepting || !context->count) {
        spin_unlock(&usb_call_audio_encoder_lock);
        return 0;
    }
    slot = &context->queue[context->read_index];
    if (slot->capture_generation != context->capture_generation) {
        context->read_index =
            (context->read_index + 1) % USB_CALL_AUDIO_ENCODER_QUEUE_DEPTH;
        context->count--;
        context->stats.generation_drops++;
        spin_unlock(&usb_call_audio_encoder_lock);
        return 0;
    }
    memcpy(data, slot->pcm, sizeof(slot->pcm));
    context->read_index =
        (context->read_index + 1) % USB_CALL_AUDIO_ENCODER_QUEUE_DEPTH;
    context->count--;
    context->stats.dequeued_frames++;
    spin_unlock(&usb_call_audio_encoder_lock);
    return sizeof(slot->pcm);
}

static const struct source_dev1_provider usb_call_audio_encoder_provider = {
    .priv = &usb_call_audio_encoder,
    .format = {
        .sample_rate = USB_CALL_AUDIO_BRIDGE_OUTPUT_SAMPLE_RATE,
        .coding_type = AUDIO_CODING_PCM,
        .channel_mode = AUDIO_CH_MIX,
        .bit_wide = DATA_BIT_WIDE_16BIT,
        .frame_dms = USB_CALL_AUDIO_BRIDGE_FRAME_MS * 10,
    },
    .frame_bytes = USB_CALL_AUDIO_BRIDGE_FRAME_SAMPLES * sizeof(s16),
    .read = usb_call_audio_encoder_source_read,
};

static void usb_call_audio_encoder_bridge_consumer(
    void *priv, u32 capture_generation, const s16 *pcm, u16 samples)
{
    struct usb_call_audio_encoder_context *context = priv;
    struct usb_call_audio_encoder_pcm_slot *slot;
    u8 write_index;

    spin_lock(&usb_call_audio_encoder_lock);
    context->stats.pcm_frames++;
    if (!context->accepting || !capture_generation || !pcm
        || samples != USB_CALL_AUDIO_BRIDGE_FRAME_SAMPLES) {
        context->stats.invalid_pcm_frames++;
        spin_unlock(&usb_call_audio_encoder_lock);
        return;
    }
    if (context->capture_generation != capture_generation) {
        context->stats.generation_drops += context->count;
        context->stats.generation_resets++;
        context->capture_generation = capture_generation;
        context->read_index = 0;
        context->count = 0;
    }
    if (context->count == USB_CALL_AUDIO_ENCODER_QUEUE_DEPTH) {
        context->read_index =
            (context->read_index + 1) % USB_CALL_AUDIO_ENCODER_QUEUE_DEPTH;
        context->count--;
        context->stats.overflow_drops++;
    }
    write_index = (context->read_index + context->count)
                  % USB_CALL_AUDIO_ENCODER_QUEUE_DEPTH;
    slot = &context->queue[write_index];
    slot->capture_generation = capture_generation;
    memcpy(slot->pcm, pcm, sizeof(slot->pcm));
    context->count++;
    context->stats.queued_frames++;
    spin_unlock(&usb_call_audio_encoder_lock);

    source_dev1_data_notify();
}

static int usb_call_audio_encoder_ai_tx(u8 *data, u32 len)
{
    spin_lock(&usb_call_audio_encoder_lock);
    usb_call_audio_encoder.stats.opus_frames++;
    usb_call_audio_encoder.stats.opus_bytes += len;
    if (!data || len != USB_CALL_AUDIO_ENCODER_PAYLOAD_BYTES) {
        usb_call_audio_encoder.stats.invalid_opus_frames++;
    }
    spin_unlock(&usb_call_audio_encoder_lock);
    return len;
}

static void usb_call_audio_encoder_report(void *priv)
{
    struct usb_call_audio_encoder_context *context = priv;
    struct usb_call_audio_encoder_stats stats;
    struct usb_call_audio_bridge_snapshot bridge;
    u32 capture_generation;
    u8 queued;

    usb_call_audio_bridge_take_snapshot(&bridge);
    spin_lock(&usb_call_audio_encoder_lock);
    stats = context->stats;
    capture_generation = context->capture_generation;
    queued = context->count;
    spin_unlock(&usb_call_audio_encoder_lock);

    printf("[USB_CALL_ENCODER][DIAG] state bridge=%u timeline=%u bridge_capture=%u encoder_capture=%u queue=%u/%u resets=%u source_lost=%u stale_out=%u\n",
           bridge.opened, bridge.timeline_active,
           (unsigned int)bridge.capture_generation,
           (unsigned int)capture_generation, queued,
           USB_CALL_AUDIO_ENCODER_QUEUE_DEPTH,
           (unsigned int)bridge.mix.generation_resets,
           (unsigned int)bridge.mix.source_lost_events,
           (unsigned int)bridge.mix.stale_output_frames);
    printf("[USB_CALL_ENCODER][DIAG] pcm in=%u queued=%u dequeued=%u overflow_drop=%u generation_drop=%u generation_reset=%u invalid=%u\n",
           (unsigned int)stats.pcm_frames,
           (unsigned int)stats.queued_frames,
           (unsigned int)stats.dequeued_frames,
           (unsigned int)stats.overflow_drops,
           (unsigned int)stats.generation_drops,
           (unsigned int)stats.generation_resets,
           (unsigned int)stats.invalid_pcm_frames);
    printf("[USB_CALL_ENCODER][DIAG] opus frames=%u bytes=%u invalid_len=%u expected_len=%u bridge_mix=%u/%u\n",
           (unsigned int)stats.opus_frames,
           (unsigned int)stats.opus_bytes,
           (unsigned int)stats.invalid_opus_frames,
           USB_CALL_AUDIO_ENCODER_PAYLOAD_BYTES,
           (unsigned int)bridge.mix.frames,
           (unsigned int)bridge.mix.samples);
    printf("[USB_CALL_ENCODER][DIAG] bridge_near level=%u under=%u/%u overflow=%u/%u drift=%u/%u invalid=%u/%u\n",
           bridge.source[USB_CALL_AUDIO_TAP_NEAR].buffer_samples,
           (unsigned int)bridge.source[USB_CALL_AUDIO_TAP_NEAR]
               .interval.underflow_events,
           (unsigned int)bridge.source[USB_CALL_AUDIO_TAP_NEAR]
               .interval.underflow_samples,
           (unsigned int)bridge.source[USB_CALL_AUDIO_TAP_NEAR]
               .interval.overflow_events,
           (unsigned int)bridge.source[USB_CALL_AUDIO_TAP_NEAR]
               .interval.overflow_samples,
           (unsigned int)bridge.source[USB_CALL_AUDIO_TAP_NEAR]
               .interval.drift_drop_samples,
           (unsigned int)bridge.source[USB_CALL_AUDIO_TAP_NEAR]
               .interval.drift_duplicate_samples,
           (unsigned int)bridge.source[USB_CALL_AUDIO_TAP_NEAR]
               .interval.stale_input_frames,
           (unsigned int)bridge.source[USB_CALL_AUDIO_TAP_NEAR]
               .interval.invalid_input_frames);
    printf("[USB_CALL_ENCODER][DIAG] bridge_far level=%u under=%u/%u overflow=%u/%u drift=%u/%u invalid=%u/%u\n",
           bridge.source[USB_CALL_AUDIO_TAP_FAR].buffer_samples,
           (unsigned int)bridge.source[USB_CALL_AUDIO_TAP_FAR]
               .interval.underflow_events,
           (unsigned int)bridge.source[USB_CALL_AUDIO_TAP_FAR]
               .interval.underflow_samples,
           (unsigned int)bridge.source[USB_CALL_AUDIO_TAP_FAR]
               .interval.overflow_events,
           (unsigned int)bridge.source[USB_CALL_AUDIO_TAP_FAR]
               .interval.overflow_samples,
           (unsigned int)bridge.source[USB_CALL_AUDIO_TAP_FAR]
               .interval.drift_drop_samples,
           (unsigned int)bridge.source[USB_CALL_AUDIO_TAP_FAR]
               .interval.drift_duplicate_samples,
           (unsigned int)bridge.source[USB_CALL_AUDIO_TAP_FAR]
               .interval.stale_input_frames,
           (unsigned int)bridge.source[USB_CALL_AUDIO_TAP_FAR]
               .interval.invalid_input_frames);
}

static void usb_call_audio_encoder_cleanup(void)
{
    struct jlstream *stream = usb_call_audio_encoder.stream;

    usb_call_audio_bridge_close();
    spin_lock(&usb_call_audio_encoder_lock);
    usb_call_audio_encoder.accepting = 0;
    usb_call_audio_encoder.stream = NULL;
    usb_call_audio_encoder.count = 0;
    spin_unlock(&usb_call_audio_encoder_lock);
    if (stream) {
        jlstream_stop(stream, 0);
        jlstream_release(stream);
    }
    source_dev1_provider_unregister(&usb_call_audio_encoder_provider);
}

static int usb_call_audio_encoder_diag_init(void)
{
    struct stream_enc_fmt stream_fmt = {
        .channel = 1,
        .bit_width = DATA_BIT_WIDE_16BIT,
        .frame_dms = USB_CALL_AUDIO_BRIDGE_FRAME_MS * 10,
        .sample_rate = USB_CALL_AUDIO_BRIDGE_OUTPUT_SAMPLE_RATE,
        .bit_rate = USB_CALL_AUDIO_ENCODER_BIT_RATE,
        .coding_type = AUDIO_CODING_OPUS,
    };
    struct encoder_fmt encoder_fmt = {
        .complexity = 0,
        .ch_num = 1,
        .format = 0,
        .bit_width = DATA_BIT_WIDE_16BIT,
        .frame_dms = USB_CALL_AUDIO_BRIDGE_FRAME_MS * 10,
        .bit_rate = USB_CALL_AUDIO_ENCODER_BIT_RATE,
        .sample_rate = USB_CALL_AUDIO_BRIDGE_OUTPUT_SAMPLE_RATE,
    };
    struct stream_fmt ai_tx_fmt = {
        .bit_wide = DATA_BIT_WIDE_16BIT,
        .channel_mode = AUDIO_CH_MIX,
        .frame_dms = USB_CALL_AUDIO_BRIDGE_FRAME_MS * 10,
        .bit_rate = USB_CALL_AUDIO_ENCODER_BIT_RATE,
        .sample_rate = USB_CALL_AUDIO_BRIDGE_OUTPUT_SAMPLE_RATE,
        .coding_type = AUDIO_CODING_OPUS,
    };
    struct jlstream *stream;
    u16 pipeline_uuid;
    int ret;

    memset(&usb_call_audio_encoder, 0, sizeof(usb_call_audio_encoder));
    ret = source_dev1_provider_register(&usb_call_audio_encoder_provider);
    if (ret) {
        goto failed;
    }
    pipeline_uuid = jlstream_event_notify(
        STREAM_EVENT_GET_PIPELINE_UUID, (int)"user_defined");
    if (!pipeline_uuid) {
        ret = -EFAULT;
        goto unregister_provider;
    }
    stream = jlstream_pipeline_parse(pipeline_uuid, NODE_UUID_SOURCE_DEV1);
    if (!stream) {
        ret = -EFAULT;
        goto unregister_provider;
    }
    usb_call_audio_encoder.stream = stream;

    ret = jlstream_ioctl(stream, NODE_IOC_SET_ENC_FMT, (int)&stream_fmt);
    if (ret) {
        goto cleanup;
    }
    ret = jlstream_node_ioctl(stream, NODE_UUID_ENCODER,
                              NODE_IOC_SET_PRIV_FMT, (int)&encoder_fmt);
    if (ret) {
        goto cleanup;
    }
    ret = jlstream_node_ioctl(stream, NODE_UUID_AI_TX,
                              NODE_IOC_SET_FMT, (int)&ai_tx_fmt);
    if (ret) {
        goto cleanup;
    }
    ret = jlstream_node_ioctl(stream, NODE_UUID_AI_TX,
                              NODE_IOC_SET_PRIV_FMT,
                              (int)usb_call_audio_encoder_ai_tx);
    if (ret) {
        goto cleanup;
    }
    jlstream_set_scene(stream, STREAM_SCENE_AI_VOICE);
    ret = jlstream_start(stream);
    if (ret) {
        goto cleanup;
    }

    spin_lock(&usb_call_audio_encoder_lock);
    usb_call_audio_encoder.accepting = 1;
    spin_unlock(&usb_call_audio_encoder_lock);
    ret = usb_call_audio_bridge_open(
        usb_call_audio_encoder_bridge_consumer,
        &usb_call_audio_encoder);
    if (ret) {
        goto cleanup;
    }
    usb_call_audio_encoder.report_timer = sys_timer_add(
        &usb_call_audio_encoder, usb_call_audio_encoder_report,
        USB_CALL_AUDIO_ENCODER_REPORT_MS);
    if (!usb_call_audio_encoder.report_timer) {
        ret = -EFAULT;
        goto cleanup;
    }

    printf("[USB_CALL_ENCODER][DIAG] init=ok pipeline=0x%x source=SourceDev1 pcm=%u/1/16 frame_ms=%u queue_frames=%u opus_bitrate=%u expected_payload=%u transport=disabled\n",
           pipeline_uuid, USB_CALL_AUDIO_BRIDGE_OUTPUT_SAMPLE_RATE,
           USB_CALL_AUDIO_BRIDGE_FRAME_MS,
           USB_CALL_AUDIO_ENCODER_QUEUE_DEPTH,
           USB_CALL_AUDIO_ENCODER_BIT_RATE,
           USB_CALL_AUDIO_ENCODER_PAYLOAD_BYTES);
    return 0;

cleanup:
    usb_call_audio_encoder_cleanup();
    goto failed;
unregister_provider:
    source_dev1_provider_unregister(&usb_call_audio_encoder_provider);
failed:
    printf("[USB_CALL_ENCODER][DIAG] init=failed ret=%d\n", ret);
    return ret;
}

late_initcall(usb_call_audio_encoder_diag_init);

#endif /* TCFG_USB_CALL_AUDIO_ENCODER_DIAG_ENABLE */
