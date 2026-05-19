/*
 	用于管理与audio_anc.c交互部分的自适应代码
 */

#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".icsd_anc_interactive.data.bss")
#pragma data_seg(".icsd_anc_interactive.data")
#pragma const_seg(".icsd_anc_interactive.text.const")
#pragma code_seg(".icsd_anc_interactive.text")
#endif

#include "app_config.h"
#include "audio_config_def.h"
#if (TCFG_AUDIO_ANC_EAR_ADAPTIVE_EN && \
	 TCFG_AUDIO_ANC_ENABLE && \
	 TCFG_AUDIO_ANC_EAR_ADAPTIVE_VERSION == ANC_EXT_V2)

#include "audio_anc_includes.h"
#include "clock_manager/clock_manager.h"
#include "audio_config.h"
#include "app_tone.h"
#include "esco_player.h"
#include "a2dp_player.h"
#include "le_audio_player.h"
#include "dac_node.h"
#include "adc_file.h"
#if TCFG_USER_TWS_ENABLE
#include "bt_tws.h"
#endif/*TCFG_USER_TWS_ENABLE*/
#include "adc_file.h"

#if 1
#define anc_log	printf
#else
#define anc_log(...)
#endif/*log_en*/

//按照自适应结果播放对应数字的提示音, 头戴式会先播放左边，在播放右边
#define ANC_EAR_ADAPTIVE_RESULT_TONE_EN		0

enum EAR_ADAPTIVE_STATE {
    EAR_ADAPTIVE_STATE_END = 0,			//耳道自适应训练结束
    EAR_ADAPTIVE_STATE_INIT,			//耳道自适应初始化

    EAR_ADAPTIVE_STATE_ALOGM_START,		//算法启动（提示音播放前）
    EAR_ADAPTIVE_STATE_ALOGM_PART1,		//算法 part 1
    EAR_ADAPTIVE_STATE_ALOGM_PART2,		//算法 part 2
    EAR_ADAPTIVE_STATE_ALOGM_STOP,		//算法结束

    EAR_ADAPTIVE_STATE_END_CHECK,		//训练结束：检查TWS是否都结束
    EAR_ADAPTIVE_STATE_END_SYNC,		//训练结束：执行同步结束流程
    EAR_ADAPTIVE_STATE_END_TIMEOUT,		//训练结束：执行同步超时
};

enum EAR_ADAPTIVE_TONE_STATE {
    EAR_ADAPTIVE_TONE_STATE_STOP = 0,
    EAR_ADAPTIVE_TONE_STATE_START,
    EAR_ADAPTIVE_TONE_STATE_NO_SLIENCE,
};

struct anc_ear_adaptive_param {
    u8 tws_sync;         	 		/*左右耳平衡使能*/
    u8 seq;							/*seq计数*/
    u8 dac_check_slience_flag;		/*DAC静音检测标志*/
    u8 ear_adaptive_data_from;		/*滤波器数据来源*/
    u8 tone_state;					/*TONE state*/
    u8 adaptive_state;				/*ADAPTIVE state*/
    u8 forced_exit_default_flag;	/*强退恢复默认ANC标志*/
    u8 forced_exit;					/*强退标志*/
    u8 next_mode;					/*自适应结束后目标模式*/
    u8 tws_end_wait_flag;			/*TWS 结束等待标识：对方已经结束，本地未结束*/
    u16 tws_end_timeout_id;			/*TWS 命令超时标志*/
    audio_anc_t *param;
    OS_SEM exit_sem;				/*自适应退出信号量*/
    anc_adaptive_iir_t adaptive_iir;

    struct anc_ear_adaptive_train_cfg train_cfg;	/*训练参数*/
};

anc_packet_data_t *anc_adaptive_data = NULL;
static struct anc_ear_adaptive_param *hdl = NULL;

static void audio_anc_adaptive_poweron_catch_data(anc_adaptive_iir_t *iir);
static void audio_anc_adaptive_data_format(anc_adaptive_iir_t *iir);
static int audio_anc_coeff_adaptive_set_base(u32 mode, u8 tone_play, u8 ignore_busy);
static void anc_ear_adaptive_icsd_anc_init(void);

/* 参数初始化 */
void anc_ear_adaptive_init(audio_anc_t *param)
{
    //内部使用句柄
    hdl = anc_malloc("ICSD_ANC_APP", sizeof(struct anc_ear_adaptive_param));
    hdl->ear_adaptive_data_from = ANC_ADAPTIVE_DATA_EMPTY;
    os_sem_create(&hdl->exit_sem, 0);
    //库关联句柄
    hdl->param = param;
    param->adaptive = anc_malloc("ICSD_ANC_APP", sizeof(anc_ear_adaptive_param_t));
    ASSERT(param->adaptive);
    hdl->train_cfg.ff_coeff = anc_malloc("ICSD_ANC_APP", ANC_ADAPTIVE_FF_ORDER * sizeof(double) * 5);
    param->adaptive->ff_yorder = ANC_ADAPTIVE_FF_ORDER;
    param->adaptive->fb_yorder = ANC_ADAPTIVE_FB_ORDER;
#if ANC_EAR_ADAPTIVE_CMP_EN
    param->adaptive->cmp_yorder = ANC_ADAPTIVE_CMP_ORDER;
#endif
    param->adaptive->dma_done_cb = icsd_anc_v2_dma_done;
}

/* ANC TASK 命令回调处理 */
void anc_ear_adaptive_cmd_handler(audio_anc_t *param, int *msg)
{
    if (hdl == NULL) {
        return;
    }
    switch (msg[1]) {
    case ANC_MSG_ICSD_ANC_V2_CMD:
        icsd_anc_v2_cmd_handler((u8)msg[2], param);
        break;
    case ANC_MSG_ICSD_ANC_V2_INIT:
        anc_ear_adaptive_icsd_anc_init();
        break;
    }
}


extern OS_SEM icsd_anc_sem;
void icsd_anc_v2_tone_play_start()
{
    printf("ICSD ANC V2 TONE PLAY START\n");
    os_sem_create(&icsd_anc_sem, 0);

    if (strcmp(os_current_task(), "anc") == 0) {
        printf(">>>>>>>>>>>>>> no post anc task\n");
        anc_ear_adaptive_icsd_anc_init();
        os_sem_set(&icsd_anc_sem, 0);
    } else {
        os_taskq_post_msg("anc", 1, ANC_MSG_ICSD_ANC_V2_INIT);
        os_sem_pend(&icsd_anc_sem, 300);	//等待ANC初始化完毕，超时300 * 10ms退出
    }
}

/* 耳道自适应提示音启动接口 */
void anc_ear_adaptive_tone_play_start(void)
{
    anc_log("EAR_ADAPTIVE_TONE_STATE:START\n");
    hdl->tone_state = EAR_ADAPTIVE_TONE_STATE_START;
    audio_mic_pwr_ctl(MIC_PWR_ON);
    audio_anc_dac_open(hdl->param->gains.dac_gain, hdl->param->gains.dac_gain);
#ifdef CONFIG_CPU_BR28
    hdl->param->adc_set_buffs_cb();
#endif
    audio_anc_mic_open(hdl->param->mic_param, 0, hdl->param->adc_ch);
#if ANC_USER_TRAIN_TONE_MODE
    if (anc_mode_get() == ANC_ON) {
        anc_log("EAR_ADAPTIVE_STATE:ALOGM_START\n");
        hdl->adaptive_state = EAR_ADAPTIVE_STATE_ALOGM_START;
        icsd_anc_v2_tone_play_start();
    }
    //准备播提示音才开始起标志
    hdl->dac_check_slience_flag = 1;
#endif/*ANC_USER_TRAIN_TONE_MODE*/
}

/* 耳道自适应提示音播放异常回调接口 */
void anc_ear_adaptive_tone_play_err_cb(void)
{
    anc_log("EAR_ADAPTIVE_TONE_STATE:ERR \n");
    icsd_anc_v2_forced_exit();
}

/* 耳道自适应提示音播放结束回调接口 */
void anc_ear_adaptive_tone_play_cb(void)
{
    anc_log("EAR_ADAPTIVE_TONE_STATE:STOP\n");
    hdl->tone_state = EAR_ADAPTIVE_TONE_STATE_STOP;
    if (hdl->adaptive_state == EAR_ADAPTIVE_STATE_ALOGM_PART2) {
        if (hdl->forced_exit) {
            return;
        }
        icsd_anc_v2_part2_run();
    }
}

void anc_ear_adaptive_part1_end_cb(void *priv)
{
    if (hdl->forced_exit) {
        return;
    }
    if (hdl->adaptive_state == EAR_ADAPTIVE_STATE_ALOGM_PART1) {
        anc_log("EAR_ADAPTIVE_STATE:ALOGM_PART2\n");
        hdl->adaptive_state = EAR_ADAPTIVE_STATE_ALOGM_PART2;
        if (hdl->tone_state == EAR_ADAPTIVE_TONE_STATE_STOP) {
            icsd_anc_v2_part2_run();
        }
    } else {
        anc_log("Error: ANC_EXT part1 abnormal state %d!\n", hdl->adaptive_state);
    }
}

/* 耳道自适应开发者模式设置 */
void anc_ear_adaptive_develop_set(u8 mode)
{
    if (anc_mode_get() != ANC_OFF) {
        anc_mode_switch(ANC_OFF, 0);									//避免右耳参数异常，让调试者感受不适
    }
    if (hdl->param->anc_coeff_mode == ANC_COEFF_MODE_ADAPTIVE) {
        return;															//自适应模式不更新参数
    }
    if (mode) { //0 -> 1
#if TCFG_AUDIO_ANC_MULT_ORDER_ENABLE
        anc_mult_scene_set(audio_anc_mult_scene_get());
#endif
        audio_anc_param_map(1, 1);										//映射ANC参数
        audio_anc_adaptive_poweron_catch_data(&hdl->adaptive_iir);	//打包自适应数据,等待工具读取
    } else {    //1 -> 0
        audio_anc_db_cfg_read();										//读取ANC配置区的数据
        if (anc_adaptive_data) {
            free_anc_data_packet(anc_adaptive_data);
            anc_adaptive_data = NULL;
        }
    }
}

