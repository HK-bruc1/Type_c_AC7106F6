const JLSP_mic_v3_cfg_t mic_init_cfg = {
    .mic0En = 1,
    .mic1En = 1,
    .mic2En = 1,
    .mic3En = 1,
    .mic0PreGain = 1.0f,
    .mic1PreGain = 1.0f,
    .mic2PreGain = 1.0f,
    .mic3PreGain = 1.0f,
    .micEnNum = 4
};

//出现单个麦掩蔽参数
const JLSP_micsel_v3_cfg_t micsel_init_cfg = {
    .detectTime = 1.0f,
    .detectEngDiffTh = 20.0f,
    .detectEngLowerBound = -55.0f,
    .detMaxFrequency = 8000,
    .detMinFrequency = 200,
    .onlyDetect = 0
};

//beamforming 参数配置
const JLSP_bf_v3_cfg_t bf_init_cfg = {
    .encProcessMaxFrequency = 8000,
    .encProcessMinFrequency = 0,
    .micDistance = 0.015f,
    .sirMaxFreq = 3000,
    .targetSignalDegradation = 1.0f,
    .aggressFactor = 1.0f,
    .minSuppress = 0.1f,

    .steer_vec1 = NULL,
    .steer_vec2 = NULL,
    .ds_steer_vec = NULL,

    .bfDualAdaptiveFilter = 0,
    .bfDrvTypeHyper = 0,
    .bfMainLobeMic = 1,
    .bfSideLobeMic = 0,

    .type = BF_TYPE_V2,
    .supressFactor = 0.6f,
    .bfPost_en = 0,
    .desiredAngle = 130.f
};


const JLSP_fusion_cfg_t fusion_init_cfg = {
    .fusionFreq = 2000,
    .dBTh1 = -4.0f,
    .dBTh2 = 105.0f,
    .dBTh3 = 86.0f,
    .type = FB_FUSION_TYPE_V6,
};

const JLSP_aec_v3_cfg_t aec_cfg_default = {
    .aecProcessMaxFrequency = 8000,
    .aecProcessMinFrequency = 0,
    .muf = 0.05f,
    .aec_type = 0
};

const JLSP_nlp_v3_cfg_t nlp_cfg_default = {
    .nlpProcessMaxFrequency = 8000,
    .nlpProcessMinFrequency = 0,
    .preEnhance = 0,
    .overDrive = 5.0f,
    .overDrivePre = 0.f
};

JLSP_aec_v3_cfg_t aec_fb_cfg_default = {
    .aecProcessMaxFrequency = 2000,
    .aecProcessMinFrequency = 0,
    .muf = 0.05f,
    .aec_type = 0
};

const JLSP_nlp_v3_cfg_t nlp_fb_cfg_default = {
    .nlpProcessMaxFrequency = 2000,
    .nlpProcessMinFrequency = 0,
    .preEnhance = 1,
    .overDrive = 5.0f,
    .overDrivePre = 0.f
};

const JLSP_dns_v3_cfg_t dns_init_cfg = {
    .aggressFactor = 1.0f,
    .minSuppress = 0.001f,
    .initNoiseLvl = 2.2e4f,
    .compenDb = 0.f
};

const JLSP_wind_v3_cfg_t wn_init_cfg = {
    .windProbHighTh = 0.55f,
    .windProbLowTh = 0.15f,
    .windCountEnterTh = 20,
    .windCountExitTh = 20,
    .windEngDbTh = 95.0f
};

const JLSP_drc_v3_cfg_t drc_init_cfg = {
    .compressAttackTime = 0.0003f,
    .compressReleaseTime = 0.1f,
    .ngAttackTime = 0.1f,
    .ngReleaseTime = 0.01f,
    .kneeWidth = 2.0f,
    .compressSlope = 1.0f / 40.0f,
    .kneeThresholdDb = -6.0f,
    .limitMagDb = 0.0f,
    .noiseGateThresholdDb = -50.0f,
    .ngStepOutDb = -1.2f,
    .makeUpGain = 14.0f,
    .samplerate = 16000
};

