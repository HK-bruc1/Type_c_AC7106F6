/*************************************************************************************************/
/*!
*  \file      local_tws_player.h
*
*  \brief
*
*  Copyright (c) 2011-2023 ZhuHai Jieli Technology Co.,Ltd.
*
*/
/*************************************************************************************************/
#ifndef _LOCAL_TWS_PLAYER_H_
#define _LOCAL_TWS_PLAYER_H_

struct local_tws_player_param {
    u8 tws_channel;
    u8  channel_mode;    //声道类型
    u8 sample_rate;      //采样率序号
    u8 coding_type; 	 //编码格式
    u8 durations; 		 //帧长
    u16 bit_rate;		 //码率
};

struct local_tws_stream_params {
    u8 nch;
    u8 bit_width;
    s16 frame_dms;
    int sample_rate;
    int bit_rate;
    u32 coding_type;
};

int local_tws_player_open(struct local_tws_player_param *param);

void local_tws_player_close(void);

bool local_tws_player_is_playing(void);

#endif
