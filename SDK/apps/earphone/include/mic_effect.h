
#ifndef __AUDIO_MIC_EFFECT_H
#define __AUDIO_MIC_EFFECT_H


enum MIC_EFX_MODE {
    MIC_EFX_NORMAL, //mic混响音效
    MIC_EFX_DHA,    //mic辅听音效
};

/* 通用麦克风音效接口*/
bool mic_effect_player_runing();
int mic_effect_player_open();
void mic_effect_player_close();
void mic_effect_player_pause(u8 mark);
void mic_effect_set_dvol(u8 vol);
u8 mic_effect_get_dvol(void);
void mic_effect_dvol_up(void);
void mic_effect_dvol_down(void);
void mic_effect_set_irq_point_unit(u16 point_unit);

/*辅听接口*/
bool hearing_aid_player_runing();
int hearing_aid_player_open();
void hearing_aid_player_close();
void hearing_aid_switch();
void hearing_aid_set_dvol(u8 vol);
u8 hearing_aid_get_dvol(void);
void hearing_aid_dvol_up(void);
void hraring_aid_dvol_down(void);

#endif
