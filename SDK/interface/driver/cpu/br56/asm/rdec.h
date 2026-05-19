#ifndef _RDEC_H_
#define _RDEC_H_

#define RDEC_NUM_MAX        8
#define RDEC_CH_MAX         1
#define RDEC_BASE_ADDR      JL_QDEC0
#define RDEC_OFFSET         0 //(JL_QDEC1 - JL_QDEC0)

//QDECx_CON
#define QDEC_INT_MODE   10
#define QDEC_MODE       8
#define QDEC_PND        7
#define QDEC_CPND       6
#define QDEC_SPND       2
#define QDEC_POL        1
#define QDEC_EN         0
#define QDEC_SMP        0

//******************************************************************************************
typedef enum {
    RDEC_0,
} rdec_ch;

typedef enum {
    RDEC_PHASE_1 = 0,
    RDEC_PHASE_2,
    RDEC_PHASE_4,
} rdec_mode ;

struct rdec_config {
    rdec_ch ch; //选择一个rdec通道
    rdec_mode mode; //选择输入模式
    u16 port_0; //IO口
    u16 port_1; //IO口
    u8  clk; //采样时钟设置,写-1使用默认值 取值范围:0x0~0xf
};

struct rdec_info_t {
    JL_QDEC_TypeDef *rdecx;
    struct rdec_config rdec_cfg;
};

int rdec_init(struct rdec_config *rdec_cfg);
void rdec_deinit(int rdec_cfg_id);
void rdec_start(int rdec_cfg_id);
void rdec_pause(int rdec_cfg_id);
void rdec_rsume(int rdec_cfg_id);
s8 rdec_get_value(int rdec_cfg_id);

#endif

