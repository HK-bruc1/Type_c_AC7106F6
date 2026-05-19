#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".icsd_anc.data.bss")
#pragma data_seg(".icsd_anc.data")
#pragma const_seg(".icsd_anc.text.const")
#pragma code_seg(".icsd_anc.text")
#endif
#include "app_config.h"
#include "audio_config_def.h"
#if (TCFG_AUDIO_ANC_EAR_ADAPTIVE_EN && \
	 TCFG_AUDIO_ANC_ENABLE && \
	 TCFG_AUDIO_ANC_EAR_ADAPTIVE_VERSION == ANC_EXT_V2)

#include "audio_anc_includes.h"
#if TCFG_USER_TWS_ENABLE
#include "classic/tws_api.h"
#endif
#include "board_config.h"
#include "tone_player.h"
#include "adv_adaptive_noise_reduction.h"
#include "audio_src.h"
#include "clock_manager/clock_manager.h"

u8 const ICSD_SPECIFIC_EN    = 0;
u8 const ICSD_ANC_TOOL_DEBUG = 0;
OS_SEM icsd_anc_sem;


const u8 EAR_ADAPTIVE_MODE_SIGN_TRIM_VEL =  EAR_ADAPTIVE_MODE_SIGN_TRIM;
struct anc_function	ANC_FUNC;
__icsd_anc_REG *ICSD_REG = NULL;

static void icsd_anc_v2_cmd(u8 cmd);

int (*anc_v2_printf)(const char *format, ...);
int anc_v2_printf_off(const char *format, ...)
{
    return 0;
}
//============================================================================

#if TCFG_USER_TWS_ENABLE

#define TWS_FUNC_ID_SDANCV2_M2S    TWS_FUNC_ID('I', 'C', 'M', 'S')
REGISTER_TWS_FUNC_STUB(icsd_anc_v2_m2s) = {
    .func_id = TWS_FUNC_ID_SDANCV2_M2S,
    .func    = icsd_anc_v2_m2s_cb,
};
#define TWS_FUNC_ID_SDANCV2_S2M    TWS_FUNC_ID('I', 'C', 'S', 'M')
REGISTER_TWS_FUNC_STUB(icsd_anc_v2_s2m) = {
    .func_id = TWS_FUNC_ID_SDANCV2_S2M,
    .func    = icsd_anc_v2_s2m_cb,
};
#define TWS_FUNC_ID_SDANCV2_MSYNC  TWS_FUNC_ID('I', 'C', 'M', 'N')
REGISTER_TWS_FUNC_STUB(icsd_anc_v2_msync) = {
    .func_id = TWS_FUNC_ID_SDANCV2_MSYNC,
    .func    = icsd_anc_v2_msync_cb,
};
#define TWS_FUNC_ID_SDANCV2_SSYNC  TWS_FUNC_ID('I', 'C', 'S', 'N')
REGISTER_TWS_FUNC_STUB(icsd_anc_v2_ssync) = {
    .func_id = TWS_FUNC_ID_SDANCV2_SSYNC,
    .func    = icsd_anc_v2_ssync_cb,
};

void icsd_anc_v2_tws_msync(u8 cmd)
{
    struct icsd_anc_v2_tws_packet packet;
    icsd_anc_v2_msync_packet(&packet, cmd);
    int ret = tws_api_send_data_to_sibling(packet.data, packet.len, TWS_FUNC_ID_SDANCV2_MSYNC);
}

void icsd_anc_v2_tws_ssync(u8 cmd)
{
    struct icsd_anc_v2_tws_packet packet;
    icsd_anc_v2_ssync_packet(&packet, cmd);
    int ret = tws_api_send_data_to_sibling(packet.data, packet.len, TWS_FUNC_ID_SDANCV2_SSYNC);
}

void icsd_anc_v2_tws_m2s(u8 cmd)
{
    s8 data[16];
    icsd_anc_v2_m2s_packet(data, cmd);
    int ret = tws_api_send_data_to_sibling(data, 16, TWS_FUNC_ID_SDANCV2_M2S);
}

