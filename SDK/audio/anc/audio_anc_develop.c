/*
============================================================================

							ANC 第三方算法开发说明
	注意:
		ADC+DAC 数据通路 256 Points @ 16K Sample Rate
			1、DAC采样率限制:参考数据复用DACBUFFER，要求DAC固定全局
			采样率，且采样率为MIC采样率的整数倍，如48K，32K;
			2、MIC 参数限制:如算法兼容通话场景，则有此限制;
			(1)MIC采样率，需支持 8k/16k，算法需支持切换;
			(2)MIC中断点数固定256 Points，兼容JL通话算法;
		ANC:FF+FB+DAC(Audio+ANC) 数据通路 174/176 Points @ 16K Sample Rate
			1、需ANC启动之后才有数据
=============================================================================
*/


#include "app_config.h"
#if (TCFG_AUDIO_ANC_ENABLE && AUDIO_ANC_DATA_EXPORT_VIA_UART)
#include "audio_anc_includes.h"
#include "app_tone.h"
#include "adc_file.h"
#include "system/includes.h"
#include "audio_config.h"
#include "asm/dac.h"
#include "aec_uart_debug.h"
#include "Resample_api.h"
#include "audio_src.h"
#include "icsd_common_v2.h"
#include "generic/circular_buf.h"
#include "clock_manager/clock_manager.h"
#include "btstack/avctp_user.h"
#include "hw_eq.h"

#if 1
#define dev_log	printf
#else
#define dev_log(...)
#endif


#define ANC_DEV_CLOCK_ALLOC	120 * 1000000L //算法运行时钟申请

//------------------ADC 配置-------------------------
#define MIC_BUF_NUM        	3		//ADC缓存BUFF
#define MIC_IRQ_POINTS     	256		//ADC中断点数
#define MIC_SR				16000	//目标SRC 采样率
#define MIC_DUMP_PACKET_CNT 8		//ADC启动丢包个数

#define CVP_MONITOR_DISABLE	0		//监听关闭
#define CVP_MONITOR_PROBE	1		//监听处理前数据
//默认监听选择
#define CVP_MONITOR_SEL		CVP_MONITOR_DISABLE

//以下3个宏只能选其中一个
#define DAC_REF_SW_SRC_ENABLE           0   //软件SRC : 下行数据下采样使能
#define ANC_DEV_HW_SRC_ENABLE           1   //硬件SRC
/*
    下采样至 23.4375k,
    ANC_DEV_DMA_INPUT_POINTS需>=256
    dump 数据2ch 可能数据会丢失(串口速度问题)
*/
#define ANC_DEV_DOWN_SR_23K_EN          0

//------------------ANC DMA配置----------------------

#define ANC_DEV_DMA_IRT_POINTS	2048	//ANC中断点数
#define ANC_DEV_DMA_CIC8_LEN  	(ANC_DEV_DMA_IRT_POINTS >> 2) //irq_points / 8 * sizeof(short)
#define ANC_DEV_DMA_INPUT_POINTS	120
#define ANC_DEV_DMA_CBUF_LEN	ANC_DEV_DMA_INPUT_POINTS * 2 * 3	//MIN = 200byte @ DMA_IRQ = 2048 Points

//------------------数据导出配置---------------------
#define ANC_UART_EXPORT_ENABLE			1
#define ANC_UART_EXPORT_CH				3
#define ANC_UART_EXPORT_LEN				512
#define ANC_UART_EXPORT_PACKET_SIZE		((ANC_UART_EXPORT_LEN + 20) * ANC_UART_EXPORT_CH)

//-----------------SPP 在线调试使能------------------
#define ANC_DEV_ONLINE_SPP_DEBUG_EN     1

/*AFQ COMMON MSG List*/
enum ANC_DEV_MSG {
    ANC_DEV_MSG_OPEN = 0xA1,
    ANC_DEV_MSG_CLOSE,
    ANC_DEV_MSG_RUN,
};

enum ANC_DEV_STATE {
    ANC_DEV_STATE_END = 0,
    ANC_DEV_STATE_CLOSE,
    ANC_DEV_STATE_OPEN,
};

enum ANC_DEV_MIC_TYPE {
    ANC_DEV_MIC_FF = 0x1,
    ANC_DEV_MIC_FB = 0x2,
    ANC_DEV_MIC_TALK = 0x4,
};
#if DAC_REF_SW_SRC_ENABLE
struct sw_src_hdl {
    RS_STUCT_API *sw_src_api;
    u32 *sw_src_buf;
};
#endif

struct anc_develop_mic_hdl {
    u8 monitor;
    u8 dump_cnt;
    u8 mic_num;	//当前MIC个数
    u16 mic_id[AUDIO_ADC_MAX_NUM];
    struct audio_adc_output_hdl adc_output;
    struct adc_mic_ch mic_ch;
    s16 *tmp_buf[AUDIO_ADC_MAX_NUM - 1];
    s16 *mic_buf;

    s16 *ff_buf;
    s16 *fb_buf;
    s16 *ref_tmpbuf;
    int ref_tmpbuf_len;

#if DAC_REF_SW_SRC_ENABLE
    struct sw_src_hdl ref_src;
#endif
};

struct anc_develop_anc_ch_hdl {
    s16 buf[ANC_DEV_DMA_CIC8_LEN];	//下采样缓存
    cbuffer_t cbuf;
    u8 cbuf_buf[ANC_DEV_DMA_CBUF_LEN];	//算法输入CBUF缓存
    s16 in_buf[ANC_DEV_DMA_INPUT_POINTS];	//算法输入BUF
#if DAC_REF_SW_SRC_ENABLE
    struct sw_src_hdl src;
#endif
    void *hw_src;
};

struct anc_develop_anc_dma_hdl {
    int *irq_buf;
    s16 *tmp_buf;
    int cic8_len;

    struct anc_develop_anc_ch_hdl ff;
    struct anc_develop_anc_ch_hdl fb;
    struct anc_develop_anc_ch_hdl ref;
};

