#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".icsd_anc_user.data.bss")
#pragma data_seg(".icsd_anc_user.data")
#pragma const_seg(".icsd_anc_user.text.const")
#pragma code_seg(".icsd_anc_user.text")
#endif
#include "app_config.h"
#include "audio_anc_includes.h"
#include "adc_file.h"
#include "app_tone.h"
#include "clock_manager/clock_manager.h"
#include "audio_src.h"
#include "audio_config.h"
#include "audio_adc.h"
#include "cvp_node.h"
#if TCFG_USER_TWS_ENABLE
#include "bt_tws.h"
#endif

#if (TCFG_AUDIO_ANC_ENABLE && \
	(((defined TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN) && TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN) || \
	((defined TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE) && TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE) || \
	((defined TCFG_AUDIO_ANC_EAR_ADAPTIVE_EN) && TCFG_AUDIO_ANC_EAR_ADAPTIVE_EN)))

#if 1
#define user_log printf
#else
#define user_log(...)
#endif

struct audio_mic_hdl {
    struct audio_adc_output_hdl adc_output;
    struct adc_mic_ch mic_ch;
    s16 *adc_buf;    //align 2Bytes
    u16 dump_packet;
    u8 mic_ch_map;
};
static struct audio_mic_hdl *audio_mic = NULL;
extern struct audio_dac_hdl dac_hdl;
extern struct audio_adc_hdl adc_hdl;
extern const u8 const_adc_async_en;

