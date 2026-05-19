#ifndef CPU_AUDIO_ADC_H
#define CPU_AUDIO_ADC_H

#define LADC_STATE_INIT			1
#define LADC_STATE_OPEN      	2
#define LADC_STATE_START     	3
#define LADC_STATE_STOP      	4

#define FPGA_BOARD          	0

#define LADC_MIC                0
#define LADC_LINEIN             1

/************************************************
  				Audio ADC Features
Notes:以下为芯片规格定义，不可修改，仅供引用
************************************************/
/*省电容麦*/
#define SUPPORT_MIC_CAPLESS     	1
/* ADC 最大通道数 */
#define AUDIO_ADC_MAX_NUM           (2)
#define AUDIO_ADC_MIC_MAX_NUM       (6)
#define AUDIO_ADC_LINEIN_MAX_NUM    (2)

/* 通道选择 */
#define AUDIO_ADC_MIC(x)					BIT(x)
#define AUDIO_ADC_MIC_0					    BIT(0)
#define AUDIO_ADC_MIC_1					    BIT(1)
#define AUDIO_ADC_MIC_2					    BIT(2)
#define AUDIO_ADC_DMIC_0					BIT(3)
#define AUDIO_ADC_DMIC_1					BIT(4)
#define AUDIO_ADC_DMIC_2					BIT(5)
#define AUDIO_ADC_DMIC_3					BIT(6)
#define AUDIO_ADC_LINE(x) 					BIT(x)
#define AUDIO_ADC_LINE0 					BIT(0)
#define AUDIO_ADC_LINE1  					BIT(1)
#define AUDIO_ADC_LINE2 					BIT(2)
#define PLNK_MIC		            		BIT(6)
#define ALNK_MIC				            BIT(7)

#define AUDIO_ADC_LINE3 					BIT(8)
#define AUDIO_ADC_MIC_3					    BIT(9)
/*******************************应用层**********************************/
/* 通道选择 */
#define AUDIO_ADC_MIC_CH		            AUDIO_ADC_MIC_0

/********************************************************************************
                MICx  输入IO配置(要注意IO与mic bias 供电IO配置互斥)
 ********************************************************************************/
// TCFG_AUDIO_MIC0_AIN_SEL
#define AUDIO_MIC0_CH0                BIT(0)    // PA1
#define AUDIO_MIC0_CH1                BIT(1)    // 不支持
#define AUDIO_MIC0_CH2                BIT(2)    // 不支持
// 当mic0模式选择差分模式时，输入N端引脚固定为  PA2

// TCFG_AUDIO_MIC1_AIN_SEL
#define AUDIO_MIC1_CH0                BIT(0)    // PB8
#define AUDIO_MIC1_CH1                BIT(1)    // 不支持
#define AUDIO_MIC1_CH2                BIT(2)    // 不支持
// 当mic1模式选择差分模式时，输入N端引脚固定为  PB7

// TCFG_AUDIO_MIC2_AIN_SEL
#define AUDIO_MIC2_CH0                BIT(0)    // 不支持
#define AUDIO_MIC2_CH1                BIT(1)    // 不支持
// 当mic2模式选择差分模式时，输入N端引脚固定为  PC11 // 不支持

// TCFG_AUDIO_MIC3_AIN_SEL
#define AUDIO_MIC3_CH0                BIT(0)    // 不支持
#define AUDIO_MIC3_CH1                BIT(1)    // 不支持
// 当mic3模式选择差分模式时，输入N端引脚固定为  PA4 // 不支持

// TCFG_AUDIO_MIC4_AIN_SEL
#define AUDIO_MIC4_CH0                BIT(0)    // 不支持
#define AUDIO_MIC4_CH1                BIT(1)    // 不支持

#define AUD_MIC_GN_0dB      (0)
#define AUD_MIC_GN_3dB      (1)
#define AUD_MIC_GN_6dB      (2)
/********************************************************************************
                MICx  mic bias 供电IO配置(要注意IO与micin IO配置互斥)
 ********************************************************************************/
// TCFG_AUDIO_MICx_BIAS_SEL
#define AUDIO_MIC_BIAS_NULL           (0)       // no bias
#define AUDIO_MIC_BIAS_CH0            BIT(0)    // PA2
#define AUDIO_MIC_BIAS_CH1            BIT(1)    // PB11
#define AUDIO_MIC_BIAS_CH2            BIT(2)    // unused
#define AUDIO_MIC_BIAS_CH3            BIT(3)    // unused
#define AUDIO_MIC_BIAS_CH4            BIT(4)    // unused
#define AUDIO_MIC_LDO_PWR             BIT(5)    // PA2/PB11
#define AUDIO_MIC_BIAS_LP             BIT(6)    // unused

/********************************************************************************
                         LINEINx  输入IO配置
 ********************************************************************************/
// TCFG_AUDIO_LINEIN0_AIN_SEL
#define AUDIO_LINEIN0_CH0                BIT(0)    // PA1
#define AUDIO_LINEIN0_CH1                BIT(1)    // 不支持
#define AUDIO_LINEIN0_CH2                BIT(2)    // 不支持

// TCFG_AUDIO_LINEIN1_AIN_SEL
#define AUDIO_LINEIN1_CH0                BIT(0)    // PB8
#define AUDIO_LINEIN1_CH1                BIT(1)    // 不支持
#define AUDIO_LINEIN1_CH2                BIT(2)    // 不支持

// TCFG_AUDIO_LINEIN2_AIN_SEL
#define AUDIO_LINEIN2_CH0                BIT(0)    // 不支持
#define AUDIO_LINEIN2_CH1                BIT(1)    // 不支持

