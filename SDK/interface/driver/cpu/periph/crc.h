#ifndef  __CRC_H__
#define  __CRC_H__

u16 CRC16(const void *ptr, u32 len);
u16 CRC16_with_initval(const void *ptr, u32 len, u16 i_val);
void CrcDecode(void *buf, u16 len);


#endif  /*CRC_H*/
