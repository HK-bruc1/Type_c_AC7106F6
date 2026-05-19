
#ifndef __BR56__
#define __BR56__

//===============================================================================//
//
//      sfr define
//
//===============================================================================//

#define hs_base            0xfe0000
#define ls_base            0xfd0000
#define as_base            0xf10000
#define wl_base            0xf00000

#define __RW               volatile       // read write
#define __RO               volatile const // only read
#define __WO               volatile       // only write

#define __u8               unsigned int   // u8  to u32 special for struct
#define __u16              unsigned int   // u16 to u32 special for struct
#define __u32              unsigned int

#define __s8(x)            char(x); char(reserved_1_##x); char(reserved_2_##x); char(reserved_3_##x)
#define __s16(x)           short(x); short(reserved_1_##x)
#define __s32(x)           int(x)

#define map_adr(grp, adr)  ((64 * grp + adr) * 4)     // grp(0x0-0xff), adr(0x0-0x3f)

//===============================================================================//
//
//      high speed sfr address define
//
//===============================================================================//

//............. 0x0000 - 0x00ff............ for hemu
typedef struct {
    __RW __u32 WREN;
    __RW __u32 CON0;
    __RW __u32 CON1;
    __RW __u32 CON2;
    __RW __u32 CON3;
    __RW __u32 MSG0;
    __RW __u32 MSG1;
    __RW __u32 MSG2;
    __RW __u32 MSG3;
    __RO __u32 ID;
} JL_HEMU_TypeDef;

#define JL_HEMU_BASE                    (hs_base + map_adr(0x00, 0x00))
#define JL_HEMU                         ((JL_HEMU_TypeDef    *)JL_HEMU_BASE)

//............. 0x0100 - 0x01ff............ for sys_mbist
typedef struct {
    __RW __u32 CON;
    __RW __u32 SEL;
    __RW __u32 BEG;
    __RW __u32 END;
    __RW __u32 DAT_VLD0;
    __RW __u32 DAT_VLD1;
    __RW __u32 DAT_VLD2;
    __RW __u32 DAT_VLD3;
    __RO __u32 ROM_CRC;
    __RW __u32 MCFG0_RF1P;
    __RW __u32 MCFG0_RF2P;
    __RW __u32 MCFG0_RM1P;
    __RW __u32 MCFG0_RM2P;
    __RW __u32 MCFG0_VROM;
    __RW __u32 MCFG0_CON;
} JL_HMBIST_TypeDef;

#define JL_HMBIST_BASE                  (hs_base + map_adr(0x01, 0x00))
#define JL_HMBIST                       ((JL_HMBIST_TypeDef *)JL_HMBIST_BASE)

//............. 0x0200 - 0x02ff............ for sfc
typedef struct {
    __RW __u32 CON0;
    __RW __u32 CON1;
    __RW __u32 USER_CMD;
    __RW __u32 USER_CRM;
    __RW __u32 OFFSET_ADR;
    __WO __u32 DEC_KEY;
    __RW __u32 NDEC_SADR;
    __RW __u32 NDEC_EADR;
    __RW __u32 CLK_CON;
    __RW __u32 DTR_CON;
} JL_SFC_TypeDef;

#define JL_SFC_BASE                     (hs_base + map_adr(0x02, 0x00))
#define JL_SFC                          ((JL_SFC_TypeDef    *)JL_SFC_BASE)

//............. 0x0300 - 0x03ff............
typedef struct {
    __RW __u16 MODE_CON;
    __RW __u16 PROT_CON;
} JL_MODE_TypeDef;

#define JL_MODE_BASE                    (hs_base + map_adr(0x03, 0x00))
#define JL_MODE                         ((JL_MODE_TypeDef     *)JL_MODE_BASE)

//............. 0x0400 - 0x04ff............ for syscfg
typedef struct {
    __RW __u32 RST_SRC;
} JL_HSBRST_TypeDef;

#define JL_HSBRST_BASE                  (hs_base + map_adr(0x04, 0x00))
#define JL_HSBRST                       ((JL_HSBRST_TypeDef        *)JL_HSBRST_BASE)

typedef struct {
    __RW __u32 PWR_CON;
    __RW __u32 SYS_CON0;
    __RW __u32 HSB_DIV;
    __RW __u32 HSB_SEL;
} JL_HSBCLK_TypeDef;

#define JL_HSBCLK_BASE                   (hs_base + map_adr(0x04, 0x01))
#define JL_HSBCLK                        ((JL_HSBCLK_TypeDef      *)JL_HSBCLK_BASE)

//............. 0x0500 - 0x05ff............ for pll0_ctl
//............. 0x0600 - 0x06ff............
//............. 0x0700 - 0x07ff............
#define JL_SPI0_BASE                    (hs_base + map_adr(0x07, 0x00))
#define JL_SPI0                         ((JL_SPI_TypeDef      *)JL_SPI0_BASE)

//............. 0x0800 - 0x08ff............

//............. 0x0900 - 0x09ff............
//#define JL_DCP_BASE                     (hs_base + map_adr(0x0a, 0x00))
//#define JL_DCP                          ((JL_DCP_TypeDef  *)JL_DCP_BASE)

//............. 0x0a00 - 0x0aff............
//............. 0x0b00 - 0x0bff............

//............. 0x0c00 - 0x0cff............  EQ
typedef struct {
    __RW __u32 CON0;
    __RW __u32 CON1;
    __RW __u32 CON2;
    __RW __u32 CON3;
    __RW __u32 DATAI_ADR;
    __RW __u32 DATAO_ADR;
    __RW __u32 DATA_LEN;
    __RW __u32 FLT_ADR;
    __RW __u32 BUF_MCTL;
    __RW __u32 BUF_MDAT;
} JL_EQ_TypeDef;
#define JL_EQ_BASE                      (hs_base + map_adr(0x0c, 0x00))
#define JL_EQ                           ((JL_EQ_TypeDef			*)JL_EQ_BASE)

//............. 0x0d00 - 0x0dff............ for src0(src_v2)
typedef struct {
    __RW __u32 CON0;
    __RW __u32 CON1;
    __RW __u32 CON2;
    __RW __u32 CON3;
    __RW __u32 IDAT_ADR;
    __RW __u32 IDAT_LEN;
    __RW __u32 ODAT_ADR;
    __RW __u32 ODAT_LEN;
    __RW __u32 FLTB_ADR;
    __WO __u32 ODAT_ADR_START;
    __WO __u32 ODAT_ADR_END;
    __WO __u32 STOP_FLAG;
    __RW __u32 INSR;
    __RW __u32 OUTSR;
    __RW __u32 PHASE;
    __RW __u32 FLTB_COEF;
    __RW __u32 USF_COEF;
} JL_SRC_TypeDef;
#define JL_SRC0_BASE                     (hs_base + map_adr(0x0d, 0x00))
#define JL_SRC0                          ((JL_SRC_TypeDef			*)JL_SRC0_BASE)

//............. 0x0e00 - 0x0eff............ for src1(src_v2)

//............. 0x0f00 - 0x0fff............
#define JL_PORTF_BASE                   (hs_base + map_adr(0x10, 0x00))
#define JL_PORTF                        ((JL_PORT_TypeDef *)JL_PORTF_BASE)

