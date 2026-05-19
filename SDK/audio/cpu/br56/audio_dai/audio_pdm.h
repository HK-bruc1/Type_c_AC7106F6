#ifndef _AUD_PDM_H_
#define _AUD_PDM_H_

#include "generic/typedef.h"

typedef enum {
    DIGITAL_MIC_DATA,
    ANALOG_MIC_DATA,
} PLNK_MIC_SEL;

/*通道0输入模式选择*/
typedef enum {
    DATA0_SCLK_RISING_EDGE,
    DATA0_SCLK_FALLING_EDGE,
    DATA1_SCLK_RISING_EDGE,
    DATA1_SCLK_FALLING_EDGE,
} PLNK_CH_MD;

struct plnk_ch_cfg {
    u8 en;
    PLNK_CH_MD mode;
    PLNK_MIC_SEL mic_type;
};

struct plnk_data_cfg {
    u8 en;
    u8 io;
};

typedef struct _PLNK_PARM {
    u8 sclk_io;						//时钟IO
    u32 sclk_fre;					//时钟频率推荐2M
    struct plnk_ch_cfg ch_cfg[4];		//通道内部配置
    struct plnk_data_cfg data_cfg[4];		//数据IO配置
    u8 ch_num;		/*使能多少个通道*/
    u32 sr;			/*采样率*/
    u32 dma_len;	/*一次中断byte数*/
    void *buf;
    void (*isr_cb)(void *priv, void *buf, u32 len);
    void *private_data;			//音频私有数据
} PLNK_PARM;

void *plnk_init(void *hw_plink);
void plnk_start(void *hw_plink);
void plnk_stop(void *hw_plink);
void plnk_uninit(void *hw_plink);


#endif/*_AUD_PDM_H_*/
