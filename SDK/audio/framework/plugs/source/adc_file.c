#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".adc_file.data.bss")
#pragma data_seg(".adc_file.data")
#pragma const_seg(".adc_file.text.const")
#pragma code_seg(".adc_file.text")
#endif
#include "source_node.h"
#include "audio_adc.h"
#include "audio_config.h"
#include "adc_file.h"
#include "gpio_config.h"
#include "effects/effects_adj.h"
#include "audio_cvp.h"
#include "asm/audio_common.h"
#include "mic_effect.h"
#include "pc_mic_recoder.h"
#include "btstack/avctp_user.h"
#include "effects/audio_howling_ahs.h"
#include "audio_anc_includes.h"
#if TCFG_AUDIO_DUT_ENABLE
#include "audio_dut_control.h"
#endif

#if TCFG_AUDIO_DUT_ENABLE
#include "audio_dut_control.h"
#endif
#if TCFG_AUDIO_ADC_ENABLE

#if TCFG_AUDIO_AVC_NODE_ENABLE
#include "audio_avc.h"
#endif

#if 1
#define adc_file_log	printf
#else
#define adc_file_log(...)
#endif/*log_en*/

extern struct audio_adc_hdl adc_hdl;
extern const u8 const_adc_async_en;
extern const struct adc_platform_cfg adc_platform_cfg_table[AUDIO_ADC_MAX_NUM];

#define ESCO_ADC_BUF_NUM        2	//mic_adc采样buf个数

struct adc_file_common ;
struct adc_file_hdl {
    char name[16];
    void *source_node;
    struct stream_node *node;
    enum stream_scene scene;
    struct adc_mic_ch mic_ch;
    struct audio_adc_output_hdl adc_output;
    s16 *adc_buf;
    struct adc_file_common *adc_f;
    int force_dump;
    int value[4];
    u32 sample_rate;
    u16 irq_points;
    u8 adc_seq;
    u8 channel_mode;
    u8 Qval;
    u8 start;
    u8 dump_cnt;
    u8 ch_num;
    u8 multi_ch_adc_en;
};

struct adc_file_common {
    u8 read_flag;				//读取配置的标志
    struct adc_file_hdl *hdl;	//当前ADC节点的句柄
    struct adc_file_cfg cfg;	//ADC的参数信息
    struct adc_platform_cfg platform_cfg[AUDIO_ADC_MAX_NUM];
};

struct adc_file_global {
    u8 fixed_ch_num;
    s16 *fixed_buf;				//固定的ADCbuffer
    struct adc_file_cfg cfg;	//ESCO ADC的参数信息
};

static struct adc_file_common esco_adc_f;
static struct adc_file_global esco_adc_file_g;
static u8 adc_file_global_open_cnt = 0;

/*判断adc 节点是否再跑*/
u8 adc_file_is_runing(void)
{
    return adc_file_global_open_cnt;
}


#if TCFG_MULTI_CH_ADC_NODE_ENABLE
__NODE_CACHE_CODE(adc)
static void audio_multi_ch_adc_fade_in(struct adc_file_hdl *hdl, void *buf, int len, u8 ch_idx)
{
    if (hdl->value[ch_idx] < FADE_GAIN_MAX) {
        int fade_ms = 100;//ms
        int fade_step = FADE_GAIN_MAX / (fade_ms * hdl->sample_rate / 1000);
        if (adc_hdl.bit_width == ADC_BIT_WIDTH_16) {
            hdl->value[ch_idx]  = jlstream_fade_in(hdl->value[ch_idx], fade_step, buf, len, AUDIO_CH_NUM(hdl->channel_mode));
        } else {
            hdl->value[ch_idx]  = jlstream_fade_in_32bit(hdl->value[ch_idx], fade_step, buf, len, AUDIO_CH_NUM(hdl->channel_mode));
        }
    }
}

/**
 * @brief       MIC 的中断回调函数
 *
 * @param _hdl  MIC 节点的操作句柄
 * @param data  MIC 中断采集到的数据地址
 * @param len   MIC 单个通道中断采集到的数据字节长度
 */
__NODE_CACHE_CODE(adc)
static void adc_mic_ch_output_handler(void *_hdl, s16 *data, int len, u8 ch_idx)
{
    struct adc_file_hdl *hdl = (struct adc_file_hdl *)_hdl;
    struct stream_frame *frame;
    frame = source_plug_get_output_frame_by_id(hdl->source_node, ch_idx, len);
    if (!frame) {
        return;
    }

    u8 data_ch_idx = get_adc_seq(&adc_hdl, BIT(ch_idx)); //查询模拟mic对应的ADC通道
    if (hdl->force_dump) {
        hdl->value[ch_idx] = 0;
        return;
    }
    if (adc_hdl.bit_width != ADC_BIT_WIDTH_16) {
        s32 *s32_src = (s32 *)data;
        s32 *s32_dst = (s32 *)frame->data;
        int points = len >> 2;
        for (int i = 0; i < points; i++) {
            s32_dst[i] = s32_src[adc_hdl.max_adc_num * i + data_ch_idx];
        }
    } else {
        s16 *s16_src = data;
        s16 *s16_dst = (s16 *)frame->data;
        int points = len >> 1;
        for (int i = 0; i < points; i++) {
            s16_dst[i] = s16_src[adc_hdl.max_adc_num * i + data_ch_idx];
        }
    }
    if (audio_common_mic_mute_en_get()) {	//mute ADC
        memset((u8 *)frame->data, 0x0, len);
    }
    frame->len          = len;
    frame->flags        = FRAME_FLAG_TIMESTAMP_ENABLE | FRAME_FLAG_PERIOD_SAMPLE | FRAME_FLAG_UPDATE_TIMESTAMP;
    frame->timestamp    = adc_hdl.timestamp * TIMESTAMP_US_DENOMINATOR;
    audio_multi_ch_adc_fade_in(hdl, frame->data, frame->len, ch_idx);
    source_plug_put_output_frame_by_id(hdl->source_node, ch_idx, frame);
}

static void adc_ch0_handle(void *priv, void *addr, int len)	//中断回调
{
    adc_mic_ch_output_handler(priv, addr, len, 0);
}
static  void adc_ch1_handle(void *priv, void *addr, int len)	//中断回调
{
    adc_mic_ch_output_handler(priv, addr, len, 1);
}
static  void adc_ch2_handle(void *priv, void *addr, int len)	//中断回调
{
    adc_mic_ch_output_handler(priv, addr, len, 2);
}
static void adc_ch3_handle(void *priv, void *addr, int len)	//中断回调
{
    adc_mic_ch_output_handler(priv, addr, len, 3);
}
static int adc_chx_handle[4] = {(int)adc_ch0_handle, (int)adc_ch1_handle, (int)adc_ch2_handle, (int)adc_ch3_handle};


