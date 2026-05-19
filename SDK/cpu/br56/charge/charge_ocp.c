#include "asm/charge.h"
#include "system/includes.h"
#include "gpadc.h"
#include "gpio.h"
#include "system/timer.h"
#include "poweroff.h"

#define LOG_TAG_CONST   CHARGE
#define LOG_TAG         "[CHARGE]"
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#include "debug.h"

#define CHARGE_OCP_TEST_EN  0
#define OCP_CH    AD_CH_OCP
#define CHARGE_OCP_TRIM_VDDIOM  3000 //ocp_trim 的vddiom档位对应的电压
#define CHARGE_OCP_TRIM_VALUE_TIMES  10 //ocp_trim_value 采样 N 次之和
#define CHARGE_OCP_CURRENT_MAX  200 //电流最大值,过流推关机消息 单位:mA

extern const u8 adc_data_res; //adc 采样精度
struct trim_data_item {
    u32 k;
    u32 offset;
};
struct charge_ocp_info {
    struct trim_data_item trim;
    u32 sum;
    u32 *buf;
    u8 index;
} ocp_info;
static u32 ocp_value_buf[CHARGE_OCP_TRIM_VALUE_TIMES];

static void charge_ocp_callback(void *priv);
u32 charge_ocp_init()
{
    log_info("func:%s()\n", __func__);
    struct trim_data_item item_fuse;
    extern u32 syscfg_read_otp(u32 id, u8 * buf, u32 len);
    int len = syscfg_read_otp(0x401, (u8 *)&item_fuse, sizeof(struct trim_data_item));
    ASSERT(len == sizeof(struct trim_data_item), "func:%s(), line:%d, len = %d, get_ocp_trim fail!\n", __func__, __LINE__, len);
    /* void dump_otp_info(); */
    /* dump_otp_info(); */

    memset(&ocp_info, 0, sizeof(ocp_info));
    ocp_info.trim.k = item_fuse.k;
    ocp_info.trim.offset = item_fuse.offset;
    log_info("ocp_trim_k:%d, ocp_trim_offset:%d\n", ocp_info.trim.k, ocp_info.trim.offset);
    ocp_info.buf = ocp_value_buf;

    P3_OCP_CON0 = 0b10111;
    /* P33_CON_SET(P3_OCP_CON0, 0, 1, 1);  //EN */
    /* P33_CON_SET(P3_OCP_CON0, 1, 3, 0b011);  //SEL */
    /* P33_CON_SET(P3_OCP_CON0, 4, 1, 1);   */
    for (u8 i = 0; i < CHARGE_OCP_TRIM_VALUE_TIMES; i++) {
        ocp_info.buf[i] = adc_get_value_blocking(OCP_CH);
        ocp_info.sum += ocp_info.buf[i];
        log_info("ocp_info.buf[%d]:%d\n", i, ocp_info.buf[i]);
    }
    log_info("ocp_info.sum:%d\n", ocp_info.sum);


#if CHARGE_OCP_TEST_EN
    sys_timer_add(NULL, charge_ocp_callback, 100);
#else
    adc_add_sample_ch(OCP_CH);
    adc_set_sample_period(OCP_CH, 10);
    sys_timer_add(NULL, charge_ocp_callback, 10);
#endif
    return 0;
}

static void charge_ocp_callback(void *priv)
{
#if CHARGE_OCP_TEST_EN //test code
    u32 value = 0;
    u8 times = 10; //采 times 次取均值
    for (u8 i = 0; i < times; i++) {
        value += adc_get_value_blocking_filter_dma(OCP_CH, NULL, CHARGE_OCP_TRIM_VALUE_TIMES + 2);
    }
    value /= times;
    value = (2 * value * get_vddiom_vol() + CHARGE_OCP_TRIM_VDDIOM) / (2 * CHARGE_OCP_TRIM_VDDIOM); //取均值,并四舍五入处理
    int adc_current = ocp_info.trim.k * value - ocp_info.trim.offset;
    printf("T %d(value) %d.%d(mA)\n", value, adc_current / 1000, adc_current / 100 % 10);

#else

    u32 value = adc_get_value(OCP_CH);
    ocp_info.sum = ocp_info.sum + value - ocp_info.buf[ocp_info.index];
    ocp_info.buf[ocp_info.index] = value;
    ocp_info.index = (ocp_info.index + 1) % CHARGE_OCP_TRIM_VALUE_TIMES;

    value = (2 * ocp_info.sum * get_vddiom_vol() + CHARGE_OCP_TRIM_VDDIOM) / (2 * CHARGE_OCP_TRIM_VDDIOM); //取均值,并四舍五入处理
    int adc_current = ocp_info.trim.k * value - ocp_info.trim.offset;
    if (adc_current < 0) {
        adc_current = 0;
    }
    log_debug("%d(value) %d.%d(mA)\n", value, adc_current / 1000, adc_current / 100 % 10);

    if (adc_current / 1000 > CHARGE_OCP_CURRENT_MAX) {
        log_error("OCP_CHECK overcurent protection\n");
        int argv[3];
        argv[0] = (int)sys_enter_soft_poweroff;
        argv[1] = 1;
        argv[2] = POWEROFF_NORMAL;
        int ret = os_taskq_post_type("app_core", Q_CALLBACK, 3, argv);
        if (ret) {
            log_error("gpadc_ocp_check taskq post err\n");
        }
    }
#endif
}

