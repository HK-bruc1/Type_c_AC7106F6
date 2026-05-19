#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".app_le_auracast.data.bss")
#pragma data_seg(".app_le_auracast.data")
#pragma const_seg(".app_le_auracast.text.const")
#pragma code_seg(".app_le_auracast.text")
#endif
#include "system/includes.h"
#include "app_config.h"
#include "app_main.h"
#include "audio_config.h"
#include "a2dp_player.h"
#include "esco_player.h"
#include "btstack/le/auracast_sink_api.h"
#include "le_audio_player.h"
#include "app_le_auracast.h"
#include "a2dp_media_codec.h"
#include "le/le_user.h"
#include "user_cfg.h"
#if TCFG_USER_TWS_ENABLE
#include "classic/tws_api.h"
#endif
#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)))
#include "app_le_connected.h"
#endif
#include "rcsp_auracast.h"

#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_AURACAST_SINK_EN)

/**************************************************************************************************
  Macros
**************************************************************************************************/
#define LOG_TAG             "[APP_LE_AURACAST]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_MONO_L) || (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_MONO_R)
#define AURACAST_SINK_BIS_NUMS               (1)			// 单声道
#else
#define AURACAST_SINK_BIS_NUMS               (2)			// 立体声
#endif

struct broadcast_rx_audio_hdl {
    void *le_audio;
    void *rx_stream;
};
typedef struct {
    u16 bis_hdl;
} bis_hdl_info_t ;

struct auracast_audio {
    u8 bis_num;
    /* u8 status;									// enum APP_AURACAST_STATUS */
    u32 presentation_delay_us;
    struct broadcast_rx_audio_hdl rx_player;
    bis_hdl_info_t audio_hdl[AURACAST_SINK_BIS_NUMS];
};
struct auracast_audio g_rx_audio = {0};
static u16 auracast_sink_sync_timeout_hdl = 0;
static auracast_sink_source_info_t *cur_listening_source_info = NULL;						// 当前正在监听广播设备播歌的信息
static auracast_sink_source_info_t *recover_listening_source_info = NULL;						// 当前正在监听广播设备播歌的信息
static u8 g_sink_bn = 0;
static u16 multi_bis_rx_temp_buf_len = 0;
static u8 *multi_bis_rx_buf[7];
static u16 multi_bis_data_offect[7];
static bool multi_bis_plc_flag[7];
static DEFINE_SPINLOCK(auracast_lock);

#if TCFG_USER_TWS_ENABLE
static u8 g_cur_auracast_is_scanning = 0;											// 当前设备是否正在扫描广播设备，0:否，非零:是
#endif

static unsigned char errpacket[2] = {
    0x02, 0x00
};

u8 le_auracast_is_running()
{
    auracast_sink_big_state_t big_state = auracast_sink_big_state_get();
    printf("le_auracast_state=%d\n", big_state);
    if (big_state > SINK_BIG_STATE_IDLE) {
        return 1;
    }
    return 0;
}

static int app_auracast_app_notify_listening_status(u8 status, u8 error)
{
#if (defined(RCSP_ADV_AURCAST_SINK) && RCSP_ADV_AURCAST_SINK)
    return auracast_app_notify_listening_status(status, error);
#else
    return 0;
#endif
}

/**
 * @brief 关闭auracast功能（音频、扫描）
 *
 * @param need_recover 需要恢复：0:不恢复; 1:恢复
 */
void le_auracast_stop(u8 need_recover)
{
    printf("le_auracast_stop:%d\n", need_recover);
    int ret = app_auracast_sink_big_sync_terminate(need_recover);
    if (ret == 0) {
        app_auracast_app_notify_listening_status(AURACAST_SINK_SYNC_STATE_IDLE, AURACAST_SINK_SYNC_NO_ERROR);
    }
}

static void le_auracast_audio_open(uint8_t *packet, uint16_t length)
{
    int err = 0;
    auracast_sink_source_info_t *config = (auracast_sink_source_info_t *)packet;
    ASSERT(config, "config is NULL");
    printf("le_auracast_audio_open\n");
    //audio open
    if (config->Num_BIS > AURACAST_SINK_BIS_NUMS) {
        g_rx_audio.bis_num = AURACAST_SINK_BIS_NUMS;
    } else {
        g_rx_audio.bis_num = config->Num_BIS;
    }
    for (u8 i = 0; i < g_rx_audio.bis_num; i++) {
        g_rx_audio.audio_hdl[i].bis_hdl = config->Connection_Handle[i];
        printf("i:%d, bis_hdl:%d\n", i, g_rx_audio.audio_hdl[i].bis_hdl);
    }
    printf("bis_num:%d, presentation_delay_us:%d\n", g_rx_audio.bis_num, config->presentation_delay_us);
    g_rx_audio.presentation_delay_us = config->presentation_delay_us;
    if (g_rx_audio.presentation_delay_us < TCFG_LE_AUDIO_PLAY_LATENCY) {
        g_rx_audio.presentation_delay_us = TCFG_LE_AUDIO_PLAY_LATENCY;
    }
    struct le_audio_stream_params params = {0};
    if (config->Num_BIS > AURACAST_SINK_BIS_NUMS) {
        params.fmt.nch = AURACAST_SINK_BIS_NUMS;
    } else {
        params.fmt.nch = config->Num_BIS;
    }
    /* params.fmt.nch = LE_AUDIO_CODEC_CHANNEL; */
    params.fmt.coding_type = LE_AUDIO_CODEC_TYPE;
    if (config->frame_duration == FRAME_DURATION_7_5) {
        params.fmt.frame_dms = 75;
    } else if (config->frame_duration == FRAME_DURATION_10) {
        params.fmt.frame_dms = 100;
    } else {
        ASSERT(0, "frame_dms err:%d", config->frame_duration);
    }
    /* params.fmt.frame_dms = config->sdu_period * 10 / 1000; */
    params.fmt.sdu_period = config->sdu_period;
    params.fmt.isoIntervalUs = config->sdu_period * g_sink_bn;
    params.fmt.sample_rate = config->sample_rate;
    params.fmt.dec_ch_mode = LEA_DEC_OUTPUT_CHANNEL;
    params.fmt.bit_rate = params.fmt.nch * config->bit_rate;
    params.conn = config->Connection_Handle[0];
    g_printf("nch:%d, coding_type:0x%x, dec_ch_mode:%d, conn:%d, dms:%d, sdu_period:%d, br:%d\n",
             params.fmt.nch, params.fmt.coding_type, params.fmt.dec_ch_mode, params.conn, params.fmt.frame_dms, params.fmt.sdu_period, params.fmt.bit_rate);
    void *le_audio_stream_ctx = le_audio_stream_create(params.conn, &params.fmt);
    void *rx_stream = le_audio_stream_rx_open(le_audio_stream_ctx, params.fmt.coding_type);
    err = le_audio_player_open(le_audio_stream_ctx, &params);
    if (err != 0) {
        ASSERT(0, "player open fail");
    }

    spin_lock(&auracast_lock);
    g_rx_audio.rx_player.le_audio = le_audio_stream_ctx;
    g_rx_audio.rx_player.rx_stream = rx_stream;
    if (g_rx_audio.bis_num > 1) {
        for (u8 i = 0; i < g_sink_bn; i++) {
            if (!multi_bis_rx_buf[i]) {
                multi_bis_rx_temp_buf_len = g_rx_audio.bis_num * config->bit_rate * params.fmt.frame_dms / 8 / 1000 / 10;
                g_printf("multi_bis_rx_temp_buf_len:%d", multi_bis_rx_temp_buf_len);
                multi_bis_rx_buf[i] = zalloc(multi_bis_rx_temp_buf_len);
            }
        }
    }
    spin_unlock(&auracast_lock);
}

