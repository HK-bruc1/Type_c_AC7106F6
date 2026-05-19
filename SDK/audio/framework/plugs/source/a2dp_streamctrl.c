#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".a2dp_streamctrl.data.bss")
#pragma data_seg(".a2dp_streamctrl.data")
#pragma const_seg(".a2dp_streamctrl.text.const")
#pragma code_seg(".a2dp_streamctrl.text")
#endif
/*************************************************************************************************/
/*!
*  \file       a2dp_streamctrl.c
*
*  \brief      a2dp plug stream control file.
*
*  Copyright (c) 2011-2022 ZhuHai Jieli Technology Co.,Ltd.
*
*/
/*************************************************************************************************/
#include "btstack/a2dp_media_codec.h"
#include "btstack/avctp_user.h"
#include "sync/audio_syncts.h"
#include "system/timer.h"
#include "media/audio_base.h"
#include "source_node.h"
#include "jiffies.h"
#include "a2dp_streamctrl.h"
#include "app_config.h"
#include "audio_time.h"
#include "classic/tws_api.h"

extern const int CONFIG_A2DP_DELAY_TIME_AAC;
extern const int CONFIG_A2DP_DELAY_TIME_SBC;
extern const int CONFIG_A2DP_ADAPTIVE_MAX_LATENCY;
extern const int CONFIG_A2DP_DELAY_TIME_AAC_LO;
extern const int CONFIG_A2DP_DELAY_TIME_SBC_LO;
extern const int CONFIG_JL_DONGLE_PLAYBACK_LATENCY;
extern const int CONFIG_DONGLE_SPEAK_ENABLE;
extern const int CONFIG_A2DP_MAX_BUF_SIZE;
extern const int CONFIG_A2DP_AAC_MAX_BUF_SIZE;
extern const int CONFIG_A2DP_SBC_MAX_BUF_SIZE;
extern const int CONFIG_A2DP_LHDC_MAX_BUF_SIZE;
extern const int CONFIG_A2DP_LDAC_MAX_BUF_SIZE;
extern const int CONFIG_TWS_PURE_MONITOR_MODE;
#if (defined(TCFG_BT_SUPPORT_LDAC) && TCFG_BT_SUPPORT_LDAC)
_WEAK_ const int CONFIG_A2DP_DELAY_TIME_LDAC = 300;
_WEAK_ const int CONFIG_A2DP_DELAY_TIME_LDAC_LO = 300;
#endif
#if (defined(TCFG_BT_SUPPORT_LHDC) && TCFG_BT_SUPPORT_LHDC)
_WEAK_ const int CONFIG_A2DP_DELAY_TIME_LHDC = 300;
_WEAK_ const int CONFIG_A2DP_DELAY_TIME_LHDC_LO = 300;
#endif
extern const int CONFIG_BTCTLER_TWS_ENABLE;

#define A2DP_ADAPTIVE_DELAY_ENABLE          1
#define ADAPTIVE_PREDICTION_LATENCY_ENABLE  1
#define A2DP_STREAM_INFO_DEBUG_ENABLE       0
#define A2DP_STREAM_INFO_REGULAR_ENABLE     1
#define A2DP_RF_QUALITY_DETECT_ENABLE       1

#define RF_QUALITY_BAD_THRESHOLD            2
#define RF_QUALITY_GOOD_THRESHOLD           3
#define RF_PER_BAD_LEVEL0                   30
#define RF_PER_BAD_LEVEL1                   50
#define RF_PER_BAD_LEVEL2                   70

#define RF_RSSI_BAD_LEVEL0                  -85
#define RF_RSSI_BAD_LEVEL1                  -99


#define A2DP_STREAM_INFO_DEBUG_PREIOD       500

#define A2DP_STREAM_NO_ERR                  0
#define A2DP_STREAM_UNDERRUN                1
#define A2DP_STREAM_RELOAD                  2
#define A2DP_STREAM_MISSED                  3
#define A2DP_STREAM_DECODE_ERR              4
#define A2DP_STREAM_LOW_UNDERRUN            5

#define RB16(b)    (u16)(((u8 *)b)[0] << 8 | (((u8 *)b))[1])
#define bt_time_to_msecs(clk)   (((clk) * 625) / 1000)

#define DELAY_INCREMENT                     150
#define LOW_LATENCY_DELAY_INCREMENT         50

#define DELAY_DECREMENT                     50
#define LOW_LATENCY_DELAY_DECREMENT         10

#define MAX_DELAY_INCREMENT                 150

#define DUAL_CONN_COMPENSATION_LATENCY      15
#define WIFI_COEXIST_COMPENSATION_LATENCY   20

#define QUALCOMM_WIFI_COEXIST_DETECT_EN     0
#define QUALCOMM_WIFI_COEXIST_INTERVAL      65
#define RX_INTERVAL_DETECT_PERIOD           30000


#define LATENCY_ADAPTIVE_MISSED_PACKET      3
#define LATENCY_ADAPTIVE_INIT               2
#define LATENCY_ADAPTIVE_UP                 1
#define LATENCY_ADAPTIVE_DOWN               0

#define UNDERFLOW_RELOAD_LATENCY_MODE       0

#define LOW_LATENCY_ADAPTIVE_MIN_DEPTH          15
#define LOW_LATENCY_ADAPTIVE_MEDIAN_DEPTH       25
#define LOW_LATENCY_OVERRUN_DETECT_PERIOD       30000
#define LOW_LATENCY_UNDERRUN_DETECT_PERIOD      1000
#define LOW_LATENCY_COMPENSATION_VALUE          10
#define ADAPTIVE_MIN_DEPTH                      30
#define ADAPTIVE_MIN_DEPTH_PERCENT              30
#define ADAPTIVE_MEDIAN_DEPTH                   50
#define ADAPTIVE_MEDIAN_DEPTH_PERCENT           50
#define ADAPTIVE_OVERRUN_DETECT_PERIOD          120000
#define ADAPTIVE_UNDERRUN_DETECT_PERIOD         5000
#define ADAPTIVE_COMPENSATION_VALUE             50
#define FILL_MISSED_PACKET_DISCRETE_THRESHOLD   -10