int audio_mic_en(u8 en, audio_mic_param_t *mic_param,
                 void (*data_handler)(void *priv, s16 *data, int len))
{
    user_log("audio_mic_en : %d", en);
    if (en) {
        if (audio_mic) {
            printf("audio_mic re-malloc error\n");
            return -1;
        }
        audio_mic = anc_malloc("ICSD_ADC", sizeof(struct audio_mic_hdl));
        if (audio_mic == NULL) {
            printf("audio mic zalloc failed\n");
            return -1;
        }

        u16 mic_ch = mic_param->mic_ch_sel;
        u16 sr = mic_param->sample_rate;
        audio_mic->mic_ch_map = mic_ch;


        u8 mic_num = 0;
        /*打开mic电压*/
        audio_mic_pwr_ctl(MIC_PWR_ON);
        audio_adc_file_init();
        for (int i = 0; i < AUDIO_ADC_MAX_NUM; i++) {
            if (mic_ch & BIT(i)) {
                user_log("adc_mic%d open, sr:%d, gain:%d\n", i, sr, mic_param->mic_gain[i]);
                adc_file_mic_open(&audio_mic->mic_ch, AUDIO_ADC_MIC(i));
                audio_adc_mic_set_gain(&audio_mic->mic_ch, AUDIO_ADC_MIC(i), mic_param->mic_gain[i]);
                mic_num ++;
            }
        }
        int adc_buf_size;
        if (const_adc_async_en) {
            adc_buf_size = mic_param->adc_irq_points * ((adc_hdl.bit_width == ADC_BIT_WIDTH_16) ? 2 : 4) * mic_param->adc_buf_num * adc_hdl.max_adc_num;
        } else {
            adc_buf_size = mic_param->adc_irq_points * ((adc_hdl.bit_width == ADC_BIT_WIDTH_16) ? 2 : 4) * mic_param->adc_buf_num * mic_num;
        }
        user_log("adc irq points %d, adc_buf_size : %d", mic_param->adc_irq_points, adc_buf_size);
        /* audio_mic->adc_buf = esco_adc_buf; */
        audio_adc_mic_set_sample_rate(&audio_mic->mic_ch, sr);
        //gali
#if ANC_CHIP_VERSION == ANC_VERSION_BR28
        audio_adc_fixed_digital_set_buffs();
#else
        if (!adc_hdl.hw_buf) {
            audio_mic->adc_buf = anc_malloc("ICSD_ADC", adc_buf_size);
            if (audio_mic->adc_buf == NULL) {
                printf("audio->adc_buf mic zalloc failed\n");
                audio_mic_pwr_ctl(MIC_PWR_OFF);
                anc_free(audio_mic);
                audio_mic = NULL;
                return -1;
            }
            int err = audio_adc_mic_set_buffs(&audio_mic->mic_ch, audio_mic->adc_buf,
                                              mic_param->adc_irq_points * 2, mic_param->adc_buf_num);
            if (err) {
                anc_free(audio_mic->adc_buf);
                audio_mic->adc_buf = NULL;
            }
        }
#endif

        audio_mic->adc_output.handler = data_handler;
        audio_adc_add_output_handler(&adc_hdl, &audio_mic->adc_output);
        audio_adc_mic_start(&audio_mic->mic_ch);
    } else {
        if (audio_mic) {
#if TCFG_AUDIO_ANC_ENABLE
            int mult_flag = audio_anc_mic_mult_flag_get(1);
            if (!mult_flag) {
                /*设置fb mic为复用mic*/
                audio_anc_mic_mult_flag_set(1, 1);
            }
            int ff_mult_flag = audio_anc_mic_mult_flag_get(0);
            if (!ff_mult_flag) {
                audio_anc_mic_mult_flag_set(0, 1);
            }
#endif
#ifdef TCFG_AUDIO_ADC_ENABLE_ALL_DIGITAL_CH
            audio_adc_mic_ch_close(&audio_mic->mic_ch, audio_mic->mic_ch_map);
#else
            //依赖ADC底层驱动修改 gali debug
            /* audio_adc_mic_ch_close(&audio_mic->mic_ch, audio_mic->mic_ch_map); */
            audio_adc_mic_close(&audio_mic->mic_ch);
#endif
            audio_adc_del_output_handler(&adc_hdl, &audio_mic->adc_output);
            if (audio_mic->adc_buf) {
                /* anc_free(audio_mic->adc_buf);  */
                anc_mem_clear(audio_mic->adc_buf); //adc_buf 在audio_adc.c释放，这里只做记录清除
                audio_mic->adc_buf = NULL;
            }
#if TCFG_AUDIO_ANC_ENABLE
            if (!mult_flag) {
                /*清除fb mic为复用mic的标志*/
                audio_anc_mic_mult_flag_set(1, 0);
            }
            if (!ff_mult_flag) {
                audio_anc_mic_mult_flag_set(0, 0);
            }
            if (anc_status_get() == 0)
#endif
            {
                audio_mic_pwr_ctl(MIC_PWR_OFF);
            }
            anc_free(audio_mic);
            audio_mic = NULL;
        }
    }
    return 0;
}

static char *tone_index_to_name(u8 index)
{
    char *file_name = NULL;
    switch (index) {
    case ICSD_ADT_TONE_NUM0 :
    case ICSD_ADT_TONE_NUM1 :
    case ICSD_ADT_TONE_NUM2 :
    case ICSD_ADT_TONE_NUM3 :
    case ICSD_ADT_TONE_NUM4 :
    case ICSD_ADT_TONE_NUM5 :
    case ICSD_ADT_TONE_NUM6 :
    case ICSD_ADT_TONE_NUM7 :
    case ICSD_ADT_TONE_NUM8 :
    case ICSD_ADT_TONE_NUM9 :
        file_name = (char *)get_tone_files()->num[index];
        break;
    case ICSD_ADT_TONE_NORMAL :
        file_name = (char *)get_tone_files()->normal;
        break;
    case ICSD_ADT_TONE_SPKCHAT_ON :
        file_name = (char *)get_tone_files()->spkchat_on;
        break;
    case ICSD_ADT_TONE_SPKCHAT_OFF :
        file_name = (char *)get_tone_files()->spkchat_off;
        break;
    case ICSD_ADT_TONE_WCLICK_ON :
        file_name = (char *)get_tone_files()->wclick_on;
        break;
    case ICSD_ADT_TONE_WCLICK_OFF :
        file_name = (char *)get_tone_files()->wclick_off;
        break;
    case ICSD_ADT_TONE_WINDDET_ON :
        file_name = (char *)get_tone_files()->winddet_on;
        break;
    case ICSD_ADT_TONE_WINDDET_OFF :
        file_name = (char *)get_tone_files()->winddet_off;
        break;
    }
    return file_name;
}