/* DAC非静音数据回调 */
static void audio_anc_dac_check_slience_cb(void *buf, int len)
{
    if (hdl->dac_check_slience_flag) {
        s16 *check_buf = (s16 *)buf;
        for (int i = 0; i < len / 2; i++) {
            if (check_buf[i]) {
                if (hdl->forced_exit) {
                    return;
                }
                hdl->dac_check_slience_flag = 0;
                anc_log("EAR_ADAPTIVE_TONE_STATE:NO_SLIENCE\n");
                hdl->tone_state = EAR_ADAPTIVE_TONE_STATE_NO_SLIENCE;
                anc_log("EAR_ADAPTIVE_STATE:ALOGM_PART1\n");
                hdl->adaptive_state = EAR_ADAPTIVE_STATE_ALOGM_PART1;
                icsd_anc_v2_part1_run(i, dac_hdl.sample_rate, anc_ear_adaptive_part1_end_cb);
                break;
            }
        }
    }
}

/* 耳道自适应启动限制 */
int audio_anc_mode_ear_adaptive_permit(void)
{
    int ret;
#if AUDIO_ANC_DATA_EXPORT_VIA_UART
    if (audio_anc_develop_is_running()) {
        printf("EAR_ANC_STATE:ERR ANC_DEV OPEN NOW\n");
        return -1;
    }
#endif
    ret = anc_ext_ear_adaptive_param_check();
    if (ret) { //没有自适应参数
        return ret;
    }
    if (hdl->adaptive_state != EAR_ADAPTIVE_STATE_END) { //重入保护
        return ANC_EXT_OPEN_FAIL_REENTRY;
    }
    if (adc_file_is_runing()) { //通话不支持
        /* if (esco_player_runing()) { //通话不支持 */
        return ANC_EXT_OPEN_FAIL_CONFLICT_PHONE;
    }
    if (anc_mode_switch_lock_get()) { //其他切模式过程不支持
        return ANC_EXT_OPEN_FAIL_SWITCH_LOCK;
    }
#if TCFG_AUDIO_FREQUENCY_GET_ENABLE
    if (audio_icsd_afq_is_running()) {	//AFQ 运行过程中不支持
        return ANC_EXT_OPEN_FAIL_AFQ_RUNNING;
    }
#endif
#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
    if (audio_anc_real_time_adaptive_state_get()) {	//RTANC运行过程不支持
        return ANC_EXT_OPEN_FAIL_CONFLICT_RTANC;
    }
#endif
    return 0;
}

int audio_anc_ear_adaptive_open(void)
{
    if (anc_mode_get() != ANC_ON) {
        //非ANC模式启动耳道自适应时，因库关联函数较多，需设置为ANC_ON打开
        audio_anc_mode_set(ANC_ON);
    }
#if ANC_EAR_ADAPTIVE_CMP_EN
    audio_anc_ear_adaptive_cmp_open(CMP_FROM_ANC_EAR_ADAPTIVE);
#endif

#if ANC_MULT_ORDER_ENABLE
    //载入场景参数-提供自适应训练过程使用
    u8 scene_id = ANC_MULT_ADPTIVE_TRAIN_USE_ID ? ANC_MULT_ADPTIVE_TRAIN_USE_ID : audio_anc_mult_scene_get();
    anc_mult_scene_set(scene_id);
#endif/*ANC_MULT_ORDER_ENABLE*/

    //ANC切模式流程自适应标志，播放完提示音会清0
    audio_ear_adaptive_en_set(1);

    //加载训练参数
    hdl->param->anc_coeff_mode = ANC_COEFF_MODE_ADAPTIVE;	//切到自适应的参数
    audio_anc_ear_adaptive_param_init(hdl->param);
#if ANC_USER_TRAIN_TONE_MODE
    dac_node_write_callback_add("ANC_ADAP", 0XFF, audio_anc_dac_check_slience_cb);	//注册DAC回调接口-静音检测
#endif/*ANC_USER_TRAIN_TONE_MODE*/
    /*自适应时需要挂起adt*/
#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
    audio_icsd_adt_scene_set(ADT_SCENE_EAR_ADAPTIVE, 1);
    if (audio_icsd_adt_is_running()) {
        audio_icsd_adt_reset(ADT_SCENE_EAR_ADAPTIVE);
    }
#endif
    anc_mode_switch(ANC_ON, ANC_USER_TRAIN_TONE_MODE);
    return 0;
}

int audio_anc_ear_adaptive_a2dp_suspend_cb(void)
{
    if (hdl->adaptive_state == EAR_ADAPTIVE_STATE_INIT) {
        int msg[2];
        msg[0] = (int)audio_anc_ear_adaptive_open;
        msg[1] = 1;
        if (os_taskq_post_type("anc", Q_CALLBACK, 2, msg)) {
            anc_log("anc ear adaptive taskq_post err\n");
        }
    }
    return 0;
}

void audio_anc_mode_ear_adaptive_sync_cb(void *_data, u16 len, bool rx)
{
    int ret = audio_anc_mode_ear_adaptive_permit();
    if (ret) {
        anc_log("anc ear adaptive open fail %d\n", ret);
#if (RCSP_ADV_EN && RCSP_ADV_ANC_VOICE && RCSP_ADV_ADAPTIVE_NOISE_REDUCTION)
        set_adaptive_noise_reduction_reset_callback(0);		//	无法进入自适应，返回失败结果
#endif
        audio_anc_debug_err_send_data(ANC_DEBUG_APP_EAR_ADAPTIVE, &ret, sizeof(int));
        return;
    }
    hdl->tws_sync = ((u8 *)_data)[0];
    anc_log("tws_sync:%d\n", hdl->tws_sync);

    anc_log("EAR_ADAPTIVE_STATE:INIT\n");
    hdl->adaptive_state = EAR_ADAPTIVE_STATE_INIT;

    if (a2dp_player_runing()
#if LE_AUDIO_STREAM_ENABLE
        || le_audio_player_is_playing()
#endif
       ) {	//当前处于音乐播放状态, 注册解码任务打断，进入自适应
        jlstream_global_pause();
    } else {
        audio_anc_ear_adaptive_a2dp_suspend_cb();
    }
}

#define TWS_FUNC_ID_ANC_EAR_ADAPTIVE_SYNC    TWS_FUNC_ID('A', 'D', 'A', 'P')
#if TCFG_USER_TWS_ENABLE
REGISTER_TWS_FUNC_STUB(anc_ear_adaptive_mode_sync) = {
    .func_id = TWS_FUNC_ID_ANC_EAR_ADAPTIVE_SYNC,
    .func    = audio_anc_mode_ear_adaptive_sync_cb,
};
#endif

/*自适应模式-重新检测
 * param: tws_sync_en          1 TWS同步自适应，支持TWS降噪平衡，需左右耳一起调用此接口
 *                             0 单耳自适应, 不支持TWS降噪平衡，可TWS状态下单耳自适应
 */
int audio_anc_mode_ear_adaptive(u8 tws_sync_en)
{
#if TCFG_USER_TWS_ENABLE
    if (get_tws_sibling_connect_state() && tws_sync_en) {
        if ((tws_api_get_role() == TWS_ROLE_MASTER)) {
            /*主机同步打开*/
            tws_api_send_data_to_sibling(&tws_sync_en, sizeof(tws_sync_en), TWS_FUNC_ID_ANC_EAR_ADAPTIVE_SYNC);
        }
    } else
#endif
    {
        audio_anc_mode_ear_adaptive_sync_cb(&tws_sync_en, sizeof(tws_sync_en), 0);
    }
    return 0;
}

#if TCFG_USER_TWS_ENABLE

static void anc_ear_adaptive_end_tws_timeout(void *priv)
{
    if (hdl->adaptive_state == EAR_ADAPTIVE_STATE_END_CHECK) {
        hdl->adaptive_state = EAR_ADAPTIVE_STATE_END_TIMEOUT;
        hdl->tws_end_wait_flag = 0;
        anc_log("EAR_ADAPTIVE_STATE:TIMEOUT\n");
        anc_mode_switch_deal(hdl->next_mode);
    }
}

#define TWS_FUNC_ID_ANC_EAR_ADAPTIVE_END_SYNC    		 TWS_FUNC_ID('A', 'E', 'A', 'S')
static void anc_ear_adaptive_end_tws_sync_cb(int mode, int err)
{
    if (hdl->adaptive_state == EAR_ADAPTIVE_STATE_END_CHECK) {
        sys_timer_del(hdl->tws_end_timeout_id);
        hdl->adaptive_state = EAR_ADAPTIVE_STATE_END_SYNC;
        hdl->tws_end_wait_flag = 0;
        anc_log("EAR_ADAPTIVE_STATE:END_SYNC\n");
        anc_mode_switch_deal(hdl->next_mode);
    }
}

TWS_SYNC_CALL_REGISTER(tws_anc_adaptive_sync) = {
    .uuid = TWS_FUNC_ID_ANC_EAR_ADAPTIVE_END_SYNC,
    .task_name = "anc",
    .func = anc_ear_adaptive_end_tws_sync_cb,
};

