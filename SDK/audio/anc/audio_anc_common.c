#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".audio_anc_common.data.bss")
#pragma data_seg(".audio_anc_common.data")
#pragma const_seg(".audio_anc_common.text.const")
#pragma code_seg(".audio_anc_common.text")
#endif

/*
 ****************************************************************
 *							AUDIO ANC COMMON
 * File  : audio_anc_comon_plug.c
 * By    :
 * Notes : 存放ANC共用流程
 *
 ****************************************************************
 */

#include "app_config.h"

#if TCFG_AUDIO_ANC_ENABLE
#include "audio_anc_includes.h"
#include "fs/resfile.h"

#include "esco_player.h"
#include "adc_file.h"
#include "bt_tws.h"

#if 1
#define anc_log	printf
#else
#define anc_log (...)
#endif


struct anc_common_t {
    u8 production_mode;
};

static struct anc_common_t common_hdl;
#define __this  (&common_hdl)

//ANC进入产测模式
int audio_anc_production_enter(void)
{
    if (__this->production_mode == 0) {
        anc_log("ANC in Production mode enter\n");
        __this->production_mode = 1;
        //挂起产测互斥功能

        //其他系列没有此接口，暂时保留
        /* audio_anc_drc_toggle_set(0); */
#if ANC_EAR_ADAPTIVE_EN
        //耳道自适应将ANC参数修改为默认参数
        if (audio_anc_coeff_mode_get()) {
            audio_anc_coeff_adaptive_set(0, 0);
        }
#endif

#if ANC_ADAPTIVE_EN
        //关闭场景自适应功能
        audio_anc_power_adaptive_suspend();
#endif

#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
        //关闭风噪检测、智能免摘、广域点击
        audio_icsd_adt_scene_set(ADT_SCENE_PRODUCTION, 1);
        if (audio_icsd_adt_is_running()) {
            audio_icsd_adt_reset(ADT_SCENE_PRODUCTION);
        }
#endif /*TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN*/

#if ANC_MUSIC_DYNAMIC_GAIN_EN
        //关闭ANC防破音
        audio_anc_music_dynamic_gain_suspend();
#endif
        //关闭啸叫抑制、DRC、自适应DCC等功能；
    }
    return 0;
}

//ANC退出产测模式
int audio_anc_production_exit(void)
{
    if (__this->production_mode == 1) {
        anc_log("ANC in Production mode exit\n");
        __this->production_mode = 0;
        //恢复产测互斥功能
    }
    return 0;
}

int audio_anc_production_mode_get(void)
{
    return __this->production_mode;
}

/*
	检查通话和ANC复用MIC 模拟增益是否一致
	param:is_phone_caller		0 非通话调用，1 通话调用
	note:如外部使用ADC模拟开关，可能导致MIC序号匹配错误，需屏蔽检查，人工对齐增益
*/
int audio_anc_mic_gain_check(u8 is_phone_caller)
{
    /* return 0;	//屏蔽检查 */

    struct adc_file_cfg *cfg;
    u8 anc_gain, phone_gain;
    //当前处于ANC_ON 且在通话中
    if (anc_mode_get() != ANC_OFF && (esco_player_runing() || is_phone_caller)) {
        cfg = audio_adc_file_get_cfg();
        if (cfg == NULL) {
            return 0;
        }
        for (int i = 0; i < AUDIO_ADC_MAX_NUM; i++) {
            if ((cfg->mic_en_map & BIT(i)) && audio_anc_mic_en_get(i)) {
                phone_gain = audio_adc_file_get_gain(i);
                anc_gain = audio_anc_mic_gain_get(i);
                ASSERT(phone_gain == anc_gain, "ERR! [mic%d_gain], esco %d != anc %d, please check MIC gain in the anc_gains.bin and the stream.bin \n", \
                       i, phone_gain, anc_gain);
            }
        }
    }
    return 0;
}

#if TCFG_USER_TWS_ENABLE

/*ANC模式同步(tws模式)*/
int anc_mode_sync(struct anc_tws_sync_info *info)
{
    anc_user_mode_set(info->user_anc_mode);
#if ANC_EAR_ADAPTIVE_EN
    anc_ear_adaptive_seq_set(info->ear_adaptive_seq);
#endif/*ANC_EAR_ADAPTIVE_EN*/

#if ANC_MULT_ORDER_ENABLE
    audio_anc_mult_scene_id_sync(info->multi_scene_id);
#endif/*ANC_MULT_ORDER_ENABLE*/
    if (info->anc_mode == anc_mode_get()) {
        return -1;
    }
    os_taskq_post_msg("anc", 2, ANC_MSG_MODE_SYNC, info->anc_mode);
    return 0;
}