const JLSP_dual_flex_adf_cfg_t dual_flex_adf_cfg = {
    .samplerate = 16000,
    .processMaxFrequency = 8000,//ps
    .processMinFrequency = 0,//ps
    .maxMuf = 0.08f,
    .minMuf = 0.0f,
    .cohefMaxGamma = 0.5f,  // 预压制强度，越小压制越强，默认值0.5
    .varmuMaxGamma = 0.85f, // 前级压制强度，越小压制越强，默认值0.85
    .sirMaxGamma = 0.1f,    // 后级压制强度，越小压制越弱，默认值0.1
    .sirMinimumSp = 0.04f,
    .cohenXdThr = 0.1f,
    .useSirGain = 0,		// 噪声压制强度(压制过大时置0)
    .useErleReset = 0,
    .erleThr = -6.f,
    .coh_cfg = {
        .samplerate = 16000,
        .cohenProcessMaxFrequency = 8000,
        .cohenProcessMinFrequency = 0,
        .resOverDrive = 1.0f,
        .miniSuppress = 0.0f,
        .xThr = 1.39e-8f, //15.f,1.39e-8f
        .useDe = 0
    }
};

const JLSP_doa_v3_cfg_t doa_init_cfg = {
    .degree_start = 30.f,
    .degree_end = 60.f,
    .doa_low_freq = 10,
    .doa_high_freq = 32,
    .pwr_avg_min_th = 1.f,
    .use_au = 0,
};

const JLSP_single_aecnlp_v3_cfg_t aecnlp_init_cfg = {
    .enableBit = 0x03,
    .samplerate = 16000,
    .preGainDb = 10.f,
};

const JLSP_single_v3_cfg_t single_init_cfg = {

    .enableBit = 0x14,
    .singleWbEq = NULL,
    .singleNbEq = NULL,
    .samplerate = 16000,
    .spe_att_en = 0,	// 1 for enable, else for disable.
    .post_pro_en = 0,		// 1 for enable, else for disable.
    .processMaxFrequency = 8000,
    .processMinFrequency = 0,
    .preGainDb = 0.0f,
    .singleCompenDb = 0.0f,
    .aggressFactor = 1.0f,
    .minSupress = 0.001f,
    .externalEnableBit = 0x00,
    .aec_dump_enable = 0,
};

const JLSP_dual_bf_v3_cfg_t dual_bf_init_cfg = {

    .enableBit = 0x3f,

    .dualPhaseCompenVec = NULL,
    .dualWbEqVec = NULL,
    .dualNbEqVec = NULL,
    .dualWnWbEqVec = NULL,
    .dualWnNbEqVec = NULL,

    .samplerate = 16000,
    .dualPreGainDb = 18.0f,
    .dualProcessMaxFrequency = 8000,
    .dualProcessMinFrequency = 0,
    .dualCompenDb = 10.0f, //18.0f, //bf增益补偿


    .spe_att_en = 0,	// 1 for enable, else for disable.
    .post_pro_en = 0,		// 1 for enable, else for disable.
    .noise_est_en = 0,		// 1 for enable, else for disable.

    .aggressFactor = 1.0f,
    .minSupress = 0.001f,

    .externalEnableBit = 0x00,
    .aec_dump_enable = 0,
};

const JLSP_dual_clip_v3_cfg_t dual_clip_init_cfg = {

    .enableBit = 0x1f,

    .dualPhaseCompenVec = NULL,
    .dualWbEqVec = NULL,
    .dualNbEqVec = NULL,
    .dualWnWbEqVec = NULL,
    .dualWnNbEqVec = NULL,

    .dualPreGainDb = 12.0f,//18.0f,
    .dualProcessMaxFrequency = 8000,
    .dualProcessMinFrequency = 0,
    .samplerate = 16000,

    .spe_att_en = 0,	// 1 for enable, else for disable.
    .post_pro_en = 1,		// 1 for enable, else for disable.

    .aggressFactor = 1.0f,
    .minSupress = 0.001f,
    .dualCompenDb = 10.0f, //10.0f,

    .nlpSelectTalkOrFf = 1,
    .externalEnableBit = 0x00,
    .aec_dump_enable = 0,
};


