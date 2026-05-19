#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".cvp_develop_node.data.bss")
#pragma data_seg(".cvp_develop_node.data")
#pragma const_seg(".cvp_develop_node.text.const")
#pragma code_seg(".cvp_develop_node.text")
#endif
#include "jlstream.h"
#include "media/audio_base.h"
#include "circular_buf.h"
#include "cvp_develop_node.h"
#include "app_config.h"
#include "cvp_node.h"

#if TCFG_AUDIO_DUT_ENABLE
#include "audio_dut_control.h"
#endif

/*
	算法inbuf 输入函数排序
    0: 手动排序，如遇多类型(ADC,TDM,PDM) MIC交叉使用，根据输入buffer，手动指定对应MIC的inbuf
    1：自动排序，根据MIC序号自动填充inbuf，按此优先级顺序填空（TALK/FF/FB/VPU）
*/
#define CVP_DEV_AUTO_SORT_INBUF_CB	1

#if TCFG_AUDIO_CVP_DEVELOP_ENABLE
#define CVP_INPUT_SIZE		256*3	//CVP输入缓存，short

struct cvp_node_hdl {
    enum stream_scene scene;
    struct stream_frame *frame[3];	//输入frame存储，算法输入缓存使用
    struct cvp_dev_cfg_t cfg;
    u8 buf_cnt;						//循环输入buffer位置
    u8 adc_ch_num;
    s16 *buf[CVP_DEV_MAX_MIC_NUM];
    u32 ref_sr;
    u16 source_uuid; //源节点uuid
    void (*lock)(void);
    void (*unlock)(void);
    void (*inbuf_p[CVP_DEV_MAX_MIC_NUM])(s16 *, u16);	//算法inbuf输入函数
};

static struct cvp_dev_cfg_t global_cvp_cfg;
static struct cvp_node_hdl *g_cvp_hdl;

int cvp_dev_node_output_handle(s16 *data, u16 len)
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

static void cvp_node_param_cfg_printf(struct cvp_dev_cfg_t *cfg)
{
    printf("===CVP DEVELOP NODE CFG===\n");
    printf("mic_num %d algo_type %d\n", cfg->mic_num, cfg->algo_type);
    printf("mic_type %d vpu_en %d\n", cfg->mic_type, cfg->vpu_en);
    printf("talk_mic    : 0x%x\n", cfg->talk_mic);
    printf("talk_ff_mic : 0x%x\n", cfg->talk_ff_mic);
    printf("talk_fb_mic : 0x%x\n", cfg->talk_fb_mic);
    printf("vpu_ch_sel  : 0x%x\n", cfg->vpu_ch_sel);
}

static void cvp_dev_node_mic_num_update(struct cvp_dev_cfg_t *cfg)
{
    u8 mic_type = cfg->mic_type;
    cfg->mic_num = 0;
    while (mic_type) {
        if (mic_type & BIT(0)) {
            cfg->mic_num++;
        }
        mic_type >>= 1;
    }
    if (cfg->vpu_en) {
        //ADC 方案则新增MIC NUM
        if (CVP_DEV_IS_VPU_ADC_MODE(cfg->vpu_ch_sel)) {
            cfg->mic_num++;
        }
    }
    ASSERT(cfg->mic_num <= AUDIO_ADC_MAX_NUM, "MIC num overflow");

#if 1	/*MIC序号校验: 不同MIC序号需不一致*/
    u16 mic_ch[CVP_DEV_MAX_MIC_NUM] = {0};
    if (cfg->mic_type & CVP_DEV_TALK_MIC_CH) {
        mic_ch[0] = cfg->talk_mic;
    }
    if (cfg->mic_type & CVP_DEV_FF_MIC_CH) {
        mic_ch[1] = cfg->talk_ff_mic;
    }
    if (cfg->mic_type & CVP_DEV_FB_MIC_CH) {
        mic_ch[2] = cfg->talk_fb_mic;
    }
    if (cfg->vpu_en) {
        mic_ch[3] = cfg->vpu_ch_sel;
    }
    for (int i = 0; i < CVP_DEV_MAX_MIC_NUM; i++) {
        for (int j = 0; j < CVP_DEV_MAX_MIC_NUM; j++) {
            if ((i != j) && (mic_ch[i] == mic_ch[j]) && mic_ch[i]) {
                ASSERT(0, "MIC index mismatch %d, %d\n", i, j);
            }
        }
    }
#endif
    cvp_node_param_cfg_printf(cfg);
}

