#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".audio_general.data.bss")
#pragma data_seg(".audio_general.data")
#pragma const_seg(".audio_general.text.const")
#pragma code_seg(".audio_general.text")
#endif

#include "system/init.h"
#include "media/audio_general.h"
#include "app_config.h"
#include "media/audio_def.h"
#include "jlstream.h"
#include "uartPcmSender.h"
#include "debug/audio_debug.h"
#include "audio_config_def.h"
#include "effects/voiceChanger_api.h"
#include "scene_update.h"
#include "cvp_v3.h"
#include "sync/audio_syncts.h"

/*音频配置在线调试配置*/
const int config_audio_cfg_debug_online = TCFG_CFG_TOOL_ENABLE;

#if TCFG_USER_TWS_ENABLE
const int config_media_tws_en = 1;
#else
const int config_media_tws_en = 0;
#endif

#ifndef LOCAL_TWS_SOURCE_PLAYER_USE_ENC_DATA  //source 端是否解编码数据
#define LOCAL_TWS_SOURCE_PLAYER_USE_ENC_DATA   0
#endif

const int local_tws_source_play_use_enc_data = LOCAL_TWS_SOURCE_PLAYER_USE_ENC_DATA;

/*蓝牙音频关联的使能配置，不含蓝牙的系统配置为0*/
#if TCFG_APP_BT_EN
const int config_bt_audio_enable = 1;
#else
const int config_bt_audio_enable = 0;
#endif
const int CONFIG_A2DP_AFATER_TONE_COEXIST_ENABLE = 0;

const int config_audio_dac_ng_debug = 0;
const int config_audio_dac_enable = TCFG_DAC_NODE_ENABLE;

/* 16bit数据流中也存在32bit位宽数据的处理 */
const int config_ch_adapter_32bit_enable = 1;
const int config_mixer_32bit_enable = 1;
const int config_jlstream_fade_32bit_enable = 1;
const int config_peak_rms_32bit_enable = 1;
const int config_audio_vocal_track_synthesis_32bit_enable = 1;

#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_MONO_L)
const int config_audio_dac_channel_left_enable = 1;
const int config_audio_dac_channel_right_enable = 0;
#elif (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_MONO_R)
const int config_audio_dac_channel_left_enable = 0;
const int config_audio_dac_channel_right_enable = 1;
#else
const int config_audio_dac_channel_left_enable = 1;
const int config_audio_dac_channel_right_enable = 1;
#endif
#ifdef TCFG_AUDIO_DAC_POWER_ON_MODE
const int config_audio_dac_power_on_mode = TCFG_AUDIO_DAC_POWER_ON_MODE;
#endif
#ifdef TCFG_AUDIO_DAC_LIGHT_CLOSE_ENABLE
const int config_audio_dac_power_off_lite = TCFG_AUDIO_DAC_LIGHT_CLOSE_ENABLE;
#else
const int config_audio_dac_power_off_lite = 0;
#endif
#if TCFG_CFG_TOOL_ENABLE
const int config_audio_cfg_online_enable = 1;
#else
const int config_audio_cfg_online_enable = 0;
#endif
const int config_audio_dac_node_fade_out_en = 1;//dac node 对末尾一帧数据做一次fade_out处理
const int config_audio_dac_dma_buf_realloc_enable = 0;
#ifdef TCFG_DAC_POWER_MODE
const int config_audio_dac_power_mode = TCFG_DAC_POWER_MODE;
#endif
const int config_audio_dac_underrun_time_lea = 100; //le_audio 音频流欠载检测时间(us); 低延时模式建议不要超过100us
const int config_audio_dac_buffer_time_ms = TCFG_AUDIO_DAC_BUFFER_TIME_MS;
const int config_audio_dac_fade_out_time = 10; //DAC suspend/stop数据淡出时间

const int config_audio_gain_enable = TCFG_GAIN_NODE_ENABLE;
const int config_audio_split_gain_enable = TCFG_SPLIT_GAIN_NODE_ENABLE;
const int config_audio_stereomix_enable = TCFG_STEROMIX_NODE_ENABLE;

//ADC Enable Config
#ifdef TCFG_ADC0_ENABLE
const int config_audio_adc0_enable = TCFG_ADC0_ENABLE;
#else
const int config_audio_adc0_enable = 0;
#endif
#ifdef TCFG_ADC1_ENABLE
const int config_audio_adc1_enable = TCFG_ADC1_ENABLE;
#else
const int config_audio_adc1_enable = 0;
#endif
#ifdef TCFG_ADC2_ENABLE
const int config_audio_adc2_enable = TCFG_ADC2_ENABLE;
#else
const int config_audio_adc2_enable = 0;
#endif
#ifdef TCFG_ADC3_ENABLE
const int config_audio_adc3_enable = TCFG_ADC3_ENABLE;
#else
const int config_audio_adc3_enable = 0;
#endif
#ifdef TCFG_ADC4_ENABLE
const int config_audio_adc4_enable = TCFG_ADC4_ENABLE;
#else
const int config_audio_adc4_enable = 0;
#endif
#ifdef TCFG_ADC5_ENABLE
const int config_audio_adc5_enable = TCFG_ADC5_ENABLE;
#else
const int config_audio_adc5_enable = 0;
#endif
#ifdef TCFG_ADC6_ENABLE
const int config_audio_adc6_enable = TCFG_ADC6_ENABLE;
#else
const int config_audio_adc6_enable = 0;
#endif
const int config_audio_adc7_enable = 0;

//ADC input Mode:Single-Ended/Differential/Single-Ended Capless
#if defined(ANC_MIC_REUSE_ENABLE) && ANC_MIC_REUSE_ENABLE
const int config_audio_adc0_input_mode = -1;
#else
#ifdef TCFG_ADC0_MODE
const int config_audio_adc0_input_mode = TCFG_ADC0_MODE;
#else
const int config_audio_adc0_input_mode = 0;
#endif
#endif

#if defined(ANC_MIC_REUSE_ENABLE) && ANC_MIC_REUSE_ENABLE
const int config_audio_adc1_input_mode = -1;
#else
#ifdef TCFG_ADC1_MODE
const int config_audio_adc1_input_mode = TCFG_ADC1_MODE;
#else
const int config_audio_adc1_input_mode = 0;
#endif
#endif

