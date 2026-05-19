#ifndef _TREMBLE_API_
#define  _TREMBLE_API_

#include "AudioEffect_DataType.h"

#ifndef u8
#define u8  unsigned char
#endif

#ifndef u16
#define u16 unsigned short
#endif

#ifndef s16
#define s16 short
#endif


#ifndef u32
#define u32 unsigned int
#endif


#ifndef s32
#define s32 int
#endif

#ifndef s16
#define s16 signed short
#endif

#define TREM_N   2


enum {
    TREMOLO_TYPE_SIN = 0,
    TREMOLO_TYPE_TRI
};


enum {
    TYPE_SIN = 0,
    TYPE_TRI
};

typedef  struct   _Tremble_parm_ {
    unsigned int nch;
    unsigned int samplerate;
    int LRdiffphase;
    int waveType;
    int outgain;
    float freq[TREM_N];
    int gain[TREM_N];
    int reserved;
    af_DataType dataTypeobj;
} Tremble_parm;



typedef struct _Tremble_STEREO_FUNC_API_ {
    u32(*need_buf)(void *vc_parm);
    u32(*tmp_buf_size)(void *vc_parm);
    int (*init)(void *ptr, void *vc_parm, void *tmpbuf);
    int (*set_tmpbuf)(void *ptr, void *tmpbuf);
    int (*run)(void *ptr, void *indata, void *outdata, int PointsPerChannel);
    int (*update)(void *ptr, void *vc_parm);
} Tremble_STEREO_FUNC_API;

#ifdef __cplusplus
extern "C"
{
extern Tremble_STEREO_FUNC_API *get_tremble_stereo_ops();
}
#else
extern Tremble_STEREO_FUNC_API *get_tremble_stereo_ops();
#endif


#endif