__CVP_BANK_CODE
int cvp_node_param_cfg_read(void *priv, u8 ignore_subid)
{
    struct cvp_node_hdl *hdl = (struct cvp_node_hdl *)priv;
    int len = 0;
    u8 subid;
    if (g_cvp_hdl) {
        subid = hdl_node(g_cvp_hdl)->subid;
    } else {
        subid = 0XFF;
    }

    struct node_param ncfg = {0};
    len = jlstream_read_node_data(NODE_UUID_CVP_DEVELOP, subid, (u8 *)&ncfg);
    if (len != sizeof(ncfg)) {
        printf("cvp_dms_name read ncfg err\n");
        return -2;
    }
    char mode_index = 0;
    char cfg_index = 0;//目标配置项序号
    struct cfg_info info = {0};
    if (!jlstream_read_form_node_info_base(mode_index, ncfg.name, cfg_index, &info)) {
        len = jlstream_read_form_cfg_data(&info, &hdl->cfg);
    }
    printf(" %s len %d, sizeof(cfg) %d\n", __func__,  len, (int)sizeof(struct cvp_dev_cfg_t));
    if (len != sizeof(struct cvp_dev_cfg_t)) {
        printf("cvp_develop_param read ncfg err\n");
        return -1 ;
    }
    cvp_dev_node_mic_num_update(&hdl->cfg);
    if (!hdl->cfg.mic_num) {
        hdl->cfg.mic_num = 1;
    }
#if (CVP_THIRD_ALGO_TYPE & CVP_ELEVOC_ALGO_BITMAP)
    switch (hdl->cfg.algo_type) {
    case CVP_CFG_ELEVOC_1MIC_VPU:
        ASSERT(hdl->cfg.mic_num == 2,
               "cfg error: alg=0x%08X mic=%d (expect 2)\n",
               hdl->cfg.algo_type, hdl->cfg.mic_num);
        break;
    case CVP_CFG_ELEVOC_2MIC_VPU:
    case CVP_CFG_ELEVOC_2MIC_VPU_CLIP:
        ASSERT(hdl->cfg.mic_num == 3,
               "cfg error: alg=0x%08X mic=%d (expect 3)\n",
               hdl->cfg.algo_type, hdl->cfg.mic_num);
        break;
    case CVP_CFG_ELEVOC_3MIC_VPU:
        ASSERT(hdl->cfg.mic_num == 4,
               "cfg error: alg=0x%08X mic=%d (expect 4)\n",
               hdl->cfg.algo_type, hdl->cfg.mic_num);
        break;
    default:
        ASSERT(0,
               "cfg error: unknown elevoc alg=0x%08X mic=%d\n",
               hdl->cfg.algo_type, hdl->cfg.mic_num);
        break;
    }
#endif
    return len;
}

//将输入的channel与inbuf_p绑定，并根据chanels排序
static void cvp_dev_sort_mic_channels(u16 *channels, void (**inbuf_p)(s16 *, u16), int count)
{
    u8 i, j, temp;
    void (*temp_inbuf_p)(s16 *, u16);
    for (i = 0; i < count - 1; i++) {
        for (j = 0; j < count - i - 1; j++) {
            if (channels[j] > channels[j + 1]) {
                /* printf("swap %d %d %p %p\n", channels[j], channels[j + 1], inbuf_p[j], inbuf_p[j + 1]); */
                temp = channels[j];
                channels[j] = channels[j + 1];
                channels[j + 1] = temp;
                temp_inbuf_p = inbuf_p[j];
                inbuf_p[j] = inbuf_p[j + 1];
                inbuf_p[j + 1] = temp_inbuf_p;
            }
        }
    }
}

