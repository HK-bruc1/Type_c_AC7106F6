#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".audio_alink_demo.data.bss")
#pragma data_seg(".audio_alink_demo.data")
#pragma const_seg(".audio_alink_demo.text.const")
#pragma code_seg(".audio_alink_demo.text")
#endif
/*
 ****************************************************************************
 *							Audio ALINK Demo
 *
 *Description	: Audio ALINK使用范例，AudioLink是一个通用的双声道音频接口，
 *				  连接信号有MCLK,SCLK,LRCK,DATA,支持IIS模式。
 *Notes			: 本demo为开发测试范例，请不要修改该demo， 如有需求，请自行
 *				  复制再修改
 ****************************************************************************
 */
#include "audio_link.h"
#include "media/includes.h"
#include "audio_config.h"

#if TCFG_IIS_NODE_ENABLE || TCFG_IIS_RX_NODE_ENABLE

//模块总句柄
static void *alink0_hdl = NULL;
//每个通道的操作句柄
static void *alink0_ch0 = NULL;
static void *alink0_ch1 = NULL;
static void *alink0_ch2 = NULL;
static void *alink0_ch3 = NULL;


/*
 *每个通道的中断回调函数，可在函数内获取输入输出buf的数据,地址buf, 长度len(byte)
 * */
static void ch0_tx_handle(void *priv, void *buf, int len);
static void ch1_rx_handle(void *priv, void *buf, int len);
static void ch2_rx_handle(void *priv, void *buf, int len);
static void ch3_tx_handle(void *priv, void *buf, int len);


/*
 *总模块共用的信息配置
 * */
#if (defined(CONFIG_CPU_BR27) || defined(CONFIG_CPU_BR28) || defined(CONFIG_CPU_BR36))
ALINK_PARM	alink0_demo = {
    //模块参数配置
    .module = ALINK0,
    .mclk_io = IO_PORTC_00,
    .sclk_io = IO_PORTC_01,
    .lrclk_io = IO_PORTC_02,
    .mode = ALINK_MD_IIS,
    .role = ALINK_ROLE_MASTER,
    .clk_mode = ALINK_CLK_FALL_UPDATE_RAISE_SAMPLE,
    .bitwide = ALINK_LEN_16BIT,
    .sclk_per_frame = ALINK_FRAME_32SCLK,
    .dma_len = 256 * 2 * 2 * 2,  //256点，每个点2byte,2个通道，2块buf
    .sample_rate = ALINK_SR_48000,
    .buf_mode = ALINK_BUF_DUAL,
};
#elif defined(CONFIG_CPU_BR29)

ALINK_PARM	alink0_demo = {
    //模块参数配置
    .module = 0,//port_a
    .mclk_io = IO_PORTA_02,
    .sclk_io = IO_PORTA_03,
    .lrclk_io = IO_PORTA_04,
    .mode = ALINK_MD_IIS,
    .role = ALINK_ROLE_MASTER,
    .clk_mode = ALINK_CLK_FALL_UPDATE_RAISE_SAMPLE,
    .bitwide = ALINK_LEN_16BIT,
    .sclk_per_frame = ALINK_FRAME_32SCLK,
    .dma_len = 256 * 2 * 2 * 2,
    .sample_rate = ALINK_SR_48000,
    .buf_mode = ALINK_BUF_DUAL,
};

#else//(defined(CONFIG_CPU_BR50) || defined(CONFIG_CPU_BR52) || defined(CONFIG_CPU_BR56))

ALINK_PARM	alink0_demo = {
    //模块参数配置
    .module = ALINK0,
    .mclk_io = IO_PORTC_00,
    .sclk_io = IO_PORTC_01,
    .lrclk_io = IO_PORTC_02,
    .mode = ALINK_MD_BASIC,
    .role = ALINK_ROLE_MASTER,
    .clk_mode = ALINK_CLK_FALL_UPDATE_RAISE_SAMPLE,
    .bitwide = ALINK_BW_16BIT,
    .dma_len = 256 * 2 * 2 * 2,
    .sample_rate = ALINK_SR_48000,
    .buf_mode = ALINK_BUF_DUAL,
    .slot_num = 2,
};

#endif


/*
 *通道信息配置
 * */
struct alnk_hw_ch hw_ch0_tx_cfg = {
#if defined(CONFIG_CPU_BR29)
    .data_io = IO_PORTA_00,
#else
    .data_io = IO_PORTC_04,
#endif
    .ch_ie = 1,
    .dir = ALINK_DIR_TX,
    .isr_cb = ch0_tx_handle,
    .private_data = NULL,
#if (CONFIG_CPU_BR28 || CONFIG_CPU_BR27 || CONFIG_CPU_BR36 || CONFIG_CPU_BR29)
#else
    .ch_mode = ALINK_CH_MD_BASIC_IIS
#endif
};

