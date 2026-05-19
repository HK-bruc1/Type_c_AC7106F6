#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".icsd_aeq.data.bss")
#pragma data_seg(".icsd_aeq.data")
#pragma const_seg(".icsd_aeq.text.const")
#pragma code_seg(".icsd_aeq.text")
#endif

#include "app_config.h"
#include "audio_config_def.h"

#if (TCFG_AUDIO_ADAPTIVE_EQ_ENABLE && TCFG_AUDIO_ANC_ENABLE && \
	 TCFG_AUDIO_ANC_EXT_VERSION == ANC_EXT_V2) && \
	(defined TCFG_EQ_ENABLE)

#include "audio_anc_includes.h"
#include "icsd_aeq_config.h"
#include "jlstream.h"
#include "node_uuid.h"
#include "clock_manager/clock_manager.h"
#include "audio_config.h"
#include "volume_node.h"
#include "icsd_aeq.h"
#include "a2dp_player.h"

#if TCFG_USER_TWS_ENABLE
#include "bt_tws.h"
#endif/*TCFG_USER_TWS_ENABLE*/

#define ADAPTIVE_EQ_OUTPUT_IN_CUR_VOL_EN			0	//AEQ只根据当前音量生成一组AEQ，其他音量用默认EQ

#define ICSD_AEQ_OUTPUT_PRINTF                  	1   //AEQ 输出滤波器打印
#define ADAPTIVE_EQ_RUN_TIME_DEBUG					1	//AEQ 算法运行时间测试

#define ADAPTIVE_EQ_MULTI_SZ_CUR_MEMORY				0	//复用SZ链表空间，需要AEQ最后一个执行才可以
#define ADAPTIVE_EQ_SZ_IN_LOW_FRE_STABLE_FILTERING	0	//输入SZ 低频平稳滤波


#if 1
#define aeq_log printf
#else
#define aeq_log(...)
#endif

#if 0
#define aeq_debug_log printf
#else
#define aeq_debug_log(...)
#endif

struct audio_aeq_t {
    u8 eff_mode;							//AEQ效果模式
    u8 real_time_eq_en;						//实时自适应EQ使能
    enum audio_adaptive_fre_sel fre_sel;	//AEQ数据来源
    volatile u8 state;						//状态
    u8 run_busy;							//运行繁忙标志
    u8 force_exit;                          //打断标志
    s16 now_volume;							//当前EQ参数的音量
    void *lib_alloc_ptr;					//AEQ申请的内存指针
    float *sz_ref;							//金机参考SZ
    float *sz_dut_cmp;						//产测补偿SZ
    struct list_head head;
    spinlock_t lock;
    struct audio_afq_output *fre_out;		//实时SZ输出句柄
    struct eq_default_seg_tab *eq_ref;		//参考EQ
    void (*result_cb)(int msg);			//AEQ训练结束回调
    OS_MUTEX mutex;
    struct __anc_ext_adaptive_eq_thr thr;
    //工具数据
    anc_packet_data_t *tool_debug_data;
    struct anc_ext_subfile_head *tool_sz;
};

struct audio_aeq_bulk {
    struct list_head entry;
    s16 volume;
    struct eq_default_seg_tab *eq_output;
};

//SZ 产测参考曲线
static const float icsd_aeq_sz_dut_cmp[AEQ_FLEN] = {
    1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
    1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
    1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
    1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
    1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
    1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
    1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
    1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
    1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
    1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
    1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
    1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f
};

static const float icsd_aeq_sz_ref[AEQ_FLEN] = {
    0.261466758044396,
    0,
    0.701280688932828,
    1.14972555748360,
    1.66233764798327,
    1.26315793995178,
    2.50099745527896,
    0.773573862987295,
    2.90807421885558,
    0.157971611627073,
    2.97485767871939,
    -0.513030413680799,
    2.83659008310006,
    -1.08256569712470,
    2.50380861263304,
    -1.56682621111992,
    2.10676252182600,
    -1.91901637519355,
    1.78083336964134,
    -2.11231089817965,
    1.33240659433553,
    -2.25922631982953,
    0.945162339083366,
    -2.29412430159396,
    0.631988475711156,
    -2.17685974519830,
    0.371529426191162,
    -2.08257490923054,
    0.116855639496407,
    -1.94600716416929,
    -0.0519646435789859,
    -1.79936473247379,
    -0.189680098344969,
    -1.64901490918454,
    -0.269331155075202,
    -1.51554611171464,
    -0.351380304547003,
    -1.38686641594787,
    -0.405914247977647,
    -1.27175276892290,
    -0.458525261505156,
    -1.15987989903633,
    -0.472426095288859,
    -1.05747513252575,
    -0.500562009353185,
    -0.984786305282858,
    -0.521865388491020,
    -0.893023026703173,
    -0.527202297764581,
    -0.814872068740152,
    -0.534287203399104,
    -0.736030933554804,
    -0.548318958545947,
    -0.658687350322873,
    -0.551279366735041,
    -0.609107880963055,
    -0.542528620543772,
    -0.539717266257862,
    -0.533172787893396,
    -0.470817737290426,
    -0.522431045578259,
    -0.427378509495619,
    -0.507614644679172,
    -0.371606628128258,
    -0.483123785422762,
    -0.328218764326298,
    -0.463746728035587,
    -0.290218863792982,
    -0.443486655955824,
    -0.255191607100144,
    -0.422022890484541,
    -0.215159388099709,
    -0.396210398398675,
    -0.194978442214716,
    -0.370810544744234,
    -0.167669831289334,
    -0.350864879932880,
    -0.145326225130425,
    -0.331702819086647,
    -0.133895615593714,
    -0.305672238975395,
    -0.113448750109817,
    -0.285787869241409,
    -0.104495812708365,
    -0.264608960462395,
    -0.0897393607330895,
    -0.247632543455178,
    -0.0824362364537309,
    -0.232070847670182,
    -0.0723460979032049,
    -0.214639338592804,
    -0.0673959224275780,
    -0.195670739918220,
    -0.0629532810088606,
    -0.183229060967565,
    -0.0588850804390924,
    -0.166858648716423,
    -0.0577849901646768,
    -0.151986316202092,
    -0.0554932589693607,
    -0.143676154784309,
    -0.0518299979594228,
    -0.128655382344605,
    -0.0551071558725033,
    -0.119770544035697,
    -0.0525461045314080,
    -0.105669209123750,
    -0.0539182090019898,
    -0.0975386408074330,
    -0.0553321220442636,
    -0.0875295461950406,
    -0.0569483660665097,
    -0.0781888922067053,
    -0.0581910645226736,
    -0.0716329554504397,
    -0.0615060059393422,
    -0.0631266013574330,
    -0.0634123902042969,
    -0.0537529034876628,
    -0.0665480109440493,
};


