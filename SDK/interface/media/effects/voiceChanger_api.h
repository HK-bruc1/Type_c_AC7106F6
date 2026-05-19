
#ifndef pitchshifer_api_h__
#define pitchshifer_api_h__


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

/*#define  EFFECT_OLD_RECORD          0x01
#define  EFFECT_MOYIN               0x0*/
//#define  EFFECT_ROBORT_FLAG          0X04

enum {
    EFFECT_VOICECHANGE_PITCHSHIFT = 0x00,
    EFFECT_VOICECHANGE_CARTOON = 0x01,
    EFFECT_VOICECHANGE_SPECTRUM = 0x02,
    EFFECT_VOICECHANGE_ROBORT = 0x03,
    EFFECT_VOICECHANGE_MELODY = 0x04,
    EFFECT_VOICECHANGE_WHISPER = 0x05,
    EFFECT_VOICECHANGE_F0_DOMAIN = 0x06,
    EFFECT_VOICECHANGE_F0_TD = 0x07,
    EFFECT_VOICECHANGE_FEEDBACK = 0x08,
    EFFECT_VOICECHANGE_NULL = 0xff,
};


enum {
    MODE_C_MAJOR = 0x0,
    MODE_Csharp_MAJOR,
    MODE_D_MAJOR,
    MODE_Dsharp_MAJOR,
    MODE_E_MAJOR,
    MODE_F_MAJOR,
    MODE_Fsharp_MAJOR,
    MODE_G_MAJOR,
    MODE_Gsharp_MAJOR,
    MODE_A_MAJOR,
    MODE_Asharp_MAJOR,
    MODE_B_MAJOR,
    MODE_KEY    // 按12个key来趋近，不确定用的是哪个调式的时候配这个，工具默认值用这个
};

enum {
    PLATFORM_VOICECHANGE_CORDIC = 0,
    PLATFORM_VOICECHANGE_CORDICV2 = 1
};

enum {
    PLATFORM_VOICECHANGE_FFT = 0,
    PLATFORM_VOICECHANGE_FFTV2 = 1
};

typedef struct _VOICECHANGER_PARM {
    u32 effect_v;                    //
    u32 shiftv;                      //pitch rate:  40-250
    u32 formant_shift;               // 40-250
    af_DataType dataTypeobj;
} VOICECHANGER_PARM;


typedef struct _AUTOTUNE_PARM {
    u32 mode;                        //调式
    u32 speedv;                      //2到100
    af_DataType dataTypeobj;
} AUTOTUNE_PARM;


typedef struct _VIBRATO_PARM {
    u32 sr;
    u32 nch;
    u32 amplitude;                   //pitch rate:  1000<=>1
    u32 period;                   // 1000<=>1hz
    u32 reserved0;
    af_DataType dataTypeobj;

} VIBRATO_PARM;


enum {
    ALG_HARMONY_PITCHSHIFT = 1,
    ALG_HARMONY_SPECTRUM = 2,
    ALG_HARMONY_F0_TD = 3,
    ALG_HARMONY_NULL
};


typedef struct _HARMONY_PARM {
    u32 sr;
    u32 nch;
    u32 effect_v;
    int keyval;
    u32 formantshift;
    u32 mode;
    u32 reserved0;
    af_DataType dataTypeobj;

} HARMONY_PARM;


typedef struct _AUTOTUNE_FUNC_API_ {
    u32(*need_buf)(void *ptr, AUTOTUNE_PARM *vc_parm);
    int (*open)(void *ptr, u32 sr, AUTOTUNE_PARM *vc_parm);       //中途改变参数，可以调init
    void (*run)(void *ptr, short *indata, short *outdata, int len);    //len是多少点数
    void (*init)(void *ptr, AUTOTUNE_PARM *vc_parm);        //中途改变参数，可以调init
} AUTOTUNE_FUNC_API;


typedef struct _VOICECHANGER_FUNC_API_ {
    u32(*need_buf)(void *ptr, VOICECHANGER_PARM *vc_parm);
    int (*open)(void *ptr, u32 sr, VOICECHANGER_PARM *vc_parm);        //中途改变参数，可以调init
    void (*run)(void *ptr, short *indata, short *outdata, int len);    //len是多少个点数
    void (*init)(void *ptr, VOICECHANGER_PARM *vc_parm);        //中途改变参数，可以调init
} VOICECHANGER_FUNC_API;


typedef struct _VIBRATO_FUNC_API_ {
    u32(*need_buf)(void *vc_parm);
    u32(*tmp_buf_size)(void *vc_parm);
    int (*init)(void *ptr, void *vc_parm, void *tmpbuf);
    int (*set_tmpbuf)(void *ptr, void *tmpbuf);
    int (*run)(void *ptr, void *indata, void *outdata, int PointsPerChannel);
    int (*update)(void *ptr, void *vc_parm);
} VIBRATO_FUNC_API;

extern VOICECHANGER_FUNC_API *get_voiceChanger_func_api();
extern VOICECHANGER_FUNC_API *get_voiceChanger_adv_func_api();
extern AUTOTUNE_FUNC_API *get_autotune_func_api();

extern VIBRATO_FUNC_API *get_vibrato_func_api();
extern VIBRATO_FUNC_API *get_harmony_func_api();

#endif // reverb_api_h__

