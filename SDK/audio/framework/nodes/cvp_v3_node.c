#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".cvp_v3_node.data.bss")
#pragma data_seg(".cvp_v3_node.data")
#pragma const_seg(".cvp_v3_node.text.const")
#pragma code_seg(".cvp_v3_node.text")
#endif
#include "jlstream.h"
#include "media/audio_base.h"
#include "circular_buf.h"
#include "cvp_node.h"
#include "app_config.h"
#include "cvp_v3.h"
#include "audio_anc.h"

#if TCFG_AUDIO_DUT_ENABLE
#include "audio_dut_control.h"
#endif

#if TCFG_AUDIO_CVP_V3_MODE
#define CVP_INPUT_SIZE		256*3	//CVP输入缓存，short

#define RUN_MIC			1
//------------------stream.bin CVP参数文件解析结构-START---------------//

struct CVP_ALGO_SEL_CONFIG {
    u32 algo_type;		//算法选择
} __attribute__((packed));

struct CVP_MIC_SEL_CONFIG {
    u8 talk_mic;		//主MIC通道选择
    u8 talk_ref_mic;	//副MIC通道选择
    u8 talk_fb_mic;	    //FBMIC通道选择
} __attribute__((packed));

struct CVP_REF_MIC_CONFIG {
    u8 en;          //ref 回采硬使能
    u8 ref_mic_ch;	//ref 硬回采MIC通道选择
} __attribute__((packed));

struct CVP_PRE_GAIN_CONFIG {
    u8 en;
    float talk_mic_gain;	//主MIC前级数字增益，default:0dB(-90 ~ 40dB)
    float talk_ref_mic_gain;	//副MIC前级数字增益，default:0dB(-90 ~ 40dB)
    float talk_fb_mic_gain;	//FB MIC前级数字增益，default:0dB(-90 ~ 40dB)
} __attribute__((packed));

struct CVP_EQ_CONFIG {
    u8 en;
} __attribute__((packed));

struct CVP_AEC_CONFIG {
    u8 en;
    int aec_process_maxfrequency;	//default:8000,range[3000:8000]
    int aec_process_minfrequency;	//default:0,range[0:1000]
} __attribute__((packed));

struct CVP_NLP_CONFIG {
    u8 en;
    int nlp_process_maxfrequency;	//default:8000,range[3000:8000]
    int nlp_process_minfrequency;	//default:0,range[0:1000]
    float overdrive;				//default:1,range[0:30]
} __attribute__((packed));

struct CVP_ENC_CONFIG {
    u8 en;
    int enc_process_maxfreq;		//default:8000,range[3000:8000]
    int enc_process_minfreq;		//default:0,range[0:1000]
    int sir_maxfreq;				//default:3000,range[1000:8000]
    float mic_distance;				//default:0.015,range[0.035:0.015]
    float target_signal_degradation;//default:1,range[0:1]
    float enc_aggressfactor;		//default:4.f,range[0:4]
    float enc_minsuppress;			//default:0.09f,range[0:0.1]
} __attribute__((packed));

struct CVP_DOA_CONFIG {
    u8 en;
    u8 use_au;
    float degree_start;
    float degree_end;
} __attribute__((packed));

struct CVP_DNS_CONFIG {
    u8 en;
    float aggressfactor;			//default:1.25,range[1:2]
    float minsuppress;				//default:0.04,range[0.01:0.1]
} __attribute__((packed));

struct CVP_DRC_CONFIG {
    u8 en;
    float noisegatethresholdDb; 	//噪声门限制,default:-60.f(-90~-30dB)
    float makeupGain;				//补偿增益,default:14.0f(0~30dB)
    float kneethresholdDb;  		//压缩拐点阈值, default:-6.0f(-30~3dB)
} __attribute__((packed));

struct CVP_WNC_CONFIG {
    u8 en;
    float windProbHighTh;			//有风状态的概率阈值,default:0.55(0~1)
    float windProbLowTh;			//无风状态的概率阈值,default:0.15(0~1)
    float windEngDbTh;				//风强
}  __attribute__((packed));

/*MFDT Parameters*/
struct CVP_MFDT_CONFIG {
    u8 en;
    float detect_time;           // // 检测时间s，影响状态切换的速度
    float detect_eng_diff_thr;   // 0~-90 dB 两个mic能量差异持续大于此阈值超过检测时间则会检测为故障
    float detect_eng_lowerbound; // 0~-90 dB 当处于故障状态时，正常的mic能量大于此阈值才会检测能量差异，避免安静环境下误判切回正常状态
    int MalfuncDet_MaxFrequency;// 检测信号的最大频率成分
    int MalfuncDet_MinFrequency;// 检测信号的最小频率成分
    int OnlyDetect;// 0 -> 故障切换到单mic模式， 1-> 只检测不切换
}  __attribute__((packed));

/* adaptive */
struct CVP_ADAPTIVE_CONFIG {
    u8	flex_mode;
    int adaptive_processMaxFrequency; // MaxProFreq
    int adaptive_processMinFrequency; // MinProFreq
    float cohefMaxGamma;// 预压制强度，越小压制越强，默认值0.5
    float varmuMaxGamma;// 前级压制强度，越小压制越强，默认值0.85
    float sirMaxGamma;// 后级压制强度，越小压制越弱，默认值0.1
    int useSirGain;// 噪声压制强度(压制过大时置0)
}  __attribute__((packed));