#if defined(ANC_MIC_REUSE_ENABLE) && ANC_MIC_REUSE_ENABLE
const int config_audio_adc2_input_mode = -1;
#else
#ifdef TCFG_ADC2_MODE
const int config_audio_adc2_input_mode = TCFG_ADC2_MODE;
#else
const int config_audio_adc2_input_mode = 0;
#endif
#endif

#if defined(ANC_MIC_REUSE_ENABLE) && ANC_MIC_REUSE_ENABLE
const int config_audio_adc3_input_mode = -1;
#else
#ifdef TCFG_ADC3_MODE
const int config_audio_adc3_input_mode = TCFG_ADC3_MODE;
#else
const int config_audio_adc3_input_mode = 0;
#endif
#endif

#if defined(ANC_MIC_REUSE_ENABLE) && ANC_MIC_REUSE_ENABLE
const int config_audio_adc4_input_mode = -1;
#else
#ifdef TCFG_ADC4_MODE
const int config_audio_adc4_input_mode = TCFG_ADC4_MODE;
#else
const int config_audio_adc4_input_mode = 0;
#endif
#endif

#if defined(ANC_MIC_REUSE_ENABLE) && ANC_MIC_REUSE_ENABLE
const int config_audio_adc5_input_mode = -1;
#else
#ifdef TCFG_ADC5_MODE
const int config_audio_adc5_input_mode = TCFG_ADC5_MODE;
#else
const int config_audio_adc5_input_mode = 0;
#endif
#endif

#if defined(ANC_MIC_REUSE_ENABLE) && ANC_MIC_REUSE_ENABLE
const int config_audio_adc6_input_mode = -1;
#else
#ifdef TCFG_ADC6_MODE
const int config_audio_adc6_input_mode = TCFG_ADC6_MODE;
#else
const int config_audio_adc6_input_mode = 0;
#endif
#endif

#if defined(ANC_MIC_REUSE_ENABLE) && ANC_MIC_REUSE_ENABLE
const int config_audio_adc7_input_mode = -1;
#else
#ifdef TCFG_ADC7_MODE
const int config_audio_adc7_input_mode = TCFG_ADC7_MODE;
#else
const int config_audio_adc7_input_mode = 0;
#endif
#endif


/*
 *******************************************************************
 *						Audio Debug Config
 *******************************************************************
 */
const int config_esco_media_stream_dump_enable = 0;	//ESCO Media Stream Dump
const int config_a2dp_media_stream_dump_enable = 0;	//A2dp Media Stream Dump
const int config_lea_media_stream_dump_enable = 0;	//LE Audio Media Stream Dump

/*
 *******************************************************************
 *						Audio CVP Config
 *******************************************************************
 */
const int config_audio_cvp_ref_source = 1;/*0:DAC Internal 1:External*/
#if ((defined(TCFG_AUDIO_SIDETONE_ENABLE) && TCFG_AUDIO_SIDETONE_ENABLE) || \
    (defined(TCFG_HOWLING_AHS_NODE_ENABLE) && TCFG_HOWLING_AHS_NODE_ENABLE))
const int config_audio_cvp_ref_ch_recognize_enable = 1;
#else
const int config_audio_cvp_ref_ch_recognize_enable = 0;
#endif
/*
 *******************************************************************
 *						Audio LLNS_DNS Config
 *******************************************************************
 */
#define LLNS_DNS_DELAY_2FRAME      1 //使用2帧延时模式，仅48k采样率有效

#ifdef TCFG_AUDIO_ADC_SAMPLE_RATE
//若支持固定ADC采样率，使用ADC采样率，否则使用全局采样率。亦可自行配置采样率，需注意仅支持32k、48k
#define LLNS_DNS_SR         TCFG_AUDIO_ADC_SAMPLE_RATE
#else
#define LLNS_DNS_SR         TCFG_AUDIO_GLOBAL_SAMPLE_RATE
#endif

#if (TCFG_LLNS_DNS_NODE_ENABLE && ((LLNS_DNS_SR != 32000) && (LLNS_DNS_SR != 48000)))
#error "llns_dns only support 32k or 48k"
#endif

/*
 *          |   32k   |        48k
 * ---------|---------|---------|-----------------
 * mode 0   |  BIT(0) | BIT(1)  | BIT(3) [2帧延时]
 * ---------|---------|---------|-----------------
 * mode 1   |  BIT(4) | BIT(5)  | BIT(6) [2帧延时]
 * ---------|---------|---------|-----------------
 * mode 2   |   N/A   |       BIT(7)
 */

#ifndef TCFG_LLNS_DNS_MODE
#define TCFG_LLNS_DNS_MODE         0 //由工具导出
#endif

#if (LLNS_DNS_SR == 32000)

#if (TCFG_LLNS_DNS_MODE == 0)
#define LLNS_TABLE_SELECT       NN_TABLE_LLNS_DNS_SR32K_MODE0
#elif (TCFG_LLNS_DNS_MODE == 1)
#define LLNS_TABLE_SELECT       NN_TABLE_LLNS_DNS_SR32K_MODE1
#elif (TCFG_LLNS_DNS_MODE == 2)
#error "llns dns not support sr 32k mode2"
#else
#define LLNS_TABLE_SELECT       0
#endif

#else

#if (TCFG_LLNS_DNS_MODE == 0)
#if LLNS_DNS_DELAY_2FRAME
#define LLNS_TABLE_SELECT       NN_TABLE_LLNS_DNS_SR48K_MODE0_2FRAME
#else
#define LLNS_TABLE_SELECT       NN_TABLE_LLNS_DNS_SR48K_MODE0
#endif
#elif (TCFG_LLNS_DNS_MODE == 1)
#if LLNS_DNS_DELAY_2FRAME
#define LLNS_TABLE_SELECT       NN_TABLE_LLNS_DNS_SR48K_MODE1_2FRAME
#else
#define LLNS_TABLE_SELECT       NN_TABLE_LLNS_DNS_SR48K_MODE1
#endif
#elif (TCFG_LLNS_DNS_MODE == 2)
#define LLNS_TABLE_SELECT       NN_TABLE_LLNS_DNS_SR48K_MODE2
#else
#define LLNS_TABLE_SELECT       0
#endif

