#ifndef _LFaudio_PLC_API_H
#define _LFaudio_PLC_API_H

#include "effects/AudioEffect_DataType.h"


typedef struct _LFaudio_plc_parm_ {
    int sr;
    char nch;
    char mode;       //mode从0到4， 0延时最大，过渡最平缓。4是最低延时，过渡最快
    short lf_audio_plc_duration_ms;
    short laudio_plc_freeze_ms;
    short recover_dropHalf;
    short reserved;
    af_DataType dataTypeobj;
} LFaudio_plc_parm;

typedef struct _LFaudio_PLC_API {
    unsigned int (*need_buf)(LFaudio_plc_parm *lfaplc_parm);
    int (*open)(unsigned int *ptr, LFaudio_plc_parm *lfaplc_parm);
    int (*run)(unsigned int *ptr, void *inbuf, void *obuf, short len, short err_flag);   //len是按多少个点的，inbuf跟obuf可以同址的
} LFaudio_PLC_API;


extern LFaudio_PLC_API *get_lfaudioPLC_api();



#endif
