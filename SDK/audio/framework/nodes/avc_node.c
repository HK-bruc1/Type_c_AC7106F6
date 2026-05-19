#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".avc_node.data.bss")
#pragma data_seg(".avc_node.data")
#pragma const_seg(".avc_node.text.const")
#pragma code_seg(".avc_node.text")
#endif

#include "jlstream.h"
#include "media/audio_base.h"
#include "app_config.h"
#include "audio_avc.h"
#include "audio_anc_lvl_sync.h"
#include "media/audio_threshold_det.h"
#include "audio_volume_mixer.h"
#include "effects/effects_adj.h"
#include "audio_config_def.h"
#include "clock_manager/clock_manager.h"
#include "os/os_api.h"

#if TCFG_USER_TWS_ENABLE
#include "bt_tws.h"
#endif

#if TCFG_AUDIO_AVC_NODE_ENABLE

#if 1
#define avc_log printf
#else
#define avc_log(...)
#endif

#if (AVC_ALGO_RUN_TYPE == ICSD_AVC_ALGO_RUN)
#include "icsd_adt_alg.h"
#include "icsd_avc.h"
#endif
#if (AVC_ALGO_RUN_TYPE == ENERGY_DETECT_RUN)
#include "math.h"
#include "asm/math_fast_function.h"
#include "media/media_analysis.h"
#endif

#if AVC_USE_AEC
#if (AUDIO_ADC_IRQ_POINTS != 256)
#error "avc_use_aec: adc_irq_points 256"
#endif
#if (TCFG_AUDIO_GLOBAL_SAMPLE_RATE % AVC_ADC_SAMPLE_RATE)
#error "avc_use_aec: dac_sr/adc_sr error"
#endif
#include "audio_aec.h"
#include "a2dp_player.h"
#include "audio_dac.h"
#include "effects/convert_data.h"
#include "audio_splicing.h"
#include "esco_recoder.h"
#include "os/os_api.h"
#include "circular_buf.h"
#include "esco_recoder.h"

extern struct audio_adc_hdl adc_hdl;
extern struct audio_dac_hdl dac_hdl;

struct avc_aec_hdl_t {
    AEC_NLP_CONFIG aec_nlp_cfg;
    void *node_hdl;                 //保存avc节点句柄，用于aec输出回调中
    u8 *ref_tmpbuf;                 //dac回采数据
    int ref_tmpbuf_len;             //回采数据长度
    u8 *buf;                        //adc缓存数据，保存3帧
    u16 frame_points;               //一帧的点数
    u16 input_buf_points;           //输入缓存总点数
    u8 buf_cnt;                     //循环buf位置
    u8 adc_bit_width;
    u8 dac_bit_width;
    u8 ref_channel;
    u8 start;
    u8 mode;                        //aec_mode
#if AVC_ESCO_AEC_PROCESS_TASK
    OS_SEM *sem;
    cbuffer_t *cbuf;
    u8 *cbuf_data;                  //cbuf 内存
    u8 task_state;
    u8 task_busy;
#endif
};
struct avc_aec_hdl_t *avc_aec_hdl = NULL;
static int audio_avc_aec_init(void *hdl);
static int audio_avc_aec_uninit();
static void audio_avc_aec_param_update(struct avc_param_tool_set *avc_tool_param);
static int audio_avc_esco_aec_open();
static int audio_avc_esco_aec_close();
#endif

static void avc_vol_offset_set(struct vol_offset_fade_handle *hdl, s8 target_offset, int fade_time_ms);

//作为全局句柄，防止模块关闭后timer还存在
struct vol_offset_fade_handle fade_hdl = { //音量淡入句柄
    .timer_ms = 100,
    .timer_id = 0,
    .cur_offset = 0.0f,
    .fade_step = 0.0f,
    .target_offset = 0.0f,
};

#if (AVC_ALGO_RUN_TYPE == ICSD_AVC_ALGO_RUN)
//由于icsd_avc算法全局只能存在一个，故不放在hdl中
struct avc_icsd_algo_status_t {
    int open_status;
    u8 *lib_alloc_ptr;                      //icsd算法work_buf
} avc_icsd_algo_status = {0};

static int avc_icsd_algo_status_add_check(int add)
{
    avc_log(">> %s add: %d, cur_status: %d\n", __func__, add, avc_icsd_algo_status.open_status);
    avc_icsd_algo_status.open_status += add;
    return avc_icsd_algo_status.open_status;
}

static void avc_icsd_algo_open()
{
    if (avc_icsd_algo_status_add_check(1) == 1) {
        int alloc_size = icsd_adt_avc_get_libfmt(ICSD_AVC_ALGO_TYPE);
        avc_log("avc size %d\n", alloc_size);
        if (!avc_icsd_algo_status.lib_alloc_ptr) {
            avc_icsd_algo_status.lib_alloc_ptr = media_malloc(AUD_MODULE_AVC_COMMON, alloc_size);
        }
        icsd_adt_avc_set_infmt((int)(avc_icsd_algo_status.lib_alloc_ptr), ICSD_AVC_ALGO_TYPE);
    }
}

static void avc_icsd_algo_close()
{
    if (avc_icsd_algo_status_add_check(-1) == 0) {
        if (avc_icsd_algo_status.lib_alloc_ptr) {
            media_free(avc_icsd_algo_status.lib_alloc_ptr);
            avc_icsd_algo_status.lib_alloc_ptr = NULL;
        }
    }
}
#endif

struct avc_hdl {
    char name[16];                          //节点名称
    struct stream_fmt fmt;		            //节点协商参数
    OS_MUTEX mutex;
    struct avc_param_tool_set *avc_tool_param; //节点界面参数
    struct audio_anc_lvl_sync *lvl_sync_hdl;   //档位同步句柄
    void *avc_thr_hdl;                      //噪声阈值检测模块句柄
    u8 *remain_buf;
    int last_offset_dB;
    //由于音量表在中断中调用，此处使用外部buf进行保存，防止在线调试直接更新参数导致边界问题
    int cur_vol_offset_fade_time;
    int cur_vol_lvl_num;
    int cur_vol_len;
    s8 *cur_vol_table;                      //当前使用的音量表
#if ((AVC_ALGO_RUN_TYPE == ICSD_AVC_ALGO_RUN) && (!ICSD_AVC_ALGO_TYPE))
    s16 dac_data[128];                      //预留回采数据
#endif
    //拼拆包处理
    u16 remain_len;
    u16 frame_len;
    //参数更新标志位
    u8 thr_update;
    u8 vol_update; //tws中断使用
    u8 tws_conn;    //tws是否为连接状态
    volatile u8 stop;        //当使能了aec后，算法处理的输入数据不由本节点提供，可能出现节点关闭后仍推数据run的情况
};

