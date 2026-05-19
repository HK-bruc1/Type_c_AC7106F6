#ifndef AUDIO_DECODER_H
#define AUDIO_DECODER_H

#include "generic/typedef.h"
#include "media/audio_base.h"
#include "media_memory.h"




enum {
    AUDIO_IOCTRL_CMD_SET_BREAKPOINT_A = 0x08,	// 设置复读A点
    AUDIO_IOCTRL_CMD_SET_BREAKPOINT_B,			// 设置复读B点
    AUDIO_IOCTRL_CMD_SET_BREAKPOINT_MODE,		// 设置AB点复读模式

    AUDIO_IOCTRL_CMD_REPEAT_PLAY = 0x90,		// 设置循环播放
    AUDIO_IOCTRL_CMD_SET_DEST_PLAYPOS = 0x93,	// 设置指定位置播放
    AUDIO_IOCTRL_CMD_GET_PLAYPOS = 0x94,		// 获取毫秒级时间

    //固件自定义命令(非解码器命令)
    AUDIO_IOCTRL_CMD_START_SILENCT_DROP = 0x1000,//开头静音数据丢弃处理
};

/*
 * 指定位置播放
 * 设置后跳到start_time开始播放，
 * 播放到dest_time后如果callback_func存在，则调用callback_func，
 * 可以在callback_func回调中实现对应需要的动作
 */
struct audio_dest_time_play_param {
    u32 start_time;	// 要跳转过去播放的起始时间。单位：ms
    u32 dest_time;	// 要跳转过去播放的目标时间。单位：ms
    u32(*callback_func)(void *priv);	// 到达目标时间后回调
    void *callback_priv;	// 回调参数
};

/*
 * ab点复读模式结构体
 */
struct audio_ab_repeat_mode_param {
    u32 value;
};

struct fixphase_repair_obj {
    short fifo_buf[18 + 12][32][2];
};

struct audio_repeat_mode_param {
    int flag;
    int headcut_frame;
    int tailcut_frame;
    int (*repeat_callback)(void *);
    void *callback_priv;
    struct fixphase_repair_obj *repair_buf;
};

struct audio_dec_breakpoint {
    int len;
    u32 fptr;
    int data_len;
    u8 data[0];
    // u8 data[128];
};


#define  MEDIA_FRAMEID_NUM     6

// const u8 media_frame_id_list[MEDIA_FRAMEID_NUM][4]=
//{
//	"TIT2",	// 标题    frame_id_att[0] & BIT(0)
//	"TPE1",	// 作者    frame_id_att[0] & BIT(1)
//	"TALB",	// 专辑
//	"TYER",	// 年代
//	"TCON",	// 类型
//	"COMM",	// 备注
//};
struct media_info_set {
    void *ptr;           // 设置media_info buffer.      len_list + maindata
    int max_len;         // buffer_size.
    unsigned char n_items;         // MEDIA_FRAMEID_NUM 可扩展.
    unsigned char item_limit_len;  // 每个条目限制长度(字节)
    unsigned char frame_id_att[1];    //对应n_items  media_frame_id_list是否需要解析  1比特表示   0_不解析,1_解析.
};

struct media_info_data {
    unsigned char mode;        //0:att..bit_set__ReturnValue      1:free. key+value.       2:需要外部解析标准ID3_v2(返回文件偏移地址存在len_list[0~3]_u32)
    unsigned char *len_list;   //n_items    len_list = ptr.       len_list[0~(MEDIA_FRAMEID_NUM-1)] = {default_0}   非0表示对应字段长度.
    unsigned char *mptr[MEDIA_FRAMEID_NUM];  //maindata_ptr[MEDIA_FRAMEID_NUM]
};

struct id3_info {
    struct media_info_set  set;
    struct media_info_data data;
};
#endif

