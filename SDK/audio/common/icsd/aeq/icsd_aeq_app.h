
#ifndef __ICSD_AEQ_APP_H_
#define __ICSD_AEQ_APP_H_

#include "typedef.h"
#include "icsd_aeq.h"
#include "app_config.h"
#include "effects/effects_adj.h"
#include "effects/eq_config.h"
#include "icsd_common_v2_app.h"

/*-----------------------------------------------------------------
              定时启动实时自适应EQ(ANC_OFF场景)
  设计意义：实时自适应EQ，ANC_OFF与ANC_ON功耗接近, 因此使用
            间隔开关的方式来节约ANC_OFF场景的功耗

  注意：    处于关闭状态时，EQ不会自适应更新
------------------------------------------------------------------*/
/*
   ANC OFF间隔开关RT AEQ使能:
       使能1：ANC_OFF(音乐场景)按照时间配置，定时开关RT AEQ
       失能0: ANC_OFF(音乐场景)常开RT AEQ
*/
#define RT_AEQ_ANCOFF_TIMER_ENABLE   0
#define RT_AEQ_ANCOFF_OPEN_TIMES     (1000 * 10) 		//ANC OFF 开实时自适应EQ的时间长度，ms
#define RT_AEQ_ANCOFF_CLOSE_TIMES    (1000 * 5 * 60)	//ANC OFF 关实时自适应EQ的时间长度，ms


#define ADAPTIVE_EQ_TARGET_NODE_NAME 			"AEQ"			//自适应EQ目标节点名称
#define ADAPTIVE_EQ_VOLUME_NODE_NAME			"Vol_BtmMusic"	//自适应EQ依赖音量节点名称
#define ADAPTIVE_EQ_ORDER						10

#define ADAPTIVE_EQ_TARGET_DEFAULT_CFG_READ		0				//自适应EQ读取目标节点参数使能，如果AEQ没有独立节点，则必须读取

#define ADAPTIVE_EQ_VOLUME_GRADE_EN				0			//根据音量分档使能, 关闭则以最大音量配置为准
#define ADAPTIVE_EQ_TIGHTNESS_GRADE_EN			1			//根据佩戴松紧度分档使能，关闭则以松配配置为准

#define ADAPTIVE_EQ_ONLY_IN_MUSIC_UPDATE		0			//(实时AEQ)仅在播歌/通话的时候更新

#if ADAPTIVE_EQ_TIGHTNESS_GRADE_EN && (!TCFG_AUDIO_FIT_DET_ENABLE)
#error "AEQ 必须打开贴合度检测功能"
#endif


//ICSD AEQ 状态
enum {
    ADAPTIVE_EQ_STATE_CLOSE = 0,
    ADAPTIVE_EQ_STATE_OPEN,
    ADAPTIVE_EQ_STATE_RUN,
    ADAPTIVE_EQ_STATE_STOP,
};

//ICSD AEQ 滤波器模式
enum ADAPTIVE_EFF_MODE {
    AEQ_EFF_MODE_DEFAULT = 0,		//默认参数
    AEQ_EFF_MODE_ADAPTIVE,			//自适应参数
};

/*
   自适应EQ功能打开
	param: fre_sel  	训练数据来源
		   result_cb,	训练结束回调接口(如没有可传NULL)
	return 0  打开成功； 1 打开失败
*/
int audio_adaptive_eq_open(enum audio_adaptive_fre_sel fre_sel, void (*result_cb)(int result));

//(实时)自适应EQ打开
int audio_real_time_adaptive_eq_open(enum audio_adaptive_fre_sel fre_sel, void (*result_cb)(int result));

//(实时)自适应EQ退出
int audio_real_time_adaptive_eq_close(void);

int audio_adaptive_eq_close();

int audio_adaptive_eq_init(void);

//查询AEQ 算法是否运行中
u8 audio_adaptive_eq_is_running(void);

//查询AEQ 当前状态
u8 audio_adaptive_eq_state_get(void);

//读取自适应AEQ结果
struct eq_default_seg_tab *audio_icsd_adaptive_eq_read(void);

/*
   自适应EQ滤波器模式设置
   param: mode 0 普通参数； 1 自适应参数
*/
int audio_adaptive_eq_eff_set(enum ADAPTIVE_EFF_MODE mode);

//删除AEQ 自适应系数记忆链表，需等下次自适应输出才有自适应参数
int audio_adaptive_eq_cur_list_del(void);

int audio_adaptive_eq_vol_update(s16 volume);

int audio_adaptive_eq_process(void);

// (单次)自适应EQ强制退出
int audio_adaptive_eq_force_exit(void);

int audio_adaptive_eq_tool_data_get(u8 **buf, u32 *len);

int audio_adaptive_eq_tool_sz_data_get(u8 **buf, u32 *len);

int audio_rtaeq_in_ancoff_open(void);

int audio_rtaeq_in_ancoff_close(void);

u8 audio_rtaeq_ancoff_timer_state_get(void);

#endif/*__ICSD_AEQ_APP_H_*/

