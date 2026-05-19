
#ifndef __JLSP_V3_NS_H__
#define __JLSP_V3_NS_H__

#include "asm/cpu.h"


//=========================================================================
//===============================模块参数配置==============================
//=========================================================================
enum {
    AEC_EN_BIT = BIT(0),
    NLP_EN_BIT = BIT(1),
    DNS_EN_BIT = BIT(2),
    BF_EN_BIT = BIT(3),
    DRC_EN_BIT = BIT(4),
    WNC_EN_BIT = BIT(5),
    MICSEL_EN_BIT = BIT(6),
    EQ_EN_BIT = BIT(7),
    DOA_EN_BIT = BIT(8),
    AEC_FB_EN_BIT = BIT(0),
    NLP_FB_EN_BIT = BIT(1),
};


//语音增强类型
typedef enum  {
    SINGLE_TYPE = BIT(0),		//单麦
    SINGLE_AECNLP_TYPE = BIT(1), // AEC_NLP
    DUAL_BF_TYPE = BIT(4),		//双麦bf
    DUAL_HYBRID_TYPE = BIT(5),	//双麦fbTalk
    DUAL_OWS_TYPE = BIT(6),		//双麦OWS
    DUAL_FLEX_TYPE = BIT(7),	//双麦话务
    DUAL_BONE_TYPE = BIT(8),	//双麦骨传
    DUAL_CLIP_TYPE = BIT(9),	// Dual clip
    DUAL_CLIP_V2_TYPE = BIT(10),// Dual clip V2
    TRI_FUSION_TYPE = BIT(16),	//三麦融合
    TRI_TELE_TYPE = BIT(17),	//三麦话务
    TRI_BONE_TYPE = BIT(18),	//三麦骨传

} enum_seType;



//融合类型
typedef enum {
    FB_FUSION_TYPE_DUMMY,
    FB_FUSION_TYPE_WIND,
    FB_FUSION_TYPE_V2,
    FB_FUSION_TYPE_V3,
    FB_FUSION_TYPE_V4,
    FB_FUSION_TYPE_V5,
    FB_FUSION_TYPE_V6,
    FB_FUSION_TYPE_V7,
} enum_fbFusion;



//bf类型
typedef enum {
    BF_TYPE_V1,
    BF_TYPE_V1_1,
    BF_TYPE_V2,
} enum_bfType;



//麦克风参数配置
typedef struct {
    unsigned char mic0En;
    unsigned char mic1En;
    unsigned char mic2En;
    unsigned char mic3En;

    float mic0PreGain;
    float mic1PreGain;
    float mic2PreGain;
    float mic3PreGain;

    int micEnNum;
} JLSP_mic_v3_cfg_t;



//出现单个麦掩蔽参数配置
typedef struct {
    float detectTime; //
    float detectEngDiffTh;
    float detectEngLowerBound;
    int detMaxFrequency;
    int detMinFrequency;
    int onlyDetect;

} JLSP_micsel_v3_cfg_t;



//beamforming参数配置
typedef struct {
    //工具配置参数
    int encProcessMaxFrequency;
    int encProcessMinFrequency;
    float micDistance;
    int sirMaxFreq;
    float targetSignalDegradation;
    float aggressFactor;
    float minSuppress;

    //steering vector param.
    float *steer_vec1;
    float *steer_vec2;
    float *ds_steer_vec;


    //sdk内部参数
    u8 bfDualAdaptiveFilter;
    u8 bfDrvTypeHyper;
    u8 bfMainLobeMic;
    u8 bfSideLobeMic;

    //beamforming类型
    enum_bfType type;

    float supressFactor;

    // post directional enhancement enable bit
    int bfPost_en;

    float desiredAngle;

} JLSP_bf_v3_cfg_t;



//三麦降噪参数配置
typedef struct {
    int fusionFreq;
    float dBTh1;
    float dBTh2;
    float dBTh3;
    enum_fbFusion type;

} JLSP_fusion_cfg_t;


//线性回声消除参数配置
typedef struct {
    int aecProcessMaxFrequency;    //最大频率
    int aecProcessMinFrequency;    //最小频率
    float muf;     //学习速率
    int aec_type;
} JLSP_aec_v3_cfg_t;


//非线性回声压制参数配置
typedef struct {
    int nlpProcessMaxFrequency;    //最小频率
    int nlpProcessMinFrequency;    //最大频率
    int preEnhance;
    float overDrive;     //压制系数
    float overDrivePre;     //压制系数
} JLSP_nlp_v3_cfg_t;


