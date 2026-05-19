#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".icsd_common.data.bss")
#pragma data_seg(".icsd_common.data")
#pragma const_seg(".icsd_common.text.const")
#pragma code_seg(".icsd_common.text")
#endif

#include "app_config.h"
#include "audio_config_def.h"

#if (TCFG_AUDIO_ANC_ENABLE && \
	 TCFG_AUDIO_ANC_EXT_VERSION == ANC_EXT_V1)

#include "icsd_common.h"

float anc_pow10(float n)
{
    float pow10n = exp_float((float)n / log10_anc(exp_float((float)1.0)));
    return pow10n;
}
double log10_anc(double x)
{
    double tmp = x;
    if (tmp == 0) {
        tmp = 0.0000001;
    }
    return log10_float(tmp);
}
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

void icsd_FFT_radix1024(int *in, int *out)
{
    u32 fft_config;
    fft_config = hw_fft_config(1024, 10, 1, 0, 1);
    hw_fft_run(fft_config, in, out);
}
#endif/*TCFG_AUDIO_ANC_ENABLE*/