struct anc_develop_hdl {
    u8 run_enable;                      //有足够的数据，可以跑算法
    enum ANC_DEV_STATE state;
    enum ANC_DEV_DATA_SEL data_sel;     //ANC_DEV算法数据来源
    int run_busy;                       //算法运行标志，用于判断临界操作
    int dump_en;                        //数据dump使能
    u8 printf_en;
    float ff_multiple_gain;             //DEBUG: FF 补偿增益
    float fb_multiple_gain;             //DEBUG: FB 补偿增益

    int anc_coeff_mode;			        //ANC mode
    int prompt_flag;			        //提示音检测帧计数
    int frame_cnt;				        //算法帧计数
    int anc_flag;                       //ANC帧计数

    double *ff_coeff;
    double *fb_coeff;
    double *cmp_coeff;

    s16 src_buf[3][2];
    OS_SEM *sem;
    spinlock_t lock;
    struct anc_develop_mic_hdl *mic;
    struct anc_develop_anc_dma_hdl *anc_dma;
};

extern struct audio_adc_hdl adc_hdl;

#if ANC_DEV_HW_SRC_ENABLE
void *audio_anc_dev_hw_src_open(u32 in_sr, u32 out_sr);
int audio_anc_dev_hw_src_run(void *p, s16 *data, int len, void *cbuf);
int audio_anc_dev_hw_src_close(void *p);
#endif

static struct anc_develop_hdl *hdl = NULL;

#if DAC_REF_SW_SRC_ENABLE
static int sw_src_init(struct sw_src_hdl *src, u8 nch, u32 insample, u32 outsample);
static int sw_src_run(struct sw_src_hdl *src, s16 *indata, s16 *outdata, u16 len);
static void sw_src_exit(struct sw_src_hdl *src);
#endif

int audio_anc_develop_spp_tx_process(u8 *data, int len);

static uint8_t sp_show[50] = {0};
static uint8_t high_bit = 0;
void sp_transfer_show(int sp, int mode, int filter_idx, int noise_level, int self_talk)
{
    int temp_value = sp;
    high_bit = 0;
    int noise_level_bit = 0;
    for (int j = 0; j < 50; j++) {
        sp_show[j] = 0;
    }
    if (sp < 0) {
        sp_show[0] = '-';
        for (int j = 0; j < 20; j++) {
            temp_value = (-temp_value) / 10;
            high_bit++;
            if (temp_value == 0) {
                break;
            }
        }
        temp_value = -sp;
        for (int k = 0; k < high_bit; k++) {
            sp_show[high_bit - k] = temp_value % 10 + '0';
            temp_value =  temp_value / 10;
        }
        high_bit++;
    } else if (sp > 0) {
        for (int j = 0; j < 20; j++) {
            temp_value = temp_value / 10;
            high_bit++;
            if (temp_value == 0) {
                break;
            }
        }
        temp_value = sp;
        for (int k = 0; k < high_bit; k++) {
            sp_show[high_bit - 1 - k] = temp_value % 10 + '0';
            temp_value =  temp_value / 10;
        }
    } else {
        sp_show[0] = '0';
        high_bit++;
    }
    sp_show[high_bit] = ' ';

    sp_show[high_bit + 1] = mode / 10 + '0';
    sp_show[high_bit + 2] = mode % 10 + '0';
    high_bit = high_bit + 3;

    sp_show[high_bit] = ' ';

    sp_show[high_bit + 1] = filter_idx / 10 + '0';
    sp_show[high_bit + 2] = filter_idx % 10 + '0';
    high_bit = high_bit + 3;

    sp_show[high_bit] = ' ';

    sp_show[high_bit + 1] = self_talk % 10 + '0';
    high_bit = high_bit + 2;

    sp_show[high_bit] = ' ';
    high_bit++;

    temp_value = noise_level;
    if (noise_level < 0) {
        sp_show[high_bit] = '-';
        for (int j = 0; j < 20; j++) {
            temp_value = (-temp_value) / 10;
            high_bit++;
            noise_level_bit++;
            if (temp_value == 0) {
                break;
            }
        }
        temp_value = -noise_level;
        for (int k = 0; k < noise_level_bit; k++) {
            sp_show[high_bit - k] = temp_value % 10 + '0';
            temp_value =  temp_value / 10;
        }
        dev_log("noise_level %s", &sp_show[high_bit - noise_level_bit]);
        dev_log("noise_level %d", noise_level);
        high_bit++;
    } else if (noise_level > 0) {
        for (int j = 0; j < 20; j++) {
            temp_value = temp_value / 10;
            high_bit++;
            noise_level_bit++;
            if (temp_value == 0) {
                break;
            }
        }
        temp_value = noise_level;
        for (int k = 0; k < noise_level_bit; k++) {
            sp_show[high_bit - 1 - k] = temp_value % 10 + '0';
            temp_value =  temp_value / 10;
        }
    } else {
        sp_show[0] = '0';
        high_bit++;
    }
}

/*监听输出：默认输出到dac*/
static int cvp_monitor_output(s16 *data, int len)
{
    int wlen = audio_dac_write(&dac_hdl, data, len);
    if (wlen != len) {
        dev_log("monitor output full\n");
    }
    return wlen;
}

/*监听使能*/
static int cvp_monitor_en(u8 en, int sr)
{
    dev_log("cvp_monitor_en:%d,sr:%d\n", en, sr);
    if (en) {
        app_audio_state_switch(APP_AUDIO_STATE_MUSIC, app_audio_volume_max_query(AppVol_BT_MUSIC), NULL);	// 音量状态设置
        //audio_dac_set_volume(&dac_hdl, app_audio_volume_max_query(AppVol_BT_MUSIC));					// dac 音量设置
        audio_dac_set_volume(&dac_hdl, 16);					// dac 音量设置
        audio_dac_set_sample_rate(&dac_hdl, sr);
        audio_dac_start(&dac_hdl);
        audio_dac_channel_start(NULL);
    } else {
        audio_dac_stop(&dac_hdl);
        audio_dac_close(&dac_hdl);
    }
    return 0;
}

