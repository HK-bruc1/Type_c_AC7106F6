#ifndef __AUDIO_DAC_H__
#define __AUDIO_DAC_H__

#include "audio_cfifo.h"
#include "asm/dac.h"

#define DAC_NOISEGATE_OFF()     audio_dac_noisefloor_optimize_onoff(0);
#define DAC_NOISEGATE_ON()      audio_dac_noisefloor_optimize_onoff(1);

struct audio_dac_channel_attr {
    u8  write_mode;         /*DAC写入模式*/
    u16 delay_time;         /*DAC通道延时*/
    u16 protect_time;       /*DAC延时保护时间*/
};

struct audio_dac_channel {
    u8  state;              /*DAC状态*/
    u8  pause;
    u8  samp_sync_step;     /*数据流驱动的采样同步步骤*/
    struct audio_dac_channel_attr attr;     /*DAC通道属性*/
    struct audio_cfifo_channel fifo;        /*DAC cfifo通道管理*/
};



struct audio_dac_sync_node {
    u8 triggered;
    u8 network;
    u32 timestamp;
    void *hdl;
    struct list_head entry;
    void *ch;
};

#define DAC_READ_MAGIC		0xAA55
struct dac_read_handle {
    u16 read_pos;
    u16 cur_dac_hrp;
    u16 last_dac_hrp;
    int dac_hrp_diff;
    struct list_head entry;
};

extern struct audio_dac_hdl dac_hdl;

void audio_dac_clock_init();

int audio_dac_clk_switch(u8 clk);

int audio_dac_clk_get(void);
/*
*********************************************************************
*              audio_dac_init
* Description: DAC 初始化
* Arguments  : dac      dac 句柄
*			   pd		dac 参数配置结构体
* Return	 : 0：成功  -1：失败
* Note(s)    :
*********************************************************************
*/
int audio_dac_init(struct audio_dac_hdl *dac, const struct dac_platform_data *pd);

void *audio_dac_get_pd_data(void);
/*
*********************************************************************
*              audio_dac_get_hdl
* Description: 获取 DAC 句柄
* Arguments  :
* Return	 : DAC 句柄
* Note(s)    :
*********************************************************************
*/
struct audio_dac_hdl *audio_dac_get_hdl(void);


/*
*********************************************************************
*              audio_dac_do_trim
* Description: DAC 直流偏置校准
* Arguments  : dac      	dac 句柄
*			   dac_trim		存放 trim 值结构体的地址
*              fast_trim 	快速 trim 使能
* Return	 : 0：成功  -1：失败
* Note(s)    :
*********************************************************************
*/
int audio_dac_do_trim(struct audio_dac_hdl *dac, struct audio_dac_trim *dac_trim,
                      struct trim_init_param_t *trim_init);

/*
*********************************************************************
*              audio_dac_set_trim_value
* Description: 将 DAC 直流偏置校准值写入 DAC 配置
* Arguments  : dac      	dac 句柄
*			   dac_trim		存放 trim 值结构体的地址
* Return	 : 0：成功  -1：失败
* Note(s)    :
*********************************************************************
*/
int audio_dac_set_trim_value(struct audio_dac_hdl *dac, struct audio_dac_trim *dac_trim);


int audio_dac_trim_value_check(struct audio_dac_trim *dac_trim);

/*********************************************************************
*              audio_dac_irq_handler
* Description: DAC 中断回调函数
* Arguments  : dac      	dac 句柄
* Return	 :
* Note(s)    :
*********************************************************************
*/
void audio_dac_irq_handler(struct audio_dac_hdl *dac);



/*
*********************************************************************
*              audio_dac_set_buff
* Description: 设置 DAC 的 DMA buff
* Arguments  : dac      	dac 句柄
*              buf  		应用层分配的 DMA buff 起始地址
*              len  		DMA buff 的字节长度
* Return	 : 0：成功  -1：失败
* Note(s)    :
*********************************************************************
*/
int audio_dac_set_buff(struct audio_dac_hdl *dac, s16 *buf, int len);

