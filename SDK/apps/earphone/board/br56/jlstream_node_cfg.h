/**
* 注意点：
* 0.此文件变化，在工具端会自动同步修改到工具配置中
* 1.功能块通过【---------xxx------】和 【#endif // xxx 】，是工具识别的关键位置，请勿随意改动
* 2.目前工具暂不支持非文件已有的C语言语法，此文件应使用文件内已有的语法增加业务所需的代码，避免产生不必要的bug
* 3.修改该文件出现工具同步异常或者导出异常时，请先检查文件内语法是否正常
**/

#ifndef JLSTREAM_NODE_CFG_H
#define JLSTREAM_NODE_CFG_H

// ------------提示音宏定义------------
#define TCFG_TONE_EN_ENABLE                       1               
#define TCFG_TONE_ZH_ENABLE                       0               
#define TCFG_TONE_AAC_ENABLE                      0                // AAC
#define TCFG_TONE_F2A_ENABLE                      0                // F2A
#define TCFG_TONE_MP3_ENABLE                      0                // MP3
#define TCFG_TONE_MSBC_ENABLE                     0                // MSBC
#define TCFG_TONE_MTY_ENABLE                      0                // MTY
#define TCFG_TONE_SBC_ENABLE                      0                // SBC
#define TCFG_TONE_SIN_ENABLE                      1                // SIN
#define TCFG_TONE_WAV_ENABLE                      0                // WAV
#define TCFG_TONE_WTG_ENABLE                      0                // WTG
#define TCFG_TONE_WTS_ENABLE                      1                // WTS
// ------------提示音宏定义------------