/*mic 中断原始数据输出， DAC参考数据读取*/
static void audio_anc_develop_mic_output(void *priv, s16 *data, int len)
{
    putchar('.');
    s16 *mic_data[hdl->mic->mic_num];
    int i, index;

    spin_lock(&hdl->lock);
    if (!hdl->mic) {
        spin_unlock(&hdl->lock);
        return;
    }
    //ADC启动不稳定，实测开头大约有50ms 杂音数据, 这里丢弃掉16*8 = 128ms
    if (hdl->mic->dump_cnt < MIC_DUMP_PACKET_CNT) {
        hdl->mic->dump_cnt++;
        spin_unlock(&hdl->lock);
        return;
    }

    int rlen = audio_dac_read(0, hdl->mic->ref_tmpbuf, hdl->mic->ref_tmpbuf_len, 1);
    if (rlen != (hdl->mic->ref_tmpbuf_len)) {
        /* putchar('r'); */
        r_printf("err anc_dev len %d\n", rlen);
        if (!rlen) {
            spin_unlock(&hdl->lock);
            return;
        }
    }


    mic_data[0] = data;
    //dev_log("mic_data:%x,%x,%d\n",data,mic1_data_pos,len);
    if (hdl->mic->mic_num > 1) {
        for (index = 1; index < hdl->mic->mic_num; index++) {
            mic_data[index] =  hdl->mic->tmp_buf[index - 1];
        }
        for (i = 0; i < (len >> 1); i++) {
            for (index = 0; index < hdl->mic->mic_num; index++) {
                mic_data[index][i] =  data[i * hdl->mic->mic_num + index];
            }
        }
    }

    for (i = 0; i < hdl->mic->mic_num; i++) {
#if ANC_UART_EXPORT_ENABLE
        aec_uart_fill_v2(i, mic_data[i], len);
#endif

        if (hdl->mic->mic_id[i] & ANC_DEV_MIC_FF) {
            hdl->mic->ff_buf = mic_data[i];
            continue;
        }
        if (hdl->mic->mic_id[i] & ANC_DEV_MIC_FB) {
            hdl->mic->fb_buf = mic_data[i];
            continue;
        }
    }

#if CVP_MONITOR_SEL != CVP_MONITOR_DISABLE
    if (hdl->mic->monitor == CVP_MONITOR_PROBE) {
        static u8 num = 0;
        static int cnt = 0;
        if (++cnt > 300) {
            cnt = 0;
            if (++num == hdl->mic->mic_num) {
                num = 0;
            }
            dev_log("num %d", num);
        }
        cvp_monitor_output(mic_data[num], len);
    }
#endif/*CVP_MONITOR_SEL != CVP_MONITOR_DISABLE*/

    spin_unlock(&hdl->lock);
    os_taskq_post_msg("anc_dev", 1, ANC_DEV_MSG_RUN);
}

//ANC MIC遍历
static int audio_anc_develop_mic_query(u8 mic_type, u16 mic_id, u8 *mic_en, u16 *mic_id_tmp, u8 *gain)
{
    u8 i;
    //遍历最大模拟ADC个数
    for (i = A_MIC0; i < AUDIO_ADC_MAX_NUM; i++) {
        if (mic_type == i) {
            gain[i] = audio_anc_mic_gain_get(i);
            mic_en[i] = 1;
            mic_id_tmp[i] |= mic_id;
            dev_log("ANC mic type %d, id %x, en %x", mic_type, mic_id, mic_en[i]);
            return 0;
        }
    }
    return 1;
}

// MIC OPEN
static int audio_anc_develop_mic_open(void)
{

    if (hdl->mic) {
        return -1;
    }

    int i, j, index;
    u8 mic_en[AUDIO_ADC_MAX_NUM] = {0};
    u8 mic_gain[AUDIO_ADC_MAX_NUM] = {0};
    u16 mic_id_tmp[AUDIO_ADC_MAX_NUM] = {0};

#if CVP_MONITOR_SEL != CVP_MONITOR_DISABLE
    int ref_sr = MIC_SR;
#else
    int ref_sr = (TCFG_AUDIO_GLOBAL_SAMPLE_RATE) ? TCFG_AUDIO_GLOBAL_SAMPLE_RATE : dac_hdl.sample_rate;
#endif

    hdl->mic = zalloc(sizeof(struct anc_develop_mic_hdl));
    hdl->mic->ref_tmpbuf_len = (ref_sr / MIC_SR) * MIC_IRQ_POINTS * sizeof(short);
    hdl->mic->ref_tmpbuf = zalloc(hdl->mic->ref_tmpbuf_len);
    dev_log("ref_sr %d, mic_sr %d, ref_tmpbuf_len %d\n", ref_sr, MIC_SR, hdl->mic->ref_tmpbuf_len);

#if DAC_REF_SW_SRC_ENABLE
    sw_src_init(&hdl->mic->ref_src, 1, ref_sr, MIC_SR);
#endif

#if CVP_MONITOR_SEL != CVP_MONITOR_DISABLE
    hdl->mic->monitor = CVP_MONITOR_SEL;
    if (hdl->mic->monitor) {
        cvp_monitor_en(1, MIC_SR);
    }
#endif/* CVP_MONITOR_SEL != CVP_MONITOR_DISABLE*/

    audio_adc_file_init();

    //映射ANC MIC通道
    audio_anc_develop_mic_query(TCFG_AUDIO_ANCL_FF_MIC, ANC_DEV_MIC_FF, mic_en, mic_id_tmp, mic_gain);
    audio_anc_develop_mic_query(TCFG_AUDIO_ANCL_FB_MIC, ANC_DEV_MIC_FB, mic_en, mic_id_tmp, mic_gain);

    //重新排列数组, 兼容ADC_BUFF的缓存结构
    j = 0;
    for (i = 0; i < AUDIO_ADC_MAX_NUM; i++) {
        if (mic_id_tmp[i]) {
            hdl->mic->mic_id[j++] = mic_id_tmp[i];
        }
    }

    //启动ADC
    audio_mic_pwr_ctl(MIC_PWR_ON);
    for (index = 0; index < AUDIO_ADC_MAX_NUM; index++) {
        if (mic_en[index]) {
            hdl->mic->mic_num++;
            adc_file_mic_open(&hdl->mic->mic_ch, BIT(index));
            audio_adc_mic_set_gain(&hdl->mic->mic_ch, BIT(index), mic_gain[index]);
        }
    }
    if (!hdl->mic->mic_num) {
        dev_log("Error valid mic_num is zero!\n");
        return 1;
    }
    for (i = 0; i < AUDIO_ADC_MAX_NUM; i++) {
        printf("mic_%d: en %d, id %d, gain %d\n", i, mic_en[i], hdl->mic->mic_id[i], mic_gain[i]);
    }

    //ADC临时BUFF、ISR BUFF申请
    for (i = 1; i < hdl->mic->mic_num; i++) {
        hdl->mic->tmp_buf[i - 1] = malloc(MIC_IRQ_POINTS * sizeof(short));
    }
    hdl->mic->mic_buf = malloc(hdl->mic->mic_num * MIC_BUF_NUM * MIC_IRQ_POINTS * sizeof(short));

    audio_adc_mic_set_sample_rate(&hdl->mic->mic_ch, MIC_SR);
    audio_adc_mic_set_buffs(&hdl->mic->mic_ch, hdl->mic->mic_buf, MIC_IRQ_POINTS * 2, MIC_BUF_NUM);

    hdl->mic->adc_output.handler = audio_anc_develop_mic_output;
    audio_adc_add_output_handler(&adc_hdl, &hdl->mic->adc_output);
    audio_adc_mic_start(&hdl->mic->mic_ch);
    dev_log("mic_open succ mic_num %d\n", hdl->mic->mic_num);
    return 0;
}

