#include "app_config.h"
#include "spinlock.h"
#include "usb_call_audio_tap.h"

struct usb_call_audio_tap_state {
    struct usb_call_audio_tap_runtime_info info;
};

struct usb_call_audio_tap_context {
    struct usb_call_audio_tap_state tap[USB_CALL_AUDIO_TAP_COUNT];
    usb_call_audio_tap_consumer_t consumer;
    void *consumer_priv;
    usb_call_audio_tap_stream_observer_t stream_observer;
    void *stream_observer_priv;
};

static struct usb_call_audio_tap_context usb_call_audio_tap;
static DEFINE_SPINLOCK(usb_call_audio_tap_lock);

static int usb_call_audio_tap_id_is_valid(enum usb_call_audio_tap_id tap_id)
{
    return tap_id >= USB_CALL_AUDIO_TAP_NEAR
           && tap_id < USB_CALL_AUDIO_TAP_COUNT;
}

static u32 usb_call_audio_tap_next_generation(u32 generation)
{
    generation++;
    return generation ? generation : 1;
}

int usb_call_audio_tap_register_consumer(
    usb_call_audio_tap_consumer_t consumer, void *priv)
{
    int ret = 0;

    if (!consumer) {
        return -EINVAL;
    }

    spin_lock(&usb_call_audio_tap_lock);
    if (usb_call_audio_tap.consumer
        && (usb_call_audio_tap.consumer != consumer
            || usb_call_audio_tap.consumer_priv != priv)) {
        ret = -EBUSY;
    } else {
        usb_call_audio_tap.consumer = consumer;
        usb_call_audio_tap.consumer_priv = priv;
    }
    spin_unlock(&usb_call_audio_tap_lock);
    return ret;
}

void usb_call_audio_tap_unregister_consumer(
    usb_call_audio_tap_consumer_t consumer, void *priv)
{
    spin_lock(&usb_call_audio_tap_lock);
    if (usb_call_audio_tap.consumer == consumer
        && usb_call_audio_tap.consumer_priv == priv) {
        usb_call_audio_tap.consumer = NULL;
        usb_call_audio_tap.consumer_priv = NULL;
    }
    spin_unlock(&usb_call_audio_tap_lock);
}

int usb_call_audio_tap_register_stream_observer(
    usb_call_audio_tap_stream_observer_t observer, void *priv)
{
    int ret = 0;

    if (!observer) {
        return -EINVAL;
    }

    spin_lock(&usb_call_audio_tap_lock);
    if (usb_call_audio_tap.stream_observer
        && (usb_call_audio_tap.stream_observer != observer
            || usb_call_audio_tap.stream_observer_priv != priv)) {
        ret = -EBUSY;
    } else {
        usb_call_audio_tap.stream_observer = observer;
        usb_call_audio_tap.stream_observer_priv = priv;
    }
    spin_unlock(&usb_call_audio_tap_lock);
    return ret;
}

void usb_call_audio_tap_unregister_stream_observer(
    usb_call_audio_tap_stream_observer_t observer, void *priv)
{
    spin_lock(&usb_call_audio_tap_lock);
    if (usb_call_audio_tap.stream_observer == observer
        && usb_call_audio_tap.stream_observer_priv == priv) {
        usb_call_audio_tap.stream_observer = NULL;
        usb_call_audio_tap.stream_observer_priv = NULL;
    }
    spin_unlock(&usb_call_audio_tap_lock);
}

void usb_call_audio_tap_set_gate(enum usb_call_audio_tap_id tap_id, u8 open)
{
    if (!usb_call_audio_tap_id_is_valid(tap_id)) {
        return;
    }

    spin_lock(&usb_call_audio_tap_lock);
    usb_call_audio_tap.tap[tap_id].info.gate_open = !!open;
    spin_unlock(&usb_call_audio_tap_lock);
}

int usb_call_audio_tap_gate_is_open(enum usb_call_audio_tap_id tap_id)
{
    int open;

    if (!usb_call_audio_tap_id_is_valid(tap_id)) {
        return 0;
    }

    spin_lock(&usb_call_audio_tap_lock);
    open = usb_call_audio_tap.tap[tap_id].info.gate_open;
    spin_unlock(&usb_call_audio_tap_lock);
    return open;
}

