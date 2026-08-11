#include "app_config.h"

#if TCFG_RDX_ENABLE && TCFG_RDX_RECORD_SPIKE_ENABLE

#include "system/includes.h"
#include "media/audio_base.h"
#include "ai_voice_recoder.h"
#include "rdx_record_spike.h"
#if TCFG_RDX_RESOURCE_MONITOR_ENABLE
#include "rdx_resource_monitor.h"
#endif

#define RDX_RECORD_SPIKE_EXPECTED_FRAME_LEN   80
#define RDX_RECORD_SPIKE_WARMUP_FRAMES        10

struct rdx_record_spike_stats {
    u32 start_ms;
    u32 first_frame_ms;
    u32 last_frame_ms;
    u32 frame_count;
    u32 min_frame_len;
    u32 max_frame_len;
    u32 unexpected_len_count;
    u32 period_sum_ms;
    u32 min_period_ms;
    u32 max_period_ms;
};

static struct rdx_record_spike_stats rdx_record_spike_stats;
static u16 rdx_record_spike_start_timer;
static u16 rdx_record_spike_stop_timer;
static volatile u8 rdx_record_spike_active;
static u8 rdx_record_spike_completed;

static void rdx_record_spike_snapshot(const char *reason)
{
#if TCFG_RDX_RESOURCE_MONITOR_ENABLE
    rdx_resource_monitor_snapshot(reason);
#else
    (void)reason;
#endif
}

static int rdx_record_spike_frame(u8 *data, u32 len)
{
    u32 now_ms;
    u32 period_ms = 0;
    u32 frame_index;

    (void)data;
    if (!rdx_record_spike_active) {
        return 0;
    }

    now_ms = jiffies_msec();
    frame_index = ++rdx_record_spike_stats.frame_count;

    if (frame_index == 1) {
        rdx_record_spike_stats.first_frame_ms = now_ms;
        rdx_record_spike_stats.min_frame_len = len;
        rdx_record_spike_stats.max_frame_len = len;
        rdx_record_spike_snapshot("record_first_frame");
    } else {
        period_ms = now_ms - rdx_record_spike_stats.last_frame_ms;
        rdx_record_spike_stats.period_sum_ms += period_ms;
        if (period_ms < rdx_record_spike_stats.min_period_ms) {
            rdx_record_spike_stats.min_period_ms = period_ms;
        }
        if (period_ms > rdx_record_spike_stats.max_period_ms) {
            rdx_record_spike_stats.max_period_ms = period_ms;
        }
        if (len < rdx_record_spike_stats.min_frame_len) {
            rdx_record_spike_stats.min_frame_len = len;
        }
        if (len > rdx_record_spike_stats.max_frame_len) {
            rdx_record_spike_stats.max_frame_len = len;
        }
    }

    if (len != RDX_RECORD_SPIKE_EXPECTED_FRAME_LEN) {
        rdx_record_spike_stats.unexpected_len_count++;
    }
    rdx_record_spike_stats.last_frame_ms = now_ms;

    if (frame_index <= RDX_RECORD_SPIKE_WARMUP_FRAMES) {
        printf("[RDX][SPIKE] warmup frame=%u len=%u at_ms=%u period_ms=%u\n",
               (unsigned int)frame_index, (unsigned int)len,
               (unsigned int)(now_ms - rdx_record_spike_stats.start_ms),
               (unsigned int)period_ms);
    }

    return 0;
}

