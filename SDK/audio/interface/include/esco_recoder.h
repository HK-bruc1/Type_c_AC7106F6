#ifndef ESCO_RECODER_H
#define ESCO_RECODER_H

#include "jlstream.h"

#define COMMON_SCO   	  0    //普通SCO
#define JL_DOGLE_ACL  	  1    //dongle   ACL链路


enum {
    ESCO_RECODER_EXT_TYPE_NONE,
    ESCO_RECODER_EXT_TYPE_JL_DONGLE_ACL,
    ESCO_RECODER_EXT_TYPE_AI
};

typedef enum {
    VOICE_CALL_TYPE_NONE,
    VOICE_CALL_TYPE_ESCO,			// esco通话
    VOICE_CALL_TYPE_LE_AUDIO,		// LE_Audio通话
} VOICE_CALL_TYPE;					// 通话类型

int esco_recoder_open(u8 link_type, void *bt_addr);

int esco_recoder_open_extended(void *bt_addr, int ext_type, void *ext_param);

void esco_recoder_close();

int esco_recoder_switch(u8 en);

int esco_recoder_reset(void);

int esco_recoder_running();

void esco_recoder_set_ai_tx_node_func(int (*func)(u8 *, u32));

int audio_sidetone_open(void);
int audio_sidetone_close(void);
int get_audio_sidetone_state();

struct jlstream *voice_call_stream_search(u16 *source_uuid, VOICE_CALL_TYPE type);


#endif
