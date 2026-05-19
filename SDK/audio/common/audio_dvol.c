/*
 ****************************************************************
 *							AUDIO DIGITAL VOLUME
 * File  : audio_dvol.c
 * By    :
 * Notes : 数字音量模块，支持多通道音量控制
 ****************************************************************
 */

#include "audio_dvol.h"
#include "system/spinlock.h"
#include "effects/audio_eq.h"

#if 1
#define dvol_log	y_printf
#else
#define dvol_log(...)
#endif

#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".audio_dvol.data.bss")
#pragma data_seg(".audio_dvol.data")
#pragma const_seg(".audio_dvol.text.const")
#pragma code_seg(".audio_dvol.text")
#endif

#define __AUDIO_DIGITAL_VOL_CACHE_CODE	__attribute__((section(".digital_vol.text.cache.L2.run")))

#define DIGITAL_FADE_EN 	1

#define BG_DVOL_MAX			14
#define BG_DVOL_MID			10
#define BG_DVOL_MIN		    6
#define BG_DVOL_MAX_FADE	5	/*>= BG_DVOL_MAX:自动淡出BG_DVOL_MAX_FADE*/
#define BG_DVOL_MID_FADE	3	/*>= BG_DVOL_MID:自动淡出BG_DVOL_MID_FADE*/
#define BG_DVOL_MIN_FADE	1	/*>= BG_DVOL_MIN:自动淡出BG_DVOL_MIN_FADE*/


typedef struct {
    u8 bg_dvol_fade_out;
    u8 start;
    u16 digital_vol_max;
    u16 *dig_vol_table;
#if BG_DVOL_FADE_ENABLE
    struct list_head dvol_head;
    spinlock_t lock;
#endif
} dvol_t;

static dvol_t dvol_attr;

/*
 *数字音量级数 DEFAULT_DIGITAL_VOL_MAX
 *数组长度 DEFAULT_DIGITAL_VOL_MAX + 1
 */
#define DEFAULT_DIGITAL_VOL_MAX     (31)
const u16 default_dig_vol_table[DEFAULT_DIGITAL_VOL_MAX + 1] = {
    0, //0: None
    92, // 1:-45.00 db
    110, // 2:-43.50 db
    130, // 3:-42.00 db
    155, // 4:-40.50 db
    184, // 5:-39.00 db
    218, // 6:-37.50 db
    260, // 7:-36.00 db
    309, // 8:-34.50 db
    367, // 9:-33.00 db
    436, // 10:-31.50 db
    518, // 11:-30.00 db
    616, // 12:-28.50 db
    732, // 13:-27.00 db
    870, // 14:-25.50 db
    1034, // 15:-24.00 db
    1229, // 16:-22.50 db
    1460, // 17:-21.00 db
    1735, // 18:-19.50 db
    2063, // 19:-18.00 db
    2451, // 20:-16.50 db
    2914, // 21:-15.00 db
    3463, // 22:-13.50 db
    4115, // 23:-12.00 db
    4891, // 24:-10.50 db
    5813, // 25:-9.00 db
    6909, // 26:-7.50 db
    8211, // 27:-6.00 db
    9759, // 28:-4.50 db
    11599, // 29:-3.00 db
    13785, // 30:-1.50 db
    16384, // 31:0.00 db
};


/*
*********************************************************************
*                  Audio Digital Volume Init
* Description: 数字音量模块初始化
* Arguments  : None.
* Return	 : 0 成功 其他 失败
* Note(s)    : None.
*********************************************************************
*/
__AUDIO_INIT_BANK_CODE
int audio_digital_vol_init(u16 *vol_table, u16 vol_max)
{
    memset(&dvol_attr, 0, sizeof(dvol_attr));

#if BG_DVOL_FADE_ENABLE
    INIT_LIST_HEAD(&dvol_attr.dvol_head);
    spin_lock_init(&dvol_attr.lock);
#endif

    if (vol_table != NULL) {
        dvol_attr.dig_vol_table = vol_table;
        dvol_attr.digital_vol_max = vol_max;
    } else {
        dvol_attr.dig_vol_table = (u16 *)default_dig_vol_table;
        dvol_attr.digital_vol_max = DEFAULT_DIGITAL_VOL_MAX;
    }

    return 0;
}