//dns参数配置
typedef struct {
    float aggressFactor;
    float minSuppress;
    float initNoiseLvl;
    float compenDb;
} JLSP_dns_v3_cfg_t;


//风噪检测参数配置
typedef struct {
    float windProbHighTh;
    float windProbLowTh;
    float windCountEnterTh;
    float windCountExitTh;
    float windEngDbTh;

} JLSP_wind_v3_cfg_t;


//drc参数配置
typedef struct {
    float compressAttackTime;
    float compressReleaseTime;

    float ngAttackTime;
    float ngReleaseTime;

    float kneeWidth;

    float compressSlope;
    float kneeThresholdDb;
    float limitMagDb;

    float noiseGateThresholdDb;
    float ngStepOutDb;

    float makeUpGain;
    int samplerate;

} JLSP_drc_v3_cfg_t;


typedef struct {
    int samplerate;
    int cohenProcessMinFrequency; // MinProFreq
    int cohenProcessMaxFrequency; // MaxProFreq
    float resOverDrive;		  // Suppress Level
    float miniSuppress;			  // Minimum Suppress Level
    float xThr;				  // X Energy threshold
    int useDe;					  // calc_cohen with D&E; otherwise with X&D
} JLSP_cohen_v3_cfg_t;


// Filter Canceller Based On NLMS
typedef struct {
    int samplerate;
    int processMaxFrequency; // MaxProFreq
    int processMinFrequency; // MinProFreq
    float maxMuf;				// MaxStep
    float minMuf;				// MinStep
    float cohefMaxGamma;
    float varmuMaxGamma;
    float sirMaxGamma;
    float sirMinimumSp;
    float cohenXdThr;
    int useSirGain;
    int useErleReset;
    float erleThr;

    JLSP_cohen_v3_cfg_t coh_cfg;

} JLSP_dual_flex_adf_cfg_t;


//=========================================================================
//===========================流程控制结构体================================
//=========================================================================

//单麦降噪参数配置
typedef struct {
    int enableBit;

    //float* Eq_TransferFunc; //eq传递函数
    float *singleWbEq;
    float *singleNbEq;
    int samplerate;
    int processMaxFrequency;
    int processMinFrequency;
    int externalEnableBit;

    int spe_att_en;		// SPE_ATT enable/disenable
    int post_pro_en;		// MCRA enable/disenable

    float preGainDb;
    float singleCompenDb;
    float aggressFactor;
    float minSupress;

    // AEC data output enablebit
    int aec_dump_enable;

} JLSP_single_v3_cfg_t;



//双麦降噪参数配置
typedef struct {
    int enableBit;

    float *dualPhaseCompenVec; //相位补偿
    float *dualWbEqVec;
    float *dualNbEqVec;
    float *dualWnWbEqVec;
    float *dualWnNbEqVec;

    int samplerate;
    float dualPreGainDb;
    int dualProcessMaxFrequency;
    int dualProcessMinFrequency;
    float dualCompenDb; //mic增益补偿

    int spe_att_en;		// SPE_ATT enable/disenable
    int post_pro_en;		// MCRA enable/disenable
    int noise_est_en;	// NOISE_EST enable/disenable

    float aggressFactor;
    float minSupress;

    int externalEnableBit;

    // AEC data output enablebit
    int aec_dump_enable;

} JLSP_dual_bf_v3_cfg_t;



// Dual clip config.
typedef struct {
    int enableBit;

    float *dualPhaseCompenVec; //相位补偿
    float *dualWbEqVec;
    float *dualNbEqVec;
    float *dualWnWbEqVec;
    float *dualWnNbEqVec;

    int samplerate;
    float dualPreGainDb;
    int dualProcessMaxFrequency;
    int dualProcessMinFrequency;
    float dualCompenDb; //mic增益补偿

    int spe_att_en;		// SPE_ATT enable/disenable
    int post_pro_en;		// MCRA enable/disenable

    float aggressFactor;
    float minSupress;

    int nlpSelectTalkOrFf;
    int externalEnableBit;

    // AEC data output enablebit
    int aec_dump_enable;

} JLSP_dual_clip_v3_cfg_t;