// MIC CLOSE
static int audio_anc_develop_mic_close(void)
{
    if (!hdl->mic) {
        return 0;
    }
    spin_lock(&hdl->lock);
#if CVP_MONITOR_SEL != CVP_MONITOR_DISABLE
    if (hdl->mic->monitor) {
        cvp_monitor_en(0, MIC_SR);
    }
#endif/* CVP_MONITOR_SEL != CVP_MONITOR_DISABLE*/
    if (hdl) {
        audio_adc_mic_close(&hdl->mic->mic_ch);
        audio_adc_del_output_handler(&adc_hdl, &hdl->mic->adc_output);
        for (int i = 1; i < hdl->mic->mic_num; i++) {
            free(hdl->mic->tmp_buf[i - 1]);
        }
        if (hdl->mic->mic_buf) {
            //在adc 驱动中释放了这个buffer
            /* free(hdl->mic->mic_buf); */
            hdl->mic->mic_buf = NULL;
        }
        audio_mic_pwr_ctl(MIC_PWR_OFF);
    }
    spin_unlock(&hdl->lock);
    return 0;
}

static void audio_anc_develop_mic_free(void)
{
    if (hdl->mic) {
#if DAC_REF_SW_SRC_ENABLE
        sw_src_exit(&hdl->mic->ref_src);
#endif
        free(hdl->mic->ref_tmpbuf);
        free(hdl->mic);
        hdl->mic = NULL;
    }
}

static void audio_anc_develop_mic_run()
{
    int rlen = 0;
#if DAC_REF_SW_SRC_ENABLE
    rlen = sw_src_run(&hdl->mic->ref_src, hdl->mic->ref_tmpbuf, hdl->mic->ref_tmpbuf, hdl->mic->ref_tmpbuf_len);
#endif
#if ANC_UART_EXPORT_ENABLE
    aec_uart_fill_v2(2, hdl->mic->ref_tmpbuf, rlen);
    aec_uart_write_v2();
#endif
}

//ANC DMA 中断处理函数
static void audio_anc_develop_anc_dma_done()
{
    int *cur_ptr;
    int done_flag = anc_dma_done_ppflag();
    cur_ptr = hdl->anc_dma->irq_buf + ((1 - done_flag) * 2 * ANC_DEV_DMA_IRT_POINTS);
    // int last_jiff = jiffies_usec();

#if 1   //4.3M mips
    icsd_common_ancdma_4ch_cic8(cur_ptr, hdl->anc_dma->tmp_buf, hdl->anc_dma->ref.buf, \
                                hdl->anc_dma->ff.buf, hdl->anc_dma->fb.buf, ANC_DEV_DMA_IRT_POINTS);
#elif 0 //71M mips
    static int dbuf[8] = {0};
    static int cbuf[8] = {0};
    icsd_cic8_4ch(cur_ptr, ANC_DEV_DMA_IRT_POINTS * 2, dbuf, cbuf, hdl->anc_dma->ref.buf, \
                  hdl->anc_dma->tmp_buf,  hdl->anc_dma->fb.buf, hdl->anc_dma->ff.buf);
#endif
    // int diff_jiff = jiffies_usec2offset(last_jiff, jiffies_usec());
    // printf("diff_jiff %d\n", diff_jiff);
    os_taskq_post_msg("anc_dev", 1, ANC_DEV_MSG_RUN);
}

static int audio_anc_develop_anc_dma_open()
{
    if (hdl->anc_dma) {
        return -1;
    }

    int cic8_len = ANC_DEV_DMA_IRT_POINTS / 8 * sizeof(short);

    hdl->anc_dma = zalloc(sizeof(struct anc_develop_anc_dma_hdl));

#if DAC_REF_SW_SRC_ENABLE
    sw_src_init(&hdl->anc_dma->ff.src, 1, 46875, MIC_SR);
    sw_src_init(&hdl->anc_dma->fb.src, 1, 46875, MIC_SR);
    sw_src_init(&hdl->anc_dma->ref.src, 1, 46875, MIC_SR);
#endif

#if ANC_DEV_HW_SRC_ENABLE
    hdl->anc_dma->ff.hw_src = audio_anc_dev_hw_src_open(46875, MIC_SR);
    hdl->anc_dma->fb.hw_src = audio_anc_dev_hw_src_open(46875, MIC_SR);
    hdl->anc_dma->ref.hw_src = audio_anc_dev_hw_src_open(46875, MIC_SR);
#endif

    cbuf_init(&hdl->anc_dma->ff.cbuf, hdl->anc_dma->ff.cbuf_buf, sizeof(hdl->anc_dma->ff.cbuf_buf));
    cbuf_init(&hdl->anc_dma->fb.cbuf, hdl->anc_dma->fb.cbuf_buf, sizeof(hdl->anc_dma->fb.cbuf_buf));
    cbuf_init(&hdl->anc_dma->ref.cbuf, hdl->anc_dma->ref.cbuf_buf, sizeof(hdl->anc_dma->ref.cbuf_buf));

    //points * ch * short * ppbuf
    hdl->anc_dma->irq_buf = malloc(ANC_DEV_DMA_IRT_POINTS * 4 * sizeof(short) * 2);

    hdl->anc_dma->tmp_buf = malloc(cic8_len);
    hdl->anc_dma->cic8_len = cic8_len;

    audio_anc_dma_add_output_handler("ANC_DEV", audio_anc_develop_anc_dma_done);

    anc_dma_on_double_4ch(13, 7, hdl->anc_dma->irq_buf, ANC_DEV_DMA_IRT_POINTS);
    return 0;
}

