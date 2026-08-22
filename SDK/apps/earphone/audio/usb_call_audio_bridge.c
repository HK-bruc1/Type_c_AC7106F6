#include "app_config.h"
#include "media/audio_base.h"
#include "spinlock.h"
#include "system/timer.h"
#include "usb_call_audio_bridge.h"

#define USB_CALL_AUDIO_BRIDGE_BUFFER_SAMPLES \
    (USB_CALL_AUDIO_BRIDGE_OUTPUT_SAMPLE_RATE \
     * USB_CALL_AUDIO_BRIDGE_BUFFER_MS / 1000)
#define USB_CALL_AUDIO_BRIDGE_PRELOAD_SAMPLES \
    (USB_CALL_AUDIO_BRIDGE_OUTPUT_SAMPLE_RATE \
     * USB_CALL_AUDIO_BRIDGE_PRELOAD_MS / 1000)
#define USB_CALL_AUDIO_BRIDGE_DRIFT_WINDOW_SAMPLES \
    (USB_CALL_AUDIO_BRIDGE_FRAME_SAMPLES / 2)
#define USB_CALL_AUDIO_BRIDGE_SOURCE_LOST_FRAMES \
    (USB_CALL_AUDIO_BRIDGE_SOURCE_LOST_MS \
     / USB_CALL_AUDIO_BRIDGE_FRAME_MS)
#define USB_CALL_AUDIO_BRIDGE_NORMALIZE_CHUNK_SAMPLES    256

struct usb_call_audio_bridge_ring {
    s16 sample[USB_CALL_AUDIO_BRIDGE_BUFFER_SAMPLES];
    u16 read_index;
    u16 count;
};

struct usb_call_audio_bridge_source {
    struct usb_call_audio_bridge_ring ring;
    struct usb_call_audio_tap_format accepted_format;
    struct usb_call_audio_tap_format producer_format;
    struct usb_call_audio_bridge_source_interval interval;
    s64 resample_sum;
    u32 last_input_ms;
    u32 resample_phase;
    u32 producer_generation;
    u16 resample_count;
    u16 peak_buffer_samples;
    u8 format_valid;
    u8 consecutive_underflows;
    s16 normalize_chunk[USB_CALL_AUDIO_BRIDGE_NORMALIZE_CHUNK_SAMPLES];
};

struct usb_call_audio_bridge_context {
    struct usb_call_audio_bridge_source source[USB_CALL_AUDIO_TAP_COUNT];
    struct usb_call_audio_bridge_mix_interval mix_interval;
    usb_call_audio_bridge_consumer_t consumer;
    void *consumer_priv;
    u32 capture_generation;
    u32 last_runtime_generation[USB_CALL_AUDIO_TAP_COUNT];
    u8 last_runtime_active[USB_CALL_AUDIO_TAP_COUNT];
    u8 opened;
    u8 timeline_active;
    u8 drain_busy;
    s16 near_frame[USB_CALL_AUDIO_BRIDGE_FRAME_SAMPLES];
    s16 far_frame[USB_CALL_AUDIO_BRIDGE_FRAME_SAMPLES];
    s16 mix_frame[USB_CALL_AUDIO_BRIDGE_FRAME_SAMPLES];
};

static struct usb_call_audio_bridge_context usb_call_audio_bridge;
static DEFINE_SPINLOCK(usb_call_audio_bridge_lock);

static void usb_call_audio_bridge_drain_from_near(void);

static u32 usb_call_audio_bridge_next_generation(u32 generation)
{
    generation++;
    return generation ? generation : 1;
}

static u32 usb_call_audio_bridge_absolute(s16 sample)
{
    s32 value = sample;

    return value < 0 ? (u32)-value : (u32)value;
}

static int usb_call_audio_bridge_format_equal(
    const struct usb_call_audio_tap_format *left,
    const struct usb_call_audio_tap_format *right)
{
    return left->sample_rate == right->sample_rate
           && left->coding_type == right->coding_type
           && left->format_generation == right->format_generation
           && left->channels == right->channels
           && left->bit_width == right->bit_width
           && left->qval == right->qval;
}

static int usb_call_audio_bridge_generation_is_newer(
    u32 generation, u32 previous)
{
    return (s32)(generation - previous) > 0;
}

static void usb_call_audio_bridge_ring_clear(
    struct usb_call_audio_bridge_ring *ring)
{
    ring->read_index = 0;
    ring->count = 0;
}

