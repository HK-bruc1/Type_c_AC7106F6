#ifndef _LINEIN_DEV_H_
#define _LINEIN_DEV_H_

#include "typedef.h"
#include "device/device.h"

struct linein_dev_data {
    u8 enable;
    u8 port;
    u8 up : 1;
    u8 down : 1;
    u32 ad_channel;
    u16 ad_vol;
};

/**
 * @brief 获取linein是否在线
 * @param 1:在线 0：不在线
 */
u8 linein_is_online(void);

extern const struct device_operations linein_dev_ops;

#endif
