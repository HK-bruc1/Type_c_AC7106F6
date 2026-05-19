#ifndef StereoToLCR_Lib_H
#define StereoToLCR_Lib_H

#include "AudioEffect_DataType.h"

#define Flag_debug_printf_on 0

#define LCR_NAMESPACE  lcr_namespace

#ifdef __cplusplus
extern "C"
{
#endif

extern const int StereoToLCR_24bit_sat;  // 等于1或者0， 控制是否对输出信号进行饱和处理（1：进行饱和处理，0：不进行饱和处理），仅在输入输出均为 DATA_INT_32BIT 且Q=23时候生效！
extern const int StereoToLCR_run_mode;   // 注意！算法仅支持16to16（if (StereoToLCR_run_mode & 0x01)...）, 32to32(if (StereoToLCR_run_mode & 0x08))两种模式

typedef struct _StereoToLCR_Api {
    af_DataType pcm_info;

    float MidComponentExtractRatio;     // 修改只需要update, 不需要重新计算runbuf和tmpbuf大小，不需要重新分配和init
    int freqCut_low;                    // 修改需要重新计算runbuf_size,tmpbuf_size, 重新申请分配runbuf和tmpbuf内存， 重新init, 重新set_tmp_buf
    int freqCut_high;                   // 修改需要重新计算runbuf_size,tmpbuf_size, 重新申请分配runbuf和tmpbuf内存， 重新init, 重新set_tmp_buf
    int sample_rate;                    // 修改需要重新计算runbuf_size,tmpbuf_size, 重新申请分配runbuf和tmpbuf内存， 重新init, 重新set_tmp_buf
    int enable_smooth;                  // 修改需要重新计算runbuf_size,tmpbuf_size, 重新申请分配runbuf和tmpbuf内存， 重新init, 重新set_tmp_buf
    float coeff_smooth;                 // 修改只需要update, 不需要重新计算runbuf和tmpbuf大小，不需要重新分配和init
    int run_model_512;                  // 修改需要重新计算runbuf_size,tmpbuf_size, 重新分配runbuf和tmpbuf内存， 重新init, 重新set_tmp_buf
} StereoToLCR_Api_t;

unsigned int StereoToLCR_BufSize(void *lcr_api);
unsigned int StereoToLCR_TempBufSize(void *lcr_api);
int StereoToLCR_Init(void *workbuf, void *_lcr_api, void *tmpbuf);
int StereoToLCR_SetTempBuf(void *workbuf, void *tmpbuf);
int StereoToLCR_Run(void *workbuf, void *indata, void *outdata, int per_channel_npoint);
int StereoToLCR_Update(void *workbuf, void *lcr_api);

int libStereoToLCR_pi32v2_OnChip_version_1_0_0();

struct StereoToLCR_func_api_t {
    unsigned int (*need_buf)(void *parm);
    unsigned int (*tmp_buf_size)(void *parm);
    int (*init)(void *ptr, void *pram, void *tmpbuf);
    int (*set_tmpbuf)(void *ptr, void *tmpbuf);
    int (*run)(void *ptr, void *indata, void *outdata, int PointPerChannel);
    int (*update)(void *ptr, void *parm);
};

struct StereoToLCR_func_api_t *get_StereoToLCR_func_api();

#ifdef __cplusplus
}
#endif
#endif