struct CVP_FLOW_CONFIG {
    float preGainDb;	//ADC前级增益， default:0dB(0 ~ 30dB)
    float CompenDb;		//流程补偿增益，default:0dB(0 ~ 30dB)
} __attribute__((packed));

struct CVP_DEBUG_CONFIG {
    u8 output_sel; //输出数据选择
}  __attribute__((packed));

struct cvp_cfg_t {
    struct CVP_ALGO_SEL_CONFIG algo_sel;
    struct CVP_MIC_SEL_CONFIG mic_sel;
    struct CVP_REF_MIC_CONFIG ref_mic;
    struct CVP_PRE_GAIN_CONFIG pre_gain;
    struct CVP_EQ_CONFIG  eq_en;
    struct CVP_AEC_CONFIG aec;		 // aec
    struct CVP_NLP_CONFIG nlp;		 // nlp
    struct CVP_ENC_CONFIG enc;		 // enc
    struct CVP_DOA_CONFIG doa;		 // doa
    struct CVP_DNS_CONFIG dns;	     // dns
    struct CVP_DRC_CONFIG drc;		 // drc
    struct CVP_WNC_CONFIG wnc;		 // wnc
    struct CVP_MFDT_CONFIG mfdt;	 // mfdt
    struct CVP_ADAPTIVE_CONFIG adaptive; // adaptive
    struct CVP_FLOW_CONFIG flow;	 // flow
    struct CVP_DEBUG_CONFIG debug;	//debug
} __attribute__((packed));
//------------------stream.bin CVP参数文件解析结构-END---------------//

struct cvp_node_hdl {
    enum stream_scene scene;
    char name[16];
    CVP_CONFIG online_cfg;
    struct stream_frame *frame[3];	//输入frame存储，算法输入缓存使用
    u8 buf_cnt;						//循环输入buffer位置
    s16 buf[CVP_INPUT_SIZE];
    s16 buf_1[CVP_INPUT_SIZE];
    s16 buf_2[CVP_INPUT_SIZE];
    s16 *buf_3;
    u32 ref_sr;
    u16 source_uuid; //源节点uuid
    void (*lock)(void);
    void (*unlock)(void);
    struct CVP_MIC_SEL_CONFIG mic_sel;
    struct CVP_REF_MIC_CONFIG ref_mic;
    struct CVP_ALGO_SEL_CONFIG algo_sel;
    u8 ref_mic_num; //回采mic个数
    u8 adc_ch_num;
};

static struct cvp_node_hdl *g_cvp_hdl;

int cvp_v3_node_output_handle(s16 *data, u16 len)
{
    struct stream_frame *frame;
    frame = jlstream_get_frame(hdl_node(g_cvp_hdl)->oport, len);
    if (!frame) {
        return 0;
    }
    frame->len = len;
    memcpy(frame->data, data, len);
    jlstream_push_frame(hdl_node(g_cvp_hdl)->oport, frame);
    return len;
}