static int audio_anc_develop_anc_dma_close()
{
    anc_dma_off();
    audio_anc_dma_del_output_handler("ANC_DEV");
    return 0;
}

static void audio_anc_develop_anc_dma_free()
{
    if (hdl->anc_dma) {

#if DAC_REF_SW_SRC_ENABLE
        sw_src_exit(&hdl->anc_dma->ff.src);
        sw_src_exit(&hdl->anc_dma->fb.src);
        sw_src_exit(&hdl->anc_dma->ref.src);
#endif
#if ANC_DEV_HW_SRC_ENABLE
        audio_anc_dev_hw_src_close(hdl->anc_dma->ff.hw_src);
        audio_anc_dev_hw_src_close(hdl->anc_dma->fb.hw_src);
        audio_anc_dev_hw_src_close(hdl->anc_dma->ref.hw_src);
#endif
        free(hdl->anc_dma->irq_buf);
        free(hdl->anc_dma->tmp_buf);
        free(hdl->anc_dma);
        hdl->anc_dma = NULL;
    }
}

#if ANC_DEV_DOWN_SR_23K_EN
static s16 anc_dev_src_out_buf[3][256];
static void dsf2(s16 *input, s16 *output, s16 *buf, int len)
{
    s16 tmp;
    for (int i = 0; i < len; i += 2) {
        output[i / 2] = (buf[0] + 2 * buf[1] + input[i]) >> 2;
        buf[0] = input[i];
        buf[1] = input[i + 1];
    }
}
#endif

#if ANC_DEV_HW_SRC_ENABLE
void audio_anc_dev_hw_src_output(void *cbuf, s16 *data, int len)
{
    int wlen = cbuf_write((cbuffer_t *)cbuf, data, len);
    if (wlen != len) {
        dev_log("[ANC_DEV]Error: Cbuf w full %p\n", cbuf);
    }
}
#endif

static int audio_anc_develop_anc_dma_ch_prcoess(u8 export_ch, struct anc_develop_anc_ch_hdl *ch)
{
    int rlen = 0;
#if DAC_REF_SW_SRC_ENABLE
    rlen = sw_src_run(&ch->src, ch->buf, ch->buf, hdl->anc_dma->cic8_len);
    int wlen = cbuf_write(&ch->cbuf, ch->buf, rlen);
    if (wlen != rlen) {
        dev_log("[ANC_DEV]Error: Cbuf w full %d\n", export_ch);
    }
#endif

#if ANC_DEV_HW_SRC_ENABLE
    audio_anc_dev_hw_src_run(ch->hw_src, ch->buf, hdl->anc_dma->cic8_len, (void *)&ch->cbuf);
#endif

#if ANC_DEV_DOWN_SR_23K_EN
    //dsf2(ch->buf, &(anc_dev_src_out_buf[export_ch][0]), &(hdl->src_buf[export_ch][0]) ,hdl->anc_dma->cic8_len/2);
    //rlen = hdl->anc_dma->cic8_len/2;
    //int wlen = cbuf_write(&ch->cbuf, &(anc_dev_src_out_buf[export_ch][0]), rlen);
    rlen = hdl->anc_dma->cic8_len;
    int wlen = cbuf_write(&ch->cbuf, ch->buf, rlen);
    if (wlen != rlen) {
        dev_log("[ANC_DEV]Error: Cbuf w full %d\n", export_ch);
    }
#endif

    if (ch->cbuf.data_len >= (ANC_DEV_DMA_INPUT_POINTS  << 1)) {
        cbuf_read(&ch->cbuf, ch->in_buf, ANC_DEV_DMA_INPUT_POINTS << 1);
        hdl->run_enable = 1;
    }
    return rlen;
}

void audio_anc_develop_anc_mode_set(u8 coeff_mode)
{
    hdl->anc_coeff_mode = coeff_mode;
    if (coeff_mode) {
        audio_anc_mult_scene_update(coeff_mode);
    }
}

static void audio_anc_develop_anc_dma_run()
{
    /* int last_jiff = jiffies_usec(); */

    struct anc_develop_anc_dma_hdl *dma = hdl->anc_dma;
    audio_anc_develop_anc_dma_ch_prcoess(0, &dma->ff);
    audio_anc_develop_anc_dma_ch_prcoess(1, &dma->fb);
    audio_anc_develop_anc_dma_ch_prcoess(2, &dma->ref);
    if (hdl->printf_en) {
        wdt_close();
        spin_lock(&hdl->lock);
        for (int i = 0; i < ANC_DEV_DMA_INPUT_POINTS; i++) {
            printf("%6d, %6d\n", dma->fb.in_buf[i], dma->ref.in_buf[i]);
        }
        spin_unlock(&hdl->lock);
        hdl->printf_en = 0;
    }

    /* int diff_jiff = jiffies_usec2offset(last_jiff, jiffies_usec()); */
    /* printf("diff_jiff %d\n", diff_jiff); */

    //hdl->prompt_flag++;
    //hdl->anc_flag++;
    if (hdl->run_enable) {
        hdl->run_enable = 0;
        hdl->frame_cnt++;
#if ANC_UART_EXPORT_ENABLE
        if (hdl->dump_en) {
            int export_len = ANC_DEV_DMA_INPUT_POINTS << 1;
            aec_uart_fill_v2(0, dma->ff.in_buf, export_len);
            aec_uart_fill_v2(1, dma->fb.in_buf, export_len);
            aec_uart_fill_v2(2, dma->ref.in_buf, export_len);
            aec_uart_write_v2();
        }
#endif
    }
}

