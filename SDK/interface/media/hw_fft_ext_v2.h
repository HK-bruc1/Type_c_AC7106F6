#ifndef JLFFTINTEFUNC_FFT_EXT_V2_H
#define JLFFTINTEFUNC_FFT_EXT_V2_H

typedef struct {
    unsigned int fft_config0;
    unsigned int fft_config1;
    float fft_config2;
} hw_fft_ext_v2_cfg;

/*********************************************************************
*                  hw_fft_config
* Description: 根据配置生成 FFT_config
* Arguments  :
        ctx  预先申请的配置参数所需空间；
        N  运算数据量；
        is_same_addr 输入输出是否同一个地址，0:否，1:是
        is_ifft 运算类型 0:FFT运算, 1:IFFT运算
        is_real 运算数据的类型  1:实数, 0:复数
        is_int_mode 运算数据模式  1:32位定点数据, 0:32位浮点数据
        scale_factor 运算结果是否做特殊缩放处理 1:无特殊缩放要求, 否则可根据实际情况配置
* Return   : 若成功，则返回修改后的配置结构体指针ctx； 失败则返回 NULL
* Note(s)    : None.
*********************************************************************/
extern void *hw_fft_config(void *_ctx, int N, int is_same_addr, int is_ifft, int is_real, int is_int_mode, float scale_factor);

/*********************************************************************
*                  hw_fft_run
* Description: fft/ifft运算函数
* Arguments  :fft_config FFT运算配置寄存器值
        in  输入数据地址(满足4byte对其，以及非const类型)
        out 输出数据地址
* Return   : void
* Note(s)    : None.
*********************************************************************/
extern void hw_fft_run(hw_fft_ext_v2_cfg *fft_config, void *in, void *out);

/*********************************************************************
 *                    fft_v3 硬件计算FFT所支持的点数如下
 * * 实数类型点数如下：
 * 20, 30, 40, 60, 80, 90, 120, 160, 180, 240, 320, 360, 480, 720, 960,
 * 16, 64, 128, 256, 512, 1024, 2048，
 *
 * * 复数类型点数如下：
 * 10, 15, 20, 30, 40, 45, 60, 80, 90, 120, 160, 180, 240, 320, 360, 480, 720, 960,
 * 8, 32, 64, 128, 256, 512, 1024, 2048，
 *
 *********************************************************************/
#endif