extern float eq_db2mag(float x);
void cvp_v3_node_param_cfg_update(struct cvp_cfg_t *cfg, void *priv)
{
    CVP_CONFIG *p = (CVP_CONFIG *)priv;

    if (g_cvp_hdl) {
        g_cvp_hdl->algo_sel.algo_type = cfg->algo_sel.algo_type;
        g_cvp_hdl->mic_sel.talk_mic = cfg->mic_sel.talk_mic;
        g_cvp_hdl->mic_sel.talk_ref_mic = cfg->mic_sel.talk_ref_mic;
        g_cvp_hdl->mic_sel.talk_fb_mic = cfg->mic_sel.talk_fb_mic;

        g_cvp_hdl->ref_mic.en = cfg->ref_mic.en;
        g_cvp_hdl->ref_mic.ref_mic_ch = cfg->ref_mic.ref_mic_ch;
        if (g_cvp_hdl->ref_mic.en && (g_cvp_hdl->buf_3 == NULL)) {
            g_cvp_hdl->ref_mic_num = audio_get_mic_num(g_cvp_hdl->ref_mic.ref_mic_ch);
            g_cvp_hdl->buf_3 = zalloc(CVP_INPUT_SIZE * sizeof(short));
        }
        printf("talk_mic=%d, ff_mic=%d, fb_mic=%d", g_cvp_hdl->mic_sel.talk_mic, g_cvp_hdl->mic_sel.talk_ref_mic, g_cvp_hdl->mic_sel.talk_fb_mic);
        printf("ref_mic_en=%d, ref_mic_ch=%d", g_cvp_hdl->ref_mic.en, g_cvp_hdl->ref_mic.ref_mic_ch);
        printf("algo type=%d pre_en=%d eq_en=%d", g_cvp_hdl->algo_sel.algo_type, cfg->pre_gain.en, cfg->eq_en.en);
    }

    //获取algo_type, 兼容非数据流，没有g_cvp_hdl的流程
    u32 algo_type = cvp_get_algo_type();

    p->adc_ref_en = cfg->ref_mic.en;
    //更新预处理参数
    struct audio_cvp_pre_param_t pre_cfg;
    pre_cfg.pre_gain_en = cfg->pre_gain.en;
    pre_cfg.talk_mic_gain = eq_db2mag(cfg->pre_gain.talk_mic_gain);
    pre_cfg.talk_ref_mic_gain = eq_db2mag(cfg->pre_gain.talk_ref_mic_gain);
    pre_cfg.talk_fb_mic_gain = eq_db2mag(cfg->pre_gain.talk_fb_mic_gain);
    audio_cvp_v3_probe_param_update(&pre_cfg);

    //更新算法参数
    if (algo_type & CVP_ALGO_1MIC_AECNLP) {		//AEC和NLP流程一般离线识别使用
        p->enable_module = cfg->aec.en | (cfg->nlp.en << 1) ;
    } else if (algo_type & CVP_ALGO_1MIC) {
        p->enable_module = cfg->aec.en | (cfg->nlp.en << 1) | (cfg->dns.en << 2) | (cfg->drc.en << 4) | (cfg->eq_en.en << 7);
    } else {
        p->enable_module = cfg->aec.en | (cfg->nlp.en << 1) | (cfg->dns.en << 2) | (cfg->enc.en << 3) | (cfg->drc.en << 4) | (cfg->wnc.en << 5) | (cfg->mfdt.en << 6) | (cfg->eq_en.en << 7) | (cfg->doa.en << 8);
    }
    //aec
    p->aec_process_maxfrequency = cfg->aec.aec_process_maxfrequency;
    p->aec_process_minfrequency = cfg->aec.aec_process_minfrequency;
    //nlp
    p->nlp_process_maxfrequency = cfg->nlp.nlp_process_maxfrequency;
    p->nlp_process_minfrequency = cfg->nlp.nlp_process_minfrequency;
    p->overdrive = cfg->nlp.overdrive;

    if (!(algo_type & CVP_ALGO_1MIC_AECNLP)) {
        //dns
        p->aggressfactor = cfg->dns.aggressfactor;
        p->minsuppress = cfg->dns.minsuppress;
        //drc
        p->noisegatethresholdDb = cfg->drc.noisegatethresholdDb;
        p->makeupGain = cfg->drc.makeupGain;
        p->kneethresholdDb = cfg->drc.kneethresholdDb;
    }
#if (TCFG_CVP_ALGO_TYPE > 0xF)
    //双麦 三麦
    if (!(algo_type & CVP_ALGO_2MIC_FLEXIBLE)) {
        if ((algo_type & CVP_TYPE_2MIC) || (algo_type & CVP_ALGO_3MIC)) {
            //enc
            p->enc_process_maxfreq = cfg->enc.enc_process_maxfreq;
            p->enc_process_minfreq = cfg->enc.enc_process_minfreq;
            p->sir_maxfreq = cfg->enc.sir_maxfreq;
            p->mic_distance = cfg->enc.mic_distance / 1000.0f;	//mm换算成m
            p->target_signal_degradation = eq_db2mag(cfg->enc.target_signal_degradation);	//dB转浮点
            p->enc_aggressfactor = cfg->enc.enc_aggressfactor;
            p->enc_minsuppress = cfg->enc.enc_minsuppress;
        }
        //双麦三麦有wnc mfdt
        if ((algo_type & CVP_TYPE_2MIC) || (algo_type & CVP_ALGO_3MIC)) {
            //wnc
            p->windProbHighTh = cfg->wnc.windProbHighTh;
            p->windProbLowTh = cfg->wnc.windProbLowTh;
            p->windEngDbTh = cfg->wnc.windEngDbTh;
            //mfdt
            p->detect_time = cfg->mfdt.detect_time;            // in second
            p->detect_eng_diff_thr = cfg->mfdt.detect_eng_diff_thr;     //  dB
            p->detect_eng_lowerbound = cfg->mfdt.detect_eng_lowerbound; // 0~-90 dB start detect when mic energy lower than this
            p->MalfuncDet_MaxFrequency = cfg->mfdt.MalfuncDet_MaxFrequency;  //检测频率上限
            p->MalfuncDet_MinFrequency = cfg->mfdt.MalfuncDet_MinFrequency;   //检测频率下限
            p->OnlyDetect = cfg->mfdt.OnlyDetect;// 0 -> 故障切换到单mic模式， 1-> 只检测不切换
        }
    }
#if (CVP_V3_2MIC_FLEXIBLE_ENABLE)
    //双麦话务adaptive
    if (algo_type & CVP_ALGO_2MIC_FLEXIBLE) {
        p->flexible_mode = cfg->adaptive.flex_mode;
        p->adaptive_processMaxFrequency	= cfg->adaptive.adaptive_processMaxFrequency;
        p->adaptive_processMinFrequency = cfg->adaptive.adaptive_processMinFrequency;
        p->cohefMaxGamma = cfg->adaptive.cohefMaxGamma;
        p->varmuMaxGamma = cfg->adaptive.varmuMaxGamma;
        p->sirMaxGamma	 = cfg->adaptive.sirMaxGamma;
        p->useSirGain	 = cfg->adaptive.useSirGain;
    }
#endif

#if (CVP_V3_3MIC_FUSION_ENABLE)
    if (g_cvp_hdl->algo_sel.algo_type & CVP_ALGO_3MIC) {
        p->use_au = cfg->doa.use_au;
        p->degree_start = cfg->doa.degree_start;
        p->degree_end = cfg->doa.degree_end;
    }
#endif
#endif
    //flow
    p->preGainDb = cfg->flow.preGainDb;
    if (!(algo_type & CVP_ALGO_1MIC_AECNLP)) {
        p->CompenDb = cfg->flow.CompenDb;
    }
    //output sel
    p->output_sel = cfg->debug.output_sel;
}

