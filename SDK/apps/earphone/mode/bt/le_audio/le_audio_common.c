/*********************************************************************************************
    *   Filename        : le_audio_common.c

    *   Description     :

    *   Author          : Weixin Liang

    *   Email           : liangweixin@zh-jieli.com

    *   Last modifiled  : 2023-8-18 19:09

    *   Copyright:(c)JIELI  2011-2022  @ , All Rights Reserved.
*********************************************************************************************/
#include "app_config.h"
#include "app_main.h"
#include "audio_base.h"
#include "le_broadcast.h"
#include "wireless_trans.h"

/* #if (LEA_BIG_CTRLER_TX_EN || LEA_BIG_CTRLER_RX_EN || LEA_CIG_CENTRAL_EN || LEA_CIG_PERIPHERAL_EN) */
#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_AURACAST_SINK_EN | LE_AUDIO_JL_AURACAST_SINK_EN)))||((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_AURACAST_SOURCE_EN | LE_AUDIO_JL_AURACAST_SOURCE_EN)))||((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)))
#include "le_audio_stream.h"
#include "le_audio_player.h"
#include "le_audio_recorder.h"


/**************************************************************************************************
  Static Prototypes
 **************************************************************************************************/
static int default_rx_le_audio_open(void *rx_audio, void *args);
static int default_rx_le_audio_close(void *rx_audio);
static int default_tx_le_audio_close(void *rx_audio);
static void *default_tx_le_audio_open(void *args);

/**************************************************************************************************
  Extern Global Variables
**************************************************************************************************/
extern const struct le_audio_mode_ops le_audio_a2dp_ops;
extern const struct le_audio_mode_ops le_audio_music_ops;
extern const struct le_audio_mode_ops le_audio_linein_ops;
extern const struct le_audio_mode_ops le_audio_iis_ops;
extern const struct le_audio_mode_ops le_audio_mic_ops;
extern const struct le_audio_mode_ops le_audio_spdif_ops;
extern const struct le_audio_mode_ops le_audio_fm_ops;
extern const struct le_audio_mode_ops le_audio_pc_ops;

/**************************************************************************************************
  Local Global Variables
**************************************************************************************************/
static u8 lea_product_test_name[28];
static u8 le_audio_pair_name[28];
static struct le_audio_mode_ops *broadcast_audio_switch_ops = NULL; /*!< le audio和local audio切换回调接口指针 */
static struct le_audio_mode_ops *connected_audio_switch_ops = NULL; /*!< le audio和local audio切换回调接口指针 */
const struct le_audio_mode_ops le_audio_default_ops = {
    .rx_le_audio_open = default_rx_le_audio_open,
    .rx_le_audio_close = default_rx_le_audio_close,
    .tx_le_audio_open = default_tx_le_audio_open,
    .tx_le_audio_close = default_tx_le_audio_close,
};

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/
void read_le_audio_product_name(void)
{
#if 0
    int len = syscfg_read(CFG_LEA_PRODUCET_TEST_NAME, lea_product_test_name, sizeof(lea_product_test_name));
    if (len <= 0) {
        r_printf("ERR:Can not read the product test name\n");
        return;
    }

    put_buf((const u8 *)lea_product_test_name, sizeof(lea_product_test_name));
    r_printf("product_test_name:%s", lea_product_test_name);
#endif
}

void read_le_audio_pair_name(void)
{
#if 0
    int len = syscfg_read(CFG_LEA_PAIR_NAME, le_audio_pair_name, sizeof(le_audio_pair_name));
    if (len <= 0) {
        r_printf("ERR:Can not read the le audio pair name\n");
        return;
    }

    put_buf((const u8 *)le_audio_pair_name, sizeof(le_audio_pair_name));
    y_printf("pair_name:%s", le_audio_pair_name);
#endif
}

const char *get_le_audio_product_name(void)
{
    return (const char *)lea_product_test_name;
}