// ------------流程图宏定义------------
#define TCFG_AUDIO_BIT_WIDTH                      1                // 位宽
#define TCFG_2BAND_MERGE_ENABLE                   0                // 2Band Merge
#define TCFG_3BAND_MERGE_ENABLE                   0                // 3Band Merge
#define TCFG_3D_PLUS_MODULE_NODE_ENABLE           0                // 3D Plus
#define TCFG_ADC_NODE_ENABLE                      1                // ADC
#define TCFG_AGC_NODE_ENABLE                      1                // AGC
#define TCFG_AI_RX_NODE_ENABLE                    0                // AI_RX
#define TCFG_AI_TX_NODE_ENABLE                    1                // AI_TX
#define TCFG_AUDIO_AVC_NODE_ENABLE                0                // AVC
#define TCFG_AUDIO_CVP_3MIC_MODE                  0                // 3MIC通话
#define TCFG_AUDIO_CVP_DEVELOP_ENABLE             0                // 通话第三方算法
#define TCFG_AUDIO_CVP_DMS_ANS_MODE               0                // 双MIC通话
#define TCFG_AUDIO_CVP_DMS_AWN_DNS_MODE           0                // 2MIC-AWN
#define TCFG_AUDIO_CVP_DMS_DNS_MODE               0                // 双MIC+DNS
#define TCFG_AUDIO_CVP_DMS_FLEXIBLE_ANS_MODE      0                // 话务双MIC通话
#define TCFG_AUDIO_CVP_DMS_FLEXIBLE_DNS_MODE      0                // 话务双MIC+DNS
#define TCFG_AUDIO_CVP_DMS_HYBRID_DNS_MODE        0                // HYBRID双MIC+DNS
#define TCFG_AUDIO_CVP_SMS_ANS_MODE               0                // 单MIC通话
#define TCFG_AUDIO_CVP_SMS_DNS_MODE               0                // 单MIC+DNS
#define TCFG_AUDIO_CVP_SMS_VF_MODE                0                // 单麦AEC
#define TCFG_AUDIO_CVP_V3_MODE                    1                // CVP_V3
#define TCFG_AUDIO_DEBUG_NODE_ENABLE              0                // Audio Debug
#define TCFG_AUTO_WAH_NODE_ENABLE                 0                // Auto Wah
#define TCFG_AUTODUCK_NODE_ENABLE                 0                // AutoDuck Trigger、AutoDuck
#define TCFG_AUTOMUTE_NODE_ENABLE                 0                // automute
#define TCFG_AUTOTUNE_NODE_ENABLE                 0                // Autotune
#define TCFG_BASS_BOOST_COMBO_NODE_ENABLE         0                // Bass Boost[C]
#define TCFG_BASS_TREBLE_NODE_ENABLE              0                // Bass Treble
#define TCFG_BT_AUDIO_SYNC_NODE_ENABLE            1                // 蓝牙音频同步
#define TCFG_CHANNEL_EXPANDER_NODE_ENABLE         1                // Channel Expander
#define TCFG_CHANNEL_MERGE_NODE_ENABLE            0                // Channel Merge
#define TCFG_CHANNEL_SWAP_NODE_ENABLE             0                // 声道交换器
#define TCFG_CHORUS_NODE_ENABLE                   0                // Chorus
#define TCFG_CLK_SYNC_NODE_ENABLE                 1                // 播放同步
#define TCFG_CONVERT_NODE_ENABLE                  1                // Convert、BitWidth Convert
#define TCFG_CROSSOVER_NODE_ENABLE                0                // 分频器 三段、分频器 两段
#define TCFG_DAC_NODE_ENABLE                      1                // DAC
#define TCFG_DATA_EXPORT_NODE_ENABLE              0                // data export
#define TCFG_DECODER_NODE_ENABLE                  1                // 解码器
#define TCFG_DNS_NODE_ENABLE                      0                // DNS Noise Suppressor
#define TCFG_DYNAMIC_EQ_EXT_DETECTOR_NODE_ENABLE  0                // Dynamic EQ  Ext Detector
#define TCFG_DYNAMIC_EQ_NODE_ENABLE               0                // Dynamic EQ
#define TCFG_DYNAMIC_EQ_PRO_EXT_DETECTOR_NODE_ENABLE 0                // Dynamic EQ  Pro Ext Detector
#define TCFG_DYNAMIC_EQ_PRO_NODE_ENABLE           0                // Dynamic EQ Pro
#define TCFG_ECHO_NODE_ENABLE                     0                // Echo
#define TCFG_EFFECT_DEV0_NODE_ENABLE              0                // EffectDev0
#define TCFG_EFFECT_DEV1_NODE_ENABLE              0                // EffectDev1
#define TCFG_EFFECT_DEV2_NODE_ENABLE              0                // EffectDev2
#define TCFG_EFFECT_DEV3_NODE_ENABLE              0                // EffectDev3
#define TCFG_EFFECT_DEV4_NODE_ENABLE              0                // EffectDev4
#define TCFG_ENCODER_NODE_ENABLE                  1                // 编码器
#define TCFG_ENERGY_DETECT_NODE_ENABLE            0                // Energy Detect
#define TCFG_EQ_ENABLE                            1                // EQ
#define TCFG_ESCO_TX_NODE_ENABLE                  0                // ESCO_TX
#define TCFG_FADE_NODE_ENABLE                     0                // Fade
#define TCFG_FREQUENCY_COMPRESSOR_NODE_ENABLE     0                // Frequency Compressor
#define TCFG_FREQUENCY_SHIFT_HOWLING_NODE_ENABLE  0                // Frequency Shift
#define TCFG_GAIN_NODE_ENABLE                     0                // Gain
#define TCFG_HARMONIC_EXCITER_NODE_ENABLE         0                // Harmonic Exciter
#define TCFG_IIS_NODE_ENABLE                      0                // IIS0_TX
#define TCFG_IIS_RX_NODE_ENABLE                   0                // IIS0_RX
#define TCFG_INDICATOR_NODE_ENABLE                0                // Indicator
#define TCFG_KEY_TONE_NODE_ENABLE                 1                // 按键音
#define TCFG_LHDC_X_NODE_ENABLE                   0                // LHDC-X
#define TCFG_LIMITER_NODE_ENABLE                  0                // Limiter
#define TCFG_LLNS_DNS_NODE_ENABLE                 0                // LLNS_DNS
#define TCFG_LLNS_NODE_ENABLE                     0                // LLNS
#define TCFG_MIXER_NODE_ENABLE                    0                // MIXER
#define TCFG_MULTI_BAND_DRC_NODE_ENABLE           0                // MDRC
#define TCFG_MULTI_BAND_LIMITER_NODE_ENABLE       0                // MB Limiter
#define TCFG_MULTI_CH_IIS_NODE_ENABLE             0                // MULTI CH IIS0 TX
#define TCFG_MULTI_CH_IIS_RX_NODE_ENABLE          0                // MULTI CH IIS0 RX
#define TCFG_MULTI_MIC_NODE_ENABLE                0                // Multi-MIC
#define TCFG_MUTE_NODE_ENABLE                     0                // mute
#define TCFG_NF_SUPPRESSOR_NODE_ENABLE            0                // NF Suppressor
#define TCFG_NOISEGATE_NODE_ENABLE                0                // NoiseGate
#define TCFG_NOISEGATE_PRO_NODE_ENABLE            0                // NoiseGate Pro
#define TCFG_NOTCH_HOWLING_NODE_ENABLE            0                // Howling Suppress
#define TCFG_NS_NODE_ENABLE                       0                // Noise Suppressor
#define TCFG_NS_NODE_LITE_ENABLE                  0                // Noise Suppressor LITE
#define TCFG_PCM_DELAY_NODE_ENABLE                0                // PCM Delay
#define TCFG_PDM_NODE_ENABLE                      0                // PDM MIC
#define TCFG_PLATE_REVERB_ADVANCE_NODE_ENABLE     0                // Plate Reverb Advance
#define TCFG_PLATE_REVERB_NODE_ENABLE             0                // Plate Reverb
#define TCFG_PLC_NODE_ENABLE                      0                // 丢包修复PLC
#define TCFG_REPLACE_NODE_ENABLE                  0                // Replace
#define TCFG_RING_TONE_NODE_ENABLE                1                // 铃声
#define TCFG_SIGNAL_GENERATOR_NODE_ENABLE         0                // SignalGen
#define TCFG_SINK_DEV0_NODE_ENABLE                0                // Sink_Dev0
#define TCFG_SINK_DEV1_NODE_ENABLE                0                // Sink_Dev1
#define TCFG_SINK_DEV2_NODE_ENABLE                0                // Sink_Dev2
#define TCFG_SINK_DEV3_NODE_ENABLE                0                // Sink_Dev3
#define TCFG_SINK_DEV4_NODE_ENABLE                0                // Sink_Dev4
#define TCFG_SOUND_SPLITTER_NODE_ENABLE           0                // 音频分流器
#define TCFG_SOURCE_DEV0_NODE_ENABLE              0                // Source_Dev0
#define TCFG_SOURCE_DEV1_NODE_ENABLE              0                // Source_Dev1
#define TCFG_SOURCE_DEV2_NODE_ENABLE              0                // Source_Dev2
#define TCFG_SOURCE_DEV3_NODE_ENABLE              0                // Source_Dev3
#define TCFG_SOURCE_DEV4_NODE_ENABLE              0                // Source_Dev4
#define TCFG_SPATIAL_ADV_NODE_ENABLE              0                // Spatial Adv
#define TCFG_SPATIAL_AUDIO_ENABLE                 0                // 空间音效
#define TCFG_SPEAKER_EQ_NODE_ENABLE               0                // SpeakerEQ
#define TCFG_SPLIT_GAIN_NODE_ENABLE               0                // Split Gain
#define TCFG_SRC_NODE_ENABLE                      1                // SRC
#define TCFG_STEREO_SPATIAL_WIDER_NODE_ENABLE     0                // Stereo Spatial Wider
#define TCFG_STEREO_WIDENER_NODE_ENABLE           0                // Stereo Widener
#define TCFG_STEROMIX_NODE_ENABLE                 0                // SteroMix
#define TCFG_SURROUND_DEMO_NODE_ENABLE            0                // 环绕音demo、Surround Demo
#define TCFG_SURROUND_NODE_ENABLE                 0                // Surround Effect
#define TCFG_SWITCH_NODE_ENABLE                   0                // Switch
#define TCFG_TDM_RX_NODE_ENABLE                   0                // TDM_RX
#define TCFG_TDM_TX_NODE_ENABLE                   0                // TDM_TX
#define TCFG_THREE_D_EFFECT_NODE_ENABLE           0                // ThreeD
#define TCFG_TONE_NODE_ENABLE                     1                // 提示音
#define TCFG_UART_NODE_ENABLE                     0                // 串口打印
#define TCFG_VBASS_NODE_ENABLE                    0                // Virtual Bass
#define TCFG_VIRTUAL_BASS_CLASSIC_NODE_ENABLE     0                // Multi Frequency Generator
#define TCFG_VIRTUAL_BASS_PRO_MODULE_NODE_ENABLE  0                // Virtual Bass Pro
#define TCFG_VIRTUAL_SURROUND_HP_NODE_ENABLE      0                // Virtual Surround Headphone 2t4
#define TCFG_VIRTUAL_SURROUND_PRO_MODULE_NODE_ENABLE 0                // Virtual Surround Pro
#define TCFG_VIRTUAL_SURROUND_PRO_NODE_ENABLE     0                // upmix 2to5
#define TCFG_VOCAL_REMOVER_NODE_ENABLE            0                // Vocal Remover
#define TCFG_VOCAL_TRACK_SEPARATION_NODE_ENBALE   0                // 声道拆分
#define TCFG_VOCAL_TRACK_SYNTHESIS_NODE_ENABLE    0                // 声道组合
#define TCFG_VOICE_CHANGER_ADV_NODE_ENABLE        0                // Voice Changer Adv
#define TCFG_VOICE_CHANGER_NODE_ENABLE            0                // Voice Changer
#define TCFG_WDRC_ADVANCE_NODE_ENABLE             0                // DRC Advance
#define TCFG_WDRC_NODE_ENABLE                     0                // DRC