void icsd_anc_v2_tws_s2m(u8 cmd)
{
    s8 data[16];
    icsd_anc_v2_s2m_packet(data, cmd);
    int ret = tws_api_send_data_to_sibling(data, 16, TWS_FUNC_ID_SDANCV2_S2M);
}

int icsd_anc_v2_get_tws_state()
{
    if (ICSD_REG->icsd_anc_v2_tws_balance_en) {
        return tws_api_get_tws_state();
    }
    return 0;
}

#else
int icsd_anc_v2_get_tws_state()
{
    return 0;
}

#endif


static void icsd_anc_v2_time_data_init(u8 *buf)
{
    if (ICSD_REG) {
        if (ICSD_REG->anc_time_data == NULL) {
            ICSD_REG->anc_time_data = (__icsd_time_data *)buf;
        }
    }
}

static void icsd_anc_config_data_init(void *config_ptr, struct icsd_anc_v2_infmt *fmt, struct anc_ext_ear_adaptive_param *ext_cfg)
{
    icsd_sd_cfg_set(SD_CFG, ext_cfg);

    if (ext_cfg->time_domain_show_en) {
        icsd_anc_v2_time_data_init(ext_cfg->time_domain_buf);
    }

    SD_CFG->tool_ffgain_sign = fmt->tool_ffgain_sign;
    SD_CFG->tool_fbgain_sign = fmt->tool_fbgain_sign;
}

static void icsd_anc_v2_open(void *_param, struct anc_ext_ear_adaptive_param *ext_cfg)
{
    audio_anc_t *param = (audio_anc_t *)_param;
    anc_v2_printf("icsd_anc_open\n");
    struct icsd_anc_v2_libfmt libfmt;
    icsd_anc_v2_get_libfmt(&libfmt);
    if (ICSD_REG->anc_v2_ram_addr == 0) {
        ICSD_REG->anc_v2_ram_addr = anc_malloc("ICSD_ANC", libfmt.lib_alloc_size);
        printf("anc_v2_ram_addr:%d>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", (int)ICSD_REG->anc_v2_ram_addr);
    }
    struct icsd_anc_v2_infmt infmt = {0};
    infmt.alloc_ptr = ICSD_REG->anc_v2_ram_addr;
    infmt.ff_gain = 3;
    infmt.fb_gain = icsd_anc_v2_readfbgain();
    infmt.tool_ffgain_sign = 1;
    infmt.tool_fbgain_sign = 1;
    anc_adaptive_iir_t *anc_iir = (anc_adaptive_iir_t *)anc_ear_adaptive_iir_get();
#if (TCFG_AUDIO_ANC_CH & ANC_L_CH)
    if (anc_iir->result & ANC_ADAPTIVE_RESULT_LFF) {
        ICSD_REG->lff_fgq_last = (float *)anc_malloc("ICSD_ANC", ((ANC_MAX_ORDER * 3) + 1) * sizeof(float));
        ICSD_REG->lff_fgq_last[0] = anc_iir->lff_gain;
        for (int i = 0; i < ANC_ADAPTIVE_FF_ORDER; i++) {
            memcpy((u8 *)(ICSD_REG->lff_fgq_last + 1 + (i * 3)), (u8 *)(anc_iir->lff[i].a), 12);
        }
        infmt.ff_fgq_l = ICSD_REG->lff_fgq_last;
        infmt.target_out_l = anc_iir->l_target;
    }
#endif
#if AUDIO_ANC_STEREO_ENABLE
    if (anc_iir->result & ANC_ADAPTIVE_RESULT_RFF) {
        ICSD_REG->rff_fgq_last = (float *)anc_malloc("ICSD_ANC", ((ANC_MAX_ORDER * 3) + 1) * sizeof(float));
        for (int i = 0; i < ANC_ADAPTIVE_FF_ORDER; i++) {
            memcpy((u8 *)(ICSD_REG->rff_fgq_last + 1 + (i * 3)), (u8 *)(anc_iir->rff[i].a), 12);
        }
        infmt.ff_fgq_r = ICSD_REG->rff_fgq_last;
        infmt.target_out_r = anc_iir->r_target;
    }
#endif
    if (param->gains.gain_sign & ANCL_FF_SIGN) {
        infmt.tool_ffgain_sign = -1;
    }
    if (param->gains.gain_sign & ANCL_FB_SIGN) {
        infmt.tool_fbgain_sign = -1;
    }
    icsd_anc_v2_set_infmt(&infmt);
    icsd_anc_config_data_init(NULL, &infmt, ext_cfg);
    anc_v2_printf("ram init: %d\n", libfmt.lib_alloc_size);
    ICSD_REG->v2_user_train_state = 0;
    ICSD_REG->icsd_anc_v2_function = 0;
}