//............. 0x1100 - 0x11ff............
typedef struct {
    __RW __u32 FSPG_CON;
    __RW __u32 IOMC0;
    __RW __u32 IOMC1;
} JL_SFC_IOMC_TypeDef;

#define JL_SFC_IOMC_BASE                    (hs_base + map_adr(0x11, 0x00))
#define JL_SFC_IOMC                         ((JL_SFC_IOMC_TypeDef     *)JL_SFC_IOMC_BASE)

//............. 0x1200 - 0x12ff............

//............. 0x1300 - 0x13ff............

//............. 0x1400 - 0x14ff............ for src_sync
typedef struct {
    __RW __u32 CON;
    __RW __u32 BEG;
    __RW __u32 END;
    __RW __u32 OFFSET;
} JL_APPMMU_TypeDef;

#define JL_SFC_MMU_BASE                     (hs_base + map_adr(0x13, 0x00))
#define JL_SFC_MMU                          ((JL_APPMMU_TypeDef *)JL_SFC_MMU_BASE)

//#define JL_PSR_MMU_BASE                     (hs_base + map_adr(0x14, 0x00))
//#define JL_PSR_MMU                          ((JL_APPMMU_TypeDef *)JL_PSR_MMU_BASE)

//............. 0x1500 - 0x15ff............ for aes
typedef struct {
    __RW __u32 CON;
    __RW __u32 DATIN;
    __WO __u32 KEY;
    __RW __u32 ENCRES0;
    __RW __u32 ENCRES1;
    __RW __u32 ENCRES2;
    __RW __u32 ENCRES3;
    __WO __u32 NONCE;
    __WO __u16 PHEADER;
    __WO __u8  HEADER;
    __WO __u32 SRCADR;
    __WO __u32 DSTADR;
    __WO __u32 CTCNT;
    __WO __u32 TAGLEN;
    __RO __u32 TAGRES0;
    __RO __u32 TAGRES1;
    __RO __u32 TAGRES2;
    __RO __u32 TAGRES3;
} JL_AES_TypeDef;

#define JL_AES_BASE               (hs_base + map_adr(0x15, 0x00))
#define JL_AES                    ((JL_AES_TypeDef *)JL_AES_BASE)

//............. 0x1600 - 0x16ff............ for aes
//............. 0x1700 - 0x17ff............ for gpcrc
typedef struct {
    __RW __u32 CON;
    __RW __u32 POL;
    __WO __u32 INIT;
    __RW __u32 REG;
} JL_GPCRC_TypeDef;

#define JL_GPCRC_BASE            (hs_base + map_adr(0x17, 0x00))
#define JL_GPCRC                 ((JL_GPCRC_TypeDef    *)JL_GPCRC_BASE)

//............. 0x1900 - 0x19ff............ for vdo_com




//............. 0x2000 - 0x20ff............ for gpu2d




//............. 0x2300 - 0x23ff............ for mtap
typedef struct {
    __RW __u32 MBISTPG_CTL ;
    __RO __u32 MBISTPG_SO  ;
    __RO __u32 MBISTPG_GO  ;
    __RO __u32 MBISTPG_DONE;
    __RW __u32 MBISTPG_SEL ;
} JL_MTAP_TypeDef;

#define JL_MTAP_BASE                    (hs_base + map_adr(0x23, 0x00))
#define JL_MTAP                         ((JL_MTAP_TypeDef       *)JL_MTAP_BASE)

//===============================================================================/
//      low speed sfr address define
//
//===============================================================================//

//............. 0x0000 - 0x00ff............
typedef struct {
    __RW __u32 WREN;
    __RW __u32 CON0;
    __RW __u32 CON1;
    __RW __u32 CON2;
    __RW __u32 CON3;
    __RW __u32 MSG0;
    __RW __u32 MSG1;
    __RW __u32 MSG2;
    __RW __u32 MSG3;
    __RO __u32 ID;
} JL_LEMU_TypeDef;

#define JL_LEMU_BASE                    (ls_base + map_adr(0x00, 0x00))
#define JL_LEMU                         ((JL_LEMU_TypeDef    *)JL_LEMU_BASE)

//............. 0x0100 - 0x01ff............ for pmu_ctl
typedef struct {
    __RW __u32 PMU_CON;
    __RW __u32 PMU_STA;
    __RW __u32 KST_P11;
    __RW __u32 DCV_VOL_CON0;
} JL_PMU_TypeDef;

#define JL_PMU_BASE                     (ls_base + map_adr(0x01, 0x00))
#define JL_PMU                          ((JL_PMU_TypeDef        *)JL_PMU_BASE)

////............. 0x0200 - 0x02ff............ for efuse

//............. 0x0300 - 0x03ff............
typedef struct {
    __RW __u32 CON;
    __RW __u32 PRD0;
    __RW __u32 INI0;
    __RW __u32 PRD1;
    __RW __u32 INI1;
    __RW __u32 PRD2;
    __RW __u32 INI2;
} JL_DCDC_TypeDef;

#define JL_DCDC_BASE          (ls_base + map_adr(0x03, 0x00))
#define JL_DCDC               ((JL_DCDC_TypeDef    *)JL_DCDC_BASE)

typedef struct {
    __WO __u32 KEY;
    __RW __u32 CR;
    __RW __u32 CFG;
    __RO __u32 SR;
} JL_WWDG_TypeDef;

#define JL_WWDG_BASE      (ls_base + map_adr(0x04, 0x00))
#define JL_WWDG           ((JL_WWDG_TypeDef        *)JL_WWDG_BASE)
//............. 0x0600 - 0x06ff............
typedef struct {
    __RW __u32 CON;
    __RW __u32 CNT;
    __RW __u32 PRD;
    __RW __u32 PWM;
    __RW __u32 IRFLT;
} JL_TIMER_TypeDef;


#define JL_TIMER0_BASE      (ls_base + map_adr(0x06, 5*0))
#define JL_TIMER1_BASE      (ls_base + map_adr(0x06, 5*1))
#define JL_TIMER2_BASE      (ls_base + map_adr(0x06, 5*2))
#define JL_TIMER3_BASE      (ls_base + map_adr(0x06, 5*3))
#define JL_TIMER4_BASE      (ls_base + map_adr(0x06, 5*4))
#define JL_TIMER5_BASE      (ls_base + map_adr(0x06, 5*5))
#define JL_TIMER6_BASE      (ls_base + map_adr(0x06, 5*6))
#define JL_TIMER7_BASE      (ls_base + map_adr(0x06, 5*7))

#define JL_TIMER0           ((JL_TIMER_TypeDef     *)JL_TIMER0_BASE)
#define JL_TIMER1           ((JL_TIMER_TypeDef     *)JL_TIMER1_BASE)
#define JL_TIMER2           ((JL_TIMER_TypeDef     *)JL_TIMER2_BASE)
#define JL_TIMER3           ((JL_TIMER_TypeDef     *)JL_TIMER3_BASE)

//............. 0x0700 - 0x07ff............
typedef struct {
    __RW __u8  CON;
    __RW __u8  DAT;
    __RW __u8  SMP;
    __RW __u8  DBE;
} JL_QDEC_TypeDef;