static void audio_anc_develop_task(void *p)
{
    int res;
    int msg[16];
    dev_log(">>>ANC DEV TASK<<<\n");
    while (1) {
        res = os_taskq_pend(NULL, msg, ARRAY_SIZE(msg));
        if (res == OS_TASKQ) {
            switch (msg[1]) {
            case ANC_DEV_MSG_OPEN:
                break;
            case ANC_DEV_MSG_RUN:
                if (hdl->state == ANC_DEV_STATE_OPEN) {
                    hdl->run_busy = 1;

                    if (hdl->data_sel == ANC_DEV_DATA_SEL_ADC) {
                        audio_anc_develop_mic_run();
                    } else if (hdl->data_sel == ANC_DEV_DATA_SEL_ANC) {
                        audio_anc_develop_anc_dma_run();
                    }
                    /* putchar('r'); */
                    hdl->run_busy = 0;
                }
                break;
            case ANC_DEV_MSG_CLOSE:
                audio_anc_develop_close();
                break;
            }
        } else {
            dev_log("res:%d,%d", res, msg[1]);
        }
    }
}

#if DAC_REF_SW_SRC_ENABLE

static int sw_src_init(struct sw_src_hdl *src, u8 nch, u32 insample, u32 outsample)
{
    if (insample != outsample) {
        src->sw_src_api = get_rs16_context();
        /* g_printf("src->sw_src_api:0x%x\n", src->sw_src_api); */
        ASSERT(src->sw_src_api);
        u32 sw_src_need_buf = src->sw_src_api->need_buf();
        /* g_printf("src->sw_src_buf:%d\n", sw_src_need_buf); */
        src->sw_src_buf = malloc(sw_src_need_buf);
        ASSERT(src->sw_src_buf);
        RS_PARA_STRUCT rs_para_obj;
        rs_para_obj.nch = nch;

        rs_para_obj.new_insample = insample;//48000;
        rs_para_obj.new_outsample = outsample;//16000;
        rs_para_obj.dataTypeobj.IndataBit = 0;
        rs_para_obj.dataTypeobj.OutdataBit = 0;
        rs_para_obj.dataTypeobj.IndataInc = (nch == 1) ? 1 : 2;
        rs_para_obj.dataTypeobj.OutdataInc = (nch == 1) ? 1 : 2;
        rs_para_obj.dataTypeobj.Qval = 15;

        /* printf("sw src,ch = %d, in = %d,out = %d\n", rs_para_obj.nch, rs_para_obj.new_insample, rs_para_obj.new_outsample); */
        src->sw_src_api->open(src->sw_src_buf, &rs_para_obj);
    }

    return 0;
}

static int sw_src_run(struct sw_src_hdl *src, s16 *indata, s16 *outdata, u16 len)
{
    int outlen = len;
    if (src->sw_src_api && src->sw_src_buf) {
        outlen = src->sw_src_api->run(src->sw_src_buf, indata, len >> 1, outdata);
        /* ASSERT(outlen <= (sizeof(outdata) >> 1));  */
        outlen = outlen << 1;
        /* printf("%d\n",outlen); */
    }
    return outlen;
}

static void sw_src_exit(struct sw_src_hdl *src)
{
    if (src->sw_src_buf) {
        free(src->sw_src_buf);
        src->sw_src_buf = NULL;
        src->sw_src_api = NULL;
    }
}
#endif

int audio_anc_develop_init(void)
{
    hdl = zalloc(sizeof(struct anc_develop_hdl));
    hdl->ff_multiple_gain = 1.0f;
    hdl->fb_multiple_gain = 1.0f;
    task_create(audio_anc_develop_task, NULL, "anc_dev");
    return 0;
}

int audio_anc_develop_open(enum ANC_DEV_DATA_SEL sel)
{
    int ret = 0;
    if (hdl->state != ANC_DEV_STATE_END) {
        dev_log("Err:anc_develop repeat open! %d\n", hdl->state);
        return -1;
    }
    dev_log("ANC_DEV_STATE:OPEN\n");
    hdl->state = ANC_DEV_STATE_OPEN;
    hdl->data_sel = sel;
    hdl->printf_en = 0;
    //算法初始化
    hdl->frame_cnt = 0;
    hdl->prompt_flag = 0;
    hdl->anc_flag = 0;
    hdl->anc_coeff_mode = 1;    //默认选择第一组

    clock_alloc("ANC_DEV", ANC_DEV_CLOCK_ALLOC);

    spin_lock_init(&hdl->lock);

#if ANC_UART_EXPORT_ENABLE
    hdl->dump_en = 1;
    aec_uart_open_v2(ANC_UART_EXPORT_CH, ANC_UART_EXPORT_PACKET_SIZE);
#endif
    switch (hdl->data_sel) {
    case ANC_DEV_DATA_SEL_ADC:
        ret = audio_anc_develop_mic_open();
        if (ret) {
            goto __exit;
        }
        audio_dac_read_reset();
        break;
    case ANC_DEV_DATA_SEL_ANC:
        ret = audio_anc_develop_anc_dma_open();
        if (ret) {
            goto __exit;
        }
        break;
    }
    return 0;

__exit:
    audio_anc_develop_close();

    return -1;
}

