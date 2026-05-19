
#ifndef __sbc_codec_h
#define __sbc_codec_h

#include "typedef.h"
#include "string.h"

////////////////////////////
//软件sbc 解码使用
///////////////////////////

//解码库代码段
//#pragma bss_seg(".sbc_dec.data.bss")
//#pragma data_seg(".sbc_dec.data")
//#pragma const_seg(".sbc_dec.text.cache.L2.const")
//#pragma code_seg(".sbc_dec.text.cache.L2.run")

#define SET_SPECIAL_OCH   0x83       //设置非0声道模式输出通道数

#define SBC_DEC_OPUT_LR		0
#define SBC_DEC_OPUT_L		1
#define SBC_DEC_OPUT_R		2
#define SBC_DEC_OPUT_MIX	3/*(L+R) / 2*/

//run_errinfo
enum {
    NO_ERR,
    ERR_NO_BIT_STREAM,
    ERR_UPBUFF_LENLEZERO,
    ERR_UPBUFF_LENGTMAX,
    ERR_UPBUFF_PTRNULL,
    ERR_NEED_NEW_PACKET = 0x10000,
    ERR_READ_NO_DATA = 0x10002,
};

typedef struct _SBC_DEC_CH_OPUT_PARA {
    u32 mode;      //0:old_LRLRLR  1:LLLLLL  2:RRRRRR  3:(L*p0+R*p1)/16384
    short pL;      //new_3_chL       Q14  8192表示合成数据中对应通道音量为50%
    short pR;      //new_3_chR       Q14
    short pL_g;    //new_3_chL_goal  Q14
    short pR_g;    //new_3_chR_goal  Q14
    char pLadd;
    char pRadd;
    char new_mode;
} SBC_DEC_CH_OPUT_PARA;

////////////////////////////
//硬件sbc 解码使用
///////////////////////////
int sbc_audio_frame_num_get(u8 *data, u32 len);


#endif