static struct cvp_cfg_t global_cvp_cfg;
int cvp_v3_param_cfg_read(void)
{
    u8 subid;
    if (g_cvp_hdl) {
        subid = hdl_node(g_cvp_hdl)->subid;
    } else {
        subid = 0XFF;
    }
    /*
     *解析配置文件内效果配置
     * */
    int len = 0;
    struct node_param ncfg = {0};
    len = jlstream_read_node_data(NODE_UUID_CVP_V3, subid, (u8 *)&ncfg);
    if (len != sizeof(ncfg)) {
        printf("cvp_tms_name read ncfg err\n");
        return -2;
    }
    char mode_index = 0;
    char cfg_index = 0;//目标配置项序号
    struct cfg_info info = {0};
    if (!jlstream_read_form_node_info_base(mode_index, ncfg.name, cfg_index, &info)) {
        len = jlstream_read_form_cfg_data(&info, &global_cvp_cfg);
    }

    printf(" %s len %d, sizeof(global_cvp_cfg) %d\n", __func__,  len, (int)sizeof(global_cvp_cfg));
    if (len != sizeof(global_cvp_cfg)) {
        printf("cvp_v3_param read ncfg err\n");
        return -1 ;
    }
    return 0;
}

u8 cvp_v3_get_talk_mic_ch(void)
{
    return global_cvp_cfg.mic_sel.talk_mic;
}

u8 cvp_v3_get_talk_ref_mic_ch(void)
{
    return global_cvp_cfg.mic_sel.talk_ref_mic;
}

u8 cvp_v3_get_talk_fb_mic_ch(void)
{
    return global_cvp_cfg.mic_sel.talk_fb_mic;
}

int cvp_get_algo_type(void)
{
    if (g_cvp_hdl) {
        return  g_cvp_hdl->algo_sel.algo_type;
    } else {
        return global_cvp_cfg.algo_sel.algo_type;
    }
}

__CVP_BANK_CODE
int cvp_v3_node_param_cfg_read(void *priv, u8 ignore_subid, u16 algo_uuid)
{
    CVP_CONFIG *p = (CVP_CONFIG *)priv;
    struct cvp_cfg_t cfg;
    u8 subid;
    if (g_cvp_hdl) {
        subid = hdl_node(g_cvp_hdl)->subid;
    } else {
        subid = 0XFF;
    }
    /*
     *解析配置文件内效果配置
     * */
    struct node_param ncfg = {0};
    printf("cvp_v3_node read,subid=0x%x\n", subid);
    int len = jlstream_read_node_data(NODE_UUID_CVP_V3, subid, (u8 *)&ncfg);
    if (len != sizeof(ncfg)) {
        printf("cvp_v3_node read ncfg err\n");
        return 0;
    }

    char mode_index = 0;
    char cfg_index = 0;//目标配置项序号
    struct cfg_info info = {0};
    if (!jlstream_read_form_node_info_base(mode_index, ncfg.name, cfg_index, &info)) {
        len = jlstream_read_form_cfg_data(&info, &cfg);
    }
    if (len != sizeof(cfg)) {
        printf("%s error, len %d, sizeof(cfg) %d\n", __func__,  len, (int)sizeof(cfg));
        return 0 ;
    }

    /*
     *获取在线调试的临时参数
     * */
    if (config_audio_cfg_online_enable) {
        if (g_cvp_hdl) {
            memcpy(g_cvp_hdl->name, ncfg.name, sizeof(ncfg.name));
            if (jlstream_read_effects_online_param(hdl_node(g_cvp_hdl)->uuid, g_cvp_hdl->name, &cfg, sizeof(cfg))) {
                printf("get cvp online param\n");
            }
        }
    }
    cvp_v3_node_param_cfg_update(&cfg, p);

    return sizeof(CVP_CONFIG);
}

