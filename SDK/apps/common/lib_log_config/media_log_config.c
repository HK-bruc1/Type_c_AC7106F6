#include "app_config.h"

/*
 *******************************************************************
 *						Audio Log Debug Config
 *
 * @brief Log (Verbose/Info/Debug/Warn/Error)
 *******************************************************************
 */
const char log_tag_const_v_DRC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_DRC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_DRC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_DRC  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_DRC  = CONFIG_DEBUG_LIB(1);


const char log_tag_const_v_EQ  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_EQ  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_EQ  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_EQ  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_EQ  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_AUD_ADC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_AUD_ADC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_AUD_ADC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_AUD_ADC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_AUD_ADC  = CONFIG_DEBUG_LIB(0);

const char log_tag_const_v_AUD_DAC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_AUD_DAC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_AUD_DAC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_AUD_DAC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_AUD_DAC  = CONFIG_DEBUG_LIB(0);

const char log_tag_const_v_APP_DAC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_APP_DAC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_APP_DAC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_APP_DAC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_APP_DAC  = CONFIG_DEBUG_LIB(0);

const char log_tag_const_v_DAC_NG  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_DAC_NG  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_DAC_NG  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_DAC_NG  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_DAC_NG  = CONFIG_DEBUG_LIB(0);

const char log_tag_const_v_AUD_AUX  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_AUD_AUX  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_AUD_AUX  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_AUD_AUX  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_AUD_AUX  = CONFIG_DEBUG_LIB(0);

const char log_tag_const_v_MIXER  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_c_MIXER  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_MIXER  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_MIXER  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_MIXER  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_AUDIO_STREAM  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_c_AUDIO_STREAM  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_AUDIO_STREAM  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_AUDIO_STREAM  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_AUDIO_STREAM  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_AUDIO_DECODER  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_c_AUDIO_DECODER  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_AUDIO_DECODER  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_AUDIO_DECODER  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_AUDIO_DECODER  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_AUDIO_ENCODER  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_c_AUDIO_ENCODER  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_AUDIO_ENCODER  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_AUDIO_ENCODER  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_AUDIO_ENCODER  = CONFIG_DEBUG_LIB(1);


const char log_tag_const_v_SYNCTS  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_c_SYNCTS  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_SYNCTS  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_SYNCTS  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_SYNCTS  = CONFIG_DEBUG_LIB(1);


const char log_tag_const_v_EFFECTS  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_c_EFFECTS  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_EFFECTS  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_EFFECTS  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_EFFECTS  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_ANC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_c_ANC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_ANC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_ANC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_ANC  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_ANC_DB  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_c_ANC_DB  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_ANC_DB  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_ANC_DB  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_ANC_DB  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_ANC_ADAPTIVE  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_c_ANC_ADAPTIVE  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_ANC_ADAPTIVE  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_ANC_ADAPTIVE  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_ANC_ADAPTIVE  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_THR_DET  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_c_THR_DET  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_THR_DET  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_THR_DET  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_THR_DET  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_JLSTREAM = CONFIG_DEBUG_LIB(0);
const char log_tag_const_c_JLSTREAM = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_JLSTREAM = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_JLSTREAM = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_JLSTREAM = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_CVP = CONFIG_DEBUG_LIB(0);
const char log_tag_const_c_CVP = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_CVP = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_CVP = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_CVP = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_ANC_HOWLING = CONFIG_DEBUG_LIB(0);
const char log_tag_const_c_ANC_HOWLING = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_ANC_HOWLING = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_ANC_HOWLING = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_ANC_HOWLING = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_AUDIO_COMM = CONFIG_DEBUG_LIB(0);
const char log_tag_const_c_AUDIO_COMM = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_AUDIO_COMM = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_AUDIO_COMM = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_AUDIO_COMM = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_AUDIO_CORE = CONFIG_DEBUG_LIB(0);
const char log_tag_const_c_AUDIO_CORE = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_AUDIO_CORE = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_AUDIO_CORE = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_AUDIO_CORE = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_SBC_HW = CONFIG_DEBUG_LIB(0);
const char log_tag_const_c_SBC_HW = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_SBC_HW = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_SBC_HW = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_SBC_HW = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_SOC_SYNC = CONFIG_DEBUG_LIB(0);
const char log_tag_const_c_SOC_SYNC = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_SOC_SYNC = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_SOC_SYNC = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_SOC_SYNC = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_PITCH_SPEED = CONFIG_DEBUG_LIB(0);
const char log_tag_const_c_PITCH_SPEED = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_PITCH_SPEED = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_PITCH_SPEED = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_PITCH_SPEED = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_ANC_BTSPP = CONFIG_DEBUG_LIB(0);
const char log_tag_const_c_ANC_BTSPP = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_ANC_BTSPP = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_ANC_BTSPP = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_ANC_BTSPP = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_ANC_SZ_FFT = CONFIG_DEBUG_LIB(0);
const char log_tag_const_c_ANC_SZ_FFT = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_ANC_SZ_FFT = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_ANC_SZ_FFT = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_ANC_SZ_FFT = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_AEC_UART = CONFIG_DEBUG_LIB(0);
const char log_tag_const_c_AEC_UART = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_AEC_UART = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_AEC_UART = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_AEC_UART = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_AEC_REF = CONFIG_DEBUG_LIB(0);
const char log_tag_const_c_AEC_REF = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_AEC_REF = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_AEC_REF = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_AEC_REF = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_AEC_AGC = CONFIG_DEBUG_LIB(0);
const char log_tag_const_c_AEC_AGC = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_AEC_AGC = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_AEC_AGC = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_AEC_AGC = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_EQ_PRO = CONFIG_DEBUG_LIB(0);
const char log_tag_const_c_EQ_PRO = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_EQ_PRO = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_EQ_PRO = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_EQ_PRO = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_AUTO_MUTE = CONFIG_DEBUG_LIB(0);
const char log_tag_const_c_AUTO_MUTE = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_AUTO_MUTE = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_AUTO_MUTE = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_AUTO_MUTE = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_AUDIO_TIMER = CONFIG_DEBUG_LIB(0);
const char log_tag_const_c_AUDIO_TIMER = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_AUDIO_TIMER = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_AUDIO_TIMER = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_AUDIO_TIMER = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_MEDIA = CONFIG_DEBUG_LIB(0);
const char log_tag_const_c_MEDIA = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_MEDIA = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_MEDIA = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_MEDIA = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_MEDIA = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_ENCODER = CONFIG_DEBUG_LIB(0);
const char log_tag_const_c_ENCODER = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_ENCODER = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_ENCODER = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_ENCODER = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_ENCODER = CONFIG_DEBUG_LIB(1);

