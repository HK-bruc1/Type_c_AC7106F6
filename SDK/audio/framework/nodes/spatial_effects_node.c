#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".spatial_effects_node.data.bss")
#pragma data_seg(".spatial_effects_node.data")
#pragma const_seg(".spatial_effects_node.text.const")
#pragma code_seg(".spatial_effects_node.text")
#endif
#include "app_config.h"
#if (TCFG_AUDIO_SPATIAL_EFFECT_ENABLE)
#include "jlstream.h"
#include "media/audio_base.h"
#include "circular_buf.h"
#include "cvp_node.h"
#include "spatial_effects_process.h"
#include "spatial_effect.h"
#include "classic/tws_api.h"


struct ANGLE_CONIFG {
    float track_sensitivity;
    float angle_reset_sensitivity;
};

struct RP_CONIFG {
    int trackKIND;    //角度合成算法用哪一种 :0或者1
    int ReverbKIND;   //2或者3
    int reverbance;   //湿声比例 : 0~100
    int dampingval;   //高频decay  ：0~80
};

struct SPATIAL_CONIFG {
    float radius;//半径
    int bias_angle;//偏角
};

struct SPATIAL_CONFIG_IMP {
    float radius;
    int bias_angle;
    float attenuation;
    int room_size;
    int stereo_width;
    int outGain;
    int lowcutoff;
    int early_taps;
    int preset0_room;
    int preset0_roomgain;
    int azimuth_angle;
    int elevation_angle;
    int delay_time;
    int ildenable;
    int rev_mode;
    int delaybuf_max_ms;
    float rev1_gain0;
    float rev1_gain1;
};

struct SPATIAL_EFFECT_CONFIG {
    int bypass;//是否跳过节点处理
    struct ANGLE_CONIFG     angle;
    struct RP_CONIFG        v1_parm_reverb;
    struct SPATIAL_CONIFG   v2_parm_spatial;
    struct SPATIAL_CONFIG_IMP v3_parm_spatial;
};

struct spatial_effects_node_hdl {
    int sample_rate;
    struct SPATIAL_EFFECT_CONFIG effect_cfg;
    struct node_port_data_wide data_wide;
    spatial_effect_cfg_t online_cfg;
    struct stream_frame *pack_frame;
    u8 *remain_buf;
    u16 remain_len;
    u16 frame_len;
    u8 out_channel;
    u8 tmp_out_channel;
    u8 bypass;//是否跳过节点处理
};
static struct spatial_effects_node_hdl *spatial_effects_hdl = NULL;

extern const int CONFIG_BTCTLER_TWS_ENABLE;