static bool le_auracast_iso_sdu_all_zeros(uint8_t *array, int length)
{
    for (int i = 0; i < length; i++) {
        if (array[i] != 0) {
            return false;
        }
    }
    return true;
}

static void le_auracast_iso_rx_callback(uint8_t *packet, uint16_t size)
{
    u8 i = 0;
    static u8 j = 0;
    s8 index = -1;
    bool plc_flag = 0;
    hci_iso_hdr_t hdr = {0};
    ll_iso_unpack_hdr(packet, &hdr);

    if (size && hdr.iso_sdu_length) {
        if (le_auracast_iso_sdu_all_zeros(hdr.iso_sdu, hdr.iso_sdu_length)) {
            /* log_error("SDU empty"); */
            putchar('z');
            plc_flag = 1;
        }
    }

    if ((hdr.pb_flag == 0b10) && (hdr.iso_sdu_length == 0)) {
        if (hdr.packet_status_flag == 0b00) {
            /* log_error("SDU empty"); */
            putchar('m');
            plc_flag = 1;
        } else {
            /* log_error("SDU lost"); */
            putchar('s');
            plc_flag = 1;
        }
    }
    if (((hdr.pb_flag == 0b10) || (hdr.pb_flag == 0b00)) && (hdr.packet_status_flag == 0b01)) {
        //log_error("SDU invalid, len=%d", hdr.iso_sdu_length);
        putchar('p');
        plc_flag = 1;
    }
    spin_lock(&auracast_lock);
    for (i = 0; i < g_rx_audio.bis_num; i++) {
        if (g_rx_audio.audio_hdl[i].bis_hdl == hdr.handle) {
            if ((!g_rx_audio.rx_player.le_audio) || (!g_rx_audio.rx_player.rx_stream)) {
                spin_unlock(&auracast_lock);
                return;
            }
            index = i;
            break;
        }
    }

    if (index == -1) {
        spin_unlock(&auracast_lock);
        return;
    }

    j++;
    if (j >= g_sink_bn) {
        j = 0;
    }

    /* printf("[%d][%d][%d][%d][%d]\n", i, j, hdr.handle, hdr.iso_sdu_length, size); */
    if (plc_flag || multi_bis_plc_flag[j]) {
        if (multi_bis_rx_buf[j]) {
            multi_bis_plc_flag[j] = 1;
            for (i = 0; i < g_rx_audio.bis_num; i++) {
                if (g_rx_audio.audio_hdl[i].bis_hdl == hdr.handle) {
                    break;
                }
            }
            if (i == (g_rx_audio.bis_num - 1)) {
                memcpy(multi_bis_rx_buf[j], errpacket, 2);
                le_audio_stream_rx_frame(g_rx_audio.rx_player.rx_stream, (void *)multi_bis_rx_buf[j], 2,
                                         hdr.time_stamp);
                multi_bis_data_offect[j] = 0;
                multi_bis_plc_flag[j] = 0;
            }
        } else {
            le_audio_stream_rx_frame(g_rx_audio.rx_player.rx_stream, (void *)errpacket, 2, hdr.time_stamp);
        }
    } else {
        if (multi_bis_rx_buf[j]) {
            memcpy(multi_bis_rx_buf[j] + multi_bis_data_offect[j], hdr.iso_sdu, hdr.iso_sdu_length);
            multi_bis_data_offect[j] += hdr.iso_sdu_length;
            ASSERT(multi_bis_data_offect[j] <= multi_bis_rx_temp_buf_len);
            if (multi_bis_data_offect[j] >= multi_bis_rx_temp_buf_len) {
                /* printf("R:%d, %d\n", multi_bis_rx_temp_buf_len, hdr.time_stamp); */
                le_audio_stream_rx_frame(g_rx_audio.rx_player.rx_stream, (void *)multi_bis_rx_buf[j], multi_bis_rx_temp_buf_len, hdr.time_stamp);
                multi_bis_data_offect[j] -= multi_bis_rx_temp_buf_len;
            }
        } else {
            le_audio_stream_rx_frame(g_rx_audio.rx_player.rx_stream, (void *)hdr.iso_sdu, hdr.iso_sdu_length, hdr.time_stamp);
        }
        /* u32 local = bb_le_clk_get_time_us(); */
        /* printf("[%d, %d, %d, %d]\n", local, hdr.time_stamp, TCFG_LE_AUDIO_PLAY_LATENCY, local - hdr.time_stamp - TCFG_LE_AUDIO_PLAY_LATENCY); */
    }
    spin_unlock(&auracast_lock);
}

