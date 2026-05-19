#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".audio_cvp_demo.data.bss")
#pragma data_seg(".audio_cvp_demo.data")
#pragma const_seg(".audio_cvp_demo.text.const")
#pragma code_seg(".audio_cvp_demo.text")
#endif
/*
 ****************************************************************
 *							AUDIO_CVP_DEMO
 * Brief : CVP模块使用范例
 * Notes :清晰语音测试使用，请不要修改本demo，如有需求，请拷贝副
 *		  本，自行修改!
 * Usage :
 *(1)测试单mic降噪和双mic降噪
 *(2)支持监听：监听原始数据/处理后的数据(MONITOR)
 *	MIC ---------> CVP ---------->SPK
 *			|				|
 *	   monitor probe	monitor post
 *(3)调用audio_cvp_test()即可测试cvp模块
 ****************************************************************
 */
#include "cpu/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "audio_cvp.h"
#include "app_main.h"
#include "audio_config.h"
#include "overlay_code.h"
#include "clock_manager/clock_manager.h"
#include "aec_uart_debug.h"
#include "node_uuid.h"

#if TCFG_AUDIO_CVP_DEMO_ENABLE

#define CVP_LOG_ENABLE
#ifdef CVP_LOG_ENABLE
#define CVP_LOG(x, ...)  y_printf("[cvp_demo]" x " ", ## __VA_ARGS__)
#else
#define CVP_LOG(...)
#endif/*CVP_LOG_ENABLE*/

extern struct adc_platform_data adc_data;
extern struct audio_dac_hdl dac_hdl;
extern struct audio_adc_hdl adc_hdl;

//----------------------MIC配置----------------------
//需要启用的MIC通道
#define CVP_MIC_CH_SEL      AUDIO_ADC_MIC_0 //AUDIO_ADC_MIC_0 | AUDIO_ADC_MIC_1
#define CVP_MIC_CH_NUM		1		//MIC数量(需对照CVP_MIC_CH_SEL配置)
#define CVP_MIC_SR			16000	//MIC采样率
#define CVP_MIC_GAIN		10		//MIC增益
#define MIC_IRQ_POINTS     	256		//中断点数(short)

#define MIC_BUF_NUM        	2		//MIC中断buffer个数，一般不需要修改

//------------------DAC监听配置----------------------
#define CVP_MONITOR_DIS		0		//监听关闭
#define CVP_MONITOR_PROBE	1		//监听处理前数据
#define CVP_MONITOR_POST	2		//监听处理后数据
//默认监听选择
#define CVP_MONITOR_SEL		CVP_MONITOR_PROBE

//------------------自研通话算法配置----------------
#define CVP_DEMO_IN_HOUSE_ENABLE				0						//启动自研算法
#define CVP_DEMO_NODE_UUID						NODE_UUID_CVP_SMS_DNS	//目标测试算法节点UUID

//------------------数据导出配置---------------------
#define CVP_DEMO_UART_EXPORT_ENABLE		    	0	//数据导出使能
#define CVP_DEMO_UART_EXPORT_CH		    		3   //数据导出通道
#define CVP_DEMO_UART_EXPORT_LEN				512 //数据导出帧长(byte)
#define CVP_DEMO_UART_EXPORT_PACKET_SIZE		((CVP_DEMO_UART_EXPORT_LEN + 20) * CVP_DEMO_UART_EXPORT_CH)

/*
   1、需要依赖TCFG_AUDIO_DATA_EXPORT_DEFINE 定义
   2、若JL自研通话需要用此数据导出，则需要关闭JL自研通话算法内部数据导出功能 CONST_AEC_EXPORT， 避免冲突
 */
#if (TCFG_AUDIO_DATA_EXPORT_DEFINE != AUDIO_DATA_EXPORT_VIA_UART) && CVP_DEMO_UART_EXPORT_ENABLE
#error "CVP_DEMO EXPORT should be open TCFG_AUDIO_DATA_EXPORT_DEFINE = AUDIO_DATA_EXPORT_VIA_UART"
#endif

//------------------时钟配置------------------------
#define CVP_DEMO_CLOCK				(96 * 1000000L)	//模块运行时钟

enum CVP_DEMO_MSG {
    CVP_DEMO_MSG_OPEN = 0xA1,
    CVP_DEMO_MSG_CLOSE,
    CVP_DEMO_MSG_RUN,
};

enum CVP_DEMO_STATE {
    CVP_DEMO_STATE_CLOSE = 0,
    CVP_DEMO_STATE_OPEN,
};