/*节点输出回调处理，可处理数据或post信号量*/
__NODE_CACHE_CODE(cvp_v3)
static void cvp_handle_frame(struct stream_iport *iport, struct stream_note *note)
{
    struct cvp_node_hdl *hdl = (struct cvp_node_hdl *)iport->node->private_data;
    struct stream_node *node = iport->node;
    //cppcheck-suppress unusedVariable
    s16 *dat, *mic_buf, *ref_buf, *tbuf_0, *tbuf_1, *tbuf_2, *tbuf_3;
    int wlen;
    u8 mic_index = 0, ref_index[2] = {0};
    struct stream_frame *in_frame;
    u8 mic_ch = audio_adc_file_get_mic_en_map();

    while (1) {
        in_frame = jlstream_pull_frame(iport, note);		//从iport读取数据
        if (!in_frame) {
            break;
        }
#if TCFG_AUDIO_DUT_ENABLE
        //产测bypass 模式 不经过算法
        if (cvp_dut_mode_get() == CVP_DUT_MODE_BYPASS) {
            jlstream_push_frame(node->oport, in_frame);
            continue;
        }
#endif
        hdl->lock();
        if (hdl->ref_mic.en) { //参考数据硬回采
            if ((g_cvp_hdl->algo_sel.algo_type & CVP_ALGO_1MIC) || (g_cvp_hdl->algo_sel.algo_type & CVP_ALGO_1MIC_AECNLP)) {
                wlen = in_frame->len / ((1 + hdl->ref_mic_num) * 2);	//一个ADC的点数
                //模仿ADCbuff的存储方法
                mic_buf = hdl->buf + (wlen * hdl->buf_cnt);
                ref_buf = hdl->buf_1 + (wlen * hdl->buf_cnt * hdl->ref_mic_num); // 耳机硬回采 ref_mic_num :1  音箱立体声 ref_mic_num : 2
            } else if (g_cvp_hdl->algo_sel.algo_type & CVP_TYPE_2MIC) {
                wlen = in_frame->len / 3 / 2;	//一个ADC的点数
                //模仿ADCbuff的存储方法
                tbuf_0 = hdl->buf + (wlen * hdl->buf_cnt);
                tbuf_1 = hdl->buf_1 + (wlen * hdl->buf_cnt);
                tbuf_2 = hdl->buf_2 + (wlen * hdl->buf_cnt);
            } else {
                wlen = in_frame->len / 4 / 2;	//一个ADC的点数
                //模仿ADCbuff的存储方法
                tbuf_0 = hdl->buf + (wlen * hdl->buf_cnt);
                tbuf_1 = hdl->buf_1 + (wlen * hdl->buf_cnt);
                tbuf_2 = hdl->buf_2 + (wlen * hdl->buf_cnt);
                tbuf_3 = hdl->buf_3 + (wlen * hdl->buf_cnt);
            }

            //common
            if (++hdl->buf_cnt > ((CVP_INPUT_SIZE / 256) - 1)) {
                hdl->buf_cnt = 0;
            }
#if CVP_V3_1MIC_ALGO_ENABLE
            if ((g_cvp_hdl->algo_sel.algo_type & CVP_ALGO_1MIC) || (g_cvp_hdl->algo_sel.algo_type & CVP_ALGO_1MIC_AECNLP)) {	//1mic or aecnlp
                u8 mic_cnt = 0, ref_cnt = 0;
                for (int i = 0; i < AUDIO_ADC_MIC_MAX_NUM; i++) {
                    if (hdl->ref_mic.ref_mic_ch & BIT(i)) {
                        ref_index[ref_cnt++] = mic_cnt++;
                        continue;
                    }
                    if (hdl->mic_sel.talk_mic & BIT(i)) {
                        mic_index = mic_cnt++;
                        continue;
                    }
                }
                dat = (s16 *)in_frame->data;
                if (hdl->ref_mic_num == 1) {
                    /*分类2个mic的数据*/
                    for (int i = 0; i < wlen; i++) {
                        mic_buf[i] = dat[2 * i + mic_index];
                        ref_buf[i] = dat[2 * i + ref_index[0]];
                    }

                } else if (hdl->ref_mic_num == 2) {
                    /*分类3个mic的数据*/
                    for (int i = 0; i < wlen; i++) {
                        mic_buf[i]          = dat[3 * i + mic_index];
                        ref_buf[2 * i]      = dat[3 * i + ref_index[0]];
                        ref_buf[2 * i + 1]  = dat[3 * i + ref_index[1]];
                    }
                }
                audio_cvp_v3_spk_data_push(ref_buf, NULL, wlen << hdl->ref_mic_num);
                audio_cvp_v3_talk_mic_push(mic_buf, wlen << 1);
                hdl->unlock();
            }
#endif
#if CVP_V3_2MIC_ALGO_ENABLE
            if (g_cvp_hdl->algo_sel.algo_type & CVP_TYPE_2MIC) { //2mic
                /*拆分mic数据*/
                dat = (s16 *)in_frame->data;
                for (int i = 0; i < wlen; i++) {
                    tbuf_0[i] = dat[3 * i];
                    tbuf_1[i] = dat[3 * i + 1];
                    tbuf_2[i] = dat[3 * i + 2];
                }
                u8 cnt = 0;
                u8 talk_data_num = 0;//记录通话MIC数据位置
                s16 *mic_data[3];
                mic_data[0] = tbuf_0;
                mic_data[1] = tbuf_1;
                mic_data[2] = tbuf_2;
                for (int i = 0; i < AUDIO_ADC_MIC_MAX_NUM; i++) {
                    if ((mic_ch & BIT(i)) == hdl->ref_mic.ref_mic_ch) {
                        audio_cvp_v3_spk_data_push(mic_data[cnt++], NULL, wlen << 1);
                        continue;
                    }
                    if ((mic_ch & BIT(i)) == hdl->mic_sel.talk_mic) {
                        talk_data_num = cnt++;
                        continue;
                    }
                    if ((mic_ch & BIT(i)) == hdl->mic_sel.talk_ref_mic) {
                        audio_cvp_v3_ff_mic_push(mic_data[cnt++], wlen << 1);
                        continue;
                    }
                }
                /*通话MIC数据需要最后传进算法*/
                audio_cvp_v3_talk_mic_push(mic_data[talk_data_num], wlen << 1);
                hdl->unlock();
            }
#endif
#if CVP_V3_3MIC_ALGO_ENABLE
            if (g_cvp_hdl->algo_sel.algo_type & CVP_ALGO_3MIC) { //3mic
                /*拆分mic数据*/
                dat = (s16 *)in_frame->data;
                for (int i = 0; i < wlen; i++) {
                    tbuf_0[i] = dat[4 * i];
                    tbuf_1[i] = dat[4 * i + 1];
                    tbuf_2[i] = dat[4 * i + 2];
                    tbuf_3[i] = dat[4 * i + 3];
                }
                u8 cnt = 0;
                u8 talk_data_num = 0;//记录通话MIC数据位置
                s16 *mic_data[4];
                mic_data[0] = tbuf_0;
                mic_data[1] = tbuf_1;
                mic_data[2] = tbuf_2;
                mic_data[3] = tbuf_3;
                for (int i = 0; i < AUDIO_ADC_MIC_MAX_NUM; i++) {
                    if ((mic_ch & BIT(i)) == hdl->ref_mic.ref_mic_ch) {
                        audio_cvp_v3_spk_data_push(mic_data[cnt++], NULL, wlen << 1);
                        continue;
                    }
                    if ((mic_ch & BIT(i)) == hdl->mic_sel.talk_mic) {
                        talk_data_num = cnt++;
                        continue;
                    }
                    if ((mic_ch & BIT(i)) == hdl->mic_sel.talk_fb_mic) {
                        audio_cvp_v3_fb_mic_push(mic_data[cnt++], wlen << 1);
                        continue;
                    }
                    if ((mic_ch & BIT(i)) == hdl->mic_sel.talk_ref_mic) {
                        audio_cvp_v3_ff_mic_push(mic_data[cnt++], wlen << 1);
                        continue;
                    }
                }
                /*通话MIC数据需要最后传进算法*/
                audio_cvp_v3_talk_mic_push(mic_data[talk_data_num], wlen << 1);
                hdl->unlock();
            }
#endif
        } else {
#if CVP_V3_1MIC_ALGO_ENABLE
            if ((g_cvp_hdl->algo_sel.algo_type & CVP_ALGO_1MIC) || (g_cvp_hdl->algo_sel.algo_type & CVP_ALGO_1MIC_AECNLP)) { 	//1mic or aecnlp
                dat = hdl->buf + (in_frame->len / 2 * hdl->buf_cnt);
                memcpy((u8 *)dat, in_frame->data, in_frame->len);
                audio_cvp_v3_talk_mic_push(dat, in_frame->len);
                if (++hdl->buf_cnt > ((CVP_INPUT_SIZE / 256) - 1)) {
                    hdl->buf_cnt = 0;
                }
                hdl->unlock();
            }
#endif
#if CVP_V3_2MIC_ALGO_ENABLE
            if (g_cvp_hdl->algo_sel.algo_type & CVP_TYPE_2MIC) { //2mic
                u8 talk_data_num = 0;
                u8 cnt = 0;
                wlen = in_frame->len >> 2;	//一个ADC的点数
                //模仿ADCbuff的存储方法
                tbuf_0 = hdl->buf + (wlen * hdl->buf_cnt);
                tbuf_1 = hdl->buf_1 + (wlen * hdl->buf_cnt);
                if (++hdl->buf_cnt > ((CVP_INPUT_SIZE / 256) - 1)) {
                    hdl->buf_cnt = 0;
                }
                dat = (s16 *)in_frame->data;
                for (int i = 0; i < wlen; i++) {
                    tbuf_0[i]     = dat[2 * i];
                    tbuf_1[i] 	  = dat[2 * i + 1];
                }
                s16 *mic_data[3];
                mic_data[0] = tbuf_0;
                mic_data[1] = tbuf_1;

                for (int i = 0; i < AUDIO_ADC_MIC_MAX_NUM; i++) {
                    if ((mic_ch & BIT(i)) == hdl->mic_sel.talk_mic) {
                        talk_data_num = cnt++;
                        continue;
                    }
                    if ((mic_ch & BIT(i)) == hdl->mic_sel.talk_ref_mic) {
                        audio_cvp_v3_ff_mic_push(mic_data[cnt++], wlen << 1);
                        continue;
                    }
                }
                /*通话MIC数据需要最后传进算法*/
                audio_cvp_v3_talk_mic_push(mic_data[talk_data_num], wlen << 1);
                hdl->unlock();
            }
#endif
#if CVP_V3_3MIC_ALGO_ENABLE
            if (g_cvp_hdl->algo_sel.algo_type & CVP_ALGO_3MIC) { //3mic
                wlen = in_frame->len / 3 / 2;	//一个ADC的点数
                //模仿ADCbuff的存储方法
                tbuf_0 = hdl->buf + (wlen * hdl->buf_cnt);
                tbuf_1 = hdl->buf_1 + (wlen * hdl->buf_cnt);
                tbuf_2 = hdl->buf_2 + (wlen * hdl->buf_cnt);
                if (++hdl->buf_cnt > ((CVP_INPUT_SIZE / 256) - 1)) {
                    hdl->buf_cnt = 0;
                }
                /*拆分mic数据*/
                dat = (s16 *)in_frame->data;
                for (int i = 0; i < wlen; i++) {
                    tbuf_0[i] = dat[3 * i];
                    tbuf_1[i] = dat[3 * i + 1];
                    tbuf_2[i] = dat[3 * i + 2];
                }
                u8 cnt = 0;
                u8 talk_data_num = 0;//记录通话MIC数据位置
                s16 *mic_data[3];
                mic_data[0] = tbuf_0;
                mic_data[1] = tbuf_1;
                mic_data[2] = tbuf_2;
                for (int i = 0; i < AUDIO_ADC_MIC_MAX_NUM; i++) {
                    if ((mic_ch & BIT(i)) == hdl->mic_sel.talk_mic) {
                        talk_data_num = cnt++;
                        continue;
                    }
                    if ((mic_ch & BIT(i)) == hdl->mic_sel.talk_fb_mic) {
                        audio_cvp_v3_fb_mic_push(mic_data[cnt++], wlen << 1);
                        continue;
                    }
                    if ((mic_ch & BIT(i)) == hdl->mic_sel.talk_ref_mic) {
                        audio_cvp_v3_ff_mic_push(mic_data[cnt++], wlen << 1);
                        continue;
                    }
                }
                /*通话MIC数据需要最后传进算法*/
                audio_cvp_v3_talk_mic_push(mic_data[talk_data_num], wlen << 1);
                hdl->unlock();
            }
#endif
        }
        jlstream_free_frame(in_frame);	//释放iport资源
    }
}

