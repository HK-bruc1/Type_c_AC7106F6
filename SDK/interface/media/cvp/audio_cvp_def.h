/*
 *******************************************************************
 *						Audio CVP Definitions
 *
 *Note(s):
 *		 (1)Only macro definitions can be defined here.
 *		 (2)Use (1UL << (n)) instead of BIT(n)
 *******************************************************************
 */
#ifndef _AUDIO_CVP_DEF_H_
#define _AUDIO_CVP_DEF_H_

/*agc类型定义*/
#define AGC_EXTERNAL    0
#define AGC_INTERNAL    1

/*
 *SMS算法选择
 *(1)SMS_TDE模式性能更好，同时也需要更多的ram和mips
 *(2)SMS_TDE内置了延时估计和延时补偿
 *可以更好的兼容延时不固定的场景
 */
//<SMS Mode Defined>
#define SMS_DEFAULT		0	//独立库算法模型
#define SMS_TDE			1	//集成库算法模型

//<Noise Suppress Config>
#define CVP_ANS_MODE	0	/*传统降噪*/
#define CVP_DNS_MODE	1	/*神经网络降噪*/

/*DMS Mode Config*/
#define DMS_NORMAL		1	//普通双mic降噪(mic距离固定)
#define DMS_FLEXIBLE	2	//适配mic距离不固定且距离比较远的情况，比如头戴式话务耳机
#define DMS_HYBRID  	3	//talk mic + fb mic 风噪处理
#define DMS_AWN  		4	//talk mic + ff mic 风噪处理

/*Develop cvp Config
 * 高位：算法原厂
 * 低位：算法类型
 */
#define CVP_CFG_USER_DEFINED			1	//用户自定义开发算法
#define CVP_CFG_AIS_3MIC				2	//思必驰3mic算法
#define CVP_CFG_ELEVOC_1MIC_VPU			((1UL << 20) | (1UL << 0))	//大象声科Elevoc：1mic+VPU
#define CVP_CFG_ELEVOC_2MIC_VPU			((1UL << 20) | (1UL << 1))	//大象声科Elevoc：2mic+VPU
#define CVP_CFG_ELEVOC_2MIC_VPU_CLIP	((1UL << 20) | (1UL << 2))	//大象声科Elevoc：2mic+VPU（耳夹形态FreeClip）
#define CVP_CFG_ELEVOC_3MIC_VPU			((1UL << 20) | (1UL << 3))	//大象声科Elevoc：3mic+VPU

//CVP ALGO BITMAP ENABLE
#define CVP_ELEVOC_ALGO_BITMAP			(1UL << 20)	//大象声科Elevoc算法使能映射

/*SMS DNS版本定义*/
#define SMS_DNS_V100        0xC1
#define SMS_DNS_V200        0xC2

/*DMS DNS版本定义*/
#define DMS_GLOBAL_V100     0xB1
#define DMS_GLOBAL_V200     0xB2
/*
 * V200新算法回声消除nlp模式
 * JLSP_NLP_MODE1: 模式1为单独的NLP回声抑制，回声压制会偏过，该模式下NLP模块可以单独开启
 * JLSP_NLP_MODE2: 模式2下回声信号会先经过AEC线性压制，然后进行NLP非线性压制
 *                 此模式NLP不能单独打开需要同时打开AEC,使用AEC模块压制不够时，建议开启该模式
 */
#define JLSP_NLP_MODE1 0x01   //模式一（默认）
#define JLSP_NLP_MODE2 0x02   //模式二

/*
 * V200新算法风噪降噪模式
 * JLSP_WD_MODE1: 模式1为常规的降风噪模式，风噪残余会偏大些
 * JLSP_WD_MODE2: 模式2为神经网络降风噪，风噪抑制会比较干净，但是会需要多消耗31kb的flash
 */
#define JLSP_WD_MODE1 0x01   //常规降噪
#define JLSP_WD_MODE2 0x02   //nn降风噪，目前该算法启用会多31kflash

/*
 * JLSP_3MIC_MODE0	//使用双麦模式
 * JLSP_3MIC_MODE1	//3mic模式1,一般tws耳机使用
 * JLSP_3MIC_MODE2	//3mic模式2,一般头戴式耳机使用
 * choosemode函数只用来在线设置切换跑2mic还是3mic，设置mode1和mode2时才有效
 */
#define JLSP_3MIC_MODE0	0x00	//使用双麦模式或单麦模式
#define JLSP_3MIC_MODE1	0x01	//3mic模式1
#define JLSP_3MIC_MODE2	0x02	//3mic模式2

/*CVP带宽配置*/
#define CVP_NB_EN       0x01    //Narrow-Band
#define CVP_WB_EN       0x02    //Wide-Band

/*降噪版本定义*/
#define ANS_V100	0xA1
#define ANS_V200	0xA2
/*
 *******************************************************************
 *						NN Table Bit Definitions
 *******************************************************************
 */
// LLNS bit : 0 ~ 8
#define NN_TABLE_LLNS_DNS_SR32K_MODE0           BIT(0)
#define NN_TABLE_LLNS_DNS_SR48K_MODE0           BIT(1)
#define NN_TABLE_LLNS_DNS_SR48K_MODE0_LLM       BIT(2)
#define NN_TABLE_LLNS_DNS_SR48K_MODE0_2FRAME    BIT(3)
#define NN_TABLE_LLNS_DNS_SR32K_MODE1           BIT(4)
#define NN_TABLE_LLNS_DNS_SR48K_MODE1           BIT(5)
#define NN_TABLE_LLNS_DNS_SR48K_MODE1_2FRAME    BIT(6)
#define NN_TABLE_LLNS_DNS_SR48K_MODE2           BIT(7)

// CVP V3 table bit : 9 ~ 31
#define NN_TABLE_CVP_GENERAL	  	  BIT(9)
#define NN_TABLE_CVP_GENERAL_V4  	  BIT(12)
#define NN_TABLE_CVP_1MIC_LARGE    	  BIT(13)		// kpt模型
#endif/*_AUDIO_CVP_DEF_H_*/