//根据配置匹配对应MIC通道，映射算法inbuf函数指针
static int cvp_dev_map_mic_channels(struct cvp_dev_cfg_t *cfg, void (**inbuf_p)(s16 *, u16))
{
#if CVP_DEV_AUTO_SORT_INBUF_CB
    u16 channels[CVP_DEV_MAX_MIC_NUM];
    int channel_count = 0;
    void (*funcs[])(s16 *, u16) = { audio_aec_inbuf, audio_aec_inbuf_ref, audio_aec_inbuf_ref_1, audio_aec_inbuf_ref_2 };

    // 根据mic_num和vpu_en添加其他通道
    if (cfg->mic_type & CVP_DEV_TALK_MIC_CH) {
        channels[channel_count++] = cfg->talk_mic;
    }
    if (cfg->mic_type & CVP_DEV_FF_MIC_CH) {
        channels[channel_count++] = cfg->talk_ff_mic;
    }
    if (cfg->mic_type & CVP_DEV_FB_MIC_CH) {
        channels[channel_count++] = cfg->talk_fb_mic;
    }
    if (cfg->vpu_en) {
        if (CVP_DEV_IS_VPU_ADC_MODE(cfg->vpu_ch_sel)) {
            channels[channel_count++] = cfg->vpu_ch_sel;
        }
    }

    /* printf("before sort channels: %d\n", channel_count); */
    /* for (int i = 0; i < channel_count; i++) { */
    /* printf("%d %p ", channels[i], funcs[i]); */
    /* } */
    // 对通道进行排序
    cvp_dev_sort_mic_channels(channels, funcs, channel_count);
    /* printf("Sorted channels: "); */
    /* for (int i = 0; i < channel_count; i++) { */
    /* printf("%d %p ", channels[i], funcs[i]); */
    /* } */

    for (int i = 0; i < channel_count; i++) {
        inbuf_p[i] = funcs[i];
    }
#else
    //1MIC + VPU 方案
    inbuf_p[0] = audio_aec_inbuf;        //TALK MIC
    inbuf_p[1] = audio_aec_inbuf_ref;    //VPU

    /*
    //3MIC + VPU 方案
    inbuf_p[0] = audio_aec_inbuf;        //TALK MIC
    inbuf_p[1] = audio_aec_inbuf_ref;    //FF MIC
    inbuf_p[2] = audio_aec_inbuf_ref_1;  //FB MIC
    inbuf_p[3] = audio_aec_inbuf_ref_2;  //VPU
    */
#endif
    return 0;
}

int cvp_dev_param_cfg_read(void)
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
    len = jlstream_read_node_data(NODE_UUID_CVP_DEVELOP, subid, (u8 *)&ncfg);
    if (len != sizeof(ncfg)) {
        printf("cvp_dev_name read ncfg err\n");
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
        printf("cvp_dev_param read ncfg err\n");
        return -1 ;
    }
    cvp_dev_node_mic_num_update(&global_cvp_cfg);

    return 0;
}

struct cvp_dev_cfg_t *cvp_dev_cfg_get(void)
{
    if (g_cvp_hdl) {
        /* cvp_node_param_cfg_printf(&g_cvp_hdl->cfg); */
        return &g_cvp_hdl->cfg;
    } else {
        /* cvp_node_param_cfg_printf(&global_cvp_cfg); */
        return &global_cvp_cfg;
    }
}

/*节点输出回调处理，可处理数据或post信号量*/
static void cvp_handle_frame(struct stream_iport *iport, struct stream_note *note)
{
    struct cvp_node_hdl *hdl = (struct cvp_node_hdl *)iport->node->private_data;
    s16 *dat;
    s16 *tbuf[CVP_DEV_MAX_MIC_NUM];
    int wlen;
    struct stream_frame *in_frame;

    while (1) {
        in_frame = jlstream_pull_frame(iport, note);		//从iport读取数据
        if (!in_frame) {
            break;
        }
#if TCFG_AUDIO_DUT_ENABLE
        //产测bypass 模式 不经过算法
        if (cvp_dut_mode_get() == CVP_DUT_MODE_BYPASS) {
            struct stream_node *node = iport->node;
            jlstream_push_frame(node->oport, in_frame);
            continue;
        }
#endif
        //模仿ADCbuff的存储方法
        if (hdl->cfg.mic_num == 1) {			//单麦第三方算法
            dat = hdl->buf[0] + (in_frame->len / 2 * hdl->buf_cnt);
            memcpy((u8 *)dat, in_frame->data, in_frame->len);
            hdl->inbuf_p[0](dat, in_frame->len);
        } else {	//多麦第三方算法
            hdl->lock();
            u8 mic_num = hdl->cfg.mic_num;
            wlen = (in_frame->len >> 1) / mic_num;	//单个ADC的点数
            dat = (s16 *)in_frame->data;
            for (int n = 0; n < mic_num; n++) {
                tbuf[n] = hdl->buf[n] + (wlen * hdl->buf_cnt);
                for (int i = 0; i < wlen; i++) {
                    tbuf[n][i] = dat[mic_num * i + n];
                }
                if (n >= 1) {
                    hdl->inbuf_p[n](tbuf[n], wlen << 1);
                }
            }
            hdl->inbuf_p[0](tbuf[0], wlen << 1);	//TALK 最后执行
            hdl->unlock();
        }
        if (++hdl->buf_cnt > ((CVP_INPUT_SIZE / 256) - 1)) {	//计算下一个ADCbuffer位置
            hdl->buf_cnt = 0;
        }
        jlstream_free_frame(in_frame);	//释放iport资源
    }
}

