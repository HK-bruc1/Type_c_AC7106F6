#ifndef _BLE_FMY_FMNA_H
#define _BLE_FMY_FMNA_H
int fmy_enable(bool en);

void fmy_open_close_pairing_mode(bool en);

bool fmy_get_pair_state(void);

void fmy_factory_reset(void);

u16 ble_fmy_get_con_handle(void);

void fmna_factory_reset(void *priv);

#endif

