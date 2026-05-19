#ifndef _AUDIO_LINK_H_
#define _AUDIO_LINK_H_

#include "generic/typedef.h"

#define ALINK_MODULE_NUM_MAX    1//iis模块数量
#define ALINK_CH_NUM_MAX        4//iis通道数量

//只有一个ALINK模块，module*0消除module未引用的警告
#define ALINK_SEL(module, reg)             (((JL_ALNK_TypeDef    *)(((u8 *)JL_ALNK0) + module*(0)))->reg)

#define ALINK_CON_RESET(module)	do {ALINK_SEL(module, CON0) = 0; ALINK_SEL(module, CON1) = 0; ALINK_SEL(module, CON2) = 0; ALINK_SEL(module, CON3) = 0;} while(0)
#define ALINK_HWPTR_RESET(module)	do {ALINK_SEL(module, HWPTR0) = 0; ALINK_SEL(module, HWPTR1) = 0; ALINK_SEL(module, HWPTR2) = 0; ALINK_SEL(module, HWPTR3) = 0;} while(0)
#define ALINK_SWPTR_RESET(module)	do {ALINK_SEL(module, SWPTR0) = 0; ALINK_SEL(module, SWPTR1) = 0; ALINK_SEL(module, SWPTR2) = 0; ALINK_SEL(module, SWPTR3) = 0;} while(0)
#define ALINK_SHN_RESET(module)	do {ALINK_SEL(module, SHN0) = 0; ALINK_SEL(module, SHN1) = 0; ALINK_SEL(module, SHN2) = 0; ALINK_SEL(module, SHN3) = 0;} while(0)
#define ALINK_ADR_RESET(module)	do {ALINK_SEL(module, ADR0) = 0; ALINK_SEL(module, ADR1) = 0; ALINK_SEL(module, ADR2) = 0; ALINK_SEL(module, ADR3) = 0;} while(0)
#define ALINK_PNS_RESET(module)	do {ALINK_SEL(module, PNS) = 0;} while(0)
#define ALINK_BLOCK_RESET(module)	do {ALINK_SEL(module, BLOCK) = 0;} while(0)

//CON0
#define	ALINK_DA2BTSRC_SEL(module, x)	SFR(ALINK_SEL(module, CON0), 0, 2, x)
#define	ALINK_DMA_MODE_SEL(module, x)	SFR(ALINK_SEL(module, CON0), 2, 1, x)
#define ALINK_DSP_MODE(module, x)		SFR(ALINK_SEL(module, CON0), 5, 2, x)
#define ALINK_SOE(module, x) 			SFR(ALINK_SEL(module, CON0), 7, 1, x)		//unless for crossbar
#define ALINK_MOE(module, x) 			SFR(ALINK_SEL(module, CON0), 8, 1, x)		//unless for crossbar
#define ALINK_SCLKINV(module, x)   		SFR(ALINK_SEL(module, CON0),10, 1, x)
#define ALINK_EN(module, x)  		    SFR(ALINK_SEL(module, CON0),11, 1, x)
#define ALINK_EN_GET(module)            (ALINK_SEL(module, CON0) & BIT(11)) ? 1 : 0


//CON1
#define ALINK_BWLEN_MODE(module, x)			 SFR(ALINK_SEL(module, CON1),12, 2, x)
#define ALINK_CHx_DIR_MODE(module, ch, x)  	 SFR(ALINK_SEL(module, CON1), 2 + 3 * ch, 1, x)
#define ALINK_CHx_MODE_SEL(module, ch, x)  	 SFR(ALINK_SEL(module, CON1), 3 * ch, 2, x)
#define ALINK_CHx_CLOSE(module, ch)  	 	 SFR(ALINK_SEL(module, CON1), 3 * ch, 2, 0)

//CON2
#define ALINK_CLR_ALL_PND(module)			(ALINK_SEL(module, CON2) |= BIT(0) | BIT(1) | BIT(2) | BIT(3))
#define ALINK_CLR_CHx_PND(module, ch)		(ALINK_SEL(module, CON2) |= BIT(ch))
#define ALINK_CHx_IE(module, ch, x)			SFR(ALINK_SEL(module, CON2), ch + 12, 1, x)
#define ALINK_CHx_IE_GET(module, ch)			(ALINK_SEL(module, CON2) & BIT(ch + 12))?1:0

