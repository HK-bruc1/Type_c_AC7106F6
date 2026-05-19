#ifndef _AUDIO_THRESHOLD_DET_H_
#define _AUDIO_THRESHOLD_DET_H_

#include "generic/typedef.h"

/*
                    使用示例：
   1、阈值表：
   const int noise_det_thr_table[] = (0, 40, 80, 120, 160);
   第0档位：0~39
   第1档位：40~79
   第2档位：80~119
   第3档位：120~159
   第4档位：>=160
   数组元素数量即为档位数量

   2、档位切换逻辑(以thr_debounce=10为例)：
    (1)当前处于第0档时，噪声水平>[40+10]，维持lvl_up_hold_time，进入第1档
    (2)当前处于第1档时，噪声水平>[80+10]，维持lvl_up_hold_time，进入第2档
       当前处于第1档时，噪声水平<[39-10]，维持lvl_down_hold_time，进入第0档
    (3)当前处于第2档时，噪声水平<[79-10]，维持lvl_down_hold_time，进入第1档
    以此类推......
*/

struct threshold_det_param {
    const int *thr_table;   //默认阈值表
    u16 run_interval;       //该模块每次处理的时间间隔 ms
    u16 lvl_up_hold_time;   //升档需要的维持时间 ms
    u16 lvl_down_hold_time; //降档需要的维持时间 ms
    u8 thr_lvl_num;         //档位数，使用ARRAY_SIZE(noise_det_thr_table)传参，防止越界
    u8 thr_debounce;        //消抖值
    u8 default_lvl;         //模块打开时的初始档位
};

//可动态更新的参数
struct threshold_det_update_param {
    int *thr_table;
    u16 run_interval;
    u16 lvl_up_hold_time;
    u16 lvl_down_hold_time;
    u8 thr_lvl_num;
    u8 thr_debounce;
};


void *audio_threshold_det_open(struct threshold_det_param *param);
void audio_threshold_det_close(void *thr_det_hdl);
u8 audio_threshold_det_run(void *thr_det_hdl, int noise_lvl);
void audio_threshold_det_lvl_reset(void *thr_det_hdl, u8 lvl);
void audio_threshold_det_update_param(void *thr_det_hdl, struct threshold_det_update_param *update_param);

#endif
