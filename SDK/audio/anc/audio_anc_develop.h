#ifndef __AUDIO_ANC_DEVELOP_H_
#define __AUDIO_ANC_DEVELOP_H_

enum ANC_DEV_DATA_SEL {
    ANC_DEV_DATA_SEL_ADC = 0,
    ANC_DEV_DATA_SEL_ANC,
};

typedef struct {
    int16_t *fb_buf;
    int16_t *ff_buf;
    int16_t *ref_buf;
    int frame_count;
} aanc_process_data_t;

typedef void(* aanc_process_cb)(aanc_process_data_t *process_data);
void audio_anc_develop_process_data_cb_register(void *cb);

int audio_anc_develop_init(void);
void audio_anc_develop_anc_mode_set(u8 coeff_mode);
int audio_anc_develop_switch_on(void);
int audio_anc_develop_switch_off(void);

int audio_anc_develop_close(void);
int audio_anc_develop_open(enum ANC_DEV_DATA_SEL sel);

u8 audio_anc_develop_is_running(void);

#endif
