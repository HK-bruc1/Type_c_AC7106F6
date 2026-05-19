#ifndef _AUDIO_DVOL_H_
#define _AUDIO_DVOL_H_

#include "generic/typedef.h"
#include "os/os_type.h"
#include "os/os_api.h"
#include "generic/list.h"
#include "asm/math_fast_function.h"

#define DVOL_RESOLUTION				14
#define DVOL_MAX					16384
#define DVOL_MAX_FLOAT				16384.0f

#define BG_DVOL_FADE_ENABLE			0		//多路声音叠加，背景声音自动淡出小声
#define AUDIO_DVOL_OFFSET_ENABLE	1		//音量偏移使能：可用来做音量补偿或者自适应
#define DIGITAL_VOLUME_LEVEL_MAX  	200  	//默认的音量等级限制设成200

/*Digital Volume Channel*/
#define MUSIC_DVOL		    0b00001
#define CALL_DVOL			0b00010
#define TONE_DVOL			0b00100
#define RING_DVOL			0b01000
#define KEY_TONE_DVOL		0b10000

/*Digital Volume Fade Step*/
#define MUSIC_DVOL_FS		2
#define CALL_DVOL_FS		4
#define TONE_DVOL_FS		30

/*Digital Volume Max Level*/
#define MUSIC_DVOL_MAX		16
#define CALL_DVOL_MAX		15
#define TONE_DVOL_MAX		16

struct audio_vol_params {
    u8 bit_wide;
    u16 vol;
    u16 vol_max;
    u16 fade_step;
    u16 *vol_table;            /*自定义音量表*/
    float min_db;
    float max_db;
};

typedef struct {
    u8 toggle;					/*数字音量开关*/
    u8 fade;					/*淡入淡出标志*/
    u8 vol_table_default;    	/*是否使用默认的音量表*/
    u8 mute_en;              	/*是否将数据设成0*/
    u8 bit_wide;            	/*数据位宽*/
    u16 vol;					/*淡入淡出当前音量(level)*/
    u16 vol_max;				/*淡入淡出最大音量(level)*/
    s16 vol_fade;				/*淡入淡出对应的起始音量*/
    s16 vol_target;	            /*淡入淡出对应的目标音量*/
    u16 fade_step;		        /*淡入淡出的步进*/
#if AUDIO_DVOL_OFFSET_ENABLE
    s16 offset;				/*音量偏移大小*/
    s16 min_vol;				/*最小音量幅度*/
    s16 max_vol;				/*最大音量幅度*/
#endif
#if BG_DVOL_FADE_ENABLE
    s16 vol_bk;					/*后台自动淡出前音量值*/
    struct list_head entry;
#endif
    float offset_dB;        	/*音量偏移分贝数*/
    float min_db;         		/*最小音量的分贝数*/
    float max_db;         		/*最大音量的分贝数*/
    u16 *vol_table;       		/*自定义音量表*/
} dvol_handle;

int audio_digital_vol_init(u16 *vol_table, u16 vol_max);
void audio_digital_vol_bg_fade(u8 fade_out);
dvol_handle *audio_digital_vol_open(struct audio_vol_params *params);
void audio_digital_vol_close(dvol_handle  *dvol);
void audio_digital_vol_set(dvol_handle *dvol, u16 vol);
void audio_digital_vol_set_mute(dvol_handle *dvol, u8 mute_en);
void audio_digital_vol_offset_dB_set(dvol_handle *dvol, float offset_dB);
int audio_digital_vol_run(dvol_handle *dvol, void *data, u32 len);
void audio_digital_vol_reset_fade(dvol_handle *dvol);

float audio_digital_vol_2_dB(dvol_handle *dvol, u16 volume);
void audio_digital_vol_db_2_gain(float *db_table, int table_size, u16 *gain_table);

#endif/*_AUDIO_DVOL_H_*/