#define TWS_FUNC_ID_ANC_EAR_ADAPTIVE_END_CHECK    TWS_FUNC_ID('A', 'E', 'A', 'C')
static void anc_ear_adaptive_end_tws_check_cb(void *_data, u16 len, bool rx)
{
    if (rx) {
        anc_log("EAR ADAPTIVE TWS CHECK: slibing_state = %d\n", hdl->adaptive_state);
        switch (hdl->adaptive_state) {
        case EAR_ADAPTIVE_STATE_END_CHECK:
            tws_api_sync_call_by_uuid(TWS_FUNC_ID_ANC_EAR_ADAPTIVE_END_SYNC, hdl->next_mode, 150);
            break;
        case EAR_ADAPTIVE_STATE_ALOGM_STOP:
            hdl->tws_end_wait_flag = 1;
            break;
        default:
            anc_log("ERR!!\n");
            break;
        }
    }
}

REGISTER_TWS_FUNC_STUB(tws_anc_power_adaptive_compare) = {
    .func_id = TWS_FUNC_ID_ANC_EAR_ADAPTIVE_END_CHECK,
    .func    = anc_ear_adaptive_end_tws_check_cb,
};

static int anc_ear_adaptive_end_tws_check(u8 state)
{
    int ret = -EINVAL;
    local_irq_disable();
    hdl->tws_end_timeout_id = sys_timeout_add(NULL, anc_ear_adaptive_end_tws_timeout, 2000);
    if (hdl->tws_end_wait_flag) {
        hdl->tws_end_wait_flag = 0;
        //对方已经结束，执行同步结束处理
        ret = tws_api_sync_call_by_uuid(TWS_FUNC_ID_ANC_EAR_ADAPTIVE_END_SYNC, hdl->next_mode, 150);
    } else {
        ret = tws_api_send_data_to_sibling(&state, 1, TWS_FUNC_ID_ANC_EAR_ADAPTIVE_END_CHECK);
    }
    local_irq_enable();
    return ret;
}
#endif

int anc_ear_adaptive_close(void)
{
    //当自适应运行停止，并且使用自适应输出频响的应用全部关闭时，才能结束
    anc_log("%s\n", __func__);
    if ((hdl->adaptive_state == EAR_ADAPTIVE_STATE_ALOGM_STOP) && \
        (audio_afq_common_app_is_active() == 0)) {
        //恢复播歌
        clock_free("ANC_ADAP");
        jlstream_global_resume();

        /*
         * 关闭耳道自适应时删除dac写数回调
         * 原因：若在写数回调中删除，会出现链表轮询嵌套，
         *       此时删除两个以上链表时存在异常
        */
        dac_node_write_callback_del("ANC_ADAP");

        anc_ext_tool_ear_adaptive_end_cb(hdl->adaptive_iir.result);
        //强退模式设置不恢复默认ANC

        os_sem_set(&hdl->exit_sem, 0);
        os_sem_post(&hdl->exit_sem);

        if (!hdl->forced_exit_default_flag && hdl->forced_exit) {
            anc_mode_switch_lock_clean();
            anc_ear_adaptive_mode_end();
            return 0;
        }
#if TCFG_USER_TWS_ENABLE
        if (get_tws_sibling_connect_state() && hdl->tws_sync) {
            hdl->adaptive_state = EAR_ADAPTIVE_STATE_END_CHECK;
            anc_log("EAR_ADAPTIVE_STATE:END_CHECK\n");
            anc_ear_adaptive_end_tws_check(hdl->adaptive_state);
        } else {
            anc_mode_switch_deal(hdl->next_mode);
        }
#else
        anc_mode_switch_deal(hdl->next_mode);
        anc_log("%s ok\n", __func__);
#endif
    }
    return 0;
}

/* 自适应结束回调函数 */
void anc_user_train_cb(u8 mode, u8 forced_exit)
{
    anc_log("anc_adaptive end, coeff_use 0x%x\n", hdl->adaptive_iir.result);
    audio_anc_t *param = hdl->param;
    anc_log("EAR_ADAPTIVE_STATE:ALOGM_STOP\n");
    hdl->adaptive_state = EAR_ADAPTIVE_STATE_ALOGM_STOP;
    hdl->forced_exit = forced_exit;
    hdl->next_mode = mode;

    audio_ear_adaptive_train_app_resume();	//恢复APP互斥功能
    if (forced_exit == 1) {
        //强制打断，自适应定义为失败，使用默认参数
        hdl->adaptive_iir.result = 0;
        //删除自适应DAC回调接口
        hdl->dac_check_slience_flag = 0;
        dac_node_write_callback_del("ANC_ADAP");
    }
    //测试模式：自适应结果必定失败

    if (hdl->adaptive_iir.result) {	//自适应成功或部分成功
        if (hdl->ear_adaptive_data_from == ANC_ADAPTIVE_DATA_FROM_VM) {		//释放掉原来VM使用的资源
            if (hdl->param->adaptive->lff_coeff) {
                anc_free(hdl->param->adaptive->lff_coeff);
                hdl->param->adaptive->lff_coeff = NULL;
            };
            if (hdl->param->adaptive->lfb_coeff) {
                anc_free(hdl->param->adaptive->lfb_coeff);
                hdl->param->adaptive->lfb_coeff = NULL;
            };
            if (hdl->param->adaptive->lcmp_coeff) {
                anc_free(hdl->param->adaptive->lcmp_coeff);
                hdl->param->adaptive->lcmp_coeff = NULL;
            };
            if (hdl->param->adaptive->rff_coeff) {
                anc_free(hdl->param->adaptive->rff_coeff);
                hdl->param->adaptive->rff_coeff = NULL;
            };
            if (hdl->param->adaptive->rfb_coeff) {
                anc_free(hdl->param->adaptive->rfb_coeff);
                hdl->param->adaptive->rfb_coeff = NULL;
            };
            if (hdl->param->adaptive->rcmp_coeff) {
                anc_free(hdl->param->adaptive->rcmp_coeff);
                hdl->param->adaptive->rcmp_coeff = NULL;
            };
        }

        //将算法库FGQ转为AABB
        hdl->ear_adaptive_data_from = ANC_ADAPTIVE_DATA_EMPTY;
        audio_anc_adaptive_data_format(&hdl->adaptive_iir);

        audio_anc_coeff_adaptive_set_base(ANC_COEFF_MODE_NORMAL, 0, 1);	//先覆盖默认参数
        audio_anc_coeff_adaptive_set_base(ANC_COEFF_MODE_ADAPTIVE, 0, 1);	//根据自适应结果选择覆盖参数

#if (!ANC_POWEOFF_SAVE_ADAPTIVE_DATA)
        audio_anc_adaptive_data_save(&hdl->adaptive_iir);
#endif/*ANC_POWEOFF_SAVE_ADAPTIVE_DATA*/
    } else {	//自适应失败使用默认的参数
        audio_anc_coeff_adaptive_set_base(ANC_COEFF_MODE_NORMAL, 0, 1);
    }

#if ANC_EAR_ADAPTIVE_CMP_EN
    //关闭CMP
    audio_anc_ear_adaptive_cmp_close();
#endif

#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
    audio_icsd_adt_scene_set(ADT_SCENE_EAR_ADAPTIVE, 0);
    //后续anc_mode_switch会复位ADT
#endif
    anc_ear_adaptive_close();
}

/* ANC滤波器模式循环切换 */
int audio_anc_coeff_adaptive_switch()
{
    if (anc_mode_get() == ANC_ON && !anc_ear_adaptive_busy_get() && !anc_mode_switch_lock_get()) {
        hdl->param->anc_coeff_mode ^= 1;
        audio_anc_coeff_adaptive_update(hdl->param->anc_coeff_mode, 1);
    }
    return 0;
}

/* 当前ANC滤波器模式获取 0:普通参数 1:自适应参数 */
int audio_anc_coeff_mode_get(void)
{
    if (!hdl) {
        return 0;
    }
    return hdl->param->anc_coeff_mode;
}

/*
   自适应/普通参数切换(只切换参数，不更新效果)
	param:  mode 		0 使用普通参数; 1 使用自适应参数
			tone_play 	0 不播放提示音；1 播放提示音
 */
int audio_anc_coeff_adaptive_set(u32 mode, u8 tone_play)
{
    return audio_anc_coeff_adaptive_set_base(mode, tone_play, 0);
}

/*
   自适应/普通参数切换(切换参数，并立即更新效果)
	param:  mode 		0 使用普通参数; 1 使用自适应参数
			tone_play 	0 不播放提示音；1 播放提示音
 */
int audio_anc_coeff_adaptive_update(u32 mode, u8 tone_play)
{
    int ret = audio_anc_coeff_adaptive_set(mode, tone_play);
    audio_anc_param_reset(1);
    return ret;
}