/*背景音乐淡出使能*/
void audio_digital_vol_bg_fade(u8 fade_out)
{
    dvol_log("audio_digital_vol_bg_fade:%d", fade_out);
    dvol_attr.bg_dvol_fade_out = fade_out;
}

void audio_digital_vol_db_2_gain(float *db_table, int table_size, u16 *gain_table)
{
    for (int i = 0; i < table_size; i++) {
        gain_table[i] = (s16)(DVOL_MAX_FLOAT  * dB_Convert_Mag(db_table[i]) + 0.5f);
    }
}

float audio_digital_vol_2_dB(dvol_handle *dvol, u16 volume)
{
    u16 step = (dvol->vol_max - 1 > 0) ? (dvol->vol_max - 1) : 1;
    float step_db = (dvol->max_db - dvol->min_db) / (float)step;
    float dvol_db = dvol->min_db  + (volume - 1) * step_db;
    printf("vol_dB :%d, %d\n", volume, (int)dvol_db);
    return (dvol_db);
}

static s16 audio_digital_vol_2_gain(dvol_handle *dvol, u16 volume)
{
    s16 gain;

    if (volume == 0) {
        gain = 0;
    } else if (dvol->vol_table) {
        gain = dvol->vol_table[volume - 1];
    } else if (dvol->vol_table_default) {
        if (volume > dvol->vol_max) {
            volume = dvol->vol_max;
        }
        int vol_level = volume * dvol_attr.digital_vol_max / dvol->vol_max;
        gain = dvol_attr.dig_vol_table[vol_level];
    } else {
        float db = audio_digital_vol_2_dB(dvol, volume);
        gain = (s16)(DVOL_MAX_FLOAT * dB_Convert_Mag(db) + 0.5f);
    }
    printf("vol_2_gain: %d, %d, %d\n", volume, dvol->vol_max, gain);
    return gain;
}
/*
*********************************************************************
*                  Audio Digital Volume Open
* Description: 数字音量模块打开
* Arguments  : vol			当前数字音量等级
*			   vol_max		最大数字音量等级
*			   fade_step	淡入淡出步进
*			   vol_limit	数字音量限制，即可以给vol_max等分的最大音量
* Return	 : 数字音量通道句柄
* Note(s)    : fade_step一般不超过两级数字音量的最小差值
*				(1)通话如果用数字音量，一般步进小一点，音量调节的时候不会有杂音
*				(2)淡出的时候可以快一点，尽快淡出到0
*********************************************************************
*/
__VOLUME_BANK_CODE
dvol_handle *audio_digital_vol_open(struct audio_vol_params *params)
{
    dvol_handle *dvol;

    dvol = zalloc(sizeof(dvol_handle));
    if (!dvol) {
        return NULL;
    }
    u16 vol       = params->vol;
    u16 vol_max   = params->vol_max;
    if (vol > vol_max) {
        vol = vol_max;
        printf("[warning]cur digital_vol(%d) > digital_vol_max(%d)!!", vol, vol_max);
    }
    dvol->bit_wide  = params->bit_wide;
    dvol->min_db    = params->min_db;
    dvol->max_db    = params->max_db;
    dvol->vol_table = params->vol_table;
    dvol->fade_step = params->fade_step;
    dvol->fade 		= DIGITAL_FADE_EN;
    dvol->vol 		= vol;
    dvol->vol_max 	= vol_max;
    dvol->toggle 	= 1;
    if (params->min_db == -45 && params->max_db == 0 && params->vol_max == 31) {
        dvol->vol_table_default = 1; //使用默认的音量表
    }
    dvol->vol_target = audio_digital_vol_2_gain(dvol, dvol->vol);
#if AUDIO_DVOL_OFFSET_ENABLE
    if (dvol->vol_table_default) {
        dvol->min_vol = default_dig_vol_table[0];
        dvol->max_vol = default_dig_vol_table[dvol->vol_max];
    } else {
        if (dvol->vol_table) {
            dvol->min_vol = 0;//(s16)(eq_db2mag(dvol->vol_table[0]) *  DVOL_MAX_FLOAT);//dB转换倍数
            dvol->max_vol = (s16)(eq_db2mag(dvol->vol_table[dvol->vol_max]) *  DVOL_MAX_FLOAT);
        } else {
            dvol->min_vol = 0;//(s16)(eq_db2mag(dvol->cfg_vol_min) *  DVOL_MAX_FLOAT);//dB转换倍数
            dvol->max_vol = (s16)(eq_db2mag(dvol->max_db) *  DVOL_MAX_FLOAT);
        }
    }
    /* printf("dvol_offset,max_vol:%d,min_vol:%d",dvol->max_vol,dvol->min_vol); */
#endif

#if BG_DVOL_FADE_ENABLE
    spin_lock(&dvol_attr.lock);
    list_add(&dvol->entry, &dvol_attr.dvol_head);
    dvol->vol_bk = -1;

    if (dvol_attr.bg_dvol_fade_out) {
        dvol_handle *hdl;
        list_for_each_entry(hdl, &dvol_attr.dvol_head, entry) {
            if (hdl != dvol) {
                hdl->vol_bk = hdl->vol;
                if (hdl->vol >= BG_DVOL_MAX) {
                    hdl->vol -= BG_DVOL_MAX_FADE;
                } else if (hdl->vol >= BG_DVOL_MID) {
                    hdl->vol -= BG_DVOL_MID_FADE;
                } else if (hdl->vol >= BG_DVOL_MIN) {
                    hdl->vol -= BG_DVOL_MIN_FADE;
                } else {
                    hdl->vol_bk = -1;
                    continue;
                }
                hdl->vol_target = audio_digital_vol_2_gain(hdl, hdl->vol);
            }
        }
    }
    spin_unlock(&dvol_attr.lock);
#endif
    /* dvol_log("dvol_open:%x-%d-%d-%d\n",  dvol, dvol->vol, dvol->vol_max, dvol->fade_step); */
    return dvol;
}