static void le_auracast_audio_close(void)
{
    printf("le_auracast_audio_close\n");
    if (g_rx_audio.rx_player.le_audio && g_rx_audio.rx_player.rx_stream) {
        spin_lock(&auracast_lock);
        for (int i = 0; i < g_sink_bn; i++) {
            if (multi_bis_rx_buf[i]) {
                free(multi_bis_rx_buf[i]);
                multi_bis_rx_buf[i] = 0;
            }
        }

        void *le_audio_stream_ctx = g_rx_audio.rx_player.le_audio;
        void *rx_stream = g_rx_audio.rx_player.rx_stream;
        g_rx_audio.rx_player.le_audio = NULL;
        g_rx_audio.rx_player.rx_stream = NULL;
        spin_unlock(&auracast_lock);
        le_audio_player_close(le_audio_stream_ctx);
        le_audio_stream_rx_close(rx_stream);
        le_audio_stream_free(le_audio_stream_ctx);
        printf("le_auracast_audio_close end!\n");
    }
}

/**
 * @brief 设备收到广播设备信息汇报给手机APP
 */
void auracast_sink_source_info_report_event_deal(uint8_t *packet, uint16_t length)
{
#if (defined(RCSP_ADV_AURCAST_SINK) && RCSP_ADV_AURCAST_SINK)
    struct auracast_source_item_t src = {0};
    auracast_sink_source_info_t *param = (auracast_sink_source_info_t *)packet;

    u8 name_len = strlen((const char *)param->broadcast_name);
    if (0 != name_len) {
        memcpy(src.broadcast_name, param->broadcast_name, name_len);
    }
    memcpy(src.adv_address, param->source_mac_addr, 6);
    src.broadcast_features = param->feature;
    src.broadcast_id[0] = param->broadcast_id & 0xFF;
    src.broadcast_id[1] = (param->broadcast_id >> 8) & 0xFF;
    src.broadcast_id[2] = (param->broadcast_id >> 16) & 0xFF;
    auracast_app_notify_source_list(&src);
#endif
}

static void auracast_sink_big_info_report_event_deal(uint8_t *packet, uint16_t length)
{
    auracast_sink_source_info_t *param = (auracast_sink_source_info_t *)packet;
    g_sink_bn = param->bn; // 一个iso interval里面有多少个包
    printf("auracast_sink_big_info_report_event_deal\n");
    y_printf("num bis : %d, bn : %d\n", param->Num_BIS, g_sink_bn);
    if (param->Num_BIS > AURACAST_SINK_BIS_NUMS) {
        param->Num_BIS = AURACAST_SINK_BIS_NUMS;
    }
}
static void le_auracast_audio_close_in_app_core()
{
    log_info("le_auracast_audio_close_in_app_core");
    int argv[2];
    argv[0] = (int)le_auracast_audio_close;
    argv[1] = 0;
    int ret = os_taskq_post_type("app_core", Q_CALLBACK, 2, argv);
    if (ret) {
        r_printf("le_auracast taskq post err %d!\n", __LINE__);
    }
}

static void __le_auracast_audio_open_in_app_core(uint8_t *packet, uint16_t length)
{
    le_auracast_audio_open(packet, length);
    free(packet);
}

static void le_auracast_audio_open_in_app_core(uint8_t *packet, uint16_t length)
{
    log_info("le_auracast_audio_open_in_app_core");
    u8 *data = malloc(length);
    if (data == NULL) {
        r_printf("le_auracast malloc err %d!\n", __LINE__);
        return;
    }
    memcpy(data, packet, length);
    int argv[4];
    argv[0] = (int)__le_auracast_audio_open_in_app_core;
    argv[1] = 2;
    argv[2] = (int)data;
    argv[3] = (int)length;
    int ret = os_taskq_post_type("app_core", Q_CALLBACK, 4, argv);
    if (ret) {
        r_printf("le_auracast taskq post err %d!\n", __LINE__);
        free(data);
    }
}

