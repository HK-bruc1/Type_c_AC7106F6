/*
 ****************************************************************
 *							FFT Accelerator
 * Briefs:
 * Notes :
 *
 ****************************************************************
 */
#include "system/includes.h"
#include "hw_fft_ext_v2.h"
#include "audio_platform_config.h"

const int JL_HW_FFT_V = HW_FFT_VERSION ;    //FFT硬件版本
/*FFT模块资源互斥方式配置*/
#define CRITICAL_NULL 		0
#define CRITICAL_IRQ 		1
#define CRITICAL_MUTEX		2
#define CRITICAL_SPIN_LOCK 	3
#define FFT_CRITICAL_MODE	CRITICAL_SPIN_LOCK

#if (FFT_CRITICAL_MODE == CRITICAL_MUTEX)
static OS_MUTEX fft_mutex;
#define FFT_CRITICAL_INIT() 	os_mutex_create(&fft_mutex)
#define FFT_ENTER_CRITICAL() 	os_mutex_pend(&fft_mutex, 0)
#define FFT_EXIT_CRITICAL() 	os_mutex_post(&fft_mutex)
#elif (FFT_CRITICAL_MODE == CRITICAL_IRQ)
#define FFT_CRITICAL_INIT(...)
#define FFT_ENTER_CRITICAL() 	local_irq_disable()
#define FFT_EXIT_CRITICAL() 	local_irq_enable()
#elif (FFT_CRITICAL_MODE == CRITICAL_SPIN_LOCK)
static spinlock_t fft_lock;
#define FFT_CRITICAL_INIT() 	spin_lock_init(&fft_lock)
#define FFT_ENTER_CRITICAL() 	spin_lock(&fft_lock)
#define FFT_EXIT_CRITICAL() 	spin_unlock(&fft_lock)

#endif /*FFT_CRITICAL_MODE*/

static volatile u8 fft_init = 0;

#define SFR(sfr, start, len, dat) (sfr = (sfr & ~((~(0xffffffff << (len))) << (start))) | (((dat) & (~(0xffffffff << (len)))) << (start)))

/* clang-format off */
/******************************
 * Define for Real Mode
 * Based on Br28
 * ***************************/
#define NUMFFTR 22

static short NFFT_R[NUMFFTR] = {
    16, 20, 30, 40, 60, 64, 80, 90, 120, 128, 160, 180, 240, 256, 320, 360, 480, 512, 720, 960, 1024, 2048, // NFFT
};

static char FFT5N_R[NUMFFTR * 2] = {
    0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 0, 0, // FFT
};

static char FFT3N_R[NUMFFTR * 2] = {
    0, 0, 1, 0, 1, 0, 0, 2, 1, 0, 0, 2, 1, 0, 0, 2, 1, 0, 2, 1, 0, 0, // FFT
};

static char FFT2N_R[NUMFFTR * 2] = {
    1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0, // FFT
};

static char FFT4N_R[NUMFFTR * 2] = {
    1, 0, 0, 1, 0, 2, 1, 0, 1, 3, 2, 0, 1, 3, 2, 1, 2, 4, 1, 2, 4, 5, // FFT
};

static float Result_Amplitude_R[NUMFFTR * 2] = {
    1.f / 2, 1.f / 2, 1.f / 2, 1.f / 2, 1.f / 2, 1.f / 2, 1.f / 2, 1.f / 2, 1.f / 2, 1.f / 2, 1.f / 2, 1.f / 2, 1.f / 2, 1.f / 2, 1.f / 2, 1.f / 2, 1.f / 2, 1.f / 2, 1.f / 2, 1.f / 2, 1.f / 2, 1.f / 2, // FFT
    1.f / 16, 1.f / 20, 1.f / 30, 1.f / 40, 1.f / 60, 1.f / 64, 1.f / 80, 1.f / 90, 1.f / 120, 1.f / 128, 1.f / 160, 1.f / 180, 1.f / 240, 1.f / 256, 1.f / 320, 1.f / 360, 1.f / 480, 1.f / 512, 1.f / 720, 1.f / 960, 1.f / 1024, 1.f / 2048, // IFFT
};

/******************************
 * Define for Complex Mode
 * Based on Br28
 * ***************************/
#define NUMFFTC 26

static short NFFT_C[NUMFFTC] = {
    8, 10, 15, 20, 30, 32, 40, 45, 60, 64, 80, 90, 120, 128, 160, 180, 240, 256, 320, 360, 480, 512, 720, 960, 1024, 2048, // FFT
};

static char FFT5N_C[NUMFFTC] = {
    0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 0, 0, // FFT
};