/*
*********************************************************************
*                  Audio Digital Volume Close
* Description: 数字音量模块关闭
* Arguments  : dvol	数字音量操作句柄，详见audio_dvol.h宏定义
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void audio_digital_vol_close(dvol_handle  *dvol)
{

    if (dvol) {
#if BG_DVOL_FADE_ENABLE
        spin_lock(&dvol_attr.lock);
        list_del(&dvol->entry);
        dvol_handle *hdl;
        list_for_each_entry(hdl, &dvol_attr.dvol_head, entry) {
            if ((hdl != dvol) && (hdl->vol_bk >= 0)) {
                //y_printf("bg_dvol fade_in:%x,%d",hdl,hdl->vol_bk);
                hdl->vol        =  hdl->vol_bk;
                hdl->vol_bk     = -1;
                hdl->vol_target = audio_digital_vol_2_gain(hdl, hdl->vol);
            }
        }
        spin_unlock(&dvol_attr.lock);
#endif
        free(dvol);
    }
}

/*
*********************************************************************
*                  Audio Digital Mute Set
* Description: 数字音量设置mute
* Arguments  : dvol     数字音量操作句柄，详见audio_dvol.h宏定义
*			   mute_en	是否使能mute
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void audio_digital_vol_set_mute(dvol_handle *dvol, u8 mute_en)
{
    dvol->mute_en = mute_en;
    if (mute_en) {
        dvol->fade = 1;
        dvol->vol_fade = dvol->vol_target;
    } else {
        dvol->vol_fade = 0;
    }
}

/*
*********************************************************************
*                  Audio Digital offset Set
* Description: 数字音量设置音量偏移大小
* Arguments  : dvol     数字音量操作句柄，详见audio_dvol.h宏定义
*			   offset	音量偏移大小
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void audio_digital_vol_offset_set(dvol_handle *dvol, s16 offset)
{
#if AUDIO_DVOL_OFFSET_ENABLE
    if (dvol == NULL) {
        return;
    }
    dvol->offset = offset;
#endif
}

/*
*********************************************************************
*                  Audio Digital offset Set
* Description: 数字音量设置音量dB偏移大小
* Arguments  : dvol         数字音量操作句柄，详见audio_dvol.h宏定义
*			   offset_dB	音量偏移大小，单位dB
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void audio_digital_vol_offset_dB_set(dvol_handle *dvol, float offset_dB)
{
#if AUDIO_DVOL_OFFSET_ENABLE
    if (dvol == NULL) {
        return;
    }
    s16 cur_vol = dvol->vol_target;
    dvol->offset_dB = offset_dB;
    float tar_dB = 20 * log10_float(cur_vol / DVOL_MAX_FLOAT) + offset_dB;
    tar_dB = (tar_dB > 0.0f) ? 0.0f : tar_dB; //不允许计算出超过0dB的目标音量值
    s16 tar_vol = (s16)(eq_db2mag(tar_dB) * DVOL_MAX_FLOAT + 0.5f);
    dvol->offset = tar_vol - cur_vol;
    printf("dvol_offset_dB");
    put_float(offset_dB);
    printf("dvol offset:%d = target(%d) - current(%d)", dvol->offset, tar_vol, cur_vol);
#endif
}

/*
*********************************************************************
*                  Audio Digital Volume Set
* Description: 数字音量设置
* Arguments  : dvol     数字音量操作句柄，详见audio_dvol.h宏定义
*			   vol		目标音量等级
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void audio_digital_vol_set(dvol_handle *dvol, u16 vol)
{
    if (dvol == NULL) {
        return;
    }
    if (dvol->toggle == 0) {
        return;
    }
    dvol->vol = (vol > dvol->vol_max) ? dvol->vol_max : vol;
#if BG_DVOL_FADE_ENABLE
    if (dvol->vol_bk != -1) {
        dvol->vol_bk = vol;
    }
#endif
    dvol->fade = DIGITAL_FADE_EN;
    dvol->vol_target = audio_digital_vol_2_gain(dvol, dvol->vol);

#if AUDIO_DVOL_OFFSET_ENABLE
    /*音量改变时，更新音量dB偏移的音量大小*/
    if (dvol->offset_dB) {
        audio_digital_vol_offset_dB_set(dvol, dvol->offset_dB);
    }