static void auracast_sink_event_callback(uint16_t event, uint8_t *packet, uint16_t length)
{
    // 这里监听后会一直打印
    /* printf("auracast_sink_event_callback:%d\n", event); */
    switch (event) {
    case AURACAST_SINK_SOURCE_INFO_REPORT_EVENT:
        printf("AURACAST_SINK_SOURCE_INFO_REPORT_EVENT\n");
        auracast_sink_source_info_report_event_deal(packet, length);
        break;
    case AURACAST_SINK_BLE_CONNECT_EVENT:
        printf("AURACAST_SINK_BLE_CONNECT_EVENT\n");
        break;
    case AURACAST_SINK_BIG_SYNC_CREATE_EVENT:
        printf("AURACAST_SINK_BIG_SYNC_CREATE_EVENT\n");
        if (cur_listening_source_info == NULL) {
            printf("cur_listening_source_info == NULL\n");
            break;
        }

        if (auracast_sink_sync_timeout_hdl != 0) {
            sys_timeout_del(auracast_sink_sync_timeout_hdl);
            auracast_sink_sync_timeout_hdl = 0;
        }
        app_auracast_app_notify_listening_status(AURACAST_SINK_SYNC_STATE_SYNC_COMPLETE, AURACAST_SINK_SYNC_NO_ERROR);
#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)))
        if (is_cig_phone_call_play()) {
            printf("auracast_sink_event_callback cig esco_player_runing\n");
            app_auracast_sink_big_sync_terminate(0);
            break;
        }
#endif
        if (esco_player_runing()) {
            printf("auracast_sink_event_callback esco_player_runing\n");
            app_auracast_sink_big_sync_terminate(0);
            break;
        }
        le_auracast_audio_open_in_app_core(packet, length);
        break;
    case AURACAST_SINK_BIG_SYNC_TERMINATE_EVENT:
        printf("AURACAST_SINK_BIG_SYNC_TERMINATE_EVENT\n");
        break;
    case AURACAST_SINK_BIG_SYNC_FAIL_EVENT:
        printf("AURACAST_SINK_BIG_SYNC_FAIL_EVENT\n");
    case AURACAST_SINK_BIG_SYNC_LOST_EVENT:
        printf("big lost or fail\n");
        le_auracast_audio_close_in_app_core();
        if (packet[0] == 0x3d) {
            printf("key error\n");
            app_auracast_app_notify_listening_status(AURACAST_SINK_SYNC_STATE_IDLE, AURACAST_SINK_SYNC_BROADCAST_CODE_ERROR);
            break;
        }

        if (auracast_sink_sync_timeout_hdl) {       //如果没有超时
            if (cur_listening_source_info) {
                printf("retry listen\n");
                auracast_sink_big_sync_create(cur_listening_source_info);   //再走一次auracast连接
                break;
            } else {
                printf("retry listen error\n");
            }
        }

        printf("big timeout lost\n");

        if (cur_listening_source_info) {
            free(cur_listening_source_info);
            cur_listening_source_info = NULL;
        }
        app_auracast_app_notify_listening_status(AURACAST_SINK_SYNC_STATE_IDLE, AURACAST_SINK_SYNC_LOST);
        break;
    case AURACAST_SINK_PERIODIC_ADVERTISING_SYNC_LOST_EVENT:
        if (cur_listening_source_info) {
            printf("AURACAST_SINK_PERIODIC_ADVERTISING_SYNC_LOST_EVENT\n");
            auracast_sink_big_sync_create(cur_listening_source_info);
        } else {
            printf("AURACAST_SINK_PERIODIC_ADVERTISING_SYNC_LOST_EVENT FAIL!\n");
        }
        break;
    case AURACAST_SINK_BIG_INFO_REPORT_EVENT:
        printf("AURACAST_SINK_BIG_INFO_REPORT_EVENT\n");
        auracast_sink_big_info_report_event_deal(packet, length);
        break;
    case AURACAST_SINK_ISO_RX_CALLBACK_EVENT:
        //printf("AURACAST_SINK_ISO_RX_CALLBACK_EVENT\n");
        le_auracast_iso_rx_callback(packet, length);
        break;
    case AURACAST_SINK_EXT_SCAN_COMPLETE_EVENT:
        printf("AURACAST_SINK_EXT_SCAN_COMPLETE_EVENT\n");
        break;
    case AURACAST_SINK_PADV_REPORT_EVENT:
        // printf("AURACAST_SINK_PADV_REPORT_EVENT\n");
        // put_buf(packet, length);
        break;
    default:
        break;
    }
}

int app_auracast_bass_server_event_callback(uint8_t event, uint8_t *packet, uint16_t size)
{
    int ret = 0;
    struct le_audio_bass_add_source_info_t *bass_source_info = (struct le_audio_bass_add_source_info_t *)packet;
    auracast_sink_source_info_t add_source_param = {0};
    switch (event) {
    case BASS_SERVER_EVENT_SCAN_STOPPED:
        break;
    case BASS_SERVER_EVENT_SCAN_STARTED:
        break;
    case BASS_SERVER_EVENT_SOURCE_ADDED:
        printf("BASS_SERVER_EVENT_SOURCE_ADDED\n");
        if ((bass_source_info->pa_sync == BASS_PA_SYNC_SYNCHRONIZE_TO_PA_PAST_AVAILABLE) \
            || (bass_source_info->pa_sync == BASS_PA_SYNC_SYNCHRONIZE_TO_PA_PAST_NOT_AVAILABLE)) {
            memcpy(add_source_param.source_mac_addr, bass_source_info->address, 6);
            bass_source_info->bis_sync_state = 0x03;
            app_auracast_sink_big_sync_create(&add_source_param);
        }
        break;
    case BASS_SERVER_EVENT_SOURCE_MODIFIED:
        printf("BASS_SERVER_EVENT_SOURCE_MODIFIED pa_sync:%x sync_state:%x\n", bass_source_info->pa_sync, bass_source_info->bis_sync_state);
        if ((bass_source_info->pa_sync == BASS_PA_SYNC_SYNCHRONIZE_TO_PA_PAST_AVAILABLE) \
            || (bass_source_info->pa_sync == BASS_PA_SYNC_SYNCHRONIZE_TO_PA_PAST_NOT_AVAILABLE)) {
            if (auracast_sink_big_state_get() && (cur_listening_source_info != NULL)) {
                if (0 == memcmp(bass_source_info->address, cur_listening_source_info->source_mac_addr, 6)) {
                    if (bass_source_info->bis_sync_state == 0) {
                        printf("source is listening, terminate !\n");
                        app_auracast_sink_big_sync_terminate(0);
                    } else  {
                        printf("same source, dont do anything !\n");
                        bass_source_info->bis_sync_state = 0x03;
                    }
                }
            } else {
                if (bass_source_info->bis_sync_state) {
                    printf("listen !\n");
                    app_auracast_sink_big_sync_terminate(0);
                    memcpy(add_source_param.source_mac_addr, bass_source_info->address, 6);
                    bass_source_info->bis_sync_state = 0x03;
                    app_auracast_sink_big_sync_create(&add_source_param);
                } else {
                    printf("is not listening, dont do anything !\n");
                }
            }
        } else {
            if (bass_source_info->bis_sync_state) {
                printf("listen ??\n");
                if (auracast_sink_big_state_get() && (cur_listening_source_info != NULL)) {
                    if (0 == memcmp(bass_source_info->address, cur_listening_source_info->source_mac_addr, 6)) {
                        printf("same source, dont do anything !\n");
                        break;
                    }
                }
                app_auracast_sink_big_sync_terminate(0);
                memcpy(add_source_param.source_mac_addr, bass_source_info->address, 6);
                bass_source_info->bis_sync_state = 0x03;
                app_auracast_sink_big_sync_create(&add_source_param);
            } else {
                printf("BASS_SERVER_EVENT_SOURCE_MODIFIED terminate\n");
                app_auracast_sink_big_sync_terminate(0);
            }
        }
        break;
    case BASS_SERVER_EVENT_SOURCE_DELETED:
        break;
    case BASS_SERVER_EVENT_BROADCAST_CODE:
        printf("BASS_SERVER_EVENT_BROADCAST_CODE id=%d\n", packet[0]);
        put_buf(packet, size);

        if (cur_listening_source_info) {
            memcpy(cur_listening_source_info->broadcast_code, &packet[1], 16);
            ret = app_auracast_sink_big_sync_create(cur_listening_source_info);
            if (ret != 0) {
                r_printf("BASS_SERVER_EVENT_BROADCAST_CODE app_auracast_sink_big_sync_create fail!\n");
            }
        } else {
            r_printf("BASS_SERVER_EVENT_BROADCAST_CODE fail!\n");
        }
        break;
    }
    return 0;
}