int audio_anc_develop_close(void)
{
    if (hdl->state == ANC_DEV_STATE_END) {
        dev_log("Err:anc_develop repeat close!\n");
        return -1;
    }
    dev_log("ANC_DEV_STATE:CLOSE\n");
    hdl->state = ANC_DEV_STATE_CLOSE;

    switch (hdl->data_sel) {
    case ANC_DEV_DATA_SEL_ADC:
        audio_anc_develop_mic_close();
        break;
    case ANC_DEV_DATA_SEL_ANC:
        audio_anc_develop_anc_dma_close();
        break;
    }

    clock_free("ANC_DEV");

#if ANC_UART_EXPORT_ENABLE
    aec_uart_close_v2();
#endif

    int wait_cnt = 100;
    while (hdl->run_busy && wait_cnt) {
        wait_cnt--;
        os_time_dly(1);
    }

    //算法结束
    hdl->frame_cnt = 0;
    hdl->prompt_flag = 0;
    hdl->anc_flag = 0;

    switch (hdl->data_sel) {
    case ANC_DEV_DATA_SEL_ADC:
        audio_anc_develop_mic_free();
        break;
    case ANC_DEV_DATA_SEL_ANC:
        audio_anc_develop_anc_dma_free();
        break;
    }

    dev_log("ANC_DEV_STATE:END\n");
    hdl->state = ANC_DEV_STATE_END;
    return 0;
}

//提示音播放结束回调接口
static int audio_anc_develop_tone_play_cb(void *priv, enum stream_event event)
{
    if ((event != STREAM_EVENT_START) && (event != STREAM_EVENT_STOP) && (event != STREAM_EVENT_INIT)) {
        /*提示音被打断，退出自适应*/
    }
    if (event != STREAM_EVENT_STOP) {
        return 0;
    }
    //算法结束
    //os_taskq_post_msg("anc_dev", 1, ANC_DEV_MSG_CLOSE);
    return 0;
}

//算法open demo : 打开ANC，等待打开完成后启动VIVO自适应算法
int audio_anc_develop_switch_on(void)
{
    int ret;

    //测试流程，切ANC并且等ANC切换完毕
    anc_mode_switch(ANC_ON, 0);
    int wait_flag = 20;
    while (anc_mode_switch_lock_get() && wait_flag) {
        os_time_dly(20);
        wait_flag--;
    };

    //算法启动
    ret = audio_anc_develop_open(ANC_DEV_DATA_SEL_ANC);
    if (ret) {
        return 1;
    }
    return 0;
}

int audio_anc_develop_switch_off(void)
{
    int ret;

    //算法关闭
    ret = audio_anc_develop_close();
    anc_mode_switch(ANC_OFF, 0);
    if (ret) {
        return 1;
    }
    return 0;
}

//提示音播放 + 打开算法 + ANC无效果(ANC模式fade增益调到0)
int audio_anc_develop_tone_play_open(void)
{
    int ret;
    ret = audio_anc_develop_switch_on();
    if (ret) {
        return ret;
    }
    //测试提示音流程，MUTE ANC输出
    ret = play_tone_file_callback(get_tone_files()->anc_on, NULL, audio_anc_develop_tone_play_cb);

    return 0;
}

int audio_anc_develop_gain_change(s8 num, s8 sign)
{
    float *multiple_gain = &hdl->ff_multiple_gain; //修改FFgain
    //float *multiple_gain = &hdl->fb_multiple_gain; //修改FBgain
    if (sign) {
        dev_log("in multiple_gain %d/100\n", (int)((*multiple_gain) * 100));
        *multiple_gain *= eq_db2mag(((float)(num * sign)) / 10.0f);
        dev_log("out multiple_gain %d/100\n", (int)((*multiple_gain) * 100));
    } else {
        *multiple_gain = 1.0f;
    }
    //更新效果
    audio_anc_mult_scene_update(audio_anc_mult_scene_get());
    return 0;
}

float audio_anc_develop_multiple_gain_get(u8 type)
{
#if ANC_DEV_ONLINE_SPP_DEBUG_EN
    switch (type) {
    case 0:
        return hdl->ff_multiple_gain;
    case 1:
        return hdl->fb_multiple_gain;
    }
#endif
    return 1.0f;
}

u8 audio_anc_develop_anc_mode_get(void)
{
    if (anc_mode_get() == ANC_OFF) {
        return 0;
    }
    return hdl->anc_coeff_mode;
}

//SPP RX
int audio_anc_develop_spp_rx_process(u8 *data, int len)
{
#if ANC_DEV_ONLINE_SPP_DEBUG_EN
    u16 cmd;
    if (!(data[0] == 0xFF && data[1] == 0X1B)) {
        return 0;
    }
    if (len < 4) {
        return 0;
    }
    u8 data_tmp[2];
    //大小端转换
    data_tmp[0] = data[3];
    data_tmp[1] = data[2];
    memcpy((u8 *)&cmd, data_tmp, 2);
    dev_log("cmd %x, data %x\n", cmd, data[4]);
    switch (cmd) {
    case 0x130:
        if (data[4]) {
            audio_anc_develop_anc_mode_set(data[4]);
            //切到ANC模式，确保有数据流
            if (anc_mode_get() != ANC_ON) {
                audio_anc_develop_switch_on();
            }
        } else {
            anc_mode_switch(ANC_OFF, 0);
            audio_anc_develop_close();
        }

        break;
    case 0x131:
        if (data[4]) {
            audio_anc_develop_open(ANC_DEV_DATA_SEL_ANC);
        } else {
            audio_anc_develop_close();
        }
        break;
    case 0x132:
    case 0x133:
#if ANC_UART_EXPORT_ENABLE
        if (data[4] == 0) {
            // hdl->dump_en = 0;
        }
        int wait_cnt = 100;
        while (hdl->run_busy && wait_cnt) {
            wait_cnt--;
            os_time_dly(1);
        }
        if (data[4]) {
            // aec_uart_open_v2(ANC_UART_EXPORT_CH, ANC_UART_EXPORT_PACKET_SIZE);
            // hdl->dump_en = 1;
        } else {
            // aec_uart_close_v2();
        }
#endif
        break;
    case 0x136:
        audio_anc_develop_gain_change((s8)data[4], 1);
        break;
    case 0x137:
        audio_anc_develop_gain_change((s8)data[4], -1);
        break;
    case 0x138: //reset
        audio_anc_develop_gain_change(0, 0);
        break;
    case 0x139:
        audio_anc_develop_anc_mode_set(data[4]);
        //切到ANC模式，确保有数据流
        if (anc_mode_get() != ANC_ON) {
            anc_mode_switch(ANC_ON, 0); //注意切换大概需要500ms左右时间
        }
        break;
    case 0x140: //测试命令
        hdl->printf_en = 1;
        break;
    default:
        break;
    }
    return 1;
#else
    return 0;
#endif
}