#if AVC_USE_AEC && TCFG_USER_TWS_ENABLE
static void avc_esco_tws_add_check(struct avc_hdl *hdl);
#endif

static int avc_thr_table_len_get(int thr_lvl_num)
{
    /* avc_log("avc_thr_table_len_get %d\n", (u32)(thr_lvl_num * sizeof(int))); */
    return thr_lvl_num * sizeof(int);
}
static int avc_vol_table_len_get(int thr_lvl_num, int vol_lvl_num)
{
    /* avc_log("avc_vol_table_len_get %d\n", (u32)(vol_lvl_num * (thr_lvl_num * sizeof(s8) + 1))); */
    return (vol_lvl_num * (thr_lvl_num * sizeof(s8) + 1));
}

static void avc_tool_param_get(struct avc_hdl *hdl, u8 *data_buf)
{
    if (!(hdl && hdl->avc_tool_param)) {
        return;
    }
    struct avc_tool_common_param *cfg = (struct avc_tool_common_param *)data_buf;
    if ((cfg->thr_lvl_num != hdl->avc_tool_param->common_param.thr_lvl_num) || \
        (cfg->vol_lvl_num != hdl->avc_tool_param->common_param.vol_lvl_num)) {
        if (hdl->avc_tool_param->table_param.thr_table) {
            media_free(hdl->avc_tool_param->table_param.thr_table);
            hdl->avc_tool_param->table_param.thr_table = NULL;
        }
        if (hdl->avc_tool_param->table_param.vol_table) {
            media_free(hdl->avc_tool_param->table_param.vol_table);
            hdl->avc_tool_param->table_param.vol_table = NULL;
        }
    }
    //common_param
    memcpy(&hdl->avc_tool_param->common_param, data_buf, sizeof(struct avc_tool_common_param));
    if (hdl->avc_tool_param->common_param.default_lvl > hdl->avc_tool_param->common_param.thr_lvl_num) {
        hdl->avc_tool_param->common_param.default_lvl = hdl->avc_tool_param->common_param.thr_lvl_num; //默认档位不超过最大档位
    }

    //噪声阈值表获取
    u16 *data_buf_table = (u16 *)(data_buf + sizeof(struct avc_tool_common_param));
    hdl->avc_tool_param->table_param.thr_len = data_buf_table[0];
    if (hdl->avc_tool_param->table_param.thr_len !=
        avc_thr_table_len_get(hdl->avc_tool_param->common_param.thr_lvl_num)) {
        //长度检查
        ASSERT(0);
    }
    avc_log("avc thr_len %d\n", hdl->avc_tool_param->table_param.thr_len);
    if (!hdl->avc_tool_param->table_param.thr_table) {
        hdl->avc_tool_param->table_param.thr_table = media_malloc(AUD_MODULE_AVC_COMMON, hdl->avc_tool_param->table_param.thr_len);
        ASSERT(hdl->avc_tool_param->table_param.thr_table);
    }
    memcpy(hdl->avc_tool_param->table_param.thr_table,
           &data_buf_table[1], hdl->avc_tool_param->table_param.thr_len);
    hdl->avc_tool_param->table_param.thr_table[0] = 0; //噪声阈值表第一个值固定为0，若配置>0，小于该值时的噪声值会被视为非法值

    //音量偏移表获取
    data_buf_table = (u16 *)((int)&data_buf_table[1] + hdl->avc_tool_param->table_param.thr_len);
    hdl->avc_tool_param->table_param.vol_len = data_buf_table[0];
    if (hdl->avc_tool_param->table_param.vol_len !=
        avc_vol_table_len_get(hdl->avc_tool_param->common_param.thr_lvl_num, hdl->avc_tool_param->common_param.vol_lvl_num)) {
        //长度检查
        ASSERT(0);
    }
    avc_log("avc vol_len %d\n", hdl->avc_tool_param->table_param.vol_len);
    if (!hdl->avc_tool_param->table_param.vol_table) {
        hdl->avc_tool_param->table_param.vol_table = media_malloc(AUD_MODULE_AVC_COMMON, hdl->avc_tool_param->table_param.vol_len);
    }
    memcpy(hdl->avc_tool_param->table_param.vol_table,
           &data_buf_table[1], hdl->avc_tool_param->table_param.vol_len);
}

static void avc_param_printf(struct avc_param_tool_set *param)
{
    avc_log("is_bypass          %d\n", param->common_param.is_bypass);
    avc_log("thr_lvl_num        %d\n", param->common_param.thr_lvl_num);
    avc_log("vol_lvl_num        %d\n", param->common_param.vol_lvl_num);
    avc_log("thr_debounce       %d\n", param->common_param.thr_debounce);
    avc_log("lvl_up_hold_time   %d\n", param->common_param.lvl_up_hold_time);
    avc_log("lvl_down_hold_time %d\n", param->common_param.lvl_down_hold_time);
    avc_log("default_lvl        %d\n", param->common_param.default_lvl);
    avc_log("high_lvl_sync      %d\n", param->common_param.high_lvl_sync);
    avc_log("aec_en             %d\n", param->common_param.aec_en);
    avc_log("nlp_en             %d\n", param->common_param.nlp_en);
    avc_log("aec_dt_aggress     %d\n", (int)(param->common_param.aec_dt_aggress * 100));
    avc_log("aec_refengthr      %d\n", (int)(param->common_param.aec_refengthr * 100));
    avc_log("es_aggress_factor  %d\n", (int)(param->common_param.es_aggress_factor * 100));
    avc_log("es_min_suppress    %d\n", (int)(param->common_param.es_min_suppress * 100));
    if (param->table_param.thr_table) {
        if (param->table_param.thr_len != avc_thr_table_len_get(param->common_param.thr_lvl_num)) {
            //thr_table长度检查
            ASSERT(0);
        }
        for (u8 i = 0; i < param->common_param.thr_lvl_num; i++) {
            avc_log("thr_table[%d]:%d\n", i, param->table_param.thr_table[i]);
        }
    }
    if (param->table_param.vol_table) {
        if (param->table_param.vol_len != avc_vol_table_len_get(param->common_param.thr_lvl_num, param->common_param.vol_lvl_num)) {
            //vol_table长度检查
            ASSERT(0);
        }
        put_buf((void *)param->table_param.vol_table, param->table_param.vol_len);
        for (u8 i = 0; i < param->common_param.vol_lvl_num; i++) {
            for (u8 j = 0; j < param->common_param.thr_lvl_num; j++) {
                u8 *vol_table = (u8 *)((int)param->table_param.vol_table + (((param->table_param.vol_len / param->common_param.vol_lvl_num) * i) + 1));
                avc_log("vol_table[%d][%d]:%d\n", i, j, vol_table[j]);
            }
        }
    }
}

