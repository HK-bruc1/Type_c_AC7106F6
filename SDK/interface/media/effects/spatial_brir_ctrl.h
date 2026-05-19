#ifndef SPATIAL_BRIR_CTRL_H
#define SPATIAL_BRIR_CTRL_H

#include "AudioEffect_DataType.h"
#include "generic/typedef.h"

extern const int spatial_brir_run_mode;//16t16 32t32
extern const int spatial_brir_azimuth;//每个角度对应一个bit位；从0~10~180共19个

#ifdef WIN32
#define SPATIAL_BRIR(x)
#define SPATIAL_BRIR_CODE
#define SPATIAL_BRIR_CONST
#define SPATIAL_BRIR_SPARSE_CODE
#define SPATIAL_BRIR_SPARSE_CONST
#else
#define SPATIAL_BRIR(x)  __attribute__((section(#x)))
#define SPATIAL_BRIR_CODE			SPATIAL_BRIR(.SPATIAL_BRIR.text.cache.L2.code)
#define SPATIAL_BRIR_CONST			SPATIAL_BRIR(.SPATIAL_BRIR.text.cache.L2.const)
#define SPATIAL_BRIR_SPARSE_CODE		SPATIAL_BRIR(.SPATIAL_BRIR.text)
#define SPATIAL_BRIR_SPARSE_CONST	SPATIAL_BRIR(.SPATIAL_BRIR.text.const)
#endif

struct SourceCtrl_brir { //修改需要重新初始化
    int sampleRate;
    char mono_enable;//单通道输入模式使能；(单入双出)
    char switch_channel;//选择输出通道数,0,1,2

};

struct SourcePosition_brir { //修改只需要更新
    int Azimuth_angle;//0-360
    int Elevation_angle;//0-360?
    int bias_angle;//0~180;
};

struct brir_intp { //隐藏不给客户调试 //修改需要重新初始化
    short per_ch_point;//2^n 定义每帧处理的固定点数（单通道）
    short length_brir_sle;//选择brir进行处理的长度 ,必须是每帧处理的固定点数的倍数
    short real_blength;//brir的实际长度
    short mode_mix;//处理方式是否是混音,0混音，1不混
};

struct brir_gain_set {
    float gain0;
    float gain1;
    float rev1;
    int rev2;
};

struct spatial_brir {
    struct SourceCtrl_brir sc;
    struct SourcePosition_brir sp;
    struct brir_intp hp;
    af_DataType adt;
    struct brir_gain_set bs;
};

struct spatial_fun_brir {
    u32(*need_buf)(void *param);
    u32(*tmp_buf_size)(void *param);
    int(*init)(void *ptr, void *param, void *tmpbuf);
    int(*set_tmp_buf)(void *ptr, void *tmpbuf);
    int(*run)(void *ptr, void *indata, void *outdata, int PointPerChannel);
    int (*update)(void *ptr, void *param);
};
struct spatial_fun_brir *get_SpatialBrir_ops();
int spatial_brir_version_1_0_0();
int get_index(struct spatial_brir *); //输出spatial_brir_azimuth值(如果写-1所有角度都包含)
int get_delaytime(struct spatial_brir *); //输出延迟点数
#endif