//SPP TX
int audio_anc_develop_spp_tx_process(u8 *data, int len)
{
#if ANC_DEV_ONLINE_SPP_DEBUG_EN
    bt_cmd_prepare(USER_CTRL_SPP_SEND_DATA, len, data);
    return len;
#else
    return 0;
#endif
}

void audio_anc_develop_coeff_set(enum ANC_IIR_TYPE type, anc_fr_t *fr, u32 order, float total_gain)
{
    if (!hdl) {
        return;
    }
    int alogm = audio_anc_gains_alogm_get(type);    //获取采样率
    audio_anc_t *param = audio_anc_param_get();
    switch (type) {
    case ANC_FF_TYPE:
        if (hdl->ff_coeff) {
            free(hdl->ff_coeff);
        }
        hdl->ff_coeff = zalloc(order * 40);
        audio_anc_biquad2ab_double(fr, hdl->ff_coeff, order, alogm);
        param->lff_coeff = hdl->ff_coeff;
        param->lff_yorder = order;
        param->gains.l_ffgain = total_gain;
        break;
    case ANC_FB_TYPE:
        if (hdl->fb_coeff) {
            free(hdl->fb_coeff);
        }
        hdl->fb_coeff = zalloc(order * 40);
        audio_anc_biquad2ab_double(fr, hdl->fb_coeff, order, alogm);
        param->lfb_coeff = hdl->fb_coeff;
        param->lfb_yorder = order;
        param->gains.l_fbgain = total_gain;
        break;
    case ANC_CMP_TYPE:
        if (hdl->cmp_coeff) {
            free(hdl->cmp_coeff);
        }
        hdl->cmp_coeff = zalloc(order * 40);
        audio_anc_biquad2ab_double(fr, hdl->cmp_coeff, order, alogm);
        param->lcmp_coeff = hdl->cmp_coeff;
        param->lcmp_yorder = order;
        param->gains.l_cmpgain = total_gain;
        break;
    default:
        return;
    }
    audio_anc_coeff_smooth_update();
}

void audio_anc_develop_coeff_set_demo(void)
{
    //set ff
    const u8 order = 10; //需要确保同类型滤波器个数一致
    anc_fr_t fr[order] = {
        //type, F/G/Q
        {ANC_IIR_BAND_PASS, 100, 10, 0.7},
        {ANC_IIR_BAND_PASS, 200, 10, 0.7},
        {ANC_IIR_BAND_PASS, 500, 10, 0.7},
        {ANC_IIR_BAND_PASS, 1000, 0, 0.7},
        {ANC_IIR_BAND_PASS, 2000, 0, 0.7},
        {ANC_IIR_BAND_PASS, 3000, 0, 0.7},
        {ANC_IIR_BAND_PASS, 4000, 0, 0.7},
        {ANC_IIR_BAND_PASS, 5000, 0, 0.7},
        {ANC_IIR_BAND_PASS, 6000, 0, 0.7},
        {ANC_IIR_BAND_PASS, 7000, 0, 0.7},
    };
    float total_gain = 1.0f; //线性值, 必须>=0
    audio_anc_develop_coeff_set(ANC_FF_TYPE, fr, order, total_gain);
}

void audio_anc_develop_iir_set(enum ANC_IIR_TYPE type, double *coeff, u32 order, float total_gain)
{
    if (!hdl) {
        return;
    }
    int alogm = audio_anc_gains_alogm_get(type);    //获取采样率
    audio_anc_t *param = audio_anc_param_get();
    switch (type) {
    case ANC_FF_TYPE:
        if (hdl->ff_coeff) {
            free(hdl->ff_coeff);
        }
        hdl->ff_coeff = zalloc(order * 40);
        memcpy(hdl->ff_coeff, coeff, order * 40);
        //audio_anc_biquad2ab_double(fr, hdl->ff_coeff, order, alogm);
        param->lff_coeff = hdl->ff_coeff;
        param->lff_yorder = order;
        param->gains.l_ffgain = total_gain;
        break;
    case ANC_FB_TYPE:
        if (hdl->fb_coeff) {
            free(hdl->fb_coeff);
        }
        hdl->fb_coeff = zalloc(order * 40);
        memcpy(hdl->fb_coeff, coeff, order * 40);
        //audio_anc_biquad2ab_double(fr, hdl->fb_coeff, order, alogm);
        param->lfb_coeff = hdl->fb_coeff;
        param->lfb_yorder = order;
        param->gains.l_fbgain = total_gain;
        break;
    case ANC_CMP_TYPE:
        if (hdl->cmp_coeff) {
            free(hdl->cmp_coeff);
        }
        hdl->cmp_coeff = zalloc(order * 40);
        memcpy(hdl->cmp_coeff, coeff, order * 40);
        //audio_anc_biquad2ab_double(fr, hdl->cmp_coeff, order, alogm);
        param->lcmp_coeff = hdl->cmp_coeff;
        param->lcmp_yorder = order;
        param->gains.l_cmpgain = total_gain;
        break;
    default:
        return;
    }
    audio_anc_coeff_smooth_update();
}

//低功耗注册
static u8 audio_anc_develop_idle_query(void)
{
    //ANC 第三方算法运行需要CPU参与，不允许进入低功耗
    if (hdl->state != ANC_DEV_STATE_END) {
        return 0;
    }
    return 1;
}

u8 audio_anc_develop_is_running(void)
{
    return !audio_anc_develop_idle_query();
}

REGISTER_LP_TARGET(ANC_dev_lp_target) = {
    .name       = "ANC_DEV",
    .is_idle    = audio_anc_develop_idle_query,
};

#endif






