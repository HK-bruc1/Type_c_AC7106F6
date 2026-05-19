#ifndef _USB_PLL_TRIM_HW_H_
#define _USB_PLL_TRIM_HW_H_

#include "typedef.h"

#define USB_PLL_TRIM_EN   1

#define USB_PLL_TRIM_CLK    48

#define USB_TRIM_CON0      JL_SYSPLL->TRIM_CON0
#define USB_TRIM_CON1      JL_SYSPLL->TRIM_CON1
#define USB_TRIM_PND       JL_SYSPLL->TRIM_PND
#define USB_FRQ_CNT        JL_SYSPLL->FRQ_CNT
#define USB_FRQ_SCA        JL_SYSPLL->FRC_SCA
#define USB_PLL_CON0       JL_SYSPLL->CON0
#define USB_PLL_CON1       JL_SYSPLL->CON1
#define USB_PLL_NR         JL_SYSPLL->NR
#define USB_PLL_CKSYN_CORE_ENABLE()     (USB_PLL_CON1 |= BIT(13))
#define USB_PLL_CKSYN_CORE_DISABLE()    (USB_PLL_CON1 &= ~BIT(13))

#endif

