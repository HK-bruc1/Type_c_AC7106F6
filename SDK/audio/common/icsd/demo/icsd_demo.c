#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".icsd_common.data.bss")
#pragma data_seg(".icsd_common.data")
#pragma const_seg(".icsd_common.text.const")
#pragma code_seg(".icsd_common.text")
#endif

#include "app_config.h"
#include "audio_config_def.h"

#if (TCFG_AUDIO_ANC_ENABLE && \
	 TCFG_AUDIO_ANC_EXT_VERSION == ANC_EXT_V2)

#include "audio_anc_includes.h"

/*
   自适应EQ运行结束回调
	param: msg 结束传参
 */
void audio_adaptive_eq_end_result(int msg)
{
    printf("adaptive_eq end %d\n", msg);
}

/*
   贴合度运行结束回调
	param: result 贴合度阈值，用户判断松紧度
 */
void audio_dot_end_result(int result)
{
    /*
     	AUDIO_FIT_DET_RESULT_TIGHT
    	AUDIO_FIT_DET_RESULT_NORMAL
    	AUDIO_FIT_DET_RESULT_LOOSE
    */
    printf("dot end %d\n", result);
}

/*
*********************************************************************
*                  audio_adaptive_eq_app_open
* Description: 单次（自适应EQ）
* Note(s)    : 播提示音 + 自适应EQ, 运行结束调用audio_adaptive_eq_end_result
*********************************************************************
*/
int audio_adaptive_eq_app_open(void)
{
    int ret = 0;

    //选择数据来源AFQ
    int fre_sel = AUDIO_ADAPTIVE_FRE_SEL_AFQ;

#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
    if (audio_anc_real_time_adaptive_state_get()) {
        printf("adaptive eq open fail: RTANC open now\n");
        return -1;
    }
#endif

#if TCFG_AUDIO_ADAPTIVE_EQ_ENABLE
    //1. 注册 自适应EQ 流程
    ret = audio_adaptive_eq_open(fre_sel, audio_adaptive_eq_end_result);
    if (ret) {
        printf("adaptive eq open fail\n");
        goto __exit;
    }
#endif

#if TCFG_AUDIO_FREQUENCY_GET_ENABLE
    //2.播放提示音-启动算法流程
    ret = audio_icsd_afq_open(1);
    if (ret) {
        printf("afq open fail\n");
        goto __exit;
    }
#endif

    return 0;
__exit:	//处理启动异常的问题

    //重入场景不需要关闭对应模块
    if (ret == ANC_EXT_OPEN_FAIL_REENTRY) {
        printf("func open now\n");
        return ret;
    }
    printf("fail process\n");
#if TCFG_AUDIO_ADAPTIVE_EQ_ENABLE
    audio_adaptive_eq_close();
#endif
    return ret;
}

/*
*********************************************************************
*                  audio_adaptive_eq_app_open
* Description: 单次（自适应EQ）
* Note(s)    : 播提示音 + 自适应EQ, 运行结束调用audio_adaptive_eq_end_result
*********************************************************************
*/
#if RTANC_SZ_SEL_FF_OUTPUT
int audio_rtanc_in_ear_demo(u8 tone_en, ANC_mode_t exit_mode)
{
    int ret = 0;

    //选择数据来源AFQ
    int fre_sel = AUDIO_ADAPTIVE_FRE_SEL_AFQ;

#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
    audio_icsd_adt_scene_set(ADT_SCENE_AFQ, 1);
#endif

#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
    if (audio_anc_real_time_adaptive_state_get()) {
        printf("rtanc in ear: close RTANC\n");
#if TCFG_AUDIO_ADAPTIVE_EQ_ENABLE
        audio_real_time_adaptive_eq_close();
#endif
        audio_icsd_adt_close(ADT_SCENE_AFQ, 1, get_icsd_adt_mode(), 0);
    }
#endif

#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
    //1. 注册 耳道自适应流程
    audio_rtanc_sz_select_open();
#endif

#if TCFG_AUDIO_FREQUENCY_GET_ENABLE
    //2.播放提示音-启动算法流程
    ret = audio_icsd_afq_open_fix_exit_mode(tone_en, exit_mode);
    if (ret) {
        printf("afq open fail\n");
        goto __exit;
    }
#endif
    return 0;
__exit:	//处理启动异常的问题

    //重入场景不需要关闭对应模块
    if (ret == ANC_EXT_OPEN_FAIL_REENTRY) {
        printf("func open now\n");
        return ret;
    }
    printf("fail process\n");
    return ret;
}
#endif