#define JL_QDEC0_BASE                   (ls_base + map_adr(0x07, 0x00))
#define JL_QDEC0                        ((JL_QDEC_TypeDef       *)JL_QDEC0_BASE)


//............. 0x0800 - 0x09ff............
typedef struct {
    __RW __u16 TX_CON0;
    __RW __u16 TX_CON1;
    __RW __u16 RX_CON0;
    __RW __u16 RX_CON1;
    __RW __u16 CON2;
    __RW __u16 BAUD;
    __RW __u8  BUF;
    __RW __u32 OTCNT;
    __RW __u32 TXADR;
    __WO __u16 TXCNT;
    __RW __u32 RXSADR;
    __RW __u32 RXEADR;
    __RW __u32 RXCNT;
    __RO __u16 HRXCNT;
    __RO __u16 RX_ERR_CNT;
} JL_UART_TypeDef;

// typedef struct {
// __RW __u16 CON0;
// __RW __u16 CON1;
// __RW __u16 CON2;
// __RW __u16 BAUD;
// __RW __u8  BUF;
// __RW __u32 OTCNT;
// __RW __u32 TXADR;
// __WO __u16 TXCNT;
// __RW __u32 RXSADR;
// __RW __u32 RXEADR;
// __RW __u32 RXCNT;
// __RO __u16 HRXCNT;
// __RO __u16 RX_ERR_CNT;
// } JL_UART_TypeDef;
#define JL_UART0_BASE                   (ls_base + map_adr(0x08, 0x00))
#define JL_UART0                        ((JL_UART_TypeDef       *)JL_UART0_BASE)
#define JL_UART1_BASE                   (ls_base + map_adr(0x09, 0x00))
#define JL_UART1                        ((JL_UART_TypeDef       *)JL_UART1_BASE)
//............. 0x0a00 - 0x0bff............
typedef struct {
    __RW __u32 CON;
    __RW __u32 BAUD;
    __RW __u32 BUF;
    __WO __u32 ADR;
    __RW __u32 CNT;
    __RW __u32 CON1;
    __RW __u8  ENCCON ;
    __WO __u16 ENCKEY ;
    __WO __u16 ENCADR ;
    __RW __u32 REG;
    __WO __u32 FIFO;
} JL_SPI_TypeDef;

//#define JL_SPI0_BASE                    (ls_base + map_adr(0x0a, 0x00))
//#define JL_SPI0                         ((JL_SPI_TypeDef      *)JL_SPI0_BASE)

#define JL_SPI1_BASE                    (ls_base + map_adr(0x0b, 0x00))
#define JL_SPI1                         ((JL_SPI_TypeDef      *)JL_SPI1_BASE)

// #define JL_SPI2_BASE                    (ls_base + map_adr(0x05, 0x00))
// #define JL_SPI2                         ((JL_SPI_TypeDef      *)JL_SPI2_BASE)
//............. 0x0c00 - 0x0cff............
typedef struct {
    __RW __u32 CON    ;
    __RW __u32 PND    ;
    __RW __u32 TX_BUF ;
    __RW __u32 TASK   ;
    __RO __u32 RX_BUF ;
    __RW __u32 ADDR   ;
    __RW __u32 BAUD   ;
    __RW __u32 TSU    ;
    __RW __u32 THD    ;
    __RO __u32 DBG    ;
    __RW __u32 TRIG_EN;
} JL_IIC_TypeDef;

#define JL_IIC0_BASE                     (ls_base + map_adr(0x0c, 0x00))
#define JL_IIC0                          ((JL_IIC_TypeDef       *)JL_IIC0_BASE)

//............. 0x0d00 - 0x0dff............
typedef struct {
    __RW __u32 CON0;
    __RW __u32 CON1;
    __RW __u32 CON2;
    __WO __u32 CPTR;
    __WO __u32 DPTR;
    __RW __u32 CTU_CON;
    __WO __u32 CTU_CNT;
    __RW __u32 BRK;
    __RW __u8  ENCCON ;
    __WO __u16 ENCKEY ;
    __WO __u16 ENCADR ;
} JL_SD_TypeDef;

#define JL_SD0_BASE                     (ls_base + map_adr(0x0d, 0x00))
#define JL_SD0                          ((JL_SD_TypeDef        *)JL_SD0_BASE)

//............. 0x0e00 - 0x0eff............
typedef struct {
    __RW __u32 CON;
    __RO __u32 RES;
} JL_ADC_TypeDef;

#define JL_ADC_BASE                     (ls_base + map_adr(0x0e, 0x00))
#define JL_ADC                          ((JL_ADC_TypeDef       *)JL_ADC_BASE)

//............. 0x0f00 - 0x0fff............
// typedef struct {
//     __RW __u32 RFLT_CON;
// } JL_IR_TypeDef;

// #define JL_IR_BASE                      (ls_base + map_adr(0x0f, 0x00))
// #define JL_IR                           ((JL_IR_TypeDef         *)JL_IR_BASE)

//............. 0x1000 - 0x10ff............
typedef struct {
    __RW __u32 CON;
    __RW __u32 VAL;
} JL_PCNT_TypeDef;

#define JL_PCNT_BASE                    (ls_base + map_adr(0x10, 0x00))
#define JL_PCNT                         ((JL_PCNT_TypeDef       *)JL_PCNT_BASE)

//............. 0x1100 - 0x11ff............
typedef struct {
    __RW __u32 CON;
    __RO __u32 NUM;
} JL_GPCNT_TypeDef;

#define JL_GPCNT0_BASE                   (ls_base + map_adr(0x11, 0x00))
#define JL_GPCNT0                        ((JL_GPCNT_TypeDef     *)JL_GPCNT0_BASE)

//............. 0x1300 - 0x13ff............


//............. 0x1200 - 0x12ff............
typedef struct {
    __WO __u32 CON;
    __RW __u32 NUM;
} JL_LRCT_TypeDef;

#define JL_LRCT_BASE                    (ls_base + map_adr(0x12, 0x00))
#define JL_LRCT                         ((JL_LRCT_TypeDef     *)JL_LRCT_BASE)

//............. 0x1400 - 0x14ff............
//............. 0x1500 - 0x15ff............ for lsb peri(spi0/sd0)
//............. 0x1600 - 0x16ff............
typedef struct {
    __RO __u32 R64L;
    __RO __u32 R64H;
} JL_RAND_TypeDef;

#define JL_RAND_BASE                    (ls_base + map_adr(0x16, 0x00))
#define JL_RAND                         ((JL_RAND_TypeDef   *)JL_RAND_BASE)