// TCFG_AUDIO_LINEIN3_AIN_SEL
#define AUDIO_LINEIN3_CH0                BIT(0)    // 不支持
#define AUDIO_LINEIN3_CH1                BIT(1)    // 不支持

#define AUD_LINEIN_GN_0dB      (0)
#define AUD_LINEIN_GN_2dB      (1)
#define AUD_LINEIN_GN_6dB      (2)

/*Audio ADC输入源选择*/
#define AUDIO_ADC_SEL_AMIC0			0
#define AUDIO_ADC_SEL_AMIC1			1
#define AUDIO_ADC_SEL_DMIC0			2
#define AUDIO_ADC_SEL_DMIC1			3

#define AUDIO_ADC_DVOL_LIMIT        ((1 << 8) - 1)

struct mic_capless_trim_result {
    u8 bias_rsel0;      // MIC_BIASA_RSEL
    u8 bias_rsel1;      // MIC_BIASB_RSEL
};

struct mic_capless_trim_param {
    u8 triming_flag;
    u8 mic_trim_ch;
    u8 mic_online_detect;
    u8 mic_online_threshold;
    u16 trigger_threshold;
    u16 open_delay_ms; //adc上电等待稳定延时
    u16 trim_delay_ms; //偏置调整等待稳定延时
};

struct audio_adc_output_hdl {
    struct list_head entry;
    void *priv;
    void (*handler)(void *, s16 *, int);
};

struct audio_adc_private_param {
    u8 performance_mode;
    u8 mic_ldo_vsel;		//MIC0_LDO[000:1.5v 001:1.8v 010:2.1v 011:2.4v 100:2.7v 101:3.0v]
    u8 mic_ldo_isel; 		//MIC0通道电流档位选择
    u8 adca_reserved0;
    u8 adcb_reserved0;
    u8 lowpower_lvl;
    u8 dvol_441k;
    u8 dvol_48k;
    u8 capless_mic_power_mode;      //省电容MIC供电电压，0：1.4V，1：1.5V
};

struct mic_open_param {
    u8 mic_ain_sel;       // 0/1/2
    u8 mic_bias_sel;      // A(PA0)/B(PA1)/C(PC10)/D(PA5)
    u8 mic_bias_rsel;     // 单端隔直电容mic bias rsel
    u8 mic_mode : 4;      // MIC工作模式
    u8 mic_dcc : 4;       // DCC level
    u8 mic_dcc_en;
    u8 micldo_pwr_sel;   		// micldo供电选择, 0: 1.8v供电	1: IOVDD供电
};

struct linein_open_param {
    u8 linein_ain_sel;       // 0/1/2
    u8 linein_mode : 4;      // LINEIN 工作模式
    u8 linein_dcc : 4;       // DCC level
};

struct audio_adc_hdl {
    struct list_head head;
    struct audio_adc_private_param *private;
    spinlock_t lock;
    struct mic_capless_trim_result capless_trim;
    struct mic_capless_trim_param  capless_param;
    u8 adc_sel[AUDIO_ADC_MAX_NUM];
    u8 adc_dcc[AUDIO_ADC_MAX_NUM];
    u8 adc_dcc_en[AUDIO_ADC_MAX_NUM];
    struct mic_open_param mic_param[AUDIO_ADC_MIC_MAX_NUM];
    struct linein_open_param linein_param[AUDIO_ADC_MAX_NUM];
    OS_MUTEX mutex;
    u32 timestamp;
    s16 *hw_buf;                    //ADC硬件buffer的地址，buffer由外部传入，digital_close时会释放，外部只需将buffer置NULL即可
    u8 mic_ldo_state;               //是否手动调用接口打开MIC_LDO
    u8 state;                       //用于获取当前ADC状态以及防重入
    u8 digital_mic_open;            //普通ADC数字打开通道(不包含ANC)
    u8 anc_mic_open;                //ANC打开通道
    u8 adc_mic_open;                //ADC所有打开的通道(包含ANC)
    u8 channel_num;                 //用于统计当前ADC数字通道打开的数量，用于dev_mem低功耗的开关选择
    u8 max_adc_num;                 //ADC使能的最大通道数，若使能ADC复用，则max_adc_num为该cpu支持的最大通道数
    u8 buf_fixed;                   //是否固定adc硬件使用的buffer地址(@BR28)
    u8 bit_width;
    u8 lpadc_en;                    //LPADC使能(@BR50)
    u8 plnk_en;
    u8 analog_common_inited;
    u8 micbias_en_port[AUDIO_ADC_MAX_NUM]; //记录各个MIC使用的供电来源
    u8 audio_anc_mic_toggle;
};

struct adc_mic_ch {
    struct audio_adc_hdl *adc;
    void (*handler)(struct adc_mic_ch *, s16 *, u16);
    u32 sample_rate;
    s16 *bufs;
    u16 buf_size;
    u16 ch;         //当前应用打开的adc通道
    u8 gain[AUDIO_ADC_MIC_MAX_NUM];
    u8 buf_num;
};

#define adc_linein_ch adc_mic_ch //linein与mic使用同一个句柄

//MIC相关管理结构
typedef struct {
    u8 en;							//ANC MIC使能
    u8 type;						//ANC MIC类型
    u8 gain;						//MIC增益, 来自anc_gains.bin ANC配置
    u8 mult_flag;					//通话复用标志
    u8 pre_gain;					//MIC前级增益，来自stream.bin ADC节点配置
    struct mic_open_param mic_p;	//MIC模拟配置, 来自stream.bin ADC节点配置
} audio_adc_mic_mana_t;


#endif/*AUDIO_ADC_H*/

