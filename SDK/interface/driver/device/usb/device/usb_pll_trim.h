#ifndef _USB_PLL_TRIM_H_
#define _USB_PLL_TRIM_H_

#include "asm/usb_pll_trim_hw.h"

#define USB_MAX_TH(x)     (USB_PLL_TRIM_CLK * (1000 + x))
#define USB_MIN_TH(x)     (USB_PLL_TRIM_CLK * (1000 - x))

enum usb_pll_trim_mode {
    USB_TRIM_HAND,  //手动校准模式
    USB_TRIM_AUTO,  //full_speed自动校准模式
};
void usb_pll_trim_release(void);
u8 usb_pll_trim_init(enum usb_pll_trim_mode mode, u16 trim_prd, u16 freq_deviation);

#endif