static int audio_anc_coeff_adaptive_set_base(u32 mode, u8 tone_play, u8 ignore_busy)
{
    u8 result = hdl->adaptive_iir.result;
    if (anc_ear_adaptive_busy_get() && !ignore_busy) {
        return 0;
    }
    hdl->param->anc_coeff_mode = mode;
    switch (mode) {
    case 0:
        if (tone_play) {
            if (ANC_TONE_PREEMPTION) {
                play_tone_file_alone(get_tone_files()->anc_normal_coeff);
            } else {
                play_tone_file(get_tone_files()->anc_normal_coeff);
            }
            /* tone_play_index(IDEX_TONE_ANC_NORMAL_COEFF, ANC_TONE_PREEMPTION); */
        }
#if ANC_COEFF_SAVE_ENABLE
#if ANC_MULT_ORDER_ENABLE
        //自适应切默认参数，回到当前场景
        anc_mult_scene_set(audio_anc_mult_scene_get());
#else
        if (audio_anc_db_cfg_read()) {
            return 1;
        }
#endif/*ANC_MULT_ORDER_ENABLE*/
#endif/*ANC_COEFF_SAVE_ENABLE*/
        break;
    case 1:
        if (((!hdl->param->adaptive->lff_coeff) && hdl->param->lff_en && (result & ANC_ADAPTIVE_RESULT_LFF)) || \
            ((!hdl->param->adaptive->lfb_coeff) && hdl->param->lfb_en && (result & ANC_ADAPTIVE_RESULT_LFB)) || \
            ((!hdl->param->adaptive->lcmp_coeff) && hdl->param->lfb_en && (result & ANC_ADAPTIVE_RESULT_LCMP)) || \
            ((!hdl->param->adaptive->rff_coeff) && hdl->param->rff_en && (result & ANC_ADAPTIVE_RESULT_RFF)) || \
            ((!hdl->param->adaptive->rfb_coeff) && hdl->param->rfb_en && (result & ANC_ADAPTIVE_RESULT_RFB)) || \
            ((!hdl->param->adaptive->rcmp_coeff) && hdl->param->rfb_en && (result & ANC_ADAPTIVE_RESULT_RCMP))) {
            g_printf("anc no adaptive data\n");
            return 1;
        }
        if (tone_play) {
            if (ANC_TONE_PREEMPTION) {
                play_tone_file_alone(get_tone_files()->anc_adaptive_coeff);
            } else {
                play_tone_file(get_tone_files()->anc_adaptive_coeff);
            }
            /* tone_play_index(IDEX_TONE_ANC_ADAPTIVE_COEFF, ANC_TONE_PREEMPTION); */
        }
#if ANC_MULT_ORDER_ENABLE
        //载入场景参数-提供自适应部分成功时，匹配使用。
        u8 scene_id = ANC_MULT_ADPTIVE_MATCH_USE_ID ? ANC_MULT_ADPTIVE_MATCH_USE_ID : audio_anc_mult_scene_get();
        anc_mult_scene_set(scene_id);
#endif/*ANC_MULT_ORDER_ENABLE*/
        audio_anc_ear_adaptive_param_init(hdl->param);	//初始化自适应固定配置参数
#if TCFG_AUDIO_ANC_CH & ANC_L_CH
        if (result & ANC_ADAPTIVE_RESULT_LFF) {
            anc_log("adaptive coeff set lff\n");
            hdl->param->gains.l_ffgain = (hdl->adaptive_iir.lff_gain < 0) ? (0 - hdl->adaptive_iir.lff_gain) : hdl->adaptive_iir.lff_gain;
            hdl->param->lff_yorder = ANC_ADAPTIVE_FF_ORDER;
            hdl->param->lff_coeff = hdl->param->adaptive->lff_coeff;
        }
        if (result & ANC_ADAPTIVE_RESULT_LFB) {
            anc_log("adaptive coeff set lfb\n");
            hdl->param->gains.l_fbgain = (hdl->adaptive_iir.lfb_gain < 0) ? (0 - hdl->adaptive_iir.lfb_gain) : hdl->adaptive_iir.lfb_gain;
            hdl->param->lfb_yorder = ANC_ADAPTIVE_FB_ORDER;
            hdl->param->lfb_coeff = hdl->param->adaptive->lfb_coeff;
        }
#if ANC_EAR_ADAPTIVE_CMP_EN
        if (result & ANC_ADAPTIVE_RESULT_LCMP) {
            anc_log("adaptive coeff set lcmp\n");
            hdl->param->gains.l_cmpgain = (hdl->adaptive_iir.lcmp_gain < 0) ? (0 - hdl->adaptive_iir.lcmp_gain) : hdl->adaptive_iir.lcmp_gain;
            hdl->param->lcmp_yorder = ANC_ADAPTIVE_CMP_ORDER;
            hdl->param->lcmp_coeff = hdl->param->adaptive->lcmp_coeff;
        }
#endif/*ANC_EAR_ADAPTIVE_CMP_EN*/
#endif/*TCFG_AUDIO_ANC_CH*/

#if AUDIO_ANC_STEREO_ENABLE
        if (result & ANC_ADAPTIVE_RESULT_RFF) {
            anc_log("adaptive coeff set rff\n");
            hdl->param->gains.r_ffgain = (hdl->adaptive_iir.rff_gain < 0) ? (0 - hdl->adaptive_iir.rff_gain) : hdl->adaptive_iir.rff_gain;
            hdl->param->rff_yorder = ANC_ADAPTIVE_FF_ORDER;
            hdl->param->rff_coeff = hdl->param->adaptive->rff_coeff;
        }
        if (result & ANC_ADAPTIVE_RESULT_RFB) {
            anc_log("adaptive coeff set rfb\n");
            hdl->param->gains.r_fbgain = (hdl->adaptive_iir.rfb_gain < 0) ? (0 - hdl->adaptive_iir.rfb_gain) : hdl->adaptive_iir.rfb_gain;
            hdl->param->rfb_yorder = ANC_ADAPTIVE_FB_ORDER;
            hdl->param->rfb_coeff = hdl->param->adaptive->rfb_coeff;
        }
#if ANC_EAR_ADAPTIVE_CMP_EN
        if (result & ANC_ADAPTIVE_RESULT_RCMP) {
            anc_log("adaptive coeff set rcmp\n");
            hdl->param->gains.r_cmpgain = (hdl->adaptive_iir.rcmp_gain < 0) ? (0 - hdl->adaptive_iir.rcmp_gain) : hdl->adaptive_iir.rcmp_gain;
            hdl->param->rcmp_yorder = ANC_ADAPTIVE_CMP_ORDER;
            hdl->param->rcmp_coeff = hdl->param->adaptive->rcmp_coeff;
        }
#endif/*ANC_EAR_ADAPTIVE_CMP_EN*/
#endif/*AUDIO_ANC_STEREO_ENABLE*/

        break;
    default:
        return 1;
    }

    //put_buf(param->lff_coeff, ANC_ADAPTIVE_FF_ORDER * 5 * 8);
    //put_buf(param->lfb_coeff, ANC_ADAPTIVE_FB_ORDER * 5 * 8);
    //put_buf(param->lcmp_coeff, ANC_ADAPTIVE_CMP_ORDER * 5 * 8);
    return 0;
}

static void audio_anc_adaptive_data_format(anc_adaptive_iir_t *iir)
{
    anc_iir_t iir_lib = {0};	//	库与外部指针分离
    u8 result = hdl->adaptive_iir.result;
    if (hdl->ear_adaptive_data_from == ANC_ADAPTIVE_DATA_EMPTY) {
        hdl->ear_adaptive_data_from = ANC_ADAPTIVE_DATA_FROM_VM;
#if ANC_CONFIG_LFF_EN
        if (hdl->param->lff_en && (result & ANC_ADAPTIVE_RESULT_LFF)) {
            hdl->param->adaptive->lff_coeff = anc_malloc("ICSD_ANC_APP", ANC_ADAPTIVE_FF_ORDER * sizeof(double) * 5);
            iir_lib.lff = hdl->adaptive_iir.lff;
        }
#endif/*ANC_CONFIG_LFF_EN*/
#if ANC_CONFIG_LFB_EN
        if (hdl->param->lfb_en) {
            if (result & ANC_ADAPTIVE_RESULT_LFB) {
                hdl->param->adaptive->lfb_coeff = anc_malloc("ICSD_ANC_APP", ANC_ADAPTIVE_FB_ORDER * sizeof(double) * 5);
                iir_lib.lfb = hdl->adaptive_iir.lfb;
            }
#if ANC_EAR_ADAPTIVE_CMP_EN
            if (result & ANC_ADAPTIVE_RESULT_LCMP) {
                hdl->param->adaptive->lcmp_coeff = anc_malloc("ICSD_ANC_APP", ANC_ADAPTIVE_CMP_ORDER * sizeof(double) * 5);
                iir_lib.lcmp = hdl->adaptive_iir.lcmp;
            }
#endif/*ANC_EAR_ADAPTIVE_CMP_EN*/
        }
#endif/*ANC_CONFIG_LFB_EN*/
#if ANC_CONFIG_RFF_EN && AUDIO_ANC_STEREO_ENABLE
        if (hdl->param->rff_en && (result & ANC_ADAPTIVE_RESULT_RFF)) {
            hdl->param->adaptive->rff_coeff = anc_malloc("ICSD_ANC_APP", ANC_ADAPTIVE_FF_ORDER * sizeof(double) * 5);
            iir_lib.rff = hdl->adaptive_iir.rff;
        }
#endif/*ANC_CONFIG_RFF_EN*/
#if ANC_CONFIG_RFB_EN && AUDIO_ANC_STEREO_ENABLE
        if (hdl->param->rfb_en) {
            if (result & ANC_ADAPTIVE_RESULT_RFB) {
                hdl->param->adaptive->rfb_coeff = anc_malloc("ICSD_ANC_APP", ANC_ADAPTIVE_FB_ORDER * sizeof(double) * 5);
                iir_lib.rfb = hdl->adaptive_iir.rfb;
            }
#if ANC_EAR_ADAPTIVE_CMP_EN
            if (result & ANC_ADAPTIVE_RESULT_LCMP) {
                hdl->param->adaptive->rcmp_coeff = anc_malloc("ICSD_ANC_APP", ANC_ADAPTIVE_CMP_ORDER * sizeof(double) * 5);
                iir_lib.rcmp = hdl->adaptive_iir.rcmp;
            }
#endif/*ANC_EAR_ADAPTIVE_CMP_EN*/
        }
#endif/*ANC_CONFIG_RFB_EN*/

        audio_anc_adaptive_data_analsis(&iir_lib);
    }
}

/* 保存VM参数 */
static int audio_anc_adaptive_data_save(anc_adaptive_iir_t *iir)
{
    printf("%s\n", __func__);
    int ret = syscfg_write(CFG_ANC_ADAPTIVE_DATA_ID, (u8 *)iir, sizeof(anc_adaptive_iir_t));
    /* printf("vmdata_num %d\n", *vmdata_num); */
    /* put_buf((u8*)vmdata_FL, EAR_RECORD_MAX*25*4); */
    /* put_buf((u8*)vmdata_FR, EAR_RECORD_MAX*25*4); */
    g_printf("ret %d, %d\n", ret, sizeof(anc_adaptive_iir_t));
    return ret;
}