/*
*********************************************************************
*                  audio_dot_app_open
* Description: 单次（贴合度检测）
* Note(s)    : 播提示音 + 贴合度检测, 运行结束调用audio_dot_end_result
*********************************************************************
*/
int audio_dot_app_open(void)
{
    int ret = 0;

    //选择数据来源AFQ
    int fre_sel = AUDIO_ADAPTIVE_FRE_SEL_AFQ;

#if TCFG_AUDIO_FIT_DET_ENABLE
    //1. 注册 贴合度检测 流程
    ret = audio_icsd_dot_open(fre_sel, audio_dot_end_result);
    if (ret) {
        printf("fit_det open fail\n");
        goto __exit;
    }
#endif

#if TCFG_AUDIO_FREQUENCY_GET_ENABLE
    //2.播放提示音-启动算法流程
    ret = audio_icsd_afq_open(1);
    if (ret) {
        printf("afq open fail\n");
        goto __exit;
    }
#endif

    return 0;
__exit:	//处理启动异常的问题

    printf("fail process\n");
#if TCFG_AUDIO_FIT_DET_ENABLE
    audio_icsd_dot_close();
#endif
    return ret;
}

/*
*********************************************************************
*                  audio_afq_open_demo
* Description: 单次（自适应EQ + 贴合度检测）
* Note(s)    : 一次提示音流程输出的SZ，挂载多个算法处理
*********************************************************************
*/
int audio_afq_app_open_demo(void)
{
    int ret = 0;

    //选择数据来源AFQ
    int fre_sel = AUDIO_ADAPTIVE_FRE_SEL_AFQ;

#if TCFG_AUDIO_ADAPTIVE_EQ_ENABLE
    //1. 注册 自适应EQ 流程
    ret = audio_adaptive_eq_open(fre_sel, audio_adaptive_eq_end_result);
    if (ret) {
        printf("adaptive eq open fail\n");
        goto __exit;
    }
#endif

#if TCFG_AUDIO_FIT_DET_ENABLE
    //2. 注册 贴合度检测 流程
    ret = audio_icsd_dot_open(fre_sel, audio_dot_end_result);
    if (ret) {
        printf("fit_det open fail\n");
        goto __exit;
    }
#endif

#if TCFG_AUDIO_FREQUENCY_GET_ENABLE
    //3.播放提示音-启动算法流程
    ret = audio_icsd_afq_open(1);
    if (ret) {
        printf("afq open fail\n");
        goto __exit;
    }
#endif

    return 0;
__exit:	//处理启动异常的问题

    //重入场景不需要关闭对应模块
    if (ret == ANC_EXT_OPEN_FAIL_REENTRY) {
        printf("func open now\n");
        return ret;
    }
    printf("fail process\n");
#if TCFG_AUDIO_ADAPTIVE_EQ_ENABLE
    audio_adaptive_eq_close();
#endif

#if TCFG_AUDIO_FIT_DET_ENABLE
    audio_icsd_dot_close();
#endif
    return ret;
}

/*
*********************************************************************
*                  audio_ear_adaptive_open_demo
* Description: 单次（ANC耳道自适应 + 自适应EQ + 贴合度检测）
* Note(s)    : ANC耳道自适应输出的SZ，挂载多个算法处理
*********************************************************************
*/
int audio_ear_adaptive_app_open_demo(void)
{
    int ret = 0;

    //选择数据来源ANC
    int fre_sel = AUDIO_ADAPTIVE_FRE_SEL_ANC;
#if TCFG_AUDIO_ADAPTIVE_EQ_ENABLE
    //1. 注册 自适应EQ 流程
    ret = audio_adaptive_eq_open(fre_sel, audio_adaptive_eq_end_result);
    if (ret) {
        printf("adaptive eq open fail\n");
        goto __exit;
    }
#endif

#if TCFG_AUDIO_FIT_DET_ENABLE
    //2. 注册 贴合度检测 流程
    ret = audio_icsd_dot_open(fre_sel, audio_dot_end_result);
    if (ret) {
        printf("fit_det open fail\n");
        goto __exit;
    }
#endif

#if TCFG_AUDIO_ANC_EAR_ADAPTIVE_EN
    //3.播放提示音-启动ANC耳道自适应算法流程
    ret = audio_anc_mode_ear_adaptive(1);
    if (ret) {
        printf("ear adaptive anc open fail\n");
        goto __exit;
    }
#endif

    return 0;
__exit:	//处理启动异常的问题

    //重入场景不需要关闭对应模块
    if (ret == ANC_EXT_OPEN_FAIL_REENTRY) {
        printf("func open now\n");
        return ret;
    }
    printf("fail process\n");
#if TCFG_AUDIO_ADAPTIVE_EQ_ENABLE
    audio_adaptive_eq_close();
#endif

#if TCFG_AUDIO_FIT_DET_ENABLE
    audio_icsd_dot_close();
#endif
    return ret;
}