#endif

const u32 LLNS_DNS_SUPPORT_SAMPLE_RATE = LLNS_DNS_SR;
const u32 const_llns_dns_set_param_mode = 0;

#define CVP_TABLE_SELECT   	TCFG_CVP_V3_NN_TABLE_TYPE

#if (CVP_TABLE_SELECT == NN_TABLE_CVP_1MIC_LARGE) && !(CVP_V3_1MIC_ALGO_ENABLE)
#error "NN_1MIC_LARGE requires CVP_V3_1MIC_ALGO_ENABLE enabled"
#endif

#if TCFG_AUDIO_CVP_V3_MODE
const u32 NN_TABLE_SELECT = (CVP_TABLE_SELECT | LLNS_TABLE_SELECT);
#else
const u32 NN_TABLE_SELECT = (LLNS_TABLE_SELECT);
#endif
/*
 *******************************************************************
 *						Audio Codec Config
 *******************************************************************
 */

////////////////////编解码格式使能/////////////////
const int CONST_AUDIO_CODEC_WTGV2_DEC_ENABLE 	= TCFG_DEC_WTS_ENABLE || TCFG_TONE_WTS_ENABLE;
const int CONST_AUDIO_CODEC_MTY_DEC_ENABLE		= TCFG_DEC_MTY_ENABLE || TCFG_TONE_MTY_ENABLE;
const int CONST_AUDIO_CODEC_WAV_DEC_ENABLE 		= TCFG_DEC_WAV_ENABLE || TCFG_TONE_WAV_ENABLE;
const int CONST_AUDIO_CODEC_F2A_DEC_ENABLE 		= TCFG_DEC_F2A_ENABLE || TCFG_TONE_F2A_ENABLE;
const int CONST_AUDIO_CODEC_MP3_DEC_ENABLE 		= TCFG_DEC_MP3_ENABLE || TCFG_TONE_MP3_ENABLE;

////////////////////ID3信息使能/////////////////
const u8 config_flac_id3_enable = 0;
const u8 config_ape_id3_enable  = 0;
const u8 config_m4a_id3_enable  = 0;
const u8 config_wav_id3_enable = 0;
const u8 config_wma_id3_enable = 0;

/////////////////////wma codec//////////////////
const int const_audio_codec_wma_dec_supoort_POS_play = 1; //是否支持指定位置播放

/////////////////////wav codec/////////////////
const int const_audio_codec_wav_dec_bitDepth_set_en = 0;

/////////////////////mp3 codec/////////////////
const int mp3encode_input_mode =  1 ;//0x01--short输入 0x02--float输入,使用mp3浮点数输入编码需要把config_mp3_enc_use_layer_3置1

/////////////////////mty codec/////////////////
const int config_mty_repeat_enable = 1; //mty 支持循环播放

/////////////////////aac codec/////////////////

//sdk流程使用 0: 堆栈buf 在初始化的时候申请，close 的时候释放，
//           1: 堆栈buf 在每一帧run的时候申请，run 结束后释放。
//解码和能量检测共用配置
const int AAC_DEC_STKBUF_RELEASE_ENABLE = 1;

/////////////////////JLVOC codec/////////////////
const int JLVOC_encode_printf_parameter = 0; //JLVOC 编码打印使能
const int JLVOC_decode_printf_parameter = 0; //JLVOC 解码打印使能

const int LC3_CODEC_STKBUF_RELEASE_ENABLE = 1;
const int JLA_CODEC_STKBUF_RELEASE_ENABLE = 1;
const int JLA_V2_CODEC_STKBUF_RELEASE_ENABLE = 1;

//***********************
//*		LDAC Codec       *
//***********************
#define   LDAC_ENC_MODE_CONFIG   0
//设为1和pc模型对比数据,可以过一致性测试.   Window或Linux编码工具均可.
//设为0 采用硬件fft加速,去掉非必要运算,速度更快.

#if (LDAC_ENC_MODE_CONFIG == 1)
const int LDAC_ENC_FRAME_STATUS = 1;
const int LDAC_ENC_HW_FFT = 0;
#else
const int LDAC_ENC_FRAME_STATUS = 0;
const int LDAC_ENC_HW_FFT = 1;
#endif

/////////////////////opus codec/////////////////
#if defined(TCFG_ENCODER_CHANNEL_NUM) && (TCFG_ENCODER_CHANNEL_NUM & BIT(1)) //双声道opus编码
const u8 OPUS_ENC_CELT_EN = 1; //是否使能opus celt 编码，仅支持10ms,20ms帧长，支持8k,16k,24k,48k采样率,支持单双声道编码
#else
const u8 OPUS_ENC_CELT_EN = 0; //是否使能opus celt 编码，仅支持10ms,20ms帧长，支持8k,16k,24k,48k采样率,支持单双声道编码
#endif
/*
 *******************************************************************
 *						Audio SYNCTS Config
 *******************************************************************
 */

const float FRAME_DURATION_THREAD = 1.5f;	//范围1.5f~2,采样率和时间戳抖动阈值倍数(丢帧检测阈值,时间戳间隔超过1.5帧，判定丢帧)

const int SRC_HW_VERSION_SEL = 0;//0:srv_v2 1:src_v3

/*SRC特定输入输出采样率对应的滤波器系数*/
const int CONST_SRC_COEF_46875_TO_16000_ENABLE = 0;
const int CONST_SRC_COEF_16000_TO_8000_ENABLE = 0;

/*
 *******************************************************************
 *						Audio Effects Config
 *******************************************************************
 */
const int config_out_dev_data_convert_enable = 0;     	//输出设备支持位宽转换处理
const int config_out_dev_limiter_enable = 0;			//输出级限幅使能
const float config_bandmerge_node_fade_step = 0.0f;		//淡入步进 0:默认不淡入 非0：淡入步进，范围：0.01f~10.0f，建议值0.1f,步进越大，更新越快
const int config_bandmerge_node_processing_method = 1;	//0：bandmerge 拿到所有iport的数据后，一次性叠加完成。 1：逐个叠加到目标地址，不做等待