void icsd_anc_v2_end(void *_param)
{
    audio_anc_t *param = _param;
    mem_stats();
    if (ICSD_REG) {
        if (ICSD_REG->lff_fgq_last) {
            anc_free(ICSD_REG->lff_fgq_last);
            ICSD_REG->lff_fgq_last = NULL;
        }
        if (ICSD_REG->rff_fgq_last) {
            anc_free(ICSD_REG->rff_fgq_last);
            ICSD_REG->rff_fgq_last = NULL;
        }
        ICSD_REG->v2_user_train_state &= ~ANC_USER_TRAIN_DMA_EN;
        ICSD_REG->icsd_anc_v2_contral = 0;
        anc_v2_printf("ICSD ANC V2 END train time finish:%dms\n", (int)((jiffies - ICSD_REG->train_time_v2) * 10));
    }
    anc_user_train_cb(ANC_ON,  0);
}

void icsd_anc_v2_forced_exit()
{
    if (ICSD_REG) {
        if (!(ICSD_REG->icsd_anc_v2_contral & ICSD_ANC_FORCED_EXIT)) {
            anc_v2_printf("icsd_anc_forced_exit\n");
            DeAlorithm_disable();
            if (ICSD_REG->icsd_anc_v2_contral & ICSD_ANC_INITED) {
                ICSD_REG->icsd_anc_v2_contral |= ICSD_ANC_FORCED_EXIT;
                ANC_FUNC.icsd_anc_v2_cmd(ANC_V2_CMD_FORCED_EXIT);
            } else {
                ICSD_REG->icsd_anc_v2_contral |= ICSD_ANC_FORCED_BEFORE_INIT;
            }
        }
    }
}

u16 icsd_anc_timeout_add(void *priv, void (*func)(void *priv), u32 msec)
{
    return sys_hi_timeout_add(priv, func, msec);
}

static void anc_function_init()
{
    printf("anc_function_init\n");
    ANC_FUNC.anc_dma_done_ppflag = anc_dma_done_ppflag;
    ANC_FUNC.local_irq_disable = local_irq_disable;
    ANC_FUNC.local_irq_enable = local_irq_enable;
    ANC_FUNC.jiffies_usec = jiffies_usec;
    ANC_FUNC.jiffies_usec2offset = jiffies_usec2offset;
    ANC_FUNC.audio_anc_debug_send_data = audio_anc_debug_send_data;
    ANC_FUNC.audio_anc_debug_busy_get = audio_anc_debug_busy_get;

    ANC_FUNC.icsd_anc_v2_get_tws_state = icsd_anc_v2_get_tws_state;
#if TCFG_USER_TWS_ENABLE
    ANC_FUNC.tws_api_get_role = tws_api_get_role;
    ANC_FUNC.icsd_anc_v2_tws_m2s = icsd_anc_v2_tws_m2s;
    ANC_FUNC.icsd_anc_v2_tws_s2m = icsd_anc_v2_tws_s2m;
    ANC_FUNC.icsd_anc_v2_tws_msync = icsd_anc_v2_tws_msync;
    ANC_FUNC.icsd_anc_v2_tws_ssync = icsd_anc_v2_tws_ssync;
#endif

    ANC_FUNC.icsd_anc_v2_cmd = icsd_anc_v2_cmd;
    ANC_FUNC.icsd_anc_timeout_add = icsd_anc_timeout_add;
    ANC_FUNC.icsd_anc_v2_dma_2ch_on = icsd_anc_v2_dma_2ch_on;
    ANC_FUNC.icsd_anc_v2_dma_4ch_on = icsd_anc_v2_dma_4ch_on;
    ANC_FUNC.icsd_anc_v2_end = icsd_anc_v2_end;
    ANC_FUNC.icsd_anc_v2_output = icsd_anc_v2_output;
    ANC_FUNC.icsd_anc_sz_output = anc_ear_adaptive_sz_output;
}