static char FFT3N_C[NUMFFTC] = {
    0, 0, 1, 0, 1, 0, 0, 2, 1, 0, 0, 2, 1, 0, 0, 2, 1, 0, 0, 2, 1, 0, 2, 1, 0, 0, // FFT
};

static char FFT2N_C[NUMFFTC] = {
    1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, // FFT
};

static char FFT4N_C[NUMFFTC] = {
    1, 0, 0, 1, 0, 2, 1, 0, 1, 3, 2, 0, 1, 3, 2, 1, 2, 4, 3, 1, 2, 4, 2, 3, 5, 5, // FFT
};

static float Result_Amplitude_C[NUMFFTC * 2] = {
    1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f,                     // FFT
    1.f / 8, 1.f / 10, 1.f / 15, 1.f / 20, 1.f / 30, 1.f / 32, 1.f / 40, 1.f / 45, 1.f / 60, 1.f / 64, 1.f / 80, 1.f / 90, 1.f / 120, 1.f / 128, 1.f / 160, 1.f / 180, 1.f / 240, 1.f / 256, 1.f / 320, 1.f / 360, 1.f / 480, 1.f / 512, 1.f / 720, 1.f / 960, 1.f / 1024, 1.f / 2048, // IFFT
};
/* clang-format on */

/*********************************************************************
 *
 * 点数对应索引值查找，找到元素返回索引值，否则返回-1
 *
 * *********************************************************************/