/*控制 eq_design.c中的butterworth 函数 设计的系数是定点还是浮点 */
#if defined(EQ_CORE_V1)
const int butterworth_iir_filter_coeff_type_select = 0;//虚拟低音根据此变量使用相应的滤波器设计函数 0:float  1:int
#else
const int butterworth_iir_filter_coeff_type_select = 1;//虚拟低音根据此变量使用相应的滤波器设计函数 0:float  1:int
#endif

const int virtual_bass_pro_soft_crossover = 0;//控制虚拟低音pro 中的分频器是用软件运行或者硬件运行  1 软件EQ  0 硬件EQ 默认硬件EQ
const int virtual_bass_pro_soft_eq = 1;       //控制虚拟低音pro 中的EQ是用软件运行或者硬件运行 1软件 0硬件 默认1
const int virtual_bass_eq_hard_select = 0; //虚拟低音使用的eq类型 0:使用软件eq  1:使用硬件eq

const int config_audio_eq_xfade_enable = 1;
const float config_audio_eq_xfade_time = 0;//0.4f;//0：一帧fade完成 非0：连续多帧fade，过度更加平滑，fade过程算力会相应增加(fade时间 范围(0~1)单位:秒)


const int limiter_run_mode = EFx_PRECISION_PRO
#if defined(TCFG_AUDIO_EFX_E9D0_RUN_MODE)
                             | TCFG_AUDIO_EFX_E9D0_RUN_MODE
#endif
#if defined(TCFG_AUDIO_EFX_4E5B_RUN_MODE)
                             | TCFG_AUDIO_EFX_4E5B_RUN_MODE
#endif
#if defined(TCFG_AUDIO_EFX_F58A_RUN_MODE)
                             | ((TCFG_AUDIO_EFX_F58A_RUN_MODE & (EFx_BW_32t16 | EFx_BW_32t32)) ? EFx_BW_32t32 : 0)
                             | ((TCFG_AUDIO_EFX_F58A_RUN_MODE &EFx_BW_16t16) ? EFx_BW_16t16 : 0)
#endif
#if !defined(TCFG_AUDIO_EFX_4E5B_RUN_MODE) && !defined(TCFG_AUDIO_EFX_F58A_RUN_MODE)
                             | 0xFFFF
#endif
                             ;

#ifdef TCFG_AUDIO_EFX_6195_RUN_MODE
const  int frequency_shift_run_mode      = TCFG_AUDIO_EFX_6195_RUN_MODE;
#else
const  int frequency_shift_run_mode      = EFx_BW_16t16 | EFx_BW_32t32;//只有 16进16出， 或者 32进32出
#endif

#ifdef TCFG_AUDIO_EFX_A48F_RUN_MODE
const  int plate_reverb_lite_run_mode    = TCFG_AUDIO_EFX_A48F_RUN_MODE;
#else
const  int plate_reverb_lite_run_mode    = EFx_BW_16t16 | EFx_BW_32t32;//只有 16进16出， 或者 32进32出
#endif

const  int echo_run_mode                 = 0
#if defined(TCFG_AUDIO_EFX_C8E6_RUN_MODE) && (TCFG_AUDIO_EFX_C8E6_RUN_MODE & (EFx_BW_16t16 | EFx_BW_16t32))
        | EFx_BW_16t16
#endif
#if defined(TCFG_AUDIO_EFX_C8E6_RUN_MODE) && (TCFG_AUDIO_EFX_C8E6_RUN_MODE & (EFx_BW_32t32))
        | EFx_BW_32t32
#endif
#ifdef TCFG_AUDIO_EFX_98A4_RUN_MODE
        | TCFG_AUDIO_EFX_98A4_RUN_MODE
#endif
        ;


const  int voicechanger_run_mode         = 0
#if defined(TCFG_AUDIO_EFX_7293_RUN_MODE)
        | TCFG_AUDIO_EFX_7293_RUN_MODE
#endif
#if defined(TCFG_AUDIO_EFX_AE43_RUN_MODE)//harmony
        | TCFG_AUDIO_EFX_AE43_RUN_MODE
#endif
#if !defined(TCFG_AUDIO_EFX_7293_RUN_MODE) && !defined(TCFG_AUDIO_EFX_AE43_RUN_MODE)
        | EFx_BW_16t16 | EFx_BW_32t32//变声位宽控制
#endif
        ;
#if TCFG_HARMONY_NODE_ENABLE
const int pitchshift_have_modeHarmony = 1;//1:harmony节点spectrum的模式支持度的配置，不过会增加它做为变声模式的一些运算, 0:反之
#else
const int pitchshift_have_modeHarmony = 0;
#endif


#ifdef TCFG_AUDIO_EFX_40DC_RUN_MODE
const int  vibrato_run_mode = TCFG_AUDIO_EFX_40DC_RUN_MODE;
#else
const int  vibrato_run_mode = EFx_BW_32t32 | EFx_BW_16t16;
#endif

const  int autotune_run_mode             = 0
#if defined(TCFG_AUDIO_EFX_C07A_RUN_MODE)
        | TCFG_AUDIO_EFX_C07A_RUN_MODE
#endif
#if defined(TCFG_AUDIO_EFX_AE43_RUN_MODE)//harmony
        | TCFG_AUDIO_EFX_AE43_RUN_MODE
#endif
#if !defined(TCFG_AUDIO_EFX_7293_RUN_MODE) && !defined(TCFG_AUDIO_EFX_AE43_RUN_MODE)
        | EFx_BW_16t16 | EFx_BW_32t32//autoTune位宽控制
#endif
        ;

#if TCFG_AUDIO_EFX_FB27_RUN_MODE
const int treomolo_run_mode = TCFG_AUDIO_EFX_FB27_RUN_MODE;
#else
const int treomolo_run_mode =  EFx_BW_16t16 | EFx_BW_32t32;
#endif



