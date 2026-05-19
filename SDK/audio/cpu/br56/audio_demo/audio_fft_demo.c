/*
 ****************************************************************************
 *							Audio FFT Demo
 *
 *Description	: Audio FFT使用范例
 *Notes			:
 *(1)本demo为开发测试范例，请不要修改该demo， 如有需求，请自行复制再修改
 *(2)FFT输入数据位宽
 *----------------------------------------------------------------------
 *	FFT模式		FFT点数
 *----------------------------------------------------------------------
 *	复数		10/15/20/30/40/45/60/80/90/120/160/180/240/320/360/480/720/960
 *				8/32/64/128/256/512/1024/2048
 *----------------------------------------------------------------------
 *	实数		/20/30/40/60/80/90/120/160/180/240/320/360/480/720/960
 *				16/64/128/256/512/1024/2048
 *----------------------------------------------------------------------
 *
 ****************************************************************************
 */

#include "audio_demo.h"
#include "hw_fft_ext_v2.h"

// ************* For 128 point Real_FFT test *************************//
/********
 *  实数数据进行fft运算时，数据存放地址应4byte对齐，数据排放方式应满足：
 *
 * * {Data_0, Data_1, Data_2, ... , Data_N}
 *
 *******/
void hw_fft_demo_real()
{

    int tmpbuf[130];  // 130 = (128/2+1)*2
    int tmpbuf2[130]; // 130 = (128/2+1)*2
    hw_fft_ext_v2_cfg fft_config;

    printf("********* FFT Real test start  **************\n");
    for (int i = 0; i < 128; i++) {
        tmpbuf[i] = i + 1;
        tmpbuf2[i] = 0;
        printf("RFFT_I_in[%d]: %d \n", i, tmpbuf[i]);
    }

    // Do 128 point FFT
    if (hw_fft_config(&fft_config, 128, 1, 0, 1, 1, 1.f) == NULL) {
        printf("====== Failed to generate fft_config \n");
        return;
    }

    hw_fft_run(&fft_config, tmpbuf, tmpbuf);

    for (int i = 0; i < 128; i++) {
        printf("RFFT_I_out[%d]: %d \n", i, tmpbuf[i]);
    }

    // Do 128 point IFFT
    if (hw_fft_config(&fft_config, 128, 0, 1, 1, 1, 1.f) == NULL) {
        printf("====== Failed to generate fft_config \n");
        return;
    }

    hw_fft_run(&fft_config, tmpbuf, tmpbuf2);

    for (int i = 0; i < 128; i++) {
        printf("RIFFT_I_out[%d]: %d \n", i, tmpbuf2[i]);
    }
}

// ************* For 128 point Complex_FFT test *************************//
/********
 *  复数数据进行fft运算时，数据存放地址应4byte对齐，数据排放方式应满足：
 *
 * {Data_0_real, Data_0_image, Data_1_real, Data_1_image, ... , Data_N_real, Data_N_image}
 *
 *******/
void hw_fft_demo_complex()
{

    float tmpbuf[280];  // 280 = (128/2+1)*2*2
    float tmpbuf2[280]; // 280 = (128/2+1)*2*2
    hw_fft_ext_v2_cfg fft_config;

    printf("********* FFT Complex test start  **************\n");
    for (int i = 0; i < 128; i++) {
        tmpbuf[2 * i] = 2 * i + 1;
        tmpbuf[2 * i + 1] = 2 * i + 2;
        tmpbuf2[2 * i] = 0;
        tmpbuf2[2 * i + 1] = 0;
        printf("CFFT_F_in[%d]: %d \n", 2 * i, (int)(tmpbuf[2 * i]));
        printf("CFFT_F_in[%d]: %d \n", 2 * i + 1, (int)(tmpbuf[2 * i + 1]));
    }

    // Do 128 point CFFT

    if (hw_fft_config(&fft_config, 128, 0, 0, 0, 0, 1.f) == NULL) {
        printf("====== Failed to generate fft_config \n");
        return;
    }

    hw_fft_run(&fft_config, tmpbuf, tmpbuf2);

    for (int i = 0; i < 128; i++) {
        printf("CFFT_F_out[%d]: %d \n", 2 * i, (int)(tmpbuf2[2 * i]));
        printf("CFFT_F_out[%d]: %d \n", 2 * i + 1, (int)(tmpbuf2[2 * i + 1]));
    }

    // Do 128 point CIFFT
    if (hw_fft_config(&fft_config, 128, 1, 1, 0, 0, 1.f) == NULL) {
        printf("====== Failed to generate fft_config \n");
        return;
    }

    hw_fft_run(&fft_config, tmpbuf2, tmpbuf2);

    for (int i = 0; i < 128; i++) {
        printf("CIFFT_F_out[%d]: %d \n", 2 * i, (int)(tmpbuf2[2 * i]));
        printf("CIFFT_F_out[%d]: %d \n", 2 * i + 1, (int)(tmpbuf2[2 * i + 1]));
    }
}


