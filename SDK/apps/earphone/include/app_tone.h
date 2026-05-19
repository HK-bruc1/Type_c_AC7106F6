#ifndef APP_TONE_H
#define APP_TONE_H

#include "tone_player.h"
#include "ring_player.h"
#include "tws_tone_player.h"

enum tone_language {
    TONE_ENGLISH,
    TONE_CHINESE,
};

struct tone_files {
    const char *num[10];
    const char *power_on;
    const char *power_off;
    const char *bt_mode;
    const char *bt_connect;
    const char *bt_disconnect;
    const char *phone_in;
    const char *phone_out;
    const char *phone_active;
    const char *tws_connect;
    const char *tws_disconnect;
    const char *charge_start;
    const char *charge_full;
    const char *low_power;
    const char *max_vol;
    const char *low_latency_in;
    const char *low_latency_out;
    const char *normal;
    const char *anc_on;
    const char *anc_trans;
    const char *anc_off;
    const char *key_tone;
    const char *anc_adaptive;
    const char *anc_adaptive_coeff;
    const char *anc_normal_coeff;
    const char *spkchat_on;
    const char *spkchat_off;
    const char *winddet_on;
    const char *winddet_off;
    const char *wclick_on;
    const char *wclick_off;
    const char *linein_mode;
    const char *pc_mode;
    const char *music_mode;
    const char *device_sd;
    const char *device_udisk;
    const char *fit_det_on;
    const char *share_search_pairing;
    const char *share_wait_pairing;
    const char *share_conn_master;
    const char *share_conn_slave;
    const char *share_disconnect;
    const char *edr_mode;
    const char *cis_mode;
    const char *cis_connect;
    const char *cis_disconnect;
};


void tone_language_set(enum tone_language lang);

enum tone_language tone_language_get();

const struct tone_files *get_tone_files();


#endif