#define TWS_FUNC_ID_ANC_SYNC    TWS_FUNC_ID('A', 'N', 'C', 'S')
static void bt_tws_anc_sync(void *_data, u16 len, bool rx)
{
    if (rx) {
        struct anc_tws_sync_info *info = (struct anc_tws_sync_info *)_data;
        anc_log("[slave]anc_sync\n");
        put_buf(_data, len);
        /*先同步adt的状态，然后在切anc里面跑同步adt的动作*/
#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
        audio_anc_icsd_adt_state_sync(info);
#else
        anc_mode_sync(info);
#endif /*TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN*/
    }
}

REGISTER_TWS_FUNC_STUB(app_anc_sync_stub) = {
    .func_id = TWS_FUNC_ID_ANC_SYNC,
    .func    = bt_tws_anc_sync,
};

//TWS 回连ANC信息同步发送API
void bt_tws_sync_anc(void)
{
    struct anc_tws_sync_info info;
    info.user_anc_mode = anc_user_mode_get();
    info.anc_mode = anc_mode_get();
#if ANC_EAR_ADAPTIVE_EN
    info.ear_adaptive_seq = anc_ear_adaptive_seq_get();
#endif/*ANC_EAR_ADAPTIVE_EN*/
#if ANC_MULT_ORDER_ENABLE
    info.multi_scene_id = audio_anc_mult_scene_get();
#endif/*ANC_MULT_ORDER_ENABLE*/
#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
#if TCFG_AUDIO_SPEAK_TO_CHAT_ENABLE
    info.vdt_state = audio_speak_to_chat_is_trigger();
#endif
    info.app_adt_mode = icsd_adt_app_mode_get();
#endif /*TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN*/
    anc_log("[master]bt_tws_sync_anc\n");
    put_buf((u8 *)&info, sizeof(struct anc_tws_sync_info));
    tws_api_send_data_to_slave(&info, sizeof(struct anc_tws_sync_info), TWS_FUNC_ID_ANC_SYNC);
}
#endif

//ANC配置文件 使用RES文件区 覆盖ANC_IF预留区的功能
#if ANC_COEFF_OTA_UPDATE_ENABLE

#define ANC_EXT_HEAD_LEN		20
#define CFG_ANC_CFG_GAINS_FILE		FLASH_RES_PATH"anc_gains.bin"
#define CFG_ANC_CFG_COEFF_FILE		FLASH_RES_PATH"anc_coeff.bin"

#define GROUP_TYPE_ANC_CFG_GAIN		0x9881
#define GROUP_TYPE_ANC_CFG_COEFF	0x98C1

//anc_ext.bin 文件数据头
struct anc_cfg_rsfile_head {
    u32 total_len;	//4 后面所有数据加起来长度
    u16 group_crc;	//6 group_type开始做的CRC，更新数据后需要对应更新CRC
    u16 group_type;	//8
    u16 group_len;  //10 header开始到末尾的长度
    char header[10];//20
};

