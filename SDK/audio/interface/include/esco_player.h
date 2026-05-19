#ifndef ESCO_PLAYER_H
#define ESCO_PLAYER_H


enum {
    ESCO_PLAYER_EXT_TYPE_NONE,
    ESCO_PLAYER_EXT_TYPE_AI
};

int esco_player_open(u8 *bt_addr);

int esco_player_open_extended(u8 *bt_addr, int ext_type, void *ext_param);

void esco_player_close();

bool esco_player_runing();

int esco_player_get_btaddr(u8 *btaddr);

int esco_player_is_playing(u8 *btaddr);

void esco_player_set_ai_tx_node_func(int (*func)(u8 *, u32));

#endif