//............. 0x1700 - 0x17ff............
typedef struct {
    __RW __u32 TMR0_CON;
    __RW __u32 TMR0_CNT;
    __RW __u32 TMR0_PR;
    __RW __u32 TMR1_CON;
    __RW __u32 TMR1_CNT;
    __RW __u32 TMR1_PR;
    __RW __u32 TMR2_CON;
    __RW __u32 TMR2_CNT;
    __RW __u32 TMR2_PR;
    __RW __u32 TMR3_CON;
    __RW __u32 TMR3_CNT;
    __RW __u32 TMR3_PR;
    __RW __u32 TMR4_CON;
    __RW __u32 TMR4_CNT;
    __RW __u32 TMR4_PR;
    __RW __u32 TMR5_CON;
    __RW __u32 TMR5_CNT;
    __RW __u32 TMR5_PR;
    __RW __u32 TMR6_CON;
    __RW __u32 TMR6_CNT;
    __RW __u32 TMR6_PR;
    __RW __u32 TMR7_CON;
    __RW __u32 TMR7_CNT;
    __RW __u32 TMR7_PR;
    __RW __u32 FPIN_CON;
    __RW __u32 CH0_CON0;
    __RW __u32 CH0_CON1;
    __RW __u32 CH0_CMPH;
    __RW __u32 CH0_CMPL;
    __RW __u32 CH1_CON0;
    __RW __u32 CH1_CON1;
    __RW __u32 CH1_CMPH;
    __RW __u32 CH1_CMPL;
    __RW __u32 CH2_CON0;
    __RW __u32 CH2_CON1;
    __RW __u32 CH2_CMPH;
    __RW __u32 CH2_CMPL;
    __RW __u32 CH3_CON0;
    __RW __u32 CH3_CON1;
    __RW __u32 CH3_CMPH;
    __RW __u32 CH3_CMPL;
    __RW __u32 CH4_CON0;
    __RW __u32 CH4_CON1;
    __RW __u32 CH4_CMPH;
    __RW __u32 CH4_CMPL;
    __RW __u32 CH5_CON0;
    __RW __u32 CH5_CON1;
    __RW __u32 CH5_CMPH;
    __RW __u32 CH5_CMPL;
    __RW __u32 CH6_CON0;
    __RW __u32 CH6_CON1;
    __RW __u32 CH6_CMPH;
    __RW __u32 CH6_CMPL;
    __RW __u32 CH7_CON0;
    __RW __u32 CH7_CON1;
    __RW __u32 CH7_CMPH;
    __RW __u32 CH7_CMPL;
    __RW __u32 MCPWM_CON0;
} JL_MCPWM_TypeDef;

#define JL_MCPWM_BASE                   (ls_base + map_adr(0x17, 0x00))
#define JL_MCPWM                        ((JL_MCPWM_TypeDef     *)JL_MCPWM_BASE)

//............. 0x1800 - 0x18ff............


//............. 0x1900 - 0x19ff............ for spdif


//............. 0x1a00 - 0x1aff............ for led
typedef struct {
    __RW __u32 CON0;
    __RW __u32 CON1;
    __RW __u32 BRI_PRD;
    __RW __u32 BRI_DUTY0;
    __RW __u32 BRI_DUTY1;
    __RW __u32 PRD_DIV;
    __RW __u32 DUTY0;
    __RW __u32 DUTY1;
    __RW __u32 DUTY2;
    __RW __u32 DUTY3;
    __RW __u32 CNT_CON;
    __RW __u32 CNT_DEC;
    __RO __u32 CNT_DATA;
    __WO __u32 CNT_SYNC;
    __RW __u32 SCON0;
    __RW __u32 SCON1;
    __RW __u32 OUT1_DIV;
    __RW __u32 OUT0_DIV;
    __RW __u32 ANA_CON0;
} JL_PLED_TypeDef;

#define JL_PLED_BASE                    (ls_base + map_adr(0x1a, 0x00))
#define JL_PLED                         ((JL_PLED_TypeDef      *)JL_PLED_BASE)

//............. 0x1b00 - 0x1bff............ for lcd
//typedef struct {
//    __RW __u32 CON0;
//    __RW __u32 CON1;
//    __RW __u32 SEG_IO_EN0;
//    __RW __u32 SEG_IO_EN1;
//    __RW __u32 SEG0_DAT;
//    __RW __u32 SEG1_DAT;
//    __RW __u32 SEG2_DAT;
//    __RW __u32 SEG3_DAT;
//    __RW __u32 SEG4_DAT;
//    __RW __u32 SEG5_DAT;
//} JL_LCDC_TypeDef;

//#define JL_LCDC_BASE                    (ls_base + map_adr(0x1b, 0x00))
//#define JL_LCDC                         ((JL_LCDC_TypeDef      *)JL_LCDC_BASE)

//............. 0x1c00 - 0x1cff............ for pps
//typedef struct {
//    __RW __u32 CON;
//    __RW __u32 CNT;
//    __RW __u32 CFG;
//    __RW __u32 VTRIM;
//    __RW __u32 TX_BASE;
//    __RW __u32 RX_BASE;
//} JL_PPS_TypeDef;
//
//#define JL_PPS_BASE                     (ls_base + map_adr(0x1c, 0x00))
//#define JL_PPS                          ((JL_PPS_TypeDef       *)JL_PPS_BASE)

//............. 0x1d00 - 0x1dff............ for ppm
//typedef struct {
//    __RW __u32 CON;
//    __RW __u32 STRAN_DAT;
//    __RW __u32 STRAN_SET;
//    __RW __u32 DMA_ADR;
//    __RW __u32 PPM_RD;
// } JL_PPM_TypeDef;

// #define JL_PPM_BASE                    (ls_base + map_adr(0x1d, 0x00))
// #define JL_PPM                         ((JL_PPM_TypeDef       *)JL_PPM_BASE)

//............. 0x1f00 - 0x1fff............ for wl uart (only FPGA)
typedef struct {
    __RW __u16 CON0;
    __RW __u16 CON1;
    __RW __u16 CON2;
    __RW __u16 BAUD;
    __RW __u8  BUF;
} JL_WL_UART_LITE_TypeDef;

#define JL_WL_UART_BASE                 (ls_base + map_adr(0x1f, 0x00))
#define JL_WL_UART                      ((JL_WL_UART_LITE_TypeDef *)JL_WL_UART_BASE)

//............. 0x2000 - 0x24ff............ for sie
typedef struct {
    __RW __u32 FADDR;
    __RW __u32 POWER;
    __RO __u32 INTRTX1;
    __RO __u32 INTRTX2;
    __RO __u32 INTRRX1;
    __RO __u32 INTRRX2;
    __RO __u32 INTRUSB;
    __RW __u32 INTRTX1E;
    __RW __u32 INTRTX2E;
    __RW __u32 INTRRX1E;
    __RW __u32 INTRRX2E;
    __RW __u32 INTRUSBE;
    __RO __u32 FRAME1;
    __RO __u32 FRAME2;
    __RO __u32 RESERVED14;
    __RW __u32 DEVCTL;
    __RO __u32 RESERVED10_0x16[0x16 - 0x10 + 1];
} JL_USB_SIE_TypeDef;

#define JL_USB_SIE_BASE                 (ls_base + map_adr(0x20, 0x00))
#define JL_USB_SIE                      ((JL_USB_SIE_TypeDef *)JL_USB_SIE_BASE)

typedef struct {
    __RO __u32 RESERVED0;
    __RW __u32 CSR0;
    __RO __u32 RESERVED2_5[5 - 1];
    __RO __u32 COUNT0;

} JL_USB_EP0_TypeDef;

#define JL_USB_EP0_BASE                 (ls_base + map_adr(0x20, 0x10))
#define JL_USB_EP0                      ((JL_USB_EP0_TypeDef *)JL_USB_EP0_BASE)

