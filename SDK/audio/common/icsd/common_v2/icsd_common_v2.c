#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".icsd_common.data.bss")
#pragma data_seg(".icsd_common.data")
#pragma const_seg(".icsd_common.text.const")
#pragma code_seg(".icsd_common.text")
#endif

#include "app_config.h"
#include "audio_config_def.h"

#if ((TCFG_AUDIO_ANC_ENABLE && TCFG_AUDIO_ANC_EXT_VERSION == ANC_EXT_V2) || \
    (TCFG_AUDIO_AVC_NODE_ENABLE && (TCFG_AVC_ALGO_SELECT == 1)))

#include "icsd_common_v2.h"
#include "audio_anc.h"

#define ICSD_DE_RUN_TIME_DEBUG				0  //DE 算法运行时间测试

#if 0
#define common_log printf
#else
#define common_log(...)
#endif

#if ANC_CHIP_VERSION == ANC_VERSION_BR28
const u8 ICSD_ANC_CPU = ICSD_BR28;
#else /*ANC_CHIP_VERSION == ANC_VERSION_BR50*/
const u8 ICSD_ANC_CPU = ICSD_BR50;
#endif

int icsd_printf_off(const char *format, ...)
{
    return 0;
}

float icsd_anc_pow10(float n)
{
    float pow10n = exp_float((float)n / icsd_log10_anc(exp_float((float)1.0)));
    return pow10n;
}

double icsd_sqrt_anc(double x)
{
    double tmp = x;
    if (tmp == 0) {
        tmp = 0.000001;//
    }
    return sqrt(tmp);
}
double icsd_log10_anc(double x)
{
    double tmp = x;
    if (tmp == 0) {
        tmp = 0.0000001;
    }
    return log10_float(tmp);
}

#if HW_FFT_VERSION == FFT_EXT
#include "hw_fft.h"

void icsd_anc_fft(int *in, int *out)
{
    u32 fft_config;
    fft_config = hw_fft_config(1024, 10, 1, 0, 1);
    hw_fft_run(fft_config, in, out);
}
void icsd_anc_fft256(int *in, int *out)
{
    u32 fft_config;
    fft_config = hw_fft_config(256, 8, 1, 0, 1);
    hw_fft_run(fft_config, in, out);
}
void icsd_anc_fft128(int *in_cur, int *out)
{
    u32 fft_config;
    fft_config = hw_fft_config(128, 7, 1, 0, 1);
    hw_fft_run(fft_config, in_cur, out);
}

void icsd_anc_fft64(int *in_cur, int *out)
{
    u32 fft_config;
    fft_config = hw_fft_config(64, 6, 1, 0, 1);
    hw_fft_run(fft_config, in_cur, out);
}

#elif HW_FFT_VERSION == FFT_EXT_V2

#include "hw_fft_ext_v2.h"

void icsd_anc_fft_base(int N, int *in, int *out)
{
    hw_fft_ext_v2_cfg fft_config;
    if (hw_fft_config(&fft_config, N, 1, 0, 1, 1, 1.f) == NULL) {
        printf("[ICSD_COMMON]Failed to fft_config\n");
        return;
    }
    hw_fft_run(&fft_config, in, out);
}

void icsd_anc_fft(int *in, int *out)
{
    icsd_anc_fft_base(1024, in, out);
}
void icsd_anc_fft256(int *in, int *out)
{
    icsd_anc_fft_base(256, in, out);
}
void icsd_anc_fft128(int *in_cur, int *out)
{
    icsd_anc_fft_base(128, in_cur, out);
}
void icsd_anc_fft64(int *in_cur, int *out)
{
    icsd_anc_fft_base(64, in_cur, out);
}
#endif

float db_diff_v2(float *in1, int in1_idx, float *in2, int in2_idx)
{
    float in1_pxx = in1[in1_idx * 2] * in1[in1_idx * 2] + in1[in1_idx * 2 + 1] * in1[in1_idx * 2 + 1];
    float in2_pxx = in2[in2_idx * 2] * in2[in2_idx * 2] + in2[in2_idx * 2 + 1] * in2[in2_idx * 2 + 1];
    float in1_db = 10 * icsd_log10_anc(in1_pxx);
    float in2_db = 10 * icsd_log10_anc(in2_pxx);
    return in1_db - in2_db;
}

