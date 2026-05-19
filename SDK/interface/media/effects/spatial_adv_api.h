#ifndef _SPATIAL_ADV_API_H_
#define _SPATIAL_ADV_API_H_

extern const int spatial_adv_run_mode;
extern const int spatial_adv_framesize_mode;//1--64最大帧长；2--128最大帧长

#if WIN32
#define SPATIAL_ADV(x)
#define SPATIAL_ADV_CODE
#define SPATIAL_ADV_CONST
#define SPATIAL_ADV_SPARSE_CODE
#define SPATIAL_ADV_SPARSE_CONST
#else
#define SPATIAL_ADV(x)  __attribute__((section(#x)))
#define SPATIAL_ADV_CODE			SPATIAL_ADV(.spatial_adv.text.cache.L2.code)
#define SPATIAL_ADV_CONST			SPATIAL_ADV(.spatial_adv.text.cache.L2.const)
#define SPATIAL_ADV_SPARSE_CODE		SPATIAL_ADV(.spatial_adv.text)
#define SPATIAL_ADV_SPARSE_CONST	SPATIAL_ADV(.spatial_adv.text.const)
#endif

#include "fix_iir_filter_api.h"
#include "AudioEffect_DataType.h"

struct iir_filter_2 {
    int (*need_fix_iir_filter_buf)(struct iir_filter_param *param);
    int (*fix_iir_filter_init)(void *work_buf, struct iir_filter_param *param);
    int (*fix_iir_filter_run)(void *work_buf, void *indata, void *outdata, int per_channel_npoint);
    int (*fix_iir_filter_update)(void *work_buf, struct iir_filter_param *param);
};

struct spatial_adv {
    short delay;        //pcm_delay 单位ms
    int fs;
    char channel;       //固定传2
    char tws;           //0表示双通道计算，1表示单通道计算
    struct iir_filter_param iir1;
    struct iir_filter_param iir2;
    struct iir_filter_param iir3;
    float gain_stereomix[4];
    float gain_bandmerge[3];
    struct iir_filter_2 *iir_filt;
    af_DataType *adt;
    char switch_channel;//0计算左通道，1计算右通道，2计算双通道，需要配合tws参数设置
};

struct spatial_adv_func_api {
    unsigned int (*need_buf)(void *param);
    unsigned int (*tmp_buf_size)(void *param);
    int(*init)(void *ptr, void *param, void *tmpbuf);
    int(*set_tmp_buf)(void *ptr, void *tmpbuf);
    int(*run)(void *ptr, void *indata, void *outdata, int PointPerChannel);
    int (*update)(void *ptr, void *param);
};

struct spatial_adv_func_api *get_spatial_adv_ops();

#endif // !SPATIAL_ADV_API_H