typedef struct {
    __RW __u32 TXMAXP;
    __RW __u32 TXCSR1;
    __RW __u32 TXCSR2;
    __RW __u32 RXMAXP;
    __RW __u32 RXCSR1;
    __RW __u32 RXCSR2;
    __RO __u32 RXCOUNT1;
    __RO __u32 RXCOUNT2;
    __RW __u32 TXTYPE;
    __RO __u32 TXINTERVAL;
    __RW __u32 RXTYPE;
    __RO __u32 RXINTERVAL;

} JL_USB_EP_TypeDef;

#define JL_USB_EP1_BASE                 (ls_base + map_adr(0x21, 0x10))
#define JL_USB_EP1                      ((JL_USB_EP_TypeDef *)JL_USB_EP1_BASE)

#define JL_USB_EP2_BASE                 (ls_base + map_adr(0x22, 0x10))
#define JL_USB_EP2                      ((JL_USB_EP_TypeDef *)JL_USB_EP2_BASE)

#define JL_USB_EP3_BASE                 (ls_base + map_adr(0x23, 0x10))
#define JL_USB_EP3                      ((JL_USB_EP_TypeDef *)JL_USB_EP3_BASE)

// #define JL_USB_EP4_BASE                 (ls_base + map_adr(0x24, 0x10))
// #define JL_USB_EP4                      ((JL_USB_EP_TypeDef *)JL_USB_EP4_BASE)

//............. 0x2500 - 0x25ff............
typedef struct {
    __RW __u32 CON0;
    __RW __u32 CON1;
    __WO __u32 EP0_CNT;
    __WO __u32 EP1_CNT;
    __WO __u32 EP2_CNT;
    __WO __u32 EP3_CNT;
    __WO __u32 EP4_CNT;
    __WO __u32 EP0_ADR;
    __WO __u32 EP1_TADR;
    __WO __u32 EP1_RADR;
    __WO __u32 EP2_TADR;
    __WO __u32 EP2_RADR;
    __WO __u32 EP3_TADR;
    __WO __u32 EP3_RADR;
    __WO __u32 EP4_TADR;
    __WO __u32 EP4_RADR;
    __RW __u32 TXDLY_CON;
    __RW __u32 EP1_RLEN;
    __RW __u32 EP2_RLEN;
    __RW __u32 EP3_RLEN;
    __RW __u32 EP4_RLEN;
    __RW __u32 EP1_MTX_PRD;
    __RW __u32 EP1_MRX_PRD;
    __RO __u32 EP1_MTX_NUM;
    __RO __u32 EP1_MRX_NUM;
    __RW __u32 SOF_STA_CON;
} JL_USB_TypeDef;

#define JL_USB_BASE                     (ls_base + map_adr(0x25, 0x00))
#define JL_USB                          ((JL_USB_TypeDef       *)JL_USB_BASE)

//............. 0x3000 - 0x34ff............ for port
typedef struct {
    __RO __u32 IN;
    __RW __u32 OUT;
    __RW __u32 DIR;
    __RW __u32 DIE;
    __RW __u32 DIEH;
    __RW __u32 PU;
    __RW __u32 PD;
    __RW __u32 HD;
    __RW __u32 SPL;
    __RW __u32 CON; // usb phy only
    __RW __u32 OUT_BSR;
    __RW __u32 DIR_BSR;
    __RW __u32 DIE_BSR;
    __RW __u32 DIEH_BSR;
    __RW __u32 PUL_BSR;
    __RW __u32 PUH_BSR;
    __RW __u32 PDL_BSR;
    __RW __u32 PDH_BSR;
    __RW __u32 HDL_BSR;
    __RW __u32 HDH_BSR;
    __RW __u32 SPL_BSR;
    __RW __u32 CON_BSR;   // usb phy only
} JL_PORT_TypeDef;

#define JL_PORTA_BASE                   (ls_base + map_adr(0x30, 0x00))
#define JL_PORTA                        ((JL_PORT_TypeDef *)JL_PORTA_BASE)

#define JL_PORTB_BASE                   (ls_base + map_adr(0x31, 0x00))
#define JL_PORTB                        ((JL_PORT_TypeDef *)JL_PORTB_BASE)

#define JL_PORTC_BASE                   (ls_base + map_adr(0x32, 0x00))
#define JL_PORTC                        ((JL_PORT_TypeDef *)JL_PORTC_BASE)

#define JL_PORTP_BASE                   (ls_base + map_adr(0x34, 0x00))
#define JL_PORTP                        ((JL_PORT_TypeDef *)JL_PORTP_BASE)

#define JL_PORTUSB_BASE                 (ls_base + map_adr(0x35, 0x00))
#define JL_PORTUSB                      ((JL_PORT_TypeDef *)JL_PORTUSB_BASE)

//............. 0x3d00 - 0x3dff............ for port others
typedef struct {
    __RW __u32 CON0;
    __RW __u32 CON1;
    __RW __u32 CON2;
    __RW __u32 CON3;
} JL_WAKEUP_TypeDef;

#define JL_WAKEUP_BASE                  (ls_base + map_adr(0x3e, 0x00))
#define JL_WAKEUP                       ((JL_WAKEUP_TypeDef   *)JL_WAKEUP_BASE)

//    __RW __u32 FSPG_CON;
typedef struct {
    __RW __u32 SPI_IOMC0  ;
    __RW __u32 SDC_IOMC0  ;
    __RW __u32 SYS_IOMC0  ;
    __RW __u32 SYS_IOMC1  ;
    __RW __u32 ASS_IOMC0  ;
    __RW __u32 WL_IOMC0   ;
    __RW __u32 VDO_IOMC0  ;
    __RW __u32 ICH_IOMC0  ;
    __RW __u32 ICH_IOMC1  ;
    __RW __u32 ICH_IOMC2  ;
    __RW __u32 ICH_IOMC3  ;
    __RW __u32 ICH_IOMC4  ;
    __RW __u32 ICH_IOMC5  ;
    __RW __u32 OCH_IOMC0  ;
    __RW __u32 OCH_IOMC1  ;
    __RW __u32 OCH_IOMC2  ;
    __RW __u32 OCH_IOMC3  ;
    __RW __u32 OCH_IOMC4  ;
    __RW __u32 OCH_IOMC5  ;
    __RW __u32 LCDPG_CON  ;
    __RW __u32 MTPG_CON   ;
    __RW __u32 STPG_CON   ;
    __RW __u32 WAT_CON    ;
} JL_IOMC_TypeDef;

#define JL_IOMC_BASE                    (ls_base + map_adr(0x3e, 0x05))
#define JL_IOMC                         ((JL_IOMC_TypeDef     *)JL_IOMC_BASE)


//............. 0x4000 - 0x40ff............ for syscfg
typedef struct {
    __RW __u32 RST_SRC;
} JL_LSBRST_TypeDef;

#define JL_LSBRST_BASE                  (ls_base + map_adr(0x40, 0x00))
#define JL_LSBRST                       ((JL_LSBRST_TypeDef        *)JL_LSBRST_BASE)