__NODE_CACHE_CODE(adc)
static void multi_ch_adc_mic_output_handler(void *_hdl, s16 *data, int len)
{
    void (*handler)(void *, void *, int);
    struct adc_file_hdl *hdl = (struct adc_file_hdl *)_hdl;
    if (!hdl) {
        return;
    }
    if (hdl->dump_cnt < 10) {
        hdl->dump_cnt++;
        return;
    }
    for (int i = 0; i < AUDIO_ADC_MAX_NUM; i++) {
        if (hdl->adc_f->cfg.mic_en_map & BIT(i)) {
            handler = (void (*)(void *, void *, int))adc_chx_handle[i];
            handler(_hdl, data, len);
        }
    }
}
#endif




#if TCFG_MULTI_CH_ADC_NODE_ENABLE
__NODE_CACHE_CODE(adc)
static void audio_multi_ch_adc_fade_in(struct adc_file_hdl *hdl, void *buf, int len, u8 ch_idx)
{
    if (hdl->value[ch_idx] < FADE_GAIN_MAX) {
        int fade_ms = 100;//ms
        int fade_step = FADE_GAIN_MAX / (fade_ms * hdl->sample_rate / 1000);
        if (adc_hdl.bit_width == ADC_BIT_WIDTH_16) {
            hdl->value[ch_idx]  = jlstream_fade_in(hdl->value[ch_idx], fade_step, buf, len, AUDIO_CH_NUM(hdl->channel_mode));
        } else {
            hdl->value[ch_idx]  = jlstream_fade_in_32bit(hdl->value[ch_idx], fade_step, buf, len, AUDIO_CH_NUM(hdl->channel_mode));
        }
    }
}

/**
 * @brief       MIC 的中断回调函数
 *
 * @param _hdl  MIC 节点的操作句柄
 * @param data  MIC 中断采集到的数据地址
 * @param len   MIC 单个通道中断采集到的数据字节长度
 */
__NODE_CACHE_CODE(adc)
static void adc_mic_ch_output_handler(void *_hdl, s16 *data, int len, u8 ch_idx)
{
    struct adc_file_hdl *hdl = (struct adc_file_hdl *)_hdl;
    struct stream_frame *frame;
    frame = source_plug_get_output_frame_by_id(hdl->source_node, ch_idx, len);
    if (!frame) {
        return;
    }

    u8 data_ch_idx = get_adc_seq(&adc_hdl, BIT(ch_idx)); //查询模拟mic对应的ADC通道
    if (hdl->force_dump) {
        hdl->value[ch_idx] = 0;
        return;
    }
    if (adc_hdl.bit_width != ADC_BIT_WIDTH_16) {
        s32 *s32_src = (s32 *)data;
        s32 *s32_dst = (s32 *)frame->data;
        int points = len >> 2;
        for (int i = 0; i < points; i++) {
            s32_dst[i] = s32_src[adc_hdl.max_adc_num * i + data_ch_idx];
        }
    } else {
        s16 *s16_src = data;
        s16 *s16_dst = (s16 *)frame->data;
        int points = len >> 1;
        for (int i = 0; i < points; i++) {
            s16_dst[i] = s16_src[adc_hdl.max_adc_num * i + data_ch_idx];
        }
    }
    if (audio_common_mic_mute_en_get()) {	//mute ADC
        memset((u8 *)frame->data, 0x0, len);
    }
    frame->len          = len;
    frame->flags        = FRAME_FLAG_TIMESTAMP_ENABLE | FRAME_FLAG_PERIOD_SAMPLE | FRAME_FLAG_UPDATE_TIMESTAMP;
    frame->timestamp    = adc_hdl.timestamp * TIMESTAMP_US_DENOMINATOR;
    audio_multi_ch_adc_fade_in(hdl, frame->data, frame->len, ch_idx);
    source_plug_put_output_frame_by_id(hdl->source_node, ch_idx, frame);
}

static void adc_ch0_handle(void *priv, void *addr, int len)	//中断回调
{
    adc_mic_ch_output_handler(priv, addr, len, 0);
}
static  void adc_ch1_handle(void *priv, void *addr, int len)	//中断回调
{
    adc_mic_ch_output_handler(priv, addr, len, 1);
}
static  void adc_ch2_handle(void *priv, void *addr, int len)	//中断回调
{
    adc_mic_ch_output_handler(priv, addr, len, 2);
}
static void adc_ch3_handle(void *priv, void *addr, int len)	//中断回调
{
    adc_mic_ch_output_handler(priv, addr, len, 3);
}
static int adc_chx_handle[4] = {(int)adc_ch0_handle, (int)adc_ch1_handle, (int)adc_ch2_handle, (int)adc_ch3_handle};


__NODE_CACHE_CODE(adc)
static void multi_ch_adc_mic_output_handler(void *_hdl, s16 *data, int len)
{
    void (*handler)(void *, void *, int);
    struct adc_file_hdl *hdl = (struct adc_file_hdl *)_hdl;
    if (!hdl) {
        return;
    }
    if (hdl->dump_cnt < 10) {
        hdl->dump_cnt++;
        return;
    }
    for (int i = 0; i < AUDIO_ADC_MAX_NUM; i++) {
        if (hdl->adc_f->cfg.mic_en_map & BIT(i)) {
            handler = (void (*)(void *, void *, int))adc_chx_handle[i];
            handler(_hdl, data, len);
        }
    }
}
#endif



/*根据mic通道值获取使用的第几个mic*/
u8 audio_get_mic_index(u8 mic_ch)
{
    u8 i = 0;
    for (i = 0; i < AUDIO_ADC_MAX_NUM; i++) {
        if (mic_ch & 0x1) {
            return i;
        }
        mic_ch >>= 1;
    }
    return 0;
}

/*根据mic通道值获取使用了多少个mic*/
u8 audio_get_mic_num(u32 mic_ch)
{
    u8 mic_num = 0;
    for (int i = 0; i < 8; i++) {
        if (mic_ch & BIT(i)) {
            mic_num++;
        }
    }
    return mic_num;
}