static void rdx_record_spike_dump_result(void)
{
    u32 period_count = rdx_record_spike_stats.frame_count > 1
                       ? rdx_record_spike_stats.frame_count - 1 : 0;
    u32 average_period_ms = period_count
                            ? rdx_record_spike_stats.period_sum_ms / period_count : 0;
    u32 warmup_frames = rdx_record_spike_stats.frame_count;

    if (warmup_frames > RDX_RECORD_SPIKE_WARMUP_FRAMES) {
        warmup_frames = RDX_RECORD_SPIKE_WARMUP_FRAMES;
    }

    printf("[RDX][SPIKE] result frames=%u warmup=%u usable=%u"
           " len_min=%u len_max=%u len_ne_80=%u"
           " period_avg_ms=%u period_min_ms=%u period_max_ms=%u"
           " first_frame_ms=%u\n",
           (unsigned int)rdx_record_spike_stats.frame_count,
           (unsigned int)warmup_frames,
           (unsigned int)(rdx_record_spike_stats.frame_count - warmup_frames),
           (unsigned int)rdx_record_spike_stats.min_frame_len,
           (unsigned int)rdx_record_spike_stats.max_frame_len,
           (unsigned int)rdx_record_spike_stats.unexpected_len_count,
           (unsigned int)average_period_ms,
           (unsigned int)(period_count ? rdx_record_spike_stats.min_period_ms : 0),
           (unsigned int)(period_count ? rdx_record_spike_stats.max_period_ms : 0),
           (unsigned int)(rdx_record_spike_stats.first_frame_ms
                          ? rdx_record_spike_stats.first_frame_ms
                            - rdx_record_spike_stats.start_ms : 0));
}

static void rdx_record_spike_stop(void *priv)
{
    (void)priv;
    rdx_record_spike_stop_timer = 0;
    if (!rdx_record_spike_active) {
        return;
    }

    rdx_record_spike_active = 0;
    ai_voice_recoder_close();
    rdx_record_spike_snapshot("record_closed");
    rdx_record_spike_dump_result();
    rdx_record_spike_completed = 1;
}

static void rdx_record_spike_start(void *priv)
{
    int ret;

    (void)priv;
    rdx_record_spike_start_timer = 0;
    if (rdx_record_spike_completed || rdx_record_spike_active) {
        return;
    }

    memset(&rdx_record_spike_stats, 0, sizeof(rdx_record_spike_stats));
    rdx_record_spike_stats.start_ms = jiffies_msec();
    rdx_record_spike_stats.min_period_ms = 0xffffffffUL;
    rdx_record_spike_snapshot("record_before_open");

    rdx_record_spike_active = 1;
    ret = ai_voice_recoder_open_with_tx(AUDIO_CODING_OPUS, 0,
                                        rdx_record_spike_frame);
    if (ret) {
        rdx_record_spike_active = 0;
        printf("[RDX][SPIKE] open_failed err=%d\n", ret);
        rdx_record_spike_completed = 1;
        return;
    }

    rdx_record_spike_snapshot("record_opened");
    printf("[RDX][SPIKE] started sr=16000 ch=2 bitrate=32000"
           " frame_ms=20 format=raw duration_ms=%u\n",
           (unsigned int)RDX_RECORD_SPIKE_DURATION_MS);

    rdx_record_spike_stop_timer = sys_timeout_add(NULL,
                                                   rdx_record_spike_stop,
                                                   RDX_RECORD_SPIKE_DURATION_MS);
    if (!rdx_record_spike_stop_timer) {
        printf("[RDX][SPIKE] stop_timer_failed\n");
        rdx_record_spike_stop(NULL);
    }
}

void rdx_record_spike_schedule(void)
{
    if (rdx_record_spike_completed || rdx_record_spike_active ||
        rdx_record_spike_start_timer) {
        return;
    }

    rdx_record_spike_start_timer = sys_timeout_add(NULL,
                                                    rdx_record_spike_start,
                                                    RDX_RECORD_SPIKE_START_DELAY_MS);
    if (!rdx_record_spike_start_timer) {
        printf("[RDX][SPIKE] start_timer_failed\n");
        rdx_record_spike_completed = 1;
        return;
    }

    printf("[RDX][SPIKE] scheduled delay_ms=%u\n",
           (unsigned int)RDX_RECORD_SPIKE_START_DELAY_MS);
}

void rdx_record_spike_cancel(void)
{
    if (rdx_record_spike_start_timer) {
        sys_timeout_del(rdx_record_spike_start_timer);
        rdx_record_spike_start_timer = 0;
    }
    if (rdx_record_spike_stop_timer) {
        sys_timeout_del(rdx_record_spike_stop_timer);
        rdx_record_spike_stop_timer = 0;
    }
    if (rdx_record_spike_active) {
        rdx_record_spike_stop(NULL);
    }
}

#endif /* TCFG_RDX_ENABLE && TCFG_RDX_RECORD_SPIKE_ENABLE */