void icsd_anc_v2_init(void *_hdl, struct anc_ext_ear_adaptive_param *ext_cfg)
{
    printf("icsd_anc_v2_init\n");
    if (ICSD_REG == NULL) {
        printf("ICSD REG malloc\n");
        ICSD_REG = anc_malloc("ICSD_ANC", sizeof(__icsd_anc_REG));
    }
    anc_function_init();

    struct icsd_anc_init_cfg *hdl = (struct icsd_anc_init_cfg *)_hdl;
    audio_anc_t *param = hdl->param;

    anc_v2_printf = _anc_v2_printf;
    ICSD_REG->icsd_anc_v2_contral |= ICSD_ANC_INITED;
    if (ICSD_REG->icsd_anc_v2_contral & ICSD_ANC_FORCED_BEFORE_INIT) {
        ICSD_REG->icsd_anc_v2_contral &= ~ICSD_ANC_FORCED_BEFORE_INIT;
        icsd_anc_v2_forced_exit();
    }
    if (ICSD_REG->icsd_anc_v2_contral & ICSD_ANC_FORCED_EXIT) {
        os_sem_set(&icsd_anc_sem, 0);
        os_sem_post(&icsd_anc_sem);
        return;
    }

    ICSD_REG->icsd_anc_v2_id = hdl->seq;
    ICSD_REG->icsd_anc_v2_tws_balance_en = hdl->tws_sync;
    anc_v2_printf("ICSD ANC v2 INIT: id %d, seq %d, tws_balance %d \n", ICSD_REG->icsd_anc_v2_id, hdl->seq, hdl->tws_sync);
    mem_stats();
    icsd_anc_v2_open(param, ext_cfg);

#if ANC_USER_TRAIN_TONE_MODE
    ICSD_REG->v2_user_train_state |=	ANC_USER_TRAIN_TONEMODE;
#endif

#if ANC_EARPHONE_CHECK_EN
    ICSD_REG->icsd_anc_v2_function |= ANC_EARPHONE_CHECK;
#endif
    ICSD_REG->v2_user_train_state |=	ANC_USER_TRAIN_DMA_EN;

    //金机曲线功能==================================
    struct icsd_fb_ref *fb_parm = NULL;
    struct icsd_ff_ref *ff_parm = NULL;
    if (param->gains.adaptive_ref_en) {
        anc_adaptive_fb_ref_data_get((u8 **)(&fb_parm));
        anc_adaptive_ff_ref_data_get((u8 **)(&ff_parm));
    }
    //==============================================
    icsd_set_adaptive_run_busy(1);
    icsd_anc_v2_mode_init();
    os_sem_set(&icsd_anc_sem, 0);
    os_sem_post(&icsd_anc_sem);
}


void icsd_anc_v2_cmd(u8 cmd)
{
    //printf("icsd_anc_v2_cmd %x\n", cmd);
    os_taskq_post_msg("anc", 2, ANC_MSG_ICSD_ANC_V2_CMD, cmd);
}
#endif/*TCFG_AUDIO_ANC_EAR_ADAPTIVE_EN*/
