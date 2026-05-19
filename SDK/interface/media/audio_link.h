#ifndef AUDIO_LINK_API_H
#define AUDIO_LINK_API_H

#include "generic/typedef.h"
#include "asm/alink.h"


//iis 模块相关
void *alink_init(void *hw_alink);  //iis 初始化
int alink_start(void *hw_alink);             //iis 开启
int alink_set_sr(void *hw_alink, u32 sr); 			//iis 设置采样率
int alink_get_sr(void *hw_alink); 				//iis 获取采样率
u32 alink_get_dma_len(void *hw_alink); 				//iis 获取DMA_LEN
void alink_set_rx_pns(void *hw_alink, u32 len);		//iis 设置接收PNS
void alink_set_tx_pns(void *hw_alink, u32 len);		//iis 设置发送PNS
int alink_uninit_base(void *hw_alink, void *hw_channel);
int alink_uninit(void *hw_alink, void *hw_channel);
void alink_uninit_force(void *hw_alink);
void alink_sr(void *hw_alink, u32 rate);

//iis 通道相关
void alink_set_irq_handler(void *hw_alink, void *hw_channel,  void *priv, void (*handle)(void *priv, void *addr, int len));
void *alink_channel_init_base(void *hw_alink, void *hw_channel, u8 ch_idx);
void *alink_channel_init(void *hw_alink, void *hw_channel);
void alink_channel_io_init(void *hw_alink, void *hw_channel, u8 ch_idx);
void alink_channel_close(void *hw_alink, void *hw_channel);
s32 *alink_get_addr(void *hw_channel);			//iis获取通道DMA地址
u32 alink_get_shn(void *hw_channel);			//iis获取通道SHN
u32 alink_get_shn_lock(void *hw_channel);
u32 alink_get_len(void *hw_channel);			//iis 获取长度LEN
void alink_set_shn(void *hw_channel, u32 len);	//iis设置通道SHN
void alink_set_shn_with_no_sync(void *hw_channel, u32 len);
u32 alink_get_swptr(void *hw_channel);			//iis获取通道swptr
void alink_set_swptr(void *hw_channel, u32 value);		//iis设置通道swptr
u32 alink_get_hwptr(void *hw_channel);			//iis获取通道hwptr
void alink_set_hwptr(void *hw_channel, u32 value);	//iis设置通道hwptr
void alink_set_ch_ie(void *hw_channel, u32 value);	//iis设置通道ie
void alink_clr_ch_pnd(void *hw_channel);			//iis清除通道pnd
int alink_get_ch_ie(void *hw_channel);
void alink_set_da2sync_ch(void *hw_alink);
void audio_alink_lock(u8 module_idx);
void audio_alink_unlock(u8 module_idx);
u32 alink_timestamp(u32 module_idx, u8 ch_idx);
void alink_sw_mute(void *hw_channel, u8 mute);

void alink0_info_dump();

#endif