struct a2dp_stream_control {
    u8 plan;
    u8 stream_error;
    u8 first_in;
    u8 low_latency;
    u8 jl_dongle;
    u8 fill_missed_packet;
    u8 repair_frame[2];
    u8 overrun_detect_enable;
    s8 link_rssi;
    u16 timer;
    u16 seqn;
    u16 missed_num;
    s16 initial_latency;
    u16 initial_compensation;
    s16 adaptive_latency;
    s16 adaptive_max_latency;
    s16 gap;
    s16 latency;
    s16 max_capacity;
    u32 overrun_detect_timeout;
    u32 underrun_detect_timeout;
    u32 underrun_time;
    u32 codec_type;
    u32 fill_missed_cooldown_time;
    void *stream;
    u32 next_timestamp;
    void *underrun_signal;
    void (*underrun_callback)(void *);
    struct list_head entry;
    u8 bt_addr[6];
    u8 rf_quality;
#if QUALCOMM_WIFI_COEXIST_DETECT_EN
    u8 rx_detect_count;
    u32 rx_detect_next_period;
#endif

#if A2DP_STREAM_INFO_DEBUG_ENABLE
    int total_underrun_num[2];
    int total_missed_num;
    u32 stream_info_timeout;
    u32 underrun_cooldown_time;
    u32 bit_rate;
    u32 reload_time;
    u32 underflow_time;
#endif
};

struct a2dp_tws_letency_data {
    u8 state;
    u8 initial_compensation;
    s16 adaptive_latency;
    u8 addr[6];
};

static LIST_HEAD(g_a2dp_stream_list);
extern u32 bt_audio_conn_clock_time(void *addr);

#define A2DP_TWS_LATENCY_SYNC \
	((int)((u8 )('A' + '2' + 'D' + 'P') << (3 * 8)) | \
	 (int)(('T' + 'W' + 'S') << (2 * 8)) | \
	 (int)(('L' + 'A' + 'T' + 'E' + 'N' + 'C' + 'Y') << (1 * 8)) | \
     (int)(('S' + 'Y' + 'N' + 'C') << (0 * 8)))

int a2dp_tws_sync_latency(struct a2dp_stream_control *ctrl, u8 state)
{
    if (!CONFIG_BTCTLER_TWS_ENABLE) {
        return 0;
    }
    if (jlstream_event_notify(STREAM_EVENT_GET_A2DP_STATE, (int)ctrl->bt_addr)) {
        /*获取入仓等链路断开状态，该状态不向TWS同步动态延时信息*/
        return 0;
    }
    struct a2dp_tws_letency_data data;

    memcpy(data.addr, ctrl->bt_addr, 6);

    data.adaptive_latency = ctrl->adaptive_latency;
    data.initial_compensation = ctrl->initial_compensation;
    data.state = state;

    int err = tws_api_send_data_to_sibling(&data, sizeof(data), A2DP_TWS_LATENCY_SYNC);
    if (err) {
        printf("tws sync latency error.\n");
    }

    return err;
}

static void a2dp_tws_latency_sync_handler(void *buf, u16 len, bool rx)
{
    if (!rx) {
        return;
    }

    struct a2dp_stream_control *ctrl = NULL;
    struct a2dp_tws_letency_data data;
    memcpy(&data, buf, sizeof(data));
    local_irq_disable();
    list_for_each_entry(ctrl, &g_a2dp_stream_list, entry) {
        if (memcmp(ctrl->bt_addr, data.addr, 6) != 0) {
            continue;
        }
        int overrun_period = ctrl->low_latency ? LOW_LATENCY_OVERRUN_DETECT_PERIOD : ADAPTIVE_OVERRUN_DETECT_PERIOD;
        int underrun_period = ctrl->low_latency ? LOW_LATENCY_UNDERRUN_DETECT_PERIOD : ADAPTIVE_UNDERRUN_DETECT_PERIOD;

        switch (data.state) {
        case LATENCY_ADAPTIVE_INIT:
            if (tws_api_get_role() == TWS_ROLE_MASTER) {
                a2dp_tws_sync_latency(ctrl, LATENCY_ADAPTIVE_INIT);
            } else {
                ctrl->adaptive_latency = data.adaptive_latency;
                ctrl->initial_compensation = data.initial_compensation;
            }
            break;
        case LATENCY_ADAPTIVE_MISSED_PACKET:
            if (ctrl->adaptive_latency < data.adaptive_latency) {
                ctrl->adaptive_latency = data.adaptive_latency;
                if (tws_api_get_role() == TWS_ROLE_MASTER) {
                    ctrl->initial_compensation = data.initial_compensation;
                }
                ctrl->underrun_detect_timeout = jiffies + msecs_to_jiffies(underrun_period);
            }
            break;
        case LATENCY_ADAPTIVE_UP:
            if (ctrl->adaptive_latency < data.adaptive_latency) {
                ctrl->adaptive_latency = data.adaptive_latency;
                ctrl->underrun_detect_timeout = jiffies + msecs_to_jiffies(underrun_period);
            }
            break;
        case LATENCY_ADAPTIVE_DOWN:
            if (ctrl->adaptive_latency > data.adaptive_latency) {
                if (ctrl->overrun_detect_enable) {
                    ctrl->overrun_detect_timeout = jiffies + msecs_to_jiffies(overrun_period);
                }
                ctrl->adaptive_latency = data.adaptive_latency;
            }
            break;
        default:
            break;
        }
    }
    local_irq_enable();
}

