#ifndef _SPEEX_ENCODE_API_H_
#define _SPEEX_ENCODE_API_H_

#include "audio_encode_common_api.h"
#include "audio_decode_common_api.h"

#ifndef u16
#define  u32  unsigned int
#define  u16  unsigned short
#define  s16  short
#define  u8  unsigned char
#endif

typedef struct _SPEEX_EN_FILE_IO_ {
    void *priv;
    u16(*input_data)(void *priv, s16 *buf, u16 len);          //short
    void (*output_data)(void *priv, u8 *buf, u16 len);       //bytes
} SPEEX_EN_FILE_IO;


typedef struct __SPEEX_ENC_OPS {
    u32(*need_buf)(u16 samplerate);
    u32(*open)(u8 *ptr, SPEEX_EN_FILE_IO *audioIO, u8 quality, u16 sample_rate, u8 complexity);
    u32(*run)(u8 *ptr);
} speex_enc_ops;


typedef struct __SPEEX_DEC_OPS {
    u32(*need_buf)(int bitwitdh);
    u32(*open)(u32 *ptr, int sr, int bitwitdh);
    u32(*run)(u32 *ptr, char *frame_data, short *frame_out);
    void (*set_maxframelen)(u8 *ptr, int maxframelen);
} speex_dec_ops;

typedef struct  _SR_CONTEXT_ {
    int sr;
} SR_CONTEXT;


extern speex_enc_ops *get_speex_enc_obj();         //����
extern speex_dec_ops *get_speex_dec_obj();         //����
extern audio_decoder_ops *get_speex_ops();         //解码

#endif



