#include "app_config.h"

#if TCFG_USB_CALL_AUDIO_BRIDGE_DIAG_ENABLE

#include "system/init.h"
#include "system/timer.h"
#include "usb_call_audio_bridge.h"

static u16 usb_call_audio_bridge_diag_timer_id;

static u32 usb_call_audio_bridge_diag_permille(u64 value, u32 samples)
{
    if (!samples) {
        return 0;
    }
    return (u32)((value * 1000) / (samples * 32767ULL));
}

static void usb_call_audio_bridge_diag_dump_source(
    const char *name,
    const struct usb_call_audio_bridge_source_snapshot *source)
{
    const struct usb_call_audio_bridge_source_interval *interval =
        &source->interval;
    u32 peak_permille = (u32)(((u64)interval->peak * 1000) / 32767);
    u32 mean_permille = usb_call_audio_bridge_diag_permille(
        interval->absolute_sum, interval->consumed_samples);

    printf("[USB_CALL_BRIDGE][DIAG] source=%s active=%u valid=%u gen=%u fmt=%u/%u/%u/q%u in=%u norm=%u used=%u level=%u peak_level=%u\n",
           name, source->stream_active, source->format_valid,
           (unsigned int)source->format.format_generation,
           (unsigned int)source->format.sample_rate,
           source->format.channels,
           source->format.bit_width ? 32 : 16,
           source->format.qval,
           (unsigned int)interval->input_samples,
           (unsigned int)interval->normalized_samples,
           (unsigned int)interval->consumed_samples,
           source->buffer_samples, source->peak_buffer_samples);
    printf("[USB_CALL_BRIDGE][DIAG] quality=%s peak_pm=%u mean_pm=%u under=%u/%u overflow=%u/%u drift_drop=%u drift_dup=%u stale=%u invalid=%u\n",
           name,
           (unsigned int)peak_permille,
           (unsigned int)mean_permille,
           (unsigned int)interval->underflow_events,
           (unsigned int)interval->underflow_samples,
           (unsigned int)interval->overflow_events,
           (unsigned int)interval->overflow_samples,
           (unsigned int)interval->drift_drop_samples,
           (unsigned int)interval->drift_duplicate_samples,
           (unsigned int)interval->stale_input_frames,
           (unsigned int)interval->invalid_input_frames);
}

static void usb_call_audio_bridge_diag_dump(void *priv)
{
    struct usb_call_audio_bridge_snapshot snapshot;
    u32 peak_permille;
    u32 mean_permille;

    (void)priv;
    usb_call_audio_bridge_take_snapshot(&snapshot);
    peak_permille =
        (u32)(((u64)snapshot.mix.peak * 1000) / 32767);
    mean_permille = usb_call_audio_bridge_diag_permille(
        snapshot.mix.absolute_sum, snapshot.mix.samples);

    printf("[USB_CALL_BRIDGE][DIAG] state open=%u timeline=%u capture=%u resets=%u source_lost=%u stale_out=%u\n",
           snapshot.opened, snapshot.timeline_active,
           (unsigned int)snapshot.capture_generation,
           (unsigned int)snapshot.mix.generation_resets,
           (unsigned int)snapshot.mix.source_lost_events,
           (unsigned int)snapshot.mix.stale_output_frames);
    usb_call_audio_bridge_diag_dump_source(
        "near", &snapshot.source[USB_CALL_AUDIO_TAP_NEAR]);
    usb_call_audio_bridge_diag_dump_source(
        "far", &snapshot.source[USB_CALL_AUDIO_TAP_FAR]);
    printf("[USB_CALL_BRIDGE][DIAG] mix frames=%u samples=%u peak_pm=%u mean_pm=%u clip=%u\n",
           (unsigned int)snapshot.mix.frames,
           (unsigned int)snapshot.mix.samples,
           (unsigned int)peak_permille,
           (unsigned int)mean_permille,
           (unsigned int)snapshot.mix.clipping_samples);
}

static int usb_call_audio_bridge_diag_init(void)
{
    int ret;

    ret = usb_call_audio_bridge_open(NULL, NULL, NULL);
    if (ret) {
        printf("[USB_CALL_BRIDGE][DIAG] init=failed reason=bridge ret=%d\n",
               ret);
        return ret;
    }
    usb_call_audio_bridge_diag_timer_id = sys_timer_add(
        NULL, usb_call_audio_bridge_diag_dump,
        USB_CALL_AUDIO_BRIDGE_REPORT_MS);
    if (!usb_call_audio_bridge_diag_timer_id) {
        usb_call_audio_bridge_close();
        printf("[USB_CALL_BRIDGE][DIAG] init=failed reason=timer\n");
        return -EFAULT;
    }

    printf("[USB_CALL_BRIDGE][DIAG] init=ok clock=near_samples lifecycle=tap_event out=%u/1/16 frame_ms=%u buffer_ms=%u preload_ms=%u start_ms=%u source_lost_ms=%u\n",
           USB_CALL_AUDIO_BRIDGE_OUTPUT_SAMPLE_RATE,
           USB_CALL_AUDIO_BRIDGE_FRAME_MS,
           USB_CALL_AUDIO_BRIDGE_BUFFER_MS,
           USB_CALL_AUDIO_BRIDGE_PRELOAD_MS,
           USB_CALL_AUDIO_BRIDGE_PRELOAD_MS
             + USB_CALL_AUDIO_BRIDGE_FRAME_MS,
           USB_CALL_AUDIO_BRIDGE_SOURCE_LOST_MS);
    return 0;
}

late_initcall(usb_call_audio_bridge_diag_init);

#endif /* TCFG_USB_CALL_AUDIO_BRIDGE_DIAG_ENABLE */
