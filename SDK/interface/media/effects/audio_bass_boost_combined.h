#ifndef __AUDIO_BASS_BOOST_COMBINED_H
#define __AUDIO_BASS_BOOST_COMBINED_H

#include "system/includes.h"
#include "media/includes.h"
#include "effects/audio_plate_reverb.h"
#include "effects/audio_reverb.h"
#include "effects/audio_echo.h"
#include "effects/audio_gain_process.h"
#include "effects/channel_adapter.h"
#include "effects/audio_eq.h"
#include "effects/audio_cbuf_pcm_delay.h"
#include "effects/audio_virtual_bass_classic.h"
#include "effects/audio_limiter.h"
#include "effects/audio_wdrc_advance.h"
#include "effects/audio_hw_crossover.h"


struct bass_boost_mix_gain_param_tool {
    int is_bypass;//打开时，只输出 gain0通道的数据
    float gain[3];
} ;

struct bass_boost_crossover_param {
    int way_num;        //段数
    int N;				//阶数
    int low_freq;	    //低中分频点
    int high_freq;      //高中分频点

    int high_freq1;     //保留
    int low_freq1;     //保留
};



//分频器 crossover:
struct bass_boost_crossover_param_tool {
    int is_bypass;     //打开时，只通过低频端口做输出，输出全频的信号,其余端口输出0数据
    struct bass_boost_crossover_param parm;
};


struct bass_boost_param_tool {
    int is_bypass;//打开时,模块直通
    struct bass_boost_crossover_param_tool crossover_param;
    virtual_bass_classic_param_tool_set wet_bass_param;
    struct limiter_param_tool_set bass_limiter_param;;
    struct limiter_param_tool_set mid_limiter_param;;
    struct limiter_param_tool_set high_limiter_param;;
    struct bass_boost_mix_gain_param_tool mix_gain_param;
    u16 len;   //存在多个可变长数据，wet drc[len、reserve data[0]], wet eq[len、reserve data[0]],dry drc[len、reserve data[0]], dry eq[len、reserve data[0]]
    u16 total_len;
    u8 tool[0];
};

struct bass_boost_info {
    u32 sample_rate;
    u8 in_bit_width;
    u8 out_bit_width;
    u8 qval;
    u8 in_ch_num;
    u8 out_ch_num;
    u8 close_flag;
    u16 param_len;
};
struct bass_boost_param {
    struct bass_boost_info info;
    struct bass_boost_param_tool cfg;
};
struct bs_tool {
    u16 len;
    u16 total_len;
    u8 tool[0];
};
struct bass_boost_combined {
    struct audio_eq           *low_crossover;
    struct audio_eq           *mid_crossover;
    struct audio_eq           *high_crossover;
    virtual_bass_classic_hdl  *wet_bass;
    struct audio_wdrc_advance *dry_drc;
    struct audio_wdrc_advance *wet_drc;
    struct audio_eq           *dry_eq;
    struct audio_eq           *wet_eq;
    MixParam                  dry_bass_mix;
    MixParam                  wet_bass_mix;
    struct audio_limiter      *bass_limiter;
    struct audio_limiter      *mid_limiter;
    struct audio_limiter      *high_limiter;
    MixParam                  mix_low;
    MixParam                  mix_mid;
    MixParam                  mix_high;
    struct bass_boost_param   *param;
    float                     low_coeff[10];
    float                     mid_coeff[2][10];
    float                     high_coeff[10];
    struct bs_tool            *wet_drc_tool;
    struct bs_tool            *wet_eq_tool;
    struct bs_tool            *dry_drc_tool;
    struct bs_tool            *dry_eq_tool;
    struct pcm_delay          *delay[5];//0:low  1:mid 2:high  3:dry_drc 4:wet_drc
};

#define BASS_BOOST_LOW_LIMITER    0
#define BASS_BOOST_MID_LIMITER    1
#define BASS_BOOST_HIGH_LIMITER   2
#define BASS_BOOST_DRY_DRC        3
#define BASS_BOOST_WET_DRC        4

#define BASS_BOOST_TWO_BAND  2
#define BASS_BOOST_THREE_BAND  3



unsigned int audio_bass_boost_combined_need_buf(struct bass_boost_param *param);
int audio_bass_boost_combined_open(struct bass_boost_combined *com, struct bass_boost_param *param, void *tmpbuf);
int audio_bass_boost_combined_run(struct bass_boost_combined *com, void *datain, void *dataout, int PointsPerChannel);
int audio_bass_boost_combined_update(struct bass_boost_combined *com, struct bass_boost_param *param);

void bass_boost_combined_printf(struct bass_boost_param *params);
extern struct effects_func_api bass_boost_ops;

#endif