#ifdef TCFG_AUDIO_EFX_24AB_RUN_MODE
const  int reverb_run_mode               = TCFG_AUDIO_EFX_24AB_RUN_MODE;
#else
const  int reverb_run_mode               = EFx_BW_16t16 | EFx_BW_32t32;//lib_Reverb.a
#endif

const  int plate_reverb_run_mode         = 0
#if defined(TCFG_AUDIO_EFX_C8E6_RUN_MODE) && (TCFG_AUDIO_EFX_C8E6_RUN_MODE & (EFx_BW_16t16 | EFx_BW_16t32))
        | EFx_BW_16t16
#endif
#if defined(TCFG_AUDIO_EFX_C8E6_RUN_MODE) && (TCFG_AUDIO_EFX_C8E6_RUN_MODE & (EFx_BW_32t32))
        | EFx_BW_32t32
#endif
#ifdef TCFG_AUDIO_EFX_5101_RUN_MODE
        | TCFG_AUDIO_EFX_5101_RUN_MODE;
#endif
;

#ifdef TCFG_PLATE_REVERB_COMBO_ENABLE
const int plate_reverb_combo_en = TCFG_PLATE_REVERB_COMBO_ENABLE;
#else
const int plate_reverb_combo_en = 0;
#endif
#ifdef TCFG_ECHO_COMBO_ENABLE
const int echo_combo_en = TCFG_ECHO_COMBO_ENABLE;
#else
const int echo_combo_en = 0;
#endif


#ifdef TCFG_AUDIO_EFX_0753_RUN_MODE
const  int plate_reverb_adv_run_mode     = TCFG_AUDIO_EFX_0753_RUN_MODE;
#else
const  int plate_reverb_adv_run_mode     = EFx_BW_16t16 | EFx_BW_32t32;//lib_plateReverb_adv.a
#endif

#ifdef TCFG_AUDIO_EFX_E955_RUN_MODE
const  int noisegate_pro_run_mode        = TCFG_AUDIO_EFX_E955_RUN_MODE;
#else
const  int noisegate_pro_run_mode        = EFx_BW_16t16 | EFx_BW_32t32;
#endif

const  int noisegate_run_mode            = 0
#if defined(TCFG_AUDIO_EFX_B7C4_RUN_MODE)
        | TCFG_AUDIO_EFX_B7C4_RUN_MODE
#endif
#if defined(TCFG_AUDIO_EFX_B0D5_RUN_MODE)//virtual bass
        | ((TCFG_AUDIO_EFX_B0D5_RUN_MODE & (EFx_BW_16t32 |  EFx_BW_32t32)) ? EFx_BW_32t32 : 0)
        | ((TCFG_AUDIO_EFX_B0D5_RUN_MODE &EFx_BW_16t16) ? EFx_BW_16t16 : 0)
#endif
#if !defined(TCFG_AUDIO_EFX_B7C4_RUN_MODE) && !defined(TCFG_AUDIO_EFX_B0D5_RUN_MODE)
        | EFx_BW_16t16 | EFx_BW_32t32
#endif
        ;

#ifdef TCFG_AUDIO_EFX_B0D5_RUN_MODE
const  int virtual_bass_run_mode         = TCFG_AUDIO_EFX_B0D5_RUN_MODE;
#else
const  int virtual_bass_run_mode         = EFx_BW_16t16 | EFx_BW_16t32 | EFx_BW_32t32;
#endif

const  int virtual_bass_classic_run_mode = 0
#if defined(TCFG_AUDIO_EFX_E9D0_RUN_MODE)
        | TCFG_AUDIO_EFX_E9D0_RUN_MODE
#endif
#if defined(TCFG_AUDIO_EFX_55C9_RUN_MODE)
        | TCFG_AUDIO_EFX_55C9_RUN_MODE
#endif
#if defined(TCFG_AUDIO_EFX_02E6_RUN_MODE)
        | TCFG_AUDIO_EFX_02E6_RUN_MODE
#endif
#if defined(TCFG_AUDIO_EFX_55C9_RUN_MODE) || defined(TCFG_AUDIO_EFX_02E6_RUN_MODE)
        | EFx_BW_16t16 | EFx_BW_32t32
#endif
        ;

const  int drc_advance_run_mode          = EFx_PRECISION_NOR
#if defined(TCFG_AUDIO_EFX_E9D0_RUN_MODE)
        | TCFG_AUDIO_EFX_E9D0_RUN_MODE
#endif

#if defined(TCFG_AUDIO_EFX_4250_RUN_MODE)
        | TCFG_AUDIO_EFX_4250_RUN_MODE
#endif
#if defined(TCFG_AUDIO_EFX_74CB_RUN_MODE)
        | ((TCFG_AUDIO_EFX_74CB_RUN_MODE & (EFx_BW_32t16 | EFx_BW_32t32)) ? EFx_BW_32t32 : 0)
        | ((TCFG_AUDIO_EFX_74CB_RUN_MODE &EFx_BW_16t16) ? EFx_BW_16t16 : 0)
#endif
#if defined(TCFG_AUDIO_EFX_02E6_RUN_MODE)
        | TCFG_AUDIO_EFX_02E6_RUN_MODE
#endif
#if !defined(TCFG_AUDIO_EFX_4250_RUN_MODE) && !defined(TCFG_AUDIO_EFX_74CB_RUN_MODE) && !defined(TCFG_AUDIO_EFX_02E6_RUN_MODE)
        | EFx_BW_16t16 | EFx_BW_32t16 | EFx_BW_32t32
#endif
        ;


#ifdef TCFG_AUDIO_EFX_9A58_RUN_MODE
const  int drc_detect_run_mode           = TCFG_AUDIO_EFX_9A58_RUN_MODE | EFx_PRECISION_NOR;
#else
const  int drc_detect_run_mode           = EFx_BW_16t16 | EFx_BW_32t16 | EFx_PRECISION_NOR | EFx_BW_32t32;
#endif

#ifdef TCFG_AUDIO_EFX_DEFE_RUN_MODE
const  int drc_run_mode                  = TCFG_AUDIO_EFX_DEFE_RUN_MODE | EFx_PRECISION_NOR;
#else
const  int drc_run_mode                  = EFx_BW_16t16 | EFx_BW_32t16 | EFx_PRECISION_NOR | EFx_BW_32t32;
#endif