struct cvp_mic_fmt {
    u8 ch;
    u8 gain[AUDIO_ADC_MAX_NUM];
    int sr;
};

typedef struct {
    u8 monitor;
    u8 monitor_start;
    u8 state;
    u8 run_busy;
    u8 mic_num;
    struct audio_adc_output_hdl adc_output;
    struct adc_mic_ch mic_ch;
    s16 *mic_buf[AUDIO_ADC_MAX_NUM];	//mic buff缓存指针
    s16 *irq_buf;
    struct audio_dac_channel dac_ch;
#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_LR)//双声道数据结构
    s16 tmp_buf_1[320 * 2];
#endif
} audio_cvp_t;

static void mic_open(struct cvp_mic_fmt *fmt);
static int cvp_monitor_output(s16 *data, int len);

audio_cvp_t *cvp_demo = NULL;

/*算法异步线程输出回调*/
static int cvp_output_hdl(s16 *data, u16 len)
{
    putchar('o');
    if (cvp_demo->monitor == CVP_MONITOR_POST) {
        cvp_monitor_output(data, len);
    }
#if CVP_DEMO_UART_EXPORT_ENABLE
    aec_uart_fill_v2(2, data, len);
    aec_uart_write_v2();			//所有通道数据填充完成，write&send data
#endif
    return len;
}

//算法启动API
static int cvp_demo_algo_open(void)
{
    struct audio_aec_init_param_t init_param;
    init_param.sample_rate = CVP_MIC_SR;
    init_param.ref_sr = 0;
    init_param.ref_channel = 1;
    init_param.node_uuid = 0;	//不用可视化算法节点配置

#if CVP_DEMO_IN_HOUSE_ENABLE
//自研双MIC算法demo
#if CVP_MIC_CH_NUM == 2
    audio_aec_open(&init_param, (ANS_EN | ENC_EN), cvp_output_hdl);

//自研单MIC算法demo
#elif CVP_MIC_CH_NUM == 1
    audio_aec_open(&init_param, (ANS_EN), cvp_output_hdl);
#endif
#endif
    return 0;
}

//算法关闭API
static int cvp_demo_algo_close(void)
{
#if CVP_DEMO_IN_HOUSE_ENABLE
    //自研common算法demo
    audio_aec_close();
#endif
    return 0;
}

//算法运行
static int cvp_demo_algo_run(int len)
{
    /*
       参数说明：

    	1、len :单MIC数据帧长（byte）

    	2、cvp_demo->mic_buf: MIC指针数组
       	按MIC序号大小顺序填充
       	如CVP_MIC_SEL == AUDIO_ADC_MIC_0 | AUDIO_ADC_MIC_1
       	mic_buf[0] : AUDIO_ADC_MIC_0
       	mic_buf[1] : AUDIO_ADC_MIC_1

       	如CVP_MIC_SEL == AUDIO_ADC_MIC_1 | AUDIO_ADC_MIC_3 | AUDIO_ADC_MIC_4
       	mic_buf[0] : AUDIO_ADC_MIC_1
       	mic_buf[1] : AUDIO_ADC_MIC_3
       	mic_buf[2] : AUDIO_ADC_MIC_4
     */

#if CVP_DEMO_IN_HOUSE_ENABLE
//自研双MIC算法demo
#if CVP_MIC_CH_NUM == 2
    //数据导出控制
#if CVP_DEMO_UART_EXPORT_ENABLE
    aec_uart_fill_v2(0, cvp_demo->mic_buf[0], len);
    aec_uart_fill_v2(1, cvp_demo->mic_buf[1], len);
#endif
    audio_aec_inbuf_ref(cvp_demo->mic_buf[0], len);
    audio_aec_inbuf(cvp_demo->mic_buf[0], len);

//自研单MIC算法demo
#elif CVP_MIC_CH_NUM == 1
    //数据导出控制
#if CVP_DEMO_UART_EXPORT_ENABLE
    aec_uart_fill_v2(0, cvp_demo->mic_buf[0], len);
#endif
    audio_aec_inbuf(cvp_demo->mic_buf[0], len);

#endif
#endif
    return 0;
}

