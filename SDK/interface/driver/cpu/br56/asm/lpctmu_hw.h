#ifndef _LPCTMU_HW_H_
#define _LPCTMU_HW_H_

#include "typedef.h"


#define LPCTMU_CHANNEL_SIZE         5

#define LPCTMU_ANA_CFG_ADAPTIVE     1


struct lpctmu_dma_kfifo {
    u16 *buffer;
    u32 buf_size;
    u32 buf_in;
    u32 buf_out;
};

enum {
    LPCTMU_VH_065V,
    LPCTMU_VH_070V,
    LPCTMU_VH_075V,
    LPCTMU_VH_080V,
};

enum {
    LPCTMU_VL_020V,
    LPCTMU_VL_025V,
    LPCTMU_VL_030V,
    LPCTMU_VL_035V,
};

enum {
    LPCTMU_ISEL_036UA,
    LPCTMU_ISEL_072UA,
    LPCTMU_ISEL_108UA,
    LPCTMU_ISEL_144UA,
    LPCTMU_ISEL_180UA,
    LPCTMU_ISEL_216UA,
    LPCTMU_ISEL_252UA,
    LPCTMU_ISEL_288UA
};

enum {
    LPCTMU_CH0_PB0,
    LPCTMU_CH1_PB1,
    LPCTMU_CH2_PB2,
    LPCTMU_CH3_PB3,
    LPCTMU_CH4_PB4,
};

enum lpctmu_pnd_type {
    LPCTMU_HW_RES_PND   = 0b000001,
    LPCTMU_SOFT_RES_PND = 0b000010,
    LPCTMU_DMA_RES_PND  = 0b000100,
    LPCTMU_DMA_HALF_PND = 0b001000,
    LPCTMU_DMA_FULL_PND = 0b010000,
    LPCTMU_KEY_MSG_PND  = 0b100000,
};

enum bt_arb_wl2ext_act {
    RF_PLL_EN = 1,
    RF_PLL_RN,
    RF_RX_LDO,
    RF_RX_EN,
    RF_TX_LDO,
    RF_TX_EN,
    EF_RX_TX_EN_XOR,
};

enum lpctmu_ext_stop_sel {
    BT_SIG_ACT0,
    BT_SIG_ACT1,
    BT_SIG_ACT0_ACT1_XOR,
    BT_SIG_ACT0_ACT1_AND,
};

enum lpctmu_wakeup_cfg {
    LPCTMU_WAKEUP_DISABLE,
    LPCTMU_WAKEUP_EN_WITHOUT_CHARGE_ONLINE,
    LPCTMU_WAKEUP_EN_ALWAYS,
};


struct lpctmu_platform_data {
    u8 ext_stop_ch_en;
    u8 ext_stop_sel;
    u8 sample_window_time;              //采样窗口时间 ms
    u8 sample_scan_time;                //多久采样一次 ms
    u8 lowpower_sample_scan_time;       //软关机下多久采样一次 ms
    u16 aim_vol_delta;
    u16 aim_charge_khz;
    u32 dma_nvram_addr;
    u32 dma_nvram_size;
};

struct lpctmu_config_data {
    u8 ch_num;
    u8 ch_list[LPCTMU_CHANNEL_SIZE];
    u8 ch_fixed_isel[LPCTMU_CHANNEL_SIZE];
    u8 ch_en;
    u8 ch_wkp_en;
    u8 softoff_wakeup_cfg;

    u8 ch_key_num;
    u8 ch_key_en;
    u8 ch_ear_en;
    volatile u8 sample_cnt;
    u8 sample_cnt_max;
    volatile u8 ear_sample_dis;
    volatile u8 ear_sample_cnt;
    u8 ear_sample_cnt_max;
    volatile u8 ear_group_cnt;
    u8 ear_group_cnt_max;
    u8 ear_ch_list[2][2];
    u8 ear_ref_io_mode[2];
    s16 ch_p_res[LPCTMU_CHANNEL_SIZE + 2];
    s16 ch_pp_res[LPCTMU_CHANNEL_SIZE + 2];

    u16 lim_l[LPCTMU_CHANNEL_SIZE];
    u16 lim_h[LPCTMU_CHANNEL_SIZE];
    u16 lim_span_max[LPCTMU_CHANNEL_SIZE];
    const struct lpctmu_platform_data *pdata;
    void (*isr_cbfunc)(u32);
};



#define LPCTMU_PLATFORM_DATA_BEGIN(data) \
    const struct lpctmu_platform_data data

#define LPCTMU_PLATFORM_DATA_END() \
    .ext_stop_ch_en = 0, \
    .ext_stop_sel = 0,\
    .sample_window_time = 2, \
    .sample_scan_time = 20, \
    .lowpower_sample_scan_time = 100, \
    .dma_nvram_addr = 0xf20000, \
    .dma_nvram_size = 240, \
}


u32 lpctmu_get_cur_ch_by_idx(u32 ch_idx);

u32 lpctmu_get_idx_by_cur_ch(u32 cur_ch);

void lpctmu_set_ana_hv_level(u32 level);

u32 lpctmu_get_ana_hv_level(void);

void lpctmu_set_ana_cur_level(u32 ch, u32 cur_level);

u32 lpctmu_get_ana_cur_level(u32 ch);

void lpctmu_cur_trim_by_res(u32 ch, u32 ch_res);

u32 lpctmu_get_ch_idx_by_res_kfifo_out(void);

u32 lpctmu_get_res_kfifo_length(void);

void lpctmu_abandon_dma_data(void);

u32 lpctmu_get_res_kfifo_data(u16 *buf, u32 len);

void lpctmu_cache_ch_res_key_msg_lim(u32 ch, u16 lim_l, u16 lim_h);

void lpctmu_ch_res_key_msg_lim_upgrade(void);

void lpctmu_set_dma_res_ie(u32 en);

void lpctmu_dump(void);

void lpctmu_init(struct lpctmu_config_data *cfg_data);

void lpctmu_disable(void);

void lpctmu_enable(void);

u32 lpctmu_is_working(void);

u32 lpctmu_is_sf_keep(void);

void lpctmu_lowpower_enter(u32 lowpwr_mode);

void lpctmu_lowpower_exit(u32 lowpwr_mode);

void lpctmu_eartch_enable(void);

void lpctmu_eartch_disable(void);


#endif


