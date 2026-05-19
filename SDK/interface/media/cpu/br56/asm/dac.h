#ifndef __CPU_DAC_H__
#define __CPU_DAC_H__


#include "generic/typedef.h"
#include "generic/atomic.h"
#include "os/os_api.h"
#include "audio_src.h"
#include "system/spinlock.h"
#include "audio_def.h"
#include "audio_general.h"

/***************************************************************************
  							Audio DAC Features
Notes:以下为芯片规格定义，不可修改，仅供引用
***************************************************************************/
#define AUDIO_DAC_CHANNEL_NUM	2	//DAC通道数

#define DACVDD_LDO_1_20V        0
#define DACVDD_LDO_1_25V        1
#define DACVDD_LDO_1_30V        2
#define DACVDD_LDO_1_35V        3

#define AUDIO_DAC_SYNC_IDLE             0
#define AUDIO_DAC_SYNC_START            1
#define AUDIO_DAC_SYNC_NO_DATA          2
#define AUDIO_DAC_SYNC_ALIGN_COMPLETE   3

#define AUDIO_SRC_SYNC_ENABLE   1
#define SYNC_LOCATION_FLOAT      1
#if SYNC_LOCATION_FLOAT
#define PCM_PHASE_BIT           0
#else
#define PCM_PHASE_BIT           8
#endif

/************************************
 *              DAC模式
 *************************************/
#define DAC_OFFSET_FADE_OUT             0
#define DAC_OFFSET_FADE_IN              1

/************************************
             dac性能模式
************************************/
#define DAC_TRIM_SEL_RDAC_LP            0
#define DAC_TRIM_SEL_RDAC_LN            0
#define DAC_TRIM_SEL_RDAC_RP            1
#define DAC_TRIM_SEL_RDAC_RN            1
#define DAC_TRIM_SEL_PA_LP              2
#define DAC_TRIM_SEL_PA_LN              2
#define DAC_TRIM_SEL_PA_RP              3
#define DAC_TRIM_SEL_PA_RN              3
// #define DAC_TRIM_SEL_VCM_L              4    (unused)
// #define DAC_TRIM_SEL_VCM_R              5    (unused)

#define DAC_TRIM_CH_L                  0
#define DAC_TRIM_CH_R                  1

/************************************
             hpvdd档位
************************************/
#define DAC_HPVDD_18V              (0)
#define DAC_HPVDD_12V              (1)

/************************************
             DAC输出功率档位
************************************/
#define DAC_POWER_MODE_20mW			0
#define DAC_POWER_MODE_30mW			1
#define DAC_POWER_MODE_50mW			2
#define DAC_POWER_MODE_80mW			3
#define DAC_POWER_MODE_100mW		4
#define DAC_POWER_MODE_2Vrms_THD	5
#define DAC_POWER_MODE_2Vrms_SNR	6

//在应用层重定义 audio_dac_power_state 函数可以获取dac模拟开关的状态
//void audio_dac_power_state(u8 state){}

struct audio_dac_hdl;
struct dac_platform_data {
    u8 vcm_cap_en;      //配1代表走外部通路,vcm上有电容时,可以提升电路抑制电源噪声能力，提高ADC的性能，配0相当于vcm上无电容，抑制电源噪声能力下降,ADC性能下降
    u8 power_on_mode;
    u8 performance_mode;
    u8 power_mode;          // DAC 功率模式， 0:20mw  1:30mw  2:50mw  3:80mw
    u8 dacldo_vsel;         // DACLDO电压档位:0~15
    u8 pa_isel0;            // 电流档位0:2~6
    u8 pa_isel1;            // 电流档位1:2~7
    u8 hpvdd_sel;
    u8 l_ana_gain;
    u8 r_ana_gain;
    u8 dcc_level;
    u8 bit_width;             	// DAC输出位宽
    u8 fade_en;
    u8 fade_points;
    u8 fade_volume;
    u8 classh_en;           // CLASSH使能(当输出功率为50mW时可用)
    u8 classh_mode;         // CLASSH 模式  0：蓝牙最低电压1.2v  1:蓝牙最低电压1.15v
    u8 mute_delay_time;         //开关机延时参数
    u8 mute_delay_isel;         //开关机速度参数
    u8 ana_vol_delay_time;      //设置模拟增益单独使用的参数
    u8 ana_vol_delay_isel;
    u8 miller_en;           // miller使能
    u8 dacvcm_sel;          // DACVCM电源选择, 0:AUDIO内部电源 1:PMU电源
    u16 dma_buf_time_ms;    // DAC dma buf 大小
    s16 *dig_vol_tab;
    u32 digital_gain_limit;
    u32 max_sample_rate;    	// 支持的最大采样率
    u32 classh_down_step;   // DAC classh 电压下降步进，1us/setp，配置范围[0.1s, 8s]，建议配置3s
    float fixed_pns;        	// 固定pns,单位ms
};

