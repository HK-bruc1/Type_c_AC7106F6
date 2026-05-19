#ifndef BT_KEY_FUNC_H
#define BT_KEY_FUNC_H

void bt_key_music_pp();
void bt_key_music_prev();
void bt_key_music_next();
void bt_key_vol_up();
void bt_key_vol_down();
void bt_key_rcsp_vol_up(void);
void bt_key_rcsp_vol_down(void);
void bt_key_call_last_on(void);
void bt_key_call_hang_up(void);
void bt_key_call_siri(void);
void bt_key_hid_control(void);
void bt_key_call_three_way_answer1(void);
void bt_key_call_three_way_answer2(void);
void bt_key_call_switch(void);

void bt_send_a2dp_cmd(int msg);
void bt_send_jl_cis_cmd(int msg);
int bt_share_call_a2dp(u8 cmd);

void bt_volume_up(u8 dec);
void bt_volume_down(u8 dec);

#endif
