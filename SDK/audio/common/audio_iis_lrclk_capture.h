#ifndef _AUDIO_IIS_LRCLK_CAPTURE_H_
#define _AUDIO_IIS_LRCLK_CAPTURE_H_

#include "media/audio_iis.h"

/*
 *当前仅支持只使用一个iis rx通道的情况
 * */

struct iis_lrclk_capture {
    u8 module_idx;      //alink 模块 0：alink0, 1:alink1
    u8 need_reopen;
    u8 alink_sr_tab_num;
    u16 timer;
    u32 *alink_sr_tab;  //支持的采样率
    int (*open)(void);	//采样率发生改变后，重新开启模块
    void (*close)(void);//采样率发生改变后，关闭模块
};

/*
 *iis lrclk capture init
 * */
void audio_iis_lrclk_capture_init(struct iis_lrclk_capture *hdl);
/*
 *iis lrclk capture uninit
 * */
void audio_iis_lrclk_capture_uninit(struct iis_lrclk_capture *hdl);
/*
 *获取当前捕获到的lrclk频率(采样率)
 * */
u32 audio_iis_get_lrclk_sample_rate(u8 module_idx);

#define AUDIO_IIS_LRCLK_CAPTURE_EN 0

#endif

