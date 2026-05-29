#ifndef LIB_MEDIA_CONFIG_H
#define LIB_MEDIA_CONFIG_H


extern const int CONFIG_MULTI_THREAD_SELF_ADAPTION_ENABLE;
extern const int config_media_24bit_enable;
extern const int CONFIG_SEAMLESS_RECORDER_ENABLE;
extern const int config_ch_adapter_32bit_enable;
extern const int config_mixer_32bit_enable;
extern const int config_out_dev_limiter_enable;
extern const int config_peak_rms_32bit_enable;
extern const int config_audio_vocal_track_synthesis_32bit_enable;
extern const int CONFIG_MAX_ENC_DATA_CACHE_SIZE;
extern const int CONFIG_LOG_OUTPUT_ENABLE;
extern const int config_audio_cfg_online_enable;
extern const int CONFIG_MEDIA_MEM_DEBUG;
extern const int config_media_tws_en;
extern const int config_audio_cvp_ref_ch_recognize_enable;
extern const int CONFIG_JLSTREAM_SCENE_DEBUG;
extern const int CONFIG_A2DP_AFATER_TONE_COEXIST_ENABLE;
/*
 *******************************************************************
 *						jlstream Configs
 *******************************************************************
 */
extern const int CONFIG_JLSTREAM_MULTI_THREAD_ENABLE;
extern const int config_jlstream_fade_32bit_enable;
extern const int config_jlstream_node_report_enable;
extern const int CONFIG_JLSTREAM_ASYNC_NODE_ENABLE;

/*
 *******************************************************************
 *						DAC Configs
 *******************************************************************
 */
extern const int config_audio_dac_output_channel;
extern const int config_audio_dac_output_mode;
extern const int config_audio_dac_channel_left_enable;
extern const int config_audio_dac_channel_right_enable;
extern const int config_audio_dac_power_on_mode;
extern const int config_audio_dac_power_off_lite;
extern const int config_audio_dac_mix_enable;
extern const int config_audio_dac_noisefloor_optimize_enable;
extern const char config_audio_dac_trim_enable;
extern const int config_audio_dac_mute_timeout;
extern const int config_audio_dac_pa_mode;
extern const int config_audio_dac_power_mode;
extern const int config_audio_dac_underrun_time_lea;
extern const int config_audio_dac_underrun_detect_enable;
extern const int config_audio_dac_ng_debug;
extern const int config_audio_dac_enable;
extern const int config_audio_dac_dma_buf_realloc_enable;
extern const int CONFIG_DAC_CACHE_MSEC;
extern const int config_audio_dac_delay_off_ms;
extern const int config_audio_dac_node_fade_out_en;
extern const int config_audio_dac_buffer_time_ms;
extern const int config_audio_dac_fade_out_time;
/*
 *******************************************************************
 *						ADC Configs
 *******************************************************************
 */
extern const int config_audio_adc0_enable;
extern const int config_audio_adc1_enable;
extern const int config_audio_adc2_enable;
extern const int config_audio_adc3_enable;
extern const int config_audio_adc4_enable;
extern const int config_audio_adc5_enable;
extern const int config_audio_adc6_enable;
extern const int config_audio_adc7_enable;
extern const int config_audio_adc0_input_mode;
extern const int config_audio_adc1_input_mode;
extern const int config_audio_adc2_input_mode;
extern const int config_audio_adc3_input_mode;
extern const int config_audio_adc4_input_mode;
extern const int config_audio_adc5_input_mode;
extern const int config_audio_adc6_input_mode;
extern const int config_audio_adc7_input_mode;
extern const u8 const_mic_capless_open_delay_debug;
extern const u8 const_mic_capless_trim_delay_debug;
extern const u8 const_adc_async_en;						//是否支持多个ADC异步打开

/*
 *******************************************************************
 *						EQ Configs
 *******************************************************************
 */
extern const int config_audio_eq_hp_enable;		//High Pass
extern const int config_audio_eq_lp_enable;		//Low Pass
extern const int config_audio_eq_bp_enable;		//Band Pass(Peaking)
extern const int config_audio_eq_hs_enable;		//High Shelf
extern const int config_audio_eq_ls_enable;		//Low Shelf
extern const int config_audio_eq_hs_q_enable;	//High Shelf Q
extern const int config_audio_eq_ls_q_enable;	//Low Shelf Q
extern const int config_audio_eq_hp_adv_enable;	//High Pass Advance
extern const int config_audio_eq_lp_adv_enable;	//Low Pass Advance
extern const int config_audio_eq_xfade_enable;
extern const float config_audio_eq_xfade_time;//0：一帧fade完成 非0：连续多帧fade，过度更加平滑，fade过程算力会相应增加(fade时间 范围(0~1)单位:秒)
/*
 *******************************************************************
 *						SRC Configs
 *******************************************************************
 */
extern const int SRC_HW_VERSION_SEL;//0:srv_v2 1:src_v3
extern const int CONST_SRC_COEF_46875_TO_16000_ENABLE;
extern const int CONST_SRC_COEF_16000_TO_8000_ENABLE;

/*
 *******************************************************************
 *						Audio sync Configs
 *******************************************************************
 */
extern const int config_audio_sync_features;    //音频同步功能配置
extern const u32 config_audio_device_sample_rate_table[][6];    //固定设备采样率设置
extern const int config_audio_sync_fixed_table_num;             //固定设备采样率预设的表内数量
/*
 *******************************************************************
 *						Effect Configs
 *******************************************************************
 */
