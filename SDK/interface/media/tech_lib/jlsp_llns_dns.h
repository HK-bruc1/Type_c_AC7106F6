#ifndef __JLSP_LLNS_DNS_H__
#define __JLSP_LLNS_DNS_H__

typedef struct {
    int Qval;						  // Data stream bit width; only support 16/24bit; 16bit Qval=15;24bit Qval=23
    float GlobalMinSuppress;		  // [-90:0.1:0]dB, global minimun suppress
    float Suppress_Level;			  // the explanation is as follows
    float Input_Gain;				  // for input pcm
    float Post_Gain;				  // for finally gain
    float NN_MiniSuppress;			  // [0:0.01:1],control NN Minisuppress
    float NN_GainMeanThr;			  // [0:0.01:1],for the ns judgment flag
    float NS_MiniSuppress;			  // [0:0.01:1],ns minisuppress
    float NS_AggressFactor;			  // [0:0.01:2],ns noisesuppress intensity
    float FDNS_NoiseFloordB;		  // [-100:0.01:0]dB, Presumed noise level
    float FDNS_NoiseDistributionRate; // [0:0.01:1] ,Noise distribution rate,actually growing exponentially
    float FDNS_NoiseFloorSuppress;	  // [0:0.01:1]dB, Suppress rate when the noise level is below the FDNS_NoiseFloordB
    int FDNS_AttackTime;			  // [0:1:1000]ms, FDNS Effective time of suppress
    int FDNS_ReleaseTime;			  // [0:1:1000]ms, FDNS Effective time of release
    int FDNS_Freq1;					  // [0:1:24000]Hz, FDNS lowMiddleBand Frequency
    int FDNS_Freq2;					  // [0:1:24000]Hz, FDNS MiddleHighBand Frequency
    float FDNS_NoiseFloordB1;		  // [-200:0.01:0]dB, Presumed noise level
    float FDNS_NoiseFloordB2;		  // [-200:0.01:0]dB, Presumed noise level
    float FDNS_NoiseFloordB3;		  // [-200:0.01:0]dB, Presumed noise level
    int GlobalGainPFreq;			  // [0:1:samplerate],boundary frequency for freq_division process
    float GlobalGainPFactor;		  // [0:0.01:10],compared to the inhibitory intensity of GlobalSuppress
    float resever[10];				  // retention bit width
} llns_dns_param_t;

/*
GlobalMinSuppress: 增益的最小值控制,范围0~1,建议值(0~0.2)之间
Suppress_Level: 控制降噪强度:
0 < Suppress_Level < 1，越小降噪强度越轻，太小噪声会很大；
Suppress_Level = 1,正常降噪
Suppress_Level > 1,降噪强度加强，越大降噪强度越强，太大会吃音
建议调节范围0.3~3之间来控制降噪强度的强弱
*/

typedef struct {
    int agc_en;
    int min_mag_db_level;
    int max_mag_db_level;
    int addition_mag_db_level;
    int clip_mag_db_level;
    int floor_mag_db_level;
} llns_dns_agc_param_t;
typedef enum {
    LLNS_DNS_LVL_BYPASS = 0,
    LLNS_DNS_LVL_SS_NR = 1, // steady-state noise reduction
    LLNS_DNS_LVL_AI_NR = 2,
} llns_dns_lvl_t;

void *JLSP_llns_dns_init(char *private_buffer, int private_size, char *shared_buffer, int share_size, const int samplerate, float gain_floor, float over_drive);
int JLSP_llns_dns_get_heap_size(int *private_size, int *shared_size, const int samplerate);

int JLSP_llns_dns_reset(void *m);
void JLSP_llns_dns_update_shared_buffer(void *m, char *shared_buffer);
int JLSP_llns_dns_process(void *m, int32_t *input, int32_t *output, int *outsize);
int JLSP_llns_dns_free(void *m);

int JLSP_llns_dns_set_winsize(void *m, int winsize);
int JLSP_llns_dns_set_parameters(void *m, int mode, llns_dns_param_t *p);

void JLSP_llns_dns_set_noiselevel(void *m, float noise_level_init);
//设置降噪等级，分为4档，0：不降噪，1：降噪强度较轻，2：降噪强度适中，3：降噪强度较强；默认选择2
int JLSP_llns_dns_set_level(void *m, int level);

// agc模块通过const变量LLNS_AGC_EN使能，0：不使能，1：使能；如果不使用该模块也可以通过外部配置drc来实现同样功能
int JLSP_llns_dns_set_agc(void *m, void *cfg);

#endif