/*监听输出：默认输出到dac*/
static int cvp_monitor_output(s16 *data, int len)
{
    //预备10ms 填充数据，避免DAC起播欠载打印U
    if (!cvp_demo->monitor_start) {
        cvp_demo->monitor_start = 1;
        int zero_len = (CVP_MIC_SR * 10 / 1000) * sizeof(short);
        /* printf("zero_len %d", zero_len); */
        s16 *zero_data = zalloc(zero_len);
        cvp_monitor_output(zero_data, zero_len);
        free(zero_data);
    }
#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_LR)//双声道数据结构
    int i = 0;
    for (i = 0; i < (len / 2); i++) {
        cvp_demo->tmp_buf_1[i * 2] = data[i];
        cvp_demo->tmp_buf_1[i * 2 + 1] = data[i];
    }
    /* int wlen = audio_dac_write(&dac_hdl, cvp_demo->tmp_buf_1, len * 2); */
    int wlen = audio_dac_channel_write((void *)&cvp_demo->dac_ch, cvp_demo->tmp_buf_1, len * 2);
    if (wlen != (len * 2)) {
        CVP_LOG("monitor output full\n");
    }
#else //单声道数据结构

    /* int wlen = audio_dac_write(&dac_hdl, data, len); */
    int wlen = audio_dac_channel_write((void *)&cvp_demo->dac_ch, data, len);
    if (wlen != len) {
        CVP_LOG("monitor output full\n");
    }
#endif
    return wlen;
}

/*监听使能*/
static int cvp_monitor_en(u8 en, int sr)
{
    CVP_LOG("cvp_monitor_en:%d,sr:%d\n", en, sr);
    if (en) {
        DAC_NOISEGATE_OFF();	//关闭底噪优化
        app_audio_state_switch(APP_AUDIO_STATE_MUSIC, app_audio_volume_max_query(AppVol_BT_MUSIC), NULL);
        CVP_LOG("cur_vol:%d,max_sys_vol:%d\n", app_audio_get_volume(APP_AUDIO_STATE_MUSIC), app_audio_volume_max_query(AppVol_BT_MUSIC));
        audio_dac_set_volume(&dac_hdl, app_audio_volume_max_query(AppVol_BT_MUSIC));
        audio_dac_set_sample_rate(&dac_hdl, sr);
        /* audio_dac_start(&dac_hdl); */

        struct audio_dac_channel_attr attr;
        attr.delay_time = 50;
        attr.protect_time = 8;
        attr.write_mode = WRITE_MODE_BLOCK;
        audio_dac_new_channel(&dac_hdl, &cvp_demo->dac_ch);
        audio_dac_channel_set_attr(&cvp_demo->dac_ch, &attr);
        audio_dac_channel_start(&cvp_demo->dac_ch);

    } else {
        DAC_NOISEGATE_ON();		//启动底噪优化
        /* audio_dac_stop(&dac_hdl); */
        /* audio_dac_close(&dac_hdl); */
        if (audio_dac_is_working(&dac_hdl)) {
            audio_dac_channel_close(&cvp_demo->dac_ch);
        }
    }
    return 0;
}

/*mic adc原始数据输出*/
static void mic_output(void *priv, s16 *data, int len)
{
    putchar('.');

#if CVP_DEMO_IN_HOUSE_ENABLE
    audio_cvp_phase_align();
#endif

    int points = len >> 1;

    for (int i = 0; i < points; i++) {
        for (int index = 0; index < cvp_demo->mic_num; index++) {
            cvp_demo->mic_buf[index][i] =  data[i * cvp_demo->mic_num + index];
        }
    }

    os_taskq_post_msg("cvp_demo", 2, CVP_DEMO_MSG_RUN, len);
}

