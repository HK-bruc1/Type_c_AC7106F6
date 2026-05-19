#ifndef __AI_AUDIO_CONFIG__
#define __AI_AUDIO_CONFIG__


//支持多少个通道
#define AI_AUDIO_CH_MAX                                 2

//缓存大小
#define AI_AUDIO_CACHE_SIZE(frame_size)                 ((frame_size) * 10)

//数据接收读写buf速度debug
#define AI_AUDIO_RECV_SPEED_DEBUG                       0

//填1时翻译播报用文件内recv_ch接口，否则可以用a2dp等方式播放翻译音频
#define AI_AUDIO_TRANSLATION_RECV_CHANNEL_ENABLE        1

//录音翻译中通话（来电），需关闭翻译，由耳机控制还是APP控制，1表示耳机控制
#define AI_AUDIO_TRANSLATION_CALL_CONTROL_BY_DEVICE     0

//a2dp翻译后是否接收翻译语音数据
#define AI_AUDIO_A2DP_TRANSLATION_RECV_ENABLE           1

//编码数据送去解码测试
#define AI_AUDIO_TRANSLATION_ENC_TO_DEC_TEST            0

//通话翻译左耳原声，右耳译文使能，关闭则左右耳都播译文
#define AI_AUDIO_CALL_TRANSLATION_LEFT_RIGHT_SPLIT      0

#ifndef TCFG_AI_PLAYER_ENABLE
#define TCFG_AI_PLAYER_ENABLE                           0
#endif

#ifndef TCFG_AI_RECORDER_ENABLE
#define TCFG_AI_RECORDER_ENABLE                         0
#endif

#ifndef TCFG_AI_TRANSLATOR_ENABLE
#define TCFG_AI_TRANSLATOR_ENABLE                       0
#endif

#if TCFG_AI_TRANSLATOR_ENABLE
#undef  TCFG_AI_PLAYER_ENABLE
#define TCFG_AI_PLAYER_ENABLE                           1
#undef  TCFG_AI_RECORDER_ENABLE
#define TCFG_AI_RECORDER_ENABLE                         1
#endif

#endif
