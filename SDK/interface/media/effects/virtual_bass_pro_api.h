#ifndef VIRTUAL_BASS_PRO_API_H
#define VIRTUAL_BASS_PRO_API_H

#include "AudioEffect_DataType.h"
#include "crossOver.h"
#include "drc_advance_api.h"
#include "fix_iir_filter_api.h"
#include "audio_hw_crossover.h"
#include "virtual_bass_classic_api.h"
#include "hw_eq.h"

extern const int virtual_bass_pro_soft_crossover;

#ifdef WIN32

#define AT_VIRTUAL_BASS_PRO(x)
#define AT_VIRTUAL_BASS_PRO_CODE
#define AT_VIRTUAL_BASS_PRO_SPARSE_CODE
#define AT_VIRTUAL_BASS_PRO_SPARSE_CONST
#else
#define AT_VIRTUAL_BASS_PRO(x)            __attribute__((section(#x)))
#define AT_VIRTUAL_BASS_PRO_CODE          AT_VIRTUAL_BASS_PRO(.virtual_bass_pro.text.cache.L2.code)
#define AT_VIRTUAL_BASS_PRO_SPARSE_CODE   AT_VIRTUAL_BASS_PRO(.virtual_bass_pro.text)
#define AT_VIRTUAL_BASS_PRO_SPARSE_CONST  AT_VIRTUAL_BASS_PRO(.virtual_bass_pro.text.const)
#endif

struct drc_advance_fun {
    int(*need_buf)(DrcAdvanceParam *param);
    int(*init)(void *WorkBuf, DrcAdvanceParam *param);
    void(*update)(void *WorkBuf, DrcAdvanceParam *param);
    int(*run)(void *WorkBuf, void *indata, void *outdata, int per_channel_npoint);
};

struct iir_filter_func {
    int(*need_buf)(struct iir_filter_param *param);
    int(*init)(void *work_buf, struct iir_filter_param *param);
    int(*update)(void *work_buf, struct iir_filter_param *param);
    int(*run)(void *work_buf, void *indata, void *outdata, int per_channel_npoint);
    int(*clear_mem)(void *work_buf);
};

struct virtual_bass_classic_func {
    int(*need_buf)(struct virtual_bass_class_param *param);
    int(*init)(void *WorkBuf, struct virtual_bass_class_param *param);
    int(*update)(void *WorkBuf, struct virtual_bass_class_param *param);
    int(*run)(void *WorkBuf, int *tmpbuf, void *in, void *out, int per_channel_npoint);
};

struct crossover_func {
    int(*need_buf)(CrossOverParam *param);
    int(*tempbuf)();
    void(*init)(void *workBuf, void *tempbuf, CrossOverParam *param);
    void(*update)(void *workBuf, void *tempbuf, CrossOverParam *param);
};

struct iir_filter_design_func {
    void(*get_lp_coeff)(int fc, int sample_rate, float quality_factor, float *coeff);
    void(*get_hp_coeff)(int fc, int sample_rate, float quality_factor, float *coeff);
};

struct virtual_bass_pro_param {
    int   fc;
    int   dry_high_pass_fc;
    float dry_gain;

    int noisegate_attack_time;
    int noisegate_release_time;
    int noisegate_attack_hold_time;
    float noisegate_threshold;

    float compressor_attack_time;
    float compressor_release_time;
    float wet_compressor_threshold;
    float wet_compressor_ratio;
    float wet_gain;

    int wet_low_pass_fc;
    int wet_high_pass_fc;
    int resever[10];
    int channel;
    int sample_rate;
    int close_flag;
    int per_channel_npoint;

    af_DataType pcm_info;
    struct crossover_func crossover_obj;
    struct drc_advance_fun drc_advance_obj;
    struct iir_filter_func iir_filter_obj;
    struct virtual_bass_classic_func virtual_bass_obj;
    struct hard_eq_func hard_eq_obj;
    struct iir_filter_design_func iir_filter_design_obj;
};

struct effects_func_api *get_virtual_bass_pro_fun();

#endif