static void mic_open(struct cvp_mic_fmt *fmt)
{
    CVP_LOG("mic_open\n");
    if (cvp_demo) {
#if 0   // 使用可视化ADC配置
        extern void audio_adc_file_init(void);
        extern int adc_file_mic_open(struct adc_mic_ch * mic, int ch);
        audio_adc_file_init();
        for (int = 0; i < AUDIO_ADC_MAX_NUM; i++) {
            if (fmt->ch & BIT(i)) {
                /* adc_file_mic_open(&cvp_demo->mic_ch, AUDIO_ADC_MIC_0); */
                adc_file_mic_open(&cvp_demo->mic_ch, BIT(i));
            }
        }
#else
        audio_mic_pwr_ctl(MIC_PWR_ON);
        struct mic_open_param mic_param = {0};
        if (fmt->ch & AUDIO_ADC_MIC_0) {
            mic_param.mic_mode      = AUDIO_MIC_CAP_MODE;
            mic_param.mic_ain_sel   = AUDIO_MIC0_CH0;
            mic_param.mic_bias_sel  = AUDIO_MIC_BIAS_CH0;
            mic_param.mic_bias_rsel = 4;
            mic_param.mic_dcc       = 8;
            audio_adc_mic_open(&cvp_demo->mic_ch, AUDIO_ADC_MIC_0, &adc_hdl, &mic_param);
            audio_adc_mic_set_gain(&cvp_demo->mic_ch, AUDIO_ADC_MIC_0, fmt->gain[0]);
            audio_adc_mic_gain_boost(AUDIO_ADC_MIC_0, 1);
        }

        if (fmt->ch & AUDIO_ADC_MIC_1) {
            mic_param.mic_mode      = AUDIO_MIC_CAP_MODE;
            mic_param.mic_ain_sel   = AUDIO_MIC1_CH0;
            mic_param.mic_bias_sel  = AUDIO_MIC_BIAS_CH1;
            mic_param.mic_bias_rsel = 4;
            mic_param.mic_dcc       = 8;
            audio_adc_mic_open(&cvp_demo->mic_ch, AUDIO_ADC_MIC_1, &adc_hdl, &mic_param);
            audio_adc_mic_set_gain(&cvp_demo->mic_ch, AUDIO_ADC_MIC_1, fmt->gain[1]);
            audio_adc_mic_gain_boost(AUDIO_ADC_MIC_1, 1);
        }

        /*
        if (fmt->ch & AUDIO_ADC_MIC_2) {
        	mic_param.mic_mode      = AUDIO_MIC_CAP_MODE;
        	mic_param.mic_ain_sel   = AUDIO_MIC2_CH0;
        	mic_param.mic_bias_sel  = AUDIO_MIC_BIAS_CH2;
        	mic_param.mic_bias_rsel = 4;
        	mic_param.mic_dcc       = 8;
        	audio_adc_mic_open(&cvp_demo->mic_ch, AUDIO_ADC_MIC_2, &adc_hdl, &mic_param);
        	audio_adc_mic_set_gain(&cvp_demo->mic_ch, AUDIO_ADC_MIC_2, fmt->gain[2]);
        	audio_adc_mic_gain_boost(AUDIO_ADC_MIC_2, 1);
        }
        */

        /*
        if (fmt->ch & AUDIO_ADC_MIC_3) {
        	mic_param.mic_mode      = AUDIO_MIC_CAP_MODE;
        	mic_param.mic_ain_sel   = AUDIO_MIC3_CH0;
        	mic_param.mic_bias_sel  = AUDIO_MIC_BIAS_CH3;
        	mic_param.mic_bias_rsel = 4;
        	mic_param.mic_dcc       = 8;
        	audio_adc_mic_open(&cvp_demo->mic_ch, AUDIO_ADC_MIC_3, &adc_hdl, &mic_param);
        	audio_adc_mic_set_gain(&cvp_demo->mic_ch, AUDIO_ADC_MIC_3, fmt->gain[3]);
        	audio_adc_mic_gain_boost(AUDIO_ADC_MIC_3, 1);
        }
        */

        /*
        if (fmt->ch & AUDIO_ADC_MIC_4) {
        	mic_param.mic_mode      = AUDIO_MIC_CAP_MODE;
        	mic_param.mic_ain_sel   = AUDIO_MIC4_CH0;
        	mic_param.mic_bias_sel  = AUDIO_MIC_BIAS_CH4;
        	mic_param.mic_bias_rsel = 4;
        	mic_param.mic_dcc       = 8;
        	audio_adc_mic_open(&cvp_demo->mic_ch, AUDIO_ADC_MIC_4, &adc_hdl, &mic_param);
        	audio_adc_mic_set_gain(&cvp_demo->mic_ch, AUDIO_ADC_MIC_4, fmt->gain[4]);
        	audio_adc_mic_gain_boost(AUDIO_ADC_MIC_4, 1);
        }
        */
#endif

        audio_adc_mic_set_sample_rate(&cvp_demo->mic_ch, fmt->sr);
        audio_adc_mic_set_buffs(&cvp_demo->mic_ch, cvp_demo->irq_buf, MIC_IRQ_POINTS * 2, MIC_BUF_NUM);
        cvp_demo->adc_output.handler = mic_output;
        audio_adc_add_output_handler(&adc_hdl, &cvp_demo->adc_output);
        audio_adc_mic_start(&cvp_demo->mic_ch);
    }
    CVP_LOG("mic_open succ\n");
}