/*
*********************************************************************
*                  audio_ear_adaptive_app_forced_exit_demo
* Description: 强制打断单次（ANC耳道自适应 + 自适应EQ）
* Note(s)    :
*********************************************************************
*/
int audio_ear_adaptive_app_forced_exit_demo()
{
#if TCFG_AUDIO_ADAPTIVE_EQ_ENABLE
    audio_adaptive_eq_force_exit();
#endif
#if TCFG_AUDIO_ANC_EAR_ADAPTIVE_EN
    anc_ear_adaptive_forced_exit(0, 1);	//退出后保持ANC_OFF，同步等待自适应结束
#endif
    return 0;
}

/*
*********************************************************************
                audio_real_time_adaptive_app_open_demo
* Description: 打开实时（ANC耳道自适应 + 自适应EQ）
* Note(s)    : ANC实时自适应输出的SZ，挂载多个算法处理
*********************************************************************
*/
int audio_real_time_adaptive_app_open_demo(void)
{
    int ret = 0;
    printf("audio_real_time_adaptive_app_open_demo\n");

    //选择数据来源ANC
    int fre_sel = AUDIO_ADAPTIVE_FRE_SEL_ANC;
#if TCFG_AUDIO_ADAPTIVE_EQ_ENABLE
    //1. 注册 实时自适应EQ 流程
    ret = audio_real_time_adaptive_eq_open(fre_sel, audio_adaptive_eq_end_result);
    if (ret && (ret != ANC_EXT_OPEN_FAIL_REENTRY)) {
        printf("adaptive eq open fail\n");
        goto __exit;
    }
#endif

#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
    //2.打开实时自适应ANC
    ret = audio_anc_real_time_adaptive_open();
    if (ret) {
        printf("real time adaptive anc open fail, ret=%d\n", ret);
        goto __exit;
    }
#endif

    return 0;
__exit:	//处理启动异常的问题

    //重入场景不需要关闭对应模块
    if (ret == ANC_EXT_OPEN_FAIL_REENTRY) {
        printf("func open now\n");
        return ret;
    }
    printf("fail process\n");
#if TCFG_AUDIO_ADAPTIVE_EQ_ENABLE
    audio_real_time_adaptive_eq_close();
#endif
    return ret;
}

/*
*********************************************************************
                audio_real_time_adaptive_app_close_demo
* Description: 关闭实时（ANC耳道自适应 + 自适应EQ）
* Note(s)    : ANC实时自适应输出的SZ，挂载多个算法处理
*********************************************************************
*/
int audio_real_time_adaptive_app_close_demo(void)
{
    printf("audio_real_time_adaptive_app_close_demo\n");
#if TCFG_AUDIO_ADAPTIVE_EQ_ENABLE
    //1. 关闭实时自适应EQ
    audio_real_time_adaptive_eq_close();
#endif

#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
    //2.关闭实时自适应ANC
    audio_anc_real_time_adaptive_close();
#endif
    return 0;
}


