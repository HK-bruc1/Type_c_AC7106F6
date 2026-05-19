#include "audio_iis_lrclk_capture.h"
#include "gpio_config.h"

#define IIS_LOG_ENABLE          0
#if IIS_LOG_ENABLE
#define LOG_TAG     "[IIS]"
#define LOG_ERROR_ENABLE
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#define LOG_DEBUG_ENABLE
#include "debug.h"
#else
#define log_info(fmt, ...)
#define log_error(fmt, ...)
#define log_debug(fmt, ...)
#endif

#if defined(AUDIO_IIS_LRCLK_CAPTURE_EN) && AUDIO_IIS_LRCLK_CAPTURE_EN

static const u16 iomap_uuid_table[][2] = {
#ifdef PA0_IN
    { PA0_IN, 0xc2e1 },
#endif
#ifdef PA1_IN
    { PA1_IN, 0xc2e2 },
#endif
#ifdef PA2_IN
    { PA2_IN, 0xc2e3 },
#endif
#ifdef PA3_IN
    { PA3_IN, 0xc2e4 },
#endif
#ifdef PA4_IN
    { PA4_IN, 0xc2e5 },
#endif
#ifdef PA5_IN
    { PA5_IN, 0xc2e6 },
#endif
#ifdef PA6_IN
    { PA6_IN, 0xc2e7 },
#endif
#ifdef PA7_IN
    { PA7_IN, 0xc2e8 },
#endif
#ifdef PA8_IN
    { PA8_IN, 0xc2e9 },
#endif
#ifdef PA9_IN
    { PA9_IN, 0xc2ea },
#endif
#ifdef PA10_IN
    { PA10_IN, 0xc302 },
#endif
#ifdef PA11_IN
    { PA11_IN, 0xc303 },
#endif
#ifdef PA12_IN
    { PA12_IN, 0xc304 },
#endif
#ifdef PA13_IN
    { PA13_IN, 0xc305 },
#endif
#ifdef PA14_IN
    { PA14_IN, 0xc306 },
#endif
#ifdef PA15_IN
    { PA15_IN, 0xc307 },
#endif

#ifdef PB0_IN
    { PB0_IN, 0x4f42 },
#endif
#ifdef PB1_IN
    { PB1_IN, 0x4f43 },
#endif
#ifdef PB2_IN
    { PB2_IN, 0x4f44 },
#endif
#ifdef PB3_IN
    { PB3_IN, 0x4f45 },
#endif
#ifdef PB4_IN
    { PB4_IN, 0x4f46 },
#endif
#ifdef PB5_IN
    { PB5_IN, 0x4f47 },
#endif
#ifdef PB6_IN
    { PB6_IN, 0x4f48 },
#endif
#ifdef PB7_IN
    { PB7_IN, 0x4f49 },
#endif
#ifdef PB8_IN
    { PB8_IN, 0x4f4a },
#endif
#ifdef PB9_IN
    { PB9_IN, 0x4f4b },
#endif
#ifdef PB10_IN
    { PB10_IN, 0x4f63 },
#endif
#ifdef PB11_IN
    { PB11_IN, 0x4f64 },
#endif
#ifdef PB12_IN
    { PB12_IN, 0x4f65 },
#endif
#ifdef PB13_IN
    { PB13_IN, 0x4f66 },
#endif
#ifdef PB14_IN
    { PB14_IN, 0x4f67 },
#endif
#ifdef PB15_IN
    { PB15_IN, 0x4f68 },
#endif
#ifdef PC0_IN
    { PC0_IN, 0xdba3 },
#endif
#ifdef PC1_IN
    { PC1_IN, 0xdba4 },
#endif
#ifdef PC2_IN
    { PC2_IN, 0xdba5 },
#endif

#ifdef PC3_IN
    { PC3_IN, 0xdba6 },
#endif
#ifdef PC4_IN
    { PC4_IN, 0xdba7 },
#endif
#ifdef PC5_IN
    { PC5_IN, 0xdba8 },
#endif
#ifdef PC6_IN
    { PC6_IN, 0xdba9 },
#endif
#ifdef PC7_IN
    { PC7_IN, 0xdbaa },
#endif
#ifdef PC8_IN
    { PC8_IN, 0xdbab },
#endif
#ifdef PC9_IN
    { PC9_IN, 0xdbac },
#endif
#ifdef PC10_IN
    { PC10_IN, 0xdbc4 },
#endif
#ifdef PC11_IN
    { PC11_IN, 0xdbc5 },
#endif
#ifdef PC12_IN
    { PC12_IN, 0xdbc6 },
#endif
#ifdef PC13_IN
    { PC13_IN, 0xdbc7 },
#endif
#ifdef PC14_IN
    { PC14_IN, 0xdbc8 },
#endif
#ifdef PC15_IN
    { PC15_IN, 0xdbc9 },
#endif
#ifdef PD0_IN
    { PD0_IN, 0x6804 },
#endif
#ifdef PD1_IN
    { PD1_IN, 0x6805 },
#endif
#ifdef PD2_IN
    { PD2_IN, 0x6806 },
#endif
#ifdef PD3_IN
    { PD3_IN, 0x6807 },
#endif
#ifdef PD4_IN
    { PD4_IN, 0x6808 },
#endif
#ifdef PD5_IN
    { PD5_IN, 0x6809 },
#endif
#ifdef PD6_IN
    { PD6_IN, 0x680a },
#endif
#ifdef PD7_IN
    { PD7_IN, 0x680b },
#endif
#ifdef PE0_IN
    { PE0_IN, 0xf465 },
#endif
#ifdef PE1_IN
    { PE1_IN, 0xf466 },
#endif
#ifdef PE2_IN
    { PE2_IN, 0xf467 },
#endif
#ifdef PE3_IN
    { PE3_IN, 0xf468 },
#endif
#ifdef PE4_IN
    { PE4_IN, 0xf469 },
#endif
#ifdef PE5_IN
    { PE5_IN, 0xf46a },
#endif
#ifdef PE6_IN
    { PE6_IN, 0xf46b },
#endif
#ifdef PG0_IN
    { PG0_IN, 0x0d27 },
#endif
#ifdef PG1_IN
    { PG1_IN, 0x0d28 },
#endif
#ifdef PG2_IN
    { PG2_IN, 0x0d29 },
#endif
#ifdef PG3_IN
    { PG3_IN, 0x0d2a },
#endif
#ifdef PG4_IN
    { PG4_IN, 0x0d2b },
#endif
#ifdef PG5_IN
    { PG5_IN, 0x0d2c },
#endif
#ifdef PG6_IN
    { PG6_IN, 0x0d2d },
#endif
#ifdef PG7_IN
    { PG7_IN, 0x0d2e },
#endif
#ifdef PG8_IN
    { PG8_IN, 0x0d2f },
#endif
#ifdef USBDP_IN
    { USBDP_IN,  0x2cd4 },
#endif
#ifdef USBDM_IN
    { USBDM_IN,  0x2cd1 },
#endif

};