void spatial_effect_node_param_cfg_updata(struct SPATIAL_EFFECT_CONFIG *effect_cfg, spatial_effect_cfg_t *param)
{
    printf("spatial_effect_node_param_cfg_updata");
    struct spatial_effects_node_hdl *hdl = spatial_effects_hdl;

    if (!effect_cfg) {
        return;
    }

    if (param) {
        param->angle.track_sensitivity = effect_cfg->angle.track_sensitivity;
        param->angle.angle_reset_sensitivity = effect_cfg->angle.angle_reset_sensitivity;
        if (CONFIG_SPATIAL_EFFECT_VERSION == SPATIAL_EFFECT_V3) {
            /*
             * 不同模式头部与声源的相对运动：
             * 跟踪模式：头部动声源不动
             * 固定模式：声源动头部不动
             * 此处固定模式参数更新时，需要做"360-angle"操作，与跟踪模式对齐
             */
            param->si.sp.Azimuth_angle = 360 - effect_cfg->v3_parm_spatial.azimuth_angle;
            param->si.sp.Elevation_angle = 360 - effect_cfg->v3_parm_spatial.elevation_angle;
            param->si.sp.radius = effect_cfg->v3_parm_spatial.radius;
            param->si.sp.bias_angle = effect_cfg->v3_parm_spatial.bias_angle;
            param->si.sp.attenuation = effect_cfg->v3_parm_spatial.attenuation;
            param->si.hp.delay_time = effect_cfg->v3_parm_spatial.delay_time;
            param->si.hp.ildenable = effect_cfg->v3_parm_spatial.ildenable;
            param->si.hp.rev_mode = effect_cfg->v3_parm_spatial.rev_mode;
            param->si.es.delaybuf_max_ms = effect_cfg->v3_parm_spatial.delaybuf_max_ms;
            param->si.es.Erfactor = effect_cfg->v3_parm_spatial.room_size;
            param->si.es.Ewidth = effect_cfg->v3_parm_spatial.stereo_width;
            param->si.es.outGain = effect_cfg->v3_parm_spatial.outGain;
            param->si.es.lowcutoff = effect_cfg->v3_parm_spatial.lowcutoff;
            param->si.es.early_taps = effect_cfg->v3_parm_spatial.early_taps;
            param->si.es.preset0_room = effect_cfg->v3_parm_spatial.preset0_room;
            param->si.es.preset0_roomgain = effect_cfg->v3_parm_spatial.preset0_roomgain;
            param->si.rs.rev1_gain0 = effect_cfg->v3_parm_spatial.rev1_gain0;
            param->si.rs.rev1_gain1 = effect_cfg->v3_parm_spatial.rev1_gain1;
            if (hdl) {
                param->si.adt.IndataBit = hdl->data_wide.iport_data_wide;
                param->si.adt.OutdataBit = hdl->data_wide.oport_data_wide;
                param->si.adt.IndataInc = AUDIO_CH_NUM(hdl_node(hdl)->iport->prev->fmt.channel_mode);
                param->si.adt.OutdataInc = AUDIO_CH_NUM(hdl_node(hdl)->oport->fmt.channel_mode);
                param->si.adt.Qval = hdl_node(hdl)->iport->prev->fmt.Qval;
                param->si.sc.sampleRate = hdl->sample_rate;
                param->si.sc.switch_channel = 0;
                if (param->si.adt.OutdataInc == 1) { //单声道
                    if (CONFIG_BTCTLER_TWS_ENABLE) {
                        if (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED) {
                            param->si.sc.switch_channel = (tws_api_get_local_channel() == 'L') ? 0 : 1;
                        }
                    }
                } else { //双声道
                    param->si.sc.switch_channel = 2;
                }
                param->si.es.sr = hdl->sample_rate;
            }
        } else if (CONFIG_SPATIAL_EFFECT_VERSION == SPATIAL_EFFECT_V2) {
            /*V2.0.0版本音效算法参数*/
            param->sag.radius = effect_cfg->v2_parm_spatial.radius;
            param->sag.bias_angle = effect_cfg->v2_parm_spatial.bias_angle;
            if (hdl) {
                param->scfi.sampleRate = hdl->sample_rate;
                param->pcm_info.IndataBit = hdl->data_wide.iport_data_wide;
                param->pcm_info.OutdataBit = hdl->data_wide.oport_data_wide;
                param->pcm_info.Qval = hdl_node(hdl)->iport->prev->fmt.Qval;
            }
        } else {
            /*V1.0.0版本音效算法参数*/
            param->rp_parm.trackKIND = effect_cfg->v1_parm_reverb.trackKIND;
            param->rp_parm.ReverbKIND = effect_cfg->v1_parm_reverb.ReverbKIND;
            param->rp_parm.reverbance = effect_cfg->v1_parm_reverb.reverbance;
            param->rp_parm.dampingval = effect_cfg->v1_parm_reverb.dampingval;
        }
    }

    if (hdl) {
        hdl->bypass = effect_cfg->bypass;
        printf("spatial node bypass %d", hdl->bypass);
    }
}

int spatial_effects_node_param_cfg_read(void *param, int size)
{
    int len = 0;
    struct spatial_effects_node_hdl *hdl = spatial_effects_hdl;

    /*
     *获取配置文件内的参数,及名字
     * */
    char name[16];
    len = jlstream_read_node_data_new(hdl_node(hdl)->uuid, hdl_node(hdl)->subid, (void *)(&hdl->effect_cfg), name);
    if (len != sizeof(hdl->effect_cfg)) {
        printf("%s, read node data err %d != %d\n", __FUNCTION__, len, (int)sizeof(hdl->effect_cfg));
        return 0 ;
    }
    printf("%s, read node cfg succ : %d \n", __FUNCTION__, len);

    /*
     *获取在线调试的临时参数
     * */
    if (config_audio_cfg_online_enable) {
        if (hdl) {
            if (jlstream_read_effects_online_param(hdl_node(hdl)->uuid, name, (void *)(&hdl->effect_cfg), len)) {
                printf("get spatial effects online param\n");
            }
        }
    }

    spatial_effect_node_param_cfg_updata(&hdl->effect_cfg, (spatial_effect_cfg_t *)param);

    return sizeof(spatial_effect_cfg_t);
}