#endif
}
/*********************************************************************
*                  Audio Digital Volume Set
* Description: 数字音量设置,设置数字音量的当前值，不用淡入淡出
* Arguments  : dvol     数字音量操作句柄，详见audio_dvol.h宏定义
*			   vol		目标音量等级
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/

void audio_digital_vol_set_no_fade(dvol_handle *dvol, u8 vol)
{
    dvol_log("audio_digital_vol set:%p,%d\n", dvol, vol);
    if (dvol == NULL) {
        return;
    }
    if (dvol->toggle == 0) {
        return;
    }
    dvol->vol = (vol > dvol->vol_max) ? dvol->vol_max : vol;
#if BG_DVOL_FADE_ENABLE
    if (dvol->vol_bk != -1) {
        dvol->vol_bk = vol;
    }
#endif
    dvol->fade = 0;
    dvol->vol_target = audio_digital_vol_2_gain(dvol, dvol->vol);

#if AUDIO_DVOL_OFFSET_ENABLE
    /*音量改变时，更新音量dB偏移的音量大小*/
    if (dvol->offset_dB) {
        audio_digital_vol_offset_dB_set(dvol, dvol->offset_dB);
    }
#endif
}

void audio_digital_vol_reset_fade(dvol_handle *dvol)
{
    if (dvol) {
        dvol->vol_fade = 0;
    }
}

