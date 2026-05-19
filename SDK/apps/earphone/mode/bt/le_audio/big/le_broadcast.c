/*********************************************************************************************
    *   Filename        : le_broadcast.c

    *   Description     :

    *   Author          : Weixin Liang

    *   Email           : liangweixin@zh-jieli.com

    *   Last modifiled  : 2022-12-15 14:18

    *   Copyright:(c)JIELI  2011-2022  @ , All Rights Reserved.
*********************************************************************************************/
#include "app_config.h"
#include "system/includes.h"
#include "btstack/avctp_user.h"
#include "app_msg.h"
#include "big.h"
#include "le_broadcast.h"
#include "wireless_trans_manager.h"
#include "wireless_trans.h"
#include "clock_manager/clock_manager.h"
#include "le_audio_stream.h"
#include "bt_event_func.h"
#include "audio_config.h"

#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_AURACAST_SINK_EN | LE_AUDIO_JL_AURACAST_SINK_EN)))||((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_AURACAST_SOURCE_EN | LE_AUDIO_JL_AURACAST_SOURCE_EN)))

/**************************************************************************************************
  Macros
**************************************************************************************************/
#define LOG_TAG             "[BROADCAST]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

/*! \brief BIS丢包修复 */
#define BIS_AUDIO_PLC_ENABLE  1
#define BIS_ISO_LENGTH_MODE  0

#define broadcast_get_bis_tick_time(bis_txsync)  wireless_trans_get_last_tx_clk("big_tx", (void *)(bis_txsync))

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! \brief 广播状态枚举 */
enum {
    BROADCAST_STATUS_STOP,      /*!< 广播停止 */
    BROADCAST_STATUS_START,     /*!< 广播启动 */
};

typedef struct {
    u16 bis_hdl;
    void *recorder;
    struct broadcast_rx_audio_hdl rx_player;
} bis_hdl_info_t ;

/*! \brief 广播结构体 */
struct broadcast_hdl {
    struct list_head entry; /*!< big链表项，用于多big管理 */
    u8 del;
    u8 big_hdl;
    u8 status;              /*!< 记录当前广播状态 */
    bis_hdl_info_t bis_hdl_info[BIG_MAX_BIS_NUMS];
    u32 big_sync_delay;
    const char *role_name;
};

/**************************************************************************************************
  Local Function Prototypes
**************************************************************************************************/
static void broadcast_tx_event_callback(const BIG_EVENT event, void *priv);
static void broadcast_rx_iso_callback(const void *const buf, size_t length, void *priv);
static int broadcast_rx_padv_data_callback(const void *const buf, size_t length, u8 big_hdl);
static void broadcast_rx_event_callback(const BIG_EVENT event, void *priv);

/**************************************************************************************************
  Local Global Variables
**************************************************************************************************/
#if BIS_ISO_LENGTH_MODE
u8 buf_del[255];
#endif
static DEFINE_SPINLOCK(broadcast_lock);
static OS_MUTEX broadcast_mutex;
static u8 broadcast_role;   /*!< 记录当前广播为接收端还是发送端 */
static u8 broadcast_init_flag;  /*!< 广播初始化标志 */
static u8 g_big_hdl;        /*!< 用于big_hdl获取 */
static u8 broadcast_num;    /*!< 记录当前开启了多少个big广播 */
static u8 *transmit_buf = NULL;    /*!< 用于发送端发数 */
static struct list_head broadcast_list_head = LIST_HEAD_INIT(broadcast_list_head);
static struct broadcast_sync_info broadcast_data_sync;  /*!< 用于接收同步状态的数据 */
static struct le_audio_mode_ops *le_audio_switch_ops = NULL; /*!< 广播音频和本地音频切换回调接口指针 */
const big_callback_t big_tx_cb = {
    .receive_packet_cb      = NULL,
    .receive_padv_data_cb   = NULL,
    .event_cb               = broadcast_tx_event_callback,
};
const big_callback_t big_rx_cb = {
    .receive_packet_cb      = broadcast_rx_iso_callback,
    .receive_padv_data_cb   = broadcast_rx_padv_data_callback,
    .event_cb               = broadcast_rx_event_callback,
};
#if BIS_AUDIO_PLC_ENABLE
//丢包修复补包 解码读到 这两个byte 才做丢包处理
static unsigned char errpacket[256] = {
    0x02, 0x00
};
#endif /*BIS_AUDIO_PLC_ENABLE*/

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/
static inline void broadcast_mutex_pend(OS_MUTEX *mutex, u32 line)
{
    int os_ret;
    os_ret = os_mutex_pend(mutex, 0);
    if (os_ret != OS_NO_ERR) {
        log_error("%s err, os_ret:0x%x", __FUNCTION__, os_ret);
        ASSERT(os_ret != OS_ERR_PEND_ISR, "line:%d err, os_ret:0x%x", line, os_ret);
    }
}

static inline void broadcast_mutex_post(OS_MUTEX *mutex, u32 line)
{
    int os_ret;
    os_ret = os_mutex_post(mutex);
    if (os_ret != OS_NO_ERR) {
        log_error("%s err, os_ret:0x%x", __FUNCTION__, os_ret);
        ASSERT(os_ret != OS_ERR_PEND_ISR, "line:%d err, os_ret:0x%x", line, os_ret);
    }
}

