#ifndef BLE_IOT_UTILS_H
#define BLE_IOT_UTILS_H

#include "asm/cpu.h"

#define IOT_READ_BIG_U16(a)   ((*((u8*)(a)) <<8)  + *((u8*)(a)+1))
#define IOT_READ_BIG_U32(a)   ((*((u8*)(a)) <<24) + (*((u8*)(a)+1)<<16) + (*((u8*)(a)+2)<<8) + *((u8*)(a)+3))
#define IOT_READ_LIT_U16(a)   (*((u8*)(a))  + (*((u8*)(a)+1)<<8))
#define IOT_READ_LIT_U32(a)   (*((u8*)(a))  + (*((u8*)(a)+1)<<8) + (*((u8*)(a)+2)<<16) + (*((u8*)(a)+3)<<24))
#define IOT_WRITE_BIG_U16(a,src)   {*((u8*)(a)+0) = (u8)(src>>8); *((u8*)(a)+1) = (u8)(src&0xff); }
#define IOT_WRITE_BIG_U32(a,src)   {*((u8*)(a)+0) = (u8)((src)>>24);  *((u8*)(a)+1) = (u8)(((src)>>16)&0xff);*((u8*)(a)+2) = (u8)(((src)>>8)&0xff);*((u8*)(a)+3) = (u8)((src)&0xff);}
#define IOT_WRITE_LIT_U16(a,src)   {*((u8*)(a)+1) = (u8)(src>>8); *((u8*)(a)+0) = (u8)(src&0xff); }
#define IOT_WRITE_LIT_U32(a,src)   {*((u8*)(a)+3) = (u8)((src)>>24);  *((u8*)(a)+2) = (u8)(((src)>>16)&0xff);*((u8*)(a)+1) = (u8)(((src)>>8)&0xff);*((u8*)(a)+0) = (u8)((src)&0xff);}

/**
 * @brief 获取vm数据
 *
 * @param buf
 * @param buf_len
 * @result 0
 */
u8 iot_read_data_from_vm(u8 syscfg_id, u8 *buf, u8 buf_len);

#endif  // BLE_IOT_UTILS_H