u8 audio_max_adc_ch_num_get()
{
#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
    return adc_hdl.max_adc_num;
#else
    return 0;
#endif/*TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN*/
}

struct adc_file_cfg *audio_adc_file_get_cfg(void)
{
    return &esco_adc_f.cfg;
}

struct adc_platform_cfg *audio_adc_platform_get_cfg(void)
{
    return &esco_adc_f.platform_cfg[0];
}

u8 audio_adc_file_get_esco_mic_num(void)
{
    u8 mic_en = 0;
    for (int i = 0; i < AUDIO_ADC_MAX_NUM; i++) {
        if (esco_adc_f.cfg.mic_en_map & BIT(i)) {
            mic_en++;
        }
    }
    return mic_en;
}

u8 audio_adc_file_get_mic_en_map(void)
{
    return esco_adc_f.cfg.mic_en_map;
}

void audio_adc_file_set_mic_en_map(u8 mic_en_map)
{
    esco_adc_f.cfg.mic_en_map = mic_en_map;
}


__AUDIO_ADC_BANK_CODE
void audio_adc_file_global_cfg_init(void)
{
    memcpy(&esco_adc_file_g.cfg, &esco_adc_f.cfg, sizeof(struct adc_file_cfg));
#if TCFG_AUDIO_ANC_ENABLE &&  (defined CONFIG_CPU_BR28)
    if (!esco_adc_file_g.fixed_buf) {
        //JL701 ANC开启ADC必须固定buffer /中断点数
        if (esco_adc_file_g.cfg.mic_en_map & AUDIO_ADC_MIC_0) {
            esco_adc_file_g.fixed_ch_num++;
        }
        if (esco_adc_file_g.cfg.mic_en_map & AUDIO_ADC_MIC_1) {
            esco_adc_file_g.fixed_ch_num++;
        }
        if (esco_adc_file_g.cfg.mic_en_map & AUDIO_ADC_MIC_2) {
            esco_adc_file_g.fixed_ch_num++;
        }
        if (esco_adc_file_g.cfg.mic_en_map & AUDIO_ADC_MIC_3) {
            esco_adc_file_g.fixed_ch_num++;
        }
        if (!adc_hdl.hw_buf) {
            u8 adc_num = adc_hdl.max_adc_num;
#if defined(TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN) && TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
            adc_num = AUDIO_ADC_MAX_NUM; //按照最大通道数申请
#endif /*TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN*/
            esco_adc_file_g.fixed_buf = malloc(ESCO_ADC_BUF_NUM * 256 * ((adc_hdl.bit_width == ADC_BIT_WIDTH_16) ? 2 : 4) * adc_num);
        }
    }
#endif/*TCFG_AUDIO_ANC_ENABLE && CONFIG_CPU_BR28*/
}

void audio_adc_fixed_digital_set_buffs(void)
{
    audio_adc_mic_set_buffs(NULL, esco_adc_file_g.fixed_buf, 256 * 2, ESCO_ADC_BUF_NUM);
    audio_adc_set_buf_fix(1, &adc_hdl);
}

void audio_all_adc_file_init(void)
{
#if 0
    int subid = 0;
    char node_name[16];
    struct adc_file_cfg adc_cfg;
    for (subid = 0; subid < 0xff; subid++) { //遍历所有adc节点来获取累计开了多少路mic
        if (jlstream_read_node_data_new(NODE_UUID_ADC, subid, &adc_cfg, node_name)) {
            adc_file_log("read adc cfg :%d,%x,%s,%x\n", __LINE__, subid, node_name, adc_cfg.mic_en_map);
            for (int i = 0; i < AUDIO_ADC_MAX_NUM; i++) {
                if (adc_cfg.mic_en_map & BIT(i)) {
                    audio_adc_add_ch(&adc_hdl, i);
                }
            }
        }
    }
#else
    for (int i = 0; i < AUDIO_ADC_MAX_NUM - AUDIO_ADC_LPADC_NUM; i++) {
        //ADC复用全通道使能，默认不add LPADC，若有节点配置LPADC，在节点cfg_init内add
        audio_adc_add_ch(&adc_hdl, i);
    }
#endif
}

__AUDIO_INIT_BANK_CODE
void audio_adc_file_init(void)  //通话的ADC节点配置
{
    if (!esco_adc_f.read_flag) {
        esco_adc_f.hdl = NULL;
        /*
         *解析配置文件内效果配置
         * */
        int len = sizeof(esco_adc_f.cfg);
        char mode_index = 0;
        char cfg_index = 0;//目标配置项序号
        struct cfg_info info = {0};
        if (!jlstream_read_form_node_info_base(mode_index, "esco_adc", cfg_index, &info)) {
            jlstream_read_form_cfg_data(&info, &esco_adc_f.cfg);
        } else {
            len = 0;
        }

        if (len != sizeof(struct adc_file_cfg)) {
            printf("esco_adc_file read cfg data err !!!\n");
            /* while (1); */
        }
        memcpy(&esco_adc_f.platform_cfg, adc_platform_cfg_table, sizeof(struct adc_platform_cfg) * AUDIO_ADC_MAX_NUM);

        adc_file_log("%s len %d, sizeof(cfg) %d\n", __func__,  len, (int)sizeof(struct adc_file_cfg));

#if 0//dump出ESCO通话ADC节点参数配置
        adc_file_log("esco_adc_f.cfg.mic_en_map = 0x%x\n", esco_adc_f.cfg.mic_en_map);
        for (u8 i = 0; i < AUDIO_ADC_MAX_NUM; i++) {
            adc_file_log("esco_adc_f.cfg.mic[%d] enable = %d\n", i, !!(esco_adc_f.cfg.mic_en_map & BIT(i)));
            adc_file_log("esco_adc_f.cfg.param[%d].mic_gain      = %d\n", i, esco_adc_f.cfg.param[i].mic_gain);
            adc_file_log("esco_adc_f.cfg.param[%d].mic_pre_gain  = %d\n", i, esco_adc_f.cfg.param[i].mic_pre_gain);
        }
#endif
        esco_adc_f.read_flag = 1;

        u32 mic_ch = esco_adc_f.cfg.mic_en_map;
#if TCFG_AUDIO_ANC_ENABLE && TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
        if (const_adc_async_en) {
            /*同时开ANC和免摘功能时，配置ADC固定打开的adc数字通道数*/
            mic_ch |= icsd_get_ref_mic_L_ch() | icsd_get_fb_mic_L_ch();

            if (get_icsd_adt_mic_num() == 3) {
                mic_ch |= icsd_get_talk_mic_ch();
            }

        }
#endif /*TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN*/
#ifdef TCFG_AUDIO_ADC_ENABLE_ALL_DIGITAL_CH
        //ADC复用，在初始化阶段注册所有ADC通道
        audio_all_adc_file_init();
#else
        for (u8 i = 0; i < AUDIO_ADC_MAX_NUM; i++) {
            if (mic_ch & BIT(i)) {
                audio_adc_add_ch(&adc_hdl, i);
            }
        }
#endif
    }

#if 0//TCFG_MC_BIAS_AUTO_ADJUST
    extern u8 mic_bias_rsel_use_save[AUDIO_ADC_MAX_NUM];
    extern u8 save_mic_bias_rsel[AUDIO_ADC_MAX_NUM];
    for (u8 i = 0; i < AUDIO_ADC_MAX_NUM; i++) {
        if (mic_bias_rsel_use_save[i]) {
            esco_adc_f.platform_cfg[i].mic_bias_rsel = save_mic_bias_rsel[i];
        }
    }
#endif

    audio_adc_file_global_cfg_init();
}

