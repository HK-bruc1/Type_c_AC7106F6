#ifndef __MASS_DATR_H__
#define __MASS_DATR_H__

#include "typedef.h"

#define MASS_DATA_TYPE_ORIGINAL         0 // 原始数据
#define MASS_DATA_TYPE_ALIYUN           1 // 阿里云
#define MASS_DATA_TYPE_RCSP_DATA        2 // RCSP
#define MASS_DATA_TYPE_AI_TEXT          3 // AI
#define MASS_DATA_TYPE_TTS              4 // TTS
#define MASS_DATA_TYPE_TONE             8 // TTS
#define MASS_DATA_TYPE_MAX              9 // TONE

int mass_data_asyn_send(u8 type, u8 *data, u16 len, void *evt_cb_priv, void (*evt_cb)(void *priv, int event), u16 ble_con_handle, u8 *spp_remote_addr);
int tone_mass_data_send(u8 *data, u16 len, u16 ble_con_handle, u8 *spp_remote_addr);

#endif//__MASS_DATR_H__