//SE_SEL
static const u8 aeq_sz_sel_h_freq_test[240] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x1B, 0x37, 0x42, 0x00, 0x1B, 0xB7, 0x42, 0x40, 0x54, 0x09, 0x43,
    0x00, 0x1B, 0x37, 0x43, 0xC0, 0xE1, 0x64, 0x43, 0x40, 0x54, 0x89, 0x43, 0xA0, 0x37, 0xA0, 0x43,
    0x00, 0x1B, 0xB7, 0x43, 0x60, 0xFE, 0xCD, 0x43, 0xC0, 0xE1, 0xE4, 0x43, 0x20, 0xC5, 0xFB, 0x43,
    0x40, 0x54, 0x09, 0x44, 0xF0, 0xC5, 0x14, 0x44, 0xA0, 0x37, 0x20, 0x44, 0x50, 0xA9, 0x2B, 0x44,
    0x00, 0x1B, 0x37, 0x44, 0xB0, 0x8C, 0x42, 0x44, 0x60, 0xFE, 0x4D, 0x44, 0x10, 0x70, 0x59, 0x44,
    0xC0, 0xE1, 0x64, 0x44, 0x70, 0x53, 0x70, 0x44, 0x20, 0xC5, 0x7B, 0x44, 0x68, 0x9B, 0x83, 0x44,
    0x40, 0x54, 0x89, 0x44, 0x18, 0x0D, 0x8F, 0x44, 0xF0, 0xC5, 0x94, 0x44, 0xC8, 0x7E, 0x9A, 0x44,
    0xA0, 0x37, 0xA0, 0x44, 0x78, 0xF0, 0xA5, 0x44, 0x50, 0xA9, 0xAB, 0x44, 0x28, 0x62, 0xB1, 0x44,
    0x00, 0x1B, 0xB7, 0x44, 0xD8, 0xD3, 0xBC, 0x44, 0xB0, 0x8C, 0xC2, 0x44, 0x88, 0x45, 0xC8, 0x44,
    0x60, 0xFE, 0xCD, 0x44, 0x38, 0xB7, 0xD3, 0x44, 0x10, 0x70, 0xD9, 0x44, 0xE8, 0x28, 0xDF, 0x44,
    0xC0, 0xE1, 0xE4, 0x44, 0x98, 0x9A, 0xEA, 0x44, 0x70, 0x53, 0xF0, 0x44, 0x48, 0x0C, 0xF6, 0x44,
    0x20, 0xC5, 0xFB, 0x44, 0xFC, 0xBE, 0x00, 0x45, 0x68, 0x9B, 0x03, 0x45, 0xD4, 0x77, 0x06, 0x45,
    0x40, 0x54, 0x09, 0x45, 0xAC, 0x30, 0x0C, 0x45, 0x18, 0x0D, 0x0F, 0x45, 0x84, 0xE9, 0x11, 0x45,
    0xF0, 0xC5, 0x14, 0x45, 0x5C, 0xA2, 0x17, 0x45, 0xC8, 0x7E, 0x1A, 0x45, 0x34, 0x5B, 0x1D, 0x45,
    0xA0, 0x37, 0x20, 0x45, 0x0C, 0x14, 0x23, 0x45, 0x78, 0xF0, 0x25, 0x45, 0xE4, 0xCC, 0x28, 0x45
};

/*
   AEQ 根据音量分档表，音量从小到大排列；
   一般建议分3档以下，档位越多，AEQ时间越长，需要空间更大
*/
u8 aeq_volume_grade_list[] = {
    5,	//0-5
    10,	//6-10
    16,	//11-16
};

#if 0
u8 aeq_volume_grade_maxdB_table[3][3] = {
    {
        //佩戴-紧
        22,//0-5
        12,//6-10
        3, //11-16
    },
    {
        //佩戴-正常
        20,
        14,
        2,
    },
    {
        //佩戴-松
        15,
        10,
        1,
    },
};
#endif

static const struct eq_seg_info test_eq_tab_normal[] = {
    {0, EQ_IIR_TYPE_BAND_PASS, 31,    0, 0.7f},
    {1, EQ_IIR_TYPE_BAND_PASS, 62,    0, 0.7f},
    {2, EQ_IIR_TYPE_BAND_PASS, 125,   0, 0.7f},
    {3, EQ_IIR_TYPE_BAND_PASS, 250,   0, 0.7f},
    {4, EQ_IIR_TYPE_BAND_PASS, 500,   0, 0.7f},
    {5, EQ_IIR_TYPE_BAND_PASS, 1000,  0, 0.7f},
    {6, EQ_IIR_TYPE_BAND_PASS, 2000,  0, 0.7f},
    {7, EQ_IIR_TYPE_BAND_PASS, 4000,  0, 0.7f},
    {8, EQ_IIR_TYPE_BAND_PASS, 8000,  0, 0.7f},
    {9, EQ_IIR_TYPE_BAND_PASS, 16000, 0, 0.7f},
};

static struct audio_aeq_t *aeq_hdl = NULL;
struct eq_default_seg_tab default_seg_tab = {
    .global_gain = 0.0f,
    .seg_num = ARRAY_SIZE(test_eq_tab_normal),
    .seg = (struct eq_seg_info *)(test_eq_tab_normal),
};

static void audio_adaptive_eq_afq_output_hdl(struct audio_afq_output *p);
static struct eq_default_seg_tab *audio_adaptive_eq_cur_list_query(s16 volume);
static float audio_adaptive_eq_vol_gain_get(s16 volume);
static struct eq_default_seg_tab *audio_icsd_eq_default_tab_read();
static int audio_icsd_eq_eff_update(struct eq_default_seg_tab *eq_tab);
static void audio_adaptive_eq_sz_data_packet(struct audio_afq_output *p);
static int audio_adaptive_eq_node_check(void);

//保留现场及功能互斥
static void audio_adaptive_eq_mutex_suspend(void)
{
    if (aeq_hdl) {
    }
}

//恢复现场及功能互斥
static void audio_adaptive_eq_mutex_resume(void)
{
    if (aeq_hdl) {
    }
}

static int audio_adaptive_eq_permit(enum audio_adaptive_fre_sel fre_sel)
{
    if (anc_ext_ear_adaptive_cfg_get()->aeq_gains == NULL) {
        aeq_log("ERR: ANC_EXT adaptive eq cfg no enough!\n");
        return ANC_EXT_FAIL_AEQ_GAIN_CFG_MISS;
    }
    if ((anc_ext_ear_adaptive_cfg_get()->aeq_mem_iir == NULL) && aeq_hdl->real_time_eq_en) {
        aeq_log("ERR: ANC_EXT adaptive eq cfg mem_iir no enough!\n");
        return ANC_EXT_FAIL_AEQ_MEM_CFG_MISS;
    }
#if TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_LR
    if (anc_ext_ear_adaptive_cfg_get()->raeq_gains == NULL) {
        aeq_log("ERR: ANC_EXT adaptive eq cfg no enough!\n");
        return ANC_EXT_FAIL_AEQ_GAIN_CFG_MISS;
    }
    if ((anc_ext_ear_adaptive_cfg_get()->raeq_mem_iir == NULL) && aeq_hdl->real_time_eq_en) {
        aeq_log("ERR: ANC_EXT adaptive eq cfg mem_iir no enough!\n");
        return ANC_EXT_FAIL_AEQ_MEM_CFG_MISS;
    }
#endif
    if (aeq_hdl->state != ADAPTIVE_EQ_STATE_CLOSE) {
        return ANC_EXT_OPEN_FAIL_REENTRY;
    }
    if (audio_adaptive_eq_node_check()) {
        aeq_log("ERR: adaptive eq node is missing!\n");
        return ANC_EXT_FAIL_AEQ_NODE_MISS;
    }
    return 0;
}

int audio_adaptive_eq_init(void)
{
    aeq_hdl = anc_malloc("ICSD_AEQ", sizeof(struct audio_aeq_t));
    spin_lock_init(&aeq_hdl->lock);
    INIT_LIST_HEAD(&aeq_hdl->head);
    os_mutex_create(&aeq_hdl->mutex);
    ASSERT(aeq_hdl);
    aeq_hdl->eff_mode = AEQ_EFF_MODE_ADAPTIVE;
    return 0;
}