__AUDIO_ADC_BANK_CODE
void audio_adc_cfg_init(struct adc_file_common *adc_f)  //通话外其他ADC节点读配置
{
    if (!adc_f->read_flag) {
        /* adc_f->hdl = NULL; */
        /*
         *解析配置文件内效果配置
         * */
        int len = sizeof(adc_f->cfg);
        char mode_index = 0;
        char cfg_index = 0;//目标配置项序号
        struct cfg_info info = {0};
        if (!jlstream_read_form_node_info_base(mode_index, adc_f->hdl->name, cfg_index, &info)) {
            jlstream_read_form_cfg_data(&info, &adc_f->cfg);
        } else {
            len = 0;
        }
        if (len != sizeof(struct adc_file_cfg)) {
            printf("adc_file read cfg data err !!!\n");
        }
        memcpy(&adc_f->platform_cfg, adc_platform_cfg_table, sizeof(struct adc_platform_cfg) * AUDIO_ADC_MAX_NUM);
        adc_file_log(" %s len %d, sizeof(cfg) %d\n", __func__,  len, (int)sizeof(struct adc_file_cfg));
        u32 i;
#if 0
        adc_file_log(" adc_f->cfg.mic_en_map = %x\n", adc_f->cfg.mic_en_map);
        for (i = 0; i < AUDIO_ADC_MAX_NUM; i++) {
            adc_file_log(" adc_f->cfg.param[%d].mic_gain      = %d\n", i, adc_f->cfg.param[i].mic_gain);
            adc_file_log(" adc_f->cfg.param[%d].mic_pre_gain  = %d\n", i, adc_f->cfg.param[i].mic_pre_gain);
        }
#endif

        adc_f->hdl->ch_num = 0;
        adc_f->read_flag = 1;
        for (i = 0; i < AUDIO_ADC_MAX_NUM; i++) {
            if (adc_f->cfg.mic_en_map & BIT(i)) {
                audio_adc_add_ch(&adc_hdl, i);
                adc_f->hdl->ch_num++;
            }
        }
    }
#if 0//TCFG_MC_BIAS_AUTO_ADJUST
    extern u8 mic_bias_rsel_use_save[AUDIO_ADC_MAX_NUM];
    extern u8 save_mic_bias_rsel[AUDIO_ADC_MAX_NUM];
    int i = 0;
    for (i = 0; i < AUDIO_ADC_MAX_NUM; i++) {
        if (mic_bias_rsel_use_save[i]) {
            adc_f->platform_cfg[i].mic_bias_rsel = save_mic_bias_rsel[i];
        }
    }
#endif
}

void audio_adc_file_set_gain(u8 mic_index, u8 mic_gain)
{
    if (mic_index > AUDIO_ADC_MAX_NUM) {
        adc_file_log("mic_index[%d] err !!!", mic_index);
    }
    esco_adc_f.cfg.param[mic_index].mic_gain = mic_gain;
    if (esco_adc_f.hdl) {
        audio_adc_mic_set_gain(&esco_adc_f.hdl->mic_ch, BIT(mic_index), mic_gain);
    }
}

u8 audio_adc_file_get_gain(u8 mic_index)
{
    audio_adc_file_init();	//如果获取的时候没有初始化，则跑初始化流程

    return esco_adc_f.cfg.param[mic_index].mic_gain;
}

u8 audio_adc_file_get_mic_mode(u8 mic_index)
{
    audio_adc_file_init();	//如果获取的时候没有初始化，则跑初始化流程
    return esco_adc_f.platform_cfg[mic_index].mic_mode;
}

__NODE_CACHE_CODE(adc)
static void adc_file_fade_in(struct adc_file_hdl *hdl, void *buf, int len)
{
    if (hdl->value[0] < FADE_GAIN_MAX) {
        int fade_ms = 100;//ms
        int fade_step = FADE_GAIN_MAX / (fade_ms * hdl->sample_rate / 1000);
        if (adc_hdl.bit_width == ADC_BIT_WIDTH_16) {
            hdl->value[0]  = jlstream_fade_in(hdl->value[0], fade_step, buf, len, AUDIO_CH_NUM(hdl->channel_mode));
        } else {
            hdl->value[0]  = jlstream_fade_in_32bit(hdl->value[0], fade_step, buf, len, AUDIO_CH_NUM(hdl->channel_mode));
        }
    }
}