static void mic_close(void)
{
    if (cvp_demo) {
        audio_adc_mic_close(&cvp_demo->mic_ch);
        audio_adc_del_output_handler(&adc_hdl, &cvp_demo->adc_output);
        audio_mic_pwr_ctl(MIC_PWR_OFF);
    }
}

static void audio_cvp_demo_task(void *p)
{
    int res;
    int msg[16];
    CVP_LOG(">>>CVP DEMO TASK<<<\n");
    while (1) {
        res = os_taskq_pend(NULL, msg, ARRAY_SIZE(msg));
        if (res == OS_TASKQ) {
            switch (msg[1]) {
            case CVP_DEMO_MSG_RUN:
                if (cvp_demo->state == CVP_DEMO_STATE_OPEN) {
                    cvp_demo->run_busy = 1;
                    int len = msg[2];
                    if (cvp_demo->monitor == CVP_MONITOR_PROBE) {
                        cvp_monitor_output(cvp_demo->mic_buf[0], len);
                    }
                    cvp_demo_algo_run(len);
                    /* putchar('r'); */
                    cvp_demo->run_busy = 0;
                }
                break;
            }
        } else {
            CVP_LOG("res:%d,%d", res, msg[1]);
        }
    }
}

int audio_cvp_demo_open(void)
{
    if (cvp_demo == NULL) {
        cvp_demo = zalloc(sizeof(audio_cvp_t));
        ASSERT(cvp_demo);
        cvp_demo->monitor = CVP_MONITOR_SEL;
        if (cvp_demo->monitor) {
            cvp_monitor_en(1, CVP_MIC_SR);
        }

        clock_alloc("CVP_DEMO", CVP_DEMO_CLOCK);

#if CVP_DEMO_IN_HOUSE_ENABLE
        cvp_node_context_setup(CVP_DEMO_NODE_UUID);
#endif

#if CVP_DEMO_UART_EXPORT_ENABLE
        aec_uart_open_v2(CVP_DEMO_UART_EXPORT_CH, CVP_DEMO_UART_EXPORT_PACKET_SIZE);
#endif

        u8 mic_num = 0;
        u8 mic_ch = CVP_MIC_CH_SEL;
        struct cvp_mic_fmt mic_fmt;

        while (mic_ch) {
            mic_ch >>= 1;
            mic_num++;
            if (mic_num > AUDIO_ADC_MAX_NUM) {
                CVP_LOG("mic_num is full, mic_num %d > max_mic_num %d\n", mic_num, AUDIO_ADC_MAX_NUM);
                return 1;
            }
        }
        mic_fmt.ch = CVP_MIC_CH_SEL;
        mic_fmt.sr = CVP_MIC_SR;
        cvp_demo->mic_num = mic_num;
        cvp_demo->irq_buf = malloc(MIC_BUF_NUM * MIC_IRQ_POINTS * mic_num * sizeof(short));
        for (int i = 0; i < mic_num; i++) {
            cvp_demo->mic_buf[i] = zalloc(MIC_IRQ_POINTS * sizeof(short));
            mic_fmt.gain[i] = CVP_MIC_GAIN;
        }
        mic_open(&mic_fmt);

        cvp_demo_algo_open();

        task_create(audio_cvp_demo_task, NULL, "cvp_demo");
        cvp_demo->state = CVP_DEMO_STATE_OPEN;

        CVP_LOG("cvp_test open\n");
        CVP_LOG("mic_fmt: ch %x, num %d, gain %d, sr %d\n", mic_ch, mic_num, CVP_MIC_GAIN, CVP_MIC_SR);
    }
    return 0;
}

int audio_cvp_demo_close(void)
{
    if (cvp_demo) {
        cvp_demo->state = CVP_DEMO_STATE_CLOSE;
        int wait_cnt = 100;
        while (cvp_demo->run_busy && wait_cnt) {
            wait_cnt--;
            os_time_dly(1);
        }
        mic_close();
        if (cvp_demo->monitor) {
            cvp_monitor_en(0, CVP_MIC_SR);
        }

        cvp_demo_algo_close();

#if CVP_DEMO_UART_EXPORT_ENABLE
        aec_uart_close_v2();
#endif
        /* free(cvp_demo->irq_buf);  //在ADC驱动释放,这里不需要释放 */
        for (int i = 0; i < cvp_demo->mic_num; i++) {
            free(cvp_demo->mic_buf[i]);
        }
        free(cvp_demo);
        cvp_demo = NULL;
        task_kill("cvp_demo");
        CVP_LOG("cvp_test close\n");
    }
    return 0;
}