int audio_adaptive_eq_open(enum audio_adaptive_fre_sel fre_sel, void (*result_cb)(int msg))
{
    int ret = audio_adaptive_eq_permit(fre_sel);
    if (ret) {
        aeq_debug_log("adaptive_eq_permit, open fail %d\n", ret);
        audio_anc_debug_err_send_data(ANC_DEBUG_APP_ADAPTIVE_EQ, &ret, sizeof(int));
        return ret;
    }

    aeq_log("ICSD_AEQ_STATE:OPEN, real time %d, fre_sel %d\n", aeq_hdl->real_time_eq_en, fre_sel);
    mem_stats();

    //1.保留现场及功能互斥
    audio_adaptive_eq_mutex_suspend();

    if (!aeq_hdl->real_time_eq_en) {	//非实时自适应EQ才申请时钟
        clock_alloc("ANC_AEQ", 160 * 1000000L);
    }

    aeq_hdl->state = ADAPTIVE_EQ_STATE_OPEN;
    aeq_hdl->fre_sel = fre_sel;
    aeq_hdl->result_cb = result_cb;

    //2.准备算法输入参数
    if (aeq_hdl->eq_ref) {
        anc_free(aeq_hdl->eq_ref);
    }
    aeq_hdl->eq_ref = anc_malloc("ICSD_AEQ", sizeof(struct eq_default_seg_tab));
    memcpy(aeq_hdl->eq_ref, &default_seg_tab, sizeof(struct eq_default_seg_tab));

    // audio_icsd_eq_eff_update(aeq_hdl->eq_ref);
    //输入算法前，dB转线性值
    aeq_hdl->eq_ref->global_gain = eq_db2mag(aeq_hdl->eq_ref->global_gain);

    //3. SZ获取
    audio_afq_common_add_output_handler("ANC_AEQ", fre_sel, audio_adaptive_eq_afq_output_hdl);

    //cppcheck-suppress memleak
    return 0;
}

//(实时)自适应EQ打开
int audio_real_time_adaptive_eq_open(enum audio_adaptive_fre_sel fre_sel, void (*result_cb)(int result))
{
    //工具AEQ关闭，不允许启动AEQ
    if (!anc_ext_adaptive_eq_tool_en_get()) {
        return 0;
    }
    aeq_hdl->real_time_eq_en = 1;
    int ret = audio_adaptive_eq_open(fre_sel, result_cb);
    //打开失败且非重入场景则清0对应标志
    if (ret && (ret != ANC_EXT_OPEN_FAIL_REENTRY)) {
        aeq_hdl->real_time_eq_en = 0;
    }
    return ret;
}

//(实时)自适应EQ退出
int audio_real_time_adaptive_eq_close(void)
{
    if (aeq_hdl->state == ADAPTIVE_EQ_STATE_CLOSE) {
        return 0;
    }
    aeq_log("ICSD_AEQ_STATE:FORCE_EXIT");
    aeq_hdl->force_exit = 1;
    icsd_aeq_force_exit();  // RUN过程强制退出，之后执行CLOSE
    audio_adaptive_eq_close();
    return 0;
}

int audio_adaptive_eq_close()
{
    aeq_debug_log("%s\n", __func__);
    if (aeq_hdl) {
        if ((aeq_hdl->state != ADAPTIVE_EQ_STATE_CLOSE) && (aeq_hdl->state != ADAPTIVE_EQ_STATE_STOP)) {
            aeq_hdl->state = ADAPTIVE_EQ_STATE_STOP;
            os_mutex_pend(&aeq_hdl->mutex, 0);
            //删除频响来源回调，若来源结束，则关闭来源
            audio_afq_common_del_output_handler("ANC_AEQ");

            aeq_debug_log("%s, ok\n", __func__);
            //恢复ANC相关状态
            audio_adaptive_eq_mutex_resume();

            if (aeq_hdl->eq_ref) {
                anc_free(aeq_hdl->eq_ref);
                aeq_hdl->eq_ref = NULL;
            }
            clock_free("ANC_AEQ");
            aeq_hdl->run_busy = 0;

            //在线更新EQ效果
            if (aeq_hdl->eff_mode == AEQ_EFF_MODE_ADAPTIVE) {
                s16 volume = app_audio_get_volume(APP_AUDIO_STATE_MUSIC);
                aeq_hdl->now_volume = volume;
                audio_icsd_eq_eff_update(audio_adaptive_eq_cur_list_query(volume));
            }
            aeq_hdl->real_time_eq_en = 0;
            aeq_hdl->force_exit = 0;
            aeq_hdl->state = ADAPTIVE_EQ_STATE_CLOSE;
            aeq_log("ICSD_AEQ_STATE:CLOSE");

            if (aeq_hdl->result_cb) {
                aeq_hdl->result_cb(0);
            }
            os_mutex_post(&aeq_hdl->mutex);
        }
    }
    return 0;
}

//查询AEQ的状态
u8 audio_adaptive_eq_state_get(void)
{
    if (aeq_hdl) {
        return aeq_hdl->state;
    }
    return ADAPTIVE_EQ_STATE_CLOSE;
}


//查询aeq是否活动中
u8 audio_adaptive_eq_is_running(void)
{
    if (aeq_hdl) {
        return aeq_hdl->run_busy;
    }
    return 0;
}

//立即更新EQ参数
static int audio_icsd_eq_eff_update(struct eq_default_seg_tab *eq_tab)
{
    if (!eq_tab) {
        return 1;
    }

    aeq_debug_log("%s\n", __func__);
    if (cpu_in_irq()) {
        aeq_debug_log("AEQ_eff update post app_core\n");
        int msg[3];
        msg[0] = (int)audio_icsd_eq_eff_update;
        msg[1] = 1;
        msg[2] = (int)eq_tab;
        int ret = os_taskq_post_type("app_core", Q_CALLBACK, 3, msg);
        if (ret) {
            aeq_debug_log("aeq taskq_post err\n");
        }
        return 0;
    }
    aeq_debug_log("ICSD_AEQ_STATE:ALOGM_UPDATE\n");
#if 0
    aeq_debug_log("global_gain %d/100\n", (int)(eq_tab->global_gain * 100.f));
    aeq_debug_log("seg_num %d\n", eq_tab->seg_num);
    for (int i = 0; i < eq_tab->seg_num; i++) {
        aeq_debug_log("index %d\n", eq_tab->seg[i].index);
        aeq_debug_log("iir_type %d\n", eq_tab->seg[i].iir_type);
        aeq_debug_log("freq %d\n", eq_tab->seg[i].freq);
        aeq_debug_log("gain %d/100\n", (int)(eq_tab->seg[i].gain * 100.f));
        aeq_debug_log("q %d/100\n", (int)(eq_tab->seg[i].q * 100.f));
    }
#endif

#if 0
    /*
     *更新用户自定义总增益
     * */
    struct eq_adj eff = {0};
    char *node_name = ADAPTIVE_EQ_TARGET_NODE_NAME;//节点名称（节点内的第一参数，用户自定义,长度小于等于15byte）
    eff.param.global_gain =  eq_tab->global_gain;       //设置总增益 -5dB
    eff.fade_parm.fade_time = 10;      //ms，淡入timer执行的周期
    eff.fade_parm.fade_step = 0.1f;    //淡入步进

    eff.type = EQ_GLOBAL_GAIN_CMD;      //参数类型：总增益
    int ret = jlstream_set_node_param(NODE_UUID_EQ, node_name, &eff, sizeof(eff));
    if (!ret) {
        puts("error\n");
    }
    /*
     *更新系数表段数(如系数表段数发生改变，需要用以下处理，更新系数表段数)
     * */
    eff.param.seg_num = eq_tab->seg_num;           //系数表段数
    eff.type = EQ_SEG_NUM_CMD;         //参数类型：系数表段数
    ret = jlstream_set_node_param(NODE_UUID_EQ, node_name, &eff, sizeof(eff));
    if (!ret) {
        puts("error\n");
    }
    /*
     *更新用户自定义的系数表
     * */
    eff.type = EQ_SEG_CMD;             //参数类型：滤波器参数
    for (int j = 0; j < eq_tab->seg_num; j++) {
        memcpy(&eff.param.seg, &(eq_tab->seg[j]), sizeof(struct eq_seg_info));//滤波器参数
        jlstream_set_node_param(NODE_UUID_EQ, node_name, &eff, sizeof(eff));
    }
#else
    struct eq_adj eff = {0};
    eff.type = EQ_TAB_CMD;
    eff.param.tab.global_gain = eq_tab->global_gain;;
    eff.param.tab.seg_num = eq_tab->seg_num;
    eff.param.tab.seg = eq_tab->seg; //系数表指针赋值
    int ret = jlstream_set_node_param(NODE_UUID_EQ, ADAPTIVE_EQ_TARGET_NODE_NAME, &eff, sizeof(eff));
#endif
    return 0;
}

