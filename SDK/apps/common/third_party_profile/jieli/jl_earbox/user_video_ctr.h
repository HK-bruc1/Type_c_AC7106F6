#ifndef __USER_VIDEO_CTR_H__
#define __USER_VIDEO_CTR_H__

#define CFG_USER_TIKTOK_X       13
#define CFG_USER_TIKTOK_Y       14

void usr_set_remote_type(u8 flag);
u8 usr_get_remote_type(void);
void key_vol_ctrl(u8 add_dec);
void key_take_photo();
void key_power_ctrl(void);
void key_memu_ctrl(void);
void bt_connect_timer_loop(void *priv);
void bt_connect_reset_xy(void);
void bt_connect_reset_coordinate(void);
u8 *usr_get_keypage_report_map(u8 flag, u16 *len);
void sbox_ctrl_tiktok(u8 cmd);
void sbox_ctrl_siri(u8 cmd);

#endif