/*
*********************************************************************
*                  Audio Digital Volume Process
* Description: 数字音量运算核心
* Arguments  : dvol 	数字音量操作句柄，详见audio_dvol.h宏定义
*			   data		待运算数据
*			   len		待运算数据长度
* Return	 : 0 成功 其他 失败
* Note(s)    : None.
*********************************************************************
*/
__AUDIO_DIGITAL_VOL_CACHE_CODE
int audio_digital_vol_run(dvol_handle *dvol, void *data, u32 len)
{
    u8 fade;
    s32 valuetemp;
    s16 *buf;
    s32 *buf32;
    if ((!dvol) || (dvol->toggle == 0)) {
        return -1;
    }

    s16 vol_target = dvol->vol_target;

#if AUDIO_DVOL_OFFSET_ENABLE
    /*音量不为0时，才做偏移*/
    if (vol_target && dvol->offset) {
        vol_target += dvol->offset;
        if (vol_target < dvol->min_vol) {
            vol_target = dvol->min_vol;
        } else if (vol_target > dvol->max_vol) {
            vol_target = dvol->max_vol;
        }
    }
#endif

    if (dvol->mute_en) {
        vol_target = 0;
    }

    if ((dvol->vol_fade != vol_target) && dvol->fade) {
        fade = 1;
    } else {
        dvol->vol_fade = vol_target;
        fade = 0;
    }

    if (dvol->bit_wide) {
        buf32 = data;
        len >>= 2;
        for (u32 i = 0; i < len; i += 1) {
            if (fade) {
                if (dvol->vol_fade > vol_target) {
                    dvol->vol_fade -= dvol->fade_step;
                    if (dvol->vol_fade < vol_target) {
                        dvol->vol_fade = vol_target;
                    }
                } else if (dvol->vol_fade < vol_target) {
                    dvol->vol_fade += dvol->fade_step;
                    if (dvol->vol_fade > vol_target) {
                        dvol->vol_fade = vol_target;
                    }
                }
            }

            valuetemp = buf32[i];
            if (valuetemp < 0) {
                /*负数先转换成正数，运算完再转换回去，是为了避免负数右移位引入1的误差，增加底噪*/
                valuetemp = -valuetemp;
                /*rounding处理（加入0.5），减少小信号时候的误差和谐波幅值*/
                valuetemp = ((long long)valuetemp * (long long)dvol->vol_fade + (long long)(1 << 13)) >> DVOL_RESOLUTION ;
                valuetemp = -valuetemp;
            } else {
                valuetemp = ((long long)valuetemp * (long long)dvol->vol_fade + (long long)(1 << 13)) >> DVOL_RESOLUTION ;
            }
#ifdef DIGITAL_VOLUME_SAT_ENABLE
            /*饱和处理*/
            if (valuetemp > 8388607) { //2^23 -1
                valuetemp = 8388607;
            } else if (valuetemp < -8388608) { //-2^23
                valuetemp = -8388608;
            }
#endif
            buf32[i] = valuetemp;
        }
    } else {
        buf = data;
        len >>= 1; //byte to point
        for (u32 i = 0; i < len; i += 1) {
            if (fade) {
                if (dvol->vol_fade > vol_target) {
                    dvol->vol_fade -= dvol->fade_step;
                    if (dvol->vol_fade < vol_target) {
                        dvol->vol_fade = vol_target;
                    }
                } else if (dvol->vol_fade < vol_target) {
                    dvol->vol_fade += dvol->fade_step;
                    if (dvol->vol_fade > vol_target) {
                        dvol->vol_fade = vol_target;
                    }
                }
            }
            valuetemp = buf[i];
            if (valuetemp < 0) {
                /*负数先转换成正数，运算完再转换回去，是为了避免负数右移位引入1的误差，增加底噪*/
                valuetemp = -valuetemp;
                /*rounding处理（加入0.5），减少小信号时候的误差和谐波幅值*/
                valuetemp = (valuetemp * dvol->vol_fade + (1 << 13)) >> DVOL_RESOLUTION ;
                valuetemp = -valuetemp;
            } else {
                valuetemp = (valuetemp * dvol->vol_fade + (1 << 13)) >> DVOL_RESOLUTION ;
            }
#ifdef DIGITAL_VOLUME_SAT_ENABLE
            /*饱和处理*/
            buf[i] = (s16)data_sat_s16(valuetemp);
#else
            buf[i] = (s16)valuetemp;
#endif
        }
    }

    return 0;
}