static void usb_call_audio_bridge_ring_discard(
    struct usb_call_audio_bridge_ring *ring, u16 samples)
{
    if (samples > ring->count) {
        samples = ring->count;
    }
    ring->read_index =
        (ring->read_index + samples) % USB_CALL_AUDIO_BRIDGE_BUFFER_SAMPLES;
    ring->count -= samples;
}

static void usb_call_audio_bridge_ring_write(
    struct usb_call_audio_bridge_source *source,
    const s16 *samples,
    u16 count)
{
    struct usb_call_audio_bridge_ring *ring = &source->ring;
    u16 overflow = 0;
    u16 write_index;
    u16 first;

    if (count > USB_CALL_AUDIO_BRIDGE_BUFFER_SAMPLES) {
        samples += count - USB_CALL_AUDIO_BRIDGE_BUFFER_SAMPLES;
        overflow = count - USB_CALL_AUDIO_BRIDGE_BUFFER_SAMPLES;
        count = USB_CALL_AUDIO_BRIDGE_BUFFER_SAMPLES;
    }
    if (count > USB_CALL_AUDIO_BRIDGE_BUFFER_SAMPLES - ring->count) {
        u16 discard = count
                      - (USB_CALL_AUDIO_BRIDGE_BUFFER_SAMPLES - ring->count);

        usb_call_audio_bridge_ring_discard(ring, discard);
        overflow += discard;
    }
    if (overflow) {
        source->interval.overflow_samples += overflow;
        source->interval.overflow_events++;
    }

    write_index = (ring->read_index + ring->count)
                  % USB_CALL_AUDIO_BRIDGE_BUFFER_SAMPLES;
    first = USB_CALL_AUDIO_BRIDGE_BUFFER_SAMPLES - write_index;
    if (first > count) {
        first = count;
    }
    memcpy(&ring->sample[write_index], samples, first * sizeof(s16));
    if (count > first) {
        memcpy(ring->sample, samples + first,
               (count - first) * sizeof(s16));
    }
    ring->count += count;
    if (ring->count > source->peak_buffer_samples) {
        source->peak_buffer_samples = ring->count;
    }
}

static u16 usb_call_audio_bridge_ring_read(
    struct usb_call_audio_bridge_ring *ring,
    s16 *samples,
    u16 count)
{
    u16 read_count = count;
    u16 first;

    if (read_count > ring->count) {
        read_count = ring->count;
    }
    first = USB_CALL_AUDIO_BRIDGE_BUFFER_SAMPLES - ring->read_index;
    if (first > read_count) {
        first = read_count;
    }
    memcpy(samples, &ring->sample[ring->read_index], first * sizeof(s16));
    if (read_count > first) {
        memcpy(samples + first, ring->sample,
               (read_count - first) * sizeof(s16));
    }
    usb_call_audio_bridge_ring_discard(ring, read_count);
    return read_count;
}

static void usb_call_audio_bridge_invalidate_locked(void)
{
    u8 tap_id;

    usb_call_audio_bridge.capture_generation =
        usb_call_audio_bridge_next_generation(
            usb_call_audio_bridge.capture_generation);
    usb_call_audio_bridge.timeline_active = 0;
    for (tap_id = 0; tap_id < USB_CALL_AUDIO_TAP_COUNT; tap_id++) {
        usb_call_audio_bridge_ring_clear(
            &usb_call_audio_bridge.source[tap_id].ring);
        usb_call_audio_bridge.source[tap_id].consecutive_underflows = 0;
    }
    usb_call_audio_bridge.mix_interval.generation_resets++;
}

static int usb_call_audio_bridge_format_is_supported(
    const struct usb_call_audio_tap_format *format,
    u32 len)
{
    u32 bytes_per_sample;
    u32 bytes_per_frame;

    if (!format || !format->format_generation
        || format->coding_type != AUDIO_CODING_PCM
        || format->sample_rate < USB_CALL_AUDIO_BRIDGE_OUTPUT_SAMPLE_RATE
        || format->sample_rate > 192000
        || !format->channels || format->channels > 2
        || format->bit_width > 1
        || format->qval > 30) {
        return 0;
    }
    bytes_per_sample = format->bit_width ? sizeof(s32) : sizeof(s16);
    bytes_per_frame = bytes_per_sample * format->channels;
    return bytes_per_frame && len && !(len % bytes_per_frame);
}

