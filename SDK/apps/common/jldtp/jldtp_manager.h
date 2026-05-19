#ifndef JLDTP_MANAGER_H
#define JLDTP_MANAGER_H


#include "jldtp/jldtp_multi_channel.h"
#include "jldtp/jldtp_channel_ids.h"

enum {
    JLDTP_WITH_CHIP0 = 0,
    JLDTP_WITH_CHIP1 = 1,
    JLDTP_WITH_PC = 2,
};


int jldtp_manager_init(u8 handle_id);

void jldtp_manager_deinit(void);

jldtp_mc_handle_t jldtp_manager_get_mc_handle(u8 handle_id);





#endif