const char *get_le_audio_pair_name(void)
{
    return (const char *)le_audio_pair_name;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 注册le audio和local audio切换回调接口
 *
 * @param ops:le audio和local audio切换回调接口结构体
 */
/* ----------------------------------------------------------------------------*/
static void le_audio_switch_ops_callback(void *ops)
{
    connected_audio_switch_ops = (struct le_audio_mode_ops *)ops;
}

struct le_audio_mode_ops *get_broadcast_audio_sw_ops()
{
    broadcast_audio_switch_ops = (struct le_audio_mode_ops *)&le_audio_default_ops;
    return broadcast_audio_switch_ops;
}

struct le_audio_mode_ops *get_connected_audio_sw_ops()
{
    return connected_audio_switch_ops;
}

int le_audio_ops_register(u8 mode)
{
    le_audio_switch_ops_callback((void *)&le_audio_default_ops);
    return 0;
}

int le_audio_ops_unregister(void)
{
    le_audio_switch_ops_callback(NULL);
    return 0;
}

/**
 *	@brief 解码器开启
 *
 *	@param rx_audio		broadcast_rx_audio_hdl
 *	@param args 		le_audio_stream_params
 */
static int default_rx_le_audio_open(void *rx_audio, void *args)
{
    int err;
    struct broadcast_rx_audio_hdl *rx_audio_hdl = (struct broadcast_rx_audio_hdl *)rx_audio;

    if (!rx_audio) {
        return -EPERM;
    }

    //打开广播音频播放
    struct le_audio_stream_params *params = (struct le_audio_stream_params *)args;
    rx_audio_hdl->le_audio = le_audio_stream_create(params->conn, &params->fmt);
    rx_audio_hdl->rx_stream = le_audio_stream_rx_open(rx_audio_hdl->le_audio, params->fmt.coding_type);
    err = le_audio_player_open(rx_audio_hdl->le_audio, params);
    if (err != 0) {
        ASSERT(0, "player open fail");
    }
    g_printf("default_rx_le_audio_open\n");

    return 0;
}

/**
 *	@brief 解码器关闭
 *
 *	@param rx_audio		broadcast_rx_audio_hdl
 */
static int default_rx_le_audio_close(void *rx_audio)
{
    struct broadcast_rx_audio_hdl *rx_audio_hdl = (struct broadcast_rx_audio_hdl *)rx_audio;

    if (!rx_audio) {
        return -EPERM;
    }

    r_printf("default_rx_le_audio_close");
    //关闭广播音频播放
    le_audio_player_close(rx_audio_hdl->le_audio);
    le_audio_stream_rx_close(rx_audio_hdl->rx_stream);
    le_audio_stream_free(rx_audio_hdl->le_audio);

    return 0;
}

/**
 *	@brief 编码器开启
 *
 *	@param args 		le_audio_stream_params
 *	@return le_audio_stream_context*
 */
static void *default_tx_le_audio_open(void *args)
{
    int err;
    void *le_audio = NULL;

    if (1) {//(get_mic_play_status() == LOCAL_AUDIO_PLAYER_STATUS_PLAY) {
        /* update_app_broadcast_deal_scene(BROADCAST_MUSIC_START); */
        //打开广播音频播放
        struct le_audio_stream_params *params = (struct le_audio_stream_params *)args;
        le_audio = le_audio_stream_create(params->conn, &params->fmt);
        err = le_audio_mic_recorder_open(params, le_audio, params->latency);
        if (err != 0) {
            ASSERT(0, "recorder open fail");
        }
    }

    return le_audio;
}

/**
 *	@brief 解码器开启
 *
 *	@param le_audio 	le_audio_stream_context
 */
static int default_tx_le_audio_close(void *le_audio)
{
    if (!le_audio) {
        return -EPERM;
    }
    printf("==  %s,%d\n", __func__, __LINE__);

    le_audio_mic_recorder_close();
    le_audio_stream_free(le_audio);

    /* update_app_broadcast_deal_scene(BROADCAST_MUSIC_STOP); */
    return 0;
}

#endif