struct trim_init_param_t {
    u8 clock_mode;
    u8 power_level;
    s16 precision;
    float trim_speed;
    struct audio_dac_trim *dac_trim;
    struct audio_dac_hdl *dac;              /* DAC设备*/
};

struct audio_dac_trim {
    s16 left;
    s16 right;
    s16 vcomo;
};

// *INDENT-OFF*
struct audio_dac_sync {
    u8 channel;
    u8 start;
    u8 fast_align;
    int fast_points;
    u32 input_num;
    int phase_sub;
    int in_rate;
    int out_rate;
#if AUDIO_SRC_SYNC_ENABLE
    struct audio_src_sync_handle *src_sync;
    void *buf;
    int buf_len;
    void *filt_buf;
    int filt_len;
#else
    struct audio_src_base_handle *src_base;
#endif
#if SYNC_LOCATION_FLOAT
    double pcm_position;
#else
    u32 pcm_position;
#endif
    void *priv;
    void (*handler)(void *priv, u8 state);
};

struct dac_offset_fade_param {
    u16 fade_target;    // 淡入目标值
    u16 fade_freq;      // 淡入频率
    s16 fade_step;      // 淡入步进
};

struct dac_offset_fade {
    u8 trim_fade_en;
    u8  direction;
    s16 l_target;
    s16 r_target;
    s16 l_step;
    s16 r_step;
    u16 fade_timer;
    struct dac_offset_fade_param param;
    OS_SEM sem;
};

struct audio_dac_hdl {
    u8 analog_inited;
    u8 digital_inited;
    volatile u8 state;
    u8 gain;
    u8 channel;
    u8 power_on;
    u8 need_close;
    u8 mute_ch;               //DAC PA Mute Channel
	u8 dvol_mute;             //DAC数字音量是否mute
    u8 dac_read_flag;	//AEC可读取DAC参考数据的标志
    u8 anc_dac_open;
    u8 protect_fadein;
    u8 vol_set_en;
    u16 d_volume[2];
    u16 start_ms;
    u16 delay_ms;
    u16 start_points;
    u16 delay_points;
    u16 prepare_points;//未开始让DAC真正跑之前写入的PCM点数
    u16 irq_points;
    s16 protect_time;
    s16 protect_pns;
    s16 fadein_frames;
    s16 fade_vol;
    u16 unread_samples;             /*未读样点个数*/
    u16 mute_timer;           //DAC PA Mute Timer
    s16 cur_trim_left; //保存当前使用的trim值，AB版芯片需要对trim值做淡入淡出
    s16 cur_trim_right;
    s16 *output_buf;
    u32 sample_rate;
    u32 digital_gain_limit;
    u32 output_buf_len;
    OS_SEM *sem;
    OS_MUTEX mutex;
    spinlock_t lock;
    struct dac_offset_fade offset;
    const struct dac_platform_data *pd;
	struct audio_dac_noisegate ng;
    void (*fade_handler)(u8 left_gain, u8 right_gain);
	u8 (*is_aec_ref_dac_ch)(void *dac_ch);
	void (*irq_handler_cb)(void);
};


void audio_dac_offset_fade(u8 fade);    // 0:淡出到默认trim值 1:淡入到设定的目标值
#endif

