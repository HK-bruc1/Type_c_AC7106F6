#include "app_config.h"

#if TCFG_USB_CALL_AUDIO_TAP_DIAG_ENABLE

#include "spinlock.h"
#include "system/init.h"
#include "system/timer.h"
#include "usb_call_audio_tap.h"

struct usb_call_audio_tap_diag_interval {
    u32 frames;
    u32 bytes;
    u32 sampled_values;
    u32 peak;
    u64 absolute_sum;
};

struct usb_call_audio_tap_diag_context {
    struct usb_call_audio_tap_diag_interval interval[USB_CALL_AUDIO_TAP_COUNT];
    u32 last_generation[USB_CALL_AUDIO_TAP_COUNT];
    u8 last_active[USB_CALL_AUDIO_TAP_COUNT];
    u16 timer_id;
};

static struct usb_call_audio_tap_diag_context usb_call_audio_tap_diag;
static DEFINE_SPINLOCK(usb_call_audio_tap_diag_lock);

static const char *usb_call_audio_tap_diag_name(
    enum usb_call_audio_tap_id tap_id)
{
    return tap_id == USB_CALL_AUDIO_TAP_NEAR ? "near" : "far";
}

static u32 usb_call_audio_tap_diag_full_scale(u8 qval)
{
    if (!qval || qval >= 31) {
        return 0x7fffffffUL;
    }
    return (1UL << qval) - 1;
}

static void usb_call_audio_tap_diag_consumer(
    void *priv,
    enum usb_call_audio_tap_id tap_id,
    const struct usb_call_audio_tap_format *format,
    const void *data,
    u32 len)
{
    struct usb_call_audio_tap_diag_interval local = {0};
    u32 i;

    if (!format || !data || !len || tap_id >= USB_CALL_AUDIO_TAP_COUNT) {
        return;
    }

    local.frames = 1;
    local.bytes = len;
    if (!format->bit_width) {
        const s16 *samples = (const s16 *)data;
        u32 count = len / sizeof(s16);

        for (i = 0; i < count;
             i += USB_CALL_AUDIO_TAP_DIAG_SAMPLE_DECIMATION) {
            s32 value = samples[i];
            u32 absolute = value < 0 ? (u32)-value : (u32)value;

            if (absolute > local.peak) {
                local.peak = absolute;
            }
            local.absolute_sum += absolute;
            local.sampled_values++;
        }
    } else {
        const s32 *samples = (const s32 *)data;
        u32 count = len / sizeof(s32);

        for (i = 0; i < count;
             i += USB_CALL_AUDIO_TAP_DIAG_SAMPLE_DECIMATION) {
            s64 value = samples[i];
            u32 absolute;

            if (value < 0) {
                value = -value;
            }
            absolute = value > 0x7fffffffLL
                       ? 0x7fffffffUL : (u32)value;
            if (absolute > local.peak) {
                local.peak = absolute;
            }
            local.absolute_sum += absolute;
            local.sampled_values++;
        }
    }

    spin_lock(&usb_call_audio_tap_diag_lock);
    usb_call_audio_tap_diag.interval[tap_id].frames += local.frames;
    usb_call_audio_tap_diag.interval[tap_id].bytes += local.bytes;
    usb_call_audio_tap_diag.interval[tap_id].sampled_values +=
        local.sampled_values;
    usb_call_audio_tap_diag.interval[tap_id].absolute_sum +=
        local.absolute_sum;
    if (local.peak > usb_call_audio_tap_diag.interval[tap_id].peak) {
        usb_call_audio_tap_diag.interval[tap_id].peak = local.peak;
    }
    spin_unlock(&usb_call_audio_tap_diag_lock);
}

static void usb_call_audio_tap_diag_dump_one(
    enum usb_call_audio_tap_id tap_id)
{
    struct usb_call_audio_tap_diag_interval interval;
    struct usb_call_audio_tap_runtime_info info;
    u32 full_scale;
    u32 mean = 0;
    u32 peak_permille = 0;
    u32 mean_permille = 0;
    u8 state_changed;

    usb_call_audio_tap_get_runtime_info(tap_id, &info);

    spin_lock(&usb_call_audio_tap_diag_lock);
    interval = usb_call_audio_tap_diag.interval[tap_id];
    memset(&usb_call_audio_tap_diag.interval[tap_id], 0,
           sizeof(usb_call_audio_tap_diag.interval[tap_id]));
    state_changed =
        usb_call_audio_tap_diag.last_generation[tap_id]
            != info.format.format_generation
        || usb_call_audio_tap_diag.last_active[tap_id] != info.stream_active;
    usb_call_audio_tap_diag.last_generation[tap_id] =
        info.format.format_generation;
    usb_call_audio_tap_diag.last_active[tap_id] = info.stream_active;
    spin_unlock(&usb_call_audio_tap_diag_lock);