#if TCFG_BT_SUPPORT_A2DP
REGISTER_TWS_FUNC_STUB(a2dp_latency_sync) = {
    .func_id = A2DP_TWS_LATENCY_SYNC,
    .func    = a2dp_tws_latency_sync_handler,
};
#endif


/*经典蓝牙RF质量评估*/
static u8 a2dp_rf_quality(struct a2dp_stream_control *ctrl)
{
    s8 quality = 5;
#if A2DP_RF_QUALITY_DETECT_ENABLE
    u8 per = 0;
    s8 link_rssi = 0;
    s8 tws_rssi = 0;

    lmp_get_rssi_end_per_for_edr_address(ctrl->bt_addr, &per, &link_rssi, &tws_rssi);

    if (per == 255) {
        per = 0;
    }

    if (per >= RF_PER_BAD_LEVEL2) {
        quality = 1;
    } else if (per >= RF_PER_BAD_LEVEL1) {
        quality = 2;
    } else if (per >= RF_PER_BAD_LEVEL0) {
        quality = 3;
    }

    if (ctrl->link_rssi <= RF_RSSI_BAD_LEVEL0 && link_rssi <= RF_RSSI_BAD_LEVEL0) {
        if (ctrl->link_rssi <= RF_RSSI_BAD_LEVEL1 && link_rssi <= RF_RSSI_BAD_LEVEL1 && quality > 3) {
            quality = 3;
        }
        quality -= 1;
    }

    ctrl->link_rssi = link_rssi;
    if (CONFIG_BTCTLER_TWS_ENABLE && (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
        if (tws_rssi <= RF_RSSI_BAD_LEVEL0) {
            quality -= 1;
        }
    }
    if (quality < 0) {
        quality = 0;
    }
#endif

    return quality;
}

void a2dp_stream_update_next_timestamp(void *_ctrl, u32 next_timestamp)
{
    struct a2dp_stream_control *ctrl = (struct a2dp_stream_control *)_ctrl;
    if (ctrl) {
        ctrl->next_timestamp = next_timestamp;
    }
}

int a2dp_stream_max_buf_size(u32 codec_type)
{
    int max_buf_size = CONFIG_A2DP_MAX_BUF_SIZE;
    switch (codec_type) {
    case A2DP_CODEC_SBC:
        max_buf_size = CONFIG_A2DP_SBC_MAX_BUF_SIZE;
        break;
    case A2DP_CODEC_MPEG24:
        max_buf_size = CONFIG_A2DP_AAC_MAX_BUF_SIZE;
        break;
#if (defined(TCFG_BT_SUPPORT_LDAC) && TCFG_BT_SUPPORT_LDAC)
    case A2DP_CODEC_LDAC:
        max_buf_size = CONFIG_A2DP_LDAC_MAX_BUF_SIZE;
        break;
#endif
#if (defined(TCFG_BT_SUPPORT_LHDC) && TCFG_BT_SUPPORT_LHDC)
    case A2DP_CODEC_LHDC:
        max_buf_size = CONFIG_A2DP_LHDC_MAX_BUF_SIZE;
        break;
#endif
#if (defined(TCFG_BT_SUPPORT_LHDC_V5) && TCFG_BT_SUPPORT_LHDC_V5)
    case A2DP_CODEC_LHDC_V5:
        max_buf_size = CONFIG_A2DP_LHDC_MAX_BUF_SIZE;
        break;
#endif
    }

    return max_buf_size;
}

void a2dp_stream_bandwidth_detect_handler(void *_ctrl, int frame_len, int pcm_frames, int sample_rate)
{
    struct a2dp_stream_control *ctrl = (struct a2dp_stream_control *)_ctrl;
    int max_latency = 0;
    if (!ctrl) {
        return;
    }

    if (ctrl->low_latency) {
        return;
    }

    if (frame_len) {
        int max_buf_size = a2dp_stream_max_buf_size(ctrl->codec_type);
        max_latency = (max_buf_size * pcm_frames / frame_len) * 1000 / sample_rate * 8 / 10;
#if A2DP_STREAM_INFO_DEBUG_ENABLE
        ctrl->bit_rate = frame_len * 10000 / (pcm_frames * 10000 / sample_rate) * 8;
#endif
        if (max_latency < ctrl->max_capacity) {
            ctrl->max_capacity = max_latency;
        }
    }

    if (!max_latency) {
        return;
    }

    if (max_latency < ctrl->adaptive_max_latency) {
        ctrl->adaptive_max_latency = max_latency;
    }

    if (ctrl->adaptive_latency > ctrl->adaptive_max_latency) {
        ctrl->adaptive_latency = ctrl->adaptive_max_latency;
        a2dp_media_update_delay_report_time(ctrl->stream, ctrl->adaptive_latency);
    }
}

static void a2dp_stream_underrun_adaptive_handler(struct a2dp_stream_control *ctrl)
{
#if A2DP_ADAPTIVE_DELAY_ENABLE
    int adaptive_latency = ctrl->adaptive_latency;

    if (!ctrl->low_latency) {
        ctrl->adaptive_latency = ctrl->adaptive_max_latency;
    } else {
        if (time_before(jiffies, ctrl->underrun_detect_timeout)) {
            return;
        }
        ctrl->adaptive_latency += LOW_LATENCY_DELAY_INCREMENT;

        if (ctrl->adaptive_latency > ctrl->adaptive_max_latency) {
            ctrl->adaptive_latency = ctrl->adaptive_max_latency;
        }
    }

    if (adaptive_latency != ctrl->adaptive_latency) {
        a2dp_media_update_delay_report_time(ctrl->stream, ctrl->adaptive_latency);
        a2dp_tws_sync_latency(ctrl, adaptive_latency < ctrl->adaptive_latency ? LATENCY_ADAPTIVE_UP : LATENCY_ADAPTIVE_DOWN);
    }
    /*printf("---underrun, adaptive : %dms---\n", ctrl->adaptive_latency);*/
#endif
}

static void a2dp_stream_missed_adaptive_handler(struct a2dp_stream_control *ctrl)
{
    if (CONFIG_TWS_PURE_MONITOR_MODE == 1 || CONFIG_AES_CCM_FOR_EDR_ENABLE != 0) {
        return;
    }

    if (time_before(jiffies, ctrl->underrun_detect_timeout)) {
        return;
    }

    if (!ctrl->low_latency) {
        int adaptive_latency = ctrl->adaptive_latency;
        local_irq_disable();
        if (tws_api_get_role() == TWS_ROLE_MASTER) {
            ctrl->adaptive_latency = ctrl->adaptive_max_latency;
        } else {
            /*监听+转发方案针对从机丢包进行预测补偿*/
            ctrl->adaptive_latency += ADAPTIVE_COMPENSATION_VALUE;
            if (ctrl->adaptive_latency > ctrl->adaptive_max_latency) {
                ctrl->adaptive_latency = ctrl->adaptive_max_latency;
            }
            ctrl->initial_compensation = ADAPTIVE_COMPENSATION_VALUE;
        }
        ctrl->underrun_detect_timeout = jiffies + msecs_to_jiffies(ADAPTIVE_UNDERRUN_DETECT_PERIOD);
        if (adaptive_latency != ctrl->adaptive_latency) {
            a2dp_media_update_delay_report_time(ctrl->stream, ctrl->adaptive_latency);
            a2dp_tws_sync_latency(ctrl, LATENCY_ADAPTIVE_MISSED_PACKET);
        }
        local_irq_enable();
    }
}

static int a2dp_stream_fill_missed_packet_enable(struct a2dp_stream_control *ctrl)
{
    int discrete = ctrl->adaptive_max_latency - ctrl->latency;
    if (!ctrl->fill_missed_packet) {
        if (discrete >= FILL_MISSED_PACKET_DISCRETE_THRESHOLD) {
            if (time_after(jiffies, ctrl->fill_missed_cooldown_time)) {
                ctrl->fill_missed_packet = 1;
            }
        } else {
            ctrl->fill_missed_cooldown_time = jiffies + msecs_to_jiffies(5000);
        }
    } else {
        if (discrete <= FILL_MISSED_PACKET_DISCRETE_THRESHOLD) {
            ctrl->fill_missed_packet = 0;
            ctrl->fill_missed_cooldown_time = jiffies + msecs_to_jiffies(5000);
        }
    }
    return 0;
}

static int a2dp_stream_adaptive_overrun_restart(struct a2dp_stream_control *ctrl)
{

    if (ctrl->overrun_detect_enable) {
        return 0;
    }

    int min_latency = ctrl->initial_latency + ctrl->initial_compensation;
    int overrun_period = ctrl->low_latency ? LOW_LATENCY_OVERRUN_DETECT_PERIOD : ADAPTIVE_OVERRUN_DETECT_PERIOD;

    if (ctrl->adaptive_latency > min_latency) {
        ctrl->overrun_detect_enable = 1;
        ctrl->overrun_detect_timeout = jiffies + msecs_to_jiffies(overrun_period);
        ctrl->gap = ctrl->adaptive_latency;
        r_printf("adaptive start overrun detect.\n");
        return 1;
    }

    return 0;
}

static int a2dp_stream_adaptive_overrun_detect(struct a2dp_stream_control *ctrl)
{
    int min_latency = ctrl->initial_latency + ctrl->initial_compensation;
    int overrun_period = ctrl->low_latency ? LOW_LATENCY_OVERRUN_DETECT_PERIOD : ADAPTIVE_OVERRUN_DETECT_PERIOD;
    int min_depth_value = ctrl->low_latency ? LOW_LATENCY_ADAPTIVE_MIN_DEPTH : (ctrl->adaptive_latency * ADAPTIVE_MIN_DEPTH_PERCENT / 100);

    if (time_after(jiffies, ctrl->overrun_detect_timeout)) {
        int discrete = ctrl->adaptive_latency - ctrl->gap;
        int down_value = 0;
        if (discrete >= min_latency) { /*离散最大值超过基础延时，谨慎回调*/
            down_value = 0;
        } else if (discrete >= min_latency / 2) {
            down_value = (ctrl->adaptive_latency - min_latency) / 4;
        } else {
            if (ctrl->adaptive_latency - min_latency < min_depth_value) {
                down_value = ctrl->adaptive_latency - min_latency;
            } else {
                down_value = (ctrl->adaptive_latency - min_latency) / 2;
            }
        }
        r_printf("adaptive down : %d, %d, %d, %d, %d\n", ctrl->adaptive_latency, discrete, down_value, ctrl->adaptive_latency - down_value, min_latency);
        ctrl->adaptive_latency -= down_value;
        if (ctrl->adaptive_latency < min_latency) {
            ctrl->adaptive_latency = min_latency;
            ctrl->overrun_detect_enable = 0;
            r_printf("adaptive back to initial latency, stop overrun detect.\n");
        } else {
            ctrl->gap = ctrl->adaptive_latency;
            ctrl->overrun_detect_timeout = jiffies + msecs_to_jiffies(overrun_period);
        }
    }

    return 0;
}

static int a2dp_stream_adaptive_underrun_prediect(struct a2dp_stream_control *ctrl, u8 rf_quality, int discrete)
{
    int min_latency = ctrl->initial_latency + ctrl->initial_compensation;
    int compensation = ctrl->low_latency ? LOW_LATENCY_COMPENSATION_VALUE : ADAPTIVE_COMPENSATION_VALUE;
    int underrun_period = ctrl->low_latency ? LOW_LATENCY_UNDERRUN_DETECT_PERIOD : ADAPTIVE_UNDERRUN_DETECT_PERIOD;
    int min_depth_value = ctrl->low_latency ? LOW_LATENCY_ADAPTIVE_MIN_DEPTH : (ctrl->adaptive_latency * ADAPTIVE_MIN_DEPTH_PERCENT / 100);

    if (discrete >= min_latency) {
        ctrl->adaptive_latency = ctrl->adaptive_max_latency;
    } else if (rf_quality < RF_QUALITY_BAD_THRESHOLD) {
        u16 rf_compensation = (ctrl->adaptive_max_latency - min_latency) / (rf_quality + 1);
        u16 new_latency = min_latency + rf_compensation;
        if (ctrl->adaptive_latency < new_latency) {
            printf("rf quality %d, up : %d, %d\n", rf_quality, ctrl->adaptive_latency, new_latency);
            ctrl->adaptive_latency = new_latency;
            if (ctrl->adaptive_latency > ctrl->adaptive_max_latency) {
                ctrl->adaptive_latency = ctrl->adaptive_max_latency;
            }
        }
    } else if (ctrl->latency < min_depth_value) {
        if (time_before(jiffies, ctrl->underrun_detect_timeout)) {
            return -EAGAIN;
        }
        ctrl->adaptive_latency += ((min_depth_value - ctrl->latency) + compensation);
        ctrl->underrun_detect_timeout = jiffies + msecs_to_jiffies(underrun_period);
        printf("adaptive latency up : %d, %d\n", ctrl->adaptive_latency, (ctrl->latency + compensation));
        if (ctrl->adaptive_latency > ctrl->adaptive_max_latency) {
            ctrl->adaptive_latency = ctrl->adaptive_max_latency;
        }
    }

    return 0;
}

/*
 * 自适应延时策略
 */
static void a2dp_stream_adaptive_detect_handler(struct a2dp_stream_control *ctrl, u8 new_frame, u32 frame_time)
{
#if A2DP_ADAPTIVE_DELAY_ENABLE
    if (ctrl->stream_error || !new_frame || !ctrl->next_timestamp) {
        return;
    }

    u32 rx_timestamp = frame_time * 625 * TIMESTAMP_US_DENOMINATOR;
    u32 play_timestamp = ctrl->next_timestamp;
    int gap = play_timestamp - rx_timestamp; /*正常播放情况下当前帧的播放时间一定大于收包点时间*/
    gap /= TIMESTAMP_US_DENOMINATOR; /*us*/
    gap /= 1000; /*ms*/

    int median_depth_value = ctrl->low_latency ? LOW_LATENCY_ADAPTIVE_MEDIAN_DEPTH : (ctrl->adaptive_latency * ADAPTIVE_MEDIAN_DEPTH_PERCENT / 100);

    if (gap < 0) {
        gap = 0;
    }
    ctrl->latency = gap;
    int discrete = ctrl->adaptive_latency - gap;
    local_irq_disable();
    int adaptive_latency = ctrl->adaptive_latency;
    a2dp_stream_fill_missed_packet_enable(ctrl);

    u8 rf_quality = a2dp_rf_quality(ctrl);
    if (rf_quality < RF_QUALITY_GOOD_THRESHOLD || gap < median_depth_value) {
        if (ctrl->overrun_detect_enable) {
            ctrl->overrun_detect_enable = 0;
            r_printf("adaptive stop overrun detect : %d, %d, %d.\n", rf_quality, gap, median_depth_value);
        }
    } else {
        if (a2dp_stream_adaptive_overrun_restart(ctrl)) {
            local_irq_enable();
            return;
        }

        if (ctrl->gap > gap) {
            ctrl->gap = gap;
        }

        a2dp_stream_adaptive_overrun_detect(ctrl);

        goto exit;
    }

    a2dp_stream_adaptive_underrun_prediect(ctrl, rf_quality, discrete);

exit:
    if (ctrl->adaptive_latency != adaptive_latency) {
        a2dp_media_update_delay_report_time(ctrl->stream, ctrl->adaptive_latency);
        a2dp_tws_sync_latency(ctrl, adaptive_latency < ctrl->adaptive_latency ? LATENCY_ADAPTIVE_UP : LATENCY_ADAPTIVE_DOWN);
    }
    local_irq_enable();
#endif
}

static void a2dp_stream_rx_interval_detect(struct a2dp_stream_control *ctrl, struct a2dp_media_frame *frame)
{
#if QUALCOMM_WIFI_COEXIST_DETECT_EN
    if (!ctrl->low_latency || !ctrl->frame.packet) {
        return;
    }

    int interval = (frame->clkn * 625) - (ctrl->frame.clkn * 625);
    interval /= 1000;

    if (interval >= (QUALCOMM_WIFI_COEXIST_INTERVAL - WIFI_COEXIST_COMPENSATION_LATENCY - 1) ||
        interval >= (ctrl->initial_latency - 6)) {
        ctrl->rx_detect_count++;
    }

    if (ctrl->rx_detect_count >= 3 && time_before(jiffies, ctrl->rx_detect_next_period)) {
        ctrl->rx_detect_count = 0;
        ctrl->rx_detect_next_period = jiffies + msecs_to_jiffies(RX_INTERVAL_DETECT_PERIOD);
        if (ctrl->initial_latency + ctrl->initial_compensation < QUALCOMM_WIFI_COEXIST_INTERVAL + WIFI_COEXIST_COMPENSATION_LATENCY) {
            ctrl->initial_compensation = QUALCOMM_WIFI_COEXIST_INTERVAL + WIFI_COEXIST_COMPENSATION_LATENCY - ctrl->initial_latency;
            local_irq_disable();
            if (ctrl->adaptive_latency < ctrl->initial_latency + ctrl->initial_compensation) {
                ctrl->adaptive_latency = ctrl->initial_latency + ctrl->initial_compensation;
                a2dp_tws_sync_latency(ctrl, LATENCY_ADAPTIVE_UP);
            }

            if (tws_api_get_role() == TWS_ROLE_MASTER) {
                a2dp_tws_sync_latency(ctrl, LATENCY_ADAPTIVE_INIT);
            }
            local_irq_enable();
        }
    } else if (time_after(jiffies, ctrl->rx_detect_next_period)) {
        ctrl->rx_detect_count = 0;
        ctrl->rx_detect_next_period = jiffies + msecs_to_jiffies(RX_INTERVAL_DETECT_PERIOD);
    }
#endif
}

void *a2dp_stream_control_plan_select(void *stream, int low_latency, u32 codec_type, u8 plan, u8 *bt_addr)
{
    struct a2dp_stream_control *ctrl = (struct a2dp_stream_control *)zalloc(sizeof(struct a2dp_stream_control));

    if (!ctrl) {
        return NULL;
    }

    switch (plan) {
    case A2DP_STREAM_JL_DONGLE_CONTROL:
        if (CONFIG_DONGLE_SPEAK_ENABLE) {
            ctrl->initial_latency = CONFIG_JL_DONGLE_PLAYBACK_LATENCY;
            ctrl->adaptive_latency = ctrl->initial_latency;
            ctrl->adaptive_max_latency = low_latency ? (ctrl->initial_latency + MAX_DELAY_INCREMENT) : CONFIG_A2DP_ADAPTIVE_MAX_LATENCY;
            ctrl->jl_dongle = 1;
        }
        break;
    default:
        if (low_latency) {
            if (codec_type == A2DP_CODEC_MPEG24) {
                ctrl->initial_latency = CONFIG_A2DP_DELAY_TIME_AAC_LO;
#if (defined(TCFG_BT_SUPPORT_LDAC) && TCFG_BT_SUPPORT_LDAC)
            } else if (codec_type == A2DP_CODEC_LDAC) {
                ctrl->initial_latency = CONFIG_A2DP_DELAY_TIME_LDAC_LO;
#endif
#if (defined(TCFG_BT_SUPPORT_LHDC) && TCFG_BT_SUPPORT_LHDC)
            } else if (codec_type == A2DP_CODEC_LHDC) {
                ctrl->initial_latency = CONFIG_A2DP_DELAY_TIME_LHDC_LO;
#endif
            } else {
                ctrl->initial_latency = CONFIG_A2DP_DELAY_TIME_SBC_LO;
            }
        } else {
            if (codec_type == A2DP_CODEC_MPEG24) {
                ctrl->initial_latency = CONFIG_A2DP_DELAY_TIME_AAC;
#if (defined(TCFG_BT_SUPPORT_LDAC) && TCFG_BT_SUPPORT_LDAC)
            } else if (codec_type == A2DP_CODEC_LDAC) {
                ctrl->initial_latency = CONFIG_A2DP_DELAY_TIME_LDAC;
#endif
#if (defined(TCFG_BT_SUPPORT_LHDC) && TCFG_BT_SUPPORT_LHDC)
            } else if (codec_type == A2DP_CODEC_LHDC) {
                ctrl->initial_latency = CONFIG_A2DP_DELAY_TIME_LHDC;
#endif
            } else {
                ctrl->initial_latency = CONFIG_A2DP_DELAY_TIME_SBC;
            }
        }
        ctrl->adaptive_max_latency = low_latency ? (ctrl->initial_latency + MAX_DELAY_INCREMENT) : CONFIG_A2DP_ADAPTIVE_MAX_LATENCY;
        ctrl->initial_compensation = 0;//bt_get_total_connect_dev() > 1 ? DUAL_CONN_COMPENSATION_LATENCY : 0;
        ctrl->adaptive_latency = ctrl->initial_latency + ctrl->initial_compensation;
        break;
    }

    ctrl->repair_frame[0] = 0x02;
    ctrl->repair_frame[1] = 0x00;
    ctrl->max_capacity = 2 * ctrl->adaptive_max_latency;
    memcpy(ctrl->bt_addr, bt_addr, 6);
    ctrl->rf_quality = 5;
    local_irq_disable();
    list_add(&ctrl->entry, &g_a2dp_stream_list);
    local_irq_enable();
    a2dp_tws_sync_latency(ctrl, LATENCY_ADAPTIVE_INIT);
    ctrl->low_latency = low_latency;
    ctrl->first_in = 1;
    ctrl->codec_type = codec_type;
    ctrl->plan = plan;
    ctrl->stream = stream;
    a2dp_media_update_delay_report_time(ctrl->stream, ctrl->adaptive_latency);
    return ctrl;
}

static void a2dp_stream_underrun_signal(void *arg)
{
    struct a2dp_stream_control *ctrl;// = (struct a2dp_stream_control *)arg;

    local_irq_disable();
    list_for_each_entry(ctrl, &g_a2dp_stream_list, entry) {
        if ((u32)ctrl != (u32)arg) {
            continue;
        }

        if (ctrl->underrun_callback) {
            ctrl->underrun_callback(ctrl->underrun_signal);
        }
        ctrl->timer = 0;
    }
    local_irq_enable();
}

static int a2dp_audio_is_underrun(struct a2dp_stream_control *ctrl)
{
    int underrun_time = ctrl->low_latency ? 6 : 20;
    if (ctrl->next_timestamp) {
        u32 reference_clock = bt_audio_conn_clock_time(ctrl->bt_addr);
        if (reference_clock == (u32) - 1) {
            return true;
        }
        u32 reference_time = reference_clock * 625 * TIMESTAMP_US_DENOMINATOR;
        int distance_time = ctrl->next_timestamp - reference_time;
        if (distance_time > 67108863L || distance_time < -67108863L) {
            if (ctrl->next_timestamp > reference_time) {
                distance_time = ctrl->next_timestamp - 0xffffffff - reference_time;
            } else {
                distance_time = 0xffffffff - reference_time + ctrl->next_timestamp;
            }
        }
        distance_time = distance_time / 1000 / TIMESTAMP_US_DENOMINATOR;
        /*printf("distance_time %d %u %u\n", distance_time, ctrl->next_timestamp, reference_time);*/
        if (distance_time <= underrun_time) { //判断是否已经超时，如果已经超时，返回欠载(临界时，也返回欠载)，
            return true;                      //如果没有欠载，下面设置自检timer,
        }

        local_irq_disable();
        if (ctrl->timer) {
            sys_hi_timeout_del(ctrl->timer);
            ctrl->timer = 0;
        }
        ctrl->timer = sys_hi_timeout_add(ctrl, a2dp_stream_underrun_signal, distance_time - underrun_time);
        local_irq_enable();
    }

    return 0;
}

static int a2dp_stream_underrun_handler(struct a2dp_stream_control *ctrl, struct a2dp_media_frame *frame)
{
    if (!a2dp_audio_is_underrun(ctrl)) {
        /*putchar('x');*/
        return 0;
    }
    putchar('X');

    if (ctrl->stream_error != A2DP_STREAM_UNDERRUN) {
#if A2DP_STREAM_INFO_DEBUG_ENABLE
        ctrl->total_underrun_num[0]++;
#endif
        ctrl->underrun_time = jiffies;
        ctrl->stream_error = A2DP_STREAM_UNDERRUN;
        a2dp_stream_underrun_adaptive_handler(ctrl);
    }
    frame->packet = ctrl->repair_frame;

    return sizeof(ctrl->repair_frame);
}

static int a2dp_stream_reload_latency_handler(struct a2dp_stream_control *ctrl, struct a2dp_media_frame *frame, int *len)
{
#if UNDERFLOW_RELOAD_LATENCY_MODE == 1
    int underrun_time = jiffies_to_msecs(jiffies) - jiffies_to_msecs(ctrl->underrun_time);
#else
    int underrun_time = 0;
#endif
    while (1) {
        int msecs = a2dp_media_get_remain_play_time(ctrl->stream, 1);
        if (msecs < ctrl->adaptive_latency - underrun_time) {
            /*printf("adaptive latency %d, msecs %d\n", ctrl->adaptive_latency, msecs);*/
            break;
        }

        int rlen = a2dp_media_try_get_packet(ctrl->stream, frame);
        if (rlen <= 0) {
            break;
        }
        *len = rlen;
        putchar('n');
#if A2DP_STREAM_INFO_DEBUG_ENABLE
        ctrl->reload_time = jiffies_to_msecs(jiffies) - jiffies_to_msecs(ctrl->underrun_time);
        printf("total reload time : %dms\n", ctrl->reload_time);
        ctrl->underrun_cooldown_time = jiffies + msecs_to_jiffies(1500);
#endif
        ctrl->fill_missed_packet = 0; /*卡顿后重新加载不对丢包数据进行补包*/
        ctrl->fill_missed_cooldown_time = jiffies + msecs_to_jiffies(5000);
        return 1;
    }

    *len = sizeof(ctrl->repair_frame);
    frame->packet = ctrl->repair_frame;
    putchar('o');
    return 0;
}

static int a2dp_stream_missed_handler(struct a2dp_stream_control *ctrl, struct a2dp_media_frame *frame, int *len)
{
    /*printf("missed num : %d\n", ctrl->missed_num);*/
    if (--ctrl->missed_num == 0) {
        return 1;
    }

    *len = sizeof(ctrl->repair_frame);
    frame->packet = ctrl->repair_frame;
    return 0;
}

static int a2dp_stream_error_filter(struct a2dp_stream_control *ctrl, struct a2dp_media_frame *frame, int len)
{
    int err = 0;
    u8 repair = 0;

    if (len == 2) {
        repair = 1;
    }

    if (!repair) {
        int header_len = a2dp_media_get_rtp_header_len(ctrl->codec_type, frame->packet, len);
        if (header_len >= len) {
            printf("##A2DP header error : %d\n", header_len);
            a2dp_media_free_packet(ctrl->stream, frame->packet);
            return -EFAULT;
        }
    }

    u16 seqn = repair ? ctrl->seqn : RB16(frame->packet + 2);
    if (ctrl->first_in) {
        ctrl->first_in = 0;
        goto __exit;
    }
    if (seqn != ctrl->seqn) {
        if (ctrl->stream_error == A2DP_STREAM_UNDERRUN) {
#if A2DP_STREAM_INFO_DEBUG_ENABLE
            ctrl->underflow_time = jiffies_to_msecs(jiffies) - jiffies_to_msecs(ctrl->underrun_time);
#endif
            ctrl->stream_error = A2DP_STREAM_RELOAD;
            err = -EAGAIN;
        } else if (!ctrl->stream_error && (u16)(seqn - ctrl->seqn) > 1) {
            err = -EAGAIN;
            u16 missed_num = seqn - ctrl->seqn - 1;
            if (missed_num > 32768) {
                missed_num = 2;
            }

#if A2DP_STREAM_INFO_DEBUG_ENABLE
            ctrl->total_missed_num += (missed_num - 1);
            if (time_after(jiffies, ctrl->underrun_cooldown_time)) {
                ctrl->total_underrun_num[1]++;
            }
#endif
            if (missed_num > 10) {
                missed_num = 10;
            }
            a2dp_stream_missed_adaptive_handler(ctrl);
            if (ctrl->fill_missed_packet) { /*补包仅在收包稳定情况下丢包后补包*/
                ctrl->missed_num = missed_num + 1;
            } else {
                ctrl->missed_num = 1;
            }
            ctrl->stream_error = A2DP_STREAM_MISSED;
#if A2DP_STREAM_INFO_DEBUG_ENABLE
            printf("miss=[%d %d %d] seqn:%d->%d\n", missed_num, a2dp_media_get_packet_num(ctrl->stream), ctrl->max_capacity, ctrl->seqn, seqn);
#endif
        }
    }

__exit:
    ctrl->seqn = seqn;
    return err;
}

static void a2dp_stream_info_print(struct a2dp_stream_control *ctrl)
{
#if A2DP_STREAM_INFO_DEBUG_ENABLE
    if (time_after(jiffies, ctrl->stream_info_timeout)) {
        u8 per = 0;
        s8 link_rssi = 0;
        s8 tws_rssi = 0;

        lmp_get_rssi_end_per_for_edr_address(ctrl->bt_addr, &per, &link_rssi, &tws_rssi);
        if (per == 255) {
            per = 0;
        }
        /*监控变量分为4个可选类型：延时跟踪、蓝牙信号/收包信息、实时码率、卡顿次数
         * 1、latency=[latency target max_latency]
         * 2、bt=[frame_num PER RSSI TWS_RSSI]
         * 3、bitrate=[]
         * 4、error=[underrun_num missed_underrun_num missed_num]
         */
        printf("latency=[%d %d %d] bt=[%d %d %d %d] bitrate=[%d] error=[%d %d %d]\n",
               ctrl->latency, ctrl->adaptive_latency, ctrl->adaptive_max_latency,
               a2dp_media_get_packet_num(ctrl->stream), per, link_rssi, tws_rssi, ctrl->bit_rate / 1000,
               ctrl->total_underrun_num[0], ctrl->total_underrun_num[1], ctrl->total_missed_num);
        ctrl->stream_info_timeout = jiffies + msecs_to_jiffies(A2DP_STREAM_INFO_DEBUG_PREIOD);
    }
#endif
}

int a2dp_stream_control_pull_frame(void *_ctrl, struct a2dp_media_frame *frame, int *len)
{
    struct a2dp_stream_control *ctrl = (struct a2dp_stream_control *)_ctrl;

    if (!ctrl) {
        *len = 0;
        return 0;
    }

    int rlen = 0;
    int err = 0;
    u8 new_packet = 0;

try_again:
    switch (ctrl->stream_error) {
    case A2DP_STREAM_RELOAD:
        new_packet = a2dp_stream_reload_latency_handler(ctrl, frame, len);
        break;
    case A2DP_STREAM_MISSED:
        new_packet = a2dp_stream_missed_handler(ctrl, frame, len);
        if (!new_packet) {
            break;
        }
    //注意：这里不break是因为很有可能由于位流的错误导致补包无法再正常补上
    default:
        rlen = a2dp_media_try_get_packet(ctrl->stream, frame);
        if (rlen <= 0) {
            rlen = a2dp_stream_underrun_handler(ctrl, frame);
        } else {
            /*a2dp_stream_rx_interval_detect(ctrl, frame); [>for Qualcomm wifi&bluetooth coexist<]*/
            new_packet = 1;
        }
        *len = rlen;
        break;
    }

    if (*len <= 0) {
        return 0;
    }
    err = a2dp_stream_error_filter(ctrl, frame, *len);
    if (err) {
        if (-err == EAGAIN) {
            a2dp_media_put_packet(ctrl->stream, frame->packet);
            goto try_again;
        }
        *len = 0;
        return 0;
    }

    a2dp_stream_adaptive_detect_handler(ctrl, new_packet, frame->clkn);
#if A2DP_STREAM_INFO_DEBUG_ENABLE
    a2dp_stream_info_print(ctrl);
#endif

    if (ctrl->stream_error) {
        if (new_packet) {
            ctrl->stream_error = 0;
            return FRAME_FLAG_RESET_TIMESTAMP_BIT;
        }
        return FRAME_FLAG_FILL_PACKET;
    }

    return 0;
}

void a2dp_stream_control_free_frame(void *_ctrl, struct a2dp_media_frame *frame)
{
    struct a2dp_stream_control *ctrl = (struct a2dp_stream_control *)_ctrl;

    if ((u8 *)frame->packet != (u8 *)ctrl->repair_frame) {
        a2dp_media_free_packet(ctrl->stream, frame->packet);
    }
}

void a2dp_stream_control_set_underrun_callback(void *_ctrl, void *priv, void (*callback)(void *priv))
{
    struct a2dp_stream_control *ctrl = (struct a2dp_stream_control *)_ctrl;
    local_irq_disable();
    ctrl->underrun_signal = priv;
    ctrl->underrun_callback = callback;
    local_irq_enable();
}

int a2dp_stream_control_delay_time(void *_ctrl)
{
    struct a2dp_stream_control *ctrl = (struct a2dp_stream_control *)_ctrl;

    if (ctrl) {
        return ctrl->adaptive_latency;
    }

    return CONFIG_A2DP_DELAY_TIME_AAC;
}

void a2dp_stream_control_free(void *_ctrl)
{
    struct a2dp_stream_control *ctrl = (struct a2dp_stream_control *)_ctrl;

    if (!ctrl) {
        return;
    }

    local_irq_disable();
    list_del(&ctrl->entry);
    if (ctrl->timer) {
        sys_hi_timeout_del(ctrl->timer);
        ctrl->timer = 0;
    }
    local_irq_enable();
#if A2DP_STREAM_INFO_DEBUG_ENABLE
    printf("a2dp_total_error=[%d %d %d]\n",
           ctrl->total_underrun_num[0], ctrl->total_underrun_num[1], ctrl->total_missed_num);
#endif
    free(ctrl);
}
