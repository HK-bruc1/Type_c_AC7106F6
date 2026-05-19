#ifndef FREQUENCY_COMPRESSOR_API_H
#define FREQUENCY_COMPRESSOR_API_H

#include "AudioEffect_DataType.h"
extern const int frequency_compressor_run_mode;

#ifdef WIN32

#define AT_FREQUENCY_COMPRESSOR(x)
#define AT_FREQUENCY_COMPRESSOR_CODE
#define AT_FREQUENCY_COMPRESSOR_CONST
#define AT_FREQUENCY_COMPRESSOR_SPARSE_CODE
#define AT_FREQUENCY_COMPRESSOR_SPARSE_CONST

#else

#define AT_FREQUENCY_COMPRESSOR(x)				__attribute__((section(#x)))
#define AT_FREQUENCY_COMPRESSOR_CODE			AT_FREQUENCY_COMPRESSOR(.freq_compressor.text.cache.L2.code)
#define AT_FREQUENCY_COMPRESSOR_CONST			AT_FREQUENCY_COMPRESSOR(.freq_compressor.text.cache.L2.const)
#define AT_FREQUENCY_COMPRESSOR_SPARSE_CODE		AT_FREQUENCY_COMPRESSOR(.freq_compressor.text)
#define AT_FREQUENCY_COMPRESSOR_SPARSE_CONST	AT_FREQUENCY_COMPRESSOR(.freq_compressor.text.const)

#endif

enum {
    FRE_EQ_IIR_TYPE_PEAKING = 0x02,
    FRE_EQ_IIR_TYPE_HIGH_SHELF,
    FRE_EQ_IIR_TYPE_LOW_SHELF
};

enum {
    INPUT_GAIN = 0x0,
    DETECT_GAIN,
    INPUT_DETECT_GAIN
};

struct frequency_compressor_effect {
    int bypass;                //勾选框 勾选为1
    unsigned int fc;                     //中心频率 范围 20 ~ 18000 默认1000 步进1
    float Q;                    //Q值      范围 0.1 ~ 10  默认 0.3  步进0.1 保留两位小数
    float attack_time;          //启动时间  范围1 ~ 1500  默认 5 步进1
    float release_time;         //释放时间  范围1~ 1500   默认 300 步进1
    float threshold;            //阈值      范围-90 ~ 0   默认 0    步进1 保留两位小数
    float ratio;                //压缩比例  范围 1 ~ 100 默认 1 步进0.5 保留两位小数
    float rms_time;             //rms 时间  范围 0.02 ~ 25 默认0.02  步进0.05 保留两位小数
    float gain;                 //增益  范围 0 ~ 20  默认0 步进1 保留两位小数
    char type;                  //滤波器类型 下拉菜单 Peaking(2) HighShelf(3) LowShelf(4) 默认Peaking
    char gain_type;             //增益类型 下拉菜单 InputGain(0) DetectGain(1) InputDetectGain(2) 默认InputDetectGain(2)
    char reserved[2];              //保留位
    int reserved1[4];           //保留位
};

struct frequency_compressor_param {
    int n_section;
    int channel;
    int sample_rate;
    int detectdata_inc;
    int detectdata_bit;
    float detect_time;
    af_DataType pcm_info;
    struct frequency_compressor_effect effect_param[0];
};

struct effects_func_api *get_frequency_compressor_ops();

#endif // !DYNAMICEQ_API_H

