#ifndef SPATIAL_IMP_CTRL_H
#define SPATIAL_IMP_CTRL_H

#include "effects/AudioEffect_DataType.h"
#include "generic/typedef.h"
extern const int spatial_imp_run_mode;//16t16 32t32
extern const int spatial_imp_fft_mode;//1浮点 2定点 0关闭
extern const int JL_HW_FFT_V;//3 FFT_V3(23/25/30/34/36/29 2的指数次幂)  4(27/28/50/52 支持非2指数次幂) 5(56)
extern const int spatial_imp_active_azi_group;//方位角控制  如果-1表示全包含
extern const int spatial_imp_active_ele_group;//俯仰角控制
extern const int spatial_imp_active_ild_group;//ild控制 0关 1开

#ifdef WIN32
#define SPATIAL_IMP(x)
#define SPATIAL_IMP_CODE
#define SPATIAL_IMP_CONST
#define SPATIAL_IMP_SPARSE_CODE
#define SPATIAL_IMP_SPARSE_CONST
#else
#define SPATIAL_IMP(x)  __attribute__((section(#x)))
#define SPATIAL_IMP_CODE			SPATIAL_IMP(.spatial_imp.text.cache.L2.code)
#define SPATIAL_IMP_CONST			SPATIAL_IMP(.spatial_imp.text.cache.L2.const)
#define SPATIAL_IMP_SPARSE_CODE		SPATIAL_IMP(.spatial_imp.text)
#define SPATIAL_IMP_SPARSE_CONST	SPATIAL_IMP(.spatial_imp.text.const)
#endif


struct SourceCtrl { //修改需要重新初始化
    int sampleRate;
    char FarField;// 0 1
    char mono_enable;//单通道输入模式使能；(单入双出),数据类型修改为AAABBB类型
    char switch_channel;//选择输出通道数,0,1,2
};

struct SourcePosition { //修改只需要更新
    int Azimuth_angle;//0-360
    int Elevation_angle;//0-360?
    float radius;//1~15
    int bias_angle;//0~180;
    float attenuation;//对两通道进行预衰减
};

struct hrir_intp { //隐藏不给客户调试 //修改需要重新初始化
    float referenceDistance;//hrtf测量参考距离
    float ListenerHeadRadius;
    short per_ch_point;//2^n 定义每帧处理的固定点数（单通道）
    short length_hrtf_sle;//选择hrtf进行处理的长度 ,必须是每帧处理的固定点数的倍数
    short real_hlength;//hrtf的实际长度
    short delay_time;//延迟时间0~20ms
    char intp;//插值使能
    char ildenable;//ild使能
    char rev_mode;//混响模式，0表示不开，1表示串联模式（先hrtf后rev），2表示并联模式
};

struct ERef_PARM_SET {
    int sr;
    int nch;//这里固定为1，方便处理；（输入数据通道，1单通道，2双通道;(会取平均处理，且输出左右相同)）
    int shortflag;//1表示跑16bit输入输出，0表示跑24bit输入输出
    int delaybuf_max_ms;      //影响申请的buf大小
    int Erfactor;         // 5%-100%      , early  roomsize
    int Ewidth;           // 0% - 100%  ,early  stereo
    int outGain;          //outputGain
    int lowcutoff;
    int early_taps;        //0-9
    unsigned char preset0_room;
    unsigned char preset0_roomgain;
};

struct rev_gain_set { //控制rev模式的增益，此参数不给客户调试
    float rev1_gain0;
    float rev1_gain1;
    float gain[8];
};



struct spatial_imp {
    struct SourceCtrl sc;
    struct SourcePosition sp;
    struct hrir_intp hp;
    af_DataType adt;
    struct ERef_PARM_SET es;
    struct rev_gain_set rs;
};

struct Spatial_imp {
    u32(*need_buf)(void *param);
    u32(*tmp_buf_size)(void *param);
    int(*init)(void *ptr, void *param, void *tmpbuf);
    int(*set_tmp_buf)(void *ptr, void *tmpbuf);
    int(*run)(void *ptr, void *indata, void *outdata, int PointPerChannel);
    int (*update)(void *ptr, void *param);
};
struct Spatial_imp *get_SpatialImp_ops();
int SPIMP_version_1_0_0();

#ifdef WIN32
#else
extern inline float sin_float(float x);
extern inline float cos_float(float x);
extern inline float  root_float(float x);
extern inline float angle_float(float x, float y);
extern inline float  log10_float(float x);
extern inline float  ln_float(float x);
extern inline float  exp_float(float x);
//extern inline float complex_dqdt_float(float x, float y);


#endif
#endif