typedef struct {
    __RW __u32 PWR_CON;
    __RW __u32 SYS_CON0;
    __RW __u32 LSB_SEL;
    __RW __u32 STD_CON0;
    __RW __u32 STD_CON1;
    __RW __u32 STD_CON2;
    __RW __u32 PRP_CON0;
    __RW __u32 PRP_CON1;
    __RW __u32 PRP_CON2;
} JL_LSBCLK_TypeDef;

#define JL_LSBCLK_BASE                   (ls_base + map_adr(0x40, 0x01))
#define JL_LSBCLK                        ((JL_LSBCLK_TypeDef      *)JL_LSBCLK_BASE)

typedef struct {
    __RW __u32 DPLL_CON     ;
    __RW __u32 DPLL_PREDIV0 ;
    __RW __u32 DPLL_POSTDIV0;
    __RW __u32 DPLL_PREDIV1 ;
    __RW __u32 DPLL_POSTDIV1;
} JL_DPLL_TypeDef;

#define JL_DPLL_BASE                    (ls_base + map_adr(0x40, 0x0a))
#define JL_DPLL                         ((JL_DPLL_TypeDef      *)JL_DPLL_BASE)

typedef struct {
    __RW __u32 XOSC_CON0    ;
    __RW __u32 XOSC_CON1    ;
    __RW __u32 XOSC_CON2    ;
    __RW __u32 XOSC_CON3    ;
    __RW __u32 XOSC_CON4    ;
} JL_XOSC_TypeDef;

#define JL_XOSC_BASE                    (ls_base + map_adr(0x40, 0x0f))
#define JL_XOSC                         ((JL_XOSC_TypeDef      *)JL_XOSC_BASE)


//............. 0x4100 - 0x41ff............ for pll0_ctl
typedef struct {
    __RW __u32 CON0;
    __RW __u32 CON1;
    __RW __u32 CON2;
    __RW __u32 CON3;
    __RW __u32 NR;
    __RW __u32 CON8;
    __RW __u32 CON9;
    __RW __u32 CON10;
    __RW __u32 CON11;
    __RW __u32 INTF0;
    __RW __u32 INTF1;
    __RW __u32 TRIM_CON0;
    __RW __u32 TRIM_CON1;
    __RW __u32 TRIM_PND;
    __RW __u32 FRQ_CNT;
    __RW __u32 FRC_SCA;
} JL_SYSPLL_TypeDef;

#define JL_SYSPLL_BASE                   (ls_base + map_adr(0x41, 0x00))
#define JL_SYSPLL                        ((JL_SYSPLL_TypeDef      *)JL_SYSPLL_BASE)


// //............. 0x4200 - 0x42ff............ for pll_ctl
// typedef struct {
//   __RW __u32 CON0;
//   __RW __u32 CON1;
//   __RW __u32 CON2;
//   __RW __u32 CON3;
//   __RW __u32 NR;
//   __RW __u32 INTF0;
//   __RW __u32 INTF1;
// } JL_BSBPLL_TypeDef;
//
// #define JL_BSBPLL_BASE                   (ls_base + map_adr(0x42, 0x00))
// #define JL_BSBPLL                        ((JL_BSBPLL_TypeDef      *)JL_BSBPLL_BASE)
//............. 0x4300 - 0x43ff...........
typedef struct {
    __RW __u32 WLA_CON0 ;
    __RW __u32 WLA_CON1 ;
    __RW __u32 WLA_CON2 ;
    __RW __u32 WLA_CON3 ;
    __RW __u32 WLA_CON4 ;
    __RW __u32 WLA_CON5 ;
    __RW __u32 WLA_CON6 ;
    __RW __u32 WLA_CON7 ;
    __RW __u32 WLA_CON8 ;
    __RW __u32 WLA_CON9 ;
    __RW __u32 WLA_CON10;
    __RW __u32 WLA_CON11;
    __RW __u32 WLA_CON12;
    __RW __u32 WLA_CON13;
    __RW __u32 WLA_CON14;
    __RW __u32 WLA_CON15;
    __RW __u32 WLA_CON16;
    __RW __u32 WLA_CON17;
    __RW __u32 WLA_CON18;
    __RW __u32 WLA_CON19;
    __RW __u32 WLA_CON20;
    __RW __u32 WLA_CON21;
    __RW __u32 WLA_CON22;
    __RW __u32 WLA_CON23;
    __RW __u32 WLA_CON24;
    __RW __u32 WLA_CON25;
    __RW __u32 WLA_CON26;
    __RW __u32 WLA_CON27;
    __RW __u32 WLA_CON28;
    __RW __u32 WLA_CON29;
    __RO __u32 WLA_CON30;
    __RW __u32 WLA_CON31;
    __RW __u32 WLA_CON32;
    __RW __u32 WLA_CON33;
    __RW __u32 WLA_CON34;
    __RW __u32 WLA_CON35;
    __RW __u32 WLA_CON36;
    __RW __u32 WLA_CON37;
    __RW __u32 WLA_CON38;
    __RW __u32 WLA_CON39;
} JL_WLA_TypeDef;

#define JL_WLA_BASE                     (ls_base + map_adr(0x43, 0x00))
#define JL_WLA                          ((JL_WLA_TypeDef       *)JL_WLA_BASE)


//............. 0x4400 - 0x44ff............

//............. 0xxx00 - 0xxxff............ TODO
//typedef struct {
//    __WO __u32 CHIP_ID;
//} JL_SYSTEM_TypeDef;
//
//#define JL_SYSTEM_BASE                  (ls_base + map_adr(0xxx, 0x00))
//#define JL_SYSTEM                       ((JL_SYSTEM_TypeDef   *)JL_SYSTEM_BASE)

#include "io_omap.h"
#include "io_imap.h"

//===============================================================================//
//
//  ass  sfr address define
//
//===============================================================================//

//............. 0x0000 - 0x00ff............ ass_mbist
typedef struct {
    __RW __u32 CON;
    __RW __u32 SEL;
    __RW __u32 BEG;
    __RW __u32 END;
    __RW __u32 DAT_VLD0;
    __RW __u32 DAT_VLD1;
    __RW __u32 DAT_VLD2;
    __RW __u32 DAT_VLD3;
    __RO __u32 ROM_CRC;
    __RW __u32 MCFG0_RF1P;
    __RW __u32 MCFG0_RF2P;
    __RW __u32 MCFG0_RM1P;
    __RW __u32 MCFG0_RM2P;
    __RW __u32 MCFG0_VROM;
    __RW __u32 MCFG0_CON;
} JL_ASS_MBIST_TypeDef;

#define JL_ASS_MBIST_BASE                     (as_base + map_adr(0x00, 0x00))
#define JL_ASS_MBIST                          ((JL_ASS_MBIST_TypeDef  *)JL_ASS_MBIST_BASE)


//............. 0x0100 - 0x01ff............ as_aud

typedef struct {
    __RW __u32 AUD_CON0;
    __RW __u32 AUD_CON1;
    __RW __u32 AUD_CON2;
    __RW __u32 AUD_CON3;
    __RW __u32 AUD_CON4;
    __RW __u32 AUD_CON5;
    __RW __u32 AUD_CON6;
    __WO __u32 AUD_COP;
} JL_AUD_TypeDef;