/*
*********************************************************************
*              audio_dac_write
* Description: 将数据写入默认的 dac channel cfifo。等同于调用 audio_dac_channel_write 函数 private_data 传 NULL
* Arguments  : dac      	dac 句柄
*              buf  		数据的起始地址
*              len  		数据的字节长度
* Return	 : 成功写入的数据字节长度
* Note(s)    :
*********************************************************************
*/
int audio_dac_write(struct audio_dac_hdl *dac, void *buf, int len);



/*
*********************************************************************
*              audio_dac_set_sample_rate
* Description: 设置 DAC 的输出采样率
* Arguments  : dac      		dac 句柄
*              sample_rate  	采样率
* Return	 : 0：成功  -1：失败
* Note(s)    :
*********************************************************************
*/
int audio_dac_set_sample_rate(struct audio_dac_hdl *dac, int sample_rate);

/*
*********************************************************************
*              audio_dac_get_sample_rate
* Description: 获取 DAC 的输出采样率
* Arguments  : dac      		dac 句柄
* Return	 : 采样率
* Note(s)    :
*********************************************************************
*/
int audio_dac_get_sample_rate(struct audio_dac_hdl *dac);

int audio_dac_get_sample_rate_base_reg();

u32 audio_dac_sample_rate_match(u32 sr, u8 *reg_value);

void audio_dac_set_sample_rate_callback(struct audio_dac_hdl *dac, void (*cb)(int));

void audio_dac_del_sample_rate_callback(struct audio_dac_hdl *dac, void (*cb)(int));


int audio_dac_get_channel(struct audio_dac_hdl *dac);

/*
*********************************************************************
*              audio_dac_set_digital_vol
* Description: 设置 DAC 的数字音量
* Arguments  : dac      	dac 句柄
*              vol  		需要设置的数字音量等级
* Return	 : 0：成功  -1：失败
* Note(s)    :
*********************************************************************
*/
int audio_dac_set_digital_vol(struct audio_dac_hdl *dac, u16 vol);

/*
*********************************************************************
*              audio_dac_set_analog_vol
* Description: 设置 DAC 的模拟音量
* Arguments  : dac      	dac 句柄
*              vol  		需要设置的模拟音量等级
* Return	 : 0：成功  -1：失败
* Note(s)    :
*********************************************************************
*/
int audio_dac_set_analog_vol(struct audio_dac_hdl *dac, u16 vol);

int audio_dac_ch_analog_gain_set(u32 ch, u32 again);

int audio_dac_ch_analog_gain_get(u32 ch);

int audio_dac_ch_digital_gain_get(struct audio_dac_hdl *dac, u32 ch);

/*
*********************************************************************
*              audio_dac_set_volume
* Description: 设置音量等级记录变量，但是不会直接修改音量。只有当 DAC 关闭状态时，第一次调用 audio_dac_channel_start 打开 dac fifo 后，会根据 audio_dac_set_fade_handler 设置的回调函数来设置系统音量，回调函数的传参就是 audio_dac_set_volume 设置的音量值。
* Arguments  : dac      	dac 句柄
			   gain			记录的音量等级
* Return	 : 0：成功  -1：失败
* Note(s)    :
*********************************************************************
*/
int audio_dac_set_volume(struct audio_dac_hdl *dac, u8 gain);


/*
*********************************************************************
*              audio_dac_ch_mute
* Description: 将某个通道静音，用于降低底噪，或者做串扰隔离的功能
* Arguments  : dac      	dac 句柄
*              ch			需要操作的通道，BIT(n)代表操作第n个通道，可以多个通道或上操作
*			   mute		    0:解除mute  1:mute
* Return	 : 0：成功  -1：失败
* Note(s)    :
*********************************************************************
*/
void audio_dac_ch_mute(struct audio_dac_hdl *dac, u8 ch, u8 mute);


u8 audio_dac_digital_mute_state(struct audio_dac_hdl *hdl);

void audio_dac_digital_mute(struct audio_dac_hdl *dac, u8 mute);