static s16 usb_call_audio_bridge_to_q15(
    s64 sample, u8 qval, u8 bit_width)
{
    if (!qval) {
        qval = bit_width ? 23 : 15;
    }
    if (qval > 15) {
        sample >>= qval - 15;
    } else if (qval < 15) {
        sample <<= 15 - qval;
    }
    if (sample > 32767) {
        return 32767;
    }
    if (sample < -32768) {
        return -32768;
    }
    return (s16)sample;
}

static void usb_call_audio_bridge_accept_format(
    enum usb_call_audio_tap_id tap_id,
    const struct usb_call_audio_tap_format *format)
{
    struct usb_call_audio_bridge_source *source;

    spin_lock(&usb_call_audio_bridge_lock);
    if (!usb_call_audio_bridge.opened) {
        spin_unlock(&usb_call_audio_bridge_lock);
        return;
    }
    source = &usb_call_audio_bridge.source[tap_id];
    source->accepted_format = *format;
    source->format_valid = 1;
    usb_call_audio_bridge_invalidate_locked();
    spin_unlock(&usb_call_audio_bridge_lock);
}

static void usb_call_audio_bridge_push_normalized(
    enum usb_call_audio_tap_id tap_id,
    const s16 *samples,
    u16 count,
    u32 input_samples)
{
    struct usb_call_audio_bridge_source *source;
    enum usb_call_audio_tap_id peer_id;
    u32 now = sys_timer_get_ms();

    spin_lock(&usb_call_audio_bridge_lock);
    if (!usb_call_audio_bridge.opened) {
        spin_unlock(&usb_call_audio_bridge_lock);
        return;
    }
    source = &usb_call_audio_bridge.source[tap_id];
    source->interval.input_samples += input_samples;
    source->interval.normalized_samples += count;
    source->last_input_ms = now;
    peer_id = tap_id == USB_CALL_AUDIO_TAP_NEAR
              ? USB_CALL_AUDIO_TAP_FAR : USB_CALL_AUDIO_TAP_NEAR;
    if (usb_call_audio_bridge.timeline_active
        && usb_call_audio_bridge.source[peer_id].last_input_ms
        && (u32)(now - usb_call_audio_bridge.source[peer_id].last_input_ms)
           >= USB_CALL_AUDIO_BRIDGE_SOURCE_LOST_MS) {
        usb_call_audio_bridge.mix_interval.source_lost_events++;
        usb_call_audio_bridge_invalidate_locked();
    }
    usb_call_audio_bridge_ring_write(source, samples, count);
    spin_unlock(&usb_call_audio_bridge_lock);

    if (tap_id == USB_CALL_AUDIO_TAP_NEAR) {
        usb_call_audio_bridge_drain_from_near();
    }
}

static void usb_call_audio_bridge_count_rejected(
    enum usb_call_audio_tap_id tap_id, u8 stale)
{
    spin_lock(&usb_call_audio_bridge_lock);
    if (stale) {
        usb_call_audio_bridge.source[tap_id].interval.stale_input_frames++;
    } else {
        usb_call_audio_bridge.source[tap_id].interval.invalid_input_frames++;
    }
    spin_unlock(&usb_call_audio_bridge_lock);
}