//获取目标文件和长度
u8 *anc_config_rsfile_get(u8 id, u32 *file_len)
{
    void *fp = NULL;
    int ret = 0;
    u8 *file;
    struct anc_cfg_rsfile_head *f_head;
    u16 target_type;
    if (id == ANC_DB_COEFF) {
        fp = resfile_open(CFG_ANC_CFG_COEFF_FILE);
        target_type = GROUP_TYPE_ANC_CFG_COEFF;
    } else {
        fp = resfile_open(CFG_ANC_CFG_GAINS_FILE);
        target_type = GROUP_TYPE_ANC_CFG_GAIN;
    }

    if (!fp) {
        anc_log("[anc_cfg.bin, id = %d], read_faild\n", id);
        return NULL;
    }
    struct resfile_attrs attr;
    resfile_get_attrs(fp, &attr);
    /* anc_log("faddr:%x,fsize:%d\n", attr.sclust, attr.fsize); */
    if (attr.sclust % 4 != 0) {
        /* ASSERT(0, "ERR!!ANC EXT NO ALIGN 4 BYTE\n"); */
        anc_log("ERR!!ANC CFG NO ALIGN 4 BYTE\n");
        ret = 1;
        goto __exit;
    }
    file = (u8 *)attr.sclust;
    /* put_buf(file, attr.fsize); */

    f_head = (struct anc_cfg_rsfile_head *)file;

    u32 check_total_len = f_head->total_len;
    if ((check_total_len == 0xFFFFFFFF) || \
        (check_total_len == 0) || \
        (f_head->group_type != target_type) || \
        (f_head->group_len == 10)) {
        anc_log("f_head head err,total_len:%u,group_type:%x,group_len:%u\n", check_total_len, f_head->group_type, f_head->group_len);
        ret = 1;
        goto __exit;
    }

    /* anc_log("total_len:0x%x=%u ", f_head->total_len, f_head->total_len); */
    /* anc_log("group_crc:0x%x ", f_head->group_crc); */
    /* anc_log("group_type:0x%x ", f_head->group_type); */
    /* anc_log("group_len:0x%x=%d ", f_head->group_len, f_head->group_len); */
    /* anc_log("tag:%s\n", f_head->header); */

    *file_len = attr.fsize - ANC_EXT_HEAD_LEN;

__exit:
    if (ret) {
        anc_log("anc_ext file get failed\n");
        return NULL;
    }
    anc_log("f_head get succ\n");
    resfile_close(fp);
    return (file + ANC_EXT_HEAD_LEN);
}

/*
    对比 资源文件 与 ANC_IF区域 的ANC配置文件，若差异，以资源文件覆盖ANC区域
    查询耗时：2.3ms;  查询并覆盖耗时：41.8ms
*/
static int anc_config_rsfile_read(void *p)
{
    u32 rs_gain_len, rs_coeff_len;
    int ret = 0;
    u8 *rs_gain = NULL;
    u8 *rs_coeff = NULL;
    u16 rs_crc, db_crc;
    u8 change_flag = 0;
    if (!p) {
        return -1;
    }
    audio_anc_t *param = (audio_anc_t *)p;

    u32 last_jiff = jiffies_usec();
    /*对比ANC增益系数*/
    rs_gain = anc_config_rsfile_get(ANC_DB_GAIN, &rs_gain_len);
    if (rs_gain) {
        rs_crc = CRC16(rs_gain, rs_gain_len);
        anc_gain_t *db_gain = (anc_gain_t *)anc_db_get(ANC_DB_GAIN, &param->gains_size);
        if (db_gain) {
            db_crc = CRC16(db_gain, param->gains_size);
            if (rs_crc != db_crc) {
                change_flag = 1;
                // anc_log("-----------anc db gains------------");
                // put_buf(db_gain, param->gains_size);
                // anc_log("-----------anc rsfile gains------------");
                // put_buf(rs_gain, rs_gain_len);
                anc_log("anc rsfile gains diff, len %d->%d\n", param->gains_size, rs_gain_len);
            }
        }
    } else {
        anc_log("anc rsfile gains read empty!\n");
    }

    /*对比ANC滤波器系数*/
    rs_coeff = anc_config_rsfile_get(ANC_DB_COEFF, &rs_coeff_len);
    if (rs_coeff) {
        rs_crc = CRC16(rs_coeff, rs_coeff_len);
        anc_coeff_t *db_coeff = (anc_coeff_t *)anc_db_get(ANC_DB_COEFF, &param->coeff_size);
        if (db_coeff) {
            db_crc = CRC16(db_coeff, param->coeff_size);
            if (rs_crc != db_crc) {
                change_flag = 1;
                // anc_log("-----------anc db coeff------------");
                // put_buf(db_coeff, param->coeff_size);
                // anc_log("-----------anc rsfile coeff------------");
                // put_buf(rs_coeff, rs_coeff_len);
                anc_log("anc rsfile coeff diff, len %d->%d\n", param->coeff_size, rs_coeff_len);
            }
        }
    } else {
        anc_log("anc rsfile coeff read empty!\n");
    }

    if (change_flag) {
        param->write_coeff_size = rs_coeff_len;
        ret = anc_db_put(param, (anc_gain_t *)rs_gain, (anc_coeff_t *)rs_coeff);
        if (!ret) {
            anc_log("anc rsfile change succ\n");
        } else {
            anc_log("anc rsfile change fail\n");
        }
    }

    anc_log("anc rsfile check time : %d\n", jiffies_usec2offset(last_jiff, jiffies_usec()));

    return ret;
}
#endif

#endif
