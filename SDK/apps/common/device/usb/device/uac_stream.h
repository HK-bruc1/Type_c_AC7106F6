/*****************************************************************
>file name : usb_audio.h
>author : lichao
>create time : Wed 22 May 2019 10:39:35 AM CST
*****************************************************************/
#ifndef _UAC_STREAM_H_
#define _UAC_STREAM_H_
#include "typedef.h"

enum uac_event {
    USB_AUDIO_PLAY_OPEN = 0x0,
    USB_AUDIO_PLAY_CLOSE,
    USB_AUDIO_MIC_OPEN,
    USB_AUDIO_MIC_CLOSE,
    // USB_AUDIO_MUTE,
    USB_AUDIO_SET_PLAY_VOL,
    USB_AUDIO_SET_MIC_VOL,
};

struct uac_stream_runtime_info {
    u32 sample_rate;
    /* Changes when the negotiated format becomes valid, invalid, or changes. */
    u32 format_generation;
    u8 channel;
    u8 bit_width;
    /* UAC interface is requested/open after the existing debounce policy. */
    u8 interface_open;
    /* The corresponding PC audio stream is actually producing/consuming PCM. */
    u8 stream_active;
};

struct uac_audio_runtime_info {
    struct uac_stream_runtime_info speaker;
    struct uac_stream_runtime_info mic;
};


int uac_get_spk_vol();
u8 uac_audio_is_24bit_in_4byte();
void uac_audio_get_runtime_info(struct uac_audio_runtime_info *info);
void uac_speaker_stream_open(u32 samplerate, u32 ch, u32 bitwidth);
void uac_speaker_stream_close(int release);
void uac_speaker_stream_write(const u8 *obuf, u32 len);
void uac_speaker_stream_get_volume(u16 *l_vol, u16 *r_vol);
void set_uac_speaker_rx_handler(void *priv, void (*rx_handler)(int, void *, int));
u8 uac_speaker_stream_status(void);
u32 uac_mic_stream_open(u32 samplerate, u32 ch, u32 bitwidth);
void uac_mic_stream_close(int release);
int uac_mic_stream_read(u8 *buf, u32 len);
void uac_mic_stream_get_volume(u16 *vol);
void set_uac_mic_tx_handler(void *priv, int (*tx_handler)(void *, void *, int));
u8 uac_get_mic_stream_status(void);
void uac_mute_volume(u32 type, u32 l_vol, u32 r_vol);


#endif