/*
*********************************************************************
*                  audio_mic_data_align
* Description: 将使用的mic数据按照序号顺序，相邻的排在一起*
* Arguments  : in_data      输入数据起始地址
*              out_data     输出数据起始地址
*			   len      	1个mic通道的数据大小，byte
*			   mic_ch		使用的mic通道
*              max_mic_num  打开的mic通道数
* Return	 : null
* Note(s)    : 输入输出地址可以一样
*********************************************************************
*/
void audio_mic_data_align(s16 *in_data, s16 *out_data, int len, u32 mic_ch, u8 max_mic_num)
{
    u8 mic_num = audio_get_mic_num(mic_ch);//使用的mic个数
    /* u8 max_mic_num = AUDIO_ADC_MAX_NUM; //打开的mic个数 */
    int i, j, tmp_len;
    u8 adc_seq = 0;
    /* u8 *adc_seq_buf = zalloc(mic_num); */
    u8 adc_seq_buf[AUDIO_ADC_MAX_NUM];
    /*获取使用的mic数据的相对位置*/
    for (j = 0; j < max_mic_num; j++) {
        if (mic_ch & BIT(j)) {
            adc_seq_buf[adc_seq] = get_adc_seq(&adc_hdl, BIT(j)); //查询模拟mic对应的ADC通道
            adc_seq++;
        }
    }
    /*将使用的mic数据按照序号顺序，相邻的排在一起*/
    if (adc_hdl.bit_width != ADC_BIT_WIDTH_16) {
        tmp_len = mic_num * sizeof(int);
        s32 *s32_src = (s32 *)in_data;
        s32 *s32_dst = (s32 *)out_data;
        s32 s32_tmp_dst[AUDIO_ADC_MAX_NUM];
        for (i = 0; i < len / 4; i++) {
            for (j = 0; j < mic_num; j++) {
                s32_tmp_dst[j] = s32_src[max_mic_num * i + adc_seq_buf[j]];
            }
            memcpy(&s32_dst[mic_num * i], s32_tmp_dst, tmp_len);
        }

    } else {
        tmp_len = mic_num * sizeof(short);
        s16 *s16_src = (s16 *)in_data;
        s16 *s16_dst = (s16 *)out_data;
        s16 s16_tmp_dst[AUDIO_ADC_MAX_NUM];
        for (i = 0; i < len / 2; i++) {
            for (j = 0; j < mic_num; j++) {
                s16_tmp_dst[j] = s16_src[max_mic_num * i + adc_seq_buf[j]];
            }
            memcpy(&s16_dst[mic_num * i], s16_tmp_dst, tmp_len);
        }
    }
    /* free(adc_seq_buf); */
}

/**
 * @brief       MIC 的中断回调函数
 *
 * @param _hdl  MIC 节点的操作句柄
 * @param data  MIC 中断采集到的数据地址
 * @param len   MIC 单个通道中断采集到的数据字节长度
 */