static void usb_call_audio_bridge_tap_consumer(
    void *priv,
    enum usb_call_audio_tap_id tap_id,
    const struct usb_call_audio_tap_format *format,
    const void *data,
    u32 len)
{
    struct usb_call_audio_bridge_source *source;
    u32 input_frames;
    u32 input_since_push = 0;
    u32 frame_index;
    u32 channel;
    u16 output_count = 0;

    (void)priv;
    if (tap_id >= USB_CALL_AUDIO_TAP_COUNT || !data
        || !usb_call_audio_bridge_format_is_supported(format, len)) {
        if (tap_id < USB_CALL_AUDIO_TAP_COUNT) {
            usb_call_audio_bridge_count_rejected(tap_id, 0);
        }
        return;
    }

    source = &usb_call_audio_bridge.source[tap_id];
    if (source->producer_generation != format->format_generation) {
        if (source->producer_generation
            && !usb_call_audio_bridge_generation_is_newer(
                format->format_generation,
                source->producer_generation)) {
            usb_call_audio_bridge_count_rejected(tap_id, 1);
            return;
        }
        source->producer_generation = format->format_generation;
        source->producer_format = *format;
        source->resample_sum = 0;
        source->resample_phase = 0;
        source->resample_count = 0;
        usb_call_audio_bridge_accept_format(tap_id, format);
    } else if (!usb_call_audio_bridge_format_equal(
                   &source->producer_format, format)) {
        usb_call_audio_bridge_count_rejected(tap_id, 0);
        return;
    }

    input_frames = len / (format->channels
                          * (format->bit_width
                             ? sizeof(s32) : sizeof(s16)));
    for (frame_index = 0; frame_index < input_frames; frame_index++) {
        s64 mono = 0;
        s16 q15;

        if (format->bit_width) {
            const s32 *input = (const s32 *)data;

            for (channel = 0; channel < format->channels; channel++) {
                mono += input[frame_index * format->channels + channel];
            }
        } else {
            const s16 *input = (const s16 *)data;

            for (channel = 0; channel < format->channels; channel++) {
                mono += input[frame_index * format->channels + channel];
            }
        }
        mono /= format->channels;
        q15 = usb_call_audio_bridge_to_q15(
            mono, format->qval, format->bit_width);
        source->resample_sum += q15;
        source->resample_count++;
        input_since_push++;
        source->resample_phase += USB_CALL_AUDIO_BRIDGE_OUTPUT_SAMPLE_RATE;
        if (source->resample_phase >= format->sample_rate) {
            source->normalize_chunk[output_count++] =
                (s16)(source->resample_sum / source->resample_count);
            source->resample_phase -= format->sample_rate;
            source->resample_sum = 0;
            source->resample_count = 0;
        }
        if (output_count == USB_CALL_AUDIO_BRIDGE_NORMALIZE_CHUNK_SAMPLES) {
            usb_call_audio_bridge_push_normalized(
                tap_id, source->normalize_chunk, output_count,
                input_since_push);
            input_since_push = 0;
            output_count = 0;
        }
    }
    if (output_count || input_since_push) {
        usb_call_audio_bridge_push_normalized(
            tap_id, source->normalize_chunk, output_count,
            input_since_push);
    }
}

static u16 usb_call_audio_bridge_read_source_locked(
    enum usb_call_audio_tap_id tap_id,
    s16 *frame,
    u8 adjust_drift)
{
    struct usb_call_audio_bridge_source *source =
        &usb_call_audio_bridge.source[tap_id];
    u16 requested = USB_CALL_AUDIO_BRIDGE_FRAME_SAMPLES;
    u16 read_count;
    u16 missing;

    if (adjust_drift) {
        if (source->ring.count
            > USB_CALL_AUDIO_BRIDGE_PRELOAD_SAMPLES
              + USB_CALL_AUDIO_BRIDGE_DRIFT_WINDOW_SAMPLES) {
            usb_call_audio_bridge_ring_discard(&source->ring, 1);
            source->interval.drift_drop_samples++;
        } else if (source->ring.count
                   < USB_CALL_AUDIO_BRIDGE_PRELOAD_SAMPLES
                     - USB_CALL_AUDIO_BRIDGE_DRIFT_WINDOW_SAMPLES
                   && source->ring.count >= requested - 1) {
            requested--;
            source->interval.drift_duplicate_samples++;
        }
    }

    read_count = usb_call_audio_bridge_ring_read(
        &source->ring, frame, requested);
    if (read_count == requested
        && requested < USB_CALL_AUDIO_BRIDGE_FRAME_SAMPLES) {
        frame[read_count] = read_count ? frame[read_count - 1] : 0;
        read_count++;
    }
    missing = USB_CALL_AUDIO_BRIDGE_FRAME_SAMPLES - read_count;
    if (missing) {
        memset(frame + read_count, 0, missing * sizeof(s16));
        source->interval.underflow_samples += missing;
        source->interval.underflow_events++;
        if (source->consecutive_underflows != 0xff) {
            source->consecutive_underflows++;
        }
    } else {
        source->consecutive_underflows = 0;
    }
    source->interval.consumed_samples +=
        USB_CALL_AUDIO_BRIDGE_FRAME_SAMPLES;
    return missing;
}

static void usb_call_audio_bridge_update_level_stats(
    struct usb_call_audio_bridge_source_interval *interval,
    const s16 *frame)
{
    u16 index;

    for (index = 0; index < USB_CALL_AUDIO_BRIDGE_FRAME_SAMPLES; index++) {
        u32 absolute = usb_call_audio_bridge_absolute(frame[index]);

        interval->absolute_sum += absolute;
        if (absolute > interval->peak) {
            interval->peak = absolute;
        }
    }
}