static int cvp_develop_lock_init(struct cvp_node_hdl *hdl)
{
    int ret = false;
    u16 node_uuid = hdl_node(hdl)->uuid;
    if (hdl) {
        switch (node_uuid) {
        case NODE_UUID_CVP_DEVELOP:
            hdl->lock   = audio_cvp_develop_lock;
            hdl->unlock = audio_cvp_develop_unlock;
            break;
        default:
            printf("cvp_develop_lock_init: unknown node UUID %d\n", node_uuid);
            hdl->lock   = NULL;
            hdl->unlock = NULL;
            break;
        }
        ret = true;
    }
    return ret;
}

static int cvp_adapter_bind(struct stream_node *node, u16 uuid)
{
    struct cvp_node_hdl *hdl = (struct cvp_node_hdl *)node->private_data;
    /*初始化spinlock锁*/
    cvp_develop_lock_init(hdl);

    node->type = NODE_TYPE_ASYNC;
    cvp_node_param_cfg_read(hdl, 0);
    cvp_node_context_setup(uuid);
    //根据算法MIC_NUM分配对应的空间
    for (int i = 0; i < hdl->cfg.mic_num; i++) {
        hdl->buf[i] = (s16 *)malloc(CVP_INPUT_SIZE << 1);
    }
    g_cvp_hdl = hdl;

    return 0;
}

/*打开改节点输入接口*/
static void cvp_ioc_open_iport(struct stream_iport *iport)
{
}


/*节点start函数*/
__CVP_BANK_CODE
static void cvp_ioc_start(struct cvp_node_hdl *hdl)
{
    struct stream_fmt *fmt = &hdl_node(hdl)->oport->fmt;
    struct audio_aec_init_param_t init_param;
    init_param.sample_rate = fmt->sample_rate;
    init_param.ref_sr = hdl->ref_sr;
    init_param.mic_num = hdl->cfg.mic_num;
    init_param.algo_type = hdl->cfg.algo_type;

    if (hdl->source_uuid == NODE_UUID_ADC) {
        if (hdl->adc_ch_num != hdl->cfg.mic_num) {
#if TCFG_AUDIO_DUT_ENABLE
            //使能产测时，只有算法模式才需判断
            if (cvp_dut_mode_get() == CVP_DUT_MODE_ALGORITHM) {
                ASSERT(0, "CVP_develop ESCO MIC num is %d != %d\n", hdl->adc_ch_num, hdl->cfg.mic_num);
            }
#else
            ASSERT(0, "CVP_develop ESCO MIC num is %d != %d\n", hdl->adc_ch_num, hdl->cfg.mic_num);
#endif
        }
    }
    cvp_dev_map_mic_channels(&hdl->cfg, hdl->inbuf_p);
    audio_aec_init(&init_param);
}

/*节点stop函数*/
static void cvp_ioc_stop(struct cvp_node_hdl *hdl)
{
    if (hdl) {
        /* for (int i = 0; i < 3; i++) { */
        /* if (hdl->frame[i] != NULL) {		//检查是否存在未释放的iport缓存 */
        /* jlstream_free_frame(hdl->frame[i]);	//释放iport缓存 */
        /* } */
        /* } */
        audio_aec_close();
    }
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
    struct cvp_node_hdl *hdl = (struct cvp_node_hdl *)node->private_data;
    //根据算法MIC_NUM分配对应的空间
    for (int i = 0; i < hdl->cfg.mic_num; i++) {
        free(hdl->buf[i]);
        hdl->buf[i] = NULL;
    }
    g_cvp_hdl = NULL;
    cvp_node_context_setup(0);
}

/*节点adapter 注意需要在sdk_used_list声明，否则会被优化*/
REGISTER_STREAM_NODE_ADAPTER(cvp_node_adapter) = {
    .uuid       = NODE_UUID_CVP_DEVELOP,
    .bind       = cvp_adapter_bind,
    .ioctl      = cvp_adapter_ioctl,
    .release    = cvp_adapter_release,
    .handle_frame = cvp_handle_frame,
    .hdl_size   = sizeof(struct cvp_node_hdl),
    .ability_channel_out = 0x80 | 1,
    .ability_channel_convert = 1,
};

#endif/*TCFG_CVP_DEVELOP_ENABLE*/


__attribute__((used))
int cvp_develop_version_1_0_0(void)
{
    return 0x100;
}