void app_auracast_sink_init(void)
{
    printf("app_auracast_sink_init\n");
    auracast_sink_init(AURACAST_SINK_API_VERSION);
    auracast_sink_event_callback_register(auracast_sink_event_callback);
    le_audio_bass_event_callback_register(app_auracast_bass_server_event_callback);
}

static int __app_auracast_sink_big_sync_terminate(u8 need_recover)
{
    printf("__app_auracast_sink_big_sync_terminate:%d\n", need_recover);

    if (cur_listening_source_info) {
        if (need_recover) {
            if (!recover_listening_source_info) {
                recover_listening_source_info = malloc(sizeof(auracast_sink_source_info_t));
            }
            if (recover_listening_source_info) {
                memcpy(recover_listening_source_info, cur_listening_source_info, sizeof(auracast_sink_source_info_t));
            }
        }
        free(cur_listening_source_info);
        cur_listening_source_info = NULL;
    }

    int ret = auracast_sink_big_sync_terminate();
    if (0 == ret) {
        le_auracast_audio_close_in_app_core();
        if (auracast_sink_sync_timeout_hdl != 0) {
            sys_timeout_del(auracast_sink_sync_timeout_hdl);
            auracast_sink_sync_timeout_hdl = 0;
        }
    }
    return ret;
}

/**
 * @brief 主动关闭所有正在监听播歌的广播设备
 *
 * @param need_recover 需要恢复
 */