#define JL_AUD_BASE                (as_base + map_adr(0x01, 0x00))
#define JL_AUD                     ((JL_AUD_TypeDef  *)JL_AUD_BASE)


//............. 0x0200 - 0x02ff............ as_dac
typedef struct {
    __RW __u32(DAC_CON0);
    __RW __u32(DAC_CON1);
    __RW __u32(DAC_CON2);
    __RW __u32(DAC_CON3);
    __RW __u32(DAC_CON4);
    __RW __u32(DAC_CON5);
    __RW __u32(DAC_CON6);
    __RW __u32(DAC_CON7);
    __RW __u32(DAC_CON8);
    __RW __u32(DAC_CON9);
    __RW __u32(DAC_CON10);
    __RW __u32(DAC_CON11);
    __RW __u32(DAC_CON12);
    __RW __u32(DAC_CON13);
    __RW __u32(DAC_CON14);
    __RW __u32(DAC_CON15);
    __RW __u32(DAC_VL0);
    __RW __u32(DAC_DIT);
    __RW __u32(DAC_TM0);
    //__WO __u16 (DAC_COP);
    __WO __u32(DAC_DCW);
    __RO __u32(DAC_DCR);

    __RW __u32(DAC_ADR);    /* 0A */
    __RW __u16(DAC_LEN);    /* 0B */
    __RW __u16(DAC_PNS);    /* 0C */
    __RW __u16(DAC_HRP);    /* 0D */
    __RW __u16(DAC_SWP);    /* 0E */
    __RW __u16(DAC_SWN);    /* 0F */
} JL_AUDDAC_TypeDef;

#define JL_AUDDAC_BASE               (as_base + map_adr(0x02, 0x00))
#define JL_AUDDAC                    ((JL_AUDDAC_TypeDef   *)JL_AUDDAC_BASE)

//............. 0x0300 - 0x03ff............ as_adc
typedef struct {
    __RW __u32(ADC_CON0);
    __RW __u32(ADC_CON1);
    __RW __u32(ADC_CON2);
    //__RW __u16 (ADC_COP);
    __WO __u32(ADC_DCW0);
    __WO __u32(ADC_DCW1);

    __RO __u32(ADC_DCR0);
    __RO __u32(ADC_DCR1);

    __RW __u32(ADC_ADR);
    __RW __u16(ADC_LEN);
    __RW __u16(ADC_PNS);
    __RW __u16(ADC_HWP);
    __RW __u16(ADC_SRP);
    __RW __u16(ADC_SRN);
} JL_AUDADC_TypeDef;

#define JL_AUDADC_BASE               (as_base + map_adr(0x03, 0x00))
#define JL_AUDADC                    ((JL_AUDADC_TypeDef   *)JL_AUDADC_BASE)

//............. 0x0400 - 0x04ff............ as_ad2da

//#define JL_AD2DA_BASE               (as_base + map_adr(0x04, 0x00))
//#define JL_AD2DA                    ((JL_AD2DA_TypeDef   *)JL_AD2DA_BASE)

//............. 0x0500 - 0x05ff............ as_anc
typedef struct {
    __RW __u32 DMA_CON0;
    __RW __u32 DMA_CON1;
    __RW __u32 DMA_CON2;
    __RW __u32 LEN;
    __RW __u32 ADR;
    __RW __u32 ANC_CON0;
    __RW __u32 ANC_CON1;
    __RW __u32 ANC_CON2;
    __RW __u32 ANC_CON3;
    __RW __u32 ANC_CON4;
    __RW __u32 ANC_CON5;
    __RW __u32 ANC_CON6;
    __RW __u32 ANC_CON7;
    __RW __u32 ANC_CON8;
    __RW __u32 ANC_CON9;
    __RW __u32 ANC_CON10;
    __RW __u32 ANC_CON11;
    __RW __u32 ANC_CON12;
    __RW __u32 ANC_CON13;
    __RW __u32 ANC_CON14;
    __RW __u32 ANC_CON15;
    __RW __u32 ANC_CON16;
    __RW __u32 ANC_CON17;
    __RW __u32 ANC_CON18;
    __RW __u32 ANC_CON19;
} JL_ANC_TypeDef;

#define JL_ANC_BASE                  (as_base + map_adr(0x05, 0x00))
#define JL_ANC                       ((JL_ANC_TypeDef       *)JL_ANC_BASE)

typedef struct {
    __RW __u32 MCTL;
    __RW __u32 MDAT;
    __RW __u32 MCON;
    __RW __u32 CON0;
    __RW __u32 CON1;
    __RW __u32 CON2;
    __RW __u32 CON3;
    __RW __u32 CON4;
    __RW __u32 CON5;
    __RW __u32 CON6;
    __RW __u32 CON7;
    __RW __u32 CON8;
    __RW __u32 CON9;
    __RW __u32 CON10;
    __RW __u32 CON11;
    __RW __u32 CON12;
    __RW __u32 CON13;
    __RW __u32 CON14;
    __RW __u32 CON15;
    __RW __u32 CON16;
    __RW __u32 CON17;
    __RW __u32 CON18;
    __RW __u32 CON19;
    __RW __u32 CON20;
    __RW __u32 CON21;
    __RW __u32 CON22;
    __RW __u32 CON23;
    __RW __u32 CON24;
    __RW __u32 CON25;
    __RW __u32 CON26;
    __RW __u32 CON27;
    __RW __u32 CON28;
    __RW __u32 CON29;
    __RW __u32 CON30;
    __RW __u32 CON31;
    __RW __u32 CON32;
    __RW __u32 CON33;
    __RW __u32 CON34;
    __RW __u32 CON35;
    __RW __u32 CON36;
    __RW __u32 CON37;
    __RW __u32 CON38;
    __RW __u32 CON39;
    __RW __u32 PND0;
    __RW __u32 PND1;
    __RW __u32 CON40;
    __RW __u32 CON41;
    __RW __u32 CON42;
    __RW __u32 CON43;
    __RW __u32 CON44;
    __RW __u32 CON45;
    __RW __u32 CON46;
    __RW __u32 CON47;
    __RW __u32 CON48;
    __RW __u32 CON49;
    __RW __u32 CON50;
} JL_ANC_CORE_TypeDef;

#define JL_ANC_CORE_BASE             (as_base + map_adr(0x09, 0x00))
#define JL_ANC_CORE                  ((JL_ANC_CORE_TypeDef   *)JL_ANC_CORE_BASE)


//............. 0x0600 - 0x06ff............ as_alnk
typedef struct {
    __RW __u16 CON0;
    __RW __u16 CON1;
    __RW __u16 CON2;
    __RW __u32 CON3;
    __RW __u32 CON4;
    __RW __u32 ADR0;
    __RW __u32 ADR1;
    __RW __u32 ADR2;
    __RW __u32 ADR3;
    __RW __u16 LEN;
    __RW __u32 PNS;
    __RW __u32 HWPTR0;
    __RW __u32 HWPTR1;
    __RW __u32 HWPTR2;
    __RW __u32 HWPTR3;
    __RW __u32 SWPTR0;
    __RW __u32 SWPTR1;
    __RW __u32 SWPTR2;
    __RW __u32 SWPTR3;
    __RW __u32 SHN0;
    __RW __u32 SHN1;
    __RW __u32 SHN2;
    __RW __u32 SHN3;
    __RW __u32 BLOCK;
} JL_ALNK_TypeDef;