// Dual clip V2 config.
typedef struct {
    int enableBit;

    float *dualPhaseCompenVec; //相位补偿
    float *dualWbEqVec;
    float *dualNbEqVec;
    float *dualWnWbEqVec;
    float *dualWnNbEqVec;

    int samplerate;
    float dualPreGainDb;
    int dualProcessMaxFrequency;
    int dualProcessMinFrequency;
    float dualCompenDb; //mic增益补偿

    int spe_att_en;		// SPE_ATT enable/disenable
    int post_pro_en;		// MCRA enable/disenable
    int noise_est_en;	// NOISE_EST enable/disenable
    int low_freq_eng_det_en;  // low freq energy detect enable/disenable


    float aggressFactor;
    float minSupress;
    int nlpSelectTalkOrFf;
    int externalEnableBit;

    // AEC data output enablebit
    int aec_dump_enable;

} JLSP_dual_clipv2_v3_cfg_t;



//三麦降噪参数配置
typedef struct {
    int enableBit;
    int enableBitEx;

    float *triFbTransferFuncOn; 	// fb -> main传递函数
    float *triFbTransferFuncOff; 	// fb -> main传递函数
    float *triPhaseCompenVec; 	// 相位补偿
    float *triWbEqVec;
    float *triNbEqVec;
    float *triWnWbEqVec;
    float *triWnNbEqVec;
    float triFbCompenDb; 		// fb补偿增益

    int samplerate;
    float triPreGainDb;
    int triProcessMaxFrequency;
    int triProcessMinFrequency;
    float triCompenDb; 	// mic增益补偿

    int spe_att_en;		// SPE_ATT enable/disenable
    int post_pro_en;		// MCRA enable/disenable
    int noise_est_en;	// NOISE_EST enable/disenable
    int triToDual_en;

    float aggressFactor;
    float minSupress;

    int externalEnableBit;

    // AEC data output enablebit
    int aec_dump_enable;

    int ref_vad_en;

} JLSP_tri_v3_cfg_t;



//HYBRID降噪参数配置
typedef struct {
    int enableBit;
    int enableBitEx;

    float *hybridFbTransferFunc; //fb -> main传递函数
    float *hybridWbEqVec;
    float *hybridNbEqVec;
    float hybridFbCompenDb; //fb补偿增益

    int samplerate;
    float hybridPreGainDb;
    int hybridProcessMaxFrequency;
    int hybridProcessMinFrequency;
    float hybridCompenDb; //mic增益补偿


    float aggressFactor;
    float minSupress;

    int externalEnableBit;

    // AEC data output enablebit
    int aec_dump_enable;

} JLSP_hybrid_v3_cfg_t;




//话务麦双麦降噪参数配置
typedef enum {
    DF_TYPE_V1 = 1,
    DF_TYPE_V2 = 2,
    DF_TYPE_V1_V2 = 3,
} enum_dfType;
typedef struct {

    int enableBit;

    float *dualFlexWbEqVec;
    float *dualFlexNbEqVec;
    float *dualWnFlexWbEqVec;
    float *dualWnFlexNbEqVec;
    float *dualFlexPhaseCompenVec; //相位补偿

    int dualFlexType;
    int samplerate;
    int dualProcessMaxFrequency;
    int dualProcessMinFrequency;

    float dualPreGainDb;
    float dualCompenDb;

    int post_pro_en; // MCRA enable/disenable

    float aggressFactor;
    float minSupress;

    int externalEnableBit;

    // AEC data output enablebit
    int aec_dump_enable;

} JLSP_dual_flex_v3_cfg_t;


// AEC&NLP module configuration
typedef struct {
    int enableBit;
    int samplerate;
    float preGainDb;

} JLSP_single_aecnlp_v3_cfg_t;

typedef struct {

    float degree_start; // 30.0f; end > start
    float degree_end;  // 60.0f;
    int doa_low_freq;
    int doa_high_freq;
    float pwr_avg_min_th;
    int use_au;

} JLSP_doa_v3_cfg_t;



