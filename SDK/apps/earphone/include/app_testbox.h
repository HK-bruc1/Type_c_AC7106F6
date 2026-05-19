#ifndef _APP_TESTBOX_H_
#define _APP_TESTBOX_H_

#include "typedef.h"

struct testbox_event {
    u8 event;
    u8 size;
    u8 *packet;
};

struct tws_pairtool_info_t {
    u8 dev_num;
    u8 ch;
    u8 common_addr[6];
    u8 tws_addr[6];
};

void testbox_set_bt_init_ok(u8 flag);
u8 testbox_get_status(void);
void testbox_clear_status(void);
u8 testbox_get_ex_enter_dut_flag(void);
u8 testbox_get_ex_enter_storage_mode_flag(void);
u8 testbox_get_connect_status(void);
void testbox_clear_connect_status(void);
u8 testbox_get_keep_tws_conn_flag(void);


#endif    //_APP_TESTBOX_H_