#ifdef TCFG_AUDIO_EFX_540E_RUN_MODE
const int pitch_speed_run_mode       = TCFG_AUDIO_EFX_540E_RUN_MODE;
#else
const int pitch_speed_run_mode       = EFx_BW_32t32 | EFx_BW_16t16;
#endif
const int resample_fast_cal_run_mode = EFx_BW_16t16 | EFx_BW_32t32;

#ifdef TCFG_AUDIO_EFX_A8F4_RUN_MODE
const int pcm_delay_run_mode         = TCFG_AUDIO_EFX_A8F4_RUN_MODE;
#else
const int pcm_delay_run_mode         = EFx_BW_16t16 | EFx_BW_32t32;
#endif
#ifdef TCFG_AUDIO_EFX_1B2A_RUN_MODE
const int harmonic_exciter_run_mode  = TCFG_AUDIO_EFX_1B2A_RUN_MODE;
#else
const int harmonic_exciter_run_mode  = EFx_BW_16t16 | EFx_BW_32t32;
#endif
#ifdef TCFG_AUDIO_EFX_ED7F_RUN_MODE
const int lfaudio_plc_run_mode       = TCFG_AUDIO_EFX_ED7F_RUN_MODE;
#else
const int lfaudio_plc_run_mode       = EFx_BW_16t16 | EFx_BW_32t32;
#endif

#ifdef TCFG_AUDIO_EFX_BC44_RUN_MODE
const int  stereo_flanger_run_mode = TCFG_AUDIO_EFX_BC44_RUN_MODE | EFx_MODULE_MONO_EN | EFx_MODULE_STEREO_EN;
#else
const int  stereo_flanger_run_mode = EFx_BW_32t32 | EFx_BW_16t16 | EFx_MODULE_MONO_EN | EFx_MODULE_STEREO_EN;
#endif

#ifdef TCFG_AUDIO_EFX_7C2B_RUN_MODE
const int  stereo_chorus_run_mode  = TCFG_AUDIO_EFX_7C2B_RUN_MODE | EFx_MODULE_MONO_EN | EFx_MODULE_STEREO_EN;
#else
const int  stereo_chorus_run_mode  = EFx_BW_32t32 | EFx_BW_16t16 | EFx_MODULE_MONO_EN | EFx_MODULE_STEREO_EN;
#endif

#ifdef TCFG_AUDIO_EFX_D2E8_RUN_MODE
const int  stereo_phaser_run_mode  = TCFG_AUDIO_EFX_D2E8_RUN_MODE | EFx_MODULE_MONO_EN | EFx_MODULE_STEREO_EN;
#else
const int  stereo_phaser_run_mode  = EFx_BW_32t32 | EFx_BW_16t16 | EFx_MODULE_MONO_EN | EFx_MODULE_STEREO_EN;
#endif

#ifdef TCFG_AUDIO_EFX_AB66_RUN_MODE
const int  pingpong_echo_run_mode  = TCFG_AUDIO_EFX_AB66_RUN_MODE;
#else
const int  pingpong_echo_run_mode  = EFx_BW_32t32 | EFx_BW_16t16;
#endif

#ifdef TCFG_AUDIO_EFX_97AA_RUN_MODE
const int distortion_run_mode = TCFG_AUDIO_EFX_97AA_RUN_MODE;
#else
const int distortion_run_mode = EFx_BW_16t16 | EFx_BW_16t32 | EFx_BW_32t16 | EFx_BW_32t32;
#endif

const int dynamic_eq_run_mode = EFx_BW_32t32 | EFx_PRECISION_NOR; //只支持32进32出 不会优化代码预留

const int dynamic_eq_pro_run_mode = EFx_BW_32t32;//只支持32进32出 不会优化代码预留

const int iir_filter_run_mode = 0  //不支持32进16出
#if defined(TCFG_AUDIO_EFX_3845_RUN_MODE)
                                | TCFG_AUDIO_EFX_3845_RUN_MODE
#endif
#if defined(TCFG_AUDIO_EFX_6700_RUN_MODE)
                                | TCFG_AUDIO_EFX_6700_RUN_MODE
#endif
#if defined(TCFG_AUDIO_EFX_02E6_RUN_MODE)
                                | TCFG_AUDIO_EFX_02E6_RUN_MODE
#endif
#if defined(TCFG_AUDIO_EFX_A64E_RUN_MODE)
                                | EFx_BW_16t32 | EFx_BW_32t32
#endif
#if !defined(TCFG_AUDIO_EFX_3845_RUN_MODE) && !defined(TCFG_AUDIO_EFX_6700_RUN_MODE) && !defined(TCFG_AUDIO_EFX_02E6_RUN_MODE) && !defined(TCFG_AUDIO_EFX_A64E_RUN_MODE)
                                | EFx_BW_16t16 | EFx_BW_16t32 | EFx_BW_32t32  //不支持32进16出
#endif
                                ;

#ifdef TCFG_AUDIO_EFX_BFE4_RUN_MODE
const int frequency_compressor_run_mode  = TCFG_AUDIO_EFX_BFE4_RUN_MODE; //只支持16进16出与32进32出
#else
const int frequency_compressor_run_mode  = EFx_BW_16t32 | EFx_BW_32t32;
#endif

#ifdef TCFG_AUDIO_EFX_A64E_RUN_MODE
const int spatial_adv_run_mode       = TCFG_AUDIO_EFX_A64E_RUN_MODE;
#else
const int spatial_adv_run_mode       = EFx_BW_16t32 | EFx_BW_32t32;
#endif

/* 空间音频运算最大帧长：配置为1--64最大帧长；配置为2--128最大帧长 */
const int spatial_adv_framesize_mode = 2;