/* 读取VM参数 */
int audio_anc_adaptive_data_read(void)
{
    if (hdl == NULL) {
        return -1;
    }
    anc_adaptive_iir_t *iir = &hdl->adaptive_iir;
    int ret = syscfg_read(CFG_ANC_ADAPTIVE_DATA_ID, (u8 *)iir, sizeof(anc_adaptive_iir_t));
    if (ret == sizeof(anc_adaptive_iir_t)) {	//能读取到参数, 则默认进入自适应
        if (!CRC16((u8 *)iir, sizeof(anc_adaptive_iir_t))) {	//读到的数全为0
            r_printf("adaptive data is emtpy!!!\n");
            return -1;
        }
        g_printf("do adaptive, ret %d\n", ret);

    } else {
        return -1;
        //给予自适应默认参数
        /* audio_anc_fr_format((u8 *)&iir->lff_gain, anc_local_ff_fr, ANC_ADAPTIVE_FF_ORDER); */
        /* audio_anc_fr_format((u8 *)&iir->lfb_gain, anc_local_fb_fr, ANC_ADAPTIVE_FB_ORDER); */
    }

    audio_anc_adaptive_poweron_catch_data(iir);	//开机需确认左右耳
    audio_anc_adaptive_data_format(iir);
    audio_anc_coeff_adaptive_set(ANC_COEFF_MODE_ADAPTIVE, 0);

    return 0;
}

/* 自适应默认参数更新 */
void audio_anc_ear_adaptive_param_init(audio_anc_t *param)
{

}

/* 失败参数处理 */
static void audio_anc_adaptive_fr_fail_fill(u8 *dat, u8 order, u8 result)
{
    u8 fr_fail[13] = {0x2, 0x0,  0x0, 0xc8, 0x42, \
                      0x0,  0x0, 0x0,  0x0, \
                      0x0,  0x0, 0x80, 0x3f
                     };
    if (!result) {
        for (int i = 0; i < order; i++) {
            memcpy(dat + 4 + i * 13, fr_fail, 13);
        }
    }
}

extern void anc_packet_show(anc_packet_data_t *packet);
void audio_anc_adaptive_data_packet(struct icsd_anc_v2_tool_data *TOOL_DATA)
{
    int len = TOOL_DATA->h_len;
    int fb_yorder = TOOL_DATA->yorderb;
    int ff_yorder = TOOL_DATA->yorderf;

    int ff_dat_len =  sizeof(anc_fr_t) * ff_yorder + 4;
    int fb_dat_len =  sizeof(anc_fr_t) * fb_yorder + 4;

#if ANC_EAR_ADAPTIVE_CMP_EN
    int cmp_yorder = ANC_ADAPTIVE_CMP_ORDER;
    int cmp_dat_len =  sizeof(anc_fr_t) * cmp_yorder + 4;
#endif

    anc_adaptive_iir_t *iir = &hdl->adaptive_iir;
    u8 *ff_dat, *fb_dat, *cmp_dat, *rff_dat, *rfb_dat, *rcmp_dat;
    u8 result = icsd_anc_v2_train_result_get(TOOL_DATA);

#if ANC_EAR_ADAPTIVE_CMP_EN
    result |= audio_anc_ear_adaptive_cmp_result_get();

    float *lcmp_output = audio_anc_ear_adaptive_cmp_output_get(ANC_EAR_ADAPTIVE_CMP_CH_L);
    float *rcmp_output = audio_anc_ear_adaptive_cmp_output_get(ANC_EAR_ADAPTIVE_CMP_CH_R);
    u8 *lcmp_type = audio_anc_ear_adaptive_cmp_type_get(ANC_EAR_ADAPTIVE_CMP_CH_L);
    u8 *rcmp_type = audio_anc_ear_adaptive_cmp_type_get(ANC_EAR_ADAPTIVE_CMP_CH_R);
#endif

#if 0

    printf("========ANC_L_ADAP_TARGET========\n");
    //put_buf((u8 *)TOOL_DATA->data_out3, len * 8);
    for (int i = 0; i < 120; i++) {
        printf("target %d\n", (int)(TOOL_DATA->data_out3[i] * 1e6));
    }
    printf("========ANC_L_ADAP_SZPZ========\n");
    for (int i = 0; i < len * 2; i++) {
        printf("sz %d\n", (int)(TOOL_DATA->data_out1[i] * 1e6));
    }
    printf("========ANC_L_ADAP_PZ ========\n");
    for (int i = 0; i < len * 2; i++) {
        printf("pz %d\n", (int)(TOOL_DATA->data_out2[i] * 1e6));
    }
    if (TOOL_DATA->data_out5) {
        printf("========ANC_L_ADAP_FF FGQ========\n");
        for (int i = 0; i < 31; i++) {
            printf("fgq %d\n", (int)(TOOL_DATA->data_out5[i] * 1e4));
        }
    }
    if (TOOL_DATA->data_out4) {
        printf("========ANC_L_ADAP_FB FGQ========\n");
        for (int i = 0; i < 31; i++) {
            printf("fgq %d\n", (int)(TOOL_DATA->data_out4[i] * 1e4));
        }
    }
    if (lcmp_output) {
        printf("========ANC_L_ADAP_CMP FGQ========\n");
        for (int i = 0; i < 31; i++) {
            printf("fgq %d\n", (int)(lcmp_output[i] * 1e4));
        }
    }
#endif

    if (anc_ext_ear_adaptive_train_mode_get()) {
        //测试模式不使用自适应的训练结果参数
        result = 0;
        if (anc_ext_ear_adaptive_train_mode_get() == EAR_ADAPTIVE_MODE_SIGN_TRIM) {
            anc_ext_tool_ear_adaptive_sign_trim_end_cb((u8 *)TOOL_DATA->calr_sign, 4);
        }
    }

    hdl->adaptive_iir.result = result;	//记录当前参数的保存结果;

#if ANC_CONFIG_LFF_EN
    ff_dat = (u8 *)&iir->lff_gain;
    audio_anc_fr_format(ff_dat, TOOL_DATA->data_out5, ff_yorder, TOOL_DATA->lff_iir_type);
    audio_anc_adaptive_fr_fail_fill(ff_dat, ff_yorder, (result & ANC_ADAPTIVE_RESULT_LFF));
#endif/*ANC_CONFIG_LFF_EN*/
#if ANC_CONFIG_LFB_EN
    fb_dat = (u8 *)&iir->lfb_gain;
    audio_anc_fr_format(fb_dat, TOOL_DATA->data_out4, fb_yorder, TOOL_DATA->lfb_iir_type);
    audio_anc_adaptive_fr_fail_fill(fb_dat, fb_yorder, (result & ANC_ADAPTIVE_RESULT_LFB));
#if ANC_EAR_ADAPTIVE_CMP_EN
    cmp_dat = (u8 *)&iir->lcmp_gain;
    audio_anc_fr_format(cmp_dat, lcmp_output, cmp_yorder, lcmp_type);
    audio_anc_adaptive_fr_fail_fill(cmp_dat, cmp_yorder, (result & ANC_ADAPTIVE_RESULT_LCMP));
#endif/*ANC_EAR_ADAPTIVE_CMP_EN*/
#endif/*ANC_CONFIG_LFB_EN*/
#if ANC_CONFIG_RFF_EN && AUDIO_ANC_STEREO_ENABLE
    rff_dat = (u8 *)&iir->rff_gain;
    if (!ANC_CONFIG_LFF_EN) { 	//TWS 单R声道使用
        ff_dat = rff_dat;
        audio_anc_fr_format(ff_dat, TOOL_DATA->data_out5, ff_yorder, TOOL_DATA->lff_iir_type);
        audio_anc_adaptive_fr_fail_fill(ff_dat, ff_yorder, (result & ANC_ADAPTIVE_RESULT_LFF));
    } else {
        audio_anc_fr_format(rff_dat, TOOL_DATA->data_out10, ff_yorder, TOOL_DATA->rff_iir_type);
        audio_anc_adaptive_fr_fail_fill(rff_dat, ff_yorder, (result & ANC_ADAPTIVE_RESULT_RFF));
    }
#endif/*ANC_CONFIG_RFF_EN*/
#if ANC_CONFIG_RFB_EN && AUDIO_ANC_STEREO_ENABLE
    rfb_dat = (u8 *)&iir->rfb_gain;
    if (!ANC_CONFIG_LFB_EN) {	//TWS 单R声道使用
        fb_dat = rfb_dat;
        audio_anc_fr_format(fb_dat, TOOL_DATA->data_out4, fb_yorder, TOOL_DATA->lfb_iir_type);
        audio_anc_adaptive_fr_fail_fill(fb_dat, fb_yorder, (result & ANC_ADAPTIVE_RESULT_LFB));
    } else {
        audio_anc_fr_format(rfb_dat, TOOL_DATA->data_out9, fb_yorder, TOOL_DATA->rfb_iir_type);
        audio_anc_adaptive_fr_fail_fill(rfb_dat, fb_yorder, (result & ANC_ADAPTIVE_RESULT_RFB));
    }
#if ANC_EAR_ADAPTIVE_CMP_EN
    rcmp_dat = (u8 *)&iir->rcmp_gain;
    if (!ANC_CONFIG_LFB_EN) {	//TWS 单R声道使用
        cmp_dat = rcmp_dat;
        audio_anc_fr_format(cmp_dat, lcmp_output, cmp_yorder, lcmp_type);
        audio_anc_adaptive_fr_fail_fill(cmp_dat, cmp_yorder, (result & ANC_ADAPTIVE_RESULT_LCMP));
    } else {
        audio_anc_fr_format(rcmp_dat, rcmp_output, cmp_yorder, rcmp_type);
        audio_anc_adaptive_fr_fail_fill(rcmp_dat, cmp_yorder, (result & ANC_ADAPTIVE_RESULT_RCMP));
    }
#endif/*ANC_EAR_ADAPTIVE_CMP_EN*/
#endif/*ANC_CONFIG_RFB_EN*/

#if (TCFG_AUDIO_ANC_CH & ANC_L_CH)
    memcpy(iir->l_target, TOOL_DATA->data_out3, len * 8);
#endif
#if (TCFG_AUDIO_ANC_CH & ANC_R_CH) && AUDIO_ANC_STEREO_ENABLE
    memcpy(iir->r_target, TOOL_DATA->data_out8, len * 8);
#endif

    if (hdl->param->developer_mode || anc_ext_tool_online_get()) {
        printf("-- len = %d\n", len);
        printf("-- ff_yorder = %d\n", ff_yorder);
        printf("-- fb_yorder = %d\n", fb_yorder);
#if ANC_EAR_ADAPTIVE_CMP_EN
        printf("-- cmp_yorder = %d\n", cmp_yorder);
#endif
        /* 先统一申请空间，因为下面不知道什么情况下调用函数 anc_data_catch 时令参数 init_flag 为1 */
        anc_adaptive_data = anc_data_catch(anc_adaptive_data, NULL, 0, 0, 1);

#if AUDIO_ANC_STEREO_ENABLE	//头戴式
        r_printf("ANC ear-adaptive send data\n");
        anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)TOOL_DATA->h_freq, len * 4, ANC_R_ADAP_FRE, 0);
        anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)TOOL_DATA->data_out6, len * 8, ANC_R_ADAP_SZPZ, 0);
        anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)TOOL_DATA->data_out7, len * 8, ANC_R_ADAP_PZ, 0);
        anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)TOOL_DATA->data_out8, len * 8, ANC_R_ADAP_TARGET, 0);
        anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)TOOL_DATA->data_out14, len * 8, ANC_R_ADAP_TARGET_CMP, 0);
        anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)TOOL_DATA->target_before_cmp_r, len * 8, ANC_R_ADAP_TARGET_BEFORE_CMP, 0);
        anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)TOOL_DATA->cmp_form_anc_train_r, len * 8, ANC_R_ADAP_CMP_FORM_TRAIN, 0);
        anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)rff_dat, ff_dat_len, ANC_R_FF_IIR, 0);  //R_ff

        anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)TOOL_DATA->h_freq, len * 4, ANC_L_ADAP_FRE, 0);
        anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)TOOL_DATA->data_out1, len * 8, ANC_L_ADAP_SZPZ, 0);
        anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)TOOL_DATA->data_out2, len * 8, ANC_L_ADAP_PZ, 0);
        anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)TOOL_DATA->data_out3, len * 8, ANC_L_ADAP_TARGET, 0);
        anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)TOOL_DATA->data_out13, len * 8, ANC_L_ADAP_TARGET_CMP, 0);
        anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)TOOL_DATA->target_before_cmp_l, len * 8, ANC_L_ADAP_TARGET_BEFORE_CMP, 0);
        anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)TOOL_DATA->cmp_form_anc_train_l, len * 8, ANC_L_ADAP_CMP_FORM_TRAIN, 0);
        anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)ff_dat, ff_dat_len, ANC_L_FF_IIR, 0);  //L_ff
