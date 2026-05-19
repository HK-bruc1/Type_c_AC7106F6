/*********************************************************************************************
 *   Filename        : connected.c

 *   Description     :

 *   Author          : Weixin Liang

 *   Email           : liangweixin@zh-jieli.com

 *   Last modifiled  : 2022-12-15 14:26

 *   Copyright:(c)JIELI  2011-2022  @ , All Rights Reserved.
 *********************************************************************************************/
#include "app_config.h"
#include "system/includes.h"
#include "btstack/avctp_user.h"
#include "app_msg.h"
#include "cig.h"
#include "le_connected.h"
#include "wireless_trans_manager.h"
#include "wireless_trans.h"
#include "clock_manager/clock_manager.h"
#include "le_audio_stream.h"
#include "le_audio_player.h"
#include "le_audio_recorder.h"
#include "btstack/le/ble_api.h"
#include "app_le_connected.h"

#if (THIRD_PARTY_PROTOCOLS_SEL & HID_ISO_EN)
#include "hid_iso.h"
#endif

#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)))

#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_MONO_L) || (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_MONO_R)
#define UNICAST_SINK_CIS_NUMS               		(1)			// 单声道
#define UNICAST_SINK_CIS_LR_BUF_MAX_NUMS			0
#else
#define UNICAST_SINK_CIS_NUMS               		(2)			// 立体声
#define UNICAST_SINK_CIS_LR_BUF_MAX_NUMS			20			// 手机cis立体声的错位拼包重发容错最大次数，对应flush_timeout_C_to_P
#define UNICAST_SINK_CIS_LR_TS_ALLOW_DIFF_VAL		1000		// 立体声buf时间允许相差1ms
#endif

/**************************************************************************************************
  Macros
 **************************************************************************************************/
#define LOG_TAG             "[LE_CONNECTED]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

/*! \brief CIS丢包修复 */
#define CIS_AUDIO_PLC_ENABLE    1

#define connected_get_cis_tick_time(cis_txsync)  wireless_trans_get_last_tx_clk((connected_role & CONNECTED_ROLE_PERIP) == CONNECTED_ROLE_PERIP ? "cig_perip" : "cig_central", (void *)(cis_txsync))

/**************************************************************************************************
  Data Types
 **************************************************************************************************/

/*! \brief 广播状态枚举 */
enum {
    CONNECTED_STATUS_STOP,      /*!< 广播停止 */
    CONNECTED_STATUS_STOPPING,  /*!< 广播正在停止 */
    CONNECTED_STATUS_OPEN,      /*!< 广播开启 */
    CONNECTED_STATUS_START,     /*!< 广播启动 */
};

typedef struct {
    u32 isoIntervalUs;
    u16 cis_hdl;								// cis连接句柄
    u16 acl_hdl;								// acl连接句柄
    u16 Max_PDU_P_To_C;
    u16 Max_PDU_C_To_P;
    u8 flush_timeout;
    u8 BN_C_To_P;								// 一个iso interval里面有多少个包
    u8 BN_P_To_C;								// 一个iso interval里面有多少个包
} cis_hdl_info_t;

/*! \brief 广播结构体 */
struct connected_hdl {
    struct list_head entry; /*!< cig链表项，用于多cig管理 */
    cis_hdl_info_t cis_hdl_info[CIG_MAX_CIS_NUMS];	// cis句柄
    struct connected_rx_audio_hdl rx_player;		// 解码器
    u32 cig_sync_delay;
    const char *role_name;							// 同步名称
    void *recorder;									// 编码器
    u8 del;											// 是否已删除
    u8 cig_hdl;										// cig句柄
    u8 rx_cis_num;									// rx cis的数量
};

struct acl_recv_hdl {
    struct list_head entry;
    void (*recv_handle)(u16 conn_handle, const void *const buf, size_t length, void *priv);
};
static bool rcsp_update_flag  = false;

#if UNICAST_SINK_CIS_LR_BUF_MAX_NUMS
typedef struct __JL_CIS_LR_BUF_LIST {
    struct list_head entry;						// 组包链表项，用于链表管理
    u32 ts;										// 时间戳
    u8 index;									// 缓存index，对应multi_cis_rx_buf
    u8 l_save;									// 左声道是否存储
    u8 r_save;									// 右声道是否存储
} JL_CIS_LR_BUF_LIST;
struct list_head _cis_lr_buf_list_head = LIST_HEAD_INIT(_cis_lr_buf_list_head);	// 组包时间戳-缓存index链表
static u16 multi_cis_rx_temp_buf_len = 0; // 组包大小
static u8 *multi_cis_rx_buf[UNICAST_SINK_CIS_LR_BUF_MAX_NUMS]; // 组包buf
static u8 multi_cis_rx_buf_used[UNICAST_SINK_CIS_LR_BUF_MAX_NUMS] = {0}; // 组包buf是否已被使用
static u16 multi_cis_data_offect[UNICAST_SINK_CIS_LR_BUF_MAX_NUMS] = {0}; // 组包偏移
static u8 multi_cis_plc_flag[UNICAST_SINK_CIS_LR_BUF_MAX_NUMS] = {0}; // 组包丢包修复
static u8 multi_cis_buf_arr_count = 0; // 一般是flush_timeout_C_to_P的2倍
#endif

/**************************************************************************************************
  Static Prototypes
 **************************************************************************************************/
static int connected_dec_data_receive_handler(void *_hdl, void *data, int len);
static void connected_iso_callback(const void *const buf, size_t length, void *priv);
static void connected_perip_event_callback(const CIG_EVENT event, void *priv);
static void connected_free_cig_hdl(u8 cig_hdl);
extern void set_aclMaxPduPToC_test(uint8_t aclMaxPduPToC);

/**************************************************************************************************
  Global Variables
 **************************************************************************************************/
static DEFINE_SPINLOCK(connected_lock);
static OS_MUTEX connected_mutex;
static u8 connected_role;   /*!< 记录当前广播为接收端还是发送端 */
static u8 connected_init_flag;  /*!< 广播初始化标志 */
static u8 g_cig_hdl;        /*!< 用于cig_hdl获取 */
static u8 connected_num;    /*!< 记录当前开启了多少个cig广播 */
static u8 *transmit_buf;    /*!< 用于发送端发数 */
static struct list_head connected_list_head = LIST_HEAD_INIT(connected_list_head);	// cig链表
static struct le_audio_mode_ops *le_audio_switch_ops = NULL; /*!< 广播音频和本地音频切换回调接口指针 */
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_JL_UNICAST_SINK_EN)
static struct list_head acl_data_recv_list_head = LIST_HEAD_INIT(acl_data_recv_list_head);
u8 cig_peripheral_support_lea_profile  = 1;			// 是否支持公有的cis协议
u8 cig_peripheral_support_dongle = 1;
u8 lea_cfg_only_dongle_support = 1;
#elif (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_UNICAST_SINK_EN)
u8 cig_peripheral_support_lea_profile  = 1;			// 是否支持公有的cis协议
u8 lea_cfg_only_dongle_support = 0;
u8 cig_peripheral_support_dongle = 0;
#endif
const cig_callback_t cig_perip_cb = {
    .receive_packet_cb      = connected_iso_callback,
    .event_cb               = connected_perip_event_callback,
};

#if CIS_AUDIO_PLC_ENABLE
//丢包修复补包 解码读到 这两个byte 才做丢包处理
static unsigned char errpacket[2] = {
    0x02, 0x00
};
#endif /*CIS_AUDIO_PLC_ENABLE*/

/**************************************************************************************************
  Function Declarations
 **************************************************************************************************/
static inline void connected_mutex_pend(OS_MUTEX *mutex, u32 line)
{
    int os_ret;
    os_ret = os_mutex_pend(mutex, 0);
    if (os_ret != OS_NO_ERR) {
        log_error("%s err, os_ret:0x%x", __FUNCTION__, os_ret);
        ASSERT(os_ret != OS_ERR_PEND_ISR, "line:%d err, os_ret:0x%x", line, os_ret);
    }
}

