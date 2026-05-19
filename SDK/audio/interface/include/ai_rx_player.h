

#ifndef _LE_AUDIO_PLAYER_H_
#define _LE_AUDIO_PLAYER_H_

#include "jlstream.h"

enum AI_SERVICE {
    AI_SERVICE_MEDIA,
    AI_SERVICE_CALL_DOWNSTREAM,
    AI_SERVICE_CALL_UPSTREAM,
    AI_SERVICE_VOICE
};

struct ai_rx_player_param {
    u8 type;
    u8 channel_mode;
    u16 frame_dms;		//帧长时间，单位 deci-ms (ms/10)
    u32 coding_type;
    u32 sample_rate;
    u32 bit_rate;
};


int ai_rx_player_open(u8 *bt_addr, u8 source, struct ai_rx_player_param *param);

void ai_rx_player_close(u8 source);

bool ai_rx_player_runing(u8 source);

#endif