/*变声模式使能*/
const int voicechanger_effect_v_config = (0
        | BIT(EFFECT_VOICECHANGE_PITCHSHIFT)
        /* | BIT(EFFECT_VOICECHANGE_CARTOON) */
        /* | BIT(EFFECT_VOICECHANGE_SPECTRUM) */
        /* | BIT(EFFECT_VOICECHANGE_ROBORT) */
        /* | BIT(EFFECT_VOICECHANGE_MELODY) */
        /* | BIT(EFFECT_VOICECHANGE_WHISPER) */
        /* | BIT(EFFECT_VOICECHANGE_F0_DOMAIN) */
        /* | BIT(EFFECT_VOICECHANGE_F0_TD) */
        /* | BIT(EFFECT_VOICECHANGE_FEEDBACK) */
                                         );

/*mb drc/limiter 3带使能(1.2k) */
const int audio_crossover_3band_enable       = 1;
/*
 * 某些算法参数更新需要重新申请buffer，
 * 为防止重新申请时内存不足导致异常，
 * 此处设置需要保留的内存大小，
 * 当判断若重新申请buf后，系统内存小于该值则不重新申请，
 * 打印"xxx buf realloc fail"(需要打开log_tag_const_d_EFFECTS)
 * 可视化工具界面显示"流程打开buf失败"
 */
const int audio_effect_realloc_reserve_mem = (13 * 1024);
const int config_audio_limiter_xfade_enable = 0;
const int config_audio_mblimiter_xfade_enable = 0;

/*Vocal Remover Configs*/
const int audio_vocal_remover_low_cut_enable = 1;
const int audio_vocal_remover_high_cut_enable = 1;
const int audio_vocal_remover_preset_mode = 0; //预设参数模式[0/1]，0：预设关，使用工具节点配置 1：使用预设模式1

/* 空间音效V300 */
#ifdef TCFG_AUDIO_EFX_83E1_RUN_MODE
const int spatial_imp_run_mode = TCFG_AUDIO_EFX_83E1_RUN_MODE;
#else
const int spatial_imp_run_mode = EFx_BW_16t16 | EFx_BW_32t32;
#endif
const int spatial_imp_fft_mode = 2;     //1软件fft(浮点输入输出) 2硬件fft(定点输入输出)
const int spatial_imp_run_points = 128; //运算点数

const int spatial_imp_active_azi_group = -1;
const int spatial_imp_active_ele_group = 0;

/* 双耳压强差使能控制，关闭可节省代码量（关闭后可视化界面参数"ildenable"调试无效）。范围：0关，1开。*/
const int spatial_imp_active_ild_group = 0;

//howling_ahs配置
const int const_audio_howling_ahs_ref_src_type = 1; //0:不使能参考数据变采样，1:软件src，2:硬件src
const int const_audio_howling_ahs_data_export = 0;  //数据写卡导出，需要使能AUDIO_DATA_EXPORT_VIA_UART，串口写卡工具配置3通道，每通道256byte

/*
 * ahs算法所处的数据流输入为iis，输出为dac时需要使能。
 */
const int const_audio_howling_ahs_iis_in_dac_out = 0;

/*
 * ADC硬件回采使能，硬件上需要预留ADC回采功放后级输出通路
 * 使能后，需要在"音频配置->ADC配置"中正确配置对应的ADC
 */
const int const_audio_howling_ahs_adc_hw_ref = 0;
const int const_audio_howling_ahs_adc_hw_ref_ch = BIT(0); //硬回采使用的ADC通道
const int const_audio_howling_ahs_adc_hw_ref_mic_gain = 10;//单位：档位，可参考adc节点配置
const int const_audio_howling_ahs_adc_hw_ref_mic_pre_gain = 1;

/* 软件回采dac_read_offset点数设置 */
const int const_audio_howling_ahs_dac_read_points_offset = 0;

/*
 * ahs-nn 算法是否使用双核
 * 1:双核-啸叫抑制效果更优，延时略大，资源消耗多。流程需要串AHS-NN + AHS-NN-POST。
 * 0:单核-延时低，资源消耗少。流程仅需要串AHS-NN。
 */
const int const_audio_howling_ahs_dual_core = 1;

const int audio_bass_boost_combo_fade_time_ms = 400;// 0:xfade, 非0：参数更新fade_out->udpate_param->fade_in, 范围 50~1000, 单位:ms



// Virtual Surround Headphone 2t4
#ifdef TCFG_AUDIO_EFX_DCD4_RUN_MODE
const int spatial_brir_run_mode = TCFG_AUDIO_EFX_DCD4_RUN_MODE;
const int StereoToLCR_run_mode = TCFG_AUDIO_EFX_DCD4_RUN_MODE;
#else
const int spatial_brir_run_mode = EFx_BW_16t16 | EFx_BW_32t32;
const int StereoToLCR_run_mode = EFx_BW_16t16 | EFx_BW_32t32;
#endif
const int virtual_surround_hp_run_points = 512; //运算点数
const int StereoToLCR_24bit_sat = 1;             /* 跑24bit数据流是否对数据进行饱和处理 */
const int spatial_brir_azimuth = -1;    /* -1加载所有brir，角度调试模式下使用 */

const int howling_freqshift_lowdelay = 0;//0:fir分支，跟以前效果一致, 1:使用iir分支来节省延时

/*
 *******************************************************************
 *						Audio Mic Capless Config
 *******************************************************************
 */
/*
 *          用于debug需要配置多长的mic_capless_delay
 * 部分mic 需要更多延时才能达到稳定的电压值
 * debug步骤：
 * 1、打开 const_mic_capless_open_delay_debug，可打印打开adc后，mic_bias需要多长时间才能达到/接近稳定的电压值
 * 2、将该延时配置到 open_delay_ms 中
 * 3、关闭 const_mic_capless_open_delay_debug

 * 4、打开 const_mic_capless_trim_delay_debug，可打印第一次trim后，mic_bias需要多长时间才能达到/接近稳定的电压值
 * 5、将该延时配置到 trim_delay_ms 中
 * 6、关闭 const_mic_capless_trim_delay_debug
 */
const u8 const_mic_capless_open_delay_debug = 0;
const u8 const_mic_capless_trim_delay_debug = 0;

const char log_tag_const_v_ALINK  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_c_ALINK  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_ALINK  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_ALINK  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_ALINK  = CONFIG_DEBUG_LIB(TRUE);