static int *icsd_sde_alloc_addr = NULL;
static int *icsd_de_alloc_addr = NULL;
#if ICSD_DE_RUN_TIME_DEBUG
u32 icsd_de_run_time_last = 0;
#endif
int *icsd_minirt_reuse_ram();
void icsd_minirt_reuse_end();
void icsd_de_malloc()
{
    common_log("icsd_de_malloc\n");
    wdt_clear();
#if ICSD_DE_RUN_TIME_DEBUG
    icsd_de_run_time_last = jiffies_usec();
#endif
    struct icsd_de_libfmt libfmt;
    struct icsd_de_infmt  fmt;
    icsd_de_get_libfmt(&libfmt);
    if (icsd_de_alloc_addr == NULL) {
        int *reuse = icsd_minirt_reuse_ram();
        if (reuse) {
            icsd_de_alloc_addr = reuse;
            memset(reuse, 0, libfmt.lib_alloc_size);
            common_log("DE REUSE RAM SIZE:%d %x\n", libfmt.lib_alloc_size, (int)icsd_de_alloc_addr);
        } else {
            common_log("DE RAM SIZE:%d\n", libfmt.lib_alloc_size);
            icsd_de_alloc_addr = anc_malloc("ICSD_ADJ", libfmt.lib_alloc_size);
        }
    }
    fmt.alloc_ptr = icsd_de_alloc_addr;
    icsd_de_set_infmt(&fmt);
}

void icsd_de_free()
{
    common_log("icsd_de_free\n");
#if ICSD_DE_RUN_TIME_DEBUG
    common_log("ICSD DE RUN time: %d us\n", (int)(jiffies_usec2offset(icsd_de_run_time_last, jiffies_usec())));
#endif
    if (icsd_de_alloc_addr) {
        //common_log("DE RAM anc_free\n");
        int *reuse = icsd_minirt_reuse_ram();
        if (reuse) {
            common_log("DE RAM reuse end\n");
            icsd_minirt_reuse_end();
        } else {
            common_log("DE RAM anc_free\n");
            anc_free(icsd_de_alloc_addr);
        }
        icsd_de_alloc_addr = NULL;
    }
}

void icsd_sde_malloc()
{
    common_log("icsd_sde_malloc\n");
    wdt_clear();
    struct icsd_de_libfmt libfmt;
    struct icsd_de_infmt  fmt;
    icsd_sde_get_libfmt(&libfmt);
    if (icsd_sde_alloc_addr == NULL) {
        common_log("sDE RAM SIZE:%d\n", libfmt.lib_alloc_size);
        int *reuse = icsd_minirt_reuse_ram();
        if (reuse) {
            icsd_sde_alloc_addr = reuse;
            memset(reuse, 0, libfmt.lib_alloc_size);
            common_log("sDE REUSE RAM SIZE:%d %x\n", libfmt.lib_alloc_size, (int)icsd_de_alloc_addr);
        } else {
            icsd_sde_alloc_addr = anc_malloc("ICSD_ADJ", libfmt.lib_alloc_size);
            common_log("sDE RAM SIZE:%d %x\n", libfmt.lib_alloc_size, (int)icsd_de_alloc_addr);
        }
    } else {
        printf("sde ram exist\n");
    }
    fmt.alloc_ptr = icsd_sde_alloc_addr;
    icsd_sde_set_infmt(&fmt);
}

void icsd_sde_free()
{
    common_log("icsd_sde_free\n");
    if (icsd_sde_alloc_addr) {
        //printf("sDE RAM anc_free\n");
        int *reuse = icsd_minirt_reuse_ram();
        if (reuse) {
            common_log("sDE RAM reuse end\n");
            icsd_minirt_reuse_end();
        } else {
            common_log("sDE RAM anc_free\n");
            anc_free(icsd_sde_alloc_addr);
        }
        icsd_sde_alloc_addr = NULL;
    }
}