    if (!interval.frames && !state_changed) {
        return;
    }

    full_scale = usb_call_audio_tap_diag_full_scale(info.format.qval);
    if (interval.sampled_values) {
        mean = (u32)(interval.absolute_sum / interval.sampled_values);
    }
    if (full_scale) {
        peak_permille = (u32)(((u64)interval.peak * 1000) / full_scale);
        mean_permille = (u32)(((u64)mean * 1000) / full_scale);
    }

    printf("[USB_CALL_TAP][DIAG] state tap=%s gate=%u active=%u gen=%u sr=%u ch=%u storage_bits=%u qval=%u\n",
           usb_call_audio_tap_diag_name(tap_id),
           info.gate_open, info.stream_active,
           (unsigned int)info.format.format_generation,
           (unsigned int)info.format.sample_rate,
           info.format.channels,
           info.format.bit_width ? 32 : 16,
           info.format.qval);
    printf("[USB_CALL_TAP][DIAG] level tap=%s frames=%u bytes=%u sampled=%u peak=%u peak_pm=%u mean_pm=%u drop=%u\n",
           usb_call_audio_tap_diag_name(tap_id),
           (unsigned int)interval.frames,
           (unsigned int)interval.bytes,
           (unsigned int)interval.sampled_values,
           (unsigned int)interval.peak,
           (unsigned int)peak_permille,
           (unsigned int)mean_permille,
           (unsigned int)info.dropped_frame_count);
}

static void usb_call_audio_tap_diag_timer(void *priv)
{
    usb_call_audio_tap_diag_dump_one(USB_CALL_AUDIO_TAP_NEAR);
    usb_call_audio_tap_diag_dump_one(USB_CALL_AUDIO_TAP_FAR);
}

static int usb_call_audio_tap_diag_init(void)
{
    int ret;

    ret = usb_call_audio_tap_register_consumer(
        usb_call_audio_tap_diag_consumer, NULL);
    if (ret) {
        printf("[USB_CALL_TAP][DIAG] init=failed reason=consumer ret=%d\n",
               ret);
        return ret;
    }

    usb_call_audio_tap_reset_stats(USB_CALL_AUDIO_TAP_NEAR);
    usb_call_audio_tap_reset_stats(USB_CALL_AUDIO_TAP_FAR);
    usb_call_audio_tap_set_gate(
        USB_CALL_AUDIO_TAP_NEAR,
        !!(TCFG_USB_CALL_AUDIO_TAP_DIAG_GATE_MASK
           & (1 << USB_CALL_AUDIO_TAP_NEAR)));
    usb_call_audio_tap_set_gate(
        USB_CALL_AUDIO_TAP_FAR,
        !!(TCFG_USB_CALL_AUDIO_TAP_DIAG_GATE_MASK
           & (1 << USB_CALL_AUDIO_TAP_FAR)));

    usb_call_audio_tap_diag.timer_id = sys_timer_add(
        NULL, usb_call_audio_tap_diag_timer,
        USB_CALL_AUDIO_TAP_DIAG_PERIOD_MS);
    if (!usb_call_audio_tap_diag.timer_id) {
        usb_call_audio_tap_set_gate(USB_CALL_AUDIO_TAP_NEAR, 0);
        usb_call_audio_tap_set_gate(USB_CALL_AUDIO_TAP_FAR, 0);
        usb_call_audio_tap_unregister_consumer(
            usb_call_audio_tap_diag_consumer, NULL);
        printf("[USB_CALL_TAP][DIAG] init=failed reason=timer\n");
        return -EFAULT;
    }

    printf("[USB_CALL_TAP][DIAG] init=ok gate_mask=0x%x period_ms=%u decimation=%u\n",
           TCFG_USB_CALL_AUDIO_TAP_DIAG_GATE_MASK,
           USB_CALL_AUDIO_TAP_DIAG_PERIOD_MS,
           USB_CALL_AUDIO_TAP_DIAG_SAMPLE_DECIMATION);
    return 0;
}

late_initcall(usb_call_audio_tap_diag_init);

#endif /* TCFG_USB_CALL_AUDIO_TAP_DIAG_ENABLE */