extern const int config_audio_gain_enable;
extern const int config_audio_split_gain_enable;
extern const int config_audio_stereomix_enable;
extern const int voicechanger_effect_v_config;
extern const int audio_crossover_3band_enable;
extern const int limiter_run_mode;
extern const int drc_advance_run_mode;
extern const int drc_run_mode;
extern const int stereo_phaser_run_mode;
extern const int stereo_flanger_run_mode;
extern const int stereo_chorus_run_mode;
extern const int dynamic_eq_run_mode;
extern const int drc_detect_run_mode;
extern const int audio_effect_realloc_reserve_mem;
extern const int virtual_bass_pro_soft_crossover;
extern const int virtual_bass_eq_hard_select;
extern const int const_audio_howling_ahs_ref_src_type;
extern const int const_audio_howling_ahs_data_export;
extern const int const_audio_howling_ahs_iis_in_dac_out;
extern const int const_audio_howling_ahs_dual_core;
extern const int const_audio_howling_ahs_adc_hw_ref;
extern const int const_audio_howling_ahs_adc_hw_ref_ch;
extern const int const_audio_howling_ahs_adc_hw_ref_mic_gain;
extern const int const_audio_howling_ahs_adc_hw_ref_mic_pre_gain;
extern const int const_audio_howling_ahs_dac_read_points_offset;

extern const int virtualbass_noisegate_attack_time;
extern const int virtualbass_noisegate_release_time;
extern const int virtualbass_noisegate_hold_time;
extern const float virtualbass_noisegate_threshold;
extern const int spatial_imp_run_points;
extern const int CONFIG_SPATIAL_EFFECT_VERSION;
extern const int config_audio_limiter_xfade_enable;
extern const int config_audio_mblimiter_xfade_enable;
extern const int virtual_surround_hp_run_points;
extern const int config_out_dev_data_convert_enable;
extern const int auido_echo_delay_ms;
extern const int plate_reverb_combo_en;
extern const int echo_combo_en;
extern const int audio_bass_boost_combo_fade_time_ms;

/*
 *******************************************************************
 *						Audio Codec Configs
 *******************************************************************
 */
//codec格式使能配置
extern const int CONST_AUDIO_CODEC_G729_DEC_ENABLE;
extern const int CONST_AUDIO_CODEC_WTGV2_DEC_ENABLE;
extern const int CONST_AUDIO_CODEC_SINE_DEC_ENABLE;
extern const int CONST_AUDIO_CODEC_MTY_DEC_ENABLE;
extern const int CONST_AUDIO_CODEC_WAV_DEC_ENABLE;
extern const int CONST_AUDIO_CODEC_F2A_DEC_ENABLE;
extern const int CONST_AUDIO_CODEC_MSBC_DEC_ENABLE;
extern const int CONST_AUDIO_CODEC_SBC_DEC_ENABLE;
extern const int CONST_AUDIO_CODEC_AAC_DEC_ENABLE;
extern const int CONST_AUDIO_CODEC_MP3_DEC_ENABLE;

//通用配置
extern const int CONFIG_DEC_SUPPORT_CHANNELS;
extern const int CONFIG_DEC_SUPPORT_SAMPLERATE;

//id3 配置
extern const u8 config_flac_id3_enable;
extern const u8 config_ape_id3_enable;
extern const u8 config_m4a_id3_enable;
extern const u8 config_wav_id3_enable;
extern const u8 config_wma_id3_enable;

//jla_v2 编解码配置
extern const unsigned short JLA_V2_FRAMELEN_MASK;
extern const int JLA_V2_PLC_EN;
extern const int JLA_V2_PLC_FADE_OUT_START_POINT;
extern const int JLA_V2_PLC_FADE_OUT_POINTS;
extern const int JLA_V2_PLC_FADE_IN_POINTS;
extern const int JLA_V2_CODEC_WITH_FRAME_HEADER;
extern const int JLA_V2_CODEC_STKBUF_RELEASE_ENABLE;

//jla 编解码配置
extern const int JLA_CODEC_STKBUF_RELEASE_ENABLE;

//lc3 编解码配置
extern const int LC3_CODEC_STKBUF_RELEASE_ENABLE;

//bt_aac 解码配置
extern const int AAC_DEC_MP4A_LATM_ANALYSIS;
extern const int AAC_DEC_LIB_SUPPORT_24BIT_OUTPUT;
extern const int AAC_DEC_STKBUF_RELEASE_ENABLE;
extern const char config_bt_aac_dec_pcm24_enable;
extern const char config_bt_aac_dec_fifo_precision;

/*
 *******************************************************************
 *						Audio Debug Config
 *******************************************************************
 */
extern const int config_esco_media_stream_dump_enable;	//ESCO Media Stream Dump
extern const int config_a2dp_media_stream_dump_enable;	//A2dp Media Stream Dump
extern const int config_lea_media_stream_dump_enable;	//LE Audio Media Stream Dump


/*
 *******************************************************************
 *					local_tws
 *******************************************************************
 */

extern const int local_tws_source_play_use_enc_data;




/*
 *******************************************************************
 *						Audio LLNS_DNS Config
 *******************************************************************
 */
extern const u32 LLNS_DNS_SUPPORT_SAMPLE_RATE;
extern const u32 NN_TABLE_SELECT;
extern const u32 const_llns_dns_set_param_mode;
#endif