//检查是否有AEQ节点
static int audio_adaptive_eq_node_check(void)
{
    struct cfg_info info = {0};
    char *node_name = ADAPTIVE_EQ_TARGET_NODE_NAME;
    return jlstream_read_form_node_info_base(0, node_name, 0, &info);
}

/*
   读取可视化工具stream.bin的配置;
   如复用EQ节点，若用户用EQ节点，并自定义参数，需要在更新参数的时候通知AEQ算法
 */
static struct eq_default_seg_tab *audio_icsd_eq_default_tab_read()
{
#if ADAPTIVE_EQ_TARGET_DEFAULT_CFG_READ
    /*
     *解析配置文件内效果配置
     * */
    struct eq_default_seg_tab *eq_ref = NULL;
    struct cfg_info info  = {0};             //节点配置相关信息（参数存储的目标地址、配置项大小）
    char mode_index       = 0;               //模式序号（当前节点无多参数组，mode_index是0）
    char *node_name       = ADAPTIVE_EQ_TARGET_NODE_NAME;       //节点名称（节点内的第一参数，用户自定义,长度小于等于15byte）
    char cfg_index        = 0;               //目标配置项序号（当前节点无多参数组，cfg_index是0）
    int ret = jlstream_read_form_node_info_base(mode_index, node_name, cfg_index, &info);
    if (!ret) {
        struct eq_tool *tab = anc_malloc("ICSD_AEQ", info.size);
        if (!jlstream_read_form_cfg_data(&info, tab)) {
            aeq_log("[AEQ]user eq cfg parm read err\n");
            anc_free(tab);
            return &default_seg_tab;
        }
#if 0
        aeq_debug_log("global_gain %d/100\n", (int)(tab->global_gain * 100.f));
        aeq_debug_log("is_bypass %d\n", tab->is_bypass);
        aeq_debug_log("seg_num %d\n", tab->seg_num);
        for (int i = 0; i < tab->seg_num; i++) {
            aeq_debug_log("index %d\n", tab->seg[i].index);
            aeq_debug_log("iir_type %d\n", tab->seg[i].iir_type);
            aeq_debug_log("freq %d\n", tab->seg[i].freq);
            aeq_debug_log("gain %d/100\n", (int)(tab->seg[i].gain * 100.f));
            aeq_debug_log("q %d/100\n", (int)(tab->seg[i].q * 100.f));
        }
#endif
        //读取EQ格式转换
        eq_ref = anc_malloc("ICSD_AEQ", sizeof(struct eq_default_seg_tab));
        int seg_size = sizeof(struct eq_seg_info) * tab->seg_num;
        eq_ref->seg = anc_malloc("ICSD_AEQ", seg_size);
        eq_ref->global_gain = tab->global_gain;
        eq_ref->seg_num = tab->seg_num;
        memcpy(eq_ref->seg, tab->seg, seg_size);

        anc_free(tab);
    } else {
        aeq_log("[AEQ]user eq cfg parm read err ret %d\n", ret);
        return &default_seg_tab;
    }

    return eq_ref;
#else

    return &default_seg_tab;
#endif
}

//读取自适应AEQ结果 - 用于播歌更新效果
struct eq_default_seg_tab *audio_icsd_adaptive_eq_read(void)
{
    struct eq_default_seg_tab *eq_tab;
    if (aeq_hdl) {
        //1、单次AEQ需要等训练完成才更新；
        //2、实时自适应支持随时更新
        u8 update = ((!audio_adaptive_eq_is_running()) || aeq_hdl->real_time_eq_en);
        if (aeq_hdl->eff_mode == AEQ_EFF_MODE_ADAPTIVE && update) {
            s16 volume = app_audio_get_volume(APP_AUDIO_STATE_MUSIC);
            eq_tab = audio_adaptive_eq_cur_list_query(volume);
            aeq_debug_log("ICSD_AEQ_STATE: MUSIC_GET PARAM\n");
#if 0
            aeq_debug_log("global_gain %d/100\n", (int)(eq_tab->global_gain * 100.f));
            aeq_debug_log("seg_num %d\n", eq_tab->seg_num);
            for (int i = 0; i < eq_tab->seg_num; i++) {
                aeq_debug_log("index %d\n", eq_tab->seg[i].index);
                aeq_debug_log("iir_type %d\n", eq_tab->seg[i].iir_type);
                aeq_debug_log("freq %d\n", eq_tab->seg[i].freq);
                aeq_debug_log("gain %d/100\n", (int)(eq_tab->seg[i].gain * 100.f));
                aeq_debug_log("q %d/100\n", (int)(eq_tab->seg[i].q * 100.f));
            }
#endif
            return eq_tab;
        }
    }
    return audio_icsd_eq_default_tab_read();	//表示使用默认参数
}

static void audio_adaptive_eq_sz_data_packet(struct audio_afq_output *p)
{
    if (!anc_ext_tool_online_get()) {
        return;
    }
    if (aeq_hdl->tool_sz) {
        anc_free(aeq_hdl->tool_sz);
    }
    aeq_hdl->tool_sz = anc_ext_subfile_catch_init(FILE_ID_ANC_EXT_REF_SZ_DATA);
    aeq_hdl->tool_sz = anc_ext_subfile_catch(aeq_hdl->tool_sz, (u8 *)(p->sz_l.out), AEQ_FLEN * sizeof(float), ANC_EXT_REF_SZ_DATA_ID);
}

int audio_adaptive_eq_tool_sz_data_get(u8 **buf, u32 *len)
{
    if (aeq_hdl->tool_sz == NULL) {
        aeq_log("AEQ sz packet is NULL, return!\n");
        return -1;
    }
    *buf = (u8 *)(aeq_hdl->tool_sz);
    *len = aeq_hdl->tool_sz->file_len;
    return 0;
}

