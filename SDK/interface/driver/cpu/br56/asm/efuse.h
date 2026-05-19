#ifndef  __EFUSE_H__
#define  __EFUSE_H__


u32 efuse_get_gpadc_vbg_trim();
u32 get_chip_version();

u8 get_en_act();
u8 get_vddio_lvd_en();
u8 efuse_get_lvd_act();
u8 efuse_get_vddio_lvd_lev();
u8 efuse_get_need_flash_reset();
u8 efuse_get_vio_act();
u8 efuse_get_vddio_lev();
u8 efuse_get_mclr_en_dis();
u8 efuse_get_xosc_pin_mode();
u8 efuse_get_dvdd_cap_dis();
u8 efuse_get_sfc_fast_boot_dis();
u8 efuse_get_pin_reset_en();
u8 efuse_get_fast_up();
u8 efuse_get_flash_io();
u8 efuse_get_vbg_act();
u8 efuse_get_lvd_bg_trim();
u8 efuse_get_mvbg_lev();
u8 efuse_get_en_wvbg_lev();

u8 efuse_get_cp_pass();
u8 efuse_get_ft_pass();
u8 efuse_get_wvdd_level_trim();
u16 efuse_get_vbat_trim_4p20(void);
u16 efuse_get_vbat_trim_4p40(void);
u16 efuse_get_vbat_trim_4p50(void);
u16 efuse_get_charge_cur_trim(void);
u16 efuse_get_io_pu_100k(void);
u16 efuse_get_vtemp(void);

u8 efuse_get_audio_vbg_trim();
u8 efuse_get_xosc_ldo();
u16 efuse_get_vbat_trim_4p45(void);
u8 efuse_get_btvbg_sysldo11a_level();
u32 efuse_get_chip_id();
u16 get_chip_id();
u32 efuse_get_vbat_3700();


void efuse_init();


#endif  /*EFUSE_H*/