u8 get_spatial_effects_node_out_channel(void)
{
    struct spatial_effects_node_hdl *hdl = spatial_effects_hdl;
    if (hdl) {
        /* printf("out _channel %x", hdl->out_channel); */
        return hdl->out_channel;
    }
    return 0;
}

u8 get_spatial_effect_node_bit_width()
{
    struct spatial_effects_node_hdl *hdl = spatial_effects_hdl;
    if (hdl) {
        return hdl->data_wide.iport_data_wide;
    }
    return 0;
}

u32 get_spatial_effect_node_sample_rate()
{
    struct spatial_effects_node_hdl *hdl = spatial_effects_hdl;
    if (hdl) {
        return hdl->sample_rate;
    }
    return 0;
}

u8 get_spatial_effect_node_bypass()
{
    struct spatial_effects_node_hdl *hdl = spatial_effects_hdl;
    if (hdl) {
        return hdl->bypass;
    }
    return 0;
}

u8 spatial_effect_node_is_running()
{
    return (spatial_effects_hdl ? 1 : 0);
}

/*节点输出回调处理，可处理数据或post信号量*/
static void spatial_effects_handle_frame(struct stream_iport *iport, struct stream_note *note)
{
    struct stream_node *node = iport->node;
    struct spatial_effects_node_hdl *hdl = (struct spatial_effects_node_hdl *)iport->node->private_data;
    s16 *dat;
    int len;
    struct stream_frame *in_frame;
    if (hdl == NULL) {
        return;
    }
    in_frame = jlstream_pull_frame(iport, note);		//从iport读取数据
    if (!in_frame) {
        return;
    }
    //v3版本，固定点数
    if (CONFIG_SPATIAL_EFFECT_VERSION == SPATIAL_EFFECT_V3) {
        /*算法出来一帧的数据长度，byte*/
        hdl->remain_len += in_frame->len;  //记录目前还有多少数据
        if (audio_spatial_effects_frame_pack_disable()) {  //不跑算法，不需要拼帧，防止插入breaker时有残留数据丢失导致po声、左右耳相位差
            //将当前剩余的所有数据推出
            struct stream_frame *frame = jlstream_get_frame(node->oport, hdl->remain_len);
            u8 out_channel = AUDIO_CH_MIX;
            if (AUDIO_CH_NUM(hdl_node(hdl)->oport->fmt.channel_mode) == 1) {
                if (CONFIG_BTCTLER_TWS_ENABLE) {
                    if (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED) {
                        out_channel = (tws_api_get_local_channel() == 'L') ? AUDIO_CH_L : AUDIO_CH_R;
                    }
                }
            } else {
                out_channel = AUDIO_CH_LR;
            }
            u16 tmp_remain = hdl->remain_len - in_frame->len;//上一次剩余的数据大小
            memcpy(frame->data, hdl->remain_buf, tmp_remain);
            memcpy((void *)((int)frame->data + tmp_remain), in_frame->data, in_frame->len);
            audio_spatial_effects_data_handler(out_channel, (s16 *)frame->data, hdl->remain_len);
            frame->len = (AUDIO_CH_NUM(hdl_node(hdl)->oport->fmt.channel_mode) == 1) ? (hdl->remain_len >> 1) : hdl->remain_len; //若是2变1，需要将长度除二
            hdl->remain_len = 0;
            jlstream_push_frame(node->oport, frame);	//将数据推到oport
            jlstream_free_frame(in_frame);	//释放iport资源
            return;
        }
        u8 pack_frame_num = hdl->remain_len / hdl->frame_len;//每次数据需要跑多少帧
        u16 pack_frame_len = pack_frame_num * hdl->frame_len;       //记录本次需要跑多少数据
        u16 offset = 0;
        if (pack_frame_num) {
            if (!hdl->pack_frame) {
                /*申请资源存储输出*/
                hdl->pack_frame = jlstream_get_frame(node->oport, pack_frame_len);
                if (!hdl->pack_frame) {
                    return;
                }
            }
            u16 tmp_remain = hdl->remain_len - in_frame->len;//上一次剩余的数据大小
            /*拷贝上一次剩余的数据*/
            memcpy(hdl->pack_frame->data, hdl->remain_buf, tmp_remain);
            /*拷贝本次数据*/
            memcpy((void *)((int)hdl->pack_frame->data + tmp_remain), in_frame->data, pack_frame_len - tmp_remain);
            //128倍数的大帧
            dat = (s16 *)(int)hdl->pack_frame->data;
            len = pack_frame_len;
            u8 out_channel = AUDIO_CH_MIX;
            if (AUDIO_CH_NUM(hdl_node(hdl)->oport->fmt.channel_mode) == 1) {
                if (CONFIG_BTCTLER_TWS_ENABLE) {
                    if (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED) {
                        out_channel = (tws_api_get_local_channel() == 'L') ? AUDIO_CH_L : AUDIO_CH_R;
                    }
                }
            } else {
                out_channel = AUDIO_CH_LR;
            }
            audio_spatial_effects_data_handler(out_channel, dat, len);
            hdl->remain_len -= pack_frame_len;//剩余数据长度
            hdl->pack_frame->len = (AUDIO_CH_NUM(hdl_node(hdl)->oport->fmt.channel_mode) == 1) ? (pack_frame_len >> 1) : pack_frame_len; //若是2变1，需要将长度除二
            jlstream_push_frame(node->oport, hdl->pack_frame);	//将数据推到oport
            hdl->pack_frame = NULL;
            /*保存剩余不够一帧的数据*/
            memcpy(hdl->remain_buf, (void *)((int)in_frame->data + in_frame->len - hdl->remain_len), hdl->remain_len);
            jlstream_free_frame(in_frame);	//释放iport资源
        } else {
            /*当前数据不够跑一帧算法时*/
            memcpy((void *)((int)hdl->remain_buf + hdl->remain_len - in_frame->len), in_frame->data, in_frame->len);
            jlstream_free_frame(in_frame);	//释放iport资源
        }
    } else {
        dat = (s16 *)in_frame->data;
        len = in_frame->len;
#if SPATIAL_AUDIO_EFFECT_OUT_STEREO_EN
        /*节点固定输出立体声*/
        in_frame->len = audio_spatial_effects_data_handler(AUDIO_CH_LR, dat, len);
#else
        in_frame->len = audio_spatial_effects_data_handler(hdl->out_channel, dat, len);
#endif
        jlstream_push_frame(node->oport, in_frame);
    }
}