static int cvp_lock_init(struct cvp_node_hdl *hdl)
{
    int ret = false;
    u16 node_uuid = hdl_node(hdl)->uuid;
    if (hdl) {
        switch (node_uuid) {
        case NODE_UUID_CVP_V3:
            hdl->lock   = audio_cvp_lock;
            hdl->unlock = audio_cvp_unlock;
            break;
        default:
            ASSERT(0, "cvp_lock_init: unknown node UUID \n");
            break;
        }
        ret = true;
    }
    return ret;
}

/*节点预处理-在ioctl之前*/
static int cvp_adapter_bind(struct stream_node *node, u16 uuid)
{
    struct cvp_node_hdl *hdl = (struct cvp_node_hdl *)node->private_data ;
    /*初始化spinlock锁*/
    cvp_lock_init(hdl);
    node->type = NODE_TYPE_ASYNC;
    g_cvp_hdl = hdl;

    cvp_node_context_setup(uuid);
    return 0;
}

/*打开改节点输入接口*/
static void cvp_ioc_open_iport(struct stream_iport *iport)
{
}

/*节点参数协商*/
static int cvp_ioc_negotiate(struct stream_iport *iport)
{
    struct stream_fmt *in_fmt = &iport->prev->fmt;
    struct stream_oport *oport = iport->node->oport;
    int ret = NEGO_STA_ACCPTED;
    int nb_sr, wb_sr, nego_sr;

#if (TCFG_AUDIO_CVP_BAND_WIDTH_CFG == CVP_WB_EN)
    nb_sr = 16000;
    wb_sr = 16000;
    nego_sr  = 16000;
#elif (TCFG_AUDIO_CVP_BAND_WIDTH_CFG == CVP_NB_EN)
    nb_sr = 8000;
    wb_sr = 8000;
    nego_sr  = 8000;
#else
    nb_sr = 8000;
    wb_sr = 16000;
    nego_sr  = 16000;
#endif
    //要求输入为8K或者16K
    if (in_fmt->sample_rate != nb_sr && in_fmt->sample_rate != wb_sr) {
        in_fmt->sample_rate = nego_sr;
        oport->fmt.sample_rate = in_fmt->sample_rate;
        ret = NEGO_STA_CONTINUE | NEGO_STA_SAMPLE_RATE_LOCK;
    }

    //要求输入16bit位宽的数据
    if (in_fmt->bit_wide != DATA_BIT_WIDE_16BIT) {
        in_fmt->bit_wide = DATA_BIT_WIDE_16BIT;
        in_fmt->Qval = AUDIO_QVAL_16BIT;
        oport->fmt.bit_wide = in_fmt->bit_wide;
        oport->fmt.Qval = in_fmt->Qval;
        ret = NEGO_STA_CONTINUE;
    }

    //固定输出单声道数据
    oport->fmt.channel_mode = AUDIO_CH_MIX;
    return ret;
}

