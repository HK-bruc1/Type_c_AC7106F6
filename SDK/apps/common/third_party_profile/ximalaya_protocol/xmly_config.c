#include "system/includes.h"
#include "sdk_config.h"
#include "earphone.h"
#include "app_main.h"

#if (THIRD_PARTY_PROTOCOLS_SEL & XIMALAYA_EN)

u8 xmly_pid[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
char xmly_pid_str[] = "0000000000000000";
u8 xmly_secret_key[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
char xmly_sn[20] = "XMSK1111112450888888";
char xmly_device_type[] = "3";
char xmly_fireware_version[] = "1.0.0";
char xmly_software_version[] = "1.0.0";
char xmly_device_model[] = "AiPods";
char xmly_company[] = "jl";

u8 xmly_adv_pid[4] = {0xce, 0xbe, 0xce, 0xa7};
u8 xmly_adv_vid[4] = {0xbf, 0xd2, 0xb9, 0x80};
u8 xmly_adv_cid[2] = {0x06, 0x26};
u8 xmly_adv_sv[2]  = {0x00, 0x01};
u8 xmly_adv_xpv = 0x02;

#endif

