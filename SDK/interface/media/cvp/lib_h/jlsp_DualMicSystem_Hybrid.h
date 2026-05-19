#ifndef __JLSP_DMSHYBRID_NS_H__
#define __JLSP_DMSHYBRID_NS_H__



//回声消除设置参数
typedef struct {
    int AEC_Process_MaxFrequency;    //最大频率
    int AEC_Process_MinFrequency;    //最小频率
    int AF_Length; //挂空
    float muf;     //学习速率
} JLSP_DmsHybrid_aec_cfg;


//非线性回声压制设置参数
typedef struct {
    int NLP_Process_MaxFrequency;    //最小频率
    int NLP_Process_MinFrequency;    //最大频率
    float OverDrive;     //压制系数

} JLSP_DmsHybrid_nlp_cfg;

//dms参数
typedef struct {
    float noise_level_db_T0;  //噪声水平等级阈值下限,退出融合阈值,范围(10~90db)
    float noise_level_db_T1;  //噪声水平等级阈值上限,进入融合阈值,范围(10~90db)
    float merge_det_in_time; //切换进入融合需要时间
    float merge_det_out_time; //切换退出融合需要时间
    float compen_db;
    float *transfer_func;
    int Enc_Process_MaxFrequency;
    int Enc_Process_MinFrequency;
} JLSP_DmsHybrid_enc_cfg;


//dns参数
typedef struct {
    float minSuppress;
    float AggressFactor;
    float init_noise_lvl;
    float compen_db;
    float *filter_eq;
    int DNS_Process_MaxFrequency;
    int DNS_Process_MinFrequency;

} JLSP_DmsHybrid_dns_cfg;


//风噪检测
typedef struct {
    float coh_val_T;	//相关性阈值
    float eng_db_T;     //能量阈值

} JLSP_DmsHybrid_wd_cfg;



typedef struct {
    int min_mag_db_level;
    int max_mag_db_level;
    int addition_mag_db_level;
    int clip_mag_db_level;
    int floor_mag_db_level;

} JLSP_DmsHybrid_agc_cfg;


/*
gain_floor: 增益的最小值控制,范围0~1,建议值(0~0.2)之间
over_drive: 控制降噪强度:
0 < over_drive < 1，越小降噪强度越轻，太小噪声会很大；
over_drive = 1,正常降噪
over_drive > 1,降噪强度加强，越大降噪强度越强，太大会吃音
建议调节范围0.3~3之间来控制降噪强度的强弱
high_gain: 控制声音的宏亮度,范围(1.0f~3.5f),越大声音越宏亮,太大可能会使噪声增加, 为1.0f表示不做增强, 建议设置2.0f左右
rb_rate: 混响强度,设置范围（0.0f~0.9f）,越大混响越强, 为0.0f表示不增强, 建议默认设置0.5f
*/
void JLSP_DmsHybridSystem_EnableModule(int Enablebit, int Enablebit_Ex);
void *JLSP_DmsHybridSystem_Init(char *private_buf, char *share_buf, void *aec_cfg, void *nlp_cfg, void *dms_cfg, void *dns_cfg, void *wn_cfg, void *agc_cfg, int samplerate, int EnableBit);
int JLSP_DmsHybridSystem_GetHeapSize(int *private_size, int *share_size, int samplerate);

int JLSP_DmsHybridSystem_Reset(void *m);
void JLSP_DmsHybridSystem_UpdateSharedBuffer(void *m, char *shared_buffer);
float JLSP_DmsHybridSystem_Process(void *handle, short *near, short *fb_in, short *far, short *out, int Batch);
int JLSP_DmsHybridSystem_Free(void *m);

void JLSP_DmsHybridSystem_SetNoiselevel(void *m, float noise_level_init);

int JLSP_DmsHybridSystem_GetCurState(void *handle);



void JLSP_DmsHybridSystem_SetFbEQ(void *handle, float *fb_tansfer_eq);

void JLSP_DmsHybridSystem_SetTalkEQ(void *handle, float *talk_eq);

#endif

