/*************************************************************************************************/
/*!
*  \file      le_audio_player.h
*
*  \brief
*
*  Copyright (c) 2011-2023 ZhuHai Jieli Technology Co.,Ltd.
*
*/
/*************************************************************************************************/
#ifndef _LE_AUDIO_PLAYER_H_
#define _LE_AUDIO_PLAYER_H_

#include "jlstream.h"
#include "le_audio_stream.h"

/*! \brief 广播接收端音频结构体 */
struct le_audio_player_hdl {
    void *le_audio;
    void *rx_stream;
};

int le_audio_player_open(u8 *conn, struct le_audio_stream_params *lea_param);

void le_audio_player_close(u8 *btaddr);

bool le_audio_player_is_playing(void);

/**
 * @brief 获取leaudio播放器的情景
 *
 * @return STREAM_SCENE_LE_AUDIO:leaudio 播歌; STREAM_SCENE_LEA_CALL:leaudio 通话;
 * 				STREAM_SCENE_NONE: leaudio player没有开启
 */
enum stream_scene le_audio_player_get_stream_scene(void);

int le_audio_set_dvol(u8 le_audio_num, u8 vol);

s16 le_audio_get_dvol(u8 le_audio_num);

void le_audio_dvol_up(u8 le_audio_num);

void le_audio_dvol_down(u8 le_audio_num);

#if TCFG_VIR_UDISK_ENABLE
int virtual_udisk_get_enc_time(void);
#endif

#endif