void fbx2_szcmp_cal(double *fb1_coeff, float fb1_gain, u8 fb1_tap, double *fb2_coeff, float fb2_gain, u8 fb2_tap, float *sz1, float *sz_cmp_factor, float *sz_cmp)
{
    const u8 len = 60;
    const float fs = 46.875e3;

    if (!fb1_coeff || !fb2_coeff || !fb1_tap || !fb2_tap || !sz_cmp_factor) {
        ASSERT(0, "fbx2_szcmp_cal error %p %p %d %d\n", fb1_coeff, fb2_coeff, fb1_tap, fb2_tap);
    }

    float *freq = anc_malloc("ICSD_COMMON", sizeof(float) * len);
    float *hz1 = anc_malloc("ICSD_COMMON", sizeof(float) * len * 2);
    float *hz2 = anc_malloc("ICSD_COMMON", sizeof(float) * len * 2);

    for (int i = 0; i < len; i++) {
        freq[i] = 1.0f * i / 1024 * fs;
    }

    icsd_cal_wz(fb1_coeff, fb1_gain, fb1_tap, freq, 375e3, hz1, len * 2);
    icsd_cal_wz(fb2_coeff, fb2_gain, fb2_tap, freq, 375e3, hz2, len * 2);

    icsd_complex_div_v2(hz2, hz1, sz_cmp, len * 2);
    icsd_complex_muln(sz_cmp, sz_cmp_factor, sz_cmp, len * 2);

    for (int i = 0; i < len; i++) {
        sz_cmp[2 * i] = 1 + sz_cmp[2 * i];
    }

    icsd_complex_muln(sz1, sz_cmp, sz_cmp, len * 2);

    /* for (int i = 0; i < (len * 2); i++) { */
    /* printf("%d, %d, %d, %d, %d \n", (int)(hz1[i] * 1e6), (int)(hz2[i] * 1e6), (int)(sz1[i] * 1e6), (int)(sz_cmp_factor[i] * 1e6), (int)(sz_cmp[i] * 1e6)); */
    /* } */

    anc_free(freq);
    anc_free(hz1);
    anc_free(hz2);
}

#define	ICSD_COMMON_4CH_CIC8_DEBUG		0
#if ICSD_COMMON_4CH_CIC8_DEBUG
#define CIC8_DEBUG_LEN		(1024*4)
#define ANC_DMA_POINT		(1024*2)
s16 wptr_dma1_h_debug[CIC8_DEBUG_LEN];
s16 wptr_dma1_l_debug[CIC8_DEBUG_LEN];
s16 wptr_dma2_h_debug[CIC8_DEBUG_LEN];
s16 wptr_dma2_l_debug[CIC8_DEBUG_LEN];
s16 wptr_dma1_h[ANC_DMA_POINT / 8];
s16 wptr_dma1_l[ANC_DMA_POINT / 8];
s16 wptr_dma2_h[ANC_DMA_POINT / 8];
s16 wptr_dma2_l[ANC_DMA_POINT / 8];
u16 cic8_wptr = 0;
u8 cic8_debug_end = 0;
void icsd_common_ancdma_4ch_cic8_demo(s32 *anc_dma_ppbuf, u8 anc_done_flag)
{
    if (cic8_debug_end) {
        return;
    }
    static u8 cnt = 0;
    if (cnt < 30) {
        cnt++;
        return;
    }
    int *r_ptr = anc_dma_ppbuf + (1 - anc_done_flag) * 2 * ANC_DMA_POINT;
    icsd_common_ancdma_4ch_cic8(r_ptr, wptr_dma1_h, wptr_dma1_l, wptr_dma2_h, wptr_dma2_l, ANC_DMA_POINT);
    memcpy(&wptr_dma1_h_debug[cic8_wptr], wptr_dma1_h, 2 * ANC_DMA_POINT / 8);
    memcpy(&wptr_dma1_l_debug[cic8_wptr], wptr_dma1_l, 2 * ANC_DMA_POINT / 8);
    memcpy(&wptr_dma2_h_debug[cic8_wptr], wptr_dma2_h, 2 * ANC_DMA_POINT / 8);
    memcpy(&wptr_dma2_l_debug[cic8_wptr], wptr_dma2_l, 2 * ANC_DMA_POINT / 8);
    cic8_wptr += ANC_DMA_POINT / 8;
    if (cic8_wptr >= CIC8_DEBUG_LEN) {
        cic8_debug_end = 1;
        local_irq_disable();
        for (int i = 0; i < CIC8_DEBUG_LEN; i++) {
            common_log("DMA1H,DMA1L,DMA2H,DMA2L:%d                         %d                           %d                                %d\n",
                       wptr_dma1_h_debug[i], wptr_dma1_l_debug[i], wptr_dma2_h_debug[i], wptr_dma2_l_debug[i]);
        }
        local_irq_enable();
    }
}
#endif

#endif/*TCFG_AUDIO_ANC_ENABLE*/