//CON3
#define ALINK_MSRC(module, x)			SFR(ALINK_SEL(module, CON3), 0, 2, x)
#define ALINK_SDIV(module, x)			SFR(ALINK_SEL(module, CON3), 2, 5, x)	//0~31 for 1~32
#define ALINK_SCLK_SEL(module, x)		SFR(ALINK_SEL(module, CON3), 7, 1, x)
#define ALINK_LRCK_PRD_SET(module, x)	SFR(ALINK_SEL(module, CON3), 8, 9, x)	//LRCK=SCLK/(x+1)	x = bit_wide * slot
#define ALINK_LRCK_DUTY_SET(module, x)	SFR(ALINK_SEL(module, CON3),17, 6, x)
#define ALINK_SLOT_SET(module, x)		SFR(ALINK_SEL(module, CON3),23, 3, x)
#define ALINK_SW_MUTE_EN(module, ch, x)    SFR(ALINK_SEL(module, CON3), ch + 26, 1, x)

//CON4
#define ALINK_SW_MUTE_WAIT_ACK(module, ch)  while(!(ALINK_SEL(module, CON4) & BIT(ch + 0)))
#define ALINK_SW_MUTE_STATUS(module, ch)    (ALINK_SEL(module, CON4) & BIT(ch + 0))?1:0



#define ALINK_OPNS_SET(module, x)		SFR(ALINK_SEL(module, PNS), 16, 16, x)
#define ALINK_IPNS_SET(module, x)		SFR(ALINK_SEL(module, PNS), 0, 16, x)

#define ALINK_LEN_SET(module, x)		(ALINK_SEL(module, LEN) = x)
#define ALINK_FIFO_LEN(module)          (ALINK_SEL(module, LEN))

#define ALINK_BLOCK_SET(module, x)   (ALINK_SEL(module, BLOCK) = x)


#define ALINK_CLK_OUPUT_DISABLE 	0xFF

typedef enum {
    ALINK0			= 0u,	//BR50只有1个ALNK模块
} ALINK_PORT;

//ch_num
typedef enum {
    ALINK_CH0		= 0u,
    ALINK_CH1,
    ALINK_CH2,
    ALINK_CH3,
} ALINK_CH;

//ch_dir
typedef enum {
    ALINK_DIR_TX	= 0u,
    ALINK_DIR_RX		,
} ALINK_DIR;

typedef enum {
    ALINK_BW_16BIT = 16,
    ALINK_BW_20BIT = 20,	//only for DSP mode
    ALINK_BW_24BIT = 24,
    ALINK_BW_32BIT	= 32,
} ALINK_DATA_BW;



typedef enum {
    ALINK_SLOT_NUM1 = 1,
    ALINK_SLOT_NUM2 	,	//basic mode must be
    ALINK_SLOT_NUM3 	,
    ALINK_SLOT_NUM4		,
    ALINK_SLOT_NUM5		,
    ALINK_SLOT_NUM6		,
    ALINK_SLOT_NUM7		,
    ALINK_SLOT_NUM8		,
} ALINK_SLOT_NUM;

//mode
typedef enum {
    ALINK_MD_BASIC	= 0u,
    ALINK_MD_DSP_SHORT	,
    ALINK_MD_DSP_LONG	,
} ALINK_MODE;

//ch_mode
typedef enum {
    ALINK_CH_MD_NONE	= 0u,
    ALINK_CH_MD_BASIC_IIS = 1,
    ALINK_CH_MD_BASIC_LALIGN = 2,
    ALINK_CH_MD_BASIC_RALIGN = 3,
    ALINK_CH_MD_DSP0 = 1,
    ALINK_CH_MD_DSP1 = 2,
} ALINK_CH_MODE;

typedef enum {
    ALINK_ROLE_MASTER, //主机
    ALINK_ROLE_SLAVE,  //从机
} ALINK_ROLE;

typedef enum {
    ALINK_CLK_FALL_UPDATE_RAISE_SAMPLE, //下降沿更新数据, 上升沿采样数据
    ALINK_CLK_RAISE_UPDATE_FALL_SAMPLE, //上降沿更新数据, 下升沿采样数据
} ALINK_CLK_MODE;