#define JL_ALNK0_BASE                (as_base + map_adr(0x06, 0x00))
#define JL_ALNK0                     ((JL_ALNK_TypeDef    *)JL_ALNK0_BASE)


//............. 0x0700 - 0x07ff............ as_spdif


//............. 0x0800 - 0x08ff............ as_plnk
//typedef struct {
//    __RW __u16 CON;
//    __RW __u8  SMR;
//    __RW __u32 ADR;
//    __RW __u32 LEN;
//    __RW __u16 DOR;
//    __RW __u32 CON1;
//
//} JL_PLNK_TypeDef;
//
//#define JL_PLNK_BASE                 (as_base + map_adr(0x08, 0x00))
//#define JL_PLNK                      ((JL_PLNK_TypeDef     *)JL_PLNK_BASE)


//............. 0x1000 - 0x10ff............ as_ana
typedef struct {
    __RW __u32 DAA_CON0;
    __RW __u32 DAA_CON1;
    __RW __u32 DAA_CON2;
    __RO __u32 DAA_CON7;
    __RW __u32 ADA_CON0;
    __RW __u32 ADA_CON1;
    __RW __u32 ADA_CON2;
    __RW __u32 ADA_CON3;
    __RW __u32 ADA_CON4;
    __RW __u32 ADA_CON5;
    __RW __u32 ADA_CON6;
    __RW __u32 ADDA_CON0;
    __RW __u32 ADDA_CON1;
} JL_ADDA_TypeDef;

#define JL_ADDA_BASE               (as_base + map_adr(0x10, 0x00))
#define JL_ADDA                    ((JL_ADDA_TypeDef       *)JL_ADDA_BASE)


//............. 0x1100 - 0x11ff............ as_apa

//#define JL_APA_BASE                 (as_base + map_adr(0x11, 0x00))
//#define JL_APA                      ((JL_APA_TypeDef   *)JL_APA_BASE)


//............. 0x1200 - 0x12ff............ as_hadc


//............. 0x2000 - 0x20ff............ as_sbc
typedef struct {
    __RW __u32 CON0;
    __WO __u32 DEC_SRC_ADR;
    __WO __u32 DEC_DST_ADR;
    __WO __u32 DEC_PCM_WCNT;
    __WO __u32 DEC_INBUF_LEN;
    __WO __u32 ENC_SRC_ADR;
    __WO __u32 ENC_DST_ADR;
    __RO __u32 DEC_DST_BASE;
    __WO __u32 DEC_WEIGHT_BASE;
    __WO __u32 DEC_WEIGHT_ADD;

} JL_SBC_TypeDef;

#define JL_SBC_BASE                   (as_base + map_adr(0x20, 0x00))
#define JL_SBC                        ((JL_SBC_TypeDef *)JL_SBC_BASE)


//............. 0x2100 - 0x21ff............ as_sbcram
typedef struct {
    __RW __u32 DATA[384];
} JL_SBCRAM_TypeDef;

#define JL_SBCRAM_BASE                (as_base + map_adr(0x21, 0x00))
#define JL_SBCRAM                    ((JL_SBCRAM_TypeDef *)JL_SBCRAM_BASE)


//............. 0x3000 - 0x30ff............
//
//............. 0x3100 - 0x31ff............ as_emu
typedef struct {
    __RW __u32 WREN;
    __RW __u32 CON0;
    __RW __u32 CON1;
    __RW __u32 CON2;
    __RW __u32 CON3;
    __RW __u32 MSG0;
    __RW __u32 MSG1;
    __RW __u32 MSG2;
    __RW __u32 MSG3;
    __RO __u32 ID;
} JL_AEMU_TypeDef;

#define JL_AEMU_BASE                    (as_base + map_adr(0x31, 0x00))
#define JL_AEMU                         ((JL_AEMU_TypeDef    *)JL_AEMU_BASE)


//===============================================================================//
//
//  wl_top
//
//===============================================================================//

typedef struct {
    __RW __u32 CON;
    __RW __u32 SEL;
    __RW __u32 BEG;
    __RW __u32 END;
    __RW __u32 DAT_VLD0;
    __RW __u32 DAT_VLD1;
    __RW __u32 DAT_VLD2;
    __RW __u32 DAT_VLD3;
    __RO __u32 ROM_CRC;
    __RW __u32 MCFG0_RF1P;
    __RW __u32 MCFG0_RF2P;
    __RW __u32 MCFG0_RM1P;
    __RW __u32 MCFG0_RM2P;
    __RW __u32 MCFG0_VROM;
    __RW __u32 MCFG0_CON;
} JL_WL_MBIST_TypeDef;

#define JL_WL_MBIST_BASE               (wl_base + map_adr(0x00, 0x00))
#define JL_WL_MBIST                    ((JL_WL_MBIST_TypeDef *)JL_WL_MBIST_BASE)

typedef struct {
    __RW __u32 CON0;
} JL_WL_AUD_TypeDef;

#define JL_WL_AUD_BASE             (wl_base + map_adr(0x01, 0x00))
#define JL_WL_AUD                  ((JL_WL_AUD_TypeDef *)JL_WL_AUD_BASE)

typedef struct {
    __RW __u32 CON0;
    __RW __u32 CON3;
    __RW __u32 LOFC_CON;
    __RW __u32 LOFC_RES;
    __RW __u32 DBG_CON;
} JL_WL_TypeDef;

#define JL_WL_BASE                 (wl_base + map_adr(0x01, 0x01))
#define JL_WL                      ((JL_WL_TypeDef     *)JL_WL_BASE)

typedef struct {
    __RW __u32 CON;
    __RW __u32 DABUF;
    __RO __u32 DASPACE;
    __RO __u32 OFFEND;
    __RO __u32 OFFSUM;
    __RO __u32 PCMNUM;
    __RO __u32 BTTIME;
    __WO __u32 CON1  ;
    __RW __u32 CON2  ;
    __WO __u32 BT_OFFSET;
    __WO __u32 MIN_SLOT;
    __RW __u32 OUTSR;
    __WO __u32 INSR;
    __RW __u32 CON3  ;
} JL_SYNC_TypeDef;

#define JL_SYNC_BASE               (wl_base + map_adr(0x0e, 0x00))
#define JL_SYNC                    ((JL_SYNC_TypeDef *)JL_SYNC_BASE)


typedef struct {
    __RW __u32 WREN;
    __RW __u32 CON0;
    __RW __u32 CON1;
    __RW __u32 CON2;
    __RW __u32 CON3;
    __RW __u32 MSG0;
    __RW __u32 MSG1;
    __RW __u32 MSG2;
    __RW __u32 MSG3;
    __RO __u32 ID;
} JL_WEMU_TypeDef;

#define JL_WEMU_BASE                (wl_base + map_adr(0x10, 0x00))
#define JL_WEMU                     ((JL_WEMU_TypeDef    *)JL_WEMU_BASE)

#endif