static int spatial_effects_adapter_bind(struct stream_node *node, u16 uuid)
{
    struct spatial_effects_node_hdl *hdl = (struct spatial_effects_node_hdl *)node->private_data;

    spatial_effects_hdl = hdl;
    return 0;
}

/*打开改节点输入接口*/
static void spatial_effects_ioc_open_iport(struct stream_iport *iport)
{
}

/*节点参数协商*/
static int spatial_effects_ioc_negotiate(struct stream_iport *iport)
{
    struct stream_oport *oport = iport->node->oport;
    /* struct stream_fmt *in_fmt = &iport->prev->fmt; */
    struct spatial_effects_node_hdl *hdl = (struct spatial_effects_node_hdl *)iport->node->private_data;

    if (hdl) {
        /*如果已经设置过通道，则用记录的输出通道*/
        if (hdl->tmp_out_channel) {
            hdl->out_channel = hdl->tmp_out_channel;
        } else {
            /*如果没有设置过输出通道，根据下一个节点设置*/
            hdl->out_channel = oport->fmt.channel_mode;
        }
        hdl->sample_rate = oport->fmt.sample_rate;
        printf("%s, out _channel %x", __func__, hdl->out_channel);
    }

    return 0;
}

/*节点start函数*/
static void spatial_effects_ioc_start(struct spatial_effects_node_hdl *hdl)
{
    if (hdl) {
        printf("spatial_effects_ioc_start");
        /*读取节点位宽信息*/
        hdl->data_wide.iport_data_wide = hdl_node(hdl)->iport->prev->fmt.bit_wide;
        hdl->data_wide.oport_data_wide = hdl_node(hdl)->oport->fmt.bit_wide;
        /* jlstream_read_node_port_data_wide(hdl_node(hdl)->uuid, hdl_node(hdl)->subid, (u8 *)&hdl->data_wide);  */
        /*读取bypass参数*/
        spatial_effects_node_param_cfg_read(NULL, 0);
        printf("%s bit_wide, %d %d\n", __FUNCTION__, hdl->data_wide.iport_data_wide, hdl->data_wide.oport_data_wide);

        if (CONFIG_SPATIAL_EFFECT_VERSION == SPATIAL_EFFECT_V3) {
            u8 points_offset = hdl->data_wide.iport_data_wide ? 2 : 1;
            hdl->frame_len = (spatial_imp_run_points * 2) << points_offset; //*2 固定双声道输入
            hdl->remain_buf = zalloc(hdl->frame_len);
        }
        audio_effect_process_start();
    }
}