#if ANC_ADAPTIVE_FB_EN
        anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)rfb_dat, fb_dat_len, ANC_R_FB_IIR, 0);  //R_fb
        anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)fb_dat, fb_dat_len, ANC_L_FB_IIR, 0);  //L_fb
#endif
#if ANC_EAR_ADAPTIVE_CMP_EN
        anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)cmp_dat, cmp_dat_len, ANC_L_CMP_IIR, 0);  //L_cmp
        anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)rcmp_dat, cmp_dat_len, ANC_R_CMP_IIR, 0);  //R_cmp
#endif/*ANC_EAR_ADAPTIVE_CMP_EN*/
#else  //入耳式
#if TCFG_USER_TWS_ENABLE
        if (bt_tws_get_local_channel() == 'R') {
            r_printf("ANC ear-adaptive send data, ch:R\n");
            anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)TOOL_DATA->h_freq, len * 4, ANC_R_ADAP_FRE, 0);
            anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)TOOL_DATA->data_out1, len * 8, ANC_R_ADAP_SZPZ, 0);
            anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)TOOL_DATA->data_out2, len * 8, ANC_R_ADAP_PZ, 0);
            anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)TOOL_DATA->data_out3, len * 8, ANC_R_ADAP_TARGET, 0);
            anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)TOOL_DATA->data_out13, len * 8, ANC_R_ADAP_TARGET_CMP, 0);
            anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)TOOL_DATA->target_before_cmp_l, len * 8, ANC_R_ADAP_TARGET_BEFORE_CMP, 0);
            anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)TOOL_DATA->cmp_form_anc_train_l, len * 8, ANC_R_ADAP_CMP_FORM_TRAIN, 0);
            anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)ff_dat, ff_dat_len, ANC_R_FF_IIR, 0);  //R_ff
#if ANC_ADAPTIVE_FB_EN
            anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)fb_dat, fb_dat_len, ANC_R_FB_IIR, 0);  //R_fb
#endif
#if ANC_EAR_ADAPTIVE_CMP_EN
            anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)cmp_dat, cmp_dat_len, ANC_R_CMP_IIR, 0);  //R_cmp
#endif/*ANC_EAR_ADAPTIVE_CMP_EN*/
#if AUDIO_ANC_ADAPTIVE_CMP_SZ_FACTOR
            float *sz_cmp_r = audio_anc_ear_adaptive_cmp_sz_cmp_get(ANC_EAR_ADAPTIVE_CMP_CH_L);
            if (sz_cmp_r) {
                anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)sz_cmp_r, len * 8, ANC_R_ADAP_SZ_CMP, 0);
            }
#endif
        } else
#endif/*TCFG_USER_TWS_ENABLE*/
        {
            r_printf("ANC ear-adaptive send data, ch:L\n");
            anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)TOOL_DATA->h_freq, len * 4, ANC_L_ADAP_FRE, 0);
            anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)TOOL_DATA->data_out1, len * 8, ANC_L_ADAP_SZPZ, 0);
            anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)TOOL_DATA->data_out2, len * 8, ANC_L_ADAP_PZ, 0);
            anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)TOOL_DATA->data_out3, len * 8, ANC_L_ADAP_TARGET, 0);
            anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)TOOL_DATA->data_out13, len * 8, ANC_L_ADAP_TARGET_CMP, 0);
            anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)TOOL_DATA->target_before_cmp_l, len * 8, ANC_L_ADAP_TARGET_BEFORE_CMP, 0);
            anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)TOOL_DATA->cmp_form_anc_train_l, len * 8, ANC_L_ADAP_CMP_FORM_TRAIN, 0);
            anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)ff_dat, ff_dat_len, ANC_L_FF_IIR, 0);  //L_ff
#if ANC_ADAPTIVE_FB_EN
            anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)fb_dat, fb_dat_len, ANC_L_FB_IIR, 0);  //L_fb
#endif
#if ANC_EAR_ADAPTIVE_CMP_EN
            anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)cmp_dat, cmp_dat_len, ANC_L_CMP_IIR, 0);  //L_cmp
#endif/*ANC_EAR_ADAPTIVE_CMP_EN*/
#if AUDIO_ANC_ADAPTIVE_CMP_SZ_FACTOR
            //双FB 方案复用耳道target线作为SZ补偿曲线显示
            float *sz_cmp_l = audio_anc_ear_adaptive_cmp_sz_cmp_get(ANC_EAR_ADAPTIVE_CMP_CH_L);
            if (sz_cmp_l) {
                anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)sz_cmp_l, len * 8, ANC_L_ADAP_SZ_CMP, 0);
            }
#endif
        }
#endif  /* TCFG_AUDIO_ANC_CH == (ANC_L_CH | ANC_R_CH) */

    }

}

int audio_anc_ear_adaptive_tool_data_get(u8 **buf, u32 *len)
{
    if (anc_adaptive_data == NULL) {
        printf("EAR ANC packet is NULL, return!\n");
        return -1;
    }
    if (anc_adaptive_data->dat_len == 0) {
        printf("EAR ANC error: dat_len == 0\n");
        return -1;
    }
    *buf = anc_adaptive_data->dat;
    *len = anc_adaptive_data->dat_len;
    return 0;
}


