#include "usb/device/usb_pll_trim.h"
#include "jiffies.h"

#define LOG_TAG_CONST       USB
#define LOG_TAG             "[USB_PLL_TRIM]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

#if USB_PLL_TRIM_EN
extern const int clock_sys_src_use_lrc_hw;
void usb_pll_trim_release(void)
{
    USB_TRIM_CON0 &= ~BIT(11);                     // diable sync update mode
    USB_TRIM_CON0  = 0;                            // turn off usb trim
    USB_TRIM_CON1  = 0;                            // turn off usb trim
    USB_TRIM_PND = BIT(4) | BIT(5) | BIT(6) | BIT(7); // clear all pending & close ie
}
u8 usb_pll_trim_init(enum usb_pll_trim_mode mode, u16 trim_prd, u16 freq_deviation) //mode:模式选择; trim_prd:自动校准周期，单位ms; freq_deviation:频率偏差范围, 10 千分之10
{
    if (clock_sys_src_use_lrc_hw == 0) {
        return 0;
    }
    /* if (get_usb_upgrade_mode()) {                  */
    /*     log_info("app_mode is upgrade, return\n"); */
    /*     return 0;                                  */
    /* }                                              */
    if (USB_TRIM_CON0 != 0) {
        log_info("usb_pll_trim is init. return\n");
        return 0;
    }
    log_info("func:%s(), line:%d\n", __func__, __LINE__);
    log_info("mode:%d, trim_prd:%dms, freq_deviation:(%d/1000)\n", mode, trim_prd, freq_deviation);

    u32 freq_cnt;
    u32 pll_nr_1;
    u32 ref_cnt;
    u32 ot;

    usb_pll_trim_release();
    USB_TRIM_CON0 = trim_prd;
    USB_TRIM_CON1 = ((USB_MAX_TH(freq_deviation) << 16) | USB_MIN_TH(freq_deviation));
    USB_PLL_CKSYN_CORE_ENABLE();//同步时钟输出使能

    switch (mode) {
    case USB_TRIM_HAND:
        log_info("USB_TRIM_HAND\n");
        USB_TRIM_CON0 |= BIT(11) | BIT(9);  //同步模式使能，CNT模块使能
        ot = jiffies_to_msecs(jiffies) + trim_prd;
        while (1) {
            if (time_after(jiffies_to_msecs(jiffies), ot)) {
                log_error("WAIT CNT_DONE_PND OT");
                return 0;
            }
            if (USB_TRIM_PND & BIT(9)) {   //查询计数PND
                freq_cnt = USB_FRQ_CNT;    //读cnt计数值
                log_info("freq=  %d \n", USB_FRQ_CNT);
                /* USB_TRIM_CON0 &= ~BIT(9);   //CNT模块关闭 */
                USB_TRIM_PND |= BIT(5);   //清CNT_PND
                break;
            }
        }
        pll_nr_1 = USB_PLL_NR; //读原来pll_nr的值
        log_info("pll_nr_old=  %d \n", pll_nr_1);
        ref_cnt = USB_PLL_TRIM_CLK * 1000;
        pll_nr_1 = ((pll_nr_1 * ref_cnt) + (freq_cnt >> 1)) / freq_cnt; //计算新的pll_nr值

        USB_PLL_NR = pll_nr_1;
        log_info("pll_nr_new=  %d \n", pll_nr_1);

        ot = jiffies_to_msecs(jiffies) + trim_prd;
        while (1) {
            if (time_after(jiffies_to_msecs(jiffies), ot)) {
                log_error("WAIT SYNC_DONE_PND OT");
                return 0;
            }
            if (USB_TRIM_PND & BIT(11)) {
                USB_TRIM_PND |= BIT(7);   //清SYNC_PND
                break;
            }
        }

        break;
    case USB_TRIM_AUTO:
        USB_TRIM_CON0 |= BIT(11) | BIT(10) | BIT(9);
        break;
    default:
        break;
    }
    return 0;
}

#if 1//test

#include "timer.h"
static void test_func(void *priv)
{
    u32 pll_nr = USB_PLL_NR;
    printf("pll_nr:%d\n", pll_nr);
    u32 flag = rand32() % 2;
    u32 rand = rand32() % 30;
    if (flag) {
        rand *= 1;
    } else {
        rand *= -1;
    }
    pll_nr = 2114 + rand;
    USB_PLL_NR = pll_nr;
    printf("rand:%d, pll_nr_set:%d\n", rand, pll_nr);
}
void usb_pll_trim_test()
{
    sys_timer_add(NULL, test_func, 500);
}
#endif


#else
u8 usb_pll_trim_init(enum usb_pll_trim_mode mode, u16 trim_prd, u16 freq_deviation)
{
    return 0;
}

#endif