__NODE_CACHE_CODE(adc)
static void adc_mic_output_handler(void *_hdl, s16 *data, int len)
{
    struct adc_file_hdl *hdl = (struct adc_file_hdl *)_hdl;
    struct stream_frame *frame;
    /* printf(">> %d %d\n", data[0], data[1]); */

    if (hdl->dump_cnt < 10) {
        hdl->dump_cnt++;
        return;
    }
    if (hdl->force_dump) {
        hdl->value[0] = 0;
        return;
    }

    frame = source_plug_get_output_frame(hdl->source_node, (len * hdl->ch_num));

#if TCFG_AUDIO_CVP_OUTPUT_WAY_IIS_ENABLE && (defined TCFG_IIS_NODE_ENABLE)
    /*对齐iis外部参考数据延时*/
    if (!get_audio_aec_rebooting()) {
        audio_cvp_ref_data_align();
    }
#else
    //cvp读dac 参考数据
#if TCFG_AUDIO_DUT_ENABLE
    //打开产测功能，只有算法模式，才会读dac参考数据，避免 data full
    if (cvp_dut_mode_get() == CVP_DUT_MODE_ALGORITHM) {
        audio_cvp_phase_align();
    }
#else
    audio_cvp_phase_align();
#endif
#endif

#if (defined(TCFG_HOWLING_AHS_NODE_ENABLE) && TCFG_HOWLING_AHS_NODE_ENABLE)
    if (hdl->scene == STREAM_SCENE_MIC_EFFECT || hdl->scene == STREAM_SCENE_LOUDSPEAKER_MIC) {
        if (audio_ahs_status()) {
            howling_ahs_read_ref_data();
        }
    }
#endif

    if (frame) {
#ifdef TCFG_AUDIO_ADC_ENABLE_ALL_DIGITAL_CH
        //ADC DIGITAL 按最大通道数开启时使用
        if (hdl->ch_num != adc_hdl.max_adc_num && const_adc_async_en) {
#else
        if (0) {
#endif
            audio_mic_data_align(data, (s16 *)frame->data, len, hdl->adc_f->cfg.mic_en_map, adc_hdl.max_adc_num);
        } else {
            memcpy(frame->data, data, (len * hdl->ch_num));
        }
        len *= hdl->ch_num;
        if (audio_common_mic_mute_en_get()) {	//mute ADC
            memset((u8 *)frame->data, 0x0, len);
        }

        frame->len          = len;
#if 1
        frame->flags        = FRAME_FLAG_TIMESTAMP_ENABLE | FRAME_FLAG_PERIOD_SAMPLE | FRAME_FLAG_UPDATE_TIMESTAMP;
        frame->timestamp    = adc_hdl.timestamp * TIMESTAMP_US_DENOMINATOR;
#else
        frame->flags        = FRAME_FLAG_SYS_TIMESTAMP_ENABLE;
        frame->timestamp    = adc_hdl.timestamp;
#endif

#if AVC_USE_AEC
        if (hdl->scene == STREAM_SCENE_ENV_NOISE) {
            audio_avc_aec_data_fill((s16 *)frame->data, frame->len);
        }
#endif
        adc_file_fade_in(hdl, frame->data, frame->len);//淡入处理
        source_plug_put_output_frame(hdl->source_node, frame);
    }
}

static void *adc_init_base(void *source_node, struct stream_node *node, void *_hdl)
{
    struct adc_file_hdl *hdl = (struct adc_file_hdl *)_hdl;
    hdl->source_node = source_node;
    hdl->node = node;
    node->type |= NODE_TYPE_IRQ;


    return hdl;
}
static void *adc_init(void *source_node, struct stream_node *node)
{
    struct adc_file_hdl *hdl = zalloc(sizeof(*hdl));
    return adc_init_base(source_node, node, hdl);
}

#if TCFG_MULTI_CH_ADC_NODE_ENABLE
static void *multi_ch_adc_init(void *source_node, struct stream_node *node)
{
    struct adc_file_hdl *hdl = zalloc(sizeof(*hdl));
    hdl->multi_ch_adc_en = 1;
    return adc_init_base(source_node, node, hdl);
}
#endif


__AUDIO_ADC_BANK_CODE
static void adc_ioc_get_fmt(struct adc_file_hdl *hdl, struct stream_fmt *fmt)
{
    /*
     *获取配置文件内的参数,及名字
     * */
    if (hdl->scene == STREAM_SCENE_ESCO) {
        hdl->adc_f = &esco_adc_f;
    } else if (!hdl->adc_f) {
        hdl->adc_f = zalloc(sizeof(struct adc_file_common));
    }
    hdl->adc_f->hdl = hdl;
    u16 uuid = 0;
#if TCFG_MULTI_CH_ADC_NODE_ENABLE
    if (hdl->multi_ch_adc_en) { //多通道dac每个通道都是单声道
        uuid = NODE_UUID_MULTI_CH_ADC;
    } else
#endif
    {
        uuid = NODE_UUID_ADC;
    }
    if (!jlstream_read_node_data_new(uuid, hdl->node->subid, (void *) & (hdl->adc_f->cfg), hdl->name)) {
        printf("%s, read node data err\n", __FUNCTION__);
        ASSERT(0);
    }

#if TCFG_AUDIO_DUT_ENABLE
    //产测bypass 模式，MIC的使能位从产测命令读取
    if (cvp_dut_mode_get() == CVP_DUT_MODE_BYPASS) {
        hdl->adc_f->cfg.mic_en_map = cvp_dut_mic_ch_get();
    }
#endif
    if (hdl->scene != STREAM_SCENE_ESCO) {
        audio_adc_cfg_init(hdl->adc_f);
    } else { //初始化通话通道数
        hdl->ch_num = 0;
        for (int i = 0; i < AUDIO_ADC_MAX_NUM; i++) {
            if (hdl->adc_f->cfg.mic_en_map & BIT(i)) {
                hdl->ch_num++;
            }
        }
    }

    if (config_audio_cfg_online_enable) {
        if (jlstream_read_effects_online_param(uuid, hdl->name, &hdl->adc_f->cfg, sizeof(hdl->adc_f->cfg))) {
            adc_file_log("get adc online param\n");
        }
    }

    switch (hdl->scene) {
    case STREAM_SCENE_ESCO:
    case STREAM_SCENE_LEA_CALL:
#if SUPPORT_CHAGE_AUDIO_CLK
        fmt->sample_rate    = audio_adc_sample_rate_mapping(16000);
#else
        fmt->sample_rate    = 16000;
#endif
        break;
    case STREAM_SCENE_HEARING_AID:
#ifdef  AUDIO_MIC_EFFECT_SAMPLE_RATE
        fmt->sample_rate = AUDIO_MIC_EFFECT_SAMPLE_RATE;
#else
        fmt->sample_rate = 44100;
#endif
        break;
#ifdef CONFIG_WIRELESS_MIC_CASE_ENABLE
    case STREAM_SCENE_WIRELESS_MIC:
#if (defined(TCFG_AUDIO_ADC_SAMPLE_RATE) && TCFG_AUDIO_ADC_SAMPLE_RATE)
        fmt->sample_rate    = TCFG_AUDIO_ADC_SAMPLE_RATE;
#else
        fmt->sample_rate    = LE_AUDIO_CODEC_SAMPLERATE;
#endif
        break;
#endif/*WIRELESS_MIC_PRODUCT_MODE*/
    case STREAM_SCENE_LOUDSPEAKER_MIC:
    case STREAM_SCENE_MIC_EFFECT:
        u32 mic_eff_sr = 44100;
#if (defined(TCFG_HOWLING_AHS_NODE_ENABLE) && TCFG_HOWLING_AHS_NODE_ENABLE)
        mic_eff_sr = 16000;
#endif
#if SUPPORT_CHAGE_AUDIO_CLK
        fmt->sample_rate    = audio_adc_sample_rate_mapping(mic_eff_sr);
#else
        fmt->sample_rate    = mic_eff_sr;
#endif
        break;
    case STREAM_SCENE_ENV_NOISE:
        fmt->sample_rate = 16000;
        break;
    default:
#if SUPPORT_CHAGE_AUDIO_CLK
        fmt->sample_rate    = audio_adc_sample_rate_mapping(44100);
#else
        fmt->sample_rate    = 44100;
#endif
        break;
    }
    if (hdl->ch_num == 4) {
        fmt->channel_mode   = AUDIO_CH_QUAD;
    } else if (hdl->ch_num == 3) {
        fmt->channel_mode   = AUDIO_CH_TRIPLE;
    } else if (hdl->ch_num == 2) {
        fmt->channel_mode   = AUDIO_CH_LR;
    } else {
        fmt->channel_mode   = AUDIO_CH_MIX;
    }

#if TCFG_MULTI_CH_ADC_NODE_ENABLE
    if (hdl->multi_ch_adc_en) { //多通道dac每个通道都是单声道
        hdl->ch_num = 1;
        fmt->channel_mode   = AUDIO_CH_MIX;
    }
#endif
    printf("adc num:%d, channel_mode:0x%x, bit_width:%dbit", hdl->ch_num, fmt->channel_mode, (adc_hdl.bit_width) ? 24 : 16);
    if (adc_hdl.bit_width == ADC_BIT_WIDTH_24) {
        fmt->bit_wide = DATA_BIT_WIDE_24BIT;
    } else {
        fmt->bit_wide = DATA_BIT_WIDE_16BIT;
    }
    fmt->coding_type = AUDIO_CODING_PCM;

    hdl->channel_mode = fmt->channel_mode;
    hdl->sample_rate = fmt->sample_rate;
}

static int adc_ioc_set_fmt(struct adc_file_hdl *hdl, struct stream_fmt *fmt)
{
    hdl->sample_rate = fmt->sample_rate;
    printf("adc_ioc_set_fmt,Fs = %d", hdl->sample_rate);
    if (hdl->ch_num != AUDIO_CH_NUM(fmt->channel_mode)) {
        //节点配置的声道类型与协商输出的声道类型不一致。
        printf("adc channel number set error, %d, %d\n", hdl->ch_num, AUDIO_CH_NUM(fmt->channel_mode));
        return -1;
    }

    return 0;
}

int adc_file_mic_open(struct adc_mic_ch *mic, int ch) //用于打开通话使用的mic
{
    struct mic_open_param mic_param = {0};
    int mic_gain;
    int mic_pre_gain;
    int ch_index;

    u32 i = 0;
    for (i = 0; i < AUDIO_ADC_MAX_NUM; i++) {
        if (ch & BIT(i)) {
            ch_index = i;

            audio_adc_param_fill(&mic_param, &esco_adc_f.platform_cfg[ch_index]);
            mic_gain                = esco_adc_f.cfg.param[ch_index].mic_gain;
            mic_pre_gain            = esco_adc_f.cfg.param[ch_index].mic_pre_gain;

            if (mic_param.mic_bias_sel == 0) {
                /* u32 gpio = uuid2gpio(esco_adc_f.platform_cfg[ch_index].power_io); */
                u32 gpio = esco_adc_f.platform_cfg[ch_index].power_io;
                gpio_set_mode(IO_PORT_SPILT(gpio), PORT_OUTPUT_HIGH);
            }

            audio_adc_mic_open(mic, AUDIO_ADC_MIC(ch_index), &adc_hdl, &mic_param);
            audio_adc_mic_set_gain(mic, AUDIO_ADC_MIC(ch_index), mic_gain);
            audio_adc_mic_gain_boost(AUDIO_ADC_MIC(ch_index), mic_pre_gain);
        }
    }
    return 0;
}

/*返回值：使能打开的ADC通道数*/
__AUDIO_ADC_BANK_CODE
int adc_file_cfg_mic_open(struct adc_mic_ch *mic, int ch, struct adc_file_common *adc_f) //用于打开通话外其他mic
{
    struct mic_open_param mic_param = {0};
    int mic_gain;
    int mic_pre_gain;
    int ch_index;
    u8 mic_en_num = 0;

    u32 i = 0;
    for (i = 0; i < AUDIO_ADC_MAX_NUM; i++) {
        if (ch & BIT(i)) {
            mic_en_num++;
            ch_index = i;

            audio_adc_param_fill(&mic_param, &adc_f->platform_cfg[ch_index]);
            mic_gain                = adc_f->cfg.param[ch_index].mic_gain;
            mic_pre_gain            = adc_f->cfg.param[ch_index].mic_pre_gain;

            if (mic_param.mic_bias_sel == 0) {
                /*u32 gpio = uuid2gpio(adc_f->platform_cfg[ch_index].power_io);*/
                u32 gpio = adc_f->platform_cfg[ch_index].power_io;
                gpio_set_mode(IO_PORT_SPILT(gpio), PORT_OUTPUT_HIGH);
            }

            audio_adc_mic_open(mic, AUDIO_ADC_MIC(ch_index), &adc_hdl, &mic_param);
            audio_adc_mic_set_gain(mic, AUDIO_ADC_MIC(ch_index), mic_gain);
            audio_adc_mic_gain_boost(AUDIO_ADC_MIC(ch_index), mic_pre_gain);
        }
    }
    return mic_en_num;
}

static u8 adc_mic_start = 0;

static void adc_open_task(void *_hdl)
{
    struct adc_file_hdl *hdl = (struct adc_file_hdl *)_hdl;

    /*y_printf("adc_mic_start\n");*/

    audio_adc_mic_start(&hdl->mic_ch);
    adc_mic_start = 1;

    while (1) {
        os_time_dly(0xffff);
    }

}

__AUDIO_ADC_BANK_CODE
static int adc_file_ioc_start(struct adc_file_hdl *hdl)
{
    int ret = 0;
    if (hdl->start == 0) {
        hdl->start = 1;
#if ((defined TCFG_CALL_KWS_SWITCH_ENABLE) && TCFG_CALL_KWS_SWITCH_ENABLE)
        extern void smart_voice_mcu_mic_suspend();
        smart_voice_mcu_mic_suspend();
#endif
        hdl->dump_cnt = 0;

        //br50的LPADC需要根据采样率配置模拟部分的时钟分频,需要先设置采样率才能打开MIC
        audio_adc_mic_set_sample_rate(&hdl->mic_ch, hdl->sample_rate);
        u8 mic_en_num = adc_file_cfg_mic_open(&hdl->mic_ch, hdl->adc_f->cfg.mic_en_map, (void *)hdl->adc_f);
        ASSERT(mic_en_num > 0, "mic cfg failed, mic_num=%d\n", mic_en_num);
        if (esco_adc_file_g.fixed_buf) {
            hdl->adc_buf = esco_adc_file_g.fixed_buf;
            audio_adc_set_buf_fix(1, &adc_hdl);
        }
        if (!hdl->adc_buf && !adc_hdl.hw_buf) {	//避免没有设置ADC的中断点数，以及数据流stop之后重新start申请buffer
            if (!hdl->irq_points) {
                hdl->irq_points = 256;
            }
            u16 adc_dma_buf_size;
            if (const_adc_async_en) {
                adc_dma_buf_size = ESCO_ADC_BUF_NUM * hdl->irq_points * ((adc_hdl.bit_width == ADC_BIT_WIDTH_16) ? 2 : 4) * (adc_hdl.max_adc_num);
                printf("adc_dma_buf_size:%d,ch_num:%d,bit_width:%d", adc_dma_buf_size, adc_hdl.max_adc_num, ((adc_hdl.bit_width == ADC_BIT_WIDTH_16) ? 16 : 24));
            } else {
                adc_dma_buf_size = ESCO_ADC_BUF_NUM * hdl->irq_points * ((adc_hdl.bit_width == ADC_BIT_WIDTH_16) ? 2 : 4) * (mic_en_num);
                printf("adc_dma_buf_size:%d,ch_num:%d,bit_width:%d", adc_dma_buf_size, mic_en_num, ((adc_hdl.bit_width == ADC_BIT_WIDTH_16) ? 16 : 24));
            }
            hdl->adc_buf = zalloc(adc_dma_buf_size);

            if (!hdl->adc_buf) {
                ret = -1;
                return ret;
            }
        }
        ret = audio_adc_mic_set_buffs(&hdl->mic_ch, hdl->adc_buf, hdl->irq_points * 2, ESCO_ADC_BUF_NUM);
        if (ret && hdl->adc_buf && !esco_adc_file_g.fixed_buf) {
            free(hdl->adc_buf);
            hdl->adc_buf = NULL;
        }

        hdl->adc_output.priv    = hdl;
#if TCFG_MULTI_CH_ADC_NODE_ENABLE
        if (hdl->multi_ch_adc_en) {
            hdl->adc_output.handler = multi_ch_adc_mic_output_handler;
        } else
#endif
        {
            hdl->adc_output.handler = adc_mic_output_handler;
        }
        audio_adc_add_output_handler(&adc_hdl, &hdl->adc_output);
#ifdef CONFIG_CPU_BR36
        /*r_printf("-----create_adc_open_task\n");*/
        adc_mic_start = 0;
        int err = os_task_create(adc_open_task, hdl, 2, 512, 0, "adc_open");
        if (err) {
            adc_mic_start = 1;
            audio_adc_mic_start(&hdl->mic_ch);
        }
#else
        audio_adc_mic_start(&hdl->mic_ch);
#endif
        adc_file_global_open_cnt++;
    }
    for (int i = 0; i < 4; i++) {
        hdl->value[i] = 0;
    }
    return ret;
}

__AUDIO_ADC_BANK_CODE
static int adc_file_ioc_stop(struct adc_file_hdl *hdl)
{
    if (hdl->start) {
        hdl->start = 0;
#if ((defined TCFG_CALL_KWS_SWITCH_ENABLE) && TCFG_CALL_KWS_SWITCH_ENABLE)
        extern u16 jl_call_kws_get_status();
        extern int audio_phone_call_kws_start(void);
        if (jl_call_kws_get_status() == BT_STATUS_PHONE_INCOME) {
            audio_phone_call_kws_start();
        }
#endif

#ifdef CONFIG_CPU_BR36
        while (adc_mic_start == 0) {
            os_time_dly(1);
            /*puts("--wait\n");*/
        }
        /*r_printf("-----del_adc_open_task\n");*/
        os_task_del("adc_open");
#endif
        audio_adc_mic_close(&hdl->mic_ch);
        if (!esco_adc_file_g.fixed_buf) {
            hdl->adc_buf = NULL; //在adc 驱动中释放了这个buffer
        }
        audio_adc_del_output_handler(&adc_hdl, &hdl->adc_output);

        for (u32 i = 0; i < AUDIO_ADC_MAX_NUM; i++) {
            if (hdl->adc_f->cfg.mic_en_map & BIT(i)) {
                if (hdl->adc_f->platform_cfg[i].mic_bias_sel == 0) {
                    if (!audio_adc_is_active()) {
                        //u32 gpio = uuid2gpio(hdl->adc_f->platform_cfg[i].power_io);
                        u32 gpio = hdl->adc_f->platform_cfg[i].power_io;
                        gpio_set_mode(IO_PORT_SPILT(gpio), PORT_OUTPUT_LOW);
                    }
                }
            }
        }
        if (adc_file_global_open_cnt) {
            adc_file_global_open_cnt--;
        }
    }
    return 0;
}

__AUDIO_ADC_BANK_CODE
static int adc_file_ioc_update_parm(struct adc_file_hdl *hdl, int parm)
{
    int i;
    int ret = false;
    struct adc_file_cfg *cfg = (struct adc_file_cfg *)parm;
    if (hdl) {
        for (i = 0; i < AUDIO_ADC_MAX_NUM; i++) {
            if (hdl->adc_f->cfg.mic_en_map & BIT(i)) {
                //目前仅支持更新增益
                audio_adc_mic_set_gain(&hdl->mic_ch, BIT(i), cfg->param[i].mic_gain);
                audio_adc_mic_gain_boost(BIT(i), cfg->param[i].mic_pre_gain);
            }
        }
        ret = true;
    }
    return ret;
}

static int adc_ioctl(void *_hdl, int cmd, int arg)
{
    int ret = 0;
    struct adc_file_hdl *hdl = (struct adc_file_hdl *)_hdl;

    switch (cmd) {
    case NODE_IOC_GET_FMT:
        adc_ioc_get_fmt(hdl, (struct stream_fmt *)arg);
        break;
    case NODE_IOC_SET_FMT:
        ret = adc_ioc_set_fmt(hdl, (struct stream_fmt *)arg);
        break;
    case NODE_IOC_SET_SCENE:
        hdl->scene = arg;
        //adc节点需要根据场景配置位宽
        if ((hdl->scene == STREAM_SCENE_ESCO) || (hdl->scene == STREAM_SCENE_PC_MIC) || (hdl->scene == STREAM_SCENE_LEA_CALL)) {
            adc_hdl.bit_width = 0;
        } else {
            adc_hdl.bit_width = audio_general_in_dev_bit_width();
        }
        break;
    case NODE_IOC_SET_PRIV_FMT:
        hdl->irq_points = arg;
        break;
    case NODE_IOC_START:
        ret = adc_file_ioc_start(hdl);
        hdl->adc_seq = get_adc_seq(&adc_hdl, hdl->adc_f->cfg.mic_en_map); //查询模拟mic对应的ADC通道
        break;
    case NODE_IOC_SUSPEND:
    case NODE_IOC_STOP:
        adc_file_ioc_stop(hdl);
        break;
    case NODE_IOC_SET_PARAM:
        ret = adc_file_ioc_update_parm(hdl, arg);
        break;
    case NODE_IOC_NODE_CONFIG:
        hdl->ch_num = (u8)arg;
        break;
    case NODE_IOC_FORCE_DUMP_PACKET:
        hdl->force_dump = arg;
        break;
    }

    return ret;
}

static void adc_release(void *_hdl)
{
    struct adc_file_hdl *hdl = (struct adc_file_hdl *)_hdl;
    if (hdl->adc_f && hdl->scene != STREAM_SCENE_ESCO) {
        free(hdl->adc_f);
        hdl->adc_f = NULL;
    }
    free(hdl);
    esco_adc_f.hdl = NULL;
}


#if TCFG_ADC_NODE_ENABLE
REGISTER_SOURCE_NODE_PLUG(adc_file_plug) = {
    .uuid       = NODE_UUID_ADC,
    .init       = adc_init,
    .ioctl      = adc_ioctl,
    .release    = adc_release,
};

REGISTER_ONLINE_ADJUST_TARGET(adc) = {
    .uuid = NODE_UUID_ADC,
};
#endif

#if TCFG_MULTI_CH_ADC_NODE_ENABLE
REGISTER_SOURCE_NODE_PLUG(multi_ch_adc_file_plug) = {
    .uuid       = NODE_UUID_MULTI_CH_ADC,
    .init       = multi_ch_adc_init,
    .ioctl      = adc_ioctl,
    .release    = adc_release,
};

REGISTER_ONLINE_ADJUST_TARGET(mulit_ch_adc) = {
    .uuid = NODE_UUID_MULTI_CH_ADC,
};
#endif
#else

u8 audio_get_mic_num(u32 mic_ch)
{
    return 0;
}
u8 audio_adc_file_get_mic_en_map(void)
{
    return 0;
}
u8 audio_adc_file_get_esco_mic_num(void)
{
    return 0;
}
#endif