static void usb_call_audio_bridge_update_mix_level_stats(const s16 *frame)
{
    u16 index;

    for (index = 0; index < USB_CALL_AUDIO_BRIDGE_FRAME_SAMPLES; index++) {
        u32 absolute = usb_call_audio_bridge_absolute(frame[index]);

        usb_call_audio_bridge.mix_interval.absolute_sum += absolute;
        if (absolute > usb_call_audio_bridge.mix_interval.peak) {
            usb_call_audio_bridge.mix_interval.peak = absolute;
        }
    }
}

static u8 usb_call_audio_bridge_sources_valid_locked(void)
{
    return usb_call_audio_bridge.last_runtime_active[USB_CALL_AUDIO_TAP_NEAR]
           && usb_call_audio_bridge.last_runtime_active[USB_CALL_AUDIO_TAP_FAR]
           && usb_call_audio_bridge.source[USB_CALL_AUDIO_TAP_NEAR].format_valid
           && usb_call_audio_bridge.source[USB_CALL_AUDIO_TAP_FAR].format_valid
           && usb_call_audio_bridge.source[USB_CALL_AUDIO_TAP_NEAR]
                  .accepted_format.format_generation
              == usb_call_audio_bridge
                     .last_runtime_generation[USB_CALL_AUDIO_TAP_NEAR]
           && usb_call_audio_bridge.source[USB_CALL_AUDIO_TAP_FAR]
                  .accepted_format.format_generation
              == usb_call_audio_bridge
                     .last_runtime_generation[USB_CALL_AUDIO_TAP_FAR];
}

static void usb_call_audio_bridge_stream_observer(
    void *priv,
    enum usb_call_audio_tap_id tap_id,
    const struct usb_call_audio_tap_format *format,
    u8 active)
{
    (void)priv;
    if (tap_id >= USB_CALL_AUDIO_TAP_COUNT || !format) {
        return;
    }

    spin_lock(&usb_call_audio_bridge_lock);
    if (!usb_call_audio_bridge.opened) {
        spin_unlock(&usb_call_audio_bridge_lock);
        return;
    }
    if (usb_call_audio_bridge.last_runtime_active[tap_id] != !!active
        || usb_call_audio_bridge.last_runtime_generation[tap_id]
           != format->format_generation) {
        if (usb_call_audio_bridge.timeline_active) {
            usb_call_audio_bridge.mix_interval.source_lost_events++;
        }
        usb_call_audio_bridge.last_runtime_active[tap_id] = !!active;
        usb_call_audio_bridge.last_runtime_generation[tap_id] =
            format->format_generation;
        usb_call_audio_bridge.source[tap_id].last_input_ms = 0;
        usb_call_audio_bridge_invalidate_locked();
    }
    spin_unlock(&usb_call_audio_bridge_lock);
}