/*
*********************************************************************
                audio_anc_switch_adt_app_open
* Description: 模式切换时，打开当前APP层的ADT算法
* Note(s)    : NA
*********************************************************************
*/
int audio_anc_switch_adt_app_open(void)
{
    int ret = 0;
#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
    u16 adt_mode = icsd_adt_app_mode_get();
    printf("audio_anc_switch_adt_app_open 0x%x\n", adt_mode);
#endif

    //选择数据来源ANC
#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
    //1. 注册 实时自适应EQ 流程
    if (adt_mode & ADT_REAL_TIME_ADAPTIVE_ANC_MODE) {
#if TCFG_AUDIO_ADAPTIVE_EQ_ENABLE
        int fre_sel = AUDIO_ADAPTIVE_FRE_SEL_ANC;
        ret = audio_real_time_adaptive_eq_open(fre_sel, audio_adaptive_eq_end_result);
        if (ret && (ret != ANC_EXT_OPEN_FAIL_REENTRY)) {
            printf("adaptive eq open fail\n");
            goto __exit;
        }
#endif
        audio_rtanc_var_cache_set(0);
    }
#endif

#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
    //2.打开ADT算法
    ret = audio_icsd_anc_switch_open(adt_mode);
    if (ret) {
        printf("anc_switch: adt app open fail, ret=%d\n", ret);
        goto __exit;
    }
#endif

    return 0;
__exit:	//处理启动异常的问题

    //重入场景不需要关闭对应模块
    if (ret == ANC_EXT_OPEN_FAIL_REENTRY) {
        printf("func open now\n");
        return ret;
    }
    printf("fail process\n");
#if TCFG_AUDIO_ADAPTIVE_EQ_ENABLE
    audio_real_time_adaptive_eq_close();
#endif
    return ret;
}

/*
*********************************************************************
                audio_anc_switch_adt_app_close
* Description: 模式切换时，关闭当前APP层的ADT算法
* Note(s)    : NA
*********************************************************************
*/
int audio_anc_switch_adt_app_close(void)
{
    printf("audio_anc_switch_adt_app_close\n");

#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
    u16 adt_mode = icsd_adt_app_mode_get();

#if TCFG_AUDIO_ADAPTIVE_EQ_ENABLE
    //1. 关闭实时自适应EQ
    if (adt_mode & ADT_REAL_TIME_ADAPTIVE_ANC_MODE) {
        audio_real_time_adaptive_eq_close();
    }
#endif

    //2.关闭ADT算法
    audio_icsd_anc_switch_close(adt_mode);
#endif
    return 0;
}

/*----------------------------------------------------------------*/
/*                         Test demo Start                        */
/*----------------------------------------------------------------*/

#include "icsd_anc_user.h"

void audio_real_time_adaptive_app_ctr_demo(void)
{
#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
    static u8 flag = 0;
    flag ^= 1;
    if (flag) {
        icsd_adt_tone_play(ICSD_ADT_TONE_NUM5);
        audio_real_time_adaptive_app_open_demo();
    } else {
        icsd_adt_tone_play(ICSD_ADT_TONE_NUM5);
        icsd_adt_tone_play(ICSD_ADT_TONE_NUM0);
        audio_real_time_adaptive_app_close_demo();
    }
#endif
}


void audio_anc_howl_det_toggle_demo()
{
#if ANC_HOWLING_DETECT_EN
    void anc_howling_detect_toggle(u8 toggle);
    static u8 flag = 0;
    flag ^= 1;
    anc_howling_detect_toggle(flag);
    /* if (flag) { */
    /* icsd_adt_tone_play(ICSD_ADT_TONE_NUM2); */
    /* } else { */
    /* icsd_adt_tone_play(ICSD_ADT_TONE_NUM2); */
    /* icsd_adt_tone_play(ICSD_ADT_TONE_NUM0); */
    /* } */
#endif
}

void audio_anc_env_det_toggle_demo()
{
#if ANC_ADAPTIVE_EN
    void audio_anc_power_adaptive_mode_set(u8 mode, u8 lvl);
    static u8 flag = 0;
    flag ^= 1;
    if (flag) {
        icsd_adt_tone_play(ICSD_ADT_TONE_NUM4);
        audio_anc_power_adaptive_mode_set(ANC_ADAPTIVE_GAIN_MODE, 0);
    } else {
        icsd_adt_tone_play(ICSD_ADT_TONE_NUM4);
        icsd_adt_tone_play(ICSD_ADT_TONE_NUM0);
        audio_anc_power_adaptive_mode_set(ANC_ADAPTIVE_MANUAL_MODE, 0);
    }
#endif/*ANC_EAR_ADAPTIVE_EN*/
}

/*----------------------------------------------------------------*/
/*                         Test demo End                          */
/*----------------------------------------------------------------*/
#endif