static u8 uuid2iomap(u16 uuid)
{
    for (int i = 0; i < ARRAY_SIZE(iomap_uuid_table); i++) {
        if (iomap_uuid_table[i][1] == uuid) {
            return iomap_uuid_table[i][0];
        }
    }
    return 0xff;
}

#if ( CONFIG_CPU_BR27)
static u32 gpcnt_sfr(u32 gss, u32 css, u32 gss_freq)
{
    int mul = 12;//10:0.41ms, 11:0.83, 12:1.64ms, 13:3.28ms, 14:6.5ms,15:13ms
    float tmp = ((float)gss_freq) / ((float)32 * (1 << mul));

    JL_GPCNT->CON = BIT(30);
    SFR(JL_GPCNT->CON,  1,  5,  css/* 30 */);//次时钟选择cap mux
    SFR(JL_GPCNT->CON,  16,  4,  mul);//主时钟周期数：32 * 2^n = 8192
    SFR(JL_GPCNT->CON,  8, 5,  gss/* 18 */);//主时钟选择lsb

    /* u32 pre_us = audio_jiffies_usec(); */

    SFR(JL_GPCNT->CON,  30,  1,  1);//clear pnd
    SFR(JL_GPCNT->CON,  0,  1,  1); //en
    while (!(JL_GPCNT->CON & BIT(31)));
    SFR(JL_GPCNT->CON,  0,  1,  0);
    u32 dat = JL_GPCNT->NUM;

    /* pre_us = audio_jiffies_usec() - pre_us; */

    float clk_freq = tmp * dat ;

    /* ("==========clk: %d, %d,us:%d\n", (int)clk_freq, dat, pre_us); */
    return (u32)clk_freq;
}
#endif

/*
 *捕获lrclk io的输入频率(iis采样率)
 * */
