#ifndef _AEC_DAC_CH_REF_DATA_H__
#define _AEC_DAC_CH_REF_DATA_H__

#include "generic/typedef.h"
#include "audio_dac.h"
#include "Resample_api.h"

int aec_ref_dac_ch_data_read_init();
int aec_ref_dac_ch_data_read_exit();
void aec_ref_dac_ch_data_src_init(RS_PARA_STRUCT *rs_para_obj);
void aec_ref_dac_ch_data_src_close();
int aec_ref_dac_ch_data_fill(struct audio_dac_hdl *dac_hdl, s16 *data, int len);
int aec_ref_dac_ch_data_read(struct audio_dac_hdl *dac_hdl, u8 is_stereo_to_mono, s16 *data, int len);

void set_aec_ref_dac_ch_write_flag(u8 flag);
u8 get_aec_ref_dac_ch_write_flag();

int aec_ref_dac_ch_data_align(int len);
int aec_ref_dac_ch_data_read_goback(int len);
int aec_ref_dac_ch_data_read_updata(int len);
int aec_ref_dac_ch_data_write_updata(struct audio_dac_hdl *dac_hdl, int len);
void aec_ref_dac_ch_data_clear(void);

void set_aec_ref_dac_ch_name(char name[16]);
u8 is_aec_ref_dac_ch(struct audio_dac_channel *dac_ch);
u32 aec_ref_dac_ch_data_size_get(void);
u32 aec_ref_dac_ch_data_total_size_get(void);

#endif
