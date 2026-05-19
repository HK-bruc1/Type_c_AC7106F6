#ifndef _MSBC_CODEC_H_
#define _MSBC_CODEC_H_


#include "typedef.h"


typedef  struct   _ENC_DATA_INFO_ {
    u16 sr;            ///<sample rate
    u16 br;            ///<mp2的时候它是bitrate，但是adpcm的时候，它是blockSize,一般配成256/512/1024/2048，超过2048会被限制成2048
    u32 nch;
} ENC_DATA_INFO;

typedef struct if_encoder_io IF_ENCODER_IO;



typedef  struct _BT_ESCO_CODEC_E {
    u32(*need_buf)(void);
    u32(*init)(void *ptr);
    u32(*encode)(void *ptr, s16 *indata, u8 *outdata, s16 len);
    u32(*set_bitpool)(void *ptr, u8 bitpool);
} BT_ESCO_CODEC_E;


typedef  struct _BT_ESCO_CODEC_D {
    u32(*need_buf)(void);
    u32(*init)(void *ptr);
    u32(*decode)(void *ptr, u8 *indata, s16 *outdata, s16 len);
    u32(*set_bitpool)(void *ptr, u8 bitpool);
} BT_ESCO_CODEC_D;


extern BT_ESCO_CODEC_E *get_msbc_enc_ops();
extern BT_ESCO_CODEC_D *get_msbc_dec_ops();

//char *name;                                 ///<编码器名称
//u32 (*open)(void *work_buf, const struct if_encoder_io *encoder_io, u8 *set_enc_inf);  ///<打开编码器
//u32 (*run)(void *work_buf);					///<主循环
//u32 (*need_dcbuf_size)() ;		            ///<获取解码需要的buffer
//u32 (*dec_confing)(void *work_buf,u32 cmd,void*parm);

#endif









