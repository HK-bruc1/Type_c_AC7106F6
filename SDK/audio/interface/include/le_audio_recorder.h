/*************************************************************************************************/
/*!
*  \file      le_audio_recorder.h
*
*  \brief
*
*  Copyright (c) 2011-2023 ZhuHai Jieli Technology Co.,Ltd.
*
*/
/*************************************************************************************************/
#ifndef _LE_AUDIO_RECORDER_H_
#define _LE_AUDIO_RECORDER_H_


extern u16 get_auracast_frame_duration(void);

int le_audio_a2dp_recorder_open(u8 *btaddr, void *arg, void *le_audio);
void le_audio_a2dp_recorder_close(u8 *btaddr);
u8 *le_audio_a2dp_recorder_get_btaddr(void);

int le_audio_linein_recorder_open(void *params, void *le_audio, int latency);
void le_audio_linein_recorder_close(void);

int le_audio_spdif_recorder_open(void *params, void *le_audio, int latency);
void le_audio_spdif_recorder_close(void);
u8 is_le_audio_spdif_open(void);

int le_audio_iis_recorder_open(void *params, void *le_audio, int latency);
void le_audio_iis_recorder_close(void);
// int le_audio_muti_ch_iis_recorder_open(void *params, void *le_audio, int latency);
int le_audio_muti_ch_iis_recorder_open(void *params_ch0, void *params_ch1, void *le_audio, int latency);
void le_audio_muti_ch_iis_recorder_close(void);

int le_audio_mic_recorder_open(void *params, void *le_audio, int latency);
void le_audio_mic_recorder_close(void);
bool le_audio_mic_recorder_is_running(void);

int le_audio_fm_recorder_open(void *params, void *le_audio, int latency);
void le_audio_fm_recorder_close(void);

int le_audio_pc_recorder_open(void *params, void *le_audio, int latency);
void le_audio_pc_recorder_close(void);

int le_audio_wireless_mic_tx_set_dvol(u8 vol, s16 mute_en);
void le_audio_wireless_mic_tx_dvol_mute(bool mute);
void le_audio_wireless_mic_tx_dvol_up(void);
void le_audio_wireless_mic_tx_dvol_down(void);
int le_audio_wireless_mic_tx_monitor_set_dvol(u8 vol, s16 mute_en);
void le_audio_wireless_mic_tx_monitor_dvol_mute(bool mute);
void le_audio_wireless_mic_tx_monitor_dvol_up(void);
void le_audio_wireless_mic_tx_monitor_dvol_down(void);
#endif