static int binarySearch(short arr[], int len, int target)
{
    int low = 0, high = len - 1, mid;
    while (low <= high) {
        mid = (low + high) / 2;
        if (arr[mid] == target) {
            return mid;
        } else if (arr[mid] > target) {
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }
    return -1;
}



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
void *hw_fft_config(void *_ctx, int N, int is_same_addr, int is_ifft, int is_real, int is_int_mode, float scale_factor)
{
    hw_fft_ext_v2_cfg *ctx = (hw_fft_ext_v2_cfg *)_ctx;
    int N_idx;
    //=====config fft_config0====
    ctx->fft_config0 = 0;
    ctx->fft_config0 |= (is_ifft << 2);
    ctx->fft_config0 |= (is_same_addr << 3);
    ctx->fft_config0 |= (is_real << 4);
    if (is_real == 1) {
        N_idx = binarySearch(NFFT_R, NUMFFTR, N);
        if (N_idx == -1) {
            printf("==== Unsupported Points  \n");
            return NULL;
        }
        //=====config fft_config1====
        ctx->fft_config1 = 0;
        ctx->fft_config1 |= FFT4N_R[N_idx];
        ctx->fft_config1 |= (FFT2N_R[N_idx] << 3);
        ctx->fft_config1 |= (FFT3N_R[N_idx] << 4);
        ctx->fft_config1 |= (FFT5N_R[N_idx] << 6);
        ctx->fft_config1 |= (is_int_mode << 7);
        ctx->fft_config1 |= (is_int_mode << 8);
        ctx->fft_config1 |= (N << 16);
        //=====config fft_config2====
        ctx->fft_config2 = 0;
        if (is_ifft == 1) {
            ctx->fft_config2 = Result_Amplitude_R[N_idx + NUMFFTR] * scale_factor;
        } else {
            ctx->fft_config2 = Result_Amplitude_R[N_idx] * scale_factor;
        }
    } else {
        N_idx = binarySearch(NFFT_C, NUMFFTC, N);
        if (N_idx == -1) {
            printf("==== Unsupported Points  \n");
            return NULL;
        }
        //=========
        ctx->fft_config1 = 0;
        ctx->fft_config1 |= FFT4N_C[N_idx];
        ctx->fft_config1 |= (FFT2N_C[N_idx] << 3);
        ctx->fft_config1 |= (FFT3N_C[N_idx] << 4);
        ctx->fft_config1 |= (FFT5N_C[N_idx] << 6);
        ctx->fft_config1 |= (is_int_mode << 7);
        ctx->fft_config1 |= (is_int_mode << 8);
        ctx->fft_config1 |= (N << 16);
        //=========
        ctx->fft_config2 = 0;
        if (is_ifft == 1) {
            ctx->fft_config2 = Result_Amplitude_C[N_idx + NUMFFTC] * scale_factor;
        } else {
            ctx->fft_config2 = Result_Amplitude_C[N_idx] * scale_factor;
        }
    }
    return ctx;
}

/*********************************************************************
*                  hw_fft_run
* Description: fft/ifft运算函数
* Arguments  :fft_config FFT运算配置寄存器值
        in  输入数据地址(满足4byte对其，以及非const类型)
        out 输出数据地址
* Return   : void
* Note(s)    : None.
*********************************************************************/
void hw_fft_run(hw_fft_ext_v2_cfg *fft_config, void *in, void *out)
{
    if (fft_init == 0) {
        fft_init = 1;
        FFT_CRITICAL_INIT();
    }
    FFT_ENTER_CRITICAL();
    JL_FFT->CON0 = fft_config->fft_config0;
    JL_FFT->CON1 = fft_config->fft_config1;
    JL_FFT->CON2 = *(unsigned int *)(&fft_config->fft_config2);
    // JL_FFT->CADR = (unsigned int)ctx;
    JL_FFT->IADR = (unsigned int)in;
    JL_FFT->OADR = (unsigned int)out;

    //===============Input data exchange in RIFFT=====================
    int is_real = (fft_config->fft_config0 >> 4) & 0x01;
    int is_ifft = (fft_config->fft_config0 >> 2) & 0x01;
    if (is_real && is_ifft) {
        int Nfft = (fft_config->fft_config1 >> 16) & 0x0FFF;
        if ((fft_config->fft_config1 >> 7) & 0x01) {
            int *ptr = (int *)in;
            ptr[1] = ptr[Nfft];
        } else {
            float *ptr = (float *)in;
            ptr[1] = ptr[Nfft];
        }
    }
    //==============================================
    SFR(JL_FFT->CON0, 0, 1, 1);
    SFR(JL_FFT->CON0, 7, 1, 1);
    while (!(JL_FFT->CON0 & (1 << 15)))
        ;
    SFR(JL_FFT->CON0, 14, 1, 1);
    SFR(JL_FFT->CON0, 0, 1, 0);
    //==============================================
    //===============Output data exchange in RFFT=====================
    if (is_real && !is_ifft) {
        int Nfft = (fft_config->fft_config1 >> 16) & 0x0FFF;
        if ((fft_config->fft_config1 >> 7) & 0x01) {
            int *ptr = (int *)out;
            ptr[Nfft] = ptr[1];
            ptr[1] = 0;
            ptr[Nfft + 1] = 0;
        } else {
            float *ptr = (float *)out;
            ptr[Nfft] = ptr[1];
            ptr[1] = 0;
            ptr[Nfft + 1] = 0;
        }
    }
    FFT_EXIT_CRITICAL();
}


//////////////////////////////////////////////////////////////////////////
//							VECTOR
//////////////////////////////////////////////////////////////////////////
#if 0
typedef struct {
    unsigned long  vector_con;
    unsigned long  vector_xadr;
    /* unsigned long  vector_yadr; */
    /* unsigned long  vector_zadr; */
    /* unsigned long  vector_config0; */
    unsigned long  vector_config1;
} hwvec_ctx_t;

static hwvec_ctx_t g_vector_core_set __attribute__((aligned(16)));

//__attribute__ ((always_inline)) inline
void hwvec_exec(
    void *xptr,
    void *yptr,
    void *zptr,
    short x_inc,
    short y_inc,
    short z_inc,
    short nlen,
    short nloop,
    char q,
    long config,
    long const_dat
)
{

    if (fft_init == 0) {
        fft_init = 1;
        FFT_CRITICAL_INIT();
    }

    FFT_ENTER_CRITICAL();

    g_vector_core_set.vector_con = config;
    g_vector_core_set.vector_config0 = (q << 24) | (nloop << 12) | (nlen);
    g_vector_core_set.vector_config1 = (z_inc << 20) | (y_inc << 10) | x_inc;
    //printf("nlen0:%d,nloop:%d,q:%d,config:%d,%d\n",nlen,nloop,q,config,const_dat);

    g_vector_core_set.vector_xadr = (unsigned long)xptr;
    g_vector_core_set.vector_yadr = (unsigned long)yptr;
    g_vector_core_set.vector_zadr = (unsigned long)zptr;

#if 1//0   //build err
    /* JL_FFT->CONST = const_dat; */

    /* JL_FFT->CADR = (unsigned long)(&g_vector_core_set); */


    // nu      clr    vector     ie      en
    /* JL_FFT->CON = (1 << 8) | (1 << 6) | (1 << 3) | (1 << 2) | (1 << 0); */

    /* lp_waiting((int *)&JL_FFT->CON, BIT(7), BIT(6), 116); */
#endif

    FFT_EXIT_CRITICAL();
}
#endif

u8 fft_wrap_for_ldac_enable(void)
{
    return 1;
}

int _FFT_wrap_(int cfg, int *in, int *out)
{
    hw_fft_ext_v2_cfg *fft_config = (hw_fft_ext_v2_cfg *)cfg;

    hw_fft_run(fft_config, in, out);
    return 0;
}