/* 开机自适应数据拼包，预备工具读取 */
static void audio_anc_adaptive_poweron_catch_data(anc_adaptive_iir_t *iir)
{
    if (hdl->param->developer_mode || anc_ext_tool_online_get()) {
        int i;
        int ff_dat_len =  sizeof(anc_fr_t) * ANC_ADAPTIVE_FF_ORDER + 4;
        int fb_dat_len =  sizeof(anc_fr_t) * ANC_ADAPTIVE_FB_ORDER + 4;
#if ANC_EAR_ADAPTIVE_CMP_EN
        int cmp_dat_len =  sizeof(anc_fr_t) * ANC_ADAPTIVE_CMP_ORDER + 4;
#endif
        u8 *ff_dat, *fb_dat, *cmp_dat, *rff_dat, *rfb_dat, *rcmp_dat;
        audio_anc_t *param = hdl->param;
#if ANC_CONFIG_LFF_EN
        ff_dat = (u8 *)&iir->lff_gain;
#endif/*ANC_CONFIG_LFF_EN*/
#if ANC_CONFIG_LFB_EN
        fb_dat = (u8 *)&iir->lfb_gain;
#if ANC_EAR_ADAPTIVE_CMP_EN
        cmp_dat = (u8 *)&iir->lcmp_gain;
#endif/*ANC_EAR_ADAPTIVE_CMP_EN*/
#endif/*ANC_CONFIG_LFB_EN*/
#if ANC_CONFIG_RFF_EN && AUDIO_ANC_STEREO_ENABLE
        rff_dat = (u8 *)&iir->rff_gain;
        if (ff_dat == NULL) { //TWS ANC_R_CH使用
            ff_dat = rff_dat;
        }
#endif/*ANC_CONFIG_RFF_EN*/
#if ANC_CONFIG_RFB_EN && AUDIO_ANC_STEREO_ENABLE
        rfb_dat = (u8 *)&iir->rfb_gain;
        if (fb_dat == NULL) {	//TWS ANC_R_CH使用
            fb_dat = rfb_dat;
        }
#if ANC_EAR_ADAPTIVE_CMP_EN
        rcmp_dat = (u8 *)&iir->rcmp_gain;
        if (cmp_dat == NULL) {	//TWS ANC_R_CH使用
            cmp_dat = rcmp_dat;
        }
#endif/*ANC_EAR_ADAPTIVE_CMP_EN*/
#endif/*ANC_CONFIG_RFB_EN*/

        /* 先统一申请空间，因为下面不知道什么情况下调用函数 anc_data_catch 时令参数 init_flag 为1 */
        anc_adaptive_data = anc_data_catch(anc_adaptive_data, NULL, 0, 0, 1);
#if AUDIO_ANC_STEREO_ENABLE	//头戴式的耳机
        r_printf("ANC ear-adaptive send data\n");
        if (param->lff_en && param->rff_en) {
            anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)ff_dat, ff_dat_len, ANC_L_FF_IIR, 0);  //L_ff
            anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)rff_dat, ff_dat_len, ANC_R_FF_IIR, 0);  //R_ff
        }
#if ANC_ADAPTIVE_FB_EN
        anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)fb_dat, fb_dat_len, ANC_L_FB_IIR, 0);  //L_fb
        anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)rfb_dat, fb_dat_len, ANC_R_FB_IIR, 0);  //R_fb
#endif
#if ANC_EAR_ADAPTIVE_CMP_EN
        anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)cmp_dat, cmp_dat_len, ANC_L_CMP_IIR, 0);  //L_cmp
        anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)rcmp_dat, cmp_dat_len, ANC_R_CMP_IIR, 0);  //R_cmp
#endif/*ANC_EAR_ADAPTIVE_CMP_EN*/
#else	//入耳式的
#if TCFG_USER_TWS_ENABLE
        if (bt_tws_get_local_channel() == 'R') {
            r_printf("ANC ear-adaptive send data, ch:R\n");
            if (param->lff_en || param->rff_en) {
                anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)ff_dat, ff_dat_len, ANC_R_FF_IIR, 0);  //R_ff
            }
#if ANC_ADAPTIVE_FB_EN
            anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)fb_dat, fb_dat_len, ANC_R_FB_IIR, 0);  //R_fb
#endif
#if ANC_EAR_ADAPTIVE_CMP_EN
            anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)cmp_dat, cmp_dat_len, ANC_R_CMP_IIR, 0);  //R_cmp
#endif/*ANC_EAR_ADAPTIVE_CMP_EN*/
        } else
#endif/*TCFG_USER_TWS_ENABLE*/
        {
            r_printf("ANC ear-adaptive send data, ch:L\n");
            if (param->lff_en || param->rff_en) {
                anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)ff_dat, ff_dat_len, ANC_L_FF_IIR, 0);  //L_ff
            }
#if ANC_ADAPTIVE_FB_EN
            anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)fb_dat, fb_dat_len, ANC_L_FB_IIR, 0);  //L_fb
#endif
#if ANC_EAR_ADAPTIVE_CMP_EN
            anc_adaptive_data = anc_data_catch(anc_adaptive_data, (u8 *)cmp_dat, cmp_dat_len, ANC_L_CMP_IIR, 0);  //L_cmp
#endif/*ANC_EAR_ADAPTIVE_CMP_EN*/
        }
#endif	/* TCFG_AUDIO_ANC_CH == (ANC_L_CH | ANC_R_CH) */
    }
}

/* TWS 左右参数初始化 - 耳道自适应工具打开时调用 */
void audio_anc_param_map_init(void)
{
#if TCFG_USER_TWS_ENABLE && (!AUDIO_ANC_MIC_ARRAY_ENABLE) && (!AUDIO_ANC_STEREO_ENABLE)
    if (hdl) {
        if (hdl->param->developer_mode || anc_ext_tool_online_get()) {
            anc_gain_t *cfg = anc_malloc("ICSD_ANC_APP", sizeof(anc_gain_t));
            anc_cfg_online_deal(ANC_CFG_READ, cfg);
            u8 update = 0;
            //将左边的MIC增益拷贝至右边，避免增益不同导致效果异常；
            if (cfg->gains.r_ffmic_gain != cfg->gains.l_ffmic_gain) {
                cfg->gains.r_ffmic_gain = cfg->gains.l_ffmic_gain;
                update = 1;
            }
            if (cfg->gains.r_fbmic_gain != cfg->gains.l_fbmic_gain) {
                cfg->gains.r_fbmic_gain = cfg->gains.l_fbmic_gain;
                update = 1;
            }
            if (update) {
                /* anc_cfg_online_deal(ANC_CFG_WRITE, cfg); */
                int ret = anc_db_put(hdl->param, cfg, NULL);
                if (ret) {
                    anc_log("Err: %s, ret %d\n", __func__, ret);
                }
            }
            anc_free(cfg);
        }
    }
#endif
}

/* TWS 左右参数映射 */
void audio_anc_param_map(u8 coeff_en, u8 gain_en)
{
#if TCFG_USER_TWS_ENABLE && (!AUDIO_ANC_MIC_ARRAY_ENABLE) && (!AUDIO_ANC_STEREO_ENABLE)
    //开发者模式 or ANC_EXT调试模式，左右耳均区分使用参数
    if (hdl->param->developer_mode || anc_ext_tool_online_get()) {
        if (coeff_en && (!hdl->param->lff_coeff || !hdl->param->rff_coeff || \
                         !hdl->param->lfb_coeff || !hdl->param->rfb_coeff)) {
#if ANC_DEVELOPER_MODE_EN
            ASSERT(0, "(ERROR)ANC adaptive coeff map %x, %x, %x, %x, Maybe anc_coeff.bin is ERR!", \
                   (int)hdl->param->lff_coeff, (int)hdl->param->rff_coeff, \
                   (int)hdl->param->lfb_coeff, (int)hdl->param->rfb_coeff);
#else
            anc_log("(ERROR)ANC adaptive coeff map %x, %x, %x, %x, Maybe anc_coeff.bin is ERR!", \
                    (int)hdl->param->lff_coeff, (int)hdl->param->rff_coeff, \
                    (int)hdl->param->lfb_coeff, (int)hdl->param->rfb_coeff);
#endif/*ANC_DEVELOPER_MODE_EN*/
        }
        if ((TCFG_AUDIO_ANC_CH == ANC_L_CH) && (bt_tws_get_local_channel() == 'R')) {
            anc_log("ANC MAP TWS_RIGHT_CHANNEL\n");
            if (gain_en) {
                hdl->param->gains.l_ffgain = hdl->param->gains.r_ffgain;
                hdl->param->gains.l_fbgain = hdl->param->gains.r_fbgain;
                hdl->param->gains.l_transgain = hdl->param->gains.r_transgain;
                hdl->param->gains.l_cmpgain = hdl->param->gains.r_cmpgain;
                hdl->param->gains.l_ffmic_gain = hdl->param->gains.r_ffmic_gain;
                hdl->param->gains.l_fbmic_gain = hdl->param->gains.r_fbmic_gain;
                hdl->param->gains.gain_sign = (hdl->param->gains.gain_sign >> 4);
            }
            if (coeff_en) {
                hdl->param->lff_coeff = hdl->param->rff_coeff;
                hdl->param->lfb_coeff = hdl->param->rfb_coeff;
                hdl->param->ltrans_coeff = hdl->param->rtrans_coeff;
                hdl->param->lcmp_coeff = hdl->param->rcmp_coeff;

                hdl->param->lff_yorder = hdl->param->rff_yorder;
                hdl->param->lfb_yorder = hdl->param->rfb_yorder;
                hdl->param->ltrans_yorder = hdl->param->rtrans_yorder;
                hdl->param->lcmp_yorder = hdl->param->rcmp_yorder;
            }
        } else if ((TCFG_AUDIO_ANC_CH == ANC_R_CH) && (bt_tws_get_local_channel() == 'L')) {
            anc_log("ANC MAP TWS_LEFT_CHANNEL\n");
            if (gain_en) {
                hdl->param->gains.r_ffgain = hdl->param->gains.l_ffgain;
                hdl->param->gains.r_fbgain = hdl->param->gains.l_fbgain;
                hdl->param->gains.r_transgain = hdl->param->gains.l_transgain;
                hdl->param->gains.r_cmpgain = hdl->param->gains.l_cmpgain;
                hdl->param->gains.r_ffmic_gain = hdl->param->gains.l_ffmic_gain;
                hdl->param->gains.r_fbmic_gain = hdl->param->gains.l_fbmic_gain;
                hdl->param->gains.gain_sign = (hdl->param->gains.gain_sign << 4);
            }
            if (coeff_en) {
                hdl->param->rff_coeff = hdl->param->lff_coeff;
                hdl->param->rfb_coeff = hdl->param->lfb_coeff;
                hdl->param->rtrans_coeff = hdl->param->ltrans_coeff;
                hdl->param->rcmp_coeff = hdl->param->lcmp_coeff;

                hdl->param->rff_yorder = hdl->param->lff_yorder;
                hdl->param->rfb_yorder = hdl->param->lfb_yorder;
                hdl->param->rtrans_yorder = hdl->param->ltrans_yorder;
                hdl->param->rcmp_yorder = hdl->param->lcmp_yorder;
            }
        }
    }
#endif/*TCFG_USER_TWS_ENABLE*/

}

