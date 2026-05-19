#ifndef DEVICE_MICKEY_H
#define DEVICE_MICKEY_H

#include "typedef.h"
#include "gpadc.h"

#ifndef CONFIG_MICKEY_MAX_NUM
#define CONFIG_MICKEY_MAX_NUM  20
#endif

struct mickey_res {
    u8 key_value;
    u16 res_value;
};

struct mickey_platform_data {
    u8 enable;
    u16 pull_up_value;
    int micbias_ad_channel;
    int dacln_ad_channel;
    struct mickey_res pp_key;
    struct mickey_res up_key;
    struct mickey_res down_key;
};
//MICKEY API:
int mickey_init(void);
u8 mic_get_key_value(void);

const struct mickey_platform_data *get_mickey_platform_data();

#endif