__attribute__((weak))
void icsd_adt_task_play_tone_cb(void *priv)
{

}

static int icsd_adt_tone_play_cb(void *priv, enum stream_event event)
{
    user_log("%s : %d", __func__, (int)event);
    if (event != STREAM_EVENT_STOP) {
        return 0;
    }
    icsd_adt_task_play_tone_cb(priv);
    return 0;
}
static void tws_icsd_adt_tone_callback(int priv, enum stream_event event)
{
    r_printf("tws_icsd_adt_tone_callback: %d\n",  event);
    icsd_adt_tone_play_cb(NULL, event);
}

#define TWS_ICSD_ADT_SYNC_TIMEOUT 400
#define TWS_ICSD_ADT_TONE_STUB_FUNC_UUID   0x3101333A
REGISTER_TWS_TONE_CALLBACK(tws_icsd_adt_tone_stub) = {
    .func_uuid  = TWS_ICSD_ADT_TONE_STUB_FUNC_UUID,
    .callback   = tws_icsd_adt_tone_callback,
};

int icsd_adt_tone_play_callback(u8 index, void (*evt_handler)(void *priv), void *priv)
{
    int ret = 0;
    char *file_name = tone_index_to_name(index);
#if TCFG_USER_TWS_ENABLE
    if (get_tws_sibling_connect_state()) {
        if (tws_api_get_role() == TWS_ROLE_MASTER) {
            ret = tws_play_tone_file_callback(file_name, TWS_ICSD_ADT_SYNC_TIMEOUT, TWS_ICSD_ADT_TONE_STUB_FUNC_UUID);
        }
    } else {
        ret = tws_play_tone_file_callback(file_name, TWS_ICSD_ADT_SYNC_TIMEOUT, TWS_ICSD_ADT_TONE_STUB_FUNC_UUID);
    }
#else
    ret = play_tone_file_callback(file_name, priv, icsd_adt_tone_play_cb);
#endif
    return ret;
}

int icsd_adt_tone_play(u8 index)
{
    int ret = 0;
    char *file_name = tone_index_to_name(index);
#if TCFG_USER_TWS_ENABLE
    if (get_tws_sibling_connect_state()) {
        if (tws_api_get_role() == TWS_ROLE_MASTER) {
            ret = tws_play_tone_file(file_name, TWS_ICSD_ADT_SYNC_TIMEOUT);
        }
    } else {
        ret = tws_play_tone_file(file_name, TWS_ICSD_ADT_SYNC_TIMEOUT);
    }
#else
    ret = play_tone_file(file_name);
#endif
    return ret;
}

void icsd_adt_clock_add()
{
    /* clock_refurbish(); */
    clock_alloc("icsd_adt", 48 * 1000000L);
}

void icsd_adt_clock_del()
{
    clock_free("icsd_adt");
}

void icsd_set_clk()
{
    clock_alloc("icsd_adt", 128 * 1000000L);
}

void icsd_anc_fade_set(enum anc_fade_mode_t mode, int gain)
{
#if TCFG_AUDIO_ANC_ENABLE
    if (mode == ANC_FADE_MODE_WIND_NOISE) {
        audio_anc_fade_ctr_set(mode, AUDIO_ANC_FDAE_CH_FF, gain);
    } else {
        audio_anc_fade_ctr_set(mode, AUDIO_ANC_FDAE_CH_ALL, gain);
    }
#endif
}