static void audio_adaptive_eq_data_packet(_aeq_output *aeq_output, float *sz, float *fgq, int cnt, __adpt_aeq_cfg *aeq_cfg)
{

    if (!anc_ext_tool_online_get()) {
        return;
    }
    int len = AEQ_FLEN / 2;
    int out_seg_num =  ADAPTIVE_EQ_ORDER;
    int eq_dat_len =  sizeof(anc_fr_t) * out_seg_num + 4;
    u8 tmp_type[out_seg_num];
    u8 *leq_dat = anc_malloc("ICSD_AEQ", eq_dat_len);
    for (int i = 0; i < out_seg_num; i++) {
        tmp_type[i] = aeq_cfg->type[i];
    }

    /* for (int i = 0; i < 120; i++) { */
    /* aeq_log("target:%d\n", (int)aeq_output->target[i]); */
    /* } */
    /* for (int i = 0; i < 60; i++) { */
    /* aeq_log("freq:%d\n", (int)aeq_output->h_freq[i]); */
    /* } */
    audio_anc_fr_format(leq_dat, fgq, out_seg_num, tmp_type);

#if ADAPTIVE_EQ_OUTPUT_IN_CUR_VOL_EN
    aeq_hdl->tool_debug_data = anc_data_catch(aeq_hdl->tool_debug_data, NULL, 0, 0, 1);
#else
    if (cnt == 0) {
        aeq_hdl->tool_debug_data = anc_data_catch(aeq_hdl->tool_debug_data, NULL, 0, 0, 1);
    }
#endif
    if (cnt == 0) {
#if TCFG_USER_TWS_ENABLE
        if (0) {
            /* if (bt_tws_get_local_channel() == 'R') { */
            aeq_debug_log("AEQ-adaptive send data, ch:R\n");
            aeq_hdl->tool_debug_data = anc_data_catch(aeq_hdl->tool_debug_data, (u8 *)aeq_output->h_freq, len * 4, ANC_R_ADAP_FRE, 0);
            aeq_hdl->tool_debug_data = anc_data_catch(aeq_hdl->tool_debug_data, (u8 *)sz, len * 8, ANC_R_ADAP_SZPZ, 0);
            aeq_hdl->tool_debug_data = anc_data_catch(aeq_hdl->tool_debug_data, (u8 *)aeq_output->target, len * 8, ANC_R_ADAP_TARGET, 0);
            aeq_hdl->tool_debug_data = anc_data_catch(aeq_hdl->tool_debug_data, (u8 *)leq_dat, eq_dat_len, AEQ_R_IIR_VOL_LOW, 0);
        } else
#endif
        {
            aeq_debug_log("AEQ-adaptive send data, ch:L\n");
            if (aeq_output) {
                aeq_hdl->tool_debug_data = anc_data_catch(aeq_hdl->tool_debug_data, (u8 *)aeq_output->h_freq, len * 4, ANC_L_ADAP_FRE, 0);
                aeq_hdl->tool_debug_data = anc_data_catch(aeq_hdl->tool_debug_data, (u8 *)aeq_output->target, len * 8, ANC_L_ADAP_TARGET, 0);
            } else {
                aeq_hdl->tool_debug_data = anc_data_catch(aeq_hdl->tool_debug_data, (u8 *)aeq_sz_sel_h_freq_test, len * 4, ANC_L_ADAP_FRE, 0);
            }
            aeq_hdl->tool_debug_data = anc_data_catch(aeq_hdl->tool_debug_data, (u8 *)sz, len * 8, ANC_L_ADAP_SZPZ, 0);
            aeq_hdl->tool_debug_data = anc_data_catch(aeq_hdl->tool_debug_data, (u8 *)leq_dat, eq_dat_len, AEQ_L_IIR_VOL_LOW, 0);
        }
    } else if (cnt == 1) {
#if TCFG_USER_TWS_ENABLE
        if (0) {
            /* if (bt_tws_get_local_channel() == 'R') { */
            aeq_debug_log("AEQ-adaptive send data, ch:R\n");
            aeq_hdl->tool_debug_data = anc_data_catch(aeq_hdl->tool_debug_data, (u8 *)leq_dat, eq_dat_len, AEQ_R_IIR_VOL_MIDDLE, 0);
        } else
#endif
        {
            aeq_debug_log("AEQ-adaptive send data, ch:L\n");
            aeq_hdl->tool_debug_data = anc_data_catch(aeq_hdl->tool_debug_data, (u8 *)leq_dat, eq_dat_len, AEQ_L_IIR_VOL_MIDDLE, 0);
        }
    } else if (cnt == 2) {
#if TCFG_USER_TWS_ENABLE
        if (0) {
            /* if (bt_tws_get_local_channel() == 'R') { */
            aeq_debug_log("AEQ-adaptive send data, ch:R\n");
            aeq_hdl->tool_debug_data = anc_data_catch(aeq_hdl->tool_debug_data, (u8 *)leq_dat, eq_dat_len, AEQ_R_IIR_VOL_HIGH, 0);
        } else
#endif
        {
            aeq_debug_log("AEQ-adaptive send data, ch:L\n");
            aeq_hdl->tool_debug_data = anc_data_catch(aeq_hdl->tool_debug_data, (u8 *)leq_dat, eq_dat_len, AEQ_L_IIR_VOL_HIGH, 0);
        }
    }

    anc_free(leq_dat);
}

int audio_adaptive_eq_tool_data_get(u8 **buf, u32 *len)
{
    if (aeq_hdl->tool_debug_data == NULL) {
        aeq_log("AEQ packet is NULL, return!\n");
        return -1;
    }
    if (aeq_hdl->tool_debug_data->dat_len == 0) {
        aeq_log("AEQ error: dat_len == 0\n");
        return -1;
    }
    *buf = aeq_hdl->tool_debug_data->dat;
    *len = aeq_hdl->tool_debug_data->dat_len;
    return 0;
}

static int audio_adaptive_eq_cur_list_add(s16 volume, struct eq_default_seg_tab *eq_output)
{
    struct audio_aeq_bulk *bulk = anc_malloc("ICSD_AEQ", sizeof(struct audio_aeq_bulk));
    struct audio_aeq_bulk *tmp;
    spin_lock(&aeq_hdl->lock);
    list_for_each_entry(tmp, &aeq_hdl->head, entry) {
        //遇见相同音量等级直接覆盖
        if (tmp->volume == volume) {
            anc_free(tmp->eq_output->seg);
            anc_free(tmp->eq_output);
            tmp->eq_output = eq_output;
            anc_free(bulk);
            spin_unlock(&aeq_hdl->lock);
            return -1;
        }
    }
    bulk->volume = volume;
    bulk->eq_output = eq_output;
    list_add_tail(&bulk->entry, &aeq_hdl->head);
    spin_unlock(&aeq_hdl->lock);
    return 0;
}

//删除AEQ系数链表
int audio_adaptive_eq_cur_list_del(void)
{
    struct audio_aeq_bulk *bulk;
    struct audio_aeq_bulk *temp;
    spin_lock(&aeq_hdl->lock);
    list_for_each_entry_safe(bulk, temp, &aeq_hdl->head, entry) {
        anc_free(bulk->eq_output->seg);
        anc_free(bulk->eq_output);
        __list_del_entry(&(bulk->entry));
        anc_free(bulk);
    }
    spin_unlock(&aeq_hdl->lock);
    return 0;
}

static struct eq_default_seg_tab *audio_adaptive_eq_cur_list_query(s16 volume)
{
    struct audio_aeq_bulk *bulk;

#if	ADAPTIVE_EQ_VOLUME_GRADE_EN
    int vol_list_num = sizeof(aeq_volume_grade_list);

    for (u8 i = 0; i < vol_list_num; i++) {
        if (volume <= aeq_volume_grade_list[i]) {
            volume = aeq_volume_grade_list[i];
            break;
        }
    }
#else
    volume = 0;
#endif

    spin_lock(&aeq_hdl->lock);
    list_for_each_entry(bulk, &aeq_hdl->head, entry) {
        if (bulk->volume == volume) {
            spin_unlock(&aeq_hdl->lock);
            return bulk->eq_output;
        }
    }
    spin_unlock(&aeq_hdl->lock);
    //读不到则返回默认参数
    return audio_icsd_eq_default_tab_read();
}

static int audio_adaptive_eq_start(void)
{
    //初始化算法，申请资源
    struct icsd_aeq_libfmt libfmt;	//获取算法库需要的参数
    struct icsd_aeq_infmt infmt;	//输入算法库的参数

    icsd_aeq_get_libfmt(&libfmt);
    aeq_debug_log("AEQ RAM SIZE:%d\n", libfmt.lib_alloc_size);
    aeq_hdl->lib_alloc_ptr = anc_malloc("ICSD_AEQ", libfmt.lib_alloc_size);
    if (!aeq_hdl->lib_alloc_ptr) {
        return -1;
    }
    infmt.alloc_ptr = aeq_hdl->lib_alloc_ptr;
    infmt.seg_num = AEG_SEG_NUM;
    icsd_aeq_set_infmt(&infmt);
    return 0;

}

static int audio_adaptive_eq_stop(void)
{
    anc_free(aeq_hdl->lib_alloc_ptr);
    aeq_hdl->lib_alloc_ptr = NULL;
    return 0;
}

static struct eq_default_seg_tab *audio_adaptive_eq_run(float maxgain_dB, int cnt)
{
    struct eq_default_seg_tab *eq_cur_tmp = NULL;
    _aeq_output *aeq_output = NULL;
    u8 sz_l_sel_idx;
    struct anc_ext_ear_adaptive_param *ext_cfg = anc_ext_ear_adaptive_cfg_get();

#if ADAPTIVE_EQ_MULTI_SZ_CUR_MEMORY
    float *sz_cur = aeq_hdl->fre_out->sz_l.out;
#else
    float *sz_cur = (float *)anc_malloc("ICSD_AEQ", aeq_hdl->fre_out->sz_l.len * sizeof(float));
    memcpy(sz_cur, aeq_hdl->fre_out->sz_l.out, aeq_hdl->fre_out->sz_l.len * sizeof(float));
#endif
    aeq_hdl->sz_ref = (float *)anc_malloc("ICSD_AEQ", AEQ_FLEN * sizeof(float));