extern struct _iis_hdl *iis_hdl[2];
u32 iis_global_sample_rate[2];
u32 lrclk_iomap[2] = {0xff, 0xff};
u32 lrclk_uuid[2] = {0};
u32 audio_lrclk_capture(u32 module_idx)
{
    if (lrclk_iomap[module_idx] == 0xff) {
        struct iis_file_cfg cfg;
        int rlen = audio_iis_read_cfg(module_idx, &cfg);
        if (rlen == sizeof(cfg)) {
            lrclk_uuid[module_idx] = cfg.io_lrclk_uuid;
        }
    }

    lrclk_iomap[module_idx] = uuid2iomap(lrclk_uuid[module_idx]);
    if (lrclk_iomap[module_idx] == 0xff) {
        return 0;
    }
    if (!iis_global_sample_rate[module_idx]) {
        u32 lrclk_gpio = uuid2gpio(lrclk_uuid[module_idx]);
        gpio_set_mode(IO_PORT_SPILT(lrclk_gpio), PORT_INPUT_FLOATING);
    }

#if defined(CONFIG_CPU_BR27)
    //input channel
    JL_IMAP->FI_GP_ICH0 = lrclk_iomap[module_idx];
    SFR(JL_IOMAP->ICH_CON1, 28, 4, 0);

    int clk_freq = gpcnt_sfr(18, 30, clk_get("lsb"));

    return (int)clk_freq;
#else
    //其余cpu,待补充
    return 0;
#endif
}

void audio_lrclk_capture_timer(void *p)
{
#if TCFG_IIS_RX_NODE_ENABLE || TCFG_MULTI_CH_IIS_NODE_ENABLE || TCFG_MULTI_CH_IIS_RX_NODE_ENABLE
    struct iis_lrclk_capture *hdl = (struct iis_lrclk_capture *)p;
    u8 module_idx = hdl->module_idx;
    u32 get_sr = audio_lrclk_capture(module_idx);
    for (int i = 0; i < hdl->alink_sr_tab_num; i++) {
        if ((get_sr < hdl->alink_sr_tab[i] * (1 + 0.05f)) && (get_sr > hdl->alink_sr_tab[i] * (1 - 0.05f))) {
            iis_global_sample_rate[module_idx] = hdl->alink_sr_tab[i];
            log_debug("=========real sr %d\n", hdl->alink_sr_tab[i]);
            break;
        }
    }
    if (!get_sr) {
        if (!hdl->need_reopen) {
            if (hdl->close) {
                hdl->close();
            }
            hdl->need_reopen = 1;
            iis_global_sample_rate[module_idx] = 0;
        }
    }
    if (audio_iis_is_working(iis_hdl[module_idx])) {
        u32 sample_rate = audio_iis_get_sample_rate(iis_hdl[module_idx]);
        if ((iis_global_sample_rate[module_idx] && sample_rate != iis_global_sample_rate[module_idx]) || hdl->need_reopen) {
            hdl->need_reopen = 0;
            log_debug("===============pause\n");
            if (hdl->close) {
                hdl->close();
            }

            log_debug("===============resume\n");
            if (hdl->open) {
                hdl->open();
            }
        }
    } else {
        if (hdl->need_reopen && get_sr) {
            hdl->need_reopen = 0;
            log_debug("===============reopen\n");
            if (hdl->open) {
                hdl->open();
            }
        }
    }
#endif
}

/*
 *iis lrclk capture init
 * */
void audio_iis_lrclk_capture_init(struct iis_lrclk_capture *hdl)
{
#if TCFG_IIS_RX_NODE_ENABLE || TCFG_MULTI_CH_IIS_NODE_ENABLE || TCFG_MULTI_CH_IIS_RX_NODE_ENABLE
    if (!hdl) {
        return;
    }
    local_irq_disable();
    hdl->alink_sr_tab_num = ARRAY_SIZE(alink_sr_tab); //支持的采样率个数
    hdl->alink_sr_tab     = alink_sr_tab;             //支持的采样率列表
    if (!hdl->timer) {
        hdl->timer = sys_timer_add(hdl, audio_lrclk_capture_timer, 500);
    }
    local_irq_enable();
#endif
}

/*
 *iis lrclk capture uninit
 * */
void audio_iis_lrclk_capture_uninit(struct iis_lrclk_capture *hdl)
{
#if TCFG_IIS_RX_NODE_ENABLE || TCFG_MULTI_CH_IIS_NODE_ENABLE || TCFG_MULTI_CH_IIS_RX_NODE_ENABLE
    if (!hdl) {
        return;
    }
    local_irq_disable();
    if (hdl->timer) {
        sys_timer_del(hdl->timer);
        hdl->timer = 0;
    }
    local_irq_enable();
#endif
}
/*
 *获取当前捕获到的lrclk频率(采样率)
 * */
u32 audio_iis_get_lrclk_sample_rate(u8 module_idx)
{
    return iis_global_sample_rate[module_idx];
}


/*
 *测试例子
 * */
struct iis_lrclk_capture thdl = {0};
void test_iis_lrclk()
{
    void linein_player_close();
    int linein_player_open();

    thdl.module_idx       = 0;                        //alink 模块 0：alink0, 1:alink1
    thdl.open             = linein_player_open;       //打开对应的播放器
    thdl.close            = linein_player_close;      //关闭对应的播放器
    audio_iis_lrclk_capture_init(&thdl);
}
#endif
