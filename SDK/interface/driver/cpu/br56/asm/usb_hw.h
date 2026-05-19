#ifndef _USB_HW_H_
#define _USB_HW_H_
#include "typedef.h"
//#include "generic/ioctl.h"


#define USB_MAX_HW_EPNUM    4

/* #define ep_regs JL_USB_EP_TypeDef */
typedef struct {
    volatile u32 TXMAXP;
    volatile u32 TXCSR1;
    volatile u32 TXCSR2;
    volatile u32 RXMAXP;
    volatile u32 RXCSR1;
    volatile u32 RXCSR2;
    volatile const u32 RXCOUNT1;
    volatile const u32 RXCOUNT2;
    volatile u32 TXTYPE;
    volatile u32 TXINTERVAL;
    volatile u32 RXTYPE;
    volatile u32 RXINTERVAL;
    u32 RESERVED[0xd0 / 4];
} ep_regs;



//USB_CON0 register
// #define RESERVED        0
#define LOW_SPEED       1
#define USB_NRST        2
#define TM1             3
#define CID             4
#define VBUS            5
// #define RESERVED        6~9
#define SOFIE           10
#define SIEIE           11
#define CLR_SOF         12
#define SOF_PND         13
#define SIE_PND         14
// #define RESERVED        15~17
#define LOWP_MD_        18
#define RST_STL         19
#define EP1_DISABLE     20
#define EP2_DISABLE     21
#define EP3_DISABLE     22
#define EP4_DISABLE     23
#define EP1_RLIM_EN     24
#define EP2_RLIM_EN     25
#define EP3_RLIM_EN     26
#define EP4_RLIM_EN     27
#define TX_BLOCK_EN     28
// #define RESERVED        29~31


//USB_CON1 register
#define EP1_MTX_EN      0
#define EP1_MRX_EN      1
#define EP1_MTX_PND_CLR 2
#define EP1_MRX_PND_CLR 3
#define EP1_MTX_PND     4
#define EP1_MRX_PND     5
// #define RESERVED        6~31


//USB_TXDLY_CON register
#define CLK_DIS         0
#define CLR_PND         1
// #define PND             2
// #define RESERVED        3~15
#define USB_TXDLY_CON(x)    SFR(JL_USB->TXDLY_CON,  16,  16,  x)

#define USB_EP1_RX_LEN(x)    SFR(JL_USB->EP1_RLEN,  0,  11,  x)
#define USB_EP2_RX_LEN(x)    SFR(JL_USB->EP2_RLEN,  0,  11,  x)
#define USB_EP3_RX_LEN(x)    SFR(JL_USB->EP3_RLEN,  0,  11,  x)
#define USB_EP4_RX_LEN(x)    SFR(JL_USB->EP4_RLEN,  0,  11,  x)

#define USB_EP1_MTX_PRD(x)    SFR(JL_USB->EP1_MTX_PRD,  0, 8,  x)
#define USB_EP1_MTX_NUM(x)    SFR(JL_USB->EP1_MTX_NUM,  0, 8,  x)
#define USB_EP1_MRX_PRD(x)    SFR(JL_USB->EP1_MRX_PRD,  0, 8,  x)
#define USB_EP1_MRX_NUM(x)    SFR(JL_USB->EP1_MRX_NUM,  0, 8,  x)

//JL_PORTUSB->CON register
#define RCVEN           0
#define DP_ADCEN        1
#define SR0             2
#define DM_ADCEN        3
#define PDCHKDP         4
#define HDEN            5
#define DIDF            6
#define CHKDPO          7
#define IO_MODE         8
#define USB_DF_MODE     9
#define USB_CP_MODE     10
#define DBG_SEL         11//2bit
#define DBG_SEL_        2
// #define RESERVED        13~15
#define DP_FUSB_PU      16
#define DM_FUSB_PU      17
#define DP_FUSB_PD      18
#define DM_FUSB_PD      19


enum {
    USB0,
};
#define USB_MAX_HW_NUM      1



#endif