/*节点start函数*/
__CVP_BANK_CODE
static void cvp_ioc_start(struct cvp_node_hdl *hdl)
{
    struct stream_fmt *fmt = &hdl_node(hdl)->oport->fmt;
    struct audio_aec_init_param_t init_param;
    init_param.sample_rate = fmt->sample_rate;
    init_param.ref_sr = hdl->ref_sr;
    init_param.ref_channel = hdl->ref_mic_num;
    init_param.node_uuid = hdl_node(hdl)->uuid;
    u8 mic_num; //算法需要使用的MIC个数

    audio_cvp_v3_init(&init_param);
#if TCFG_AUDIO_ANC_ENABLE
    audio_cvp_v3_set_anc_mode(anc_mode_get());
#endif
    if (hdl->source_uuid == NODE_UUID_ADC) {
        if (hdl->ref_mic.en) {
            if ((g_cvp_hdl->algo_sel.algo_type & CVP_ALGO_1MIC) || (g_cvp_hdl->algo_sel.algo_type & CVP_ALGO_1MIC_AECNLP)) {  //1mic or aecnlp
                /*1MIC 硬回采需要开1个MIC*/
                mic_num = 1 + hdl->ref_mic_num;
            } else if (g_cvp_hdl->algo_sel.algo_type & CVP_TYPE_2MIC) {
                /*2MIC 硬回采需要开3个MIC*/
                mic_num = 3;
            } else {
                /*3MIC 硬回采需要开4个MIC*/
                mic_num = 4;
            }
        } else {
            if ((g_cvp_hdl->algo_sel.algo_type & CVP_ALGO_1MIC) || (g_cvp_hdl->algo_sel.algo_type & CVP_ALGO_1MIC_AECNLP)) {  //1mic or aecnlp
                /*1MIC 软回采需要开1个MIC*/
                mic_num = 1;
            } else if (g_cvp_hdl->algo_sel.algo_type & CVP_TYPE_2MIC) {
                /*2MIC 软回采需要开2个MIC*/
                mic_num = 2;
            } else {
                /*3MIC 软回采需要开3个MIC*/
                mic_num = 3;
            }
        }
        if (hdl->adc_ch_num != mic_num) {
#if TCFG_AUDIO_DUT_ENABLE
            //使能产测时，只有算法模式才需判断
            if (cvp_dut_mode_get() == CVP_DUT_MODE_ALGORITHM) {
                ASSERT(0, "CVP_DMS, ESCO MIC num is %d != %d\n", hdl->adc_ch_num, mic_num);
            }
#else
            ASSERT(0, "CVP_DMS, ESCO MIC num is %d != %d\n", hdl->adc_ch_num, mic_num);
#endif
        }
    }

}