/*
   强制中断自适应
   param: default_flag		1 退出后恢复默认ANC效果； 0 退出后保持ANC_OFF(避免与下一个切模式流程冲突)
   		  wait_pend			1 阻塞等待自适应退出完毕；0 不等待直接退出
 */
void anc_ear_adaptive_forced_exit(u8 default_flag, u8 wait_pend)
{
    if (hdl) {
        //当自适应启动且算法未停止时，才允许强制停止算法
        if (hdl->adaptive_state && hdl->adaptive_state < EAR_ADAPTIVE_STATE_ALOGM_STOP) {
            hdl->forced_exit_default_flag = default_flag;
            icsd_anc_v2_forced_exit();
            tone_player_stop();
#if TCFG_AUDIO_ADAPTIVE_EQ_ENABLE
            //开启自适应EQ时，自适应EQ也强制退出
            audio_adaptive_eq_force_exit();
#endif
            if (wait_pend) {
                os_sem_pend(&hdl->exit_sem, 300);
            }
        }
    }
}

/* ANC自适应训练处理回调函数 */
void audio_anc_adaptive_result_callback(u8 result_l, u8 result_r)
{
#if (RCSP_ADV_EN && RCSP_ADV_ANC_VOICE && RCSP_ADV_ADAPTIVE_NOISE_REDUCTION)
    extern void set_adaptive_noise_reduction_reset_callback(u8 result);
    //V2 版本自适应一定成功
    set_adaptive_noise_reduction_reset_callback(1);
#endif

#if ANC_EAR_ADAPTIVE_RESULT_TONE_EN

    char *file_name = NULL;

#if TCFG_USER_TWS_ENABLE
    file_name = (char *)get_tone_files()->num[result_l];
    if (get_tws_sibling_connect_state()) {
        if (tws_api_get_role() == TWS_ROLE_MASTER) {
            tws_play_tone_file(file_name, 400);
        }
    } else {
        tws_play_tone_file(file_name, 400);
    }
#else	//Headset
    if (result_l == FF_ANC_SUCCESS && result_r == FF_ANC_SUCCESS) {
        file_name = (char *)get_tone_files()->num[0];
        play_tone_file(file_name);
    } else {
        file_name = (char *)get_tone_files()->num[result_l];
        play_tone_file(file_name);
        file_name = (char *)get_tone_files()->num[result_r];
        play_tone_file(file_name);
    }
#endif

#endif
}

/* 关机数据保存 */
void anc_ear_adaptive_power_off_save_data(void)
{
#if ANC_POWEOFF_SAVE_ADAPTIVE_DATA
    audio_anc_adaptive_data_save(&hdl->adaptive_iir);
#endif/*ANC_POWEOFF_SAVE_ADAPTIVE_DATA*/
}

/* 训练参数加载至ANC param句柄 */
static void anc_ear_adaptive_train_cfg_update(struct anc_ear_adaptive_train_cfg *cfg)
{
    hdl->param->gains.l_ffgain = cfg->ff_gain;
    hdl->param->lff_coeff = cfg->ff_coeff;
    hdl->param->lff_yorder = cfg->ff_yorder;
    hdl->param->gains.r_ffgain = cfg->ff_gain;
    hdl->param->rff_coeff = cfg->ff_coeff;
    hdl->param->rff_yorder = cfg->ff_yorder;
    hdl->param->gains.gain_sign &= ~(ANCL_FF_SIGN | ANCR_FF_SIGN);	//L/R FF符号位固定为0, 由自适应控制

    /* hdl->param->gains.l_fbgain = cfg->fb_gain;  */
    /* hdl->param->lfb_coeff = cfg->fb_coeff; */
    /* hdl->param->lfb_yorder = cfg->fb_yorder; */
    /* hdl->param->gains.r_fbgain = cfg->fb_gain;  */
    /* hdl->param->rfb_coeff = cfg->fb_coeff; */
    /* hdl->param->rfb_yorder = cfg->fb_yorder; */

    /* hdl->param->gains.l_cmpgain = cfg->cmp_gain;  */
    /* hdl->param->lcmp_coeff = cfg->cmp_coeff;  */
    /* hdl->param->lcmp_yorder = cfg->cmp_yorder;  */

    /* hdl->param->gains.r_cmpgain = cfg->cmp_gain;  */
    /* hdl->param->rcmp_coeff = cfg->cmp_coeff;  */
    /* hdl->param->rcmp_yorder = cfg->cmp_yorder;  */
}

u8 anc_ear_adaptive_train_set_pnc(u8 ff_yorder, float bypass_vol, s8 bypass_sign)
{
    int dly = audio_anc_fade_dly_get(16384, audio_anc_gains_alogm_get(ANC_FF_TYPE));

    anc_v2_use_train_open_ancout(ff_yorder, bypass_vol, bypass_sign);		//test 可以更早触发的
    audio_anc_fade(AUDIO_ANC_FDAE_CH_ALL, 0);			//增益淡出
    os_time_dly(dly / 10 + 1);                          //等待淡出完毕
    audio_ear_adaptive_train_app_suspend();				//挂起互斥功能
    anc_ear_adaptive_train_cfg_update(&hdl->train_cfg); //加载自适应训练参数
    anc_user_train_process(hdl->param);					//初始化ANC
    os_time_dly(40);                     //test 确认具体稳定时间
    return 0;
}

void audio_anc_reset(audio_anc_t *param, u8 fade_en);
u8 anc_ear_adaptive_train_set_bypass(u8 ff_yorder, float bypass_vol, s8 bypass_sign)
{
    int dly = audio_anc_fade_dly_get(16384, audio_anc_gains_alogm_get(ANC_FF_TYPE));

    anc_v2_use_train_open_ancout(ff_yorder, bypass_vol, bypass_sign);		//test 可以更早触发的
    audio_anc_fade(AUDIO_ANC_FDAE_CH_ALL, 0);			//增益淡出
    os_time_dly(dly / 10 + 1);							//等待淡出完毕
    audio_ear_adaptive_train_app_suspend();				//挂起互斥功能
    anc_ear_adaptive_train_cfg_update(&hdl->train_cfg);	//加载自适应训练参数
    anc_user_train_process(hdl->param);					//初始化ANC
    audio_anc_fade(AUDIO_ANC_FDAE_CH_ALL, 16384);		//增益淡入
    os_time_dly(dly / 10 + 1);							//等待淡入完毕
    os_time_dly(40);                    //test 确认具体稳定时间
    return 0;
}

u8 anc_ear_adaptive_train_set_bypass_off(void)
{
    int dly = audio_anc_fade_dly_get(16384, audio_anc_gains_alogm_get(ANC_FF_TYPE));

    audio_anc_fade(AUDIO_ANC_FDAE_CH_ALL, 0);			//增益淡出
    audio_ear_adaptive_train_app_resume();	//恢复APP互斥功能
    return 0;
}

static void anc_ear_adaptive_icsd_anc_init(void)
{
    if (hdl) {
        clock_alloc("ANC_ADAP", 160 * 1000000L);
        if (++hdl->seq == 0xff) {
            hdl->seq = 0;
        }
        struct icsd_anc_init_cfg init_cfg  = {
            .seq = hdl->seq,
            .tws_sync = hdl->tws_sync,
            .param = hdl->param,
        };
        struct anc_ext_ear_adaptive_param *ext_cfg = anc_ext_ear_adaptive_cfg_get();
        icsd_anc_v2_init(&init_cfg, ext_cfg);
    }
}

//自适应SZ数据回调处理接口
void anc_ear_adaptive_sz_output(__afq_output *output)
{
    if (audio_afq_common_app_is_active()) {
        audio_afq_common_output_post_msg(output, AUDIO_AFQ_PRIORITY_NORMAL);
    }
#if ANC_EAR_ADAPTIVE_CMP_EN
    audio_anc_ear_adaptive_cmp_run(output);
#endif
}

//测试demo: 模拟出耳打断，入耳自适应流程
void anc_ear_adaptive_forced_demo(void)
{
#if 0
    static u8 flag = 0;
    flag ^= 1;
    if (flag) {
        g_printf("adaptive in ear\n");
        audio_anc_mode_ear_adaptive(1, 0);
    } else {
        g_printf("adaptive out ear\n");
        anc_ear_adaptive_forced_exit(0, 0);
    }
#endif
}

//------------------------------- 外部参数读写接口 ------------------------------
u8 anc_ear_adaptive_tws_sync_en_get(void)
{
    if (hdl) {
        return hdl->tws_sync;
    }
    return 0;
}

void anc_ear_adaptive_seq_set(u8 seq)
{
    if (hdl) {
        hdl->seq = seq;
    }
}

u8 anc_ear_adaptive_seq_get(void)
{
    if (hdl) {
        return hdl->seq;
    }
    return 0;
}

/* 判断当前是否处于训练中 */
u8 anc_ear_adaptive_busy_get(void)
{
    if (hdl) {
        return (hdl->adaptive_state) ? 1 : 0;
    }
    return 0;
}

/* 耳道自适应训练结束 */
void anc_ear_adaptive_mode_end(void)
{
    if (hdl) {
        anc_log("EAR_ADAPTIVE_STATE:END\n");
        hdl->forced_exit = 0;
        hdl->adaptive_state = EAR_ADAPTIVE_STATE_END;
    }
}

struct anc_ear_adaptive_train_cfg *anc_ear_adaptive_train_cfg_get(void)
{
    if (hdl) {
        return &hdl->train_cfg;
    }
    return NULL;
}

void *anc_ear_adaptive_iir_get(void)
{
    if (hdl) {
        return &hdl->adaptive_iir;
    }
    return NULL;
}

#endif/*ANC_EAR_ADAPTIVE_EN*/