static void usb_call_audio_bridge_drain_from_near(void)
{
    usb_call_audio_bridge_consumer_t consumer;
    void *consumer_priv;
    u32 capture_generation;
    u8 source_lost;
    u16 index;

    spin_lock(&usb_call_audio_bridge_lock);
    if (!usb_call_audio_bridge.opened || usb_call_audio_bridge.drain_busy) {
        spin_unlock(&usb_call_audio_bridge_lock);
        return;
    }
    usb_call_audio_bridge.drain_busy = 1;
    spin_unlock(&usb_call_audio_bridge_lock);

    while (1) {
        source_lost = 0;
        consumer = NULL;
        consumer_priv = NULL;
        capture_generation = 0;

        spin_lock(&usb_call_audio_bridge_lock);
        if (!usb_call_audio_bridge.opened
            || !usb_call_audio_bridge_sources_valid_locked()) {
            usb_call_audio_bridge.drain_busy = 0;
            spin_unlock(&usb_call_audio_bridge_lock);
            return;
        }

        if (!usb_call_audio_bridge.timeline_active) {
            if (usb_call_audio_bridge.source[USB_CALL_AUDIO_TAP_NEAR].ring.count
                    < USB_CALL_AUDIO_BRIDGE_PRELOAD_SAMPLES
                      + USB_CALL_AUDIO_BRIDGE_FRAME_SAMPLES
                || usb_call_audio_bridge.source[USB_CALL_AUDIO_TAP_FAR]
                       .ring.count
                   < USB_CALL_AUDIO_BRIDGE_PRELOAD_SAMPLES
                     + USB_CALL_AUDIO_BRIDGE_FRAME_SAMPLES) {
                usb_call_audio_bridge.drain_busy = 0;
                spin_unlock(&usb_call_audio_bridge_lock);
                return;
            }
            usb_call_audio_bridge.timeline_active = 1;
        }
        if (usb_call_audio_bridge.source[USB_CALL_AUDIO_TAP_NEAR].ring.count
            < USB_CALL_AUDIO_BRIDGE_PRELOAD_SAMPLES
              + USB_CALL_AUDIO_BRIDGE_FRAME_SAMPLES) {
            usb_call_audio_bridge.drain_busy = 0;
            spin_unlock(&usb_call_audio_bridge_lock);
            return;
        }

        usb_call_audio_bridge_read_source_locked(
            USB_CALL_AUDIO_TAP_NEAR,
            usb_call_audio_bridge.near_frame, 0);
        usb_call_audio_bridge_read_source_locked(
            USB_CALL_AUDIO_TAP_FAR,
            usb_call_audio_bridge.far_frame, 1);
        if (usb_call_audio_bridge.source[USB_CALL_AUDIO_TAP_FAR]
                .consecutive_underflows
            >= USB_CALL_AUDIO_BRIDGE_SOURCE_LOST_FRAMES) {
            source_lost = 1;
        }
        capture_generation = usb_call_audio_bridge.capture_generation;
        consumer = usb_call_audio_bridge.consumer;
        consumer_priv = usb_call_audio_bridge.consumer_priv;
        spin_unlock(&usb_call_audio_bridge_lock);

        for (index = 0; index < USB_CALL_AUDIO_BRIDGE_FRAME_SAMPLES; index++) {
            s32 mixed = (s32)usb_call_audio_bridge.near_frame[index]
                        + usb_call_audio_bridge.far_frame[index];

            usb_call_audio_bridge.mix_frame[index] = (s16)(mixed / 2);
        }

        spin_lock(&usb_call_audio_bridge_lock);
        if (!usb_call_audio_bridge.opened) {
            usb_call_audio_bridge.drain_busy = 0;
            spin_unlock(&usb_call_audio_bridge_lock);
            return;
        }
        if (capture_generation != usb_call_audio_bridge.capture_generation) {
            usb_call_audio_bridge.mix_interval.stale_output_frames++;
            spin_unlock(&usb_call_audio_bridge_lock);
            continue;
        }
        usb_call_audio_bridge_update_level_stats(
            &usb_call_audio_bridge.source[USB_CALL_AUDIO_TAP_NEAR].interval,
            usb_call_audio_bridge.near_frame);
        usb_call_audio_bridge_update_level_stats(
            &usb_call_audio_bridge.source[USB_CALL_AUDIO_TAP_FAR].interval,
            usb_call_audio_bridge.far_frame);
        usb_call_audio_bridge_update_mix_level_stats(
            usb_call_audio_bridge.mix_frame);
        usb_call_audio_bridge.mix_interval.frames++;
        usb_call_audio_bridge.mix_interval.samples +=
            USB_CALL_AUDIO_BRIDGE_FRAME_SAMPLES;
        if (source_lost) {
            usb_call_audio_bridge.mix_interval.source_lost_events++;
            usb_call_audio_bridge_invalidate_locked();
            consumer = NULL;
        }
        spin_unlock(&usb_call_audio_bridge_lock);

        if (consumer) {
            consumer(consumer_priv, capture_generation,
                     usb_call_audio_bridge.mix_frame,
                     USB_CALL_AUDIO_BRIDGE_FRAME_SAMPLES);
        }
    }
}

