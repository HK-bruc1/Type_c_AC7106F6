#ifndef __PUB_MUTUAL_SET_CMD__
#define __PUB_MUTUAL_SET_CMD__

#include "typedef.h"

enum {
    PUB_MTUAL_SET_CMD_TONE_FUNCTION = 0x06,
};

void public_settings_interaction_command(void *priv, u8 OpCode, u8 OpCode_SN, u8 *data, u16 len, u16 ble_con_handle, u8 *spp_remote_addr);
void public_settings_interaction_command_notify(u16 function, u8 result, u8 op, u16 ble_con_handle, u8 *spp_remote_addr);
#endif//__PUB_MUTUAL_SET_CMD__