static void avc_thr_to_lvl_sync(struct avc_hdl *hdl, int noise_thr)
{
    if (!(hdl && hdl->avc_thr_hdl && hdl->lvl_sync_hdl)) {
        return;
    }
    noise_thr = (noise_thr < 0) ? 0 : noise_thr;

    if (hdl->thr_update) {
        hdl->thr_update = 0;
        struct threshold_det_update_param update_param = {0};
        update_param.thr_table = hdl->avc_tool_param->table_param.thr_table;
        update_param.run_interval = (ALGO_RUN_FRAME_LEN / (hdl->fmt.bit_wide ? 4 : 2) * 1000) / hdl->fmt.sample_rate;
        update_param.lvl_up_hold_time = hdl->avc_tool_param->common_param.lvl_up_hold_time;
        update_param.lvl_down_hold_time = hdl->avc_tool_param->common_param.lvl_down_hold_time;
        update_param.thr_lvl_num = hdl->avc_tool_param->common_param.thr_lvl_num;
        update_param.thr_debounce = hdl->avc_tool_param->common_param.thr_debounce;
        audio_threshold_det_update_param(hdl->avc_thr_hdl, &update_param);
    }

    u8 avc_lvl = audio_threshold_det_run(hdl->avc_thr_hdl, noise_thr);
    //avc_lvl+1 : 1为最低档与工具对齐，实际内部buf偏移为0
#if AVC_THR_DEBUG_ENABLE
    avc_log("avc_det-avc_thr: %d, avc_lvl: %d, cur_lvl: %d\n", noise_thr, avc_lvl + 1, hdl->lvl_sync_hdl->cur_lvl + 1);
#endif

#if AVC_USE_AEC && TCFG_USER_TWS_ENABLE
    avc_esco_tws_add_check(hdl);
#endif

    if (hdl->lvl_sync_hdl->cur_lvl == avc_lvl) {
        return;
    }
    u8 data[4] = {0};
    static u32 next_period = 0;
    /*间隔200ms以上发送一次数据*/
    if (time_after(jiffies, next_period)) {
        next_period = jiffies + msecs_to_jiffies(200);
#if TCFG_USER_TWS_ENABLE
        if (tws_in_sniff_state()) {
            /*如果在蓝牙siniff下需要退出蓝牙sniff再发送*/
            tws_api_tx_unsniff_req();
        }
#if AVC_USE_AEC
        if (get_tws_sibling_connect_state() && !esco_recoder_running()) {
            //通话AVC，不走TWS档位同步流程，直接TWS同步设置音量offset
#else
        if (get_tws_sibling_connect_state()) {
#endif
            data[0] = AUDIO_ANC_LVL_SYNC_CMP;
            data[1] = avc_lvl;
            audio_anc_lvl_sync_info(hdl->lvl_sync_hdl, data, 4);
        } else {
            /*没有tws时直接更新状态*/
            data[0] = AUDIO_ANC_LVL_SYNC_RESULT;
            data[1] = avc_lvl;
            audio_anc_lvl_sync_info(hdl->lvl_sync_hdl, data, 4);
        }
#else
        data[0] = AUDIO_ANC_LVL_SYNC_RESULT;
        data[1] = avc_lvl;
        audio_anc_lvl_sync_info(hdl->lvl_sync_hdl, data, 4);
#endif //TCFG_USER_TWS_ENABLE
    }
}

//获取当前音量所处的音量档位
u8 get_vol_lvl(struct avc_hdl *hdl)
{
    s16 max_vol = app_audio_get_max_volume();
    s16 cur_vol = app_audio_get_volume(APP_AUDIO_CURRENT_STATE);
    u8 vol_lvl = 0;
    /*
     * 查找当前音量在哪个音量区间，确定vol_lvl档位
     * 该逻辑也可根据实际样机的音量表以及调试情况自行修改
     */
    for (u8 i = 0; i < hdl->cur_vol_lvl_num; i++) {
        if (cur_vol <= (max_vol * (i + 1) / hdl->cur_vol_lvl_num)) {
            vol_lvl = i;
            break;
        }
    }
    vol_lvl++; //1为最低档，与工具对齐
    return vol_lvl;
}

#if AVC_USE_AEC && TCFG_USER_TWS_ENABLE //以下为TWS通话AVC处理，直接同步音量偏移值
/*
   低8bit：target_offset
   8-24bit：fade_time
 */
static void tws_avc_vol_offset_set_hdl(int priv, int err)
{
    avc_log("tws_avc_vol_offset_set_hdl target_offset %d, fade_time_ms %d\n", (priv & 0xFF), ((priv >> 8) & 0xFFFFFF));
    audio_app_set_vol_offset_dB_fade_process_by_app_core(&fade_hdl, (float)(priv & 0xFF), (int)((priv >> 8) & 0xFFFFFF));
}

#define TWS_AVC_VOL_OFFSET_SET    		 TWS_FUNC_ID('A', 'V', 'C', 'O')
TWS_SYNC_CALL_REGISTER() = {
    .uuid = TWS_AVC_VOL_OFFSET_SET,
    .task_name = "app_core",
    .func = tws_avc_vol_offset_set_hdl,
};

static void tws_avc_vol_offset_set(s8 target_offset, int fade_time_ms)
{
    int priv = (fade_time_ms << 8) | target_offset;
    tws_api_sync_call_by_uuid(TWS_AVC_VOL_OFFSET_SET, priv, AVC_VOL_OFFSET_TWS_SYNC_TIMEOUT);
}

//单边触发时，另一边加入，此时需要直接同步音量值
static void avc_esco_tws_add_check(struct avc_hdl *hdl)
{
    if (!esco_recoder_running()) {
        return;
    }
    if (!hdl->tws_conn && get_tws_sibling_connect_state()) {
        //另一边加入，同步音量值
        if (hdl->last_offset_dB) {
            avc_log("tws add: sync last_vol_offset %d\n", hdl->last_offset_dB);
            avc_vol_offset_set(&fade_hdl, hdl->last_offset_dB, 0);
        }
    }
    hdl->tws_conn = get_tws_sibling_connect_state();
}
#endif

static void avc_vol_offset_set(struct vol_offset_fade_handle *hdl, s8 target_offset, int fade_time_ms)
{
#if AVC_USE_AEC && TCFG_USER_TWS_ENABLE
    if (esco_recoder_running()) {
        //通话：由于仅主机端打开AVC，故需要同步音量offset到从机端
        tws_avc_vol_offset_set(target_offset, fade_time_ms);
    } else {
        //音乐：主从机均打开AVC，走正常流程
        audio_app_set_vol_offset_dB_fade_process_by_app_core(hdl, (float)target_offset, fade_time_ms);
    }
#else
    audio_app_set_vol_offset_dB_fade_process_by_app_core(hdl, (float)target_offset, fade_time_ms);
#endif
}

//irq
static void avc_lvl_sync_cb(void *_hdl)
{
    struct audio_anc_lvl_sync *lvl_sync_hdl = (struct audio_anc_lvl_sync *)_hdl;
    struct avc_hdl *hdl = (struct avc_hdl *)lvl_sync_hdl->priv;
    if (!hdl || hdl->stop || !hdl->cur_vol_table || !hdl->avc_tool_param) {
        return;
    }

    u8 avc_lvl = lvl_sync_hdl->cur_lvl;
    s16 max_vol = app_audio_get_max_volume();
    s16 cur_vol = app_audio_get_volume(APP_AUDIO_CURRENT_STATE);

    if (hdl->vol_update) {
        hdl->vol_update = 0;
        if (hdl->cur_vol_len != hdl->avc_tool_param->table_param.vol_len) {
            media_free(hdl->cur_vol_table);
            hdl->cur_vol_table = media_malloc(AUD_MODULE_AVC_COMMON, hdl->cur_vol_len);
        }
        hdl->cur_vol_lvl_num = hdl->avc_tool_param->common_param.vol_lvl_num;
        hdl->cur_vol_len = hdl->avc_tool_param->table_param.vol_len;
        memcpy(hdl->cur_vol_table, hdl->avc_tool_param->table_param.vol_table, hdl->avc_tool_param->table_param.vol_len);
        hdl->cur_vol_offset_fade_time = hdl->avc_tool_param->common_param.vol_offset_fade_time;
    }

    u8 vol_lvl = get_vol_lvl(hdl);

    s8 *vol_table = (s8 *)((int)hdl->cur_vol_table +
                           (((hdl->cur_vol_len / hdl->cur_vol_lvl_num) * (vol_lvl - 1)) + 1));

    if (hdl->last_offset_dB == vol_table[avc_lvl]) {
        return;
    }
    if (lvl_sync_hdl->first_sync) {
        //单边触发后对耳加入，不需跑fade，直接设置音量offset，保证双耳音量一致
        avc_log("avc_first_sync: %d\n", vol_table[avc_lvl]);
        avc_vol_offset_set(&fade_hdl, vol_table[avc_lvl], 0);
    } else {
        avc_vol_offset_set(&fade_hdl, vol_table[avc_lvl], hdl->cur_vol_offset_fade_time);
    }
    hdl->last_offset_dB = vol_table[avc_lvl];

    //avc_lvl+1 : 1为最低档与工具对齐，实际内部buf偏移为0
    avc_log("avc_det-vol_lvl %d, noise_lvl %d, offset_dB %d\n", vol_lvl, avc_lvl + 1, hdl->last_offset_dB);
}

static void avc_run(struct avc_hdl *hdl, void *data, int data_len)
{
    if (hdl->avc_tool_param->common_param.is_bypass) {
        return;
    }
#if (AVC_ALGO_RUN_TYPE == ICSD_AVC_ALGO_RUN)
    __adt_avc_run_parm run_parm;
    __adt_avc_output output;

    run_parm.refmic = (s16 *)data;
#if !ICSD_AVC_ALGO_TYPE
    run_parm.dac_data = hdl->dac_data;
#endif
    run_parm.type = ICSD_AVC_ALGO_TYPE;
    icsd_adt_avc_run(&run_parm, &output);
    avc_thr_to_lvl_sync(hdl, (int)output.spldb_iir);

#else
    u8 nbit = hdl->fmt.bit_wide ? 24 : 16;
    short npoint_per_ch = hdl->fmt.bit_wide ? (data_len) >> 2 : (data_len) >> 1;
    int peak = hdl->fmt.bit_wide ? peak_calc_32bit((int *)data, npoint_per_ch) + 1 : peak_calc(data, npoint_per_ch) + 1;
    float dB = 20 * log10_float(peak) - 20 * log10_float((float)(1 << (nbit - 1)));
    /* avc_log("avc_run peak %d, dB:%d\n", peak, (int)dB); */
    avc_thr_to_lvl_sync(hdl, peak);
#endif

}

static void avc_pack_frame_process(struct avc_hdl *hdl, u8 *data, u16 *data_offset, u16 len)
{
    if (!hdl) {
        return;
    }
    os_mutex_pend(&hdl->mutex, 0);
    if (hdl->stop) {
        return;
    }

    int total_len = hdl->remain_len + len;  //记录目前还有多少数据
    u8 pack_frame_num = total_len / hdl->frame_len;//每次数据需要跑多少帧
    if (pack_frame_num) {
        if (hdl->remain_len) {
            int need_size = hdl->frame_len - hdl->remain_len;
            /*拷贝上一次剩余的数据*/
            memcpy((u8 *)hdl->remain_buf + hdl->remain_len, data, need_size);
            avc_run(hdl, hdl->remain_buf, hdl->frame_len);
            hdl->remain_len = 0;
            (*data_offset) += need_size;
            pack_frame_num--;
        }
        while (pack_frame_num--) {
            avc_run(hdl, (data + (*data_offset)), hdl->frame_len);
            (*data_offset) += hdl->frame_len;
        }
        if ((*data_offset) < len) {
            memcpy(hdl->remain_buf, data + (*data_offset), len - (*data_offset));
            hdl->remain_len = len - (*data_offset);
        }
    } else {
        /*当前数据不够跑一帧算法时*/
        memcpy((void *)((int)hdl->remain_buf + hdl->remain_len), data, len);
        hdl->remain_len += len;
    }
    os_mutex_post(&hdl->mutex);
}


static void avc_handle_frame(struct stream_iport *iport, struct stream_note *note)
{
    struct avc_hdl *hdl = (struct avc_hdl *)iport->node->private_data;
    struct stream_frame *frame;

    while (1) {
        //获取上一个节点的输出
        frame = jlstream_pull_frame(iport, note);
        if (!frame) {
            break;
        }
#if AVC_USE_AEC
        if (avc_aec_hdl && (avc_aec_hdl->mode == AVC_AEC_A2DP_MODE) && !avc_aec_hdl->start) {
            //start之后在aec_output调用
            avc_pack_frame_process(hdl, (u8 *)frame->data, &frame->offset, frame->len);
        }
#else
        avc_pack_frame_process(hdl, (u8 *)frame->data, &frame->offset, frame->len);
#endif
        jlstream_free_frame(frame);	//释放iport资源
    }
}

static int avc_bind(struct stream_node *node, u16 uuid)
{
    return 0;
}

static void avc_open_iport(struct stream_iport *iport)
{
}

/*
*********************************************************************
*                  avc_ioc_fmt_nego
* Description: avc 参数协商
* Arguments  : iport 输入端口句柄
* Return	 : 节点参数协商结果
* Note(s)    : 目的在于检查与上一个节点的参数是否匹配，不匹配则重新协商;
   			   根据输出节点的参数特性，区分为
   				1、固定参数, 或者通过NODE_IOC_SET_FMT获取fmt信息,
   				协商过程会将此参数向前级节点传递, 直至协商成功;
   				2、可变参数，继承上一节点的参数
*********************************************************************
*/
static int avc_ioc_fmt_nego(struct stream_iport *iport)
{
    struct stream_fmt *in_fmt = &iport->prev->fmt;	//上一个节点的参数
    struct avc_hdl *hdl = (struct avc_hdl *)iport->node->private_data;
    memcpy(&hdl->fmt, in_fmt, sizeof(struct stream_fmt));
    ASSERT(hdl->fmt.sample_rate == AVC_ADC_SAMPLE_RATE);    //采样率检查
    return NEGO_STA_ACCPTED;
}



static int avc_ioc_start(struct avc_hdl *hdl)
{
    struct cfg_info info = {0};
    int ret = jlstream_read_node_info_data(hdl_node(hdl)->uuid, hdl_node(hdl)->subid,
                                           hdl->name, &info);
    if (!ret) {
        avc_log("avc get param err\n");
        return 0;
    }

    u8 *data_buf = media_malloc(AUD_MODULE_AVC_COMMON, info.size);
    avc_log("avc info size %d\n", info.size);
    if (data_buf) {
        jlstream_read_form_cfg_data(&info, data_buf);
        hdl->avc_tool_param = media_malloc(AUD_MODULE_AVC_COMMON, sizeof(struct avc_param_tool_set));
        avc_tool_param_get(hdl, data_buf);
        media_free(data_buf);
    }
    avc_log("avc read stream bin\n");
    avc_param_printf(hdl->avc_tool_param);

    /*
     * 获取在线调试的临时参数
     */
    if (config_audio_cfg_online_enable) {
        u32 online_param_size = sizeof(struct avc_tool_common_param) + sizeof(u16) * 2 + avc_thr_table_len_get(THR_LVL_MAX_NUM) + avc_vol_table_len_get(THR_LVL_MAX_NUM, VOL_LVL_MAX_NUM);
        u8 *online_cfg = media_malloc(AUD_MODULE_AVC_COMMON, online_param_size);
        if (online_cfg) {
            ret = jlstream_read_effects_online_param(hdl_node(hdl)->uuid, hdl->name, online_cfg, online_param_size);
            if (ret) {
                avc_tool_param_get(hdl, online_cfg);
                avc_log("avc read online cfg\n");
                avc_param_printf(hdl->avc_tool_param);
            }
            media_free(online_cfg);
            online_cfg = NULL;
        }
    }

    hdl->cur_vol_lvl_num = hdl->avc_tool_param->common_param.vol_lvl_num;
    hdl->cur_vol_len = hdl->avc_tool_param->table_param.vol_len;
    hdl->cur_vol_offset_fade_time = hdl->avc_tool_param->common_param.vol_offset_fade_time;
    hdl->cur_vol_table = media_malloc(AUD_MODULE_AVC_COMMON, hdl->cur_vol_len);
    memcpy(hdl->cur_vol_table, hdl->avc_tool_param->table_param.vol_table, hdl->avc_tool_param->table_param.vol_len);

    //阈值检测
    struct threshold_det_param param = {0};
    param.thr_table = hdl->avc_tool_param->table_param.thr_table;
    param.thr_lvl_num = hdl->avc_tool_param->common_param.thr_lvl_num;
    param.thr_debounce = hdl->avc_tool_param->common_param.thr_debounce;
    param.run_interval = (ALGO_RUN_FRAME_LEN / (hdl->fmt.bit_wide ? 4 : 2) * 1000) / hdl->fmt.sample_rate;
    param.lvl_up_hold_time = hdl->avc_tool_param->common_param.lvl_up_hold_time;
    param.lvl_down_hold_time = hdl->avc_tool_param->common_param.lvl_down_hold_time;
    param.default_lvl = hdl->avc_tool_param->common_param.default_lvl - 1; //1为最低档，与工具对齐
    hdl->avc_thr_hdl = audio_threshold_det_open(&param);

    //档位同步
    struct audio_anc_lvl_sync_param lvl_sync_param = {0};
    lvl_sync_param.sync_result_cb = avc_lvl_sync_cb;
    lvl_sync_param.priv = (void *)hdl;
    lvl_sync_param.sync_check = 1;
    lvl_sync_param.high_lvl_sync = hdl->avc_tool_param->common_param.high_lvl_sync;
    lvl_sync_param.default_lvl = hdl->avc_tool_param->common_param.default_lvl - 1; //1为最低档，与工具对齐
    lvl_sync_param.name = COMMON_LVL_SYNC_AVC;
    hdl->lvl_sync_hdl = audio_anc_lvl_sync_open(&lvl_sync_param);

    //拼拆包buf
    hdl->frame_len = ALGO_RUN_FRAME_LEN;
    hdl->remain_buf = media_malloc(AUD_MODULE_AVC_COMMON, hdl->frame_len);

#if (AVC_ALGO_RUN_TYPE == ICSD_AVC_ALGO_RUN)
    //icsd avc
    avc_icsd_algo_open();
#if !ICSD_AVC_ALGO_TYPE
    memset(hdl->dac_data, 0, 256);
#endif
#endif
#if AVC_USE_AEC
    audio_avc_aec_init((void *)hdl);
    if (a2dp_player_runing()) {
        audio_avc_aec_open();
    } else if (esco_recoder_running()) {
        audio_avc_esco_aec_open();
    }
#endif
#if TCFG_USER_TWS_ENABLE
    hdl->tws_conn = get_tws_sibling_connect_state();
#endif
    hdl->stop = 0;
    os_mutex_create(&hdl->mutex);
    return 0;
}

static int avc_ioc_stop(struct avc_hdl *hdl)
{
    if (!hdl || !avc_aec_hdl) {
        return 0;
    }
#if AVC_USE_AEC
    if (avc_aec_hdl->mode == AVC_AEC_A2DP_MODE) {
        audio_avc_aec_close();
    } else if (avc_aec_hdl->mode == AVC_AEC_ESCO_MODE) {
        audio_avc_esco_aec_close();
    }
    audio_avc_aec_uninit();
#endif
    os_mutex_pend(&hdl->mutex, 0);
    hdl->stop = 1;
    hdl->remain_len = 0;
    if (hdl->avc_thr_hdl) {
        audio_threshold_det_close(hdl->avc_thr_hdl);
        hdl->avc_thr_hdl = NULL;
    }
    if (hdl->lvl_sync_hdl) {
        audio_anc_lvl_sync_close(hdl->lvl_sync_hdl);
        hdl->lvl_sync_hdl = NULL;
    }
    if (hdl->avc_tool_param) {
        if (hdl->avc_tool_param->table_param.thr_table) {
            media_free(hdl->avc_tool_param->table_param.thr_table);
            hdl->avc_tool_param->table_param.thr_table = NULL;
        }
        if (hdl->avc_tool_param->table_param.vol_table) {
            media_free(hdl->avc_tool_param->table_param.vol_table);
            hdl->avc_tool_param->table_param.vol_table = NULL;
        }
        media_free(hdl->avc_tool_param);
        hdl->avc_tool_param = NULL;
    }
    if (hdl->cur_vol_table) {
        media_free(hdl->cur_vol_table);
        hdl->cur_vol_table = NULL;
    }
    if (hdl->remain_buf) {
        media_free(hdl->remain_buf);
        hdl->remain_buf = NULL;
    }

#if (AVC_ALGO_RUN_TYPE == ICSD_AVC_ALGO_RUN)
    avc_icsd_algo_close();
#endif

    //模块关闭，恢复音量
    avc_vol_offset_set(&fade_hdl, 0, hdl->cur_vol_offset_fade_time);
    os_mutex_post(&hdl->mutex);


    return 0;
}

static int avc_ioc_update_parm(struct avc_hdl *hdl, int parm)
{
    int ret = false;
    if (hdl) {
        avc_tool_param_get(hdl, (u8 *)parm);
        avc_log("avc update param\n");
        avc_param_printf(hdl->avc_tool_param);
#if AVC_USE_AEC
        audio_avc_aec_param_update(hdl->avc_tool_param);
#endif

        hdl->thr_update = 1;
        hdl->vol_update = 1;
        ret = true;
    }
    return ret;
}

#if AVC_USE_AEC
static void audio_avc_aec_param_printf(struct avc_aec_hdl_t *avc_aec_hdl)
{
    avc_log("adc_bit_width %d\n", avc_aec_hdl->adc_bit_width);
    avc_log("dac_bit_width %d\n", avc_aec_hdl->dac_bit_width);
    avc_log("ref_channel %d\n", avc_aec_hdl->ref_channel);
    avc_log("ref_tmpbuf_len %d\n", avc_aec_hdl->ref_tmpbuf_len);
    avc_log("adc sr %d\n", AVC_ADC_SAMPLE_RATE);
    avc_log("dac sr %d\n", TCFG_AUDIO_GLOBAL_SAMPLE_RATE);
}

static void audio_avc_aec_param_update(struct avc_param_tool_set *avc_tool_param)
{
    if (!(avc_tool_param && avc_aec_hdl)) {
        return;
    }
    avc_aec_hdl->aec_nlp_cfg.aec_mode = 0;
    if (avc_tool_param->common_param.aec_en) {
        avc_aec_hdl->aec_nlp_cfg.aec_mode |= AEC_EN;
    }
    if (avc_tool_param->common_param.nlp_en) {
        avc_aec_hdl->aec_nlp_cfg.aec_mode |= NLP_EN;
    }
    avc_aec_hdl->aec_nlp_cfg.aec_dt_aggress = avc_tool_param->common_param.aec_dt_aggress;
    avc_aec_hdl->aec_nlp_cfg.aec_refengthr = avc_tool_param->common_param.aec_refengthr;
    avc_aec_hdl->aec_nlp_cfg.es_aggress_factor = avc_tool_param->common_param.es_aggress_factor;
    avc_aec_hdl->aec_nlp_cfg.es_min_suppress = avc_tool_param->common_param.es_min_suppress;
    aec_nlp_cfg_update(&avc_aec_hdl->aec_nlp_cfg);
}

static int audio_avc_aec_output_hdl(s16 *data, u16 len)
{
    if (!avc_aec_hdl) {
        return len;
    }
    u16 offset = 0;
    avc_pack_frame_process((struct avc_hdl *)avc_aec_hdl->node_hdl, (u8 *)data, &offset, len);
    return len;
}

//call in adc irq
int audio_avc_aec_data_fill(s16 *data, u16 len)
{
    if (!(a2dp_player_runing() && avc_aec_hdl && avc_aec_hdl->start)) {
        return 0;
    }
    if (len != (256 * (avc_aec_hdl->adc_bit_width ? 4 : 2))) {
        avc_log("avc_aec adc_data_len err");
        return 0;
    }

    //cvp lock
    if (TCFG_AUDIO_SMS_SEL == SMS_TDE) {
        audio_cvp_sms_tde_lock();
    } else {
        audio_cvp_sms_lock();
    }

    //回采数据
    int dac_read_len = avc_aec_hdl->ref_tmpbuf_len;
    int wlen = audio_dac_read(0, avc_aec_hdl->ref_tmpbuf, dac_read_len, avc_aec_hdl->ref_channel);
    if (wlen != dac_read_len) {
        avc_log("avc_aec dac_read err %d, %d\n", wlen, dac_read_len);
        if (TCFG_AUDIO_SMS_SEL == SMS_TDE) {
            audio_cvp_sms_tde_unlock();
        } else {
            audio_cvp_sms_unlock();
        }
        return 0;
    }
    if (avc_aec_hdl->dac_bit_width) {
        audio_convert_data_32bit_to_16bit_round((s32 *)avc_aec_hdl->ref_tmpbuf, (s16 *)avc_aec_hdl->ref_tmpbuf, dac_read_len / 4);
        dac_read_len >>= 1;
    }
    if (avc_aec_hdl->ref_channel == 2) {
        pcm_dual_to_single(avc_aec_hdl->ref_tmpbuf, avc_aec_hdl->ref_tmpbuf, dac_read_len);
        dac_read_len >>= 1;
    }
    acoustic_echo_cancel_refbuf((s16 *)avc_aec_hdl->ref_tmpbuf, NULL, dac_read_len);

    //MIC数据
    s16 *dat = (s16 *)((int)avc_aec_hdl->buf + (len * avc_aec_hdl->buf_cnt));
    memcpy(dat, data, len);
    if (avc_aec_hdl->adc_bit_width) {
        audio_convert_data_32bit_to_16bit_round((s32 *)dat, (s16 *)dat, len / 4);
        len >>= 1;
    }
    acoustic_echo_cancel_inbuf((s16 *)dat, len);
    if (++avc_aec_hdl->buf_cnt > ((avc_aec_hdl->input_buf_points / avc_aec_hdl->frame_points) - 1)) {	//计算下一个ADCbuffer位置
        avc_aec_hdl->buf_cnt = 0;
    }

    if (TCFG_AUDIO_SMS_SEL == SMS_TDE) {
        audio_cvp_sms_tde_unlock();
    } else {
        audio_cvp_sms_unlock();
    }
    return len;
}

int audio_avc_aec_open()
{
    if (!avc_aec_hdl) {
        return -1;
    }
    avc_aec_hdl->mode = AVC_AEC_A2DP_MODE;
    struct avc_hdl *hdl = avc_aec_hdl->node_hdl;
    if (!hdl || !hdl->avc_tool_param) {
        return -1;
    }
    avc_aec_hdl->adc_bit_width = adc_hdl.bit_width;
    avc_aec_hdl->dac_bit_width = dac_hdl.pd->bit_width;
    //dac回采
    avc_aec_hdl->ref_channel = AUDIO_CH_NUM(TCFG_AUDIO_DAC_CONNECT_MODE);
    avc_aec_hdl->ref_tmpbuf_len = AUDIO_ADC_IRQ_POINTS * (avc_aec_hdl->dac_bit_width ? 4 : 2) *
                                  avc_aec_hdl->ref_channel * (TCFG_AUDIO_GLOBAL_SAMPLE_RATE / AVC_ADC_SAMPLE_RATE);
    avc_aec_hdl->ref_tmpbuf = media_malloc(AUD_MODULE_AVC_AEC, avc_aec_hdl->ref_tmpbuf_len);
    //adc缓存
    avc_aec_hdl->frame_points = AUDIO_ADC_IRQ_POINTS;
    avc_aec_hdl->input_buf_points = avc_aec_hdl->frame_points * 3;
    avc_aec_hdl->buf_cnt = 0;
    avc_aec_hdl->buf = media_malloc(AUD_MODULE_AVC_AEC, avc_aec_hdl->input_buf_points * (avc_aec_hdl->adc_bit_width ? 4 : 2));
    avc_log("avc adc buf len %d\n", avc_aec_hdl->input_buf_points * (avc_aec_hdl->adc_bit_width ? 4 : 2));

    struct audio_aec_init_param_t init_param = {
        .sample_rate = AVC_ADC_SAMPLE_RATE,
        .ref_sr = TCFG_AUDIO_GLOBAL_SAMPLE_RATE,
        .ref_channel = 1, //外部传入已做二变一，节省内部buf
    };
    struct aec_param algo_param = {
        .aec_dt_aggress = hdl->avc_tool_param->common_param.aec_dt_aggress,
        .aec_refengthr = hdl->avc_tool_param->common_param.aec_refengthr,
        .es_aggress_factor = hdl->avc_tool_param->common_param.es_aggress_factor,
        .es_min_suppress = hdl->avc_tool_param->common_param.es_min_suppress,
    };
    audio_avc_aec_param_printf(avc_aec_hdl);
    s16 aec_enable_bit = 0;
    if (hdl->avc_tool_param->common_param.aec_en) {
        aec_enable_bit |= AEC_EN;
    }
    if (hdl->avc_tool_param->common_param.nlp_en) {
        aec_enable_bit |= NLP_EN;
    }
    acoustic_echo_cancel_init(&init_param, aec_enable_bit, audio_avc_aec_output_hdl, &algo_param);
    audio_dac_read_reset();
    clock_alloc("avc_aec", AVC_AEC_CLOCK);
    avc_aec_hdl->start = 1;
    return 0;
}
int audio_avc_aec_close()
{
    if (!avc_aec_hdl) {
        return -1;
    }
    avc_aec_hdl->start = 0;
    acoustic_echo_cancel_close();
    if (avc_aec_hdl->ref_tmpbuf) {
        media_free(avc_aec_hdl->ref_tmpbuf);
        avc_aec_hdl->ref_tmpbuf = NULL;
    }
    if (avc_aec_hdl->buf) {
        media_free(avc_aec_hdl->buf);
        avc_aec_hdl->buf = NULL;
    }
    clock_free("avc_aec");
    return 0;
}

int audio_avc_aec_init(void *hdl)
{
    if (avc_aec_hdl) {
        avc_log("avc aec already open\n");
        return -1;
    }
    avc_aec_hdl = media_malloc(AUD_MODULE_AVC_AEC, sizeof(struct avc_aec_hdl_t));
    ASSERT(avc_aec_hdl);
    avc_aec_hdl->node_hdl = hdl;
    avc_aec_hdl->start = 0;
    return 0;
}

int audio_avc_aec_uninit()
{
    if (avc_aec_hdl) {
        avc_aec_hdl->node_hdl = NULL;
        media_free(avc_aec_hdl);
        avc_aec_hdl = NULL;
    }
    return 0;
}

void audio_avc_esco_aec_run(s16 *data, u16 len)
{
#if AVC_ESCO_AEC_PROCESS_TASK
    cbuf_write(avc_aec_hdl->cbuf, data, len);
    os_sem_post(avc_aec_hdl->sem);
#else
    audio_avc_aec_output_hdl(data, len);
#endif
}

#if AVC_ESCO_AEC_PROCESS_TASK
static void audio_avc_esco_aec_process_task(void *priv)
{
    while (1) {
        os_sem_pend(avc_aec_hdl->sem, 0);
        if (!avc_aec_hdl->task_state) {
            continue;
        }
        avc_aec_hdl->task_busy = 1;
        s16 *data = media_malloc(AUD_MODULE_AVC_AEC, ALGO_RUN_FRAME_LEN);
        cbuf_read(avc_aec_hdl->cbuf, data, ALGO_RUN_FRAME_LEN);
        audio_avc_aec_output_hdl(data, ALGO_RUN_FRAME_LEN);
        media_free(data);
        avc_aec_hdl->task_busy = 0;
    }
}
#endif

static int audio_avc_esco_aec_open()
{
    if (!avc_aec_hdl) {
        return -1;
    }
    avc_aec_hdl->mode = AVC_AEC_ESCO_MODE;
#if AVC_ESCO_AEC_PROCESS_TASK
    avc_aec_hdl->task_state = 1;
    avc_aec_hdl->task_busy = 0;
    avc_aec_hdl->sem = media_malloc(AUD_MODULE_AVC_AEC, sizeof(OS_SEM));
    os_sem_create(avc_aec_hdl->sem, 0);
    avc_aec_hdl->cbuf = media_malloc(AUD_MODULE_AVC_AEC, sizeof(cbuffer_t));
    avc_aec_hdl->cbuf_data = media_malloc(AUD_MODULE_AVC_AEC, ALGO_RUN_FRAME_LEN * AVC_ESCO_AEC_PROCESS_BUF_NUM);
    cbuf_init(avc_aec_hdl->cbuf, avc_aec_hdl->cbuf_data, ALGO_RUN_FRAME_LEN * AVC_ESCO_AEC_PROCESS_BUF_NUM);
    int err = task_create(audio_avc_esco_aec_process_task, NULL, AVC_ESCO_AEC_PROCESS_TASK_NAME);
    if (err != OS_NO_ERR) {
        ASSERT(0, "audio_avc_esco_aec_process_task create err!!! %x\n", err);
    }
#endif
    return 0;
}

static int audio_avc_esco_aec_close()
{
    if (!avc_aec_hdl) {
        return -1;
    }
#if AVC_ESCO_AEC_PROCESS_TASK
    avc_aec_hdl->task_state = 0;
    while (avc_aec_hdl->task_busy) {
        os_time_dly(1);
    }
    task_kill(AVC_ESCO_AEC_PROCESS_TASK_NAME);
    if (avc_aec_hdl->sem) {
        os_sem_del(avc_aec_hdl->sem, 0);
        media_free(avc_aec_hdl->sem);
        avc_aec_hdl->sem = NULL;
    }
    if (avc_aec_hdl->cbuf_data) {
        media_free(avc_aec_hdl->cbuf_data);
        avc_aec_hdl->cbuf_data = NULL;
    }
    if (avc_aec_hdl->cbuf) {
        media_free(avc_aec_hdl->cbuf);
        avc_aec_hdl->cbuf = NULL;
    }
#endif
    return 0;
}

#endif


static int avc_ioctl(struct stream_iport *iport, int cmd, int arg)
{
    int ret = 0;
    struct avc_hdl *hdl = (struct avc_hdl *)iport->node->private_data;

    switch (cmd) {
    case NODE_IOC_OPEN_IPORT:
        avc_open_iport(iport);
        break;
    case NODE_IOC_START:
        avc_ioc_start(hdl);
        break;
    case NODE_IOC_SUSPEND:
    case NODE_IOC_STOP:
        avc_ioc_stop(hdl);
        break;
    case NODE_IOC_SET_FMT:
        break;
    case NODE_IOC_NEGOTIATE:
        *(int *)arg |= avc_ioc_fmt_nego(iport);
        break;
    case NODE_IOC_GET_DELAY:
        break;
    case NODE_IOC_NAME_MATCH:
        if (!strcmp((const char *)arg, hdl->name)) {
            ret = 1;
        }
        break;
    case NODE_IOC_SET_PARAM:
        ret = avc_ioc_update_parm(hdl, arg);
        break;
    }

    return ret;
}

//释放当前节点资源
static void avc_release(struct stream_node *node)
{
}

REGISTER_STREAM_NODE_ADAPTER(avc_adapter) = {
    .uuid       = NODE_UUID_AVC,
    .bind       = avc_bind,
    .ioctl      = avc_ioctl,
    .release    = avc_release,
    .handle_frame = avc_handle_frame,
    .hdl_size   = sizeof(struct avc_hdl),
};

REGISTER_ONLINE_ADJUST_TARGET(avc) = {
    .uuid = NODE_UUID_AVC,
};

#endif
