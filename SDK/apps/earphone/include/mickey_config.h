#ifndef MICKEY_CONFIG_H
#define MICKEY_CONFIG_H

#include "typedef.h"
#include "key/mickey.h"
struct mickey_res_value {
    u8 mickey_pin;
    u16 extern_bias_res;
    u16 pp_res_value;
    u16 up_res_value;
    u16 down_res_value;
};
#endif