int usb_call_audio_bridge_open(
    usb_call_audio_bridge_consumer_t consumer, void *priv)
{
    struct usb_call_audio_tap_runtime_info runtime;
    u8 tap_id;
    int ret;

    spin_lock(&usb_call_audio_bridge_lock);
    if (usb_call_audio_bridge.opened) {
        spin_unlock(&usb_call_audio_bridge_lock);
        return -EBUSY;
    }
    memset(&usb_call_audio_bridge, 0, sizeof(usb_call_audio_bridge));
    usb_call_audio_bridge.consumer = consumer;
    usb_call_audio_bridge.consumer_priv = priv;
    usb_call_audio_bridge.capture_generation = 1;
    usb_call_audio_bridge.opened = 1;
    spin_unlock(&usb_call_audio_bridge_lock);

    ret = usb_call_audio_tap_register_consumer(
        usb_call_audio_bridge_tap_consumer, NULL);
    if (ret) {
        spin_lock(&usb_call_audio_bridge_lock);
        usb_call_audio_bridge.opened = 0;
        usb_call_audio_bridge.consumer = NULL;
        usb_call_audio_bridge.consumer_priv = NULL;
        spin_unlock(&usb_call_audio_bridge_lock);
        return ret;
    }
    ret = usb_call_audio_tap_register_stream_observer(
        usb_call_audio_bridge_stream_observer, NULL);
    if (ret) {
        usb_call_audio_tap_unregister_consumer(
            usb_call_audio_bridge_tap_consumer, NULL);
        spin_lock(&usb_call_audio_bridge_lock);
        usb_call_audio_bridge.opened = 0;
        usb_call_audio_bridge.consumer = NULL;
        usb_call_audio_bridge.consumer_priv = NULL;
        spin_unlock(&usb_call_audio_bridge_lock);
        return ret;
    }
    for (tap_id = 0; tap_id < USB_CALL_AUDIO_TAP_COUNT; tap_id++) {
        usb_call_audio_tap_get_runtime_info(tap_id, &runtime);
        usb_call_audio_bridge_stream_observer(
            NULL, tap_id, &runtime.format, runtime.stream_active);
    }

    usb_call_audio_tap_reset_stats(USB_CALL_AUDIO_TAP_NEAR);
    usb_call_audio_tap_reset_stats(USB_CALL_AUDIO_TAP_FAR);
    usb_call_audio_tap_set_gate(USB_CALL_AUDIO_TAP_NEAR, 1);
    usb_call_audio_tap_set_gate(USB_CALL_AUDIO_TAP_FAR, 1);
    return 0;
}

void usb_call_audio_bridge_close(void)
{
    usb_call_audio_tap_set_gate(USB_CALL_AUDIO_TAP_NEAR, 0);
    usb_call_audio_tap_set_gate(USB_CALL_AUDIO_TAP_FAR, 0);
    usb_call_audio_tap_unregister_consumer(
        usb_call_audio_bridge_tap_consumer, NULL);
    usb_call_audio_tap_unregister_stream_observer(
        usb_call_audio_bridge_stream_observer, NULL);

    spin_lock(&usb_call_audio_bridge_lock);
    usb_call_audio_bridge.opened = 0;
    usb_call_audio_bridge.timeline_active = 0;
    usb_call_audio_bridge.consumer = NULL;
    usb_call_audio_bridge.consumer_priv = NULL;
    spin_unlock(&usb_call_audio_bridge_lock);
}

void usb_call_audio_bridge_take_snapshot(
    struct usb_call_audio_bridge_snapshot *snapshot)
{
    u8 tap_id;

    if (!snapshot) {
        return;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    spin_lock(&usb_call_audio_bridge_lock);
    snapshot->opened = usb_call_audio_bridge.opened;
    snapshot->timeline_active = usb_call_audio_bridge.timeline_active;
    snapshot->capture_generation = usb_call_audio_bridge.capture_generation;
    snapshot->mix = usb_call_audio_bridge.mix_interval;
    memset(&usb_call_audio_bridge.mix_interval, 0,
           sizeof(usb_call_audio_bridge.mix_interval));
    for (tap_id = 0; tap_id < USB_CALL_AUDIO_TAP_COUNT; tap_id++) {
        struct usb_call_audio_bridge_source *source =
            &usb_call_audio_bridge.source[tap_id];

        snapshot->source[tap_id].format = source->accepted_format;
        snapshot->source[tap_id].format_valid = source->format_valid;
        snapshot->source[tap_id].stream_active =
            usb_call_audio_bridge.last_runtime_active[tap_id];
        snapshot->source[tap_id].buffer_samples = source->ring.count;
        snapshot->source[tap_id].peak_buffer_samples =
            source->peak_buffer_samples;
        snapshot->source[tap_id].interval = source->interval;
        memset(&source->interval, 0, sizeof(source->interval));
        source->peak_buffer_samples = source->ring.count;
    }
    spin_unlock(&usb_call_audio_bridge_lock);
}