/*
*********************************************************************
*          audio_dac_volume_enhancement_mode_set
* Description: DAC 音量增强模式切换
*			   mode		1：音量增强模式  0：普通模式
* Return	 : 0：成功  -1：失败
* Note(s)    : 不能在中断调用
*********************************************************************
*/
int audio_dac_volume_enhancement_mode_set(struct audio_dac_hdl *dac, u8 mode);

/*
*********************************************************************
*          audio_dac_volume_enhancement_mode_get
* Description: DAC 音量增强模式切换
* Arguments  : dac      dac 句柄
* Return	 : 1：音量增强模式  0：普通模式
* Note(s)    : None.
*********************************************************************
*/
u8 audio_dac_volume_enhancement_mode_get(struct audio_dac_hdl *dac);



/*
*********************************************************************
*              audio_dac_try_power_on
* Description: 根据设置好的参数打开 DAC 的模拟模块和数字模块。与 audio_dac_start 功能基本一样，但不设置 PNS
* Arguments  : dac      	dac 句柄
* Return	 : 0：成功  -1：失败
* Note(s)    :
*********************************************************************
*/
int audio_dac_try_power_on(struct audio_dac_hdl *dac);

/*
*********************************************************************
*              audio_dac_start
* Description: 根据设置好的参数打开 DAC 的模拟模块和数字模块。
* Arguments  : dac      	dac 句柄
* Return	 : 0：成功  -1：失败
* Note(s)    :
*********************************************************************
*/
int audio_dac_start(struct audio_dac_hdl *dac);

/*
*********************************************************************
*              audio_dac_stop
* Description: 关闭 DAC 数字部分。所有 DAC channel 都关闭后才能调用这个函数
* Arguments  : dac      	dac 句柄
* Return	 : 0：成功  -1：失败
* Note(s)    :
*********************************************************************
*/
int audio_dac_stop(struct audio_dac_hdl *dac);



int audio_dac_open(struct audio_dac_hdl *dac);

/*
*********************************************************************
*              audio_dac_close
* Description: 关闭 DAC 模拟部分。audio_dac_stop 之后才可以调用
* Arguments  : dac      	dac 句柄
* Return	 : 0：成功  -1：失败
* Note(s)    :
*********************************************************************
*/
int audio_dac_close(struct audio_dac_hdl *dac);


void audio_dac_ch_high_resistance(struct audio_dac_hdl *dac, u8 ch, u8 en);


/*
*********************************************************************
*              audio_dac_set_fade_handler
* Description: DAC 关闭状态时，第一次调用 audio_dac_channel_start 打开 dac fifo 后，会根据 audio_dac_set_fade_handler 设置的回调函数来设置系统音量
* Arguments  : dac      		dac 句柄
*              priv				暂无作用
*			   fade_handler 	回调函数的地址
* Return	 : 0：成功  -1：失败
* Note(s)    :
*********************************************************************
*/
void audio_dac_set_fade_handler(struct audio_dac_hdl *dac, void *priv, void (*fade_handler)(u8, u8));

int audio_dac_get_status(struct audio_dac_hdl *dac);

u8 audio_dac_is_working(struct audio_dac_hdl *dac);

u8 audio_dac_is_idle();

/*初始化dac read的资源*/
int audio_dac_read_base_init(struct dac_read_handle *hdl);
/*释放dac read的资源*/
int audio_dac_read_base_exit(struct dac_read_handle *hdl);
/*重置当前dac read读取的参数*/
int audio_dac_read_base_reset(struct dac_read_handle *hdl);
/* 读取dac播放完的数据，回音消除参考数据使用
 * hdl :            参数句柄
 * points_offset :  起始读取位置偏移
 * data :           读取数据buf
 * len :            读取数据单声道的长度，单位byte
 * read_channel :   读取声道, 1:读单声道/左声道数据，2:读立体声数据
 * autocorrection : 用于设置是否判断dac读取节奏是否正确或者dac数据数据量是否满足读取需求
 * */