static inline void connected_mutex_post(OS_MUTEX *mutex, u32 line)
{
    int os_ret;
    os_ret = os_mutex_post(mutex);
    if (os_ret != OS_NO_ERR) {
        log_error("%s err, os_ret:0x%x", __FUNCTION__, os_ret);
        ASSERT(os_ret != OS_ERR_PEND_ISR, "line:%d err, os_ret:0x%x", line, os_ret);
    }
}

/**
 * @brief cig事件发送到用户线程
 *
 * @param event 事件类型
 * @param value 事件消息
 * @param len 	事件消息长度
 */
int cig_event_to_user(int event, void *value, u32 len)
{
    int *evt = zalloc(sizeof(int) + len);
    ASSERT(evt);
    evt[0] = event;
    memcpy(&evt[1], value, len);
    app_send_message_from(MSG_FROM_CIG, sizeof(int) + len, (int *)evt);
    free(evt);
    return 0;
}

/**
 *	@brief 获取cig句柄
 *
 *	@param id cig句柄id
 *	@param head cig链表头
 */
static u16 get_available_cig_hdl(u8 id, struct list_head *head)
{
    struct connected_hdl *p;
    u8 hdl = id;
    if ((hdl == 0) || (hdl > 0xEF)) {
        hdl = 1;
        g_cig_hdl = 1;
    }

    connected_mutex_pend(&connected_mutex, __LINE__);
__again:
    list_for_each_entry(p, head, entry) {
        if (hdl == p->cig_hdl) {
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

    g_cig_hdl = hdl;
    connected_mutex_post(&connected_mutex, __LINE__);
    return hdl;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 初始化CIG所需的参数及流程
 *
 */
/* ----------------------------------------------------------------------------*/
void connected_init(void)
{
    log_info("--func=%s", __FUNCTION__);
    int ret;

    if (connected_init_flag) {
        return;
    }

    int os_ret = os_mutex_create(&connected_mutex);
    if (os_ret != OS_NO_ERR) {
        log_error("%s %d err, os_ret:0x%x", __FUNCTION__, __LINE__, os_ret);
        ASSERT(0);
    }

    connected_init_flag = 1;

    //初始化cis接收参数及注册回调
    ret = wireless_trans_init("cig_perip", NULL);
    if (ret != 0) {
        log_error("wireless_trans_uninit fail:0x%x\n", ret);
        connected_init_flag = 0;
    }
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 复位CIG所需的参数及流程
 *
 */
/* ----------------------------------------------------------------------------*/
void connected_uninit(void)
{
    log_info("--func=%s", __FUNCTION__);
    int ret;

    if (!connected_init_flag) {
        return;
    }

    int os_ret = os_mutex_del(&connected_mutex, OS_DEL_NO_PEND);
    if (os_ret != OS_NO_ERR) {
        log_error("%s %d err, os_ret:0x%x", __FUNCTION__, __LINE__, os_ret);
    }

    connected_init_flag = 0;

    ret = wireless_trans_uninit("cig_perip", NULL);
    if (ret != 0) {
        log_error("wireless_trans_uninit fail:0x%x\n", ret);
    }
}

/* --------------------------------------------------------------------------*/
/**
 * @brief CIG从机连接成功处理事件
 *
 * @param priv:连接成功附带的句柄参数
 *
 * @return 是否执行成功 -- 0为成功，其他为失败
 */
/* ----------------------------------------------------------------------------*/
int connected_perip_connect_deal(void *priv)
{
    u8 i;
    u8 index = 0;
    u8 find = 0;
    struct connected_hdl *connected_hdl = 0;
    cig_hdl_t *hdl = (cig_hdl_t *)priv;
    struct le_audio_stream_params params = {0};


    r_printf("connected_perip_connect_deal");
    log_info("cig_hdl:%d, acl_hdl:%d, cis_hdl:%d, Max_PDU_C_To_P:%d, Max_PDU_P_To_C:%d, BN_C_To_P:%d, BN_P_To_C:%d\n", hdl->cig_hdl, hdl->acl_hdl, hdl->cis_hdl, hdl->Max_PDU_C_To_P, hdl->Max_PDU_P_To_C, hdl->BN_C_To_P, hdl->BN_P_To_C);

    //真正连上设备后，清除BIT(7)，使外部跑转发流程
    connected_role &= ~BIT(7);

    connected_mutex_pend(&connected_mutex, __LINE__);
    list_for_each_entry(connected_hdl, &connected_list_head, entry) {
        if (connected_hdl->cig_hdl == hdl->cig_hdl) {
            find = 1;
            break;
        }
    }
    connected_mutex_post(&connected_mutex, __LINE__);

    if (!find) {
        connected_hdl = (struct connected_hdl *)zalloc(sizeof(struct connected_hdl));
        ASSERT(connected_hdl, "connected_hdl is NULL");
        connected_hdl->role_name = "cig_perip";
        connected_hdl->cig_hdl = hdl->cig_hdl;
        if (!connected_hdl->cig_sync_delay) {
            connected_hdl->cig_sync_delay = hdl->cig_sync_delay;
        }
    }

    for (i = 0; i < CIG_MAX_CIS_NUMS; i++) {
        if (!connected_hdl->cis_hdl_info[i].cis_hdl) {
            index = i;
            break;
        }
    }

    if (hdl->Max_PDU_C_To_P) {
        connected_hdl->rx_cis_num += 1;
    }
    connected_hdl->cis_hdl_info[index].acl_hdl = hdl->acl_hdl;
    connected_hdl->cis_hdl_info[index].cis_hdl = hdl->cis_hdl;
    connected_hdl->cis_hdl_info[index].flush_timeout = hdl->flush_timeout_C_to_P;
    connected_hdl->cis_hdl_info[index].isoIntervalUs = hdl->isoIntervalUs;
    connected_hdl->cis_hdl_info[index].BN_C_To_P = hdl->BN_C_To_P;
    connected_hdl->cis_hdl_info[index].BN_P_To_C = hdl->BN_P_To_C;
    connected_hdl->cis_hdl_info[index].Max_PDU_P_To_C = hdl->Max_PDU_P_To_C;
    connected_hdl->cis_hdl_info[index].Max_PDU_C_To_P = hdl->Max_PDU_C_To_P;


    log_info("rx_cis_num:%d, hdl->flush_timeout:%d, hdl->isoIntervalUs:%d\n", connected_hdl->rx_cis_num, hdl->flush_timeout_C_to_P, hdl->isoIntervalUs);


    if (get_le_audio_jl_dongle_device_type()) {
        get_decoder_params_fmt(&(params.fmt));
    } else {
        params.fmt.nch = UNICAST_SINK_CIS_NUMS;
        params.fmt.bit_rate = get_cig_audio_coding_bit_rate() * UNICAST_SINK_CIS_NUMS;
        params.fmt.frame_dms = get_cig_audio_coding_frame_duration();
        params.fmt.sdu_period = get_cig_sdu_period_us();
        params.fmt.sample_rate = get_cig_audio_coding_sample_rate();
    }
    params.fmt.coding_type = LE_AUDIO_CODEC_TYPE;
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_JL_UNICAST_SINK_EN)
    params.fmt.coding_type = le_audio_get_dongle_codec_type();
    // 需要leaudio发射源确实是否提前设置编解码格式
    ASSERT(params.fmt.coding_type, "le_audio_source need send coding_type!\n");
#endif
    params.fmt.dec_ch_mode = LEA_DEC_OUTPUT_CHANNEL;
    params.fmt.flush_timeout = hdl->flush_timeout_C_to_P;
    params.fmt.isoIntervalUs = hdl->isoIntervalUs;

    params.latency = 50 * 1000;//tx延时暂时先设置 50ms
    params.conn = hdl->cis_hdl;//使用当前路的cis_hdl
    if (get_le_audio_jl_dongle_device_type()) {
        params.service_type = LEA_SERVICE_MEDIA;
    } else {
        params.service_type = (hdl->Max_PDU_P_To_C) ? LEA_SERVICE_CALL : LEA_SERVICE_MEDIA;
    }

    g_printf("nch:%d, coding_type:0x%x, dec_ch_mode:%d, conn:%d, dms:%d, sdu_period:%d, br:%d, sr:%d\n",
             params.fmt.nch, params.fmt.coding_type, params.fmt.dec_ch_mode, params.conn, params.fmt.frame_dms, params.fmt.sdu_period, params.fmt.bit_rate, params.fmt.sample_rate);

#if UNICAST_SINK_CIS_LR_BUF_MAX_NUMS
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_JL_UNICAST_SINK_EN)
    if (hdl->flush_timeout_C_to_P > 1) {
#else
    if (connected_hdl->rx_cis_num > 1 && hdl->flush_timeout_C_to_P > 1) {
#endif
        log_info("cis perip conn, flush_timeout > 1");
        clock_alloc("le_connected_lr", 80 * 1000000UL);
        spin_lock(&connected_lock);
        multi_cis_buf_arr_count = hdl->flush_timeout_C_to_P * 2; // 只对重发周期内的数据进行错位组包
        ASSERT(multi_cis_buf_arr_count < UNICAST_SINK_CIS_LR_BUF_MAX_NUMS);
        for (u8 m = 0; m < multi_cis_buf_arr_count; m++) {
            if (multi_cis_rx_buf[m]) {
                free(multi_cis_rx_buf[m]);
                multi_cis_rx_buf[m] = 0;
            }
            if (!multi_cis_rx_buf[m]) {
                multi_cis_rx_temp_buf_len = params.fmt.bit_rate * params.fmt.frame_dms / 8 / 1000 / 10;
                multi_cis_rx_buf[m] = zalloc(multi_cis_rx_temp_buf_len);
                printf("multi_cis_rx_temp_buf_len:%d, m:%d, BN_C_To_P:%d, 0x%x\n", multi_cis_rx_temp_buf_len, m, hdl->BN_C_To_P, (unsigned int)multi_cis_rx_buf[m]);
                ASSERT(multi_cis_rx_buf[m], "multi_cis_rx_buf is NULL");
            }
        }
        spin_unlock(&connected_lock);
    }
#endif

#if (THIRD_PARTY_PROTOCOLS_SEL & HID_ISO_EN)
    if (get_hid_iso_conn_hdl_en(hdl->cig_hdl)) {
        goto __hid_skip;
    }
#endif
    if (!le_audio_player_is_playing()) {

        le_audio_switch_ops = get_connected_audio_sw_ops();

        //关闭本地音频播放
        if (le_audio_switch_ops && le_audio_switch_ops->local_audio_close) {
            le_audio_switch_ops->local_audio_close();
        }

        //TODO:开启播放器播放远端传来的音频
        if (le_audio_switch_ops && le_audio_switch_ops->rx_le_audio_open) {
            le_audio_switch_ops->rx_le_audio_open(&connected_hdl->rx_player, &params);
        }
    }

    if (!le_audio_mic_recorder_is_running()) {
        //phone call
        if (!get_le_audio_jl_dongle_device_type() && hdl->Max_PDU_P_To_C) {
            //发数中间临时缓存buffer
            if (transmit_buf == NULL) {
                transmit_buf = zalloc(get_cig_transmit_data_len() * hdl->BN_P_To_C);    //by haibin--0624
                ASSERT(transmit_buf, "transmit_buf is NULL");
            }
            params.service_type = LEA_SERVICE_CALL;
            if (!connected_hdl->recorder) {
#if TCFG_AUDIO_BIT_WIDTH
                params.latency = 80 * 1000;//tx延时暂时先设置 80ms
#else
                params.latency = 80 * 1000;//tx延时暂时先设置 50ms：三星手机单mic+ANS设置50ms会有上行无声的问题，这里改成80ms
#endif
                if (le_audio_switch_ops && le_audio_switch_ops->tx_le_audio_open) {
                    connected_hdl->recorder = le_audio_switch_ops->tx_le_audio_open(&params);
                }
            }

        }
    }


__hid_skip:
    connected_mutex_pend(&connected_mutex, __LINE__);
    if (!find) {
        spin_lock(&connected_lock);
        list_add_tail(&connected_hdl->entry, &connected_list_head);
        spin_unlock(&connected_lock);
    }
    connected_mutex_post(&connected_mutex, __LINE__);
    return 0;
}

#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_JL_UNICAST_SINK_EN)
/**
 * @brief 私有cis命令开启或者关闭编码器
 *
 * @param en 开启/关闭解码器
 * @param acl_hdl acl链路句柄
 */
void connected_perip_connect_recoder(u8 en, u16 acl_hdl)
{
    int i;
    struct connected_hdl *p;
    u8 _bn_p_to_c = 0;
    le_audio_switch_ops = get_connected_audio_sw_ops();
    void *recorder = NULL;
    if (en) {
        r_printf("connected_perip_connect_recoder open\n");
        struct le_audio_stream_params params = {0};
        get_encoder_params_fmt(&params.fmt);
        params.latency = 50 * 1000;//tx延时暂时先设置 50ms
        params.fmt.coding_type = le_audio_get_dongle_codec_type();
        // 需要leaudio发射源确实是否提前设置编解码格式
        ASSERT(params.fmt.coding_type, "le_audio_source need send coding_type!\n");
        params.fmt.dec_ch_mode = LEA_DEC_OUTPUT_CHANNEL;
        params.service_type = LEA_SERVICE_CALL;
        connected_mutex_pend(&connected_mutex, __LINE__);
        spin_lock(&connected_lock);
        list_for_each_entry(p, &connected_list_head, entry) {
            for (i = 0; i < CIG_MAX_CIS_NUMS; i++) {
                if (p->cis_hdl_info[i].acl_hdl == acl_hdl) {
                    params.fmt.flush_timeout = p->cis_hdl_info[i].flush_timeout;
                    params.fmt.isoIntervalUs = p->cis_hdl_info[i].isoIntervalUs;
                    params.conn = p->cis_hdl_info[i].cis_hdl;
                    _bn_p_to_c = p->cis_hdl_info[i].BN_P_To_C;
                }
            }
        }
        spin_unlock(&connected_lock);
        connected_mutex_post(&connected_mutex, __LINE__);
        if (!_bn_p_to_c) {
            r_printf("connected_perip_connect_recoder fail, Maybe cis is closed!\n");
            return;
        }
        if (transmit_buf == NULL) {
            transmit_buf = zalloc(get_cig_transmit_data_len() * _bn_p_to_c);    //by haibin--0624
            ASSERT(transmit_buf, "transmit_buf is NULL, BN_P_To_C:%d\n", _bn_p_to_c);
        }
        if (le_audio_switch_ops && le_audio_switch_ops->tx_le_audio_open) {
            recorder = le_audio_switch_ops->tx_le_audio_open(&params);
        }
    } else {
        r_printf("connected_perip_connect_recoder close\n");
    }
    connected_mutex_pend(&connected_mutex, __LINE__);
    spin_lock(&connected_lock);
    list_for_each_entry(p, &connected_list_head, entry) {
        for (i = 0; i < CIG_MAX_CIS_NUMS; i++) {
            if (p->cis_hdl_info[i].acl_hdl == acl_hdl) {
                if (en) {
                    if (!p->recorder) {
                        p->recorder = recorder;
                    }
                } else {
                    if (le_audio_switch_ops && le_audio_switch_ops->tx_le_audio_close) {
                        if (p->recorder) {
                            spin_unlock(&connected_lock);
                            le_audio_switch_ops->tx_le_audio_close(p->recorder);
                            spin_lock(&connected_lock);
                            p->recorder = NULL;

                        }
                    }
                }
            }
        }
    }
    spin_unlock(&connected_lock);
    connected_mutex_post(&connected_mutex, __LINE__);
}

int connected_iso_recv_handle_register(void *priv, void (*recv_handle)(u16 conn_handle, const void *const buf, size_t length, void *priv))
{
    struct acl_recv_hdl *item = NULL;
    if (recv_handle) {
        item = zalloc(sizeof(struct acl_recv_hdl));
        if (NULL == item) {
            printf("err: %s alloc fail\n", __func__);
            return -1;
        }
        item->recv_handle = recv_handle;
        list_add_tail(&item->entry, &acl_data_recv_list_head);
    }

    /* rcsp_update_flag = 1; */
    return 0;
}

static void connected_iso_recv_handle(u16 conn_handle, const void *const buf, size_t length, void *priv)
{
    struct acl_recv_hdl *item = NULL;

    if (rcsp_update_flag) {
        //set rcsp acl param
        printf("Set rcsp acl param:0x%x", conn_handle);
        set_aclMaxPduPToC_test(255);
        ble_op_set_rxmaxbuf(conn_handle, 255);
        rcsp_update_flag = 0;
    }

    list_for_each_entry(item, &acl_data_recv_list_head, entry) {
        item->recv_handle(conn_handle, buf, length, priv);
    }
}
#endif

/* --------------------------------------------------------------------------*/
/**
 * @brief CIG从机连接断开成功处理接口口
 *
 * @param priv:断链成功附带的句柄参数
 *
 * @return 是否执行成功 -- 0为成功，其他为失败
 */
/* ----------------------------------------------------------------------------*/
int connected_perip_disconnect_deal(void *priv)
{
    u8 i, index;
    u8 cis_connected_num = 0;
    int player_status = 0;
    struct connected_hdl *p, *connected_hdl;
    cig_hdl_t *hdl = (cig_hdl_t *)priv;
    void *recorder = 0;
    struct connected_rx_audio_hdl player = {0};

    player.le_audio = 0;
    player.rx_stream = 0;

    log_info("connected_perip_disconnect_deal");
    log_info("%s, cig_hdl:%d", __FUNCTION__, hdl->cig_hdl);

    //TODO:获取播放状态
    if (le_audio_switch_ops && le_audio_switch_ops->play_status) {
        player_status = le_audio_switch_ops->play_status();
    }

    connected_mutex_pend(&connected_mutex, __LINE__);
    spin_lock(&connected_lock);
    list_for_each_entry(p, &connected_list_head, entry) {
        /* log_info("%s, cig_hdl:%x  %x", __FUNCTION__, hdl->cig_hdl, p->cig_hdl); */
        if (p && (p->cig_hdl == hdl->cig_hdl)) {
            for (i = 0; i < CIG_MAX_CIS_NUMS; i++) {
                /* log_info("%s, cis_hdl:%x  %x", __FUNCTION__, p->cis_hdl_info[i].cis_hdl,  hdl->cis_hdl); */
                p->cis_hdl_info[i].cis_hdl = 0xff;
                if (p->recorder) {
                    recorder = p->recorder;
                    p->recorder = NULL;
                }

                if (p->rx_player.le_audio) {
                    player.le_audio = p->rx_player.le_audio;
                    p->rx_player.le_audio = NULL;
                }

                if (p->rx_player.rx_stream) {
                    player.rx_stream = p->rx_player.rx_stream;
                    p->rx_player.rx_stream = NULL;
                }
                index = i;
            }
            p->rx_cis_num = 0;

            spin_unlock(&connected_lock);

            if (recorder) {
                //TODO:关闭recorder
                if (le_audio_switch_ops && le_audio_switch_ops->tx_le_audio_close) {
                    le_audio_switch_ops->tx_le_audio_close(recorder);
                }
            }
            if (player.le_audio && player.rx_stream) {
                //TODO:关闭player
                if (le_audio_switch_ops && le_audio_switch_ops->rx_le_audio_close) {
                    le_audio_switch_ops->rx_le_audio_close(&player);
                }
#if UNICAST_SINK_CIS_LR_BUF_MAX_NUMS
                if (hdl->flush_timeout_C_to_P > 1) {
                    log_info("cis perip disconn, flush_timeout > 1");
                    clock_free("le_connected_lr");
                    for (u8 m = 0; m < UNICAST_SINK_CIS_LR_BUF_MAX_NUMS; m++) {
                        if (multi_cis_rx_buf[m]) {
                            free(multi_cis_rx_buf[m]);
                            multi_cis_rx_buf[m] = 0;
                        }
                        multi_cis_rx_buf_used[m] = 0;
                        multi_cis_plc_flag[m] = 0;
                        multi_cis_data_offect[m] = 0;
                    }
                    JL_CIS_LR_BUF_LIST *list_buf_1 = NULL, *list_buf_2 = NULL;
                    list_for_each_entry_safe(list_buf_1, list_buf_2, &_cis_lr_buf_list_head, entry) {
                        if (list_buf_1) {
                            list_del(&list_buf_1->entry);
                            free(list_buf_1);
                        }
                    }
                    multi_cis_buf_arr_count = 0;
                }
#endif
            }

            spin_lock(&connected_lock);

            memset(&p->cis_hdl_info[index], 0, sizeof(cis_hdl_info_t));

            connected_hdl = p;
            //break;
        }
    }

    spin_unlock(&connected_lock);
    connected_mutex_post(&connected_mutex, __LINE__);


    if (!cis_connected_num) {

        connected_role |= BIT(7);   //断开连接后，或上BIT(7)，防止外部流程判断错误

        connected_hdl->cig_sync_delay = 0;

        //recorder 关闭之后发数中间临时缓存buffer释放
        if (transmit_buf) {
            free(transmit_buf);
            transmit_buf = NULL;
        }

        connected_free_cig_hdl(hdl->cig_hdl);

        if (player_status == LOCAL_AUDIO_PLAYER_STATUS_PLAY) {
            //TODO:打开本地音频
            if (le_audio_switch_ops && le_audio_switch_ops->local_audio_open) {
                le_audio_switch_ops->local_audio_open();
            }
        }
    }

    return 0;
}

/**
 *	@brief 蓝牙取数回调接口
 *
 *	@param cig_hdl cig句柄
 */
static int connected_tx_align_data_handler(u8 cig_hdl)
{
    struct connected_hdl *connected_hdl = 0;
    cis_hdl_info_t *cis_hdl_info;
    u32 timestamp = 0;
    cis_txsync_t txsync;
    int rlen = 0, i, j, k;
    int err;
    u8 capture_send_update = 0;
    u8 packet_num;
    u16 single_ch_trans_data_len;
    void *L_buffer, *R_buffer;
    void *last_recorder;
    cig_stream_param_t param = {0};

    spin_lock(&connected_lock);
    list_for_each_entry(connected_hdl, &connected_list_head, entry) {
        if (connected_hdl->cig_hdl != cig_hdl) {
            continue;
        }

        if (connected_hdl->del) {
            continue;
        }

        if (!connected_hdl->recorder) {
            continue;
        }

        for (i = 0; i < CIG_MAX_CIS_NUMS; i++) {
            cis_hdl_info = &connected_hdl->cis_hdl_info[i];
            /* printf("c:%d, i:%d, bn:%d\n", cis_hdl_info->cis_hdl, i, cis_hdl_info->BN_P_To_C, cis_hdl_info->Max_PDU_P_To_C); */
            if (cis_hdl_info->cis_hdl && cis_hdl_info->BN_P_To_C && cis_hdl_info->Max_PDU_P_To_C) {

                if (connected_hdl->recorder && (connected_hdl->recorder != last_recorder)) {
                    last_recorder = connected_hdl->recorder;
                    txsync.cis_hdl = cis_hdl_info->cis_hdl;
                    connected_get_cis_tick_time(&txsync);
                    timestamp = (txsync.tx_ts + connected_hdl->cig_sync_delay +
                                 get_cig_mtl_time() * 1000L + get_cig_sdu_period_us()) & 0xfffffff;
                }

                if (timestamp) {
                    rlen = le_audio_stream_tx_data_handler(connected_hdl->recorder, transmit_buf, get_cig_transmit_data_len() * cis_hdl_info->BN_P_To_C, timestamp, 0);
                }
                if (!rlen) {
                    putchar('^');
                    memset(transmit_buf, 0, get_cig_transmit_data_len() * cis_hdl_info->BN_P_To_C);
                    /* printf("c:%d, n\n", cis_hdl_info->cis_hdl); */
                }
                int offset = 0;
                for (k = 0; k < cis_hdl_info->BN_P_To_C; k++) {
                    param.cis_hdl = cis_hdl_info->cis_hdl;
                    err = wireless_trans_transmit((connected_role & CONNECTED_ROLE_PERIP) == CONNECTED_ROLE_PERIP ? "cig_perip" : "cig_central", transmit_buf + offset, get_cig_transmit_data_len(), &param);
                    if (err != 0) {
                        log_error("wireless_trans_transmit fail\n");
                    }
                    offset += get_cig_transmit_data_len();
                }
            }
        }
    }
    spin_unlock(&connected_lock);

    return 0;
}

void le_audio_phone_connect_profile_begin(uint16_t con_handle)
{
    r_printf("le_audio_phone_connect_profile_begin");
    cig_event_to_user(CIG_EVENT_PHONE_CONNECT, (void *)&con_handle, 2);
}

void le_audio_phone_connect_profile_disconnect(uint16_t con_handle)
{
    r_printf("le_audio_phone_connect_profile_disconnect");
    cig_event_to_user(CIG_EVENT_PHONE_DISCONNECT, (void *)&con_handle, 2);
}

/**
 *	cis事件回调
 */
static void connected_perip_event_callback(const CIG_EVENT event, void *priv)
{
    /* log_info("--func=%s, %d", __FUNCTION__, event); */

    /* g_printf("connected_perip_event_callback=%x\n", event); */
    switch (event) {
    //cis接收端连接成功后回调事件
    case CIG_EVENT_CIS_CONNECT:
        log_info("%s, CIG_EVENT_CIS_CONNECT\n", __FUNCTION__);
        cig_event_to_user(CIG_EVENT_PERIP_CONNECT, priv, sizeof(cig_hdl_t));
        break;

    //cis接收端断开成功后回调事件
    case CIG_EVENT_CIS_DISCONNECT:
        log_info("%s, CIG_EVENT_CIS_DISCONNECT\n", __FUNCTION__);
        cig_event_to_user(CIG_EVENT_PERIP_DISCONNECT, priv, sizeof(cig_hdl_t));
        break;

    case CIG_EVENT_ACL_CONNECT:
        log_info("%s, CIG_EVENT_ACL_CONNECT\n", __FUNCTION__);
        cig_event_to_user(event, priv, sizeof(cis_acl_info_t));
        rcsp_update_flag = 1;
        break;

    case CIG_EVENT_ACL_DISCONNECT:
        log_info("%s, CIG_EVENT_ACL_DISCONNECT\n", __FUNCTION__);
        cig_event_to_user(event, priv, sizeof(cis_acl_info_t));
        break;

    //蓝牙取数发射回调事件
    case CIG_EVENT_TRANSMITTER_ALIGN:
        /* WARNING:该事件为中断函数回调, 不要添加过多打印 */
        u8 cig_hdl = *((u8 *)priv);
        connected_tx_align_data_handler(cig_hdl);
        break;

    default:
        break;
    }
}

#if UNICAST_SINK_CIS_LR_BUF_MAX_NUMS
static void connected_reset_multi_cis_info(u8 index, JL_CIS_LR_BUF_LIST *list_buf)
{
    multi_cis_data_offect[index] = 0;
    multi_cis_plc_flag[index] = 0;
    multi_cis_rx_buf_used[index] = 0;
    if (list_buf) {
        list_del(&list_buf->entry);
        free(list_buf);
    }
}
/**
 * @brief 发送需要发送的早期err数据
 * 			这里的err数据：泛指后续已经组成了lr的数据包，当起码还有没有组成lr的数据包
 * 			这里就直接当做错包push给音频流了
 *
 * @param list_buf 一定是即将发送的数据包，要把这个数据前面未发送的数据包发送
 * @param rx_stream
 */
static void connected_send_first_in_lr_err_buf(JL_CIS_LR_BUF_LIST *list_buf, void *rx_stream)
{
    if (!rx_stream || !list_buf) {
        ASSERT(0);
        return;
    }
    JL_CIS_LR_BUF_LIST *list_buf_1 = NULL, *list_buf_2 = NULL;
    u8 index = 0;
    list_for_each_entry_safe(list_buf_1, list_buf_2, &_cis_lr_buf_list_head, entry) {
        // 先进先出
        if (list_buf_1 != list_buf) {
            index = list_buf_1->index;
            if (multi_cis_rx_buf[index]) {
                memcpy(multi_cis_rx_buf[index], errpacket, 2);
                /* printf("RE:%d\n", list_buf_1->ts); */
                le_audio_stream_rx_frame(rx_stream, (void *)multi_cis_rx_buf, 2, list_buf_1->ts);
                connected_reset_multi_cis_info(index, list_buf_1);
            }
        } else {
            break;
        }
    }
}
#endif

/*
 * CIS通道收到数据回连到上层处理
 * */
static void connected_iso_callback(const void *const buf, size_t length, void *priv)
{
    u8 i = 0;
    bool plc_flag = 0;
    cig_stream_param_t *param;
    struct connected_hdl *hdl;

    param = (cig_stream_param_t *)priv;

    if (param->acl_hdl) {
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_JL_UNICAST_SINK_EN)
        //收取同步数据
        connected_iso_recv_handle(param->acl_hdl, buf, length, priv);
#endif
        /* log_error("%s, %d\n", __FUNCTION__, __LINE__); */
        return;
    }

    spin_lock(&connected_lock);
    list_for_each_entry(hdl, &connected_list_head, entry) {
        if (hdl->del) {
            log_error("%s, %d\n", __FUNCTION__, __LINE__);
            continue;
        }
#if (THIRD_PARTY_PROTOCOLS_SEL & HID_ISO_EN)
        if (get_hid_iso_conn_hdl_en(hdl->cig_hdl)) {
            if (length) {
                hid_connected_iso_callback(buf, length, priv);
            }
            continue;
        }
#endif
        if ((!hdl->rx_player.le_audio) || (!hdl->rx_player.rx_stream)) {
            log_error("%s, %d\n", __FUNCTION__, __LINE__);
            continue;
        }

#if CIS_AUDIO_PLC_ENABLE
        if (length == 0) {
            plc_flag = 1;
            /* log_error("SDU empty"); */
            putchar('z');
        }
#endif//CIS_AUDIO_PLC_ENABLE

        for (i = 0; i < hdl->rx_cis_num; i++) {
            if (hdl->cis_hdl_info[i].cis_hdl == param->cis_hdl) {
                break;
            }
            if (i == (hdl->rx_cis_num - 1)) {
                continue;
            }
        }

        /* printf("r1[%d][%d][%d]\n", i, param->cis_hdl, param->ts); */
#if UNICAST_SINK_CIS_LR_BUF_MAX_NUMS
        if (hdl->cis_hdl_info[i].flush_timeout > 1) {
            u8 is_left = (i == 0) ? 1 : 0;
            if (!multi_cis_buf_arr_count) {
                // 只允许接收立体声数据
                continue;
            }
            // UNICAST_SINK_CIS_LR_BUF_MAX_NUMS's code description
            // 1、根据时间戳去找到链表中对应的buf index
            // 		时间戳容错需要在UNICAST_SINK_CIS_LR_TS_ALLOW_DIFF_VAL之内
            // 2、检查缓存中是否存入lr数据
            // 		lr数据都缓存完毕则push到audio
            // 3、丢弃早期的lr没存够的buf到丢包修复
            u8 index = 0, find = 0;
            JL_CIS_LR_BUF_LIST *list_buf = NULL;
            list_for_each_entry(list_buf, &_cis_lr_buf_list_head, entry) {
                u32 diff_val = ABS_DIFF(param->ts, list_buf->ts);
                if (diff_val < UNICAST_SINK_CIS_LR_TS_ALLOW_DIFF_VAL) {
                    find = 1;
                    index = list_buf->index;
                    break;
                }
            }
            if (!find && multi_cis_buf_arr_count) {
                // 寻找空闲的组包buf及index
                for (i = 0; i < multi_cis_buf_arr_count; i++) {
                    if (!multi_cis_rx_buf_used[i]) {
                        index = i;
                        multi_cis_rx_buf_used[index] = 1;
                        break;
                    }
                    if (i == multi_cis_buf_arr_count) {
                        // 需要检查为什么multi_cis_rx_buf_used没有被释放
                        ASSERT(0, "need to checkout multi_cis_buf_arr_count\n");
                    }
                }
                list_buf = (JL_CIS_LR_BUF_LIST *)zalloc(sizeof(JL_CIS_LR_BUF_LIST));
                ASSERT(list_buf, "list_buf is NULL");
                list_buf->ts = param->ts;
                list_buf->index = index;
                list_add_tail(&list_buf->entry, &_cis_lr_buf_list_head);
            }
            if (plc_flag || multi_cis_plc_flag[index]) {
                if (multi_cis_rx_buf[index]) {
                    multi_cis_plc_flag[index] = 1;
                    if (is_left) {
                        list_buf->l_save = 1;
                    } else {
                        list_buf->r_save = 1;
                    }
                    /* printf("r[%d][%d][%d][%d]\n", index, (int)length, list_buf->l_save, list_buf->r_save); */
                    if (list_buf->l_save && list_buf->r_save) {
                        // 查找有没有老的数据没有发出去的
                        connected_send_first_in_lr_err_buf(list_buf, hdl->rx_player.rx_stream);
                        memcpy(multi_cis_rx_buf[index], errpacket, 2);
                        /* putchar('r'); */
                        /* printf("r2:%d\n", list_buf->ts); */
                        le_audio_stream_rx_frame(hdl->rx_player.rx_stream, (void *)multi_cis_rx_buf, 2, list_buf->ts);
                        connected_reset_multi_cis_info(index, list_buf);
                    }

                    // 刚创建音频流，只有一路cis的数据，且不断申请内存，防止内存泄漏，需要把早期数据丢掉并释放内存
                    // u8 cis_early_data_num = 0;   // 早期申请内存的次数
                    // list_for_each_entry(list_buf, &_cis_lr_buf_list_head, entry) {
                    //     cis_early_data_num++;
                    // }
                    // if (cis_early_data_num > UNICAST_SINK_CIS_LR_BUF_MAX_NUMS) {
                    //     /* printf("r4:%d, %d\n", (int)length, (int)param->ts); */
                    //     connected_send_first_in_lr_err_buf(list_buf, hdl->rx_player.rx_stream);
                    // }

                } else {
                    connected_send_first_in_lr_err_buf(list_buf, hdl->rx_player.rx_stream);
                    /* putchar('r'); */
                    /* printf("r3:%d, %d\n", (int)length, (int)param->ts); */
                    le_audio_stream_rx_frame(hdl->rx_player.rx_stream, (void *)errpacket, 2, param->ts);
                    connected_reset_multi_cis_info(index, list_buf);
                }
            } else {
                if (multi_cis_rx_buf[index]) {
                    // 区分左右耳填buffer
                    if (is_left) {
                        memcpy(multi_cis_rx_buf[index], buf, length);
                        multi_cis_data_offect[index] += length;
                        list_buf->l_save = 1;
                    } else {
                        memcpy(multi_cis_rx_buf[index] + (multi_cis_rx_temp_buf_len / 2), buf, length);
                        multi_cis_data_offect[index] += length;
                        list_buf->r_save = 1;
                    }
                    /* printf("R[%d][%d][%d][%d]\n", index, multi_cis_data_offect[index], list_buf->l_save, list_buf->r_save); */
                    ASSERT(multi_cis_data_offect[index] <= multi_cis_rx_temp_buf_len);
                    if (multi_cis_data_offect[index] >= multi_cis_rx_temp_buf_len) {
                        // 查找有没有老的数据没有发出去的
                        connected_send_first_in_lr_err_buf(list_buf, hdl->rx_player.rx_stream);
                        /* put_buf((void *)multi_cis_rx_buf[index], multi_cis_rx_temp_buf_len); */
                        /* putchar('R'); */
                        /* printf("R1:%d\n", list_buf->ts); */
                        le_audio_stream_rx_frame(hdl->rx_player.rx_stream, (void *)multi_cis_rx_buf[index], multi_cis_rx_temp_buf_len, list_buf->ts);
                        /* extern uint32_t bb_le_clk_get_time_us(void); */
                        /* u32 local = bb_le_clk_get_time_us(); */
                        /* printf("[%d, %d, %d]\n", local, list_buf->ts, local - list_buf->ts); */
                        connected_reset_multi_cis_info(index, list_buf);
                    }
                } else {
                    connected_send_first_in_lr_err_buf(list_buf, hdl->rx_player.rx_stream);
                    /* putchar('R'); */
                    /* printf("R2:%d, %d\n", (int)length, (int)param->ts); */
                    le_audio_stream_rx_frame(hdl->rx_player.rx_stream, (void *)buf, length, param->ts);
                    connected_reset_multi_cis_info(index, list_buf);
                }
            }
        } else {
            if (plc_flag) {
                /* putchar('r'); */
                /* printf("r:%d, %d\n", (int)length, (int)param->ts); */
                le_audio_stream_rx_frame(hdl->rx_player.rx_stream, (void *)errpacket, 2, param->ts);
            } else {
                /* putchar('R'); */
                /* printf("R:%d, %d\n", (int)length, (int)param->ts); */
                le_audio_stream_rx_frame(hdl->rx_player.rx_stream, (void *)buf, length, param->ts);
            }
        }
#else
        if (plc_flag) {
            /* putchar('r'); */
            /* printf("r:%d, %d\n", (int)length, (int)param->ts); */
            le_audio_stream_rx_frame(hdl->rx_player.rx_stream, (void *)errpacket, 2, param->ts);
        } else {
            /* putchar('R'); */
            /* printf("R:%d, %d\n", (int)length, (int)param->ts); */
            le_audio_stream_rx_frame(hdl->rx_player.rx_stream, (void *)buf, length, param->ts);
        }
#endif
    }
    spin_unlock(&connected_lock);
}

/* --------------------------------------------------------------------------*/
/**
 * @brief CIG从机启动接口
 *
 * @param params:CIG从机启动所需参数
 *
 * @return 分配的cig_hdl
 */
/* ----------------------------------------------------------------------------*/
int connected_perip_open(cig_parameter_t *params)
{
    u8 i, j;
    int ret;

    connected_init();

    if (!connected_init_flag) {
        return -2;
    }

    if (connected_num >= CIG_MAX_NUMS) {
        log_error("connected_num overflow");
        return -1;
    }

    if ((connected_role & CONNECTED_ROLE_CENTRAL) == CONNECTED_ROLE_CENTRAL) {
        log_error("connected_role err");
        return -1;
    }

    g_printf("--func=%s", __FUNCTION__);
    u8 available_cig_hdl = 0xFF;

    set_cig_hdl(CONNECTED_ROLE_PERIP, available_cig_hdl);
    ret = wireless_trans_open("cig_perip", (void *)params);
    if (ret != 0) {
        log_error("wireless_trans_open fail:0x%x\n", ret);
        if (connected_num == 0) {
            connected_role = CONNECTED_ROLE_UNKNOW;
        }
        return -1;
    }
    /*
        if (transmit_buf) {
            free(transmit_buf);
        }
        transmit_buf = zalloc(get_cig_transmit_data_len());
        ASSERT(transmit_buf, "transmit_buf is NULL");
    */
    connected_role = CONNECTED_ROLE_PERIP | BIT(7);	//或上BIT(7)，防止外部流程判断错误

    connected_num++;

    //TODO:修改时钟
    clock_alloc("le_connected", 12 * 1000000UL);

    return available_cig_hdl;
}

/**
 * @brief 根据cig句柄回复上下行
 *
 * @param cig_hdl cig句柄
 * @param cig_phone_call_play 是否需要恢复上行
 */
int cis_audio_player_resume(u8 cig_hdl, u8 cig_phone_call_play)
{
    int ret = 0;
    u8 i;
    u8 find = 0;
    struct connected_hdl *connected_hdl = 0;
    struct le_audio_stream_params params = {0};

    connected_mutex_pend(&connected_mutex, __LINE__);
    list_for_each_entry(connected_hdl, &connected_list_head, entry) {
        if (connected_hdl->cig_hdl == cig_hdl) {
            find = 1;
            break;
        }
    }
    connected_mutex_post(&connected_mutex, __LINE__);

    if (!find) {
        return 0;
    }

    printf("cis_audio_player_resume\n");
    le_audio_switch_ops = get_connected_audio_sw_ops();

    for (i = 0; i < CIG_MAX_CIS_NUMS; i++) {
        if (connected_hdl->cis_hdl_info[i].cis_hdl) {
            if (get_le_audio_jl_dongle_device_type()) {
                get_decoder_params_fmt(&(params.fmt));
            } else {
                params.fmt.nch = get_cig_audio_coding_nch();
                params.fmt.bit_rate = get_cig_audio_coding_bit_rate();
                params.fmt.frame_dms = get_cig_audio_coding_frame_duration();
                params.fmt.sdu_period = get_cig_sdu_period_us();
                params.fmt.sample_rate = get_cig_audio_coding_sample_rate();
            }
            params.fmt.coding_type = LE_AUDIO_CODEC_TYPE;
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_JL_UNICAST_SINK_EN)
            params.fmt.coding_type = le_audio_get_dongle_codec_type();
            // 需要leaudio发射源确实是否提前设置编解码格式
            ASSERT(params.fmt.coding_type, "le_audio_source need send coding_type!\n");
#endif
            params.fmt.dec_ch_mode = LEA_DEC_OUTPUT_CHANNEL;
            params.fmt.flush_timeout = connected_hdl->cis_hdl_info[i].flush_timeout;
            params.fmt.isoIntervalUs = connected_hdl->cis_hdl_info[i].isoIntervalUs;
            /* params.reference_time = connected_audio_reference_time; */
            params.latency = 50 * 1000;//tx延时暂时先设置 50ms
            params.conn = connected_hdl->cis_hdl_info[i].cis_hdl;

            if (get_le_audio_jl_dongle_device_type()) {
                params.service_type = LEA_SERVICE_MEDIA;
            } else {
                params.service_type = (connected_hdl->cis_hdl_info[i].Max_PDU_P_To_C) ? LEA_SERVICE_CALL : LEA_SERVICE_MEDIA;
            }

            //TODO:开启播放器播放远端传来的音频
            if (le_audio_switch_ops && le_audio_switch_ops->rx_le_audio_open) {
                connected_mutex_pend(&connected_mutex, __LINE__);
                le_audio_switch_ops->rx_le_audio_open(&connected_hdl->rx_player, &params);

#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_JL_UNICAST_SINK_EN)
                if (cig_phone_call_play) {
                    connected_perip_connect_recoder(1, connected_hdl->cis_hdl_info[i].acl_hdl);
                }
#endif
                connected_mutex_post(&connected_mutex, __LINE__);
            }
            ret = 1;
        }
    }
    return ret;
}

/**
 * @brief 根据cig句柄关闭解码器
 *
 * @param cig_hdl cig句柄
 */
int cis_audio_player_close(u8 cig_hdl)
{
    printf("cis_audio_player_close\n");
    struct connected_hdl *p;
    struct connected_rx_audio_hdl player;
    int ret = 0;

    void *recorder = 0;
    player.le_audio = 0;
    player.rx_stream = 0;
    connected_mutex_pend(&connected_mutex, __LINE__);
    spin_lock(&connected_lock);
    list_for_each_entry(p, &connected_list_head, entry) {
        if (p->cig_hdl == cig_hdl) {
            for (int i = 0; i < CIG_MAX_CIS_NUMS; i++) {
                if (p->recorder) {
                    recorder = p->recorder;
                    p->recorder = NULL;
                }
                if (p->rx_player.le_audio) {
                    player.le_audio = p->rx_player.le_audio;
                    p->rx_player.le_audio = NULL;
                }

                if (p->rx_player.rx_stream) {
                    player.rx_stream = p->rx_player.rx_stream;
                    p->rx_player.rx_stream = NULL;
                }

                spin_unlock(&connected_lock);
                if (recorder) {
                    //TODO:关闭recorder
                    if (le_audio_switch_ops && le_audio_switch_ops->tx_le_audio_close) {
                        le_audio_switch_ops->tx_le_audio_close(recorder);
                        recorder = NULL;
                    }
                }

                if (player.le_audio && player.rx_stream) {
                    if (le_audio_switch_ops && le_audio_switch_ops->rx_le_audio_close) {
                        le_audio_switch_ops->rx_le_audio_close(&player);
                        player.le_audio = NULL;
                        player.rx_stream = NULL;
                        ret = 1;
                    }
                }

                spin_lock(&connected_lock);
            }
        }
    }
    spin_unlock(&connected_lock);
    connected_mutex_post(&connected_mutex, __LINE__);
    return ret;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 关闭对应cig_hdl的CIG连接
 *
 * @param cig_hdl:需要关闭的CIG连接的cig_hdl
 */
/* ----------------------------------------------------------------------------*/
static void connected_free_cig_hdl(u8 cig_hdl)
{
    //释放链表
    struct connected_hdl *p, *n;
    void *recorder = 0;
    struct connected_rx_audio_hdl player;

    player.le_audio = 0;
    player.rx_stream = 0;
    connected_mutex_pend(&connected_mutex, __LINE__);
    spin_lock(&connected_lock);
    list_for_each_entry_safe(p, n, &connected_list_head, entry) {
        if (p->cig_hdl != cig_hdl) {
            continue;
        }

        list_del(&p->entry);

        for (int i = 0; i < CIG_MAX_CIS_NUMS; i++) {
            p->cis_hdl_info[i].cis_hdl = 0;
            if (p->recorder) {
                recorder = p->recorder;
                p->recorder = NULL;
            }

            if (p->rx_player.le_audio) {
                player.le_audio = p->rx_player.le_audio;
                p->rx_player.le_audio = NULL;
            }

            if (p->rx_player.rx_stream) {
                player.rx_stream = p->rx_player.rx_stream;
                p->rx_player.rx_stream = NULL;
            }

            spin_unlock(&connected_lock);

            if (recorder) {
                //TODO:关闭recorder
                if (le_audio_switch_ops && le_audio_switch_ops->tx_le_audio_close) {
                    le_audio_switch_ops->tx_le_audio_close(recorder);
                    recorder = NULL;
                }
            }
            if (player.le_audio && player.rx_stream) {
                //TODO:关闭player
                if (le_audio_switch_ops && le_audio_switch_ops->rx_le_audio_close) {
                    le_audio_switch_ops->rx_le_audio_close(&player);
                    player.le_audio = NULL;
                    player.rx_stream = NULL;
                }
            }

            spin_lock(&connected_lock);
        }

        free(p);
    }
    spin_unlock(&connected_lock);
    connected_mutex_post(&connected_mutex, __LINE__);
}

/**
 * @brief 关闭对应cig_hdl的CIG连接
 *
 * @param cig_hdl:需要关闭的CIG连接的cig_hdl
 */
void connected_close(u8 cig_hdl)
{
    int player_status = 0;
    int ret;

    if (!connected_init_flag) {
        return;
    }

    log_info("--func=%s", __FUNCTION__);

    //TODO:获取播放状态
    if (le_audio_switch_ops && le_audio_switch_ops->play_status) {
        player_status = le_audio_switch_ops->play_status();
    }

    struct connected_hdl *hdl;
    connected_mutex_pend(&connected_mutex, __LINE__);
    spin_lock(&connected_lock);
    list_for_each_entry(hdl, &connected_list_head, entry) {
        if (hdl->cig_hdl != cig_hdl) {
            continue;
        }
        hdl->del = 1;
    }
    spin_unlock(&connected_lock);
    connected_mutex_post(&connected_mutex, __LINE__);

    //关闭CIG
    if ((connected_role & CONNECTED_ROLE_PERIP) == CONNECTED_ROLE_PERIP) {
        ret = wireless_trans_close("cig_perip", &cig_hdl);
        if (ret != 0) {
            log_error("wireless_trans_close fail:0x%x\n", ret);
        }
    } else if ((connected_role & CONNECTED_ROLE_CENTRAL) == CONNECTED_ROLE_CENTRAL) {
        ret = wireless_trans_close("cig_central", &cig_hdl);
        if (ret != 0) {
            log_error("wireless_trans_close fail:0x%x\n", ret);
        }
    }

    reset_cig_params();

    connected_free_cig_hdl(cig_hdl);

    connected_num--;
    if (connected_num == 0) {
        connected_uninit();
        connected_role = CONNECTED_ROLE_UNKNOW;
        clock_free("le_connected");
    }

    if (player_status == LOCAL_AUDIO_PLAYER_STATUS_PLAY) {
        //TODO:打开本地音频
        if (le_audio_switch_ops && le_audio_switch_ops->local_audio_open) {
            le_audio_switch_ops->local_audio_open();
        }
    }
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 获取当前CIG是主机还是从机
 *
 * @return CIG角色
 */
/* ----------------------------------------------------------------------------*/
u8 get_connected_role(void)
{
    return connected_role;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief acl数据发送接口
 *
 * @param acl_hdl:acl数据通道句柄
 * @param data:需要发送的数据
 * @param length:数据长度
 *
 * @return 实际发送出去的数据长度
 */
/* ----------------------------------------------------------------------------*/
int connected_send_acl_data(u16 acl_hdl, void *data, size_t length)
{
    int err = -1;
    cig_stream_param_t param = {0};
    param.acl_hdl = acl_hdl;

    if ((connected_role & CONNECTED_ROLE_CENTRAL) == CONNECTED_ROLE_CENTRAL) {
        err = wireless_trans_transmit("cig_central", data, length, &param);
    } else if ((connected_role & CONNECTED_ROLE_PERIP) == CONNECTED_ROLE_PERIP) {
        err = wireless_trans_transmit("cig_perip", data, length, &param);
    }
    if (err != 0) {
        log_error("acl wireless_trans_transmit fail:0x%x\n", err);
        err = 0;
    } else {
        err = length;
    }
    return err;
}

int connected_send_cis_data(u8 cig_hdl, void *data, size_t length)
{
    int err = -1;
    cig_stream_param_t param = {0};
    cis_hdl_info_t *cis_hdl_info;
    int i, k;
    struct connected_hdl *connected_hdl = 0;
    spin_lock(&connected_lock);
    list_for_each_entry(connected_hdl, &connected_list_head, entry) {
        if (connected_hdl->cig_hdl != cig_hdl) {
            log_error("cig = %d  -- %d\n", connected_hdl->cig_hdl, cig_hdl);
            continue;
        }
        if (connected_hdl->del) {
            log_error("del = %d \n", connected_hdl->del);
            continue;
        }
        for (i = 0; i < CIG_MAX_CIS_NUMS; i++) {
            cis_hdl_info = &connected_hdl->cis_hdl_info[i];
            log_info("c:%d, i:%d, bn:%d - %d\n", cis_hdl_info->cis_hdl, i, cis_hdl_info->BN_P_To_C, cis_hdl_info->Max_PDU_P_To_C);
            if (cis_hdl_info->cis_hdl && cis_hdl_info->BN_P_To_C && cis_hdl_info->Max_PDU_P_To_C) {
                for (k = 0; k < cis_hdl_info->BN_P_To_C; k++) {
                    param.cis_hdl = cis_hdl_info->cis_hdl;
                    if ((connected_role & CONNECTED_ROLE_CENTRAL) == CONNECTED_ROLE_CENTRAL) {
                        err = wireless_trans_transmit("cig_perip", data, length, &param);
                    } else if ((connected_role & CONNECTED_ROLE_PERIP) == CONNECTED_ROLE_PERIP) {
                        err = wireless_trans_transmit("cig_perip", data, length, &param);
                    }
                    if (err != 0) {
                        log_error("wireless_trans_transmit fail %d\n", err);
                    }
                }
            }
        }
    }
    spin_unlock(&connected_lock);

    return err;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 供外部手动初始化recorder模块
 *
 * @param cis_hdl:recorder模块所对应的cis_hdl
 */
/* ----------------------------------------------------------------------------*/
void cis_audio_recorder_reset(u16 cis_hdl)
{
    printf("cis_audio_recorder_reset\n");
    u8 i;
    struct connected_hdl *p;
    void *recorder = 0;

    connected_mutex_pend(&connected_mutex, __LINE__);
    list_for_each_entry(p, &connected_list_head, entry) {
        for (i = 0; i < CIG_MAX_CIS_NUMS; i++) {
            if (p->cis_hdl_info[i].cis_hdl == cis_hdl) {
                spin_lock(&connected_lock);
                if (p->recorder) {
                    recorder = p->recorder;
                    p->recorder = NULL;
                }
                spin_unlock(&connected_lock);

                //关闭旧的recorder
                if (recorder) {

                }

                //重新打开新的recorder
                if (!p->recorder) {

                }
                spin_lock(&connected_lock);
                p->recorder = recorder;
                spin_unlock(&connected_lock);
            }
        }
    }
    connected_mutex_post(&connected_mutex, __LINE__);
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 供外部手动关闭recorder模块
 *
 * @param cis_hdl:recorder模块所对应的cis_hdl
 */
/* ----------------------------------------------------------------------------*/
void cis_audio_recorder_close(u16 cis_hdl)
{
    printf("cis_audio_recorder_close\n");
    u8 i;
    struct connected_hdl *p;
    void *recorder = 0;

    connected_mutex_pend(&connected_mutex, __LINE__);
    list_for_each_entry(p, &connected_list_head, entry) {
        for (i = 0; i < CIG_MAX_CIS_NUMS; i++) {
            if (p->cis_hdl_info[i].cis_hdl == cis_hdl) {
                spin_lock(&connected_lock);
                if (p->recorder) {
                    recorder = p->recorder;
                    p->recorder = NULL;
                }
                spin_unlock(&connected_lock);

                //关闭旧的recorder
                if (recorder) {

                }
            }
        }
    }
    connected_mutex_post(&connected_mutex, __LINE__);
}

u8 is_cis_connected_init()
{
    log_info("connected_init_flag:%d\n", connected_init_flag);
    return connected_init_flag;
}

#endif