int app_auracast_sink_big_sync_terminate(u8 need_recover)
{
    printf("app_auracast_sink_big_sync_terminate:%d\n", need_recover);
#if TCFG_USER_TWS_ENABLE
    int ret = 0;
    if (!(tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
        ret = __app_auracast_sink_big_sync_terminate(need_recover);
    } else {
        u8 local_need_recover = need_recover;
        tws_api_send_data_to_sibling((void *)&local_need_recover, sizeof(u8), 0x23482C5C);
        ret = 0;
    }
#else
    int ret = __app_auracast_sink_big_sync_terminate(need_recover);

    g_rx_audio.bis_num = 0;
#endif
    return ret;
}

static int __app_auracast_sink_scan_start(void)
{
    bt_cmd_prepare(USER_CTRL_ALL_SNIFF_EXIT, 0, NULL);
    int ret = auracast_sink_scan_start();
    printf("auracast_sink_scan_start ret:%d\n", ret);
    return ret;
}

/**
 * @brief 手机通知设备开始搜索auracast广播
 */
int app_auracast_sink_scan_start(void)
{
    printf("app_auracast_sink_scan_start\n");
#if TCFG_USER_TWS_ENABLE
    g_cur_auracast_is_scanning = 1;
    int err = tws_api_send_data_to_sibling((void *)&g_cur_auracast_is_scanning, sizeof(u8), 0x23482C5D);
    if (err) {
        err = __app_auracast_sink_scan_start();
    }
    return err;
#else
    return __app_auracast_sink_scan_start();
#endif
}

static int __app_auracast_sink_scan_stop(void)
{
    int ret = auracast_sink_scan_stop();
    printf("auracast_sink_scan_stop ret:%d\n", ret);
    return ret;
}

/**
 * @brief 手机通知设备关闭搜索auracast广播
 */
int app_auracast_sink_scan_stop(void)
{
    printf("app_auracast_sink_scan_stop\n");
#if TCFG_USER_TWS_ENABLE
    g_cur_auracast_is_scanning = 0;
    return tws_api_send_data_to_sibling((void *)&g_cur_auracast_is_scanning, sizeof(u8), 0x23482C5D);
#else
    return __app_auracast_sink_scan_stop();
#endif
}

/**
 * @brief 创建超时，需要关闭
 */
static void auracast_sink_sync_timeout_handler(void *priv)
{
    printf("auracast_sink_sync_timeout_handler\n");
#if TCFG_USER_TWS_ENABLE
    app_auracast_sink_scan_stop();
    app_auracast_sink_big_sync_terminate(0);
    u8 pa_sync_state = BASS_PA_SYNC_STATE_FAILED_TO_SYNCHRONIZE_TO_PA;
    tws_api_send_data_to_sibling((void *)&pa_sync_state, sizeof(u8), 0x23482C5F);
#else
    auracast_sink_scan_stop();
    auracast_sink_big_sync_terminate();
    app_le_audio_bass_notify_pa_sync_state(BASS_PA_SYNC_STATE_FAILED_TO_SYNCHRONIZE_TO_PA, BASS_BIG_ENCRYPTION_NOT_ENCRYPTED, 0xFFFFFFFF);
    app_auracast_app_notify_listening_status(AURACAST_SINK_SYNC_STATE_IDLE, AURACAST_SINK_SYNC_TIMEOUT);
    auracast_sink_sync_timeout_hdl = 0;
#endif
}

static int __app_auracast_sink_big_sync_create(auracast_sink_source_info_t *param, u8 tws_malloc)
{
    // 开始监听的时候就暂停a2dp播歌
    u8 bt_addr[6];
    if (a2dp_player_get_btaddr(bt_addr)) {
        a2dp_player_close(bt_addr);
        a2dp_media_mute(bt_addr);
        void *device = btstack_get_conn_device(bt_addr);
        if (device) {
            btstack_device_control(device, USER_CTRL_AVCTP_OPID_PAUSE);
        }
    }
    if (cur_listening_source_info == NULL) {
        cur_listening_source_info = malloc(sizeof(auracast_sink_source_info_t));
    }
    memcpy(cur_listening_source_info, param, sizeof(auracast_sink_source_info_t));
    log_info("auracast sync create, set code\n");
    put_buf(cur_listening_source_info->broadcast_code, 16);
    auracast_sink_set_broadcast_code(cur_listening_source_info->broadcast_code);
    int ret = auracast_sink_big_sync_create(cur_listening_source_info);
    if (0 == ret) {
        if (auracast_sink_sync_timeout_hdl == 0) {
            auracast_sink_sync_timeout_hdl = sys_timeout_add(NULL, auracast_sink_sync_timeout_handler, 15000);
        }
    } else {
        printf("__app_auracast_sink_big_sync_create ret:%d\n", ret);
    }
    if (tws_malloc) {
        free(param);
    }

    return ret;
}

static void __le_auracast_audio_recover()
{
    if ((recover_listening_source_info != NULL) && !le_auracast_is_running()) {
        printf("__le_auracast_audio_recover1\n");
        int ret = app_auracast_sink_big_sync_create(recover_listening_source_info);
        if (0 == ret) {
            if (auracast_sink_sync_timeout_hdl == 0) {
                auracast_sink_sync_timeout_hdl = sys_timeout_add(NULL, auracast_sink_sync_timeout_handler, 15000);
            }
        } else {
            printf("__le_auracast_audio_recover ret:%d\n", ret);
        }
        free(recover_listening_source_info);
        recover_listening_source_info = NULL;
    } else {
        printf("__le_auracast_audio_recover2\n");
    }
}

/**
 * @brief auracast音频恢复
 */
void le_auracast_audio_recover()
{
    printf("le_auracast_audio_recover\n");
#if TCFG_USER_TWS_ENABLE
    if (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED) {
        tws_api_send_data_to_sibling(NULL, 0, 0x23482C5E);
    } else {
        int argv[2];
        argv[0] = (int)__le_auracast_audio_recover;
        argv[1] = 0;
        int ret = os_taskq_post_type("app_core", Q_CALLBACK, 2, argv);
        if (ret) {
            r_printf("le_auracast taskq post err %d!\n", __LINE__);
        }
    }
#else
    int argv[2];
    argv[0] = (int)__le_auracast_audio_recover;
    argv[1] = 0;
    int ret = os_taskq_post_type("app_core", Q_CALLBACK, 2, argv);
    if (ret) {
        r_printf("le_auracast taskq post err %d!\n", __LINE__);
    }
#endif
}

/**
 * @brief 手机选中广播设备开始播歌
 *
 * @param param 要监听的广播设备
 */
int app_auracast_sink_big_sync_create_in_task(auracast_sink_source_info_t *param)
{
    printf("app_auracast_sink_big_sync_create_in_task\n");
    bt_cmd_prepare(USER_CTRL_ALL_SNIFF_EXIT, 0, NULL);

#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)))
    if (is_cig_music_play() || is_cig_phone_call_play()) {
        printf("cig state: 0x%x 0x%x\n", is_cig_music_play(), is_cig_phone_call_play());
        printf("app_auracast_sink_big_sync_create cig esco_player_runing\n");
        app_le_audio_bass_notify_pa_sync_state(BASS_PA_SYNC_STATE_NOT_SYNCHRONIZED_TO_PA, BASS_BIG_ENCRYPTION_NOT_ENCRYPTED, 0xFFFFFFFF);
        return -1;
    }
#endif
    if (esco_player_runing()) {
        printf("app_auracast_sink_big_sync_create esco_player_runing\n");
        // 暂停auracast的播歌
        return -1;
    }

#if TCFG_USER_TWS_ENABLE
    int ret = 0;
    if (!(tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
        ret = __app_auracast_sink_big_sync_create(param, 0);
    } else {
        tws_api_send_data_to_sibling(param, sizeof(auracast_sink_source_info_t), 0x23482C5B);
        ret = 0;
    }
#else
    int ret = __app_auracast_sink_big_sync_create(param, 0);
#endif

    return ret;
}

void app_auracast_sink_big_sync_create_in_app_core(u8 *data)
{
    auracast_sink_source_info_t param = {0};
    printf("app_auracast_sink_big_sync_create_in_app_core 0x%x\n", (u32)data);
    if (data == NULL) {
        printf("data == NULL");
        return;
    }
    memcpy((u8 *)&param, data, sizeof(auracast_sink_source_info_t));
    put_buf(data, sizeof(auracast_sink_source_info_t));
    app_auracast_sink_big_sync_create_in_task(&param);
    free(data);
}

/**
 * @brief 手机选中广播设备开始播歌
 *
 * @param param 要监听的广播设备
 */
int app_auracast_sink_big_sync_create(auracast_sink_source_info_t *param)
{
    int argv[4];
    printf("app_auracast_sink_big_sync_create 0x%x\n", (u32)param);
    if (param == NULL) {
        printf("param == NULL");
        return -1;
    }
    u8 *data = malloc(sizeof(auracast_sink_source_info_t));
    if (data == NULL) {
        printf("data == NULL");
        return -1;
    }
    memcpy(data, param, sizeof(auracast_sink_source_info_t));
    put_buf(data, sizeof(auracast_sink_source_info_t));
    argv[0] = (int)app_auracast_sink_big_sync_create_in_app_core;
    argv[1] = 1;
    argv[2] = (int)data;
    int ret = os_taskq_post_type("app_core", Q_CALLBACK, 4, argv);
    if (ret) {
        printf("%s taskq post err \n", __func__);
        free(data);
        return -1;
    }
    return 0;
}

static int le_auracast_app_msg_handler(int *msg)
{
    switch (msg[0]) {
    case APP_MSG_STATUS_INIT_OK:
        printf("app_auracast APP_MSG_STATUS_INIT_OK");
        char le_audio_name[LOCAL_NAME_LEN] = {0};
        memcpy(le_audio_name, (u8 *)bt_get_local_name(), LOCAL_NAME_LEN);
        le_audio_name_reset((u8 *)le_audio_name, strlen(le_audio_name));
#if !(TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_UNICAST_SINK_EN)
        le_audio_init(3);
        bt_le_audio_adv_enable(1);
#endif

        app_auracast_sink_init();
        break;
    case APP_MSG_POWER_OFF://1
        //le_audio_uninit(3);
        printf("app_auracast APP_MSG_POWER_OFF");
        break;
    default:
        break;
    }
    return 0;
}

APP_MSG_HANDLER(le_auracast_app_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_APP,
    .handler    = le_auracast_app_msg_handler,
};

static int le_auracast_app_hci_event_handler(int *_event)
{
    struct bt_event *event = (struct bt_event *)_event;
    printf("le_auracast_app_hci_event_handler:%d\n", event->event);
    switch (event->event) {
    case HCI_EVENT_CONNECTION_COMPLETE:
        break;
    case HCI_EVENT_DISCONNECTION_COMPLETE :
        printf("app le auracast HCI_EVENT_DISCONNECTION_COMPLETE: %0x\n", event->value);
#if !(TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_UNICAST_SINK_EN)
        if (event->value == ERROR_CODE_CONNECTION_TIMEOUT) {
            bt_le_audio_adv_enable(0);
            bt_le_audio_adv_enable(1);
        }
#endif
        le_auracast_stop(0);
        break;
    }
    return 0;
}

APP_MSG_HANDLER(auracast_hci_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_BT_HCI,
    .handler    = le_auracast_app_hci_event_handler,
};

#if TCFG_USER_TWS_ENABLE

static int le_auracast_tws_msg_handler(int *msg)
{
    struct tws_event *evt = (struct tws_event *)msg;
    u8 role = evt->args[0];
    u8 phone_link_connection = evt->args[1];
    u8 reason = evt->args[2];
    switch (evt->event) {
    case TWS_EVENT_CONNECTED:
        printf("le_auracast_tws_event_handler le_auracast role:%d, %d, %d, 0x%x\n", role, tws_api_get_role(), g_cur_auracast_is_scanning, (unsigned int)cur_listening_source_info);
        if (role != TWS_ROLE_SLAVE) {
            // tws之间配对后，主机把手机的扫描状态同步给从机
            tws_api_send_data_to_sibling((void *)&g_cur_auracast_is_scanning, sizeof(u8), 0x23482C5D);
        }
        if (cur_listening_source_info != NULL) {
            // 主机已经在监听播歌了，配对后让从机也执行相同操作
            tws_api_send_data_to_sibling(cur_listening_source_info, sizeof(auracast_sink_source_info_t), 0x23482C5B);
        }
        break;
    case TWS_EVENT_PHONE_LINK_DETACH:
        break;
    case TWS_EVENT_CONNECTION_DETACH:
        printf("le_auracast_tws_msg_handler tws detach\n");
        if (g_cur_auracast_is_scanning) {
            app_auracast_sink_scan_start();
        }
        break;
    case TWS_EVENT_ROLE_SWITCH:
        printf("le_auracast_tws_event_handler le_auracast role switch:%d, %d, %d, 0x%x\n", role, tws_api_get_role(), g_cur_auracast_is_scanning, (unsigned int)cur_listening_source_info);
        if ((role == TWS_ROLE_SLAVE) && (g_cur_auracast_is_scanning)) {
            // 新从机如果是scan状态则需要关闭自己的scan，让新主机开启scan
            tws_api_send_data_to_sibling((void *)&g_cur_auracast_is_scanning, sizeof(u8), 0x23482C5D);
        }
    default:
        break;
    }
    return 0;
}

APP_MSG_HANDLER(le_auracast_tws_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_TWS,
    .handler    = le_auracast_tws_msg_handler,
};


static void app_auracast_sink_big_sync_start_tws_in_irq(void *_data, u16 len, bool rx)
{
    printf("app_auracast_sink_big_sync_start_tws_in_irq rx:%d\n", rx);
    /* printf("%s, %s, %d\n", __FILE__, __FUNCTION__, __LINE__); */
    /* put_buf(_data, len); */
    u8 *rx_data = malloc(len);
    if (rx_data == NULL) {
        return;
    }
    memcpy(rx_data, _data, len);

    int argv[4];
    argv[0] = (int)__app_auracast_sink_big_sync_create;
    argv[1] = 1;
    argv[2] = (int)rx_data;
    argv[3] = (int)1;
    int ret = os_taskq_post_type("app_core", Q_CALLBACK, 4, argv);
    if (ret) {
        r_printf("le_auracast taskq post err %d!\n", __LINE__);
        free(rx_data);
    }
}

REGISTER_TWS_FUNC_STUB(app_auracast_sink_big_sync_start_tws) = {
    .func_id = 0x23482C5B,
    .func = app_auracast_sink_big_sync_start_tws_in_irq,
};

static void app_auracast_sink_big_sync_terminate_tws_in_irq(void *_data, u16 len, bool rx)
{
    /* printf("%s, %s, %d\n", __FILE__, __FUNCTION__, __LINE__); */
    /* put_buf(_data, len); */

    u8 *u8_data = (u8 *)_data;
    u8 need_recover = (u8) * u8_data;
    printf("app_auracast_sink_big_sync_terminate_tws_in_irq rx:%d, need_recover:%d\n", rx, need_recover);
    int argv[3];
    argv[0] = (int)__app_auracast_sink_big_sync_terminate;
    argv[1] = 1;
    argv[2] = (int)need_recover;
    int ret = os_taskq_post_type("app_core", Q_CALLBACK, 3, argv);
    if (ret) {
        r_printf("le_auracast taskq post err %d!\n", __LINE__);
    }
}

REGISTER_TWS_FUNC_STUB(app_auracast_sink_big_sync_terminate_tws) = {
    .func_id = 0x23482C5C,
    .func = app_auracast_sink_big_sync_terminate_tws_in_irq,
};

static void le_auracast_scan_state_tws_sync_in_irq(void *_data, u16 len, bool rx)
{
    u8 *u8_data = (u8 *)_data;
    g_cur_auracast_is_scanning = (u8) * u8_data;
    printf("le_auracast_scan_state_tws_sync_in_irq:%d\n", g_cur_auracast_is_scanning);
    if ((tws_api_get_role() != TWS_ROLE_SLAVE) && g_cur_auracast_is_scanning) {
        int argv[2];
        argv[0] = (int)__app_auracast_sink_scan_start;
        argv[1] = 0;
        int ret = os_taskq_post_type("app_core", Q_CALLBACK, 2, argv);
        if (ret) {
            r_printf("le_auracast taskq post err %d!\n", __LINE__);
        }
    } else {
        int argv[2];
        argv[0] = (int)__app_auracast_sink_scan_stop;
        argv[1] = 0;
        int ret = os_taskq_post_type("app_core", Q_CALLBACK, 2, argv);
        if (ret) {
            r_printf("le_auracast taskq post err %d!\n", __LINE__);
        }
    }
}

REGISTER_TWS_FUNC_STUB(le_auracast_scan_state_sync) = {
    .func_id = 0x23482C5D,
    .func = le_auracast_scan_state_tws_sync_in_irq,
};

static void le_auracast_audio_recover_tws_sync_in_irq(void *_data, u16 len, bool rx)
{
    int argv[2];
    argv[0] = (int)__le_auracast_audio_recover;
    argv[1] = 0;
    int ret = os_taskq_post_type("app_core", Q_CALLBACK, 2, argv);
    if (ret) {
        r_printf("le_auracast taskq post err %d!\n", __LINE__);
    }
}

REGISTER_TWS_FUNC_STUB(le_auracast_audio_recover_sync) = {
    .func_id = 0x23482C5E,
    .func = le_auracast_audio_recover_tws_sync_in_irq,
};

static int __le_auracast_notify_status(u8 pa_sync_state)
{
    printf("__le_auracast_notify_status:%d\n", pa_sync_state);
    if (pa_sync_state == BASS_PA_SYNC_STATE_FAILED_TO_SYNCHRONIZE_TO_PA) {
        app_le_audio_bass_notify_pa_sync_state(BASS_PA_SYNC_STATE_FAILED_TO_SYNCHRONIZE_TO_PA, BASS_BIG_ENCRYPTION_NOT_ENCRYPTED, 0xFFFFFFFF);
        app_auracast_app_notify_listening_status(AURACAST_SINK_SYNC_STATE_IDLE, AURACAST_SINK_SYNC_TIMEOUT);
        return 0;
    }
    return -1;
}

static void le_auracast_notify_status_tws_sync_in_irq(void *_data, u16 len, bool rx)
{
    u8 *u8_data = (u8 *)_data;
    u8 pa_sync_state = (u8) * u8_data;

    printf("le_auracast_notify_status_tws_sync_in_irq:%d, rx:%d\n", pa_sync_state, rx);
    int argv[3];
    argv[0] = (int)__le_auracast_notify_status;
    argv[1] = 1;
    argv[2] = (int)pa_sync_state;
    int ret = os_taskq_post_type("app_core", Q_CALLBACK, 3, argv);
    if (ret) {
        r_printf("le_auracast taskq post err %d!\n", __LINE__);
    }
}

REGISTER_TWS_FUNC_STUB(le_auracast_notify_status_sync) = {
    .func_id = 0x23482C5F,
    .func = le_auracast_notify_status_tws_sync_in_irq,
};

#endif // TCFG_USER_TWS_ENABLE

#if !(TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_UNICAST_SINK_EN)
/*
 * 一些蓝牙线程消息按需处理
 * */
static int le_auracast_app_btstack_event_handler(int *_event)
{
    struct bt_event *event = (struct bt_event *)_event;
    printf("le_auracast_app_btstack_event_handler:%d\n", event->event);
    switch (event->event) {
    case BT_STATUS_FIRST_CONNECTED:
        bt_le_audio_adv_enable(0);
        break;
    case BT_STATUS_FIRST_DISCONNECT:
        bt_le_audio_adv_enable(1);
        break;
    }
    return 0;
}

APP_MSG_HANDLER(auracast_stack_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_BT_STACK,
    .handler    = le_auracast_app_btstack_event_handler,
};
#endif  // #if !(TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_UNICAST_SINK_EN)

#endif