    /* memcpy((u8 *)aeq_hdl->sz_ref, sz_ref_test, AEQ_FLEN * sizeof(float)); */

    if (ext_cfg->sz_ref) {
        /* memcpy((u8 *)aeq_hdl->sz_ref, (u8 *)icsd_aeq_sz_ref, AEQ_FLEN * sizeof(float)); */
        memcpy((u8 *)aeq_hdl->sz_ref, ext_cfg->sz_ref, AEQ_FLEN * sizeof(float));
    }

    aeq_hdl->sz_dut_cmp = (float *)icsd_aeq_sz_dut_cmp;

//低频平稳滤波
#if ADAPTIVE_EQ_SZ_IN_LOW_FRE_STABLE_FILTERING
    for (int i = 0; i < 10; i += 2) {
        aeq_hdl->sz_ref[i] = aeq_hdl->sz_ref[10] * 0.9 ; // i
        aeq_hdl->sz_ref[i + 1] = aeq_hdl->sz_ref[11] * 0.9; //

        sz_cur[i] = (sz_cur[12] + sz_cur[10]) / 2;
        sz_cur[i + 1] = (sz_cur[13] + sz_cur[11]) / 2;
    }
#endif

#if 0
    for (int i = 0; i < AEQ_FLEN; i += 2) {
        //测试代码-目前产测未接入
        aeq_hdl->sz_dut_cmp[i] = 1.0;
        aeq_hdl->sz_dut_cmp[i + 1] = 0;
        /* aeq_hdl->sz_ref[i] = 1.0; */
        /* aeq_hdl->sz_ref[i + 1] = 0; */
        /* sz_in[i  ] = 1.0; */
        /* sz_in[i + 1] = 0; */
    }
#endif
    /* put_buf((u8 *)sz_cur, 120 * 4); */

    int output_seg_num = ADAPTIVE_EQ_ORDER;
    float *lib_eq_cur = anc_malloc("ICSD_AEQ", ((3 * output_seg_num) + 1) * sizeof(float));
    //需要足够运行时间

    __adpt_aeq_cfg *aeq_cfg = anc_malloc("ICSD_AEQ", sizeof(__adpt_aeq_cfg));
    icsd_aeq_cfg_set(aeq_cfg, ext_cfg, 0, output_seg_num);

#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
    if (aeq_hdl->fre_out->sz_l_sel_idx && aeq_hdl->real_time_eq_en) {
        sz_l_sel_idx = (aeq_hdl->fre_out->sz_l_sel_idx == 101) ? 1 : aeq_hdl->fre_out->sz_l_sel_idx;
        aeq_log("aeq sz_sel %d -> %d\n", aeq_hdl->fre_out->sz_l_sel_idx, sz_l_sel_idx);
        /* fgq_getfrom_table((s16 *)eq_fgq_table, sz_l_sel_idx - 1, lib_eq_cur); */
        fgq_getfrom_table(ext_cfg->aeq_mem_iir->mem_iir, sz_l_sel_idx - 1, lib_eq_cur);
        audio_adaptive_eq_data_packet(NULL, sz_cur, lib_eq_cur, cnt, aeq_cfg);
    } else
#endif
    {
        audio_adaptive_eq_start();
        aeq_output = icsd_aeq_run(aeq_hdl->sz_ref, sz_cur, \
                                  (void *)aeq_hdl->eq_ref, aeq_hdl->sz_dut_cmp, maxgain_dB, lib_eq_cur, aeq_cfg);
        if (aeq_output->state) {
            aeq_debug_log("AEQ OUTPUT ERR!\n");
            audio_adaptive_eq_stop();
            goto __exit;
        }
        audio_adaptive_eq_data_packet(aeq_output, sz_cur, lib_eq_cur, cnt, aeq_cfg);
        audio_adaptive_eq_stop();
    }


    //格式化算法输出
    int seg_size = sizeof(struct eq_seg_info) * output_seg_num;
    eq_cur_tmp = anc_malloc("ICSD_AEQ", sizeof(struct eq_default_seg_tab));
    eq_cur_tmp->seg = anc_malloc("ICSD_AEQ", seg_size);
    struct eq_seg_info *out_seg = eq_cur_tmp->seg;

    //算法输出时，线性值转dB
    eq_cur_tmp->global_gain = 20 * log10_float(lib_eq_cur[0] < 0 ? (0 - lib_eq_cur[0]) : lib_eq_cur[0]);
    eq_cur_tmp->seg_num = output_seg_num;

    /* aeq_debug_log("Global %d\n", (int)(lib_eq_cur[0] * 10000)); */
    for (int i = 0; i < output_seg_num; i++) {
        out_seg[i].index = i;
        out_seg[i].iir_type = aeq_cfg->type[i];
        out_seg[i].freq = lib_eq_cur[i * 3 + 1];
        out_seg[i].gain = lib_eq_cur[i * 3 + 1 + 1];
        out_seg[i].q 	= lib_eq_cur[i * 3 + 2 + 1];
        /* aeq_debug_log("SEG[%d]:Type %d, FGQ:%d %d %d\n", i, out_seg[i].iir_type, \ */
        /* (int)(out_seg[i].freq * 10000), (int)(out_seg[i].gain * 10000), \ */
        /* (int)(out_seg[i].q * 10000)); */
    }

#if ICSD_AEQ_OUTPUT_PRINTF
    for (int i = aeq_cfg->iir_num_fix; i < output_seg_num; i++) {
        aeq_log("AEQ[%d] %d, %d, %d\n", i, (int)(lib_eq_cur[i * 3 + 1] * 10000), \
                (int)(lib_eq_cur[i * 3 + 2] * 10000), (int)(lib_eq_cur[i * 3 + 3] * 10000));
    }
#endif
__exit:
    anc_free(lib_eq_cur);
    anc_free(aeq_cfg);
#if (ADAPTIVE_EQ_MULTI_SZ_CUR_MEMORY == 0)
    anc_free(sz_cur);
#endif
    if (aeq_hdl->sz_ref) {
        anc_free(aeq_hdl->sz_ref);
        aeq_hdl->sz_ref = NULL;
    }
    return eq_cur_tmp;
}

static int audio_adaptive_eq_dot_run(void)
{
#if TCFG_AUDIO_FIT_DET_ENABLE
    float dot_db = audio_icsd_dot_light_open(aeq_hdl->fre_out);
    aeq_debug_log("========================================= \n");
    aeq_debug_log("                    dot db = %d/100 \n", (int)(dot_db * 100.0f));
    aeq_debug_log("========================================= \n");

    audio_anc_debug_app_send_data(ANC_DEBUG_APP_ADAPTIVE_EQ, 0x0, (u8 *)&dot_db, 4);

    if (dot_db > aeq_hdl->thr.dot_thr2) {
        aeq_debug_log(" dot: tight ");
        return AUDIO_FIT_DET_RESULT_TIGHT;
    } else if (dot_db > aeq_hdl->thr.dot_thr1) {
        aeq_debug_log(" dot: norm ");
        return AUDIO_FIT_DET_RESULT_NORMAL;
    } else { // < loose_thr
        aeq_debug_log(" dot: loose ");
        return AUDIO_FIT_DET_RESULT_LOOSE;
    }
#endif
    return 0;
}