int audio_dac_read_base(struct dac_read_handle *hdl, s16 points_offset, void *data, int len, u8 read_channel, u8 autocorrection);
/*重置所有dac read的读取参数*/
int audio_dac_read_reset_all();

/*初始化dac read的资源*/
int audio_dac_read_init(void);
/*释放dac read的资源*/
int audio_dac_read_exit(void);
/*重置当前dac read读取的参数*/
int audio_dac_read_reset(void);
/* 读取dac播放完的数据，回音消除参考数据使用
 * points_offset :  起始读取位置偏移
 * data :           读取数据buf
 * len :            读取数据单声道的长度，单位byte
 * read_channel :   读取声道, 1:读单声道/左声道数据，2:读立体声数据
 * */
int audio_dac_read(s16 points_offset, void *data, int len, u8 read_channel);

int audio_dac_set_protect_time(struct audio_dac_hdl *dac, int time);

int audio_dac_buffered_frames(struct audio_dac_hdl *dac);

void audio_dac_remove_syncts_handle(struct audio_dac_channel *ch, void *syncts);

int audio_dac_add_syncts_with_timestamp(struct audio_dac_channel *ch, void *syncts, u32 timestamp);
void audio_dac_syncts_trigger_with_timestamp(struct audio_dac_channel *ch, u32 timestamp);
void audio_dac_force_use_syncts_frames(struct audio_dac_channel *ch, int frames, u32 timestamp);






int audio_dac_new_channel(struct audio_dac_hdl *dac, struct audio_dac_channel *ch);

int audio_dac_channel_write(struct audio_dac_channel *ch, void *buf, int len);

int audio_dac_channel_fill_slience_frames(struct audio_dac_channel *ch, int frames);

int audio_dac_channel_set_attr(struct audio_dac_channel *ch, struct audio_dac_channel_attr *attr);

int audio_dac_channel_data_len(struct audio_dac_channel *ch);

void audio_dac_channel_start(struct audio_dac_channel *ch);

void audio_dac_channel_close(struct audio_dac_channel *ch);


void audio_dac_add_update_frame_handler(struct audio_dac_hdl *dac,
                                        void (*update_frame_handler)(u8, void *, u32));
void audio_dac_del_update_frame_handler(struct audio_dac_hdl *dac);




int audio_dac_noisefloor_optimize_onoff(u8 onoff);



void audio_dac_anc_set(struct audio_dac_hdl *dac, u8 toggle);
void audio_anc_dac_dsm_sel(u8 sel);
void audio_anc_dac_open(u8 gain_l, u8 gain_r);
void audio_anc_dac_close(void);



// weak函数
int dac_analog_open_cb(struct audio_dac_hdl *);
int dac_analog_close_cb(struct audio_dac_hdl *);
int dac_analog_light_open_cb(struct audio_dac_hdl *);
int dac_analog_light_close_cb(struct audio_dac_hdl *);


#if AUDIO_DAC_IO_ENABLE
void audio_dac_io_init(struct audio_dac_io_param *param);
void audio_dac_io_uninit(struct audio_dac_io_param *param);

/*
 * ch：通道
 *  - 四声道系列芯片(JL703N):
 *      可配 “BIT(0)、BIT(1)、BIT(2)、BIT(3)” 对应 “FL FR RL RR”
 *      多通道时使用或配置：channel = BIT(0) | BIT(1) | BIT(2) | BIT(3);
 *  - 双声道系列芯片(JL701N/JL709N...):
 *      初始化时使能单左/单右声道："ch = BIT(0)"
 *      初始化时使能立体声：
 *          左声道"ch = BIT(0)"
 *          右声道"ch = BIT(1)"
 *          左右声道"ch = BIT(0) | BIT(1)"
 * val：电平
 *    高电平 val = 1
 *    低电平 val = 0
 */
void audio_dac_io_set(u8 ch, u8 val);
#endif

/*MIC Capless API*/
void audio_dac_set_capless_DTB(struct audio_dac_hdl *dac, u32 dacr32);
int wait_for_dac_stop(struct audio_dac_hdl *dac);

void dac_try_power_on_thread();

#endif

