#ifndef USB_CALL_AUDIO_BRIDGE_H
#define USB_CALL_AUDIO_BRIDGE_H

#include "system/includes.h"
#include "usb_call_audio_tap.h"

#define USB_CALL_AUDIO_BRIDGE_FRAME_SAMPLES \
    (USB_CALL_AUDIO_BRIDGE_OUTPUT_SAMPLE_RATE \
     * USB_CALL_AUDIO_BRIDGE_FRAME_MS / 1000)

struct usb_call_audio_bridge_source_interval {
    u32 input_samples;
    u32 normalized_samples;
    u32 consumed_samples;
    u32 underflow_samples;
    u32 underflow_events;
    u32 overflow_samples;
    u32 overflow_events;
    u32 drift_drop_samples;
    u32 drift_duplicate_samples;
    u32 stale_input_frames;
    u32 invalid_input_frames;
    u32 peak;
    u64 absolute_sum;
};

struct usb_call_audio_bridge_mix_interval {
    u32 frames;
    u32 samples;
    u32 clipping_samples;
    u32 source_lost_events;
    u32 generation_resets;
    u32 stale_output_frames;
    u32 peak;
    u64 absolute_sum;
};

struct usb_call_audio_bridge_source_snapshot {
    struct usb_call_audio_tap_format format;
    struct usb_call_audio_bridge_source_interval interval;
    u16 buffer_samples;
    u16 peak_buffer_samples;
    u8 stream_active;
    u8 format_valid;
};

struct usb_call_audio_bridge_snapshot {
    struct usb_call_audio_bridge_source_snapshot
        source[USB_CALL_AUDIO_TAP_COUNT];
    struct usb_call_audio_bridge_mix_interval mix;
    u32 capture_generation;
    u8 opened;
    u8 timeline_active;
};

typedef void (*usb_call_audio_bridge_consumer_t)(
    void *priv,
    u32 capture_generation,
    const s16 *pcm,
    u16 samples);

/*
 * The output consumer runs in the Near Tap JLStream audio context.  It must
 * finish within a fixed bound, must not block, and must not retain the PCM
 * pointer after returning.
 */
int usb_call_audio_bridge_open(
    usb_call_audio_bridge_consumer_t consumer, void *priv);
void usb_call_audio_bridge_close(void);
void usb_call_audio_bridge_take_snapshot(
    struct usb_call_audio_bridge_snapshot *snapshot);

#endif