struct alnk_hw_ch hw_ch1_rx_cfg = {
#if defined(CONFIG_CPU_BR29)
    .data_io = IO_PORTA_01,
#else
    .data_io = IO_PORTC_05,
#endif
    .ch_ie = 1,
    .dir = ALINK_DIR_RX,
    .isr_cb = ch1_rx_handle,
    .private_data = NULL,
#if (CONFIG_CPU_BR28 || CONFIG_CPU_BR27 || CONFIG_CPU_BR36 || CONFIG_CPU_BR29)
#else
    .ch_mode = ALINK_CH_MD_BASIC_IIS
#endif
};

struct alnk_hw_ch hw_ch2_rx_cfg = {
#if defined(CONFIG_CPU_BR29)
    .data_io = IO_PORTA_05,
#else
    .data_io = IO_PORTC_06,
#endif
    .ch_ie = 1,
    .dir = ALINK_DIR_RX,
    .isr_cb = ch2_rx_handle,
    .private_data = NULL,
#if (CONFIG_CPU_BR28 || CONFIG_CPU_BR27 || CONFIG_CPU_BR36 || CONFIG_CPU_BR29)
#else
    .ch_mode = ALINK_CH_MD_BASIC_IIS
#endif
};

struct alnk_hw_ch hw_ch3_tx_cfg = {

#if defined(CONFIG_CPU_BR29)
    .data_io = IO_PORTA_06,
#else
    .data_io = IO_PORTC_07,
#endif
    .ch_ie = 1,
    .dir = ALINK_DIR_TX,
    .isr_cb = ch3_tx_handle,
    .private_data = NULL,
#if (CONFIG_CPU_BR28 || CONFIG_CPU_BR27 || CONFIG_CPU_BR36 || CONFIG_CPU_BR29)
#else
    .ch_mode = ALINK_CH_MD_BASIC_IIS
#endif
};


static void ch0_tx_handle(void *priv, void *buf, int len)
{
    putchar('0');
    //往buf填数据,填len的长度
}
static void ch1_rx_handle(void *priv, void *buf, int len)
{
    putchar('1');
    //从buf读数据,读len的长度
}
static void ch2_rx_handle(void *priv, void *buf, int len)
{
    putchar('2');
    //从buf读数据,读len的长度
}
static void ch3_tx_handle(void *priv, void *buf, int len)
{
    putchar('3');
    //往buf填数据,填len的长度
}


void alink_channel_dump(void *hw_channel)
{
    struct alnk_hw_ch *hw_channel_parm = (struct alnk_hw_ch *)hw_channel;
    if (!hw_channel_parm) {
        return;
    }
    r_printf("hw_channel_parm->data_io = %d\n", hw_channel_parm->data_io);
    r_printf("hw_channel_parm->ch_ie = %d\n", hw_channel_parm->ch_ie);
    r_printf("hw_channel_parm->dir = %d\n", hw_channel_parm->dir);
    r_printf("hw_channel_parm->isr_cb = 0x%x\n", hw_channel_parm->isr_cb);
    r_printf("hw_channel_parm->buf = 0x%x\n", hw_channel_parm->buf);
    r_printf("hw_channel_parm->module = %d\n", hw_channel_parm->module);
    r_printf("hw_channel_parm->ch_idx = %d\n", hw_channel_parm->ch_idx);
    r_printf("hw_channel_parm->ch_en = %d\n", hw_channel_parm->ch_en);
}


/*
 *模块打开
 * */
void audio_link_open_demo(void)
{

    alink0_hdl = alink_init(&alink0_demo);				//模块初始化

    alink0_ch0 = alink_channel_init_base(alink0_hdl, &hw_ch0_tx_cfg, 0);	//通道0初始化 tx
    alink0_ch1 = alink_channel_init_base(alink0_hdl, &hw_ch1_rx_cfg, 1);	//通道1初始化 rx
    alink0_ch2 = alink_channel_init_base(alink0_hdl, &hw_ch2_rx_cfg, 2);	//通道2初始化 rx
    alink0_ch3 = alink_channel_init_base(alink0_hdl, &hw_ch3_tx_cfg, 3);	//通道3初始化 tx

    alink_start(alink0_hdl);							//模块打开


    //将 tx的io挪到alink_start之后初始化
    if (hw_ch0_tx_cfg.dir == ALINK_DIR_TX) {
        alink_channel_io_init(alink0_hdl, alink0_ch0, 0);
    }
    if (hw_ch3_tx_cfg.dir == ALINK_DIR_TX) {
        alink_channel_io_init(alink0_hdl, alink0_ch3, 3);
    }


    alink_channel_dump(alink0_ch0);
    alink_channel_dump(alink0_ch1);
    alink_channel_dump(alink0_ch2);
    alink_channel_dump(alink0_ch3);

    void alink0_info_dump();
    alink0_info_dump();
}

/*
 *模块关闭
 * */
void audio_link_close_demo(void)
{
    alink_channel_close(alink0_hdl, alink0_ch0);		//通道0关闭
    alink_channel_close(alink0_hdl, alink0_ch1);		//通道1关闭
    alink_channel_close(alink0_hdl, alink0_ch2);		//通道2关闭
    alink_channel_close(alink0_hdl, alink0_ch3);		//通道3关闭

    alink_uninit_base(alink0_hdl, NULL);	//模块关闭
    os_time_dly(100);

    void alink0_info_dump();
    alink0_info_dump();
}

#endif