/*节点stop函数*/
static void spatial_effects_ioc_stop(struct spatial_effects_node_hdl *hdl)
{
    if (hdl) {
        printf("spatial_effects_ioc_stop");
        if (hdl->remain_buf) {
            free(hdl->remain_buf);
            hdl->remain_buf = NULL;
        }
        hdl->remain_len = 0;
        audio_effect_process_stop();
    }
}

static int spatial_effects_ioc_update_parm(struct spatial_effects_node_hdl *hdl, int parm)
{
    int ret = false;
    if (hdl) {
        spatial_effect_node_param_cfg_updata((struct SPATIAL_EFFECT_CONFIG *)parm, &hdl->online_cfg);
        spatial_effect_online_updata((void *)(&hdl->online_cfg));
        ret = true;
    }
    return ret;
}

/*节点ioctl函数*/
static int spatial_effects_adapter_ioctl(struct stream_iport *iport, int cmd, int arg)
{
    int ret = 0;
    struct spatial_effects_node_hdl *hdl = (struct spatial_effects_node_hdl *)iport->node->private_data;

    switch (cmd) {
    case NODE_IOC_OPEN_IPORT:
        spatial_effects_ioc_open_iport(iport);
        break;
    case NODE_IOC_OPEN_OPORT:
        break;
    case NODE_IOC_CLOSE_IPORT:
        break;
    case NODE_IOC_NEGOTIATE:
        *(int *)arg |= spatial_effects_ioc_negotiate(iport);
        break;
    case NODE_IOC_SET_FMT:
        break;
    case NODE_IOC_START:
        spatial_effects_ioc_start(hdl);
        break;
    case NODE_IOC_SUSPEND:
    case NODE_IOC_STOP:
        spatial_effects_ioc_stop(hdl);
        break;
    case NODE_IOC_SET_PARAM:
        ret = spatial_effects_ioc_update_parm(hdl, arg);
        break;
    case NODE_IOC_SET_CHANNEL:
        if (hdl) {
            /*记录输出通道*/
            hdl->tmp_out_channel = (u8)arg;
            /*设置输出通道*/
            hdl->out_channel = hdl->tmp_out_channel;
            printf("spatial effect set out_channel : 0x%x", hdl->out_channel);
        }
        break;
    }

    return ret;
}

/*节点用完释放函数*/
static void spatial_effects_adapter_release(struct stream_node *node)
{
    spatial_effects_hdl = NULL;
}

/*节点adapter 注意需要在sdk_used_list声明，否则会被优化*/
REGISTER_STREAM_NODE_ADAPTER(spatial_effects_node_adapter) = {
    .uuid       = NODE_UUID_SPATIAL_EFFECTS,
    .bind       = spatial_effects_adapter_bind,
    .ioctl      = spatial_effects_adapter_ioctl,
    .release    = spatial_effects_adapter_release,
    .handle_frame = spatial_effects_handle_frame,
    .hdl_size   = sizeof(struct spatial_effects_node_hdl),
    .ability_channel_in = 2, //输入只支持双声道
    .ability_channel_out =  1 | 2, //输出单声道或者双声道
    .ability_channel_convert = 1, //支持声道转换
};


//注册工具在线调试
REGISTER_ONLINE_ADJUST_TARGET(spatial_effects) = {
    .uuid       = NODE_UUID_SPATIAL_EFFECTS,
};
#endif