/*清晰语音测试模块*/
int audio_cvp_test(void)
{
    if (cvp_demo == NULL) {
        audio_cvp_demo_open();
    } else {
        audio_cvp_demo_close();
    }
    return 0;
}

static u8 audio_cvp_idle_query()
{
    return (cvp_demo == NULL) ? 1 : 0;
}
REGISTER_LP_TARGET(audio_cvp_lp_target) = {
    .name = "audio_cvp",
    .is_idle = audio_cvp_idle_query,
};

#if 0
/*
*********************************************************************
*                  KSR Echo Cancellation
*
* Description: 语音识别回音消除，用来消除语音识别中，mic从speaker中拾
*			   取到的回音，提高语音识别率
* Note(s)    : KSR = Keyword Speech Recognition
*********************************************************************
*/
#define KSR_MIC_BUF_NUM        2
#define KSR_MIC_IRQ_POINTS     256
#define KSR_MIC_BUFS_SIZE      (KSR_MIC_BUF_NUM * KSR_MIC_IRQ_POINTS)

struct KSR_mic_var {
    u16 in_sr;
    u16 ref_sr;
    struct audio_adc_output_hdl adc_output;
    struct adc_mic_ch mic_ch;
    s16 mic_buf[KSR_MIC_BUFS_SIZE];
};
struct KSR_mic_var *KSR_mic = NULL;

/*ADC mic采样数据输出*/
static void KSR_mic_output(void *priv, s16 *data, int len)
{
    putchar('o');
    audio_aec_inbuf(data, len);
}

/*回音消除后的数据输出*/
static int KSR_mic_aec_output(s16 *data, u16 len)
{
    putchar('O');
    return len;
}

int KSR_aec_open(u16 in_sr, u16 ref_sr)
{
    u16 mic_sr = 16000;
    u8 mic_gain = 10;
    int err = 0;

    printf("KSR_aec_open\n");
    if (KSR_mic == NULL) {
        KSR_mic = zalloc(sizeof(struct KSR_mic_var));
        KSR_mic->in_sr = in_sr;
        KSR_mic->ref_sr = ref_sr;

        /*根据资源情况，配置AEC模块使能*/
        u16 aec_enablebit = NLP_EN;
        //u16 aec_enablebit = AEC_EN;
        //u16 aec_enablebit = AEC_EN | NLP_EN;
        struct audio_aec_init_param_t init_param;
        init_param.sample_rate = mic_sr;
        init_param.ref_sr = 0;
        err = audio_aec_open(&init_param, aec_enablebit, KSR_mic_aec_output);
        if (err) {
            printf("KSR aec open err:%d\n", err);
            return -1;
        }

        audio_adc_mic_open(&KSR_mic->mic_ch, AUDIO_ADC_MIC_CH, &adc_hdl);
        audio_adc_mic_set_sample_rate(&KSR_mic->mic_ch, mic_sr);
        audio_adc_mic_set_gain(&KSR_mic->mic_ch, mic_gain);
        audio_adc_mic_set_buffs(&KSR_mic->mic_ch, KSR_mic->mic_buf, KSR_MIC_IRQ_POINTS * 2, KSR_MIC_BUF_NUM);
        KSR_mic->adc_output.handler = KSR_mic_output;
        audio_adc_add_output_handler(&adc_hdl, &KSR_mic->adc_output);
        audio_adc_mic_start(&KSR_mic->mic_ch);
        printf("KSR aec open succ\n");
    }

    return 0;
}

int KSR_aec_info(u8 idx)
{
    if (KSR_mic == NULL) {
        return -1;
    }
    switch (idx) {
    case 0:
        return KSR_mic->in_sr;
    case 1:
        return KSR_mic->ref_sr;
    }
    return -1;
}


void KSR_aec_close(void)
{
    printf("KSR_aec_close\n");
    if (KSR_mic) {
        /*关闭adc_mic采样*/
        audio_adc_del_output_handler(&adc_hdl, &KSR_mic->adc_output);
        audio_adc_mic_close(&KSR_mic->mic_ch);
        free(KSR_mic);
        KSR_mic = NULL;
        /*关闭aec模块*/
        audio_aec_close();
    }
}
#endif

#endif/*AUDIO_CVP_DEMO*/


