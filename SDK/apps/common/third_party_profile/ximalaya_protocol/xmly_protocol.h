#ifndef _XMLY_PROTOCOL_H_
#define _XMLY_PROTOCOL_H_

#include "system/includes.h"

extern void ximalaya_protocol_init(void);
extern void ximalaya_protocol_exit(void);

// other api
extern int ai_mic_is_busy(void); //mic正在被使用
extern int ai_mic_rec_start(void); //启动mic和编码模块
extern int ai_mic_rec_close(void); //停止mic和编码模块
extern bool bt_is_sniff_close(void);
//语音识别接口
extern int mic_rec_pram_init(/* const char **name,  */u32 enc_type, u8 opus_type, u16(*speech_send)(u8 *buf, u16 len), u16 frame_num, u16 cbuf_size);

#endif

