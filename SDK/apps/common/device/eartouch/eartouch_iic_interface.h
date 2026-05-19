#ifndef __EARTOUCH_IIC_INTERFACE_H__
#define __EARTOUCH_IIC_INTERFACE_H__

u8 irSensor_iic_write(u8 w_chip_id, u8 register_address, u8 function_command);
u8 irSensor_iic_read(u8 r_chip_id, u8 register_address, u8 *buf, u8 data_len);
int irSensor_iic_init(eartouch_platform_data *data);

#endif