typedef struct {
    //降噪类型
    // const enum_seType type;

    //麦克风参数配置
    JLSP_mic_v3_cfg_t mic_cfg;


    //线性回声压制参数配置
    JLSP_aec_v3_cfg_t aec1_cfg;
    JLSP_aec_v3_cfg_t aec2_cfg;
    JLSP_aec_v3_cfg_t aec3_cfg;


    //非线性回声压制参数配置
    JLSP_nlp_v3_cfg_t nlp1_cfg;
    JLSP_nlp_v3_cfg_t nlp2_cfg;
    JLSP_nlp_v3_cfg_t nlp3_cfg;


    //风噪检测参数配置
    JLSP_wind_v3_cfg_t wind_cfg;


    //beamforming参数配置
    JLSP_bf_v3_cfg_t bf_cfg;


    //融合参数配置
    JLSP_fusion_cfg_t fusion_cfg;


    //drc参数配置
    JLSP_drc_v3_cfg_t drc_cfg;


    //麦掩蔽检测参数配置
    JLSP_micsel_v3_cfg_t micSel_cfg;

    // Filter Canceller Based On NLMS
    JLSP_dual_flex_adf_cfg_t dual_flex_adf_cfg;

    /* ------------- Processing flow config -------------*/
    //单麦参数配置
    JLSP_single_v3_cfg_t  single_cfg;

    //双麦参数配置
    JLSP_dual_bf_v3_cfg_t dual_cfg;


    //三麦参数配置
    JLSP_tri_v3_cfg_t tri_cfg;


    //hybrid参数配置
    JLSP_hybrid_v3_cfg_t hybrid_cfg;

    // Single aec+nlp module
    JLSP_single_aecnlp_v3_cfg_t single_aecnlp_cfg;

    //dual flexible ns module
    JLSP_dual_flex_v3_cfg_t dual_flex_cfg;

    //dual clip ns module
    JLSP_dual_clip_v3_cfg_t dual_clip_cfg;

    //dual clip v2 ns module
    JLSP_dual_clipv2_v3_cfg_t dual_clip_v2_cfg;

    /* ------------- Others config -------------*/

    JLSP_doa_v3_cfg_t doa_cfg;
} JLSP_params_v3_cfg;


//=========================================================================
//===========================函数接口参数配置==============================
//=========================================================================
typedef enum {
    // common function port
    SET_WBORNB_EQ,           // set EQ
    GET_WBORNB_EQ,           // get EQ-wb/nb state
    GET_AECNLP_DATA,

    // Dual-mic function port
    DUAL_SET_ATT_PARAMS,
    DUAL_SET_PHASE_COMPEN,   // set phase compensatory EQ
    DUAL_GET_WD_INFO,        // get wind info
    DUAL_GET_MIC_STATE,      // get micsel state: `0` for dual_mic, `1` for mic0, `2` for mic1
    DUAL_SET_MIC_STATE,      // set micsel state: `0` for dual_mic, `1` for mic0, `2` for mic1


    // Tri-mic function port
    TRI_SET_ANC_STATEMODE,   // set anc state
    TRI_SET_FB2MAIN_EQ,      // set anc on/off fb2main transfer function
    TRI_SET_ATT_PARAMS,
    TRI_SET_MIC_NUM,		 // set triToDual_en: 0 for tri, 1 for dual(no fb mic)
    TRI_SET_PHASE_COMPEN,    // set phase compensatory EQ
    TRI_GET_WD_INFO,         // get wind info
    TRI_GET_MIC_STATE,       // get micsel state: `0` for dual_mic, `1` for mic0, `2` for mic1
    TRI_GET_ANC_STATEMODE,   // set anc state

} enum_portName;


typedef struct {
    u8 is_wb;
    float *eqCoeffs;
    u8 type;			// 1 for eq, 2 for wn eq
} JLSP_set_wbornb_eq;


typedef struct {
    u8 isAncOn;        // 1 for anc off, 2/3 for anc on
    u8 ancMode;        // Not inplement for the moment
} JLSP_set_anc_state_mode;


typedef struct {
    u8 isAncon;        // 1 for anc off, 2/3 for anc on
    float *fb2main_eq; // Corresponding fb2main_eq
} JLSP_set_fb2main_eq;


typedef struct {
    float attThreshold;
    float keepSilenceTime;
    float attTime;
} JLSP_set_att_time_th;


typedef struct {
    int wd_flag;
    float wd_val;
    // int state;
} JLSP_get_wd_info_t;


int JLSP_EncApi_GetBufSize(int *private_size, int *share_size, void *cfg, int setype);
void *JLSP_EncApi_Init(char *private_buf, char *share_buf, void *cfg, int setype);
int JLSP_EncApi_Process(void *handle, short *in0, short *in1, short *in2, short *ref, short *out, void *cfg, int batchSize, int setype);
int JLSP_EncApi_SetCfg(void *handle, void *cfg, int setype);
int JLSP_EncApi_FuncInfoPort_Cfg(void *handle, enum_portName portName, void *port_cfg, int setype);
int JLSP_EncApi_Free(void *handle);


#endif
