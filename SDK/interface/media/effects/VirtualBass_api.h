#ifndef VIRTUALBASS_API_H
#define VIRTUALBASS_API_H

#include "AudioEffect_DataType.h"

extern const int butterworth_iir_filter_coeff_type_select; //0 float 1 int
extern const int virtual_bass_eq_hard_select;
extern const int virtual_bass_run_mode;

#ifdef WIN32

#define AT_VBSS(x)
#define AT_VBSS_CODE
#define AT_VBSS_CONST
#define AT_VBSS_SPARSE_CODE
#define AT_VBSS_SPARSE_CONST

#else
#define AT_VBSS(x)           __attribute__((section(#x)))
#define AT_VBSS_CODE         AT_VBSS(.vbss.text.cache.L2.code)
#define AT_VBSS_CONST        AT_VBSS(.vbss.text.cache.L2.const)
#define AT_VBSS_SPARSE_CODE  AT_VBSS(.vbss.text)
#define AT_VBSS_SPARSE_CONST AT_VBSS(.vbss.text.const)
#endif


typedef struct _VirtualBassParam {
    int ratio;
    int boost;
    int fc;
    int ReserveLowFreqEnable;
    int noisegate_attack_time;
    int noisegate_release_time;
    int noisegate_hold_time;
    float threshold;
    int resever[3];
    int channel;
    int SampleRate;
    af_DataType pcm_info;
    struct hard_eq_func hard_eq;
} VirtualBassParam;

int getVirtualBassBuf();
int VirtualBassInit(void *WorkBuf, VirtualBassParam *param);
void VirtualBassUpdate(void *WorkBuf, VirtualBassParam *param);
int VirtualBassRun(void *WorkBuf, int *tmpbuf, void *in, void *out, int per_channel_npoint);
int virtual_bass_hard_eq_close(void *WorkBuf);

#endif // !VIRTUALBASS_API_H