/*vbass noisegate 参数配置*/
const int virtualbass_noisegate_attack_time = 50;
const int virtualbass_noisegate_release_time = 30;
const int virtualbass_noisegate_hold_time = 15;
const float virtualbass_noisegate_threshold = -85.0f;

/*
 *******************************************************************
 *			Audio hardware DMA malloc config
 *******************************************************************
 */
const int config_src_hw_dma_malloc_enable = 1;

/*
 *******************************************************************
 *			Audio sync  config
 *******************************************************************
 */
/*
 * 固定采样率且bypass同步的配置使能 - CONFIG_AUDIO_SYNC_FIXED_SAMPLE_RATE_ENABLE
 *
 * 缓慢对齐控制使能 - CONFIG_AUDIO_SYNC_SLOW_CONTROL_ENABLE
 *
 * 多配置选项使用或运算进行配置
 * */
const int config_audio_sync_features = CONFIG_AUDIO_SYNC_SLOW_CONTROL_BY_MODE;//CONFIG_AUDIO_SYNC_SLOW_CONTROL_ENABLE;

/*
 *格式：{输入设备UUID, 输出设备UUID，输入设备采样率，输出设备采样率，输入预设采样率，输出预设采样率}
 */
const u32 config_audio_device_sample_rate_table[][6] = {
    {NODE_UUID_LINEIN, NODE_UUID_IIS0_TX, 48000, 48000, 625, 624},
    {NODE_UUID_LINEIN, NODE_UUID_IIS1_TX, 48000, 48000, 625, 624},
};

const int config_audio_sync_fixed_table_num = ARRAY_SIZE(config_audio_device_sample_rate_table);


__attribute__((weak))
int get_system_stream_bit_width(void *par)
{
    return 0;
}
__attribute__((weak))
int get_media_stream_bit_width(void *par)
{
    return 0;
}
__attribute__((weak))
int get_esco_stream_bit_width(void *par)
{
    return 0;
}
__attribute__((weak))
int get_mic_eff_stream_bit_width(void *par)
{
    return 0;
}
__attribute__((weak))
int get_usb_audio_stream_bit_width(void *par)
{
    return 0;
}


static struct audio_general_params audio_general_param = {0};

struct audio_general_params *audio_general_get_param(void)
{
    return &audio_general_param;
}
/*
 *输出设备硬件位宽
 * */
int audio_general_out_dev_bit_width()
{
    if (audio_general_param.system_bit_width || audio_general_param.media_bit_width ||
        audio_general_param.esco_bit_width || audio_general_param.mic_bit_width
        || audio_general_param.usb_audio_bit_width) {
        return DATA_BIT_WIDE_24BIT;
    }
    return DATA_BIT_WIDE_16BIT;
}
/*
 *输入设备硬件位宽
 * */
int audio_general_in_dev_bit_width()
{
    if (audio_general_param.media_bit_width || audio_general_param.mic_bit_width) {
        return DATA_BIT_WIDE_24BIT;
    }
    return DATA_BIT_WIDE_16BIT;
}

__AUDIO_INIT_BANK_CODE
int audio_general_init()
{
#if defined(TCFG_SCENE_UPDATE_ENABLE) && TCFG_SCENE_UPDATE_ENABLE
    //若流程中有较多音效模块（或渲染封装节点），会导致此处遍历模块耗时较长
    get_music_pipeline_node_uuid();
#if (TCFG_MIC_EFFECT_ENABLE || (defined CONFIG_WIRELESS_MIC_CASE_ENABLE))
    get_mic_pipeline_node_uuid();
#endif
#endif

#if ((defined TCFG_AUDIO_DATA_EXPORT_DEFINE) && (TCFG_AUDIO_DATA_EXPORT_DEFINE == AUDIO_DATA_EXPORT_VIA_UART))
    uartSendInit();
#endif/*TCFG_AUDIO_DATA_EXPORT_DEFINE*/

#if TCFG_AUDIO_CONFIG_TRACE
    audio_config_trace_setup(TCFG_AUDIO_CONFIG_TRACE_INTERVAL);
#endif/*TCFG_AUDIO_CONFIG_TRACE*/

#if MEDIA_24BIT_ENABLE
    struct stream_bit_width stream_par = {0};
    if (get_system_stream_bit_width(&stream_par)) {
        audio_general_param.system_bit_width = stream_par.bit_width;
    }

    if (get_media_stream_bit_width(&stream_par)) {
        audio_general_param.media_bit_width = stream_par.bit_width;
    }

    if (get_esco_stream_bit_width(&stream_par)) {
        audio_general_param.esco_bit_width = stream_par.bit_width;
    }

    if (get_mic_eff_stream_bit_width(&stream_par)) {
        audio_general_param.mic_bit_width = stream_par.bit_width;
    }

    if (get_usb_audio_stream_bit_width(&stream_par)) {
        audio_general_param.usb_audio_bit_width = stream_par.bit_width;
    }
#endif

#if defined(TCFG_AUDIO_GLOBAL_SAMPLE_RATE) &&TCFG_AUDIO_GLOBAL_SAMPLE_RATE
    audio_general_param.sample_rate = TCFG_AUDIO_GLOBAL_SAMPLE_RATE;
#endif
    return 0;
}

#if (CONFIG_CPU_BR27 || CONFIG_CPU_BR28 || CONFIG_CPU_BR29 || CONFIG_CPU_BR36)
static const int general_sample_rate_table[] = {8000, 16000, 32000, 44100, 48000, 96000};
#else
static const int general_sample_rate_table[] = {8000, 16000, 32000, 44100, 48000, 96000, 192000};
#endif

int audio_general_set_global_sample_rate(int sample_rate)
{
    local_irq_disable();
    for (u8 i = 0; i < ARRAY_SIZE(general_sample_rate_table); i++) {
        if (general_sample_rate_table[i] == sample_rate) {
            audio_general_param.sample_rate = sample_rate;
            local_irq_enable();
            printf("cur_global_samplerate %d", sample_rate);
            return 0;
        }
    }
    local_irq_enable();
    return -1;
}