/* static u8 esco_player_runing(){} */
/* static u8 bt_get_call_status(){} */
/* static u8 bt_a2dp_get_status(){} */
/* static void bt_cmd_prepare(){} */

void *icsd_adt_src_init(int in_rate, int out_rate, int (*handler)(void *, void *, int))
{
    //void *src_hdl = audio_resample_hw_open(1, in_rate, out_rate, 1);
    //audio_resample_hw_set_output_handler(src_hdl, 0, handler);
    //return src_hdl;

    void *src_hdl = anc_malloc("ICSD_SRC", sizeof(struct audio_src_handle));
    audio_hw_src_open(src_hdl, 1, 1);
    audio_hw_src_set_rate(src_hdl, in_rate, out_rate);
    audio_src_set_output_handler(src_hdl, 0, handler);
    return src_hdl;
}

void icsd_adt_src_write(void *data, int len, void *resample)
{
    //audio_resample_hw_write(resample, data, len);
    audio_src_resample_write(resample, data, len);
}

void icsd_adt_src_push(void *resample)
{
    //audio_resample_hw_push_data_out(resample);
    audio_src_push_data_out((struct audio_src_handle *)resample);
}

void icsd_adt_src_close(void *resample)
{
    //audio_resample_hw_close(resample);
    audio_hw_src_close(resample);
    anc_free(resample);
}

void tws_tx_unsniff_req()
{

}

void icsd_adt_tx_unsniff_req()
{
#if TCFG_USER_TWS_ENABLE
    tws_api_tx_unsniff_req();
#endif /*TCFG_USER_TWS_ENABLE*/
}

void icsd_bt_sniff_set_enable(u8 en)
{
    /* bt_sniff_set_enable(en); */
}

void icsd_set_tws_t_sniff(u16 slot)
{
    /* set_tws_t_sniff(slot); */
}

u8 icsd_get_talk_mic_ch(void)
{
#ifdef TCFG_AUDIO_ANC_TALK_MIC
    if (TCFG_AUDIO_ANC_TALK_MIC != MIC_NULL) {
        return BIT(TCFG_AUDIO_ANC_TALK_MIC);
    } else {
        return 0;
    }
#else
    static u8 talk_mic_ch_cfg_read_flag = 0;
    if (!talk_mic_ch_cfg_read_flag) {
        talk_mic_ch_cfg_read_flag = 1;
        cvp_param_cfg_read();
    }
    return cvp_get_talk_mic_ch();
#endif
}

u8 icsd_get_ref_mic_L_ch(void)
{
    if (ANC_CONFIG_LFF_EN && (TCFG_AUDIO_ANCL_FF_MIC != MIC_NULL)) {
        return BIT(TCFG_AUDIO_ANCL_FF_MIC);
    } else {
        return 0;
    }
}

u8 icsd_get_fb_mic_L_ch(void)
{
    if (ANC_CONFIG_LFB_EN && (TCFG_AUDIO_ANCL_FB_MIC != MIC_NULL)) {
        return BIT(TCFG_AUDIO_ANCL_FB_MIC);
    } else {
        return 0;
    }
}
u8 icsd_get_ref_mic_R_ch(void)
{
    if (ANC_CONFIG_RFF_EN && (TCFG_AUDIO_ANCR_FF_MIC != MIC_NULL)) {
        return BIT(TCFG_AUDIO_ANCR_FF_MIC);
    } else {
        return 0;
    }
}

u8 icsd_get_fb_mic_R_ch(void)
{
    if (ANC_CONFIG_RFB_EN && (TCFG_AUDIO_ANCR_FB_MIC != MIC_NULL)) {
        return BIT(TCFG_AUDIO_ANCR_FB_MIC);
    } else {
        return 0;
    }
}

u8 icsd_get_esco_mic_en_map(void)
{
    return audio_adc_file_get_mic_en_map();
}


#endif /*TCFG_AUDIO_ANC_ENABLE*/