// ------------表单节点全局配置宏------------
#define TCFG_CVP_V3_NN_TABLE_TYPE                 NN_TABLE_CVP_GENERAL_V4 // NN网络配置
#define CVP_BF_VERSION                            2                // Version
#define MULTI_BAND_PHASE_COMPENSATION_EN          0                // MDRC 相位补偿
// ------------表单节点全局配置宏------------
#define EQ_SECTION_MAX                            0xc              // EQ_SECTION_MAX
#define TCFG_AVC_ALGO_SELECT                      0                // AVC-算法类型选择
#define TCFG_AVC_AEC_ENABLE                       0                // AVC-AEC使能
#define TCFG_AVC_NLP_ENABLE                       0                // AVC-NLP使能
#define TCFG_CVP_ALGO_TYPE                        1                // CVP_V3-通话算法
#define TCFG_CVP_FLEXIBLE_MODE                    1                // CVP_V3-Mode_Type:
#define TCFG_LLNS_DNS_MODE                        0                // LLNS_DNS-Mode
#define TCFG_ENCODER_CHANNEL_NUM                  1                // 编码器-声道
#define CVP_THIRD_ALGO_TYPE                       0                // 通话第三方算法-算法预设
#define CVP_THIRD_ALGO_VPU_ENABLE                 0                // 通话第三方算法-VPU方案
#define TCFG_STREAM_BIN_ENC_ENABLE                0                // stream.bin加密使能
// ------------流程图宏定义------------