static void audio_adaptive_eq_afq_output_hdl(struct audio_afq_output *p)
{
    u8 ignore_mode = 0;
    struct eq_default_seg_tab *eq_output;
    float maxgain_dB;

    if (p->priority == AUDIO_AFQ_PRIORITY_HIGH) {
        ignore_mode = 1;
    }
#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
#if ADAPTIVE_EQ_ONLY_IN_MUSIC_UPDATE
    //非通话/播歌状态 不更新
    if ((aeq_hdl->fre_sel == AUDIO_ADAPTIVE_FRE_SEL_ANC) && aeq_hdl->real_time_eq_en) {
        if ((!(a2dp_player_runing() || esco_player_runing())) && \
            !ignore_mode) {
            aeq_debug_log("icsd_aeq:audio idle, return\n");
            audio_anc_real_time_adaptive_resume("AEQ");
            return;
        }
    }
#endif

    if ((esco_player_runing() && (p->sz_l_sel_idx == 0)) ||
        (!(p->l_cmp_eq_update & AUDIO_ADAPTIVE_AEQ_UPDATE_FLAG))) { //通话下不支持AEQ
        if (aeq_hdl->fre_sel == AUDIO_ADAPTIVE_FRE_SEL_ANC) {
            aeq_debug_log("icsd_aeq:in_phone or AEQ no need update, return\n");
            audio_anc_real_time_adaptive_resume("AEQ");
        }
        return;
    }
    /*if (aeq_hdl->fre_sel == AUDIO_ADAPTIVE_FRE_SEL_ANC) {
        audio_anc_real_time_adaptive_suspend();
    }*/
#endif

    if (aeq_hdl->force_exit) {
        goto __aeq_close;
    }
    aeq_hdl->fre_out = p;

    aeq_log("ICSD_AEQ_STATE:RUN");

    aeq_hdl->state = ADAPTIVE_EQ_STATE_RUN;
    aeq_hdl->run_busy = 1;
    audio_adaptive_eq_sz_data_packet(p);
    /* mem_stats(); */

    aeq_hdl->thr = *(anc_ext_ear_adaptive_cfg_get()->aeq_thr);
    aeq_volume_grade_list[0] = aeq_hdl->thr.vol_thr1;
    aeq_volume_grade_list[1] = aeq_hdl->thr.vol_thr2;

#if ADAPTIVE_EQ_RUN_TIME_DEBUG
    u32 last = jiffies_usec();
#endif

#if ADAPTIVE_EQ_VOLUME_GRADE_EN
    //释放上一次AEQ存储空间
    audio_adaptive_eq_cur_list_del();
#endif

#if ADAPTIVE_EQ_TIGHTNESS_GRADE_EN
    int dot_lvl = audio_adaptive_eq_dot_run();
    /* if(dot_lvl == AUDIO_FIT_DET_RESULT_TIGHT) { */
    /* goto __aeq_close; */
    /* } */
#else
    int dot_lvl = 0;
#endif
#if ADAPTIVE_EQ_VOLUME_GRADE_EN
    //根据参数需求循环跑AEQ

#if ADAPTIVE_EQ_OUTPUT_IN_CUR_VOL_EN
    //根据当前音量计算1次AEQ
    s16 target_volume = app_audio_get_volume(APP_AUDIO_STATE_MUSIC);
    int vol_list_num = sizeof(aeq_volume_grade_list);
    for (u8 i = 0; i < vol_list_num; i++) {
        if (target_volume <= aeq_volume_grade_list[i]) {
            target_volume = aeq_volume_grade_list[i];
            maxgain_dB = aeq_hdl->thr.max_dB[dot_lvl][i];
            aeq_debug_log("max_dB %d/10, lvl %d\n", (int)(maxgain_dB * 10), aeq_volume_grade_list[i]);
            eq_output = audio_adaptive_eq_run(maxgain_dB, i);
            if (!eq_output) {
                aeq_log("aeq forced exit! i=%d\n", i);
                audio_adaptive_eq_stop();
                break;
            }
            audio_adaptive_eq_cur_list_add(aeq_volume_grade_list[i], eq_output);
            break;
        }
    }
#else
    int vol_list_num = sizeof(aeq_volume_grade_list);
    for (u8 i = 0; i < vol_list_num; i++) {
        /* maxgain_dB = 0 - audio_adaptive_eq_vol_gain_get(aeq_volume_grade_list[i]); */
        os_time_dly(2); //避免系统跑不过来
        wdt_clear();
        maxgain_dB = aeq_hdl->thr.max_dB[dot_lvl][i];
        aeq_debug_log("max_dB %d/10, lvl %d\n", (int)(maxgain_dB * 10), aeq_volume_grade_list[i]);
        eq_output = audio_adaptive_eq_run(maxgain_dB, i);
        if (!eq_output) {
            aeq_log("aeq forced exit! i=%d\n", i);
            audio_adaptive_eq_stop();
            break;
        }
        audio_adaptive_eq_cur_list_add(aeq_volume_grade_list[i], eq_output);
    }
#endif
#else
    //固定使用大音量的配置
    maxgain_dB = aeq_hdl->thr.max_dB[dot_lvl][2];
    aeq_debug_log("max_dB %d/10\n", (int)(maxgain_dB * 10));
    eq_output = audio_adaptive_eq_run(maxgain_dB, 0);
    if (eq_output) {
        audio_adaptive_eq_cur_list_add(0, eq_output);
    }

#endif

#if ADAPTIVE_EQ_RUN_TIME_DEBUG
    aeq_debug_log("ADAPTIVE EQ RUN time: %d us\n", (int)(jiffies_usec2offset(last, jiffies_usec())));
#endif
    aeq_hdl->run_busy = 0;

__aeq_close:

#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
    if (aeq_hdl->real_time_eq_en) {
        //实时AEQ 在线更新EQ效果
        if (aeq_hdl->force_exit) {
            audio_anc_real_time_adaptive_resume("AEQ");
            audio_adaptive_eq_close();
            return;
        }
        if (aeq_hdl->eff_mode == AEQ_EFF_MODE_ADAPTIVE) {
            s16 volume = app_audio_get_volume(APP_AUDIO_STATE_MUSIC);
            aeq_hdl->now_volume = volume;
            audio_icsd_eq_eff_update(audio_adaptive_eq_cur_list_query(volume));
        }

        aeq_log("ICSD_AEQ_STATE:RUN FINISH");
        if (aeq_hdl->fre_sel == AUDIO_ADAPTIVE_FRE_SEL_ANC) {
            audio_anc_real_time_adaptive_resume("AEQ");
        }
    } else
#endif
    {
        //单次AEQ 执行关闭
        audio_adaptive_eq_close();
    }
}

int audio_adaptive_eq_eff_set(enum ADAPTIVE_EFF_MODE mode)
{
    if (aeq_hdl) {
        if (audio_adaptive_eq_is_running()) {
            return -1;
        }
        aeq_debug_log("AEQ EFF mode set %d\n", mode);
        struct eq_default_seg_tab *eq_tab;
        aeq_hdl->eff_mode = mode;
        switch (mode) {
        case AEQ_EFF_MODE_DEFAULT:	//默认效果
            eq_tab = audio_icsd_eq_default_tab_read();
            audio_icsd_eq_eff_update(eq_tab);
            if (eq_tab != &default_seg_tab) {
                anc_free(eq_tab->seg);
                anc_free(eq_tab);
            }
            break;
        case AEQ_EFF_MODE_ADAPTIVE:	//自适应效果
            s16 volume = app_audio_get_volume(APP_AUDIO_STATE_MUSIC);
            audio_adaptive_eq_vol_update(volume);
            break;
        }
    }
    return 0;
}

//根据音量获取对应AEQ效果，并更新
int audio_adaptive_eq_vol_update(s16 volume)
{
    if (audio_adaptive_eq_is_running()) {
        aeq_debug_log("[AEQ_WARN] vol update fail, AEQ is running\n");
        return -1;
    }
    if (aeq_hdl->eff_mode == AEQ_EFF_MODE_DEFAULT) {
        aeq_debug_log("[AEQ] vol no need update, AEQ mode is default\n");
        return 0;
    }
    aeq_hdl->now_volume = volume;
    audio_icsd_eq_eff_update(audio_adaptive_eq_cur_list_query(volume));
    return 0;
}

