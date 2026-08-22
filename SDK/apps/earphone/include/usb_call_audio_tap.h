#ifndef USB_CALL_AUDIO_TAP_H
#define USB_CALL_AUDIO_TAP_H

#include "system/includes.h"

#define USB_CALL_AUDIO_NEAR_SWITCH_NAME    "RDX_NearTap"
#define USB_CALL_AUDIO_FAR_SWITCH_NAME     "RDX_FarTap"

enum usb_call_audio_tap_id {
    USB_CALL_AUDIO_TAP_NEAR = 0,
    USB_CALL_AUDIO_TAP_FAR,
    USB_CALL_AUDIO_TAP_COUNT,
};

struct usb_call_audio_tap_format {
    u32 sample_rate;
    u32 coding_type;
    u32 format_generation;
    u8 channels;
    u8 bit_width;
    u8 qval;
};

struct usb_call_audio_tap_runtime_info {
    struct usb_call_audio_tap_format format;
    u32 frame_count;
    u32 byte_count;
    u32 dropped_frame_count;
    u8 gate_open;
    u8 stream_active;
    u8 consumer_attached;
};

typedef void (*usb_call_audio_tap_consumer_t)(
    void *priv,
    enum usb_call_audio_tap_id tap_id,
    const struct usb_call_audio_tap_format *format,
    const void *data,
    u32 len);

typedef void (*usb_call_audio_tap_stream_observer_t)(
    void *priv,
    enum usb_call_audio_tap_id tap_id,
    const struct usb_call_audio_tap_format *format,
    u8 active);

/*
 * The consumer runs in the JLStream audio context.  It must not block or
 * retain data after returning.  A bridge may perform fixed-bound PCM
 * normalization and bounded-buffer work here, but must not allocate, wait,
 * or perform I/O.
 */
int usb_call_audio_tap_register_consumer(
    usb_call_audio_tap_consumer_t consumer, void *priv);
void usb_call_audio_tap_unregister_consumer(
    usb_call_audio_tap_consumer_t consumer, void *priv);
int usb_call_audio_tap_register_stream_observer(
    usb_call_audio_tap_stream_observer_t observer, void *priv);
void usb_call_audio_tap_unregister_stream_observer(
    usb_call_audio_tap_stream_observer_t observer, void *priv);

/* The observer runs synchronously in the Tap stream start/stop context. */

void usb_call_audio_tap_set_gate(enum usb_call_audio_tap_id tap_id, u8 open);
int usb_call_audio_tap_gate_is_open(enum usb_call_audio_tap_id tap_id);

void usb_call_audio_tap_stream_start(
    enum usb_call_audio_tap_id tap_id,
    u32 sample_rate,
    u32 coding_type,
    u8 channels,
    u8 bit_width,
    u8 qval);
void usb_call_audio_tap_stream_stop(enum usb_call_audio_tap_id tap_id);
void usb_call_audio_tap_input(
    enum usb_call_audio_tap_id tap_id, const void *data, u32 len);

void usb_call_audio_tap_get_runtime_info(
    enum usb_call_audio_tap_id tap_id,
    struct usb_call_audio_tap_runtime_info *info);
void usb_call_audio_tap_reset_stats(enum usb_call_audio_tap_id tap_id);

#endif