/*节点stop函数*/
__CVP_BANK_CODE
static void cvp_ioc_stop(struct cvp_node_hdl *hdl)
{
    if (hdl) {
        audio_cvp_v3_close();
    }
}

__CVP_BANK_CODE
static int cvp_ioc_update_parm(struct cvp_node_hdl *hdl, int parm)
{
    int ret = false;
    struct cvp_cfg_t *cfg = (struct cvp_cfg_t *)parm;
    if (hdl) {
        cvp_v3_node_param_cfg_update(cfg, &hdl->online_cfg);
        cvp_cfg_update(&hdl->online_cfg);
        ret = true;
    }
    return ret;
}

/*节点ioctl函数*/
static int cvp_adapter_ioctl(struct stream_iport *iport, int cmd, int arg)
{
    int ret = 0;
    struct cvp_node_hdl *hdl = (struct cvp_node_hdl *)iport->node->private_data;

    switch (cmd) {
    case NODE_IOC_OPEN_IPORT:
        cvp_ioc_open_iport(iport);
        break;
    case NODE_IOC_OPEN_OPORT:
        break;
    case NODE_IOC_CLOSE_IPORT:
        break;
    case NODE_IOC_NEGOTIATE:
        *(int *)arg |= cvp_ioc_negotiate(iport);
        break;
    case NODE_IOC_SET_FMT:
        hdl->ref_sr = (u32)arg;
        break;
    case NODE_IOC_SET_SCENE:
        hdl->scene = arg;
        break;
    case NODE_IOC_START:
        cvp_ioc_start(hdl);
        break;
    case NODE_IOC_SUSPEND:
    case NODE_IOC_STOP:
        cvp_ioc_stop(hdl);
        break;
    case NODE_IOC_NAME_MATCH:
        if (!strcmp((const char *)arg, hdl->name)) {
            ret = 1;
        }
        break;
    case NODE_IOC_SET_PARAM:
#if (TCFG_CFG_TOOL_ENABLE || TCFG_AEC_TOOL_ONLINE_ENABLE)
        ret = cvp_ioc_update_parm(hdl, arg);
#endif
        break;
    case NODE_IOC_SET_PRIV_FMT:
        struct cvp_param_fmt *fmt = (struct cvp_param_fmt *)arg;
        hdl->source_uuid = fmt->source_uuid;
        hdl->adc_ch_num = fmt->mic_num;
        printf("source_uuid %x adc_ch_num %d", (int)hdl->source_uuid, hdl->adc_ch_num);
        break;
    }

    return ret;
}

/*节点用完释放函数*/
static void cvp_adapter_release(struct stream_node *node)
{
    if (g_cvp_hdl->buf_3) {
        free(g_cvp_hdl->buf_3);
        g_cvp_hdl->buf_3 = NULL;
    }
    g_cvp_hdl = NULL;
    cvp_node_context_setup(0);
}

/*节点adapter 注意需要在sdk_used_list声明，否则会被优化*/
REGISTER_STREAM_NODE_ADAPTER(cvp_v3_node_adapter) = {
    .uuid       = NODE_UUID_CVP_V3,
    .bind       = cvp_adapter_bind,
    .ioctl      = cvp_adapter_ioctl,
    .release    = cvp_adapter_release,
    .handle_frame = cvp_handle_frame,				//注册输出回调
    .hdl_size   = sizeof(struct cvp_node_hdl),
    .ability_channel_convert = 1, //支持声道转换
};

//注册工具在线调试
REGISTER_ONLINE_ADJUST_TARGET(cvp_v3) = {
    .uuid       = NODE_UUID_CVP_V3,
};

#endif/* TCFG_AUDIO_CVP_ALGO_3MIC_MODE */