// ------------流程图节点位宽定义------------
#define TCFG_AUDIO_EFX_665C_RUN_MODE              EFx_BW_UNUSED    // 2Band Merge
#define TCFG_AUDIO_EFX_F0C3_RUN_MODE              EFx_BW_UNUSED    // 2MIC-AWN
#define TCFG_AUDIO_EFX_1B9D_RUN_MODE              EFx_BW_UNUSED    // 3Band Merge
#define TCFG_AUDIO_EFX_0048_RUN_MODE              EFx_BW_UNUSED    // 3MIC通话
#define TCFG_AUDIO_EFX_4C00_RUN_MODE              EFx_BW_UNUSED    // 3D Plus
#define TCFG_AUDIO_EFX_F975_RUN_MODE              EFx_BW_UNUSED    // A2DP_RX
#define TCFG_AUDIO_EFX_D06D_RUN_MODE              EFx_BW_16t16     // ADC
#define TCFG_AUDIO_EFX_DFDA_RUN_MODE              EFx_BW_16t16     // AI_TX
#define TCFG_AUDIO_EFX_86B9_RUN_MODE              EFx_BW_UNUSED    // automute
#define TCFG_AUDIO_EFX_D0D0_RUN_MODE              EFx_BW_16t16     // AGC
#define TCFG_AUDIO_EFX_C07A_RUN_MODE              EFx_BW_UNUSED    // Autotune
#define TCFG_AUDIO_EFX_2F5E_RUN_MODE              EFx_BW_UNUSED    // Auto Wah
#define TCFG_AUDIO_EFX_6CE5_RUN_MODE              EFx_BW_UNUSED    // AutoDuck
#define TCFG_AUDIO_EFX_7099_RUN_MODE              EFx_BW_UNUSED    // AutoDuck Trigger
#define TCFG_AUDIO_EFX_DF98_RUN_MODE              EFx_BW_UNUSED    // AI_RX
#define TCFG_AUDIO_EFX_D2BF_RUN_MODE              EFx_BW_UNUSED    // AVC
#define TCFG_AUDIO_EFX_96DE_RUN_MODE              EFx_BW_UNUSED    // Audio Debug
#define TCFG_AUDIO_EFX_CC8C_RUN_MODE              EFx_BW_UNUSED    // Bass Treble
#define TCFG_AUDIO_EFX_7DE5_RUN_MODE              EFx_BW_16t16 | EFx_BW_32t32 // BitWidth Convert
#define TCFG_AUDIO_EFX_E9D0_RUN_MODE              EFx_BW_UNUSED    // Bass Boost[C]
#define TCFG_AUDIO_EFX_1AA6_RUN_MODE              EFx_BW_UNUSED    // Convert
#define TCFG_AUDIO_EFX_6DD9_RUN_MODE              EFx_BW_UNUSED    // Chorus
#define TCFG_AUDIO_EFX_DA15_RUN_MODE              EFx_BW_16t16     // Channel Expander
#define TCFG_AUDIO_EFX_BF8E_RUN_MODE              EFx_BW_UNUSED    // Channel Merge
#define TCFG_AUDIO_EFX_5FF6_RUN_MODE              EFx_BW_16t16     // CVP_V3
#define TCFG_AUDIO_EFX_DEFE_RUN_MODE              EFx_BW_UNUSED    // DRC
#define TCFG_AUDIO_EFX_DCCD_RUN_MODE              EFx_BW_32t32     // DAC
#define TCFG_AUDIO_EFX_87A0_RUN_MODE              EFx_BW_UNUSED    // Dynamic EQ
#define TCFG_AUDIO_EFX_00AE_RUN_MODE              EFx_BW_UNUSED    // DNS Noise Suppressor
#define TCFG_AUDIO_EFX_A60B_RUN_MODE              EFx_BW_UNUSED    // Dynamic EQ  Ext Detector
#define TCFG_AUDIO_EFX_4731_RUN_MODE              EFx_BW_UNUSED    // Dynamic EQ Pro
#define TCFG_AUDIO_EFX_793C_RUN_MODE              EFx_BW_UNUSED    // Dynamic EQ  Pro Ext Detector
#define TCFG_AUDIO_EFX_DE61_RUN_MODE              EFx_BW_UNUSED    // data export
#define TCFG_AUDIO_EFX_4250_RUN_MODE              EFx_BW_UNUSED    // DRC Advance
#define TCFG_AUDIO_EFX_8458_RUN_MODE              EFx_BW_UNUSED    // ESCO_RX
#define TCFG_AUDIO_EFX_849A_RUN_MODE              EFx_BW_UNUSED    // ESCO_TX
#define TCFG_AUDIO_EFX_737B_RUN_MODE              EFx_BW_16t16 | EFx_BW_32t32 // EQ
#define TCFG_AUDIO_EFX_98A4_RUN_MODE              EFx_BW_UNUSED    // Echo
#define TCFG_AUDIO_EFX_A248_RUN_MODE              EFx_BW_UNUSED    // Energy Detect
#define TCFG_AUDIO_EFX_A4E1_RUN_MODE              EFx_BW_UNUSED    // EffectDev0
#define TCFG_AUDIO_EFX_A4E2_RUN_MODE              EFx_BW_UNUSED    // EffectDev1
#define TCFG_AUDIO_EFX_A4E3_RUN_MODE              EFx_BW_UNUSED    // EffectDev2
#define TCFG_AUDIO_EFX_A4E4_RUN_MODE              EFx_BW_UNUSED    // EffectDev3
#define TCFG_AUDIO_EFX_A4E5_RUN_MODE              EFx_BW_UNUSED    // EffectDev4
#define TCFG_AUDIO_EFX_6195_RUN_MODE              EFx_BW_UNUSED    // Frequency Shift
#define TCFG_AUDIO_EFX_1BF5_RUN_MODE              EFx_BW_UNUSED    // Fade
#define TCFG_AUDIO_EFX_BFE4_RUN_MODE              EFx_BW_UNUSED    // Frequency Compressor
#define TCFG_AUDIO_EFX_A904_RUN_MODE              EFx_BW_UNUSED    // Gain
#define TCFG_AUDIO_EFX_C482_RUN_MODE              EFx_BW_UNUSED    // Howling Suppress
#define TCFG_AUDIO_EFX_1B2A_RUN_MODE              EFx_BW_UNUSED    // Harmonic Exciter
#define TCFG_AUDIO_EFX_5DF0_RUN_MODE              EFx_BW_UNUSED    // HYBRID双MIC+DNS
#define TCFG_AUDIO_EFX_48E2_RUN_MODE              EFx_BW_UNUSED    // Indicator
#define TCFG_AUDIO_EFX_6963_RUN_MODE              EFx_BW_UNUSED    // IIS0_RX
#define TCFG_AUDIO_EFX_69A5_RUN_MODE              EFx_BW_UNUSED    // IIS0_TX
#define TCFG_AUDIO_EFX_09FE_RUN_MODE              EFx_BW_UNUSED    // LLNS
#define TCFG_AUDIO_EFX_0624_RUN_MODE              EFx_BW_UNUSED    // LINEIN
#define TCFG_AUDIO_EFX_2DFB_RUN_MODE              EFx_BW_UNUSED    // LE_Audio_Sink
#define TCFG_AUDIO_EFX_2D57_RUN_MODE              EFx_BW_UNUSED    // LE_Audio_Source
#define TCFG_AUDIO_EFX_FF09_RUN_MODE              EFx_BW_UNUSED    // LE_Audio延时同步
#define TCFG_AUDIO_EFX_4E5B_RUN_MODE              EFx_BW_UNUSED    // Limiter
#define TCFG_AUDIO_EFX_65C5_RUN_MODE              EFx_BW_UNUSED    // LHDC-X
#define TCFG_AUDIO_EFX_1CA2_RUN_MODE              EFx_BW_UNUSED    // LLNS_DNS
#define TCFG_AUDIO_EFX_E62A_RUN_MODE              EFx_BW_UNUSED    // MIXER
#define TCFG_AUDIO_EFX_D5E0_RUN_MODE              EFx_BW_UNUSED    // mute
#define TCFG_AUDIO_EFX_74CB_RUN_MODE              EFx_BW_UNUSED    // MDRC
#define TCFG_AUDIO_EFX_F58A_RUN_MODE              EFx_BW_UNUSED    // MB Limiter
#define TCFG_AUDIO_EFX_F7FC_RUN_MODE              EFx_BW_UNUSED    // MULTI CH IIS0 TX
#define TCFG_AUDIO_EFX_F7BA_RUN_MODE              EFx_BW_UNUSED    // MULTI CH IIS0 RX
#define TCFG_AUDIO_EFX_55C9_RUN_MODE              EFx_BW_UNUSED    // Multi Frequency Generator
#define TCFG_AUDIO_EFX_E856_RUN_MODE              EFx_BW_UNUSED    // Multi-MIC
#define TCFG_AUDIO_EFX_3BC9_RUN_MODE              EFx_BW_UNUSED    // Noise Suppressor
#define TCFG_AUDIO_EFX_B7C4_RUN_MODE              EFx_BW_UNUSED    // NoiseGate
#define TCFG_AUDIO_EFX_E955_RUN_MODE              EFx_BW_UNUSED    // NoiseGate Pro
#define TCFG_AUDIO_EFX_FFF7_RUN_MODE              EFx_BW_UNUSED    // Noise Suppressor LITE
#define TCFG_AUDIO_EFX_9F3F_RUN_MODE              EFx_BW_UNUSED    // NF Suppressor
#define TCFG_AUDIO_EFX_A09F_RUN_MODE              EFx_BW_UNUSED    // PDM MIC
#define TCFG_AUDIO_EFX_A8F4_RUN_MODE              EFx_BW_UNUSED    // PCM Delay
#define TCFG_AUDIO_EFX_5101_RUN_MODE              EFx_BW_UNUSED    // Plate Reverb
#define TCFG_AUDIO_EFX_0753_RUN_MODE              EFx_BW_UNUSED    // Plate Reverb Advance
#define TCFG_AUDIO_EFX_D186_RUN_MODE              EFx_BW_32t32     // PCSPK
#define TCFG_AUDIO_EFX_B711_RUN_MODE              EFx_BW_16t16     // PCMIC
#define TCFG_AUDIO_EFX_B5E1_RUN_MODE              EFx_BW_UNUSED    // Replace
#define TCFG_AUDIO_EFX_1ECD_RUN_MODE              EFx_BW_32t32     // SRC
#define TCFG_AUDIO_EFX_3BE6_RUN_MODE              EFx_BW_UNUSED    // SpeakerEQ
#define TCFG_AUDIO_EFX_FB00_RUN_MODE              EFx_BW_UNUSED    // SteroMix
#define TCFG_AUDIO_EFX_603A_RUN_MODE              EFx_BW_UNUSED    // Sink
#define TCFG_AUDIO_EFX_EB56_RUN_MODE              EFx_BW_UNUSED    // Source
#define TCFG_AUDIO_EFX_2357_RUN_MODE              EFx_BW_UNUSED    // Switch
#define TCFG_AUDIO_EFX_8934_RUN_MODE              EFx_BW_UNUSED    // Surround Effect
#define TCFG_AUDIO_EFX_C90C_RUN_MODE              EFx_BW_UNUSED    // Surround Demo
#define TCFG_AUDIO_EFX_88E5_RUN_MODE              EFx_BW_UNUSED    // Stereo Widener
#define TCFG_AUDIO_EFX_8FC4_RUN_MODE              EFx_BW_UNUSED    // Source_Dev0
#define TCFG_AUDIO_EFX_8FC5_RUN_MODE              EFx_BW_UNUSED    // Source_Dev1
#define TCFG_AUDIO_EFX_8FC7_RUN_MODE              EFx_BW_UNUSED    // Source_Dev3
#define TCFG_AUDIO_EFX_8FC8_RUN_MODE              EFx_BW_UNUSED    // Source_Dev4
#define TCFG_AUDIO_EFX_8FC6_RUN_MODE              EFx_BW_UNUSED    // Source_Dev2
#define TCFG_AUDIO_EFX_B328_RUN_MODE              EFx_BW_UNUSED    // Sink_Dev0
#define TCFG_AUDIO_EFX_B329_RUN_MODE              EFx_BW_UNUSED    // Sink_Dev1
#define TCFG_AUDIO_EFX_B32A_RUN_MODE              EFx_BW_UNUSED    // Sink_Dev2
#define TCFG_AUDIO_EFX_B32B_RUN_MODE              EFx_BW_UNUSED    // Sink_Dev3
#define TCFG_AUDIO_EFX_B32C_RUN_MODE              EFx_BW_UNUSED    // Sink_Dev4
#define TCFG_AUDIO_EFX_225D_RUN_MODE              EFx_BW_UNUSED    // SignalGen
#define TCFG_AUDIO_EFX_2A10_RUN_MODE              EFx_BW_UNUSED    // Split Gain
#define TCFG_AUDIO_EFX_6700_RUN_MODE              EFx_BW_UNUSED    // Stereo Spatial Wider
#define TCFG_AUDIO_EFX_A64E_RUN_MODE              EFx_BW_UNUSED    // Spatial Adv
#define TCFG_AUDIO_EFX_8C21_RUN_MODE              EFx_BW_UNUSED    // ThreeD
#define TCFG_AUDIO_EFX_B455_RUN_MODE              EFx_BW_UNUSED    // TDM_TX
#define TCFG_AUDIO_EFX_B413_RUN_MODE              EFx_BW_UNUSED    // TDM_RX
#define TCFG_AUDIO_EFX_B542_RUN_MODE              EFx_BW_UNUSED    // upmix 2to5
#define TCFG_AUDIO_EFX_B0D5_RUN_MODE              EFx_BW_UNUSED    // Virtual Bass
#define TCFG_AUDIO_EFX_7293_RUN_MODE              EFx_BW_UNUSED    // Voice Changer
#define TCFG_AUDIO_EFX_320E_RUN_MODE              EFx_BW_UNUSED    // Voice Changer Adv
#define TCFG_AUDIO_EFX_2F7A_RUN_MODE              EFx_BW_UNUSED    // Vocal Remover
#define TCFG_AUDIO_EFX_02E6_RUN_MODE              EFx_BW_UNUSED    // Virtual Bass Pro
#define TCFG_AUDIO_EFX_191F_RUN_MODE              EFx_BW_UNUSED    // Virtual Surround Pro
#define TCFG_AUDIO_EFX_DCD4_RUN_MODE              EFx_BW_UNUSED    // Virtual Surround Headphone 2t4
#define TCFG_AUDIO_EFX_8346_RUN_MODE              EFx_BW_32t32     // 按键音
#define TCFG_AUDIO_EFX_6FEC_RUN_MODE              EFx_BW_UNUSED    // 本地音乐
#define TCFG_AUDIO_EFX_0A2C_RUN_MODE              EFx_BW_16t16     // 编码器
#define TCFG_AUDIO_EFX_9B3B_RUN_MODE              EFx_BW_16t16 | EFx_BW_32t32 // 播放同步
#define TCFG_AUDIO_EFX_E76E_RUN_MODE              EFx_BW_UNUSED    // 串口打印
#define TCFG_AUDIO_EFX_299E_RUN_MODE              EFx_BW_UNUSED    // 单麦AEC
#define TCFG_AUDIO_EFX_DBF5_RUN_MODE              EFx_BW_UNUSED    // 单MIC+DNS
#define TCFG_AUDIO_EFX_D0BC_RUN_MODE              EFx_BW_UNUSED    // 单MIC通话
#define TCFG_AUDIO_EFX_ED7F_RUN_MODE              EFx_BW_UNUSED    // 丢包修复PLC
#define TCFG_AUDIO_EFX_2C15_RUN_MODE              EFx_BW_UNUSED    // 多路复用器
#define TCFG_AUDIO_EFX_8D63_RUN_MODE              EFx_BW_UNUSED    // 分频器 两段
#define TCFG_AUDIO_EFX_BF28_RUN_MODE              EFx_BW_UNUSED    // 分频器 三段
#define TCFG_AUDIO_EFX_68F2_RUN_MODE              EFx_BW_UNUSED    // 话务双MIC+DNS
#define TCFG_AUDIO_EFX_90F9_RUN_MODE              EFx_BW_UNUSED    // 话务双MIC通话
#define TCFG_AUDIO_EFX_3F20_RUN_MODE              EFx_BW_UNUSED    // 环绕音demo
#define TCFG_AUDIO_EFX_1A85_RUN_MODE              EFx_BW_32t32     // 解码器
#define TCFG_AUDIO_EFX_83E1_RUN_MODE              EFx_BW_UNUSED    // 空间音效
#define TCFG_AUDIO_EFX_A576_RUN_MODE              EFx_BW_32t32     // 蓝牙音频同步
#define TCFG_AUDIO_EFX_CADC_RUN_MODE              EFx_BW_32t32     // 铃声
#define TCFG_AUDIO_EFX_A724_RUN_MODE              EFx_BW_UNUSED    // 声道拆分
#define TCFG_AUDIO_EFX_36D7_RUN_MODE              EFx_BW_UNUSED    // 声道交换器
#define TCFG_AUDIO_EFX_503D_RUN_MODE              EFx_BW_UNUSED    // 声道组合
#define TCFG_AUDIO_EFX_420E_RUN_MODE              EFx_BW_UNUSED    // 双MIC+DNS
#define TCFG_AUDIO_EFX_2115_RUN_MODE              EFx_BW_UNUSED    // 双MIC通话
#define TCFG_AUDIO_EFX_768A_RUN_MODE              EFx_BW_32t32     // 提示音
#define TCFG_AUDIO_EFX_76EF_RUN_MODE              EFx_BW_UNUSED    // 通话第三方算法
#define TCFG_AUDIO_EFX_F73C_RUN_MODE              EFx_BW_UNUSED    // 写卡
#define TCFG_AUDIO_EFX_74E3_RUN_MODE              EFx_BW_16t16 | EFx_BW_32t32 // 音量控制器
#define TCFG_AUDIO_EFX_D911_RUN_MODE              EFx_BW_UNUSED    // 音频分流器
// ------------流程图节点位宽定义------------

// ------------流程图EQ节点类型宏定义------------
#define EQ_CFG_TYPE_HIGH_PASS                     0                // High Pass
#define EQ_CFG_TYPE_LOW_PASS                      0                // Low Pass
#define EQ_CFG_TYPE_PEAKING                       1                // Peaking
#define EQ_CFG_TYPE_HIGH_SHELF                    0                // High Shelf
#define EQ_CFG_TYPE_LOW_SHELF                     0                // Low Shelf
#define EQ_CFG_TYPE_HIGH_SHELF_Q                  0                // High Shelf Q
#define EQ_CFG_TYPE_LOW_SHELF_Q                   0                // Low Shelf Q
#define EQ_CFG_TYPE_HP                            0                // Hp
#define EQ_CFG_TYPE_LP                            0                // Lp
// ------------流程图EQ节点类型宏定义------------
#endif

