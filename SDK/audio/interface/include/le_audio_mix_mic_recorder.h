#ifndef LE_AUDIO_MIX_MIC_RECORDER_H
#define LE_AUDIO_MIX_MIC_RECORDER_H




#if LE_AUDIO_LOCAL_MIC_EN

void *local_mix_mic_le_audio_open(void *args);
int local_mic_tx_le_audio_close(void);
void set_local_mic_le_audio_status(u8 val);
u8 get_local_mic_le_audio_status(void);

int get_local_le_audio_status(void);

void local_le_audio_music_start_deal(void);
void local_le_audio_music_stop_deal(void);


void set_local_mix_mic_le_audio(void *le_audio);

void set_local_mic_le_audio_en(u8 en);
u8 get_local_mic_le_audio_en(void);

bool is_local_mix_mic_le_audio_runing(void);
bool is_local_le_audio_music_runing(void);
void *get_local_mix_mic_le_audio(void);

extern void broadcast_audio_cur_mode_tx_stream_open(void);
extern void broadcast_audio_cur_mode_tx_stream_close(void);
extern void alone_local_mic_tx_le_audio_close(void);
extern void alone_local_mic_tx_le_audio_open(void);

int get_micEff2LeAudio_switch_status(void);


// all close: 0x00
// mic open music open 0x11
// mic open music close 0x10
// mic close music open 0x01
enum {
    LOCAL_MIX_MIC_CLOSE_MUSIC_CLOSE = 0x00,	//0b00
    LOCAL_MIX_MIC_CLOSE_MUSIC_OPEN = 0x01,	//0b01
    LOCAL_MIX_MIC_OPEN_MUSIC_CLOSE = 0x02,	//0b10
    LOCAL_MIX_MIC_OPEN_MUSIC_OPEN = 0x03,	//0b11
};




#endif



#endif

