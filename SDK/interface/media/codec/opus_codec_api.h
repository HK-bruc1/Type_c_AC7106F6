#ifndef OPUS_CODEC_API_H
#define OPUS_CODEC_API_H

#include "audio_encode_common_api.h"
#include "audio_decode_common_api.h"


/******************* ENC *******************/

typedef struct _OPUS_EN_FILE_IO_ {
    void *priv;
    u16(*input_data)(void *priv, s16 *buf, u8 channel, u16 points);   //short   2字节一个点.
    u32(*output_data)(void *priv, u8 *buf, u16 len);  //bytes
} OPUS_EN_FILE_IO;


typedef struct _OPUS_ENC_PARA_ {
    int sr;   //samplerate: fixed_16000.
    int br;   //bitrate:    old:16000/32000/64000. --->new:16000~80000.16~80kbps.
    u16 nch;  //channels    fixed_1.  仅支持单声道编码.
    u16 format_mode;   //封装格式:   0:百度无头.      1:酷狗_eng+range.    2:ogg封装,pc软件可播放.  3:size+rangeFinal. 源码可兼容版本.
    u16 complexity;    //0.1.2.3.    3质量最好.速度要求最高.
    u16 frame_ms;      //20|40|60|80|100ms.
} OPUS_ENC_PARA;

typedef struct __OPUS_ENC_OPS {
    u32(*need_buf)(OPUS_ENC_PARA *para);
    u32(*open)(u8 *ptr, OPUS_EN_FILE_IO *audioIO, OPUS_ENC_PARA *para);
    u32(*run)(u8 *ptr);
} OPUS_ENC_OPS;

typedef struct __OPUS_STENC_OPS {
    u32(*need_buf)(OPUS_ENC_PARA *para);       //samplerate=16k   ignore
    u32(*open)(u8 *ptr, OPUS_EN_FILE_IO *audioIO, OPUS_ENC_PARA *para);
    u32(*run)(u8 *ptr);
    u32(*run_wb)(u8 *ptr, short *pcmL, short *pcmR, u8 *obuf, int *encLen);  //run_with_buffer  仅限封装模式0 or 1.
} OPUS_STENC_OPS;

extern OPUS_ENC_OPS *get_opus_enc_ops(void);
extern OPUS_ENC_OPS *get_opus_stenc_ops(void);


/******************** DEC **********************/

typedef struct __OPUSLIB_DEC_OPS {
    u32(*need_buf)(int  bitwidth);
    u32(*open)(u32 *ptr, int br_index, int  bitwidth);         //0,1,2
    u32(*run)(u32 *ptr, char *frame_data, short *frame_out);
} opuslib_dec_ops;

typedef struct  _BR_CONTEXT_ {
    int br_index;
} BR_CONTEXT;


// opus (silkm模式)解码设置
#define SET_DEC_SR           0x91

// ogg_opus 解码设置
#define	SET_OPUS_RAWDTF      0x90 //设置OPUS 为raw 数据. 带8字节packet头(4字节大端包长+4字节range校验值)
#define	SET_OPUS_CBR_PKTLEN  0x91 //设置OPUS 为raw 数据 + CBR_OPUS 包长,可能有多帧共用TOC. 返回0设置成功.

typedef struct _AUDIO_OPUS_PKTLEN {
    u32 opus_pkt_len;    //SET_OPUS_CBR_PKTLEN
} AUDIO_OPUS_PKTLEN;


extern opuslib_dec_ops *getopuslibdec_ops();



#define OPUS_SR_8000_OUT_POINTS			(160)
#define OPUS_SR_16000_OUT_POINTS		(320)


#endif