static float audio_adaptive_eq_vol_gain_get(s16 volume)
{
    struct cfg_info info  = {0};             //节点配置相关信息（参数存储的目标地址、配置项大小）
    char mode_index       = 0;               //模式序号（当前节点无多参数组，mode_index是0）
    char *node_name       = ADAPTIVE_EQ_VOLUME_NODE_NAME;       //节点名称（节点内的第一参数，用户自定义,长度小于等于15byte）
    char cfg_index        = 0;               //目标配置项序号（当前节点无多参数组，cfg_index是0）
    float vol_db = 0.0f;
    int ret = jlstream_read_form_node_info_base(mode_index, node_name, cfg_index, &info);
    if (!ret) {
        struct volume_cfg *vol_cfg = anc_malloc("ICSD_AEQ", info.size);
        if (!jlstream_read_form_cfg_data(&info, (void *)vol_cfg)) {
            aeq_debug_log("[AEQ]user vol cfg parm read err\n");
            anc_free(vol_cfg);
            return vol_db;
        }
        /* aeq_log("cfg_level_max = %d\n", vol_cfg->cfg_level_max); */
        /* aeq_log("cfg_vol_min = %d\n", vol_cfg->cfg_vol_min); */
        /* aeq_log("cfg_vol_max = %d\n", vol_cfg->cfg_vol_max); */
        /* aeq_log("vol_table_custom = %d\n", vol_cfg->vol_table_custom); */
        /* aeq_log("cur_vol = %d\n", vol_cfg->cur_vol); */
        /* aeq_log("tab_len = %d\n", vol_cfg->tab_len); */

        if (info.size > sizeof(struct volume_cfg)) { //有自定义音量表, 直接返回对应音量
            vol_db = vol_cfg->vol_table[volume];
        } else {
            u16 step = (vol_cfg->cfg_level_max - 1 > 0) ? (vol_cfg->cfg_level_max - 1) : 1;
            float step_db = (vol_cfg->cfg_vol_max - vol_cfg->cfg_vol_min) / (float)step;
            vol_db = vol_cfg->cfg_vol_min + (volume - 1) * step_db;
        }
        anc_free(vol_cfg);
    } else {
        aeq_debug_log("[AEQ]user vol cfg parm read err ret %d\n", ret);
    }
    return vol_db;
}

// (单次)自适应EQ强制退出
int audio_adaptive_eq_force_exit(void)
{
    aeq_debug_log("func:%s, aeq_hdl->state:%d", __FUNCTION__, aeq_hdl->state);
    switch (aeq_hdl->state) {
    case ADAPTIVE_EQ_STATE_RUN:
        aeq_log("ICSD_AEQ_STATE:(SINGLE)FORCE_EXIT");
        aeq_hdl->force_exit = 1;
        icsd_aeq_force_exit();  // RUN才跑算法流程
        break;
    case ADAPTIVE_EQ_STATE_OPEN:
        audio_adaptive_eq_close();
        break;
    }
    return 0;
}

#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE

struct rtaeq_in_ancoff_handle {
    u8 state;			//状态
    u8 busy;			//定时器繁忙状态，不可关闭
    u8 timer_state; 	//AEQ_ANCOFF支持状态  1 打开; 0 关闭
    u32 open_timer_id; 	//记录定时器id
    u32 close_timer_id; //记录定时器id
    u32 open_times;     //配置从关闭到打开间隔时间，即关闭rtanc的时间长度，ms
    u32 close_times;    //配置从打开到关闭间隔时间，即打开rtanc的时间长度，ms
};
static struct rtaeq_in_ancoff_handle *rtaeq_in_ancoff_hdl = NULL;

static void rtaeq_ancoff_close_timer(void *p);
static void rtaeq_ancoff_open_timer(void *p)
{
    struct rtaeq_in_ancoff_handle *hdl = rtaeq_in_ancoff_hdl;
    if (anc_mode_get() != ANC_OFF) {
        return;
    }
    aeq_log("RTEQ_ANCOFF_STATE:OPEN TIMER TRIGGER\n");
    if (hdl && hdl->state) {
        hdl->busy = 1;
        hdl->open_timer_id = 0;

        hdl->timer_state = 1;
        audio_icsd_adt_scene_check(icsd_adt_app_mode_get(), 0);	//赋值当前APP模式, 便于复位启动
        audio_icsd_adt_async_reset(0);

#if RT_AEQ_ANCOFF_CLOSE_TIMES
        //重新定时关闭rtaeq
        if (hdl->close_timer_id) {
            sys_timeout_del(hdl->close_timer_id);
            hdl->close_timer_id = 0;
        }
        hdl->close_timer_id = sys_timeout_add((void *)hdl, rtaeq_ancoff_close_timer, hdl->close_times);
#endif

        hdl->busy = 0;
    }
}

static void rtaeq_ancoff_close_timer(void *p)
{
    struct rtaeq_in_ancoff_handle *hdl = rtaeq_in_ancoff_hdl;
    if (anc_mode_get() != ANC_OFF) {
        return;
    }
    aeq_log("RTEQ_ANCOFF_STATE:CLOSE TIMER TRIGGER\n");
    if (hdl && hdl->state) {
        hdl->busy = 1;
        hdl->close_timer_id = 0;

        hdl->timer_state = 0;
        audio_icsd_adt_async_reset(0);

        if (hdl->open_timer_id) {
            sys_timeout_del(hdl->open_timer_id);
            hdl->open_timer_id = 0;
        }
        hdl->open_timer_id = sys_timeout_add((void *)hdl, rtaeq_ancoff_open_timer, hdl->open_times);

        hdl->busy = 0;
    }
}

int audio_rtaeq_in_ancoff_open()
{
#if RT_AEQ_ANCOFF_TIMER_ENABLE
    aeq_log("RTEQ_ANCOFF_STATE:OPEN\n");
    if (rtaeq_in_ancoff_hdl) {
        aeq_log("rtaeq_in_ancoff_hdl is already open!!!\n");
        return -1;
    }
    struct rtaeq_in_ancoff_handle *hdl = anc_malloc("ICSD_RTAEQ", sizeof(struct rtaeq_in_ancoff_handle));
    ASSERT(hdl);

    hdl->open_times = RT_AEQ_ANCOFF_CLOSE_TIMES; //配置从关闭到打开间隔时间，即关闭rtaeq的时间长度，ms
    hdl->close_times = RT_AEQ_ANCOFF_OPEN_TIMES; //配置从打开到关闭间隔时间，即打开rtaeq的时间长度，ms
    aeq_log("RTEQ open: %d ms, close: %d ms\n", hdl->open_times, hdl->close_times);

#if RT_AEQ_ANCOFF_CLOSE_TIMES
    hdl->close_timer_id = sys_timeout_add((void *)hdl, rtaeq_ancoff_close_timer, hdl->close_times);
#endif

    hdl->timer_state = 1;
    hdl->state = 1;
    rtaeq_in_ancoff_hdl = hdl;
#endif
    return 0;

}

int audio_rtaeq_in_ancoff_close()
{
#if RT_AEQ_ANCOFF_TIMER_ENABLE
    struct rtaeq_in_ancoff_handle *hdl = rtaeq_in_ancoff_hdl;
    if (hdl) {
        hdl->state = 0;
        aeq_log("RTEQ_ANCOFF_STATE:CLOSE\n");
        while (hdl->busy) {
            putchar('w');
            os_time_dly(1);
        }
        if (hdl->open_timer_id) {
            sys_timeout_del(hdl->open_timer_id);
            hdl->open_timer_id = 0;
        }
#if RT_AEQ_ANCOFF_CLOSE_TIMES
        if (hdl->close_timer_id) {
            sys_timeout_del(hdl->close_timer_id);
            hdl->close_timer_id = 0;
        }
#endif
        anc_free(hdl);
        rtaeq_in_ancoff_hdl = NULL;
    }
#endif
    return 0;
}

u8 audio_rtaeq_ancoff_timer_state_get(void)
{
    struct rtaeq_in_ancoff_handle *hdl = rtaeq_in_ancoff_hdl;
    if (hdl && hdl->state) {
        /* aeq_log("RTEQ timer_state = %d", hdl->timer_state); */
        return hdl->timer_state;
    }
    return 1;
}

#endif


#endif/*TCFG_AUDIO_ADAPTIVE_EQ_ENABLE*/