const JLSP_dual_clipv2_v3_cfg_t dual_clip_v2_init_cfg = {

    .enableBit = 0b0111111,
    .dualPhaseCompenVec = NULL,
    .dualWbEqVec = NULL,
    .dualNbEqVec = NULL,
    .dualWnWbEqVec = NULL,
    .dualWnNbEqVec = NULL,

    .dualPreGainDb = 10.0f,//18.0f,
    .dualProcessMaxFrequency = 8000,
    .dualProcessMinFrequency = 0,
    .samplerate = 16000,

    .spe_att_en = 0,	// 1 for enable, else for disable.
    .post_pro_en = 1,		// 1 for enable, else for disable.
    .noise_est_en = 0,		// 1 for enable, else for disable.
    .low_freq_eng_det_en = 1,	// 1 for enable, else for disable.

    .aggressFactor = 1.0f,
    .minSupress = 0.001f,
    .dualCompenDb = 12.0f, //10.0f,


    .nlpSelectTalkOrFf = 1,
    .externalEnableBit = 0x00,
    .aec_dump_enable = 0,
};

JLSP_dual_flex_v3_cfg_t dual_flex_cfg = {
    .enableBit = 0b010111,//0b0010100,

    .dualFlexWbEqVec = NULL,
    .dualFlexNbEqVec = NULL,
    .dualWnFlexWbEqVec = NULL,
    .dualWnFlexNbEqVec = NULL,
    .dualFlexPhaseCompenVec = NULL,

    .dualFlexType = 1,
    .dualPreGainDb = 0.0f,
    .dualProcessMaxFrequency = 8000,
    .dualProcessMinFrequency = 0,
    .samplerate = 16000,

    .post_pro_en = 1,		// 1 for enable, else for disable.

    .aggressFactor = 1.0f,
    .minSupress = 0.001f,
    .dualCompenDb = 8.0f, //18.0f,

    .externalEnableBit = 0x00,
    .aec_dump_enable = 0,
};

const JLSP_hybrid_v3_cfg_t hybrid_init_cfg = {
    .enableBit = 0x3c,
    .enableBitEx = 0x00,

    .hybridFbTransferFunc = NULL,
    .hybridWbEqVec = NULL,
    .hybridNbEqVec = NULL,
    .hybridFbCompenDb = 0.0f,

    .samplerate = 16000,
    .hybridPreGainDb  = 18.0f,
    .hybridProcessMaxFrequency = 8000,
    .hybridProcessMinFrequency = 0,
    .hybridCompenDb = 10.0f,

    .aggressFactor = 1.0f,
    .minSupress = 0.001f,

    .externalEnableBit = 0x00,
    .aec_dump_enable = 0,
};

const JLSP_tri_v3_cfg_t tri_init_cfg = {

    .enableBit = 0x3f,
    .enableBitEx = 0x03,

    .triFbTransferFuncOn = NULL,  //fb -> main传递函数(anc on)
    .triFbTransferFuncOff = NULL, //fb -> main传递函数(anc off)
    .triPhaseCompenVec = NULL,
    .triWbEqVec = NULL,
    .triNbEqVec = NULL,
    .triWnWbEqVec = NULL,
    .triWnNbEqVec = NULL,
    .triFbCompenDb = 0.0f,	//fb增益补偿

    .spe_att_en = 0,	// 1 for enable, else for disable.
    .post_pro_en = 0,		// 1 for enable, else for disable.
    .noise_est_en = 0,		// 1 for enable, else for disable.

    .samplerate = 16000,
    .triPreGainDb = 18.0f,
    .triProcessMaxFrequency = 8000,
    .triProcessMinFrequency = 0,
    .triCompenDb = 10.0f, //18.0f, //bf增益补偿

    .aggressFactor = 1.0f,
    .minSupress = 0.001f,

    .externalEnableBit = 0x00,
    .aec_dump_enable = 0,
    .ref_vad_en = 1,
};
