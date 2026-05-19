#ifndef CVP_NODE_H
#define CVP_NODE_H

#include "audio_config.h"
#include "audio_cvp.h"
#include "effects/effects_adj.h"
#include "adc_file.h"

struct cvp_param_fmt {
    u8  mic_num;
    u16 source_uuid;
};

int cvp_sms_node_param_cfg_read(void *priv, u8 ignore_subid, u16 algo_uuid);
int cvp_sms_vf_node_param_cfg_read(void *priv, u8 ignore_subid, u16 algo_uuid);
int cvp_dms_node_param_cfg_read(void *priv, u8 ignore_subid, u16 algo_uuid);
int cvp_tms_node_param_cfg_read(void *priv, u8 ignore_subid, u16 algo_uuid);
int cvp_v3_node_param_cfg_read(void *priv, u8 ignore_subid, u16 algo_uuid);
int cvp_sms_node_output_handle(s16 *data, u16 len);
int cvp_sms_vf_node_output_handle(s16 *data, u16 len);
int cvp_dms_node_output_handle(s16 *data, u16 len);
int cvp_tms_node_output_handle(s16 *data, u16 len);
int cvp_v3_node_output_handle(s16 *data, u16 len);

int cvp_sms_param_cfg_read(void);
int cvp_dms_param_cfg_read(void);
int cvp_tms_param_cfg_read(void);
int cvp_v3_param_cfg_read(void);
int cvp_param_cfg_read(void);
u8 cvp_get_talk_mic_ch(void);
u8 cvp_v3_get_talk_mic_ch(void);
u8 cvp_get_talk_ref_mic_ch(void);
u8 cvp_v3_get_talk_ref_mic_ch(void);
u8 cvp_get_talk_fb_mic_ch(void);
u8 cvp_v3_get_talk_fb_mic_ch(void);
int cvp_get_algo_type(void);

#endif/*CVP_NODE_H*/

