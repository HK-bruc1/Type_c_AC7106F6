
//===============================================================================//
//
//      input IO define
//
//===============================================================================//
#define PA0_IN  1
#define PA1_IN  2
#define PA2_IN  3
#define PA3_IN  4
#define PA4_IN  5
#define PA5_IN  6
#define PA6_IN  7
#define PB0_IN  8
#define PB1_IN  9
#define PB2_IN  10
#define PB3_IN  11
#define PB4_IN  12
#define PB5_IN  13
#define PB6_IN  14
#define PB7_IN  15
#define PB8_IN  16
#define PB9_IN  17
#define PB10_IN  18
#define PB11_IN  19
#define PC0_IN  20
#define PC1_IN  21
#define PC2_IN  22
#define PC3_IN  23
#define PC4_IN  24
#define USBDP_IN  25
#define USBDM_IN  26
#define PP0_IN  27
#define PP1_IN  28
#define PP2_IN  29

//===============================================================================//
//
//      function input select sfr
//
//===============================================================================//
typedef struct {
    __RW __u8 FI_GP_ICH0;
    __RW __u8 FI_GP_ICH1;
    __RW __u8 FI_GP_ICH2;
    __RW __u8 FI_GP_ICH3;
    __RW __u8 FI_GP_ICH4;
    __RW __u8 FI_GP_ICH5;
    __RW __u8 FI_SD0_CMD;
    __RW __u8 FI_SD0_DA0;
    __RW __u8 FI_SD0_DA1;
    __RW __u8 FI_SD0_DA2;
    __RW __u8 FI_SD0_DA3;
    __RW __u8 FI_SPI1_CLK;
    __RW __u8 FI_SPI1_DA0;
    __RW __u8 FI_SPI1_DA1;
    __RW __u8 FI_SPI1_DA2;
    __RW __u8 FI_SPI1_DA3;
    __RW __u8 FI_IIC0_SCL;
    __RW __u8 FI_IIC0_SDA;
    __RW __u8 FI_UART0_RX;
    __RW __u8 FI_UART1_RX;
    __RW __u8 FI_QDEC0_A;
    __RW __u8 FI_QDEC0_B;
    __RW __u8 FI_ALNK0_MCLK;
    __RW __u8 FI_ALNK0_SCLK;
    __RW __u8 FI_ALNK0_LRCK;
    __RW __u8 FI_ALNK0_DAT0;
    __RW __u8 FI_ALNK0_DAT1;
    __RW __u8 FI_ALNK0_DAT2;
    __RW __u8 FI_ALNK0_DAT3;
    __RW __u8 FI_TOTAL;
} JL_IMAP_TypeDef;

#define JL_IMAP_BASE      (ls_base + map_adr(0x3a, 0x00))
#define JL_IMAP           ((JL_IMAP_TypeDef   *)JL_IMAP_BASE)