void usb_call_audio_tap_stream_start(
    enum usb_call_audio_tap_id tap_id,
    u32 sample_rate,
    u32 coding_type,
    u8 channels,
    u8 bit_width,
    u8 qval)
{
    struct usb_call_audio_tap_runtime_info *info;
    struct usb_call_audio_tap_format format;
    usb_call_audio_tap_stream_observer_t observer;
    void *observer_priv;
    u32 generation;

    if (!usb_call_audio_tap_id_is_valid(tap_id)) {
        return;
    }

    spin_lock(&usb_call_audio_tap_lock);
    info = &usb_call_audio_tap.tap[tap_id].info;
    info->format.sample_rate = sample_rate;
    info->format.coding_type = coding_type;
    info->format.channels = channels;
    info->format.bit_width = bit_width;
    info->format.qval = qval;
    info->format.format_generation =
        usb_call_audio_tap_next_generation(info->format.format_generation);
    generation = info->format.format_generation;
    info->stream_active = 1;
    format = info->format;
    observer = usb_call_audio_tap.stream_observer;
    observer_priv = usb_call_audio_tap.stream_observer_priv;
    spin_unlock(&usb_call_audio_tap_lock);

    if (observer) {
        observer(observer_priv, tap_id, &format, 1);
    }

    printf("[USB_CALL_TAP] stream=start tap=%s generation=%u sr=%u channels=%u bit_width=%u qval=%u coding=0x%x\n",
           tap_id == USB_CALL_AUDIO_TAP_NEAR ? "near" : "far",
           (unsigned int)generation,
           (unsigned int)sample_rate, channels, bit_width, qval,
           (unsigned int)coding_type);
}

void usb_call_audio_tap_stream_stop(enum usb_call_audio_tap_id tap_id)
{
    struct usb_call_audio_tap_format format;
    usb_call_audio_tap_stream_observer_t observer;
    void *observer_priv;
    u32 generation;

    if (!usb_call_audio_tap_id_is_valid(tap_id)) {
        return;
    }

    spin_lock(&usb_call_audio_tap_lock);
    usb_call_audio_tap.tap[tap_id].info.stream_active = 0;
    usb_call_audio_tap.tap[tap_id].info.format.format_generation =
        usb_call_audio_tap_next_generation(
            usb_call_audio_tap.tap[tap_id].info.format.format_generation);
    generation =
        usb_call_audio_tap.tap[tap_id].info.format.format_generation;
    format = usb_call_audio_tap.tap[tap_id].info.format;
    observer = usb_call_audio_tap.stream_observer;
    observer_priv = usb_call_audio_tap.stream_observer_priv;
    spin_unlock(&usb_call_audio_tap_lock);

    if (observer) {
        observer(observer_priv, tap_id, &format, 0);
    }

    printf("[USB_CALL_TAP] stream=stop tap=%s generation=%u\n",
           tap_id == USB_CALL_AUDIO_TAP_NEAR ? "near" : "far",
           (unsigned int)generation);
}

void usb_call_audio_tap_input(
    enum usb_call_audio_tap_id tap_id, const void *data, u32 len)
{
    struct usb_call_audio_tap_format format;
    struct usb_call_audio_tap_runtime_info *info;
    usb_call_audio_tap_consumer_t consumer;
    void *consumer_priv;

    if (!usb_call_audio_tap_id_is_valid(tap_id) || !data || !len) {
        return;
    }

    spin_lock(&usb_call_audio_tap_lock);
    info = &usb_call_audio_tap.tap[tap_id].info;
    info->frame_count++;
    info->byte_count += len;
    consumer = usb_call_audio_tap.consumer;
    consumer_priv = usb_call_audio_tap.consumer_priv;
    info->consumer_attached = !!consumer;
    if (!info->gate_open || !info->stream_active || !consumer) {
        info->dropped_frame_count++;
        spin_unlock(&usb_call_audio_tap_lock);
        return;
    }
    format = info->format;
    spin_unlock(&usb_call_audio_tap_lock);

    consumer(consumer_priv, tap_id, &format, data, len);
}

void usb_call_audio_tap_get_runtime_info(
    enum usb_call_audio_tap_id tap_id,
    struct usb_call_audio_tap_runtime_info *info)
{
    if (!info) {
        return;
    }

    memset(info, 0, sizeof(*info));
    if (!usb_call_audio_tap_id_is_valid(tap_id)) {
        return;
    }

    spin_lock(&usb_call_audio_tap_lock);
    *info = usb_call_audio_tap.tap[tap_id].info;
    info->consumer_attached = !!usb_call_audio_tap.consumer;
    spin_unlock(&usb_call_audio_tap_lock);
}

void usb_call_audio_tap_reset_stats(enum usb_call_audio_tap_id tap_id)
{
    struct usb_call_audio_tap_runtime_info *info;

    if (!usb_call_audio_tap_id_is_valid(tap_id)) {
        return;
    }

    spin_lock(&usb_call_audio_tap_lock);
    info = &usb_call_audio_tap.tap[tap_id].info;
    info->frame_count = 0;
    info->byte_count = 0;
    info->dropped_frame_count = 0;
    spin_unlock(&usb_call_audio_tap_lock);
}