static int big_event_to_user(int event, void *value, u32 len)
{
    int *evt = zalloc(sizeof(int) + len);
    ASSERT(evt);
    evt[0] = event;
    memcpy(&evt[1], value, len);
    app_send_message_from(MSG_FROM_BIG, sizeof(int) + len, (int *)evt);
    free(evt);
    return 0;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 设置需要同步的状态数据
 *
 * @param big_hdl:big句柄
 * @param data:数据buffer
 * @param length:数据长度
 *
 * @return 0:success
 */
/* ----------------------------------------------------------------------------*/
int broadcast_set_sync_data(u8 big_hdl, void *data, size_t length)
{

    memcpy(&broadcast_data_sync, data, sizeof(struct broadcast_sync_info));

    if (broadcast_role == BROADCAST_ROLE_TRANSMITTER) {
        wireless_trans_status_sync("big_tx", data, length, NULL);
    }

    return 0;
}


/* --------------------------------------------------------------------------*/
/**
 * @brief 分配big_hdl，并检查hdl是否已被使用
 *
 * @param id:希望分配的id
 * @param head:链表头
 *
 * @return hdl:实际分配的id
 */
/* ----------------------------------------------------------------------------*/
static u16 get_available_big_hdl(u8 id, struct list_head *head)
{
    struct broadcast_hdl *p;
    u8 hdl = id;
    if ((hdl == 0) || (hdl > 0xEF)) {
        hdl = 1;
        g_big_hdl = 1;
    }

    broadcast_mutex_pend(&broadcast_mutex, __LINE__);
__again:
    list_for_each_entry(p, head, entry) {
        if (hdl == p->big_hdl) {
            hdl++;
            goto __again;
        }
    }

    if (hdl > 0xEF) {
        hdl = 0;
    }

    if (hdl == 0) {
        hdl++;
        goto __again;
    }

    g_big_hdl = hdl;
    broadcast_mutex_post(&broadcast_mutex, __LINE__);
    return hdl;
}

void broadcast_init(void)
{
    log_info("--func=%s", __FUNCTION__);
    int ret;

    if (broadcast_init_flag) {
        return;
    }

    int os_ret = os_mutex_create(&broadcast_mutex);
    if (os_ret != OS_NO_ERR) {
        log_error("%s %d err, os_ret:0x%x", __FUNCTION__, __LINE__, os_ret);
        ASSERT(0);
    }

    broadcast_init_flag = 1;

#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_AURACAST_SOURCE_EN | LE_AUDIO_JL_AURACAST_SOURCE_EN)))
    //初始化bis发送参数及注册回调
    ret = wireless_trans_init("big_tx", NULL);
    if (ret != 0) {
        log_error("wireless_trans_init fail:0x%x\n", ret);
    }
#endif

#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_AURACAST_SINK_EN | LE_AUDIO_JL_AURACAST_SINK_EN)))
    //初始化bis接收参数及注册回调
    ret = wireless_trans_init("big_rx", NULL);
    if (ret != 0) {
        log_error("wireless_trans_init fail:0x%x\n", ret);
    }
#endif
}

void broadcast_uninit(void)
{
    log_info("--func=%s", __FUNCTION__);
    int ret;

    if (!broadcast_init_flag) {
        return;
    }

    int os_ret = os_mutex_del(&broadcast_mutex, OS_DEL_NO_PEND);
    if (os_ret != OS_NO_ERR) {
        log_error("%s %d err, os_ret:0x%x", __FUNCTION__, __LINE__, os_ret);
    }

    broadcast_init_flag = 0;

#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_AURACAST_SOURCE_EN | LE_AUDIO_JL_AURACAST_SOURCE_EN)))
    ret = wireless_trans_uninit("big_tx", NULL);
    if (ret != 0) {
        log_error("wireless_trans_uninit fail:0x%x\n", ret);
    }
#endif

#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_AURACAST_SINK_EN | LE_AUDIO_JL_AURACAST_SINK_EN)))
    ret = wireless_trans_uninit("big_rx", NULL);
    if (ret != 0) {
        log_error("wireless_trans_uninit fail:0x%x\n", ret);
    }
#endif
}

static void channel_separation(void *data, void *lch_buf, void *rch_buf, u32 len, u8 packet_num)
{
    u16 single_channel_data_len = (get_big_enc_output_frame_len() - 2) / 2;
    u8 *read_ptr = (u8 *)data;
    u8 *lch_write_ptr = (u8 *)lch_buf;
    u8 *rch_write_ptr = (u8 *)rch_buf;

    for (u8 i = 0; i < packet_num; i++) {
        //拷贝数据长度信息
        memcpy(lch_write_ptr, &single_channel_data_len, sizeof(single_channel_data_len));
        lch_write_ptr += sizeof(single_channel_data_len);
        memcpy(rch_write_ptr, &single_channel_data_len, sizeof(single_channel_data_len));
        rch_write_ptr += sizeof(single_channel_data_len);

        //拷贝实际数据
        read_ptr += sizeof(single_channel_data_len);
        memcpy(lch_write_ptr, read_ptr, single_channel_data_len);
        read_ptr += single_channel_data_len;
        lch_write_ptr += single_channel_data_len;
        memcpy(rch_write_ptr, read_ptr, single_channel_data_len);
        read_ptr += single_channel_data_len;
        rch_write_ptr += single_channel_data_len;
    }
}

static int broadcast_tx_align_data_handler(u8 big_hdl)
{
    struct broadcast_hdl *broadcast_hdl = 0;
    bis_hdl_info_t *bis_hdl_info;
    int err = 0;
    int rlen = 0;
    u8 packet_num;
    u16 single_ch_trans_data_len;
    void *L_buffer, *R_buffer;
    big_stream_param_t param = {0};
    bis_txsync_t txsync;
    u32 timestamp;
    void *last_recorder;

#if (LEA_TX_CHANNEL_SEPARATION && (LE_AUDIO_CODEC_CHANNEL == 2))
    packet_num = get_big_transmit_data_len() / get_big_enc_output_frame_len();
    single_ch_trans_data_len = get_big_transmit_data_len() / 2 + packet_num;
    L_buffer = malloc(single_ch_trans_data_len);
    ASSERT(L_buffer);
    R_buffer = malloc(single_ch_trans_data_len);
    ASSERT(R_buffer);
    /* r_printf("tlen:%d, elen:%d, pnum:%d", get_big_transmit_data_len(), get_big_enc_output_frame_len(), packet_num); */
#endif

    spin_lock(&broadcast_lock);
    list_for_each_entry(broadcast_hdl, &broadcast_list_head, entry) {
        if (broadcast_hdl->big_hdl != big_hdl) {
            continue;
        }

        if (broadcast_hdl->del) {
            continue;
        }

        for (u8 i = 0; i < get_bis_num(BROADCAST_ROLE_TRANSMITTER); i++) {
            bis_hdl_info = &broadcast_hdl->bis_hdl_info[i];
            if (broadcast_hdl->bis_hdl_info[i].bis_hdl) {

                if (bis_hdl_info->recorder && (bis_hdl_info->recorder != last_recorder)) {
                    last_recorder = bis_hdl_info->recorder;
                    txsync.bis_hdl = bis_hdl_info->bis_hdl;
                    broadcast_get_bis_tick_time(&txsync);
                    timestamp = (txsync.tx_ts + broadcast_hdl->big_sync_delay +
                                 get_big_mtl_time() + get_big_sdu_period_us()) & 0xfffffff;
                    rlen = le_audio_stream_tx_data_handler(bis_hdl_info->recorder, transmit_buf, get_big_transmit_data_len(), timestamp, 0);
                }

                if (!rlen) {
                    putchar('^');
                    continue;
                }

                rlen = 0;
                param.bis_hdl = bis_hdl_info->bis_hdl;
#if (LEA_TX_CHANNEL_SEPARATION && (LE_AUDIO_CODEC_CHANNEL == 2))
                channel_separation(transmit_buf, L_buffer, R_buffer, get_big_transmit_data_len(), packet_num);
                if (!(i % 2)) {
                    err = wireless_trans_transmit("big_tx", L_buffer, single_ch_trans_data_len, &param);
                } else {
                    err = wireless_trans_transmit("big_tx", R_buffer, single_ch_trans_data_len, &param);
                }
#else
                err = wireless_trans_transmit("big_tx", transmit_buf, get_big_transmit_data_len(), &param);
#endif
                if (err != 0) {
                    log_error("wireless_trans_transmit fail\n");
                }
            }
        }
    }
    spin_unlock(&broadcast_lock);

#if (LEA_TX_CHANNEL_SEPARATION && (LE_AUDIO_CODEC_CHANNEL == 2))
    if (L_buffer) {
        free(L_buffer);
    }

    if (R_buffer) {
        free(R_buffer);
    }
#endif

    return 0;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 发送端开启广播成功后的流程开启接口
 *
 * @param priv:big_hdl_t
 * @param mode:保留
 *
 * @return 0:成功
 */
/* ----------------------------------------------------------------------------*/
int broadcast_transmitter_connect_deal(void *priv, u8 mode)
{
    u8 i;
    u8 bis_num = get_bis_num(BROADCAST_ROLE_TRANSMITTER);
    struct broadcast_hdl *broadcast_hdl = 0;
    big_hdl_t *hdl = (big_hdl_t *)priv;
    struct le_audio_stream_params params = {0};

    log_info("broadcast_transmitter_connect_deal");
    log_info("hdl->big_hdl:%d, hdl->bis_hdl:%d", hdl->big_hdl, hdl->bis_hdl[0]);

    broadcast_hdl = (struct broadcast_hdl *)zalloc(sizeof(struct broadcast_hdl));
    ASSERT(broadcast_hdl, "broadcast_hdl is NULL");

    le_audio_switch_ops = get_broadcast_audio_sw_ops();

    //关闭本地音频播放
    if (le_audio_switch_ops && le_audio_switch_ops->local_audio_close) {
        le_audio_switch_ops->local_audio_close();
    }

    params.fmt.nch = get_big_audio_coding_nch();
    params.fmt.bit_rate = get_big_audio_coding_bit_rate();
    params.fmt.coding_type = LE_AUDIO_CODEC_TYPE;
    params.fmt.frame_dms = get_big_audio_coding_frame_duration();
    params.fmt.sdu_period = get_big_sdu_period_us();
    params.fmt.sample_rate = LE_AUDIO_CODEC_SAMPLERATE;
    params.fmt.dec_ch_mode = LEA_DEC_OUTPUT_CHANNEL;
    params.latency = get_big_tx_latency();

    broadcast_mutex_pend(&broadcast_mutex, __LINE__);
    for (i = 0; i < bis_num; i++) {
        broadcast_hdl->bis_hdl_info[i].bis_hdl = hdl->bis_hdl[i];
        params.conn = broadcast_hdl->bis_hdl_info[i].bis_hdl;
        //打开广播音频播放
        if (le_audio_switch_ops && le_audio_switch_ops->tx_le_audio_open) {
            broadcast_hdl->bis_hdl_info[i].recorder = le_audio_switch_ops->tx_le_audio_open(&params);
        }
    }

    broadcast_hdl->role_name = "big_tx";
    broadcast_hdl->status = BROADCAST_STATUS_START;
    broadcast_hdl->big_hdl = hdl->big_hdl;
    broadcast_hdl->big_sync_delay = hdl->big_sync_delay;
    spin_lock(&broadcast_lock);
    list_add_tail(&broadcast_hdl->entry, &broadcast_list_head);
    spin_unlock(&broadcast_lock);
    broadcast_mutex_post(&broadcast_mutex, __LINE__);

    return 0;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 广播发送端上层事件处理回调
 *
 * @param event:具体事件
 * @param priv:事件处理用到的相关参数
 */
/* ----------------------------------------------------------------------------*/
static void broadcast_tx_event_callback(const BIG_EVENT event, void *priv)
{
    /* log_info("--func=%s, %d", __FUNCTION__, event); */

    switch (event) {
    //bis发射端开启成功后回调事件
    case BIG_EVENT_TRANSMITTER_CONNECT:
        log_info("BIG_EVENT_TRANSMITTER_CONNECT\n");
        r_printf("BIG_EVENT_TRANSMITTER_CONNECT\n");
        big_event_to_user(event, priv, sizeof(big_hdl_t));
        break;

    case BIG_EVENT_TRANSMITTER_DISCONNECT:
        log_info("BIG_EVENT_TRANSMITTER_DISCONNECT\n");
        r_printf("BIG_EVENT_TRANSMITTER_DISCONNECT\n");
        break;

    //蓝牙取数发射回调事件
    case BIG_EVENT_TRANSMITTER_ALIGN:
        /* WARNING:该事件为中断函数回调, 不要添加过多打印 */
        u8 big_hdl = *((u8 *)priv);
        broadcast_tx_align_data_handler(big_hdl);
        break;

    default:
        break;
    }
}

/* ***************************************************************************/
/**
 * @brief open broadcast as transmitter
 *
 * @return available_big_hdl:success
 */
/* *****************************************************************************/
int broadcast_transmitter(big_parameter_t *params)
{
    int ret;

    if (broadcast_role == BROADCAST_ROLE_RECEIVER) {
        log_error("broadcast_role err");
        return -EPERM;
    }

    if (broadcast_num >= BIG_MAX_NUMS) {
        log_error("broadcast_num overflow");
        return -EPERM;
    }

    if (!broadcast_init_flag) {
        return -EPERM;
    }

    u8 available_big_hdl = get_available_big_hdl(++g_big_hdl, &broadcast_list_head);

    log_info("--func=%s", __FUNCTION__);

    clock_alloc("le_broadcast", 12 * 1000000UL);

    set_big_hdl(BROADCAST_ROLE_TRANSMITTER, available_big_hdl);
    //启动广播
    ret = wireless_trans_open("big_tx", (void *)params);
    if (ret != 0) {
        log_error("wireless_trans_open fail:0x%x\n", ret);
        if (broadcast_num == 0) {
            broadcast_role = BROADCAST_ROLE_UNKNOW;
        }
        return -EPERM;
    }

    if (transmit_buf) {
        free(transmit_buf);
    }
    transmit_buf = zalloc(get_big_transmit_data_len());
    ASSERT(transmit_buf, "transmit_buf is NULL");

    broadcast_role = BROADCAST_ROLE_TRANSMITTER;	//发送模式

    broadcast_num++;

    return available_big_hdl;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 接收端开启广播成功后的流程开启接口
 *
 * @param priv:big_hdl_t
 *
 * @return 0:成功
 */
/* ----------------------------------------------------------------------------*/
int broadcast_receiver_connect_deal(void *priv)
{
    u8 bis_num = get_bis_num(BROADCAST_ROLE_RECEIVER);
    int rx_crc = 0;
    struct broadcast_hdl *broadcast_hdl = 0;
    big_hdl_t *hdl = (big_hdl_t *)priv;
    struct le_audio_stream_params params;

    log_info("broadcast_receiver_connect_deal");
    log_info("hdl->big_hdl:%d, hdl->bis_hdl:%d", hdl->big_hdl, hdl->bis_hdl[0]);

    broadcast_hdl = (struct broadcast_hdl *)zalloc(sizeof(struct broadcast_hdl));
    ASSERT(broadcast_hdl, "broadcast_hdl is NULL");

    le_audio_switch_ops = get_broadcast_audio_sw_ops();

    //关闭本地音频播放
    if (le_audio_switch_ops && le_audio_switch_ops->local_audio_close) {
        le_audio_switch_ops->local_audio_close();
    }

    params.fmt.nch = get_big_audio_coding_nch();
    params.fmt.coding_type = get_big_coding_type();
    params.fmt.frame_dms = LE_AUDIO_CODEC_FRAME_LEN;
    params.fmt.sdu_period = get_big_sdu_period_us();
    params.fmt.sample_rate = get_big_sample_rate();
    params.fmt.dec_ch_mode = LEA_DEC_OUTPUT_CHANNEL;
    params.fmt.bit_rate = get_big_audio_coding_bit_rate();
    log_info("bis auido param nch[%d]coding_type[%x]frame_dms[%d]sdu_period_us[%d]sample_rate[%d]dec_ch_mode[%d]bit_rate[%d]\n", params.fmt.nch, params.fmt.coding_type, params.fmt.frame_dms, params.fmt.sdu_period, params.fmt.sample_rate, params.fmt.dec_ch_mode, params.fmt.bit_rate);

    broadcast_mutex_pend(&broadcast_mutex, __LINE__);
    for (u8 i = 0; i < bis_num; i++) {
        broadcast_hdl->bis_hdl_info[i].bis_hdl = hdl->bis_hdl[i];
        params.conn = broadcast_hdl->bis_hdl_info[i].bis_hdl;
        //打开广播音频播放
        if (le_audio_switch_ops && le_audio_switch_ops->rx_le_audio_open) {
            le_audio_switch_ops->rx_le_audio_open(&broadcast_hdl->bis_hdl_info[i].rx_player, &params);
        }
    }
    broadcast_hdl->role_name = "big_rx";
    broadcast_hdl->status = BROADCAST_STATUS_START;
    broadcast_hdl->big_hdl = hdl->big_hdl;
    spin_lock(&broadcast_lock);
    list_add_tail(&broadcast_hdl->entry, &broadcast_list_head);
    spin_unlock(&broadcast_lock);
    broadcast_mutex_post(&broadcast_mutex, __LINE__);

    return 0;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 接收端开启广播监听失败后的流程关闭接口
 *
 * @param priv:big句柄
 *
 * @return 0:成功
 */
/* ----------------------------------------------------------------------------*/
int broadcast_receiver_disconnect_deal(void *priv)
{
    u8 bis_num = get_bis_num(BROADCAST_ROLE_RECEIVER);
    struct broadcast_hdl *p, *n;
    u8 big_hdl = (u8)priv;
    struct broadcast_rx_audio_hdl player;

    player.le_audio = 0;
    player.rx_stream = 0;

    log_info("%s, big_hdl:%d", __FUNCTION__, big_hdl);

    broadcast_mutex_pend(&broadcast_mutex, __LINE__);
    spin_lock(&broadcast_lock);
    list_for_each_entry_safe(p, n, &broadcast_list_head, entry) {
        if (p->big_hdl == big_hdl) {
            list_del(&p->entry);
            for (u8 i = 0; i < bis_num; i++) {
                if (p->bis_hdl_info[i].rx_player.le_audio) {
                    player.le_audio = p->bis_hdl_info[i].rx_player.le_audio;
                    p->bis_hdl_info[i].rx_player.le_audio = NULL;
                }

                if (p->bis_hdl_info[i].rx_player.rx_stream) {
                    player.rx_stream = p->bis_hdl_info[i].rx_player.rx_stream;
                    p->bis_hdl_info[i].rx_player.rx_stream = NULL;
                }
                spin_unlock(&broadcast_lock);

                if (player.le_audio && player.rx_stream) {
                    if (le_audio_switch_ops && le_audio_switch_ops->rx_le_audio_close) {
                        le_audio_switch_ops->rx_le_audio_close(&player);
                    }
                }
                spin_lock(&broadcast_lock);
            }

            free(p);
        }
    }
    spin_unlock(&broadcast_lock);
    broadcast_mutex_post(&broadcast_mutex, __LINE__);

    return 0;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 广播接收端上层事件处理回调
 *
 * @param event:具体事件
 * @param priv:事件处理用到的相关参数
 */
/* ----------------------------------------------------------------------------*/
static void broadcast_rx_event_callback(const BIG_EVENT event, void *priv)
{
    u8 rx_big_hdl;

    log_info("--func=%s, %d", __FUNCTION__, event);

    switch (event) {
    //bis接收端开启成功后回调事件
    case BIG_EVENT_RECEIVER_CONNECT:
        log_info("BIS_EVENT_RECEIVER_CONNECT\n");
        r_printf("BIS_EVENT_RECEIVER_CONNECT\n");
        big_event_to_user(event, priv, sizeof(big_hdl_t));
        break;
    //bis接收端关闭成功后回调事件
    case BIG_EVENT_PERIODIC_DISCONNECT:
    case BIG_EVENT_RECEIVER_DISCONNECT:
        log_info("BIG_EVENT_RECEIVER_DISCONNECT\n");
        r_printf("BIG_EVENT_RECEIVER_DISCONNECT\n");
        rx_big_hdl = *((u8 *)priv);
        big_event_to_user(event, (void *)&rx_big_hdl, sizeof(u8));
        break;

    default:
        break;
    }
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 接收端接收数据回调函数
 *
 * @param buf:数据缓存
 * @param length:数据长度
 * @param priv:时间戳等参数
 */
/* ----------------------------------------------------------------------------*/

static void broadcast_rx_iso_callback(const void *const buf, size_t length, void *priv)
{
#if BIS_ISO_LENGTH_MODE
    buf_del[0] = length;
    buf_del[1] = 0;
    if (length != 0) {
        memcpy(&buf_del[2], buf, length);
        length += 2;
    }
#endif
    u8 err_flag = 0;
    u8 bis_num = get_bis_num(BROADCAST_ROLE_RECEIVER);
    big_stream_param_t *param = (big_stream_param_t *)priv;
    struct broadcast_hdl *hdl;

    /* log_info("<<- BIS Data Out <<- TS:%d,%d", param->ts,(int)length); */
    /* put_buf(buf, 2); */

    int catch; //写入解码buf 的长度
    spin_lock(&broadcast_lock);
    list_for_each_entry(hdl, &broadcast_list_head, entry) {
        if (hdl->del) {
            continue;
        }

        for (u8 i = 0; i < bis_num; i++) {
            if (hdl->bis_hdl_info[i].bis_hdl == param->bis_hdl) {

#if BIS_AUDIO_PLC_ENABLE
                if (length == 0) {
                    u8 frame_num = get_big_transmit_data_len() / get_big_enc_output_frame_len();
                    for (int i = 0; i < frame_num; i++) {
                        memcpy((u8 *)errpacket + length, errpacket, 2);
                        length += 2;
                    }
                    err_flag = 1;
                }
#endif//BIS_AUDIO_PLC_ENABLE

                if (err_flag) {
                    //TODO:将接收的错包数据传给音频
                    le_audio_stream_rx_frame(hdl->bis_hdl_info[i].rx_player.rx_stream, (void *)errpacket, length, param->ts);
                } else {
                    //TODO:将接收的正常数据传给音频
#if BIS_ISO_LENGTH_MODE
                    le_audio_stream_rx_frame(hdl->bis_hdl_info[i].rx_player.rx_stream, (void *)buf_del, length, param->ts);
#else
                    le_audio_stream_rx_frame(hdl->bis_hdl_info[i].rx_player.rx_stream, (void *)buf, length, param->ts);
#endif
                }
            }
        }
    }
    spin_unlock(&broadcast_lock);
}

static int broadcast_rx_padv_data_callback(const void *const buf, size_t length, u8 big_hdl)
{
    int ret = 0;

    if (!length) {
        return -EINVAL;
    }

    u8 need_deal_flag = 0;
    if (length != sizeof(struct broadcast_sync_info)) {
        u8 i = 0;
        struct  bt_data *p = (struct  bt_data *)buf;
        struct  broadcast_bap_base *p1 = (struct  broadcast_bap_base *)p->data;
        while (i < p1->subgroups.codec_cfg.codec_len) {
            struct bt_data *p2 = (struct bt_data *)(p1->subgroups.codec_cfg.codec + i);
            switch (p2->type) {
            case BT_AUDIO_CODEC_LC3_FREQ:
                u8 freq;
                memcpy(&freq, p2->data, p2->length);
                switch (freq) {
                case BT_AUDIO_CODEC_CONFIG_LC3_FREQ_8KHZ:
                    broadcast_data_sync.sample_rate = 8000;
                    break;
                case BT_AUDIO_CODEC_CONFIG_LC3_FREQ_16KHZ:
                    broadcast_data_sync.sample_rate = 16000;
                    break;
                case BT_AUDIO_CODEC_CONFIG_LC3_FREQ_24KHZ:
                    broadcast_data_sync.sample_rate = 24000;
                    break;
                case BT_AUDIO_CODEC_CONFIG_LC3_FREQ_48KHZ:
                    broadcast_data_sync.sample_rate = 48000;
                    break;
                default:
                    broadcast_data_sync.sample_rate = 48000;
                    break;
                }
                break;
            case BT_AUDIO_CODEC_LC3_DURATION:
                u8 duration;
                memcpy(&duration, p2->data, p2->length);
                switch (duration) {
                case BT_AUDIO_CODEC_CONFIG_LC3_DURATION_10:
                    broadcast_data_sync.sdu_period_ms = 10;
                    break;
                default:
                    broadcast_data_sync.sdu_period_ms = 10;
                    break;
                }
                break;
            case BT_AUDIO_CODEC_LC3_CHAN_COUNT:
                break;
            case BT_AUDIO_CODEC_LC3_FRAME_LEN:
                u16 frame_len;
                memcpy(&frame_len, p2->data, p2->length);
                broadcast_data_sync.frame_size = frame_len;
                break;
            default:
                break;
            }
            i += p2->length + sizeof(u8);
        }
        broadcast_data_sync.nch = 1;
        broadcast_data_sync.coding_type = AUDIO_CODING_LC3;
        update_receiver_big_codec_params(&broadcast_data_sync);
        need_deal_flag = 1;
    }
#if 0
    const u8 *const temp_buf = buf;
    if (memcmp(&broadcast_data_sync, temp_buf, sizeof(struct broadcast_sync_info))) {
        memcpy(&broadcast_data_sync, temp_buf, sizeof(struct broadcast_sync_info));
        if (!broadcast_data_sync.sample_rate ||
            !broadcast_data_sync.frame_size ||
            !broadcast_data_sync.bit_rate) {
            broadcast_data_sync.sample_rate = 0;
            broadcast_data_sync.frame_size = 0;
            broadcast_data_sync.bit_rate = 0;
            /* log_error("broadcast codec params sync err\n"); */
            ret = -EINVAL;
        } else {
            update_receiver_big_codec_params(&broadcast_data_sync);
        }
        need_deal_flag = 1;
    }
}
if (need_deal_flag)
{
    big_event_to_user(BIG_EVENT_PADV_DATA_SYNC, (void *)&broadcast_data_sync, sizeof(struct broadcast_sync_info));
}
#endif

return ret;
}

/* ***************************************************************************/
/**
 * @brief open broadcast as receiver
 *
 * @return available_big_hdl:success
 */
/* *****************************************************************************/
int broadcast_receiver(big_parameter_t *params)
{
    u8 i;
    int ret;

    if (broadcast_role == BROADCAST_ROLE_TRANSMITTER) {
        log_error("broadcast_role err");
        return -EPERM;
    }

    if (broadcast_num >= BIG_MAX_NUMS) {
        log_error("broadcast_num overflow");
        return -EPERM;
    }

    if (!broadcast_init_flag) {
        return -EPERM;
    }

    u8 available_big_hdl = get_available_big_hdl(++g_big_hdl, &broadcast_list_head);

    //防止当接收端连接不到发送端时，le_audio_switch_ops 指针更新不到
    le_audio_switch_ops = get_broadcast_audio_sw_ops();

    log_info("--func=%s", __FUNCTION__);

    clock_alloc("le_broadcast", 12 * 1000000UL);

    set_big_hdl(BROADCAST_ROLE_RECEIVER, available_big_hdl);
    //启动广播
    ret = wireless_trans_open("big_rx", (void *)params);
    if (ret != 0) {
        log_error("wireless_trans_open fail:0x%x\n", ret);
        if (broadcast_num == 0) {
            broadcast_role = BROADCAST_ROLE_UNKNOW;
        }
        return -EPERM;
    }

    broadcast_role = BROADCAST_ROLE_RECEIVER;

#if LEA_BIG_RX_CLOSE_EDR_EN
    bt_close_discoverable_and_connectable();
#endif

    broadcast_num++;

    return available_big_hdl;
}

/* ***************************************************************************/
/**
 * @brief close broadcast function
 *
 * @param big_hdl:need closed of big_hdl
 *
 * @return 0:success
 */
/* *****************************************************************************/
int broadcast_close(u8 big_hdl)
{
    u8 i;
    u8 player_status = 0;
    u8 find = 0;
    int ret;
    struct broadcast_hdl *p, *n;
    void *recorder = 0;
    struct broadcast_rx_audio_hdl player;

    player.le_audio = 0;
    player.rx_stream = 0;

    if (!broadcast_init_flag) {
        return -EPERM;
    }

    log_info("--func=%s", __FUNCTION__);

    struct broadcast_hdl *hdl;
    broadcast_mutex_pend(&broadcast_mutex, __LINE__);
    spin_lock(&broadcast_lock);
    //广播开始关闭
    list_for_each_entry(hdl, &broadcast_list_head, entry) {
        if (hdl->big_hdl != big_hdl) {
            continue;
        }
        hdl->del = 1;
        hdl->status = BROADCAST_STATUS_STOP;
    }

    spin_unlock(&broadcast_lock);
    broadcast_mutex_post(&broadcast_mutex, __LINE__);

    if (le_audio_switch_ops && le_audio_switch_ops->play_status) {
        player_status = le_audio_switch_ops->play_status();
    }

    //关闭广播
    if (broadcast_role == BROADCAST_ROLE_RECEIVER) {
        ret = wireless_trans_close("big_rx", &big_hdl);
        if (ret != 0) {
            log_error("wireless_trans_close fail:0x%x\n", ret);
        }
    } else if (broadcast_role == BROADCAST_ROLE_TRANSMITTER) {
        ret = wireless_trans_close("big_tx", &big_hdl);
        if (ret != 0) {
            log_error("wireless_trans_close fail:0x%x\n", ret);
        }
    }

    broadcast_mutex_pend(&broadcast_mutex, __LINE__);
    spin_lock(&broadcast_lock);
    list_for_each_entry_safe(p, n, &broadcast_list_head, entry) {
        if (p->big_hdl != big_hdl) {
            continue;
        }

        list_del(&p->entry);

        if (broadcast_role == BROADCAST_ROLE_TRANSMITTER) {
            for (i = 0; i < get_bis_num(BROADCAST_ROLE_TRANSMITTER); i++) {
                if (p->bis_hdl_info[i].recorder) {
                    recorder = p->bis_hdl_info[i].recorder;
                    p->bis_hdl_info[i].recorder = NULL;
                }
                spin_unlock(&broadcast_lock);

                if (recorder) {
                    if (le_audio_switch_ops && le_audio_switch_ops->tx_le_audio_close) {
                        le_audio_switch_ops->tx_le_audio_close(recorder);
                    }
                }
                spin_lock(&broadcast_lock);
            }
        }

        if (broadcast_role == BROADCAST_ROLE_RECEIVER) {
            for (i = 0; i < get_bis_num(BROADCAST_ROLE_RECEIVER); i++) {
                if (p->bis_hdl_info[i].rx_player.le_audio) {
                    player.le_audio = p->bis_hdl_info[i].rx_player.le_audio;
                    p->bis_hdl_info[i].rx_player.le_audio = NULL;
                }

                if (p->bis_hdl_info[i].rx_player.rx_stream) {
                    player.rx_stream = p->bis_hdl_info[i].rx_player.rx_stream;
                    p->bis_hdl_info[i].rx_player.rx_stream = NULL;
                }
                spin_unlock(&broadcast_lock);

                if (player.le_audio && player.rx_stream) {
                    if (le_audio_switch_ops && le_audio_switch_ops->rx_le_audio_close) {
                        le_audio_switch_ops->rx_le_audio_close(&player);
                    }
                }
                spin_lock(&broadcast_lock);
            }
        }

        free(p);
    }
    spin_unlock(&broadcast_lock);
    broadcast_mutex_post(&broadcast_mutex, __LINE__);

    broadcast_num--;
    if (broadcast_num == 0) {
#if LEA_BIG_RX_CLOSE_EDR_EN
        if (broadcast_role == BROADCAST_ROLE_RECEIVER) {
            //恢复经典蓝牙可发现可连接
            bt_discovery_and_connectable_using_loca_mac_addr(1, 1);
        }
#endif
        broadcast_role = BROADCAST_ROLE_UNKNOW;
        clock_free("le_broadcast");
    }

#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_AURACAST_SINK_EN | LE_AUDIO_JL_AURACAST_SINK_EN)))||((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_AURACAST_SOURCE_EN | LE_AUDIO_JL_AURACAST_SOURCE_EN)))&&(LEA_BIG_FIX_ROLE==2)
    //如果广播固定是接收端，则可能是mute的情况下关闭广播接收, 关闭广播需要解mute
    u8 mute_mark = app_audio_get_mute_state(APP_AUDIO_STATE_MUSIC);
    if (mute_mark == 1) {
        //切出模式之前是mute住了，那么先解mute
        mute_mark ^= 1;
        audio_app_mute_en(mute_mark);
    }
#endif

    if (player_status == LOCAL_AUDIO_PLAYER_STATUS_PLAY) {
        if (le_audio_switch_ops && le_audio_switch_ops->local_audio_open) {
            le_audio_switch_ops->local_audio_open();
        }
    }

    return 0;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 进入配对模式接口
 *
 * @param role:进入配对模式的角色
 * @param mode:配对模式，0-广播配对，1-连接配对
 * @param pair_event_cb:配对事件回调函数
 * @param user_pair_code:非0时，底层使用该配对码配对
 *
 * @return 0:成功
 */
/* ----------------------------------------------------------------------------*/
int broadcast_enter_pair(u8 role, u8 mode, void *pair_event_cb, u32 user_pair_code)
{
    int err = -EPERM;
    if (broadcast_role != BROADCAST_ROLE_UNKNOW) {
        return -EPERM;
    }
    if (!broadcast_init_flag) {
        return -EPERM;
    }
    if (role == BROADCAST_ROLE_RECEIVER) {
        err = wireless_trans_enter_pair("big_rx", mode, pair_event_cb, user_pair_code);
    } else if (role == BROADCAST_ROLE_TRANSMITTER) {
        err = wireless_trans_enter_pair("big_tx", mode, pair_event_cb, user_pair_code);
    }

    return err;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 退出配对模式接口
 *
 * @param role:退出配对模式的角色
 *
 * @return 0:成功
 */
/* ----------------------------------------------------------------------------*/
int broadcast_exit_pair(u8 role)
{
    int err = -EPERM;
    if (broadcast_role != BROADCAST_ROLE_UNKNOW) {
        return -EPERM;
    }
    if (!broadcast_init_flag) {
        return -EPERM;
    }
    if (role == BROADCAST_ROLE_RECEIVER) {
        err = wireless_trans_exit_pair("big_rx", (void *)0);
    } else if (role == BROADCAST_ROLE_TRANSMITTER) {
        err = wireless_trans_exit_pair("big_tx", (void *)0);
    }

    return err;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief get current broadcast role
 *
 * @return broadcast role
 */
/* ----------------------------------------------------------------------------*/
u8 get_broadcast_role(void)
{
    return broadcast_role;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 初始化同步的状态数据的内容
 *
 * @param data:用来同步的数据
 */
/* ----------------------------------------------------------------------------*/
void broadcast_sync_data_init(struct broadcast_sync_info *data)
{
    memcpy(&broadcast_data_sync, data, sizeof(struct broadcast_sync_info));
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 供外部手动重置recorder模块
 *
 * @param big_hdl:recorder模块所对应的big_hdl
 *
 * @return 0:success
 */
/* ----------------------------------------------------------------------------*/
int broadcast_audio_recorder_reset(u16 big_hdl)
{
    u8 i;
    u8 bis_num = get_bis_num(BROADCAST_ROLE_TRANSMITTER);
    struct broadcast_hdl *p;
    void *recorder = 0;
    struct le_audio_stream_params params = {0};

    if (broadcast_role != BROADCAST_ROLE_TRANSMITTER) {
        return -EPERM;
    }

    broadcast_mutex_pend(&broadcast_mutex, __LINE__);
    list_for_each_entry(p, &broadcast_list_head, entry) {
        if (p->big_hdl == big_hdl) {
            //关闭原来的recorder
            for (i = 0; i < bis_num; i++) {
                spin_lock(&broadcast_lock);
                if (p->bis_hdl_info[i].recorder) {
                    recorder = p->bis_hdl_info[i].recorder;
                    p->bis_hdl_info[i].recorder = NULL;
                }
                spin_unlock(&broadcast_lock);

                if (recorder) {
                    if (le_audio_switch_ops && le_audio_switch_ops->tx_le_audio_close) {
                        le_audio_switch_ops->tx_le_audio_close(recorder);
                    }
                }
            }

            params.fmt.nch = get_big_audio_coding_nch();
            params.fmt.bit_rate = get_big_audio_coding_bit_rate();
            params.fmt.coding_type = LE_AUDIO_CODEC_TYPE;
            params.fmt.frame_dms = get_big_audio_coding_frame_duration();
            params.fmt.sdu_period = get_big_sdu_period_us();
            params.fmt.sample_rate = LE_AUDIO_CODEC_SAMPLERATE;
            params.fmt.dec_ch_mode = LEA_DEC_OUTPUT_CHANNEL;
            params.latency = get_big_tx_latency();

            //重新打开新的recorder
            for (i = 0; i < bis_num; i++) {
                if (!p->bis_hdl_info[i].recorder) {
                    params.conn = p->bis_hdl_info[i].bis_hdl;
                    if (le_audio_switch_ops && le_audio_switch_ops->tx_le_audio_open) {
                        recorder = le_audio_switch_ops->tx_le_audio_open(&params);
                    }
                    spin_lock(&broadcast_lock);
                    p->bis_hdl_info[i].recorder = recorder;
                    spin_unlock(&broadcast_lock);
                }
            }
        }
    }
    broadcast_mutex_post(&broadcast_mutex, __LINE__);

    return 0;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 供外部手动关闭recorder模块
 *
 * @param big_hdl:recorder模块所对应的big_hdl
 *
 * @return 0:success
 */
/* ----------------------------------------------------------------------------*/
int broadcast_audio_recorder_close(u16 big_hdl)
{
    u8 i;
    u8 bis_num = get_bis_num(BROADCAST_ROLE_TRANSMITTER);
    struct broadcast_hdl *p;
    void *recorder = 0;

    if (broadcast_role != BROADCAST_ROLE_TRANSMITTER) {
        return -EPERM;
    }

    broadcast_mutex_pend(&broadcast_mutex, __LINE__);
    list_for_each_entry(p, &broadcast_list_head, entry) {
        if (p->big_hdl == big_hdl) {
            for (i = 0; i < bis_num; i++) {
                spin_lock(&broadcast_lock);
                if (p->bis_hdl_info[i].recorder) {
                    recorder = p->bis_hdl_info[i].recorder;
                    p->bis_hdl_info[i].recorder = NULL;
                }
                spin_unlock(&broadcast_lock);

                if (recorder) {
                    if (le_audio_switch_ops && le_audio_switch_ops->tx_le_audio_close) {
                        le_audio_switch_ops->tx_le_audio_close(recorder);
                    }
                }
            }
        }
    }
    broadcast_mutex_post(&broadcast_mutex, __LINE__);

    return 0;
}

#endif