typedef enum {
    ALINK_SR_192000 = 192000,
    ALINK_SR_176400 = 176400,
    ALINK_SR_128000 = 128000,
    ALINK_SR_96000 = 96000,
    ALINK_SR_88200 = 88200,
    ALINK_SR_64000 = 64000,
    ALINK_SR_48000 = 48000,
    ALINK_SR_44100 = 44100,
    ALINK_SR_32000 = 32000,
    ALINK_SR_24000 = 24000,
    ALINK_SR_22050 = 22050,
    ALINK_SR_16000 = 16000,
    ALINK_SR_12000 = 12000,
    ALINK_SR_11025 = 11025,
    ALINK_SR_8000  = 8000,
} ALINK_SR;

typedef enum {
    ALINK_BUF_DUAL, 	//乒乓BUF
    ALINK_BUF_CIRCLE,	//循环BUF
} ALINK_BUF_MODE;

struct alnk_hw_ch {
    //for user
    u8 data_io;					//data IO配置
    u8 ch_ie;					//中断使能
    ALINK_DIR dir; 				//通道传输数据方向: Tx, Rx
    ALINK_CH_MODE ch_mode;		//iis left right DSP0 DSP1
    void (*isr_cb)(void *priv, void *addr, int len);	//中断回调
    void *private_data;			//音频私有数据

    //for driver
    ALINK_PORT	module;
    ALINK_CH	ch_idx;
    u8 ch_en;					//使用标记
    void *buf;					//dma buf地址
    u8 bit_width;
    u8 sw_mute_status;
};

//===================================//
//多个通道使用需要注意:
//1.数据位宽需要保持一致
//2.buf长度相同
//===================================//
typedef struct _ALINK_PARM {
    ALINK_PORT	module;
    u8 mclk_io; 				//mclk IO输出配置: ALINK_CLK_OUPUT_DISABLE不输出该时钟
    u8 sclk_io;					//sclk IO输出配置: ALINK_CLK_OUPUT_DISABLE不输出该时钟
    u8 lrclk_io;				//lrclk IO输出配置: ALINK_CLK_OUPUT_DISABLE不输出该时钟
    struct alnk_hw_ch ch_cfg[4];		//通道内部配置
    ALINK_MODE mode; 					//IIS, left, right, dsp
    ALINK_ROLE role; 			//主机/从机
    ALINK_CLK_MODE clk_mode; 			//更新和采样边沿
    ALINK_DATA_BW  bitwide;   //数据位宽16/32bit
    ALINK_SLOT_NUM  slot_num;   //slot num
    u32 dma_len; 						//buf长度: byte
    ALINK_SR sample_rate;					//采样
    ALINK_BUF_MODE 	buf_mode;  	//乒乓buf or 循环buf率
    u32 init_cnt; 						//buf长度: byte
    u16 rx_pns;
    u8 da2sync_ch;             //与蓝牙同步关联的目标通道
    u8 clk_close;         //0：模块关闭后，保持时钟继续输出。 1：模块关闭后，关闭时钟输出
    u8 chx_sw[4];     //记录哪些通道需要作模式切换
    u32 timestamp[4];
    u32 timestamp_gap[4];
    u32 timestamp_cnt[4];
} ALINK_PARM;


#define IIS_BASE            1
#define IIS_LEFT            2
#define IIS_RIGHT           3
#define DSP0_SHORT          4
#define DSP1_SHORT          5
#define DSP0_LONG           6
#define DSP1_LONG           7
#define ALINK_WORK_MODE IIS_BASE


#define IIS_CH_NUM  2  //测试通道数 1：mono  2:stereo 4:quad
#define ALINK_SR_NUM 15
#define IIS_SW_MUTE_EN 0 //br56 alink 支持hw_mute/hw_unmute
extern u32 alink_sr_tab[ALINK_SR_NUM];

#define IIS_USE_DOUBLE_BUF_MODE_EN 0
#define AUDIO_IIS_AT_RAM	__attribute__((section(".audio_iis_ram_code.text.cache.L1")))
#endif/*_AUDIO_LINK_H_*/
